# Qt 5 PoC — Coding Plan 配额小部件

> 分支: `explore/qt-cpp`
> 日期: 2026-06-24
> 状态: 🚀 **PoC 编译运行成功**

## 当前进度

| 步骤 | 状态 | 说明 |
|------|------|------|
| Qt 环境就绪 | ✅ | Qt 5.13.1 + MinGW 7.3 @ `E:\Qt` |
| 项目骨架 | ✅ | `.pro` + qmake + mingw32-make |
| 透明圆角窗口 | ✅ | `WA_TranslucentBackground` + `paintEvent` |
| 可拖动标题栏 | ✅ | `mousePressEvent` + `mouseMoveEvent` |
| 自动定位右上角 | ✅ | `QScreen::availableGeometry` |
| MiniMax HTTP 客户端 | ⏳ | 待接入（`QNetworkAccessManager`） |
| 火山方舟 V4 签名 | ⏳ | 待接入（`QCryptographicHash` HMAC-SHA256） |
| 配置文件管理 | ⏳ | 待接入（`QJsonDocument` + `QStandardPaths`） |
| 系统托盘 | ⏳ | 待接入（`QSystemTrayIcon`） |
| 进度条 UI | ⏳ | 待接入（自绘 `QWidget`） |

## 实测产物

```
build/bin/
├─ MiniMaxQuotaWidget.exe    28KB   (业务代码本体)
├─ Qt5Core.dll, Qt5Gui.dll, ... ~28MB (Qt 运行时)
└─ platforms/, styles/, imageformats/  (Qt 插件)

总大小: ~28MB
内存占用: 23MB（实测）
```

对比：
- **Electron 版**：68MB exe，60-80MB 内存
- **Qt 版**：28MB（10x 缩减），23MB（3x 缩减）

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
| HTTP | QNetworkAccessManager | Qt Network 模块 |
| JSON | QJsonDocument | Qt 内置 |
| HMAC-SHA256 | QMessageAuthenticationCode | Qt 内置（Qt 5.1+） |
| 透明 UI | paintEvent + QPainterPath | 自绘圆角 + 半透明 |
| 部署 | windeployqt | 自动复制 DLL |

**无外部依赖** — 全部用 Qt 内置模块。

## 项目结构

```
poc-qt/
├─ MiniMaxQuotaWidget.pro   # qmake 配置
├─ build.bat                # 一键构建脚本
├─ .gitignore               # 屏蔽 build/
└─ src/
   ├─ main.cpp              # 入口（QApplication）
   ├─ MainWindow.h          # 主窗口声明
   └─ MainWindow.cpp        # 透明窗口 + 拖拽实现
```

## 关键决策

### 1. 选 Qt 5.13.1 不选 Qt 6

本机已安装 Qt 5.13.1（动态链接 + MinGW），无需重新安装。Qt 5 在我们的简单 UI 场景下与 Qt 6 无差别。

### 2. 选 qmake 不选 CMake

`calendar` 项目已经验证了 qmake 模式可用，且更少配置。CMake 留给 Qt 6 项目用。

### 3. 选 Qt Widgets 不选 QML

我们的 UI 是静态布局 + 进度条 + 透明窗口，QWidget 完全够用，加载更快，无 QML 引擎开销。

### 4. 选动态链接 + windeployqt

本机只有动态链接版 Qt，且 `windeployqt` 已自动处理依赖。28MB 产物可接受。

如果需要单 exe：后续可用 `enigma virtual box` 或 7z SFX 打包。

## 为什么 Neutralino 不行

详见 [`../poc-neutralino/README.md`](../poc-neutralino/README.md)。简言之：

- WebView2 的 `fetch()` 受 CORS 限制
- `--disable-web-security` 在 WebView2 中不生效
- `os.execCommand` 调用 curl / powershell 时**参数传递有 bug**
- 唯一可行方案是写 C++ 扩展——既然要写 C++，不如直接用 Qt

## 下一步

1. **接入 MiniMax HTTP 客户端** — `QNetworkAccessManager` + Bearer Auth
2. **接入火山方舟** — `QMessageAuthenticationCode` 实现 V4 签名
3. **配置管理** — `~/.minimax-quota-widget/config.json` 读写 + 自动迁移
4. **系统托盘** — `QSystemTrayIcon` + 右键菜单
5. **进度条 UI** — 复用 `calendar` 项目的 `paintEvent` 模式

参考 `D:\projects\calendar` 项目作为 Qt 应用模板。

## License

MIT
