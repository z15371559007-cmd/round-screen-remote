# 软件框架

## 模块边界

### 1. 媒体遥控

ESP32 以 WebSocket 客户端连接 Omarchy 上的桥接服务。桥接端读取当前 MPRIS 播放器并调用 `playerctl`；固件端负责屏幕显示和发送媒体命令。首版只针对 Linux/Omarchy 主机，不把媒体控制塞进宠物或 PTT 模块。

### 2. Push-to-Talk

圆屏按钮的按下/松开事件分别映射为 BLE HID key-down/key-up。语音输入软件由电脑端选择和配置，电脑麦克风负责收音。快捷键并非各系统统一，尚未绑定具体按键，也未实现 HID profile 切换。

### 3. 待机互动宠物

宠物是独立待机模式，只把预设语音口令映射为表情/动作，不执行媒体命令。目标使用板上麦克风和 ESP-SR 离线识别；官方命令词流程为唤醒后再识别短命令，不是自由对话或在任意语句中可靠抓取关键词。最终口令和动画由后续选择。

## 固件入口与状态机

`mode_router` 是唯一模式入口。启动默认进入 `APP_MODE_STANDBY_PET`；触屏 UI 后续可调用 `mode_router_switch_to()` 切换到媒体遥控或 PTT。各模块暴露自己的生命周期和事件接口，不直接互相调用。

当前 `board_port`、`media_link`、`ble_hid`、`pet_voice_backend` 均为可替换 stub。stub 会记录未接硬件，不会伪装成已连接或已识别。

## 桥接协议草案

连接 `/ws` 后，客户端先发送：

```json
{"type":"hello","token":"<local token>","device_id":"round-remote-01"}
```

成功后桥接端发 `hello_ack`，并周期推送 `state`：

```json
{"type":"state","status":"playing","title":"...","artist":"...","album":"...","position_ms":12000,"duration_ms":180000,"artwork_available":true}
```

控制消息使用 `type=command`，命令限定为 `play_pause`、`previous`、`next`、`stop`、`seek_to`、`set_volume`。桥接只调用 `playerctl` 的白名单动作，不接受任意 shell 命令。协议暂不传封面图片字节，也不开放主机文件路径。

## 安全与未完成事项

桥接当前使用未加密 WebSocket，令牌会在局域网链路上传输；只适合受信任的本地网络开发测试，不要做路由器端口映射或公开到互联网。量产/跨网络前需补 TLS、安全配网、令牌生命周期和设备配对。当前服务默认只监听回环地址；要让 ESP32 连入，需由用户显式改为绑定局域网接口并配置防火墙。

本框架没有在真实板子上构建或验证。开始固件集成前先核对板子完整型号、屏幕和触控驱动版本，再接入 LVGL、ESP-SR、BLE HID 和 Wi-Fi/WebSocket 客户端。