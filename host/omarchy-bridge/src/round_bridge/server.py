"""Development WebSocket server for the local Omarchy bridge."""

from __future__ import annotations

import asyncio
import logging
import os
import time

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn

from . import __version__
from .playerctl import execute, read_state
from .protocol import PROTOCOL_VERSION, ProtocolError, authenticate, parse_command

logging.basicConfig(level=os.getenv("ROUND_BRIDGE_LOG_LEVEL", "INFO"))
logger = logging.getLogger("round_bridge")
app = FastAPI(title="Round Remote Local Bridge", version=__version__)
POLL_SECONDS = max(0.25, float(os.getenv("ROUND_BRIDGE_POLL_SECONDS", "1.0")))


@app.get("/health")
async def health() -> dict[str, object]:
    return {"ok": True, "bridge": "round-remote", "protocol_version": PROTOCOL_VERSION}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket) -> None:
    await websocket.accept()
    token = os.getenv("ROUND_REMOTE_TOKEN", "")
    if not token:
        await websocket.send_json({"type": "error", "code": "bridge_token_not_configured"})
        await websocket.close(code=1011)
        return

    try:
        hello = await asyncio.wait_for(websocket.receive_json(), timeout=5.0)
    except asyncio.TimeoutError:
        await websocket.close(code=1008, reason="hello_timeout")
        return
    except WebSocketDisconnect:
        return

    if not authenticate(hello, token):
        await websocket.send_json({"type": "error", "code": "authentication_failed"})
        await websocket.close(code=1008)
        return

    device_id = hello.get("device_id", "unknown") if isinstance(hello, dict) else "unknown"
    logger.info("Local remote connected: %s", str(device_id)[:64])
    await websocket.send_json({"type": "hello_ack", "protocol_version": PROTOCOL_VERSION})
    next_poll = time.monotonic()

    try:
        while True:
            timeout = max(0.0, next_poll - time.monotonic())
            try:
                frame = await asyncio.wait_for(websocket.receive_json(), timeout=timeout)
            except asyncio.TimeoutError:
                await websocket.send_json(await read_state())
                next_poll = time.monotonic() + POLL_SECONDS
                continue
            except WebSocketDisconnect:
                break

            try:
                command = parse_command(frame)
                ok, error = await execute(command.name, command.value)
                await websocket.send_json({
                    "type": "command_result",
                    "id": command.request_id,
                    "ok": ok,
                    "error": error,
                })
            except ProtocolError as exc:
                await websocket.send_json({"type": "error", "code": str(exc)})
    except WebSocketDisconnect:
        pass
    finally:
        logger.info("Local remote disconnected: %s", str(device_id)[:64])


def main() -> None:
    if not os.getenv("ROUND_REMOTE_TOKEN"):
        raise SystemExit("Set ROUND_REMOTE_TOKEN before starting the bridge")
    host = os.getenv("ROUND_BRIDGE_HOST", "127.0.0.1")
    port = int(os.getenv("ROUND_BRIDGE_PORT", "8765"))
    uvicorn.run(app, host=host, port=port, log_level="info")


if __name__ == "__main__":
    main()
