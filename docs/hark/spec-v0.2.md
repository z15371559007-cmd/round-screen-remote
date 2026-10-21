# Omarchy Orb 规格 v0.2（定稿草案）

- 日期：2026-10-21　状态：定稿草案，待用户与 Joi 审阅
- 前置：`spec/phone-pwa-v0.1.md`（草稿，本版按它落地并细化）
- 配套模拟器：`/workspace/work/orb/v3/index.html`（单文件，离线可开；顶部切换「手机 / 圆屏 ESP32」）
- 变更说明：`/workspace/work/orb/v3/CHANGES.md`

**一句话**：手机 PWA 是主力遥控器（小米 17 Pro 安卓 + iPhone，竖屏单手）；ESP32 466×466 圆屏是后续实体配件。两者共用**同一套模块注册表与同一份协议**，由 X1 上现有的 remote-daemon 提供。

---

## ① 功能取舍

### v1 做（4 个模块，按底部标签顺序）

| # | 模块 id | 名称 | 内容 | 空状态 |
|---|---|---|---|---|
| 1 | `nowplaying` | 正在播放 | 统一控制**当前 MPRIS 播放器**，自动跟随音乐/播客/视频；显示来源标签（如「音乐 · spotify」）、标题、艺人/节目、封面（v1 占位色块，按标题取色）、进度条、音量 | 无播放器时显示三个大打开键：音乐 / 播客 / 视频 |
| 2 | `workspaces` | 工作区 | Hyprland 1–9 网格，高亮当前工作区，格内显示窗口数；顶部显示当前窗口标题 + 应用 class | 空工作区显示「（空工作区）」 |
| 3 | `quick` | 快捷 | 4 个大按钮：截图、锁屏、关闭窗口、启动器 | — |
| 4 | `ptt` | PTT | 按住录音（波形 + 计时），松开上传，显示转写 + 处理层级 | 显示示例句提示 |

**PTT 三层处理**

| 层级 | 含义 | v1 示例 | v1 行为 |
|---|---|---|---|
| 第1层 固定命令 | 无参数，正则全匹配 | 暂停、继续、下一首、上一首、静音、锁屏、截图 | 执行 |
| 第2层 带参数 | 带槽位：数字 / 工作区号 / 点歌 | 音量调到30、切到工作区3、播放 周杰伦 晴天 | 执行 |
| 第3层 Agent | 自由语句 | 帮我把今天的会议整理成待办 | **暂缓**：只记录，返回「已记录，v1 不处理」 |

### 推后（v1 不出现任何页面或入口）
媒体库 / 播客选集 / 音乐列表、电视、多主机、宠物、模块排序设置、PTT 第3层 Agent、ESP32 实体硬件。

### 不做
红外（电视用小米遥控器）、PWA 内蓝牙（PWA 无法稳定使用 Web Bluetooth，且 iOS 不支持）。

---

## ② 交互规范

### 2.1 通用
- 竖屏单手。手机主按钮 **≥56px**；**每屏主按钮 ≤4 个**（正在播放的「上一首 / 播放暂停 / 下一首」= 3）。
- 模块切换：手机走**底部标签栏**（4 个）；圆屏走**实体键**「上一模块 / 下一模块」。**左右滑不用于翻页**。
- 反馈：可视提示气泡（toast，22px，停留约 0.9s，如「音量 45」「+15s」「静音」「→ 2:31」），手机端同时调 `navigator.vibrate(10)`（安卓有效；iOS Safari 无此 API，静默跳过）。
- 所有会改变 X1 状态的操作做**乐观更新**（本地先变，再以 WS 推送为准）。

### 2.2 正在播放 · 手势板
- 位置：手机占屏幕上方约 3/5（模拟器中 466px / 844px）；圆屏为整个圆内区域（外环为进度）。
- **只看方向，不分左右区**：首个 12px 位移决定主轴（横 / 纵）并锁定到抬手。

| 手势 | 判定 | 动作 | 反馈 |
|---|---|---|---|
| 轻点 | 位移 <12px，按住 <600ms | 播放 / 暂停，**松手立即执行，无双击、不等待** | ▶ 播放 / ❚❚ 暂停 |
| 上下拖 | 锁定纵轴 | 音量，上=加；手机 4px/级，圆屏 3px/级；拖动中实时显示，150ms 节流发送，松手发终值 | 音量 NN |
| 左右快滑 | 锁定横轴，且 时长 ≤280ms、平均速度 ≥0.45px/ms、位移 ≥36px | 音乐：右=下一首、左=上一首；播客/视频：右=+15s、左=−15s | 下一首 ▶ / +15s |
| 左右慢拖 | 锁定横轴，不满足快滑 | 拖进度：显示目标时间（进度条第二层 + 大号「→ m:ss」），松手提交 seek | → 2:31 / 跳到 2:31 |
| 长按 | 600ms 内未锁定方向 | 静音切换（触发即执行，之后的移动与松手忽略） | 静音 / 取消静音 |

- 慢拖映射：秒/像素 = clamp(时长 / (板宽 × 1.5), 0.25, 8)。4 分钟歌曲约 0.46 s/px；3 小时视频封顶 8 s/px。
- 手势板下方（手机）：标题、艺人、进度条 + 时间、音量条、三个按钮（音乐：上一首/播放暂停/下一首；播客/视频：−15/播放暂停/+15）。
- 判定逻辑为纯函数（`lockAxis` / `classifyRelease` / `createGestureEngine`），已有 15 条 node 单测（`/workspace/work/orb/v3/gesture.test.js`）。

### 2.3 其他模块
- 工作区：轻点格子切换；当前格高亮（主色填充）；点当前格只提示「已在工作区 N」不发命令。
- 快捷：轻点即执行。关闭窗口下方注明作用对象（当前窗口标题）。v1 不加二次确认（见待确认）。
- PTT：按住开始录音，松开发送；**按住 <300ms 丢弃**并提示「按住再说」。结果卡显示「转写 / 层级标签 / 执行结果」。圆屏另有实体 PTT 键，按下自动切到 PTT 模块。

### 2.4 尺寸硬规范
| 外壳 | 触控区 | 主按钮 | 文字 |
|---|---|---|---|
| 手机 390×844 | ≥56px | 64–84px（播放键 72px，PTT 168px） | 正文 ≥13px，标题 22px |
| 圆屏 466×466（沿用 v2） | ≥88px（工作区格 88px） | 120px（快捷、空状态打开键），PTT 180px | ≥20px |

### 2.5 皮肤
主题与内容分离：模块只引用 CSS 变量（`--scr --fg --sub --mut --acc --acc2 --btn --line --trk --ok --warn --bad` 等），换肤只换变量。v1 两套：**Tokyo Night（深色，默认）**、**浅色**（Tokyo Night Day 色系）。圆屏深色下底色为纯黑（AMOLED 省电）。

---

## ③ 内核与模块合同

### 3.1 分层
```
CORE（冻结，改动需单独审批）
  gesture engine   纯函数：方向锁定 / 快滑-慢拖 / 长按
  nav              底部标签（手机）| 实体键上一/下一（圆屏）
  transport        GET /state · WS /ws · POST /cmd · POST /ptt（断线重连、乐观更新）
  state store      GET /state 全量 + WS merge-patch 增量
  skin/theme       CSS 变量
  feedback         toast + vibrate
MODULES（每个模块一个目录，只改自己）
  nowplaying/ workspaces/ quick/ ptt/
```
- 指针事件在屏幕根节点委托：`[data-pad]` → 手势引擎，`[data-act]` → 按钮（按下-抬起位移 <16px 才算点击），`[data-hold]` → 按住。模块重绘不会打断进行中的手势。

### 3.2 模块合同
```js
{
  id: "nowplaying",            // 唯一，也是 POST /cmd 的 module 字段
  name: "正在播放",            // 标签栏 / 圆屏显示名
  icon: "music",               // 内核图标集的 key
  layout: "pad+info",          // 版式声明：pad+info | grid3x3 | grid2x2 | card+hold
  gestures: ["tap:toggle", "vdrag:volume", "hflick:next|prev", "hdrag:seek", "long600:mute"],
  commands: ["toggle","play","pause","next","prev","skip{by}","seek{to}","volume{set}","mute{on?}","open{kind}","playq{artist,title}"],
  reads:   ["player.title", "player.pos", ...],   // 订阅的状态字段（为后续按字段增量渲染/裁剪 WS 预留）
  voice:   ["暂停", "音量调到{n}", "播放 {歌手} {歌名}"], // PTT 句式，daemon 侧解析器按此生成规则
  render(shell, view) { return html },            // shell: "phone" | "orb"；view: {state, ui, shell}
  handle(evt, ctx) { }                            // 合同补充字段：手势/按钮事件入口
}
```
- `ctx` 提供：`state`、`ui`（模块私有 UI 状态）、`cmd(action,args)`（自动带 module）、`optimistic(patch)`、`toast`、`haptic`、`render`。
- **新增模块 = 新目录 + `Registry.register()`，不改 CORE**。注册时校验必填字段，缺字段直接抛错。
- 同一模块对象同时渲染两种外壳（模拟器已演示 4 个模块 × 2 外壳）。
- daemon 侧对应：模块 = 插件（命令执行器 + 状态采集器）+ `cards.json`（模块清单、顺序、启用）。

---

## ④ 通信协议

基于 X1 现有 remote-daemon（systemd 服务，HTTP **:8767**，已有 `GET /state` 与 POST 指令）。**只扩展，不另起服务。**

> ⚠ 以下字段与路径需对照 X1 实际 remote-daemon 接口校正——目前 SSH 未授权，无法核对现有实现。

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/` | 提供 PWA（index.html + manifest + service worker） |
| GET | `/state` | 全量状态（见下） |
| WS | `/ws` | 增量推送：`{"type":"patch","data":{...}}`，按 JSON Merge Patch（RFC 7386）合并；`null` 表示删除（如 `player:null` = 播放器消失） |
| POST | `/cmd` | `{"module","action","args"}` → `{"ok":true}` 或 `{"ok":false,"error":"no_player"}`（409） |
| POST | `/ptt` | `multipart/form-data`：`audio`（audio/webm;codecs=opus，iOS 为 audio/mp4）、`lang=zh-CN` → `{"text","tier","action","result"}` |

### 4.1 `GET /state`
```json
{
  "host": "x1",
  "player": { "source": "spotify", "kind": "music", "title": "晴天", "artist": "周杰伦",
              "pos": 37, "len": 269, "status": "Playing", "volume": 45, "muted": false, "art": null },
  "workspaces": { "active": 1, "list": [ { "id": 1, "windows": 3 }, { "id": 2, "windows": 2 } ] },
  "window": { "title": "nvim ~/orb/spec.md", "class": "kitty" }
}
```
- `player` 字段：`source`（MPRIS 播放器名）、`title`、`artist`、`pos`/`len`（秒）、`status`（Playing|Paused|Stopped）、`volume`（0–100，默认音频输出）、`muted`。
- **建议新增** `kind`（music|podcast|video，daemon 按播放器名 + 元数据推断，决定快滑是切歌还是 ±15s）与 `art`（封面 URL，取自 `mpris:artUrl`，v1 可为 null）。
- WS 推送来源：MPRIS（`playerctl --follow`）、Hyprland socket2（workspace / activewindow）、PipeWire 音量。进度每秒推送一次（客户端也可本地插值）。

### 4.2 `POST /cmd` 动作表（daemon 实现参考）
| module | action | args | daemon 执行（参考） |
|---|---|---|---|
| nowplaying | toggle / play / pause | — | `playerctl -p <src> play-pause / play / pause` |
| nowplaying | next / prev | — | `playerctl -p <src> next / previous` |
| nowplaying | skip | `{by:±15}` | `playerctl -p <src> position 15+ / 15-` |
| nowplaying | seek | `{to:秒}` | `playerctl -p <src> position <to>` |
| nowplaying | volume | `{set:0–100}` | `wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.45` |
| nowplaying | mute | `{on:bool}` | `wpctl set-mute @DEFAULT_AUDIO_SINK@ 1/0` |
| nowplaying | open | `{kind}` | 启动对应播放器（具体命令待定） |
| nowplaying | playq | `{artist,title}` | 点歌（spotify_player / 本地库搜索，待定） |
| workspaces | switch | `{id:1–9}` | `hyprctl dispatch workspace <id>` |
| quick | screenshot / lock / close / launcher | — | `omarchy-cmd-screenshot` / `omarchy-lock-screen` / `hyprctl dispatch killactive` / `omarchy-launch-walker`（以 X1 实际 Omarchy 版本为准） |

- 音量与静音为设备级，挂在 `player` 下只是 UI 归属；无播放器时 volume 命令仍应可用（v1 UI 不暴露）。

### 4.3 `POST /ptt`
- 返回：`{"text":"音量调到30","tier":2,"action":{"module":"nowplaying","action":"volume","args":{"set":30}},"result":"ok"}`；第3层：`{"text":"…","tier":3,"action":null,"result":"已记录，v1 不处理"}`。
- 解析：第1/2层用规则表（由各模块 `voice` 句式生成）；第3层写入日志，v1 不调 Agent。
- ASR 引擎（本地 whisper / 云端）待定，不影响协议。
- **录音需 HTTPS**：浏览器只在安全上下文开放麦克风。方案：内网域名（如 `x1.lan`）+ 自签证书，手机安装并信任根证书；iOS 还需在「证书信任设置」中启用。
- **与 Multica ACRU-569「语音键补三级反馈」对齐**：两边共用同一个 `/ptt` 端点与三层定义，避免出现两套语音管线。

### 4.4 安全与连接
- 只监听内网；v1 用共享令牌（`Authorization: Bearer`，首次扫码写入 PWA）。
- 断线：WS 指数退避重连（1s→30s），重连后先 `GET /state` 全量再接收增量；顶栏显示连接状态点。

### 4.5 ESP32 侧（后续）
- **BLE HID 底盘**：无网络时仍可当媒体键（Consumer Play/Pause、Vol±、Next/Prev）。
- **富状态**：Wi-Fi 下走同一协议（WS /ws + POST /cmd），渲染同一模块注册表的圆屏版式。
- PTT：板载麦克风录音，同样 POST /ptt。

---

## ⑤ 验收与分单

### 规则（每一单都适用）
- **只改自己的模块目录**；CORE 改动单独提单、单独审批。
- 走 git：一单一分支一 PR，提交信息写单号。
- 每单附验收清单，逐条勾选后才合并；在小米 17 Pro 与 iPhone 各实测一遍。

| 顺序 | 单 | 范围 | 验收清单 |
|---|---|---|---|
| 1 | 骨架端到端 | daemon `GET /` 出 PWA、`GET /state`、`WS /ws`、`POST /cmd`；PWA 内核（标签栏、store、transport、toast）；仅一个播放/暂停按钮 | ① 手机打开 `https://x1.lan:8767/` 可安装到主屏；② 点播放/暂停，X1 播放器 ≤150ms 内变化；③ 在 X1 上暂停，手机 ≤1s 内同步；④ 断网重连后状态正确 |
| 2 | 正在播放 | `modules/nowplaying/` + daemon MPRIS/音量插件 | ① 五种手势判定与单测一致（点/上下/快滑/慢拖/长按）；② 音乐/播客/视频自动切换来源标签与快滑语义；③ 空状态三个打开键可用；④ 拖音量时显示数值且不卡顿 |
| 3 | 工作区 + 快捷 | `modules/workspaces/`、`modules/quick/` + socket2 插件 | ① 1–9 切换正确、高亮与 X1 实际一致；② 窗口标题随 X1 焦点变化 ≤1s；③ 四个快捷动作在 X1 生效 |
| 4 | PTT | `modules/ptt/` + daemon `/ptt` | ① HTTPS 下两台手机都能录音；② 6 条示例句层级与执行正确；③ 第3层只记录；④ <300ms 丢弃；⑤ 与 ACRU-569 共用端点 |
| 5 | 皮肤与动效 | skin 变量、过渡动画、震动细节 | ① 两套主题切换无残留硬编码颜色；② 动效不影响手势响应 |

---

## 待确认
1. **主力手机优先级**：小米 17 Pro（安卓）还是 iPhone 先做实测基准？（影响震动、录音格式、证书安装流程的优先顺序）
2. **X1 SSH 授权**：授权后核对 remote-daemon 现有 `GET /state` 字段与 POST 指令格式，校正本规格 ④。
3. **ACRU-569 对齐**：与 Multica「语音键补三级反馈」确认共用 `/ptt` 与三层定义，由哪一方实现解析器。
4. **是否参考开源 UMR**（Go 守护进程 + PWA，已有封面取色 / WS / 音量）：作为 daemon 扩展的参考实现，还是只借鉴思路。
5. 快滑方向：本稿取「右=下一首/+15s，左=上一首/−15s」（与慢拖时间轴方向一致）；若更习惯「左滑下一首」（轮播习惯）需改。
6. 关闭窗口是否需要二次确认（v1 为单击即关）。
7. 圆屏是否恢复 v2 的边缘旋钮（外圈拖动调音量/进度）——v3 圆屏只用与手机相同的中心手势。
