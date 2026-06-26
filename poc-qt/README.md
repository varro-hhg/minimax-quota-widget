# Qt 5 C++ — Coding Plan 配额桌面小部件

> 分支: `explore/qt-cpp`
> 日期: 2026-06-26
> 状态: ✅ **全功能完成，编译运行通过**

## 当前进度

| 步骤 | 状态 | 说明 |
|------|------|------|
| Qt 环境就绪 | ✅ | Qt 5.13.1 + MinGW 7.3 @ `E:\Qt` |
| 项目骨架 | ✅ | `.pro` + qmake + mingw32-make |
| 透明圆角窗口 | ✅ | `WA_TranslucentBackground` + `paintEvent` |
| 可拖动标题栏 | ✅ | `mousePressEvent` + `mouseMoveEvent` |
| 自动定位右上角 | ✅ | `QScreen::availableGeometry` |
| MiniMax HTTP 客户端 | ✅ | `QProcess + curl`（绕过 Qt SSL 问题） |
| 火山方舟 V4 签名 | ✅ | `QMessageAuthenticationCode` HMAC-SHA256 |
| 配置文件管理 | ✅ | `QJsonDocument` + 旧格式自动迁移 |
| 系统托盘 | ✅ | `QSystemTrayIcon` + 右键菜单 + 程序化黑色 `M` 图标 |
| 进度条 UI | ✅ | 自绘 `QWidget`（圆角 + 渐变 + Boost 标签） |
| 配额数据展示 | ✅ | 火山 / MiniMax 5h / Weekly 三区域 + 状态栏 |
| 无配置引导 | ✅ | 全屏 overlay + 打开配置文件按钮 |
| 智能刷新 | ✅ | 5min 常规 / 重置前 5s 自适应 |
| exe 图标 | ✅ | `RC_ICONS` 嵌入 icon.ico |
| 固定窗口尺寸 | ✅ | `setFixedSize(460, 580)` |

## 实测产物

```
build/bin/
├─ MiniMaxQuotaWidget.exe    238KB   (业务代码 + 图标资源)
├─ Qt5Core.dll, Qt5Gui.dll, ... ~28MB (Qt 运行时)
└─ platforms/, styles/, imageformats/  (Qt 插件)

总大小: ~34MB
内存占用: ~25MB（实测）
```

对比 Electron 版：

| 维度 | Electron | Qt 5 C++ |
|------|----------|----------|
| exe 大小 | 68MB | **238KB**（-99.6%） |
| 总产物 | 68MB | **34MB**（-50%） |
| 内存 | 60-80MB | **25MB**（-65%） |
| 启动速度 | 慢（Chromium） | 快（原生） |

## 快速开始

### 编译

```bash
# 双击 build.bat 或在 cmd 里运行：
build.bat
```

构建脚本会：
1. 设置 PATH（指向 `E:\Qt\Qt5.13.1`）
2. `qmake` 生成 Makefile
3. `mingw32-make -j` 并行编译
4. `windeployqt` 自动复制依赖 DLL 到 `build/bin/`

### 运行

```bash
cd build/bin && MiniMaxQuotaWidget.exe
```

## 技术栈

| 组件 | 选型 | 说明 |
|------|------|------|
| 框架 | Qt 5.13.1 (Widgets) | 桌面 GUI |
| 编译器 | MinGW 7.3 (g++) | C++17 |
| 构建系统 | qmake + mingw32-make | 简单可靠 |
| HTTP | QProcess + curl | 绕过 Qt 5.13 的 SSL 兼容问题 |
| JSON | QJsonDocument | Qt 内置 |
| HMAC-SHA256 | QMessageAuthenticationCode | Qt 内置（Qt 5.1+） |
| 透明 UI | paintEvent + QPainterPath | 自绘圆角 + 半透明 |
| 部署 | windeployqt | 自动复制 DLL |

**无外部依赖** — 全部用 Qt 内置模块。

## 项目结构

```
poc-qt/
├─ MiniMaxQuotaWidget.pro   # qmake 配置（含 RC_ICONS）
├─ build.bat                # 一键构建脚本
├─ .gitignore               # 屏蔽 build/
├─ README.md
└─ src/
   ├─ main.cpp               # 入口（QApplication）
   ├─ MainWindow.h/cpp        # 主窗口：装配所有模块 + 托盘 + 刷新调度
   ├─ QuotaConfig.h/cpp       # 配置管理：providers 格式 + 旧格式迁移
   ├─ MiniMaxClient.h/cpp     # MiniMax HTTP 客户端（QProcess + curl）
   ├─ VolcengineClient.h/cpp  # 火山方舟 V4 签名 + API 客户端
   ├─ QuotaView.h/cpp         # 配额数据展示视图（火山/5h/周/状态栏）
   ├─ ProgressBar.h/cpp       # 自绘进度条（圆角渐变 + Boost 标签）
   ├─ TrayIcon.h/cpp          # 系统托盘 + 右键菜单
   └─ assets/
      └─ icon.ico             # 应用图标（嵌入 exe）
```

## 关键决策

### 1. 选 Qt 5.13.1 不选 Qt 6

本机已安装 Qt 5.13.1（动态链接 + MinGW），无需重新安装。Qt 5 在我们的简单 UI 场景下与 Qt 6 无差别。

### 2. 选 qmake 不选 CMake

`calendar` 项目已经验证了 qmake 模式可用，且更少配置。CMake 留给 Qt 6 项目用。

### 3. 选 Qt Widgets 不选 QML

我们的 UI 是静态布局 + 进度条 + 透明窗口，QWidget 完全够用，加载更快，无 QML 引擎开销。

### 4. 选动态链接 + windeployqt

本机只有动态链接版 Qt，且 `windeployqt` 已自动处理依赖。34MB 产物可接受。

如果需要单 exe：后续可用 `enigma virtual box` 或 7z SFX 打包。

### 5. HTTP 用 QProcess + curl 而非 QNetworkAccessManager

Qt 5.13.1 的 SSL 库版本较旧，与部分现代 HTTPS 端点存在兼容问题。通过 `QProcess` 调用系统 `curl` 完全绕过此问题，且 `curl` 在 Windows 上普遍可用。

### 6. V4 签名踩坑记录

火山方舟 V4 签名的 `canonicalRequest` 必须包含 **canonical query string**（URL 中的查询参数需排序 + URI 编码后加入签名计算）。遗漏 `Action=GetCodingPlanUsage&Version=2024-01-01` 会导致 `SignatureDoesNotMatch` 错误。

### 7. QByteArray 自引用踩坑

`signRequest()` 中局部变量 `kRegion` / `kService` 与全局 `const char*` 同名，导致 `QByteArray(kRegion)` 初始化时自引用崩溃。修复：加 `::` 前缀明确引用全局作用域。

### 8. 托盘 / 置顶按钮三连修（2026-06-26）

托盘消失 + 置顶按钮无反馈两个 bug 实际是三个独立问题，必须一起修才能闭环：

| 问题 | 根因 | 修复 |
|------|------|------|
| **关闭窗口后托盘消失** | Qt 默认 `quitOnLastWindowClosed = true`，`closeEvent` 里 `hide()` 后没有可见窗口 → 整个应用退出，托盘随之销毁 | `main.cpp` 加 `app.setQuitOnLastWindowClosed(false)` |
| **启动时窗口实际不置顶** | 构造函数里 `setWindowFlags(FramelessWindowHint \| Tool)` 没包含 `WindowStaysOnTopHint`，虽然 `isPinned_ = true` 但状态从未应用到窗口 | 构造时按 `isPinned_` 动态加入 `WindowStaysOnTopHint` |
| **点击置顶按钮无效** | `setWindowFlag()` 必须重建原生 HWND 才能让 `WS_EX_TOPMOST` 生效，单独调 `show()` 在窗口已可见时不会触发重建 | `togglePin()` 中 `hide()` + `show()` 强制重建 |

辅助修复：

- **托盘图标空白** — `TrayIcon` 之前被改成从 `:/icon.ico` 资源加载，但项目根本没有 `.qrc` 文件，且 `QIcon(path)` 不验证文件存在性，结果托盘图标是空 pixmap → Windows 不绘制。回到程序化 `QPainter` 绘制，黑色粗体 `M` on 透明背景（36pt），不依赖任何外部资源
- **标题栏按钮接线遗漏** — 之前的提交虽然在 `QuotaView` 里加了 📌 / ✕ 按钮，但忘了在 `MainWindow` 里 `connect` 它们的 `pinToggled` / `closeRequested` 信号，按钮点了等于没点。补上 connect + `setPinned()` 初始同步

#### Win11 提示

Windows 11 默认把新托盘图标折叠进 `^` 溢出面板，看起来像"消失"。要常驻任务栏需在 *设置 → 个性化 → 任务栏 → 其他系统托盘图标* 里手动开启，或直接从溢出面板拖出来。

#### 验证手段

用 Win32 API 自动化测试（PowerShell + `GetWindowLong` 查 `WS_EX_TOPMOST` exStyle 位）验证：

1. 启动后 `WS_EX_TOPMOST = True` ✅
2. 触发 `togglePin` 后 `TOPMOST = False` ✅
3. 再次触发后 `TOPMOST = True` ✅
4. 发 `WM_CLOSE` 后进程仍存活（托盘保活）✅
5. 发 `WM_CLOSE` 后窗口 `IsWindowVisible = False` ✅

完整测试脚本见 commit 描述，已清理。

## 为什么 Neutralino 不行

详见 [`../poc-neutralino/README.md`](../poc-neutralino/README.md)。简言之：

- WebView2 的 `fetch()` 受 CORS 限制
- `--disable-web-security` 在 WebView2 中不生效
- `os.execCommand` 调用 curl / powershell 时**参数传递有 bug**
- 唯一可行方案是写 C++ 扩展——既然要写 C++，不如直接用 Qt

## 已知问题

| 问题 | 说明 |
|------|------|
| 依赖 curl | Windows 上需系统 PATH 中有 curl（Win10+ 自带） |
| 无代码签名 | SmartScreen 可能拦截（与 Electron 版相同） |
| Win11 托盘默认折叠 | 新托盘图标会被收进溢出面板（`^`），需手动开启常驻或拖出来 |

## License

MIT
