# ESP-IDF 固件骨架

本目录给三种模式建立状态机和模块接口，默认启动待机互动宠物。采用 ESP-IDF；后续再按实物板型接入板厂屏幕/触控驱动、BLE HID、Wi-Fi/WebSocket 和 ESP-SR。

## 构建入口

```sh
cd firmware
idf.py set-target esp32s3
idf.py build
```

此处只验证 ESP-IDF 工程结构；当前没有接入实际板子的显示、触控、麦克风或无线协议组件。未安装 ESP-IDF 时无法在普通 Python 环境里构建。

## 模块接口

- `mode_router`：唯一的模式切换入口。
- `modules/media_remote`：媒体 UI 事件与本地桥接接口。
- `modules/push_to_talk`：按下/松开事件，预留 BLE HID key-down/key-up。
- `modules/pet_standby`：待机宠物状态与命令 ID 到表情的映射。
- `platform/*_stub.c`：暂时的硬件/网络适配器，清楚返回未实现，不假装连接成功。

`pet_commands.c` 里的 ID 仅是示范映射；中文唤醒词、命令词、动画素材尚未选定。ESP-SR 的离线命令识别需后续结合 AFE 和 WakeNet 集成。

目标屏幕暂按 Waveshare ESP32-S3-Touch-AMOLED-1.75 作为参考；请在接线和驱动集成前核对实物完整型号。