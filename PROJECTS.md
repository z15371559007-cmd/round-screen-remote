# 五个相关开源项目

调研日期：2026-10-22（Asia/Shanghai）。已读取仓库 README、许可及下述关键源码；未编译或上板验证这些第三方项目。目标板暂按 Waveshare ESP32-S3-Touch-AMOLED-1.75、466×466、CO5300 / CST9217 理解，而非确认用户实物型号。

## 1. Waveshare 官方目标板示例：最直接的固件起点

https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75

- 与当前设计假设相同的 1.75 英寸 466×466 圆形 AMOLED 板，CO5300 QSPI 屏驱动及 CST9217 触控；8 MB PSRAM、16 MB Flash。
- `examples/arduino/06_LVGL_Widgets/06_LVGL_Widgets.ino` 实际初始化 `Arduino_CO5300`、`TouchDrvCST92xx`，并使用 LVGL 音乐 UI。适合先让显示、触摸、刷新与缓冲区工作，再接我们的圆形播放盘。
- 不是 Omarchy 媒体遥控器，也没有现成的 Linux Now Playing 桥接。Arduino 示例使用旧版 LVGL 风格 API；仓库另有 ESP-IDF LVGL 9 示例，应先选开发路线，不能直接混用新旧 API。
- 许可：仓库 Apache-2.0；依赖库仍遵守各自许可。

## 2. LVGL：制作圆封面、弧形音量与进度

https://github.com/lvgl/lvgl

- `src/widgets/arc/lv_arc.c` 与 `demos/music/lv_demo_music.c` 是直接相关实现。可复用弧形控件、图像、触摸事件、音乐界面的组织方式，制作中央可点击封面、左右切换、上下互补圆弧。
- 它是嵌入式 UI 框架，不是带好 Linux 数据连接的成品遥控器。圆形裁切、触控区域、字体、图片解码及显示缓冲仍需针对 466×466 板实现；网页 HTML 不能直接作为 LVGL 固件烧录。
- 主仓库版本与板厂示例依赖必须匹配，尤其 v8/v9 的 API 差异。
- 许可：MIT。

## 3. playerctl：Omarchy 主机端获取状态与控制

https://github.com/altdesktop/playerctl

- 通过 Linux MPRIS / D-Bus 读取标题、艺术家、播放状态、时长与进度等元数据，并执行播放/暂停、上一首/下一首、seek、音量操作；支持符合 MPRIS 的浏览器和播放器。
- `playerctld` 按最近活动管理播放器；`--follow` 可持续输出元数据变化；Python GObject 库接口可订阅媒体事件。适合桥接服务的底座。
- `mpris:artUrl` 是否存在由播放器决定，可能是 Linux 本机 `file://` 或网络 URL。ESP32 不能直接打开主机的本地路径；主机桥接应读取/代理、缩放后提供图片，缺图时显示占位。
- 多个播放器同时运行时要定义选择规则。MPRIS `Volume` 是播放器音量，并不保证等于系统主音量；若控制系统音量，应单独接 PipeWire / WirePlumber。
- 许可：LGPL-3.0。

## 4. KDE Connect：参考“元数据、能力、封面、命令”协议

https://github.com/KDE/kdeconnect-kde

- 已读取 `plugins/mpriscontrol/mpriscontrolplugin.cpp`：发现 MPRIS 播放器、发送标题/艺术家/专辑/时长/进度/封面 URL，并接收媒体控制命令；还区分播放器能力，可用于禁用不支持 seek 或切换的按钮。
- 封面传输会核对请求 URL 是否就是当前播放器的 `mpris:artUrl`，该函数只传 `file://` 本地图片。这是避免把遥控端变成任意文件读取入口的有用参考。
- 不是 ESP32 开箱即用客户端；完整 KDE Connect 协议、配对及安全通道都需要额外实现。建议借鉴数据模型及权限校验，不为这一块圆屏照搬整个协议栈。
- 已读文件 SPDX：`GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL`。仓库包含不同文件许可；复用代码须逐文件遵守，不能把整个项目当成 MIT。

## 5. ESP32 Spotify Player：封面下载、缓存及 Now Playing 的实用参考

https://github.com/Tonio5978/ESP32-Spotify-Player

- 用 LVGL 展示封面、曲名、艺术家和进度，支持播放/暂停、切歌、音量、点按进度和播放设备选择。
- 已读取 `src/spotify.cpp`、`src/ui.cpp`：歌曲换图后下载封面，限制文件大小、临时保存 LittleFS、准备图片数据供 UI 使用。适合研究 ESP32 图像传输、解码和界面线程间协调。
- 原目标是 2.8 英寸 320×240 矩形 CYD（ILI9341 / XPT2046），不是微雪 CO5300 / CST9217 圆屏，固件不能直接刷到目标板。
- 数据来自 Spotify Web API，只解决 Spotify，不解决整个 Linux 的 YouTube/播客/视频统一控制；播放控制需要 Spotify Premium。我们的主路线应保留 MPRIS，不为统一媒体控制引入 Spotify 单源限制。
- 安全限制：源码使用 `setInsecure()` 不校验 TLS 证书，设置页默认为无密码且使用 HTTP；只能参考，不建议照搬这些默认值。
- 许可：GPL-3.0。

## 建议组合

优先组合 1 + 2 + 3；4 用于借鉴协议设计，5 用于借鉴封面处理与嵌入式内存控制，不整体移植。

架构建议：

`Omarchy 上的 MPRIS/playerctl → 本地桥接（状态事件 + 控制命令 + 图片代理）→ 局域网 Wi-Fi → ESP32-S3 / LVGL 圆屏`

桥接需提供播放器标识、曲目标识、标题/艺术家、播放状态、位置/时长、能力标志及缩小后的封面。曲目切换才更新封面；进度在屏幕本地平滑推进，定期与主机校正。播放器消失、无封面、不可 seek 或未知时长都要有明确 UI 状态。系统音量与播放器音量分开定义；桌面切换独立对接 Hyprland，不混入 MPRIS。

生产实现还应限制局域网可达范围、配对设备并验证控制命令；图片代理只服务当前合法媒体封面，不暴露任意文件路径。这是建议架构，当前静态预览尚未实现它。
