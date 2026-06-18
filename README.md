# Coding Plan 配额桌面小部件

> 透明液态玻璃 UI 的桌面悬浮小部件，实时监控多个 AI 编程平台的 Coding Plan 配额使用情况。

## ✨ 功能特性

- **多平台支持** — 当前支持 MiniMax 和火山方舟，架构预留智谱/Kimi/腾讯等扩展
- **液态玻璃 UI** — `backdrop-filter: blur(28px)` 透明悬浮窗口
- **智能刷新** — 默认 5 分钟轮询，配额窗口临近重置时自动切换为 5 秒
- **三色阈值** — 🟢 ≥50% / 🟡 10-49% / 🔴 <10% 直观判断配额状态
- **系统托盘** — 最小化到托盘、一键刷新、置顶切换、打开配置文件
- **配置自动迁移** — 旧格式配置文件启动时自动转换并备份
- **零依赖** — HTTP 用 Node 18 内置 `fetch`，签名用 `node:crypto`

## 📦 快速开始

### 环境要求

- **Node.js** 18+（使用内置 `fetch` 和 `AbortSignal.timeout`）
- **Windows** 10/11（当前仅支持 Windows portable）

### 开发模式

```bash
# 安装依赖（首次需下载 Electron ~200MB，建议用镜像加速）
# Windows 用户可设置：$env:ELECTRON_MIRROR="https://npmmirror.com/mirrors/electron/"
npm install

# 启动开发模式
npm run dev
```

### 打包

```bash
npm run build
# 产物：dist/MiniMax-Quota-Widget-0.1.0-x64.exe（~68MB）
```

> Windows SmartScreen 可能弹警告 → 点「更多信息」→「仍要运行」（无代码签名，预期行为）

## ⚙️ 配置

配置文件路径：`~/.minimax-quota-widget/config.json`

> 也支持 `~/.mmx/config.json`（mmx-cli 配置，优先级更高）

### 配置格式

```json
{
  "providers": {
    "minimax": {
      "enabled": true,
      "api_key": "sk-your-minimax-api-key",
      "region": "cn"
    },
    "volcengine": {
      "enabled": true,
      "access_key_id": "your-volcengine-access-key-id",
      "secret_access_key": "your-volcengine-secret-access-key"
    }
  }
}
```

### 配置字段说明

| Provider | 字段 | 说明 |
|----------|------|------|
| `minimax` | `api_key` | MiniMax API Key（从 [MiniMax 开放平台](https://www.minimaxi.com/) 获取）|
| `minimax` | `region` | 区域：`cn`（国内）或 `global`（国际）|
| `volcengine` | `access_key_id` | 火山引擎 AccessKey ID |
| `volcengine` | `secret_access_key` | 火山引擎 Secret Access Key |
| 通用 | `enabled` | 是否启用该 provider（默认 `true`）|

### 首次运行

1. 双击 exe 启动，自动创建示例配置文件
2. 界面提示「请配置 Coding Plan」+ 配置路径
3. 点 **⚙️ 按钮**（标题栏）或 **📂 打开配置文件** 编辑
4. 填入对应平台凭证，保存
5. 点右上角刷新或托盘菜单「刷新额度」

## 🏗️ 项目结构

```
minimax-quota-widget/
├─ src/
│  ├─ main/
│  │  ├─ main.js                 # Electron 入口（窗口/托盘/IPC/调度）
│  │  ├─ preload.js              # contextBridge 安全 IPC 桥接
│  │  ├─ quota-service.js        # 配置管理 + MiniMax API 客户端
│  │  └─ volcengine-service.js   # 火山方舟 V4 签名 + API 客户端
│  ├─ renderer/
│  │  ├─ index.html              # 液态玻璃 UI + 内嵌渲染逻辑
│  │  └─ style.css               # 玻璃质感样式
│  └─ assets/
│     ├─ icon.ico                # 托盘图标（256x256）
│     └─ icon.png                # 应用图标
├─ package.json
├─ README.md
├─ CHANGELOG.md
└─ .gitignore
```

## 🔧 技术栈

| 组件 | 技术 | 说明 |
|------|------|------|
| 框架 | Electron 31.7.7 | 跨平台桌面应用 |
| 语言 | JavaScript（非 TS）| 零构建管线 |
| HTTP | Node 18+ 内置 fetch | 零 HTTP 依赖 |
| 签名 | node:crypto | 火山引擎 V4 HMAC-SHA256（零依赖）|
| 打包 | electron-builder 24 | Windows portable .exe |
| UI | 原生 HTML/CSS | backdrop-blur 液态玻璃 |

## 🎨 UI 布局

```
┌─────────────────────────────────────────────┐
│  CODING PLAN 配额          ⚙️  📌  ─  ✕      │  ← 透明 + backdrop-blur
│                                             │
│  🚀 火山方舟 Coding Plan      运行中         │
│  ┌────────────────────────────────────────┐ │
│  │ 🟢 会话     96%  █████████░    2h 15m │ │
│  │ 🟢 本周     85%  ████████░░    3d 5h  │ │
│  │ 🟡 本月     50%  █████░░░░░    15d    │ │
│  └────────────────────────────────────────┘ │
│  ────────────────────────────────────────   │
│  🤖 MiniMax Coding Plan                     │
│  ⏰ 5h 窗口     重置于 41m                   │
│  ┌────────────────────────────────────────┐ │
│  │ 🟢 general   81%  ████████░░    +20%  │ │
│  │ ⬜ video     100% ██████████          │ │
│  └────────────────────────────────────────┘ │
│  📅 本周 (06-02 → 06-08)                    │
│  ┌────────────────────────────────────────┐ │
│  │ 🟡 general   64%  ██████░░░░    +30%  │ │
│  └────────────────────────────────────────┘ │
│                                             │
│  ● 在线 | 上次刷新 09:23:05 | 自动 5m      │
└─────────────────────────────────────────────┘
```

## 📋 交互说明

| 操作 | 行为 |
|------|------|
| 点击进度条行 | 展开/收起 `已用 / 配额 / 重置时间` 详情 |
| Hover 进度条 | Tooltip 显示详细数字 |
| ⚙️ 标题栏按钮 | 打开配置文件 |
| 📌 标题栏按钮 | 切换窗口置顶 |
| 托盘右键 → 刷新额度 | 立即重新拉取所有 provider 数据 |
| 托盘右键 → 打开配置文件 | 用默认编辑器打开 config.json |

## 📄 隐私原则

- ✅ 只读必要数据（配额接口）
- ❌ 不读 OAuth tokens
- ❌ 不写日志（API Key 绝不进 log）
- ❌ 不上传任何使用数据
- ✅ 数据仅在内存中保留

## 🐛 已知问题

- Windows SmartScreen 拦截（无代码签名）
- Electron 首次安装较慢（~200MB，建议用 npmmirror 镜像）

## 📜 License

MIT
