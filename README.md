# 圆屏遥控器 · 本地与 GitHub 静态预览

此目录是现有 v6 UI 的静态发布准备稿；尚未上传到 GitHub，也未启用 GitHub Pages。

## 文件

- `index.html`：原先交付的 `round-remote-v6-local.html` 原样改名，未改变界面、交互或样例内容。
- `.nojekyll`：便于直接作为 GitHub Pages 静态文件发布。
- `PROJECTS.md`：五个相关开源项目的用途、兼容限制和建议架构。

## 本地预览

直接用 Chrome / Chromium 打开 `index.html`。不需安装依赖，也不需联网。

若希望通过本机 HTTP 访问，在此目录运行 `python3 -m http.server 8000 --bind 127.0.0.1`，然后打开 `http://127.0.0.1:8000`。

## 当前边界

这是交互演示，不是 ESP32 固件，不会连接 Omarchy。界面使用样例媒体及模拟状态，刷新后重置。没有实施此前暂缓的播放键透明度修改。

## 发布待决定

建议新仓库名 `round-screen-remote`。发布前须选定仓库与可见性：公开仓库并开启公开 Pages，或仅保存到私有仓库、不发布公开预览。私有仓库的 Pages 资格取决于 GitHub 套餐；此包不要求付费或升级。

选定公开发布后，可将本目录文件提交到 `main`，并把 Pages 的发布来源设为 `main` 根目录。仓库写入成功与网页上线是两个步骤，须分别验证；这里尚未执行这两个步骤。

## 素材与许可

封面示例为 Queen《A Night at the Opera》，来自原演示中使用的 uDiscoverMusic 图片，已内嵌于 HTML。来源：
https://www.udiscovermusic.com/wp-content/uploads/2017/07/Queen-A-Night-At-The-Opera-album-cover-with-border-web-optimised-820.jpg

该第三方唱片封面的版权不随演示代码转移，本包没有为它授予开源许可。公开再发布或产品化前应确认相应使用权，或另行同意替换为可发布素材。本次为忠实保留原稿，未自行替换封面。

## 验证

以实际 `index.html` 运行浏览器测试：离线启动、内嵌封面、播放/暂停与计时、混合媒体切换、键盘音量/静音/进度、桌面选择/返回、圆弧音量和进度拖动、横向滑动、重置、手机及桌面视口均通过。未观察到页面脚本错误或外部网络请求。该测试不代表硬件或 Linux 桥接已实现。
