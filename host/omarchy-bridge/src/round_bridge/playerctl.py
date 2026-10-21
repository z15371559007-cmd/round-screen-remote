"""Narrow playerctl adapter. It never invokes a shell or returns host file paths."""

from __future__ import annotations

import asyncio


async def _run_playerctl(*args: str) -> str | None:
    try:
        process = await asyncio.create_subprocess_exec(
            "playerctl", *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL,
        )
    except FileNotFoundError:
        return None

    try:
        stdout, _ = await asyncio.wait_for(process.communicate(), timeout=2.0)
    except asyncio.TimeoutError:
        process.kill()
        await process.communicate()
        return None

    if process.returncode != 0:
        return None
    return stdout.decode("utf-8", errors="replace").strip()


def _duration_ms(raw: str | None) -> int | None:
    if not raw:
        return None
    try:
        # MPRIS mpris:length is expressed in microseconds.
        value = int(raw)
    except ValueError:
        return None
    return max(0, value // 1000)


def _position_ms(raw: str | None) -> int | None:
    if not raw:
        return None
    try:
        return max(0, int(float(raw) * 1000))
    except ValueError:
        return None


async def read_state() -> dict[str, object]:
    queries = (
        ("status",),
        ("metadata", "xesam:title"),
        ("metadata", "xesam:artist"),
        ("metadata", "xesam:album"),
        ("metadata", "mpris:length"),
        ("position",),
        ("metadata", "mpris:artUrl"),
    )
    values = await asyncio.gather(*(_run_playerctl(*args) for args in queries))
    status, title, artist, album, duration, position, art_url = values
    normalized_status = status.lower() if status and status.lower() in {"playing", "paused", "stopped"} else "unavailable"
    return {
        "type": "state",
        "status": normalized_status,
        "title": title or "",
        "artist": artist or "",
        "album": album or "",
        "position_ms": _position_ms(position),
        "duration_ms": _duration_ms(duration),
        # Art is not transferred yet; never expose mpris:artUrl (which may be file://).
        "artwork_available": bool(art_url),
    }


async def execute(command_name: str, value: float | None = None) -> tuple[bool, str | None]:
    verbs = {
        "play_pause": ("play-pause",),
        "previous": ("previous",),
        "next": ("next",),
        "stop": ("stop",),
    }
    if command_name in verbs:
        result = await _run_playerctl(*verbs[command_name])
    elif command_name == "seek_to" and value is not None:
        result = await _run_playerctl("position", f"{value:.3f}")
    elif command_name == "set_volume" and value is not None:
        result = await _run_playerctl("volume", f"{value:.3f}")
    else:
        return False, "unsupported_command"

    if result is None:
        return False, "player_unavailable_or_command_failed"
    return True, None
