# Changelog

本文件记录 Coding Plan 配额桌面小部件的所有重要变更。

## [0.1.0] - 2026-06-17

### 初始发布

首个可用版本，支持 MiniMax + 火山方舟双 Coding Plan 配额监控。

#### 核心功能

- **MiniMax Coding Plan 配额监控**
  - 5h 窗口 + 本周窗口双进度条
  - 多模型（general / video / speech / image）分行显示
  - 颜色阈值：🟢 ≥50% / 🟡 10-49% / 🔴 <10%
  - 点击行展开详情（已用/配额/重置时间/突增标签）
  - Boost 突增标签显示（千分比 > 1000 时）

- **火山方舟 Coding Plan 配额监控**
  - session / weekly / monthly 三级配额
  - 火山引擎 V4 HMAC-SHA256 签名认证（零依赖 node:crypto 实现）
  - POST 到 `open.volcengineapi.com`，`Action=GetCodingPlanUsage&Version=2024-01-01`
  - 剩余百分比 + 进度条 + 重置倒计时

- **多 Provider 配置架构**
  - `providers` 统一容器结构，每个 provider 为独立 key
  - 支持 `enabled` 开关控制
  - 旧格式自动迁移（检测根级 `api_key` / `volcengine` → 转换为 `providers` 结构）
  - 迁移时自动备份原文件为 `.bak`

- **UI 与交互**
  - 液态玻璃 UI（`backdrop-filter: blur(28px) saturate(180%)`）
  - 窗口标题：「Coding Plan 配额」
  - 标题栏按钮：⚙️ 设置 / 📌 置顶 / ─ 最小化 / ✕ 关闭
  - 系统托盘：显示/隐藏、刷新额度、置顶切换、📂 打开配置文件、退出
  - 无配置时全屏 overlay 引导（显示配置路径 + 打开按钮）
  - Footer 状态栏（在线/离线 + 上次刷新时间）

- **智能刷新调度**
  - 默认 5 分钟轮询
  - 5h 窗口重置时间 < 2 分钟时切换为 5 秒轮询
  - MiniMax 和火山方舟并行请求（`Promise.allSettled`）

- **打包与分发**
  - electron-builder portable 打包为单 .exe（~68MB）
  - 免安装，双击即可运行

#### 技术实现

| 模块 | 文件 | 职责 |
|------|------|------|
| 主进程入口 | `src/main/main.js` | 窗口创建、托盘、IPC 处理、刷新调度 |
| 配置管理 | `src/main/quota-service.js` | providers 配置读写、自动迁移、MiniMax API 客户端 |
| 火山方舟服务 | `src/main/volcengine-service.js` | V4 签名、API 调用、数据标准化 |
| IPC 桥接 | `src/main/preload.js` | contextBridge 安全暴露 API |
| 渲染层 | `src/renderer/index.html` | 液态玻璃 UI + 内嵌渲染逻辑 |
| 样式 | `src/renderer/style.css` | 玻璃质感 + 进度条 + 响应式布局 |

#### 开发过程记录

| 日期 | 里程碑 | 备注 |
|------|--------|------|
| 2026-06-05 | 项目设计文档完成 | 基于 codex-led-widget + mmx-cli 逆向分析 |
| 2026-06-17 | v0.1.0 初始实现 | MiniMax 单平台 + 液态玻璃 UI |
| 2026-06-17 | 修复 IPC 竞态条件 | refreshQuota 移至 did-finish-load 事件 |
| 2026-06-17 | 新增火山方舟模块 | V4 签名 + 三级配额 |
| 2026-06-17 | UI 布局调整 | 火山方舟上移，MiniMax 下移 |
| 2026-06-17 | 配置架构重构 | providers 统一结构 + 自动迁移 + 设置入口 |

#### 已知坑点

| 问题 | 解决方案 |
|------|---------|
| Electron 安装超时 | `ELECTRON_MIRROR=https://npmmirror.com/mirrors/electron/` |
| IPC 竞态（消息丢失） | `refreshQuota()` 放 `did-finish-load` 而非 `app.whenReady()` |
| NSIS 二进制缺失 | 手动下载 nsis-3.0.4.1.7z 用 7za 解压到缓存 |
| 图标尺寸不足 | 必须 ≥256×256 px |
| spawn EPERM | electron-builder 需沙箱外执行 |
| exe 被文件锁定 | 打包前先关闭运行中的 Electron/Widget 进程 |

---

## 路线图（未实现）

- [ ] 多账户切换（global + cn）
- [ ] macOS 支持（vibrancy + dock badge）
- [ ] Linux AppImage
- [ ] 本地 SQLite 存快照 + 折线图趋势分析
- [ ] 配额 < 20% 系统通知告警
- [ ] 代码签名 + auto-update
- [ ] 更多 provider（智谱 / Mimo / Kimi / 腾讯等）
