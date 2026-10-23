# 圆屏遥控器 ESP-IDF 固件

## 当前实现

- 使用 Waveshare 官方 `esp32_s3_touch_amoled_1_75` BSP 2.0.5（ESP-IDF >=5.5），由 BSP 初始化 CO5300 466×466 QSPI 屏和 CST9217 触控；LVGL 9.4 驱动界面。
- BLE HID 已接入：媒体 Consumer Control 支持播放/暂停、上一首/下一首、音量加减、静音和停止。
- PTT = 按住 F13：PTT 页按住上键（PWR）或屏幕按钮，期间持续按下键盘 F13（usage 0x68，无修饰键），松开发送空报告（抬键）；不是点按。键盘报告描述符 usage/逻辑最大值已扩展到 0xFF，F13–F24 可正常发送。蓝牙断开、切页、关机、超时（默认 120 s 看门狗）都会强制抬键，不会卡键。**已配对过旧固件的电脑需删除配对后重新配对**，否则主机缓存旧描述符收不到 F13。
- UI 的 BLE 状态反映实际 HID 连接。未连接或 PTT 键未配置时，操作不会假装成功。
- Wi-Fi/WebSocket 主机桥与设备侧语音采集/识别尚未实现，仍是明确 stub；目前不支持主机端协议握手，也不会把双麦硬件当成已经可用的语音链路。
- The Badge 风格的“工具”页已作为独立 RAM 模块接入现有 Wabi 圆屏界面：秒表（开始/暂停、复位、离开页面自动暂停）和泡泡纸触摸小游戏（计数、清零）。复用原来的炭黑/暖白/锈橙表盘、中文字体和圆形主控；这是独立 RAM 工具，不写入 Flash，也不要求新分区。
- 已集成 The Badge 风格的 BLE HID 8 键环（ESC、ENTER、TAB、SPACE、方向键）；尚未集成 BLE 空鼠/触控板、录音与 USB 导出、闹钟/时钟和其他游戏。它们涉及板载陀螺仪、USB 模式、RTC/联网时间以及显存/分区边界，不能仅凭“32MB”推断当前板型都可用；记录仍需先确认完整 SKU 和目标硬件。

## 完成 vs 占位（给 UI 的接口，统一 `#include "ui_api.h"`）

| 功能 | 状态 | 接口 |
|---|---|---|
| PTT 按住 F13 | ✅ 完成 | `push_to_talk.h`：`ptt_press()` / `ptt_release()`（触摸按下/松开），`ptt_is_held()`，`ptt_is_ready()`，`ptt_set_state_cb(cb, ctx)`（含断连/看门狗/切页的强制释放通知）；侧键走 `push_to_talk_press/release(PTT_SOURCE_KEY)`，多来源同时按住只在最后一个松开时抬键 |
| 播放控制（BLE HID 媒体键） | ✅ 完成 | `media_remote.h`：`media_remote_play_pause/previous/next/volume_up/volume_down/mute/stop()`，任意页面可调用；`media_remote_is_connected()`。上键单击 = 播放/暂停（PTT 页除外） |
| 蓝牙连接状态 | ✅ | `ble_hid.h`：`ble_hid_is_connected()`，`ble_hid_register_conn_cb(cb, ctx)` |
| 正在播放信息（Wi-Fi 主机桥） | ⏳ stub | `platform/host_link.h`：`host_link_get_now_playing(&np)`，`host_link_register_now_playing_cb()`，`host_link_get_state()`；结构体含 title/artist/album/position_ms/duration_ms/volume_pct/playing/status/cover_handle/placeholder。stub 返回占位“未连接电脑”（`np.placeholder=true`）；`ROUND_REMOTE_HOST_LINK_STUB_DEMO=y` 时返回走动的示例歌曲。消息格式与 `host/omarchy-bridge` protocol v1 对齐（hello / state / command / command_result），`host_link_format_hello/command()` 已实现 |
| 宠物语音（设备端识别） | ⏳ stub | `platform/pet_voice_backend.h`：`pet_voice_backend_start(cb)` 返回 `ESP_ERR_NOT_SUPPORTED`，`pet_voice_backend_available()` = false；真实后端（ESP-SR）按头文件约定另建文件实现 |

回调在后台任务中执行，UI 侧需先 `board_port_ui_lock()` 再操作 LVGL。

Kconfig（`idf.py menuconfig` → Round screen remote）：
- `ROUND_REMOTE_PTT_HID_USAGE` 默认 `0x68`（F13），范围 0x00–0xDF，0 = 关闭 PTT；`ROUND_REMOTE_PTT_HID_MODIFIERS` 默认 0。
- `ROUND_REMOTE_PTT_MAX_HOLD_MS` 默认 120000，0 = 关闭看门狗。
- `Host link backend`：Stub（默认）/ Wi-Fi WebSocket（未实现，选中会构建失败）；`HOST/PORT/TOKEN` 预留，令牌勿提交。
- `Pet voice backend`：Stub（默认）/ ESP-SR（未实现，选中会构建失败）。

## 电子吧唧视频播放暂时关闭

设备上进入视频播放页会出现花屏，因此当前固件已移除该页的导航入口、LVGL 播放层和固件中的播放器/上传服务编译项。视频播放与上传源文件留在仓库中供后续修复，但不会进入此版本固件；旧版已烧录固件不受此次代码修改影响。秒表、泡泡纸和 BLE HID 八键工具仍保留。

`firmware/partitions.csv` 与原有 16MB Flash 布局保持不变，其中 `badge` 数据分区仍占 8 MiB；本次只是停用功能，没有格式化、擦除或重分配分区。重新启用前仍需先定位并修复设备显示问题。

## 构建

```sh
cd firmware
idf.py set-target esp32s3
idf.py menuconfig   # 默认 SKU 为“未核实”；可在核对板背铭牌后记录
idf.py build
```

当前 family-level 默认配置为 16 MB flash、single-app-large 分区布局（partition table 位于 0x8000，factory app 位于 0x10000；应用分区大小 0x400000）。用户已确认板卡标称 32MB，但这不足以证明它与当前 16MB 分区表、具体屏幕或引脚配置兼容；必须先核对板背完整型号/SKU。GitHub CI 产物是构建验证包，不代表已实机验证或适配目标板。

项目固定 Waveshare BSP 2.0.5，组件依赖保存在 `idf_component.yml` 与 `dependencies.lock`。目标 ESP-IDF 为 5.5 系列；工程默认启用 ESP32-S3 octal PSRAM 与 BLE/Bluedroid。

## 尚需输入与板上验证

- 记录/确认板背完整 SKU，避免把系列默认 flash/分区参数误套到不同子型号。
- PTT 默认发送 F13：在电脑的语音/听写应用里把“按住说话”绑定到 F13；如需别的键，在 `menuconfig` 中改 usage。
- BLE HID 当前提供标准媒体键和可配置 PTT 键盘传输；Windows/macOS/Linux 的配对与各应用响应需实机测试。
- Wi-Fi/WebSocket host bridge 需要目标主机地址、SSID/密码与本地令牌和协议定义；凭据不应提交到仓库。
- Linux 构建机与用户 Mac 是不同设备；远程构建不代表 Mac 串口已验证，也不等于实机刷写。刷写前必须在 Mac 上核对芯片、板型和端口。

## The Badge 参考与板型边界

本次参考 [curisama/The-Badge](https://github.com/curisama/The-Badge) 的可玩小工具方向；新增 `modules/badge_tools.*` 是本项目独立实现，没有复制该仓库源文件。参考项目标注 MIT License（Copyright 2026 curisama），见 [`docs/third-party-notices.md`](../docs/third-party-notices.md)。The Badge 面向 Waveshare ESP32-S3-Touch-AMOLED-1.75C；此仓库的现有固件仍保留当前 16MB 默认分区布局，没有套用它的 32MB/大录音分区表。用户报告手上是 32MB 板，但完整 SKU/外围设备仍未确认，因此这批新增工具仅使用 RAM，不代表该板可以直接刷当前镜像。

工具页操作：主圆按钮启动/暂停秒表或戳泡泡；左侧按钮复位/清零，右侧按钮切换工具。秒表只在进入工具页期间运行，离页自动暂停；数值在本次上电期间保留，不写入 NVS/Flash。
