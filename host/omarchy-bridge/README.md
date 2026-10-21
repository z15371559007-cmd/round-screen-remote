# Omarchy / Linux 本地桥接

可运行的开发参考：FastAPI WebSocket 服务通过 `playerctl` 读取默认 MPRIS 播放器状态，并只执行白名单媒体命令。它还没有连接 ESP32 固件，也不传封面图片。

## 运行

要求 Python 3.11+、`playerctl` 和能正常工作的 MPRIS/D-Bus 会话。建议先在 Omarchy 上安装并启动 `playerctld`，让 `playerctl` 选择最近活动的播放器。

```sh
cd host/omarchy-bridge
python -m venv .venv
. .venv/bin/activate
pip install -e .
export ROUND_REMOTE_TOKEN="$(python -c 'import secrets; print(secrets.token_urlsafe(24))')"
round-bridge
```

默认只监听 `127.0.0.1:8765`，只能从本机连。要让 ESP32 在局域网访问，需主动将 `ROUND_BRIDGE_HOST` 设为主机的局域网地址或 `0.0.0.0`，并在防火墙中仅允许可信 LAN 设备访问。不要配置路由器端口转发或公开到互联网。

```sh
ROUND_BRIDGE_HOST=0.0.0.0 ROUND_BRIDGE_PORT=8765 round-bridge
```

## 协议与范围

客户端连接 `ws://<主机局域网地址>:8765/ws`，首帧为 `{"type":"hello","token":"…","device_id":"…"}`。之后桥接每秒推送状态；客户端可发送 `play_pause`、`previous`、`next`、`stop`、`seek_to`（秒）或 `set_volume`（0–1）。协议示例见 [`docs/architecture.md`](../../docs/architecture.md)。

令牌在当前 `ws://` 连接中未加密，只适合受信任的本地网络开发测试。此骨架没有 TLS、正式配对/撤销、图片代理或系统主音量控制；`artwork_available` 只报告播放器是否给出封面地址，不会把 `file://` 路径传给客户端。不要把它当作安全的公网服务。

测试协议输入校验：

```sh
python -m unittest discover -s tests
```
