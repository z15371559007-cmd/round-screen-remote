# 圆屏遥控器

466 × 466 圆屏媒体遥控器的 UI 演示与 ESP32-S3 实现框架。GitHub 仓库保持私有。

## 目录

- `index.html`：离线可打开的现有交互演示，不连接硬件。
- `PROJECTS.md`：相关开源项目调研及硬件集成参考。
- `docs/architecture.md`：模块边界、通信方案和当前未实现部分。
- `firmware/`：ESP-IDF 固件骨架，拆为遥控、Push-to-Talk、待机互动宠物三种模式。
- `host/omarchy-bridge/`：Omarchy/Linux 的本地 MPRIS/WebSocket 桥接服务骨架。

## 当前交互模式

1. 遥控：通过局域网桥接控制媒体，并接收播放状态。
2. Push-to-Talk：圆屏按住/松开发送 BLE HID 按键事件，电脑用已配置的语音输入工具和电脑麦克风。
3. 待机互动宠物：宠物独立运行；目标是在板上用离线口令识别驱动表情和动作，不控制媒体。
4. 工具：The Badge 风格的 RAM 秒表与泡泡纸触摸小游戏，沿用现有圆屏表盘视觉。
5. 电子吧唧视频播放：因设备上打开时出现花屏，已在当前固件中暂时移除入口和播放路径；分区表保留不变。

## 运行本地媒体桥接（Linux/Omarchy）

见 [`host/omarchy-bridge/README.md`](host/omarchy-bridge/README.md)。桥接仅在你主动启动时运行；当前版本是本地开发骨架，默认只监听 `127.0.0.1`。

## 固件骨架

见 [`firmware/README.md`](firmware/README.md)。选择 ESP-IDF 是为了后续接入 ESP32-S3 的 BLE、Wi-Fi 和 ESP-SR。当前驱动适配器仍是 stub；构建通过也不代表已能驱动实际屏幕或麦克风。

## The Badge 风格小工具

固件已加入秒表和泡泡纸两个独立模块，并已集成 BLE HID 8 键快捷键环（ESC、ENTER、TAB、SPACE、方向键）；保留媒体/PTT 功能及现有 UI 语言；电子吧唧视频播放已暂时关闭。蓝牙空鼠/触控板、录音/导出、闹钟/时钟和其他小游戏仍未集成。参考项目与许可证说明见 [`docs/third-party-notices.md`](docs/third-party-notices.md)。

## 当前边界

这是初始软件框架，不是可直接烧录使用的成品：真实屏幕/触控驱动、BLE HID 多设备配对、固件 WebSocket 客户端、ESP-SR 识别模型与动画资源尚未接入。目标板型号目前按项目参考中的 Waveshare ESP32-S3-Touch-AMOLED-1.75 假设，尚未实物核对。播放封面只在桥接状态中标示是否存在，图片代理还未实现。
