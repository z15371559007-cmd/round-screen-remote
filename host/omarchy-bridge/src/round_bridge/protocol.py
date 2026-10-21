"""Small, strict JSON protocol shared with the future ESP32 client."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
import hmac
import math

PROTOCOL_VERSION = 1
_ALLOWED_COMMANDS = {"play_pause", "previous", "next", "stop", "seek_to", "set_volume"}


class ProtocolError(ValueError):
    """A client sent a malformed or unsupported protocol frame."""


@dataclass(frozen=True)
class Command:
    request_id: str
    name: str
    value: float | None = None


def authenticate(frame: object, expected_token: str) -> bool:
    if not isinstance(frame, Mapping) or frame.get("type") != "hello":
        return False
    supplied = frame.get("token")
    return isinstance(supplied, str) and bool(expected_token) and hmac.compare_digest(supplied, expected_token)


def parse_command(frame: object) -> Command:
    if not isinstance(frame, Mapping) or frame.get("type") != "command":
        raise ProtocolError("expected_command_frame")

    request_id = frame.get("id")
    name = frame.get("command")
    if not isinstance(request_id, str) or not request_id or len(request_id) > 64:
        raise ProtocolError("invalid_request_id")
    if not isinstance(name, str) or name not in _ALLOWED_COMMANDS:
        raise ProtocolError("unsupported_command")

    if name in {"seek_to", "set_volume"}:
        value = frame.get("value")
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ProtocolError("invalid_numeric_value")
        value = float(value)
        if name == "seek_to" and not 0 <= value <= 86_400:
            raise ProtocolError("seek_out_of_range")
        if name == "set_volume" and not 0 <= value <= 1:
            raise ProtocolError("volume_out_of_range")
    else:
        value = None

    return Command(request_id=request_id, name=name, value=value)
