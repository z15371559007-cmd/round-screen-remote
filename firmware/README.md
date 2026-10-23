# 圆屏遥控器 ESP-IDF 固件

## 当前实现

- 使用 Waveshare 官方 `esp32_s3_touch_amoled_1_75` BSP 2.0.5（ESP-IDF >=5.5），由 BSP 初始化 CO5300 466×466 QSPI 屏和 CST9217 触控；LVGL 9.4 驱动界面。
- BLE HID 已接入：媒体 Consumer Control 支持播放/暂停、上一首/下一首、音量加减和静音。PTT 使用标准键盘 HID 按键按下/释放；默认键位为 0（未配置），不会发送按键，需先按目标主机的绑定设置显式选择 usage ID 和修饰键。
- UI 的 BLE 状态反映实际 HID 连接。未连接或 PTT 键未配置时，操作不会假装成功。
- Wi-Fi/WebSocket 主机桥与设备侧语音采集/识别尚未实现，仍是明确 stub；目前不支持主机端协议握手，也不会把双麦硬件当成已经可用的语音链路。

## 构建

```sh
cd firmware
idf.py set-target esp32s3
idf.py menuconfig   # 默认 SKU 为“未核实”；可在核对板背铭牌后记录
idf.py build
```

当前 family-level 默认配置为 16 MB flash、single-app-large 分区布局（partition table 位于 0x8000，factory app 位于 0x10000；应用分区大小 0x177000）。该容量依据 Waveshare 标准 1.75 系列资料设置；系列存在子型号差异，实板的完整 SKU 尚未从板背核实。因此本仓库不附带“可直接刷写”的二进制包，也没有在实板上验证。

项目固定 Waveshare BSP 2.0.5，组件依赖保存在 `idf_component.yml` 与 `dependencies.lock`。目标 ESP-IDF 为 5.5 系列；工程默认启用 ESP32-S3 octal PSRAM 与 BLE/Bluedroid。

## 尚需输入与板上验证

- 记录/确认板背完整 SKU，避免把系列默认 flash/分区参数误套到不同子型号。
- 若要使用 PTT，配置主机语音应用实际绑定的键位；usage 可在 `menuconfig` 的 Round screen remote 中设置，默认保持关闭。
- BLE HID 当前提供标准媒体键和可配置 PTT 键盘传输；Windows/macOS/Linux 的配对与各应用响应需实机测试。
- Wi-Fi/WebSocket host bridge 需要目标主机地址、SSID/密码与本地令牌和协议定义；凭据不应提交到仓库。
- Linux 构建机与用户 Mac 是不同设备；远程构建不代表 Mac 串口已验证，也不等于实机刷写。刷写前必须在 Mac 上核对芯片、板型和端口。
