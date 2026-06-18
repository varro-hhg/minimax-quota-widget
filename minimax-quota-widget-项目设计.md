# minimax-quota-widget 项目设计

> 文档创建：2026-06-05
> 最后更新：2026-06-17
> 项目状态：✅ v0.1.0 已发布（支持 MiniMax + 火山方舟双 Coding Plan）
> 技术栈：Electron 31 + electron-builder 24（Windows portable）
> 数据源：MiniMax `GET /v1/token_plan/remains` + 火山方舟 `POST GetCodingPlanUsage`

---

## 一、项目背景与动机

### 为什么做这个工具

主人当前痛点（2026-06-05 MiniMax 限流事件后浮现）：

- 使用 MiniMax Token Plan，每天 5h 窗口 + 周窗口配额不直观
- 现有方案 `mmx quota` 每次要在终端跑，**没法"瞄一眼"就知道还剩多少**
- 突发 2062 限流时（5 分钟内 3 个 cron 同时跑），**需要快速判断是 MiniMax 全局降级还是只是自己触发**
- 同时用 `general` + `video` 两类模型，**单一条进度条不够看**

### 参考项目

**codex-led-widget**（github.com/xicunwus2025-sys/codex-led-widget）

- Windows 桌面悬浮小组件
- 透明液态玻璃界面，红黄绿三色 LED
- 调 codex CLI 的 app-server JSON-RPC 协议读 `account/rateLimits/read`
- 不读 / 不存 / 不上传 Token（子进程继承父进程登录态）

**MiniMax 端与 codex 端的差异（核心发现）**：

| 维度 | codex | minimax |
|------|-------|---------|
| 协议 | codex CLI JSON-RPC（stdio:// NDJSON）| **HTTP REST**（一次性 JSON）|
| 认证 | 子进程继承父进程登录态 | **API Key**（已存 `~/.mmx/config.json`）|
| CLI 依赖 | 强耦合 | **零依赖** |
| 数据维度 | 1 条（5h primary）| **2 段 × 多模型**（5h + 周 × general/video/...）|
| 跨平台 | Windows only | **Win/Mac/Linux** |

→ 我们的实现比 codex 项目**简单一半**（无 JSON-RPC 协议层），但 UI 更丰富（两段进度条 + 多模型）。

---

## 二、核心数据源（MiniMax 公开 API）

### Endpoint

```js
function getQuotaUrl(baseUrl) {
  return `${baseUrl}/v1/token_plan/remains`
}
```

| 区域 | baseUrl | quota endpoint |
|------|---------|---------------|
| `global` | `https://api.minimax.io` | `https://api.minimax.io/v1/token_plan/remains` |
| `cn` | `https://api.minimaxi.com` | `https://api.minimaxi.com/v1/token_plan/remains` |

来源：`mmx-cli/dist/mmx.mjs:251` 实际定义（2026-05-27 mmx-cli 1.x 版本）。

### 请求

```http
GET /v1/token_plan/remains HTTP/1.1
Host: api.minimaxi.com
Authorization: Bearer sk-cp-...
x-api-key: sk-cp-...   # 兜底
Content-Type: application/json
```

### 响应（实测 2026-06-05 09:08）

```json
{
  "model_remains": [
    {
      "model_name": "general",
      "start_time": 1780606800000,
      "end_time": 1780624800000,
      "remains_time": 2518575,
      "current_interval_total_count": 0,
      "current_interval_usage_count": 0,
      "current_weekly_total_count": 0,
      "current_weekly_usage_count": 0,
      "weekly_start_time": 1780243200000,
      "weekly_end_time": 1780848000000,
      "weekly_remains_time": 225718575,
      "current_interval_status": 1,
      "current_interval_remaining_percent": 81,
      "current_weekly_status": 1,
      "current_weekly_remaining_percent": 64,
      "interval_boost_permille": 2000,
      "weekly_boost_permille": 3000
    },
    {
      "model_name": "video",
      "current_interval_remaining_percent": 100,
      "current_weekly_remaining_percent": 100,
      "current_interval_status": 3,
      "current_weekly_status": 3
    }
  ],
  "base_resp": { "status_code": 0, "status_msg": "success" }
}
```

### 关键字段

| 字段 | 含义 | 用途 |
|------|------|------|
| `current_interval_remaining_percent` | 5h 窗口剩余百分比 | **5h 进度条** |
| `current_weekly_remaining_percent` | 周窗口剩余百分比 | **周进度条** |
| `remains_time` | 下次重置剩余毫秒 | **倒计时**（5h / 周）|
| `current_interval_usage_count` | 5h 已用 | 详情展开 |
| `current_interval_total_count` | 5h 配额上限 | 详情展开 |
| `interval_boost_permille` | 5h 临时突增（千分比）| 详情展开 |
| `weekly_boost_permille` | 周临时突增（千分比）| 详情展开 |
| `current_interval_status` | 状态码：1=正常 / 3=充足 / 0=用完 | 颜色判断 |
| `model_name` | 模型分类 | 行标识（general / video / speech / image）|

### 状态判断（颜色阈值）

参照 codex 项目阈值 + MiniMax 数据含义调整：

| 状态码 | 含义 | 颜色 |
|--------|------|------|
| `status === 1`（正常区间 1-50%）| 较紧张 | 🟡 黄 |
| `status === 3`（充足，>50%）| 富裕 | 🟢 绿 |
| `status === 0` 或 percent=0 | 用完 | 🔴 红 |

更直接用 `remaining_percent` 阈值：
- 🟢 ≥ 50%（MiniMax 套餐默认余量较多，< 50% 算偏紧）
- 🟡 10% ≤ x < 50%
- 🔴 < 10% 或 0%

> **注**：codex 项目阈值是 10%，因为 codex 单次会话消耗大；MiniMax 5h 窗口较大，**主人实际焦虑点**是"还能不能正常用"，建议 ≥50% 绿 / <10% 红更贴合体感。

---

## 三、项目骨架

```
minimax-quota-widget/
├─ src/
│  ├─ main/
│  │  ├─ main.js                  # Electron 入口 + 窗口/托盘/IPC/调度
│  │  ├─ preload.js               # contextBridge 暴露安全 IPC
│  │  ├─ quota-service.js         # ★ 配置管理 + MiniMax HTTP API 客户端
│  │  └─ volcengine-service.js    # ★ 火山方舟 V4 签名 + API 客户端
│  ├─ renderer/
│  │  ├─ index.html               # 单文件 HTML（液态玻璃 UI）
│  │  └─ style.css                # 玻璃质感样式
│  └─ assets/
│     ├─ icon.ico                 # Windows 托盘图标（256x256）
│     └─ icon.png                 # 应用图标
├─ package.json
├─ README.md
├─ CHANGELOG.md
└─ .gitignore
```

**多 Provider 架构**：
- `quota-service.js` 提供 `loadProviderConfig(name)` 通用函数，支持 `providers` 统一配置
- `volcengine-service.js` 实现火山引擎 V4 HMAC-SHA256 签名（零依赖 `node:crypto`）
- 新增 provider 只需新建 service 文件 + 在配置 `providers` 下加 key

---

## 四、技术选型

| 维度 | 选择 | 理由 |
|------|------|------|
| 框架 | **Electron 31+** | 跟 codex 项目零学习成本；跨平台一份代码 |
| 语言 | **JavaScript**（非 TS）| 单文件 < 200 行，TS 收益小，构建复杂度↑ |
| HTTP 客户端 | **Node 18+ 内置 fetch** | 不用 axios/node-fetch；零依赖 |
| 打包 | **electron-builder** portable | 单 .exe 免安装，codex 项目同款 |
| 图标 | **system tray + .ico** | Windows 托盘要求 .ico |
| 自启动 | `app.setLoginItemSettings()` | Electron 原生 API |
| API Key 加密 | `safeStorage.encryptString()` | 跨用户安全（Windows DPAPI / macOS Keychain / Linux libsecret）|

### package.json 关键字段

```json
{
  "name": "minimax-quota-widget",
  "version": "0.1.0",
  "main": "src/main/main.js",
  "scripts": {
    "dev": "electron .",
    "start": "electron .",
    "build": "set CSC_IDENTITY_AUTO_DISCOVERY=false && electron-builder --win portable",
    "build:mac": "electron-builder --mac",
    "build:linux": "electron-builder --linux AppImage"
  },
  "build": {
    "appId": "com.minimax.quota.widget",
    "productName": "MiniMax Quota Widget",
    "directories": { "output": "dist" },
    "files": ["src/**/*", "package.json"],
    "win": {
      "target": [{ "target": "portable", "arch": ["x64"] }],
      "signAndEditExecutable": false
    },
    "portable": { "artifactName": "MiniMax-Quota-Widget-${version}-${arch}.exe" }
  },
  "devDependencies": {
    "electron": "^31.7.7",
    "electron-builder": "^24.13.3"
  }
}
```

---

## 五、quota-service.js 核心实现

### 5.1 读配置文件（providers 统一结构 + 旧格式自动迁移）

```js
// src/main/quota-service.js
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');

// 新配置格式：providers 统一容器
// { "providers": { "minimax": {...}, "volcengine": {...} } }

function loadProviderConfig(name) {
  const cfg = readConfig();  // 读 providers 结构
  if (!cfg) return null;
  const providers = cfg.providers || {};
  const prov = providers[name];
  if (prov && prov.enabled !== false) {
    // 新格式：从 providers.<name> 读取
    if (name === 'minimax') return { apiKey, region, baseUrl };
    if (name === 'volcengine') return { accessKeyId, secretAccessKey };
  }
  // Fallback: 旧格式兼容（根级 api_key / volcengine）
  return null;
}

function migrateOldConfig() {
  // 检测旧格式 → 转换为 providers → 备份 .bak
}
```

### 5.2 拉额度（直 HTTP GET，零协议层）

```js
async function getQuota({ baseUrl, apiKey }, { timeoutMs = 10_000 } = {}) {
  const url = `${baseUrl}/v1/token_plan/remains`;
  const headers = {
    'Authorization': `Bearer ${apiKey}`,
    'x-api-key': apiKey,  // 兜底
    'Content-Type': 'application/json'
  };

  const res = await fetch(url, {
    headers,
    signal: AbortSignal.timeout(timeoutMs)
  });

  if (!res.ok) {
    const body = await res.text().catch(() => '');
    throw new Error(`HTTP ${res.status}: ${body.slice(0, 200)}`);
  }

  const json = await res.json();
  if (json.base_resp?.status_code !== 0) {
    throw new Error(json.base_resp?.status_msg || 'MiniMax API error');
  }

  return (json.model_remains || []).map(normalizeModel);
}

function normalizeModel(m) {
  return {
    modelName: m.model_name || 'unknown',
    intervalRemainPercent: clampPercent(m.current_interval_remaining_percent ?? 100),
    weeklyRemainPercent: clampPercent(m.current_weekly_remaining_percent ?? 100),
    intervalUsed: m.current_interval_usage_count ?? 0,
    intervalTotal: m.current_interval_total_count ?? 0,
    weeklyUsed: m.current_weekly_usage_count ?? 0,
    weeklyTotal: m.current_weekly_total_count ?? 0,
    intervalResetsInMs: m.remains_time ?? null,
    intervalBoostPermille: m.interval_boost_permille ?? 0,
    weeklyBoostPermille: m.weekly_boost_permille ?? 0,
    intervalStatus: m.current_interval_status ?? null,
    weeklyStatus: m.current_weekly_status ?? null,
    weeklyStart: m.weekly_start_time ?? null,
    weeklyEnd: m.weekly_end_time ?? null,
    fetchedAt: new Date().toISOString()
  };
}

function clampPercent(v) {
  if (!Number.isFinite(v)) return 0;
  return Math.max(0, Math.min(100, Math.round(v)));
}
```

### 5.3 颜色判定（统一函数）

```js
function colorOf(percent) {
  if (percent === 0) return 'red';
  if (percent < 10) return 'red';
  if (percent < 50) return 'yellow';
  return 'green';
}

function formatDuration(ms) {
  if (ms <= 0) return '—';
  const h = Math.floor(ms / 3_600_000);
  const m = Math.floor((ms % 3_600_000) / 60_000);
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
}
```

---

## 六、UI 设计（液态玻璃 + 两段进度条）

```
┌─────────────────────────────────────────────┐
│  CODING PLAN 配额          ⚙️  📌  ─  ✕      │  ← 透明 + backdrop-blur
│                                             │
│  🚀 火山方舟 Coding Plan      运行中         │
│  ┌────────────────────────────────────────┐ │
│  │ 🟢 会话     96%  █████████░    2h 15m │ │  ← session 配额
│  │ 🟢 本周     85%  ████████░░    3d 5h  │ │  ← weekly 配额
│  │ 🟡 本月     50%  █████░░░░░    15d    │ │  ← monthly 配额
│  └────────────────────────────────────────┘ │
│  ────────────────────────────────────────   │
│  🤖 MiniMax Coding Plan                     │
│  ⏰ 5h 窗口     重置于 41m                   │
│  ┌────────────────────────────────────────┐ │
│  │ 🟢 general   81%  ████████░░    +20%  │ │  ← 5h 进度
│  │ ⬜ video     100% ██████████          │ │     boost 标签
│  └────────────────────────────────────────┘ │
│  📅 本周 (06-02 → 06-08)                    │
│  ┌────────────────────────────────────────┐ │
│  │ 🟡 general   64%  ██████░░░░    +30%  │ │  ← 周进度
│  └────────────────────────────────────────┘ │
│                                             │
│  ● 在线 | 上次刷新 09:23:05 | 自动 5m      │  ← footer
└─────────────────────────────────────────────┘
```

### 6.1 视觉设计

```css
/* 液态玻璃 + 半透明 */
body {
  background: rgba(20, 20, 30, 0.65);
  backdrop-filter: blur(24px) saturate(180%);
  -webkit-backdrop-filter: blur(24px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 16px;
  color: #fff;
  font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
}

.bar { height: 6px; border-radius: 3px; overflow: hidden; }
.bar-green { background: linear-gradient(90deg, #4ade80, #16a34a); }
.bar-yellow { background: linear-gradient(90deg, #facc15, #ca8a04); }
.bar-red { background: linear-gradient(90deg, #f87171, #dc2626); }

.boost-tag {
  font-size: 9px;
  background: rgba(99, 102, 241, 0.3);
  padding: 1px 5px;
  border-radius: 6px;
  margin-left: 4px;
}
```

### 6.2 关键交互

| 触发 | 行为 |
|------|------|
| 进度条点击 | 展开/收起该模型的 `used / total` 详细数字 |
| 托盘"刷新额度" | 立即拉取 + 短暂进度条高亮 |
| 进度条 hover | tooltip 显示"X / Y（重置于 Z）" |
| 状态栏 ● 指示 | 🟢 在线 / 🔴 离线（上次失败） / 🟡 限流中 |

---

## 七、窗口与托盘（main.js 关键代码）

### 7.1 窗口

```js
const { BrowserWindow, screen } = require('electron');
const path = require('node:path');

let mainWindow;
let isAlwaysOnTop = true;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 460,
    height: 320,            // 比 codex 项目大（多一段进度）
    minWidth: 460,
    minHeight: 320,
    frame: false,
    transparent: true,
    resizable: true,         // codex 项目是 false，但 MiniMax 进度条多，给用户调整权
    alwaysOnTop: isAlwaysOnTop,
    skipTaskbar: false,
    show: false,
    backgroundColor: '#00000000',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  mainWindow.loadFile(path.join(__dirname, '../renderer/index.html'));
  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
    placeWindowTopRight();
  });
}

function placeWindowTopRight() {
  if (!mainWindow) return;
  const { workArea } = screen.getPrimaryDisplay();
  const { width, height } = mainWindow.getBounds();
  mainWindow.setBounds({
    x: workArea.x + workArea.width - width - 24,
    y: workArea.y + 24,
    width,
    height
  });
}
```

### 7.2 托盘

```js
const { Tray, Menu, nativeImage } = require('electron');

function createTray() {
  const icon = nativeImage.createFromPath(path.join(__dirname, '../../assets/icon.ico'));
  tray = new Tray(icon);
  tray.setToolTip('MiniMax Quota Widget');
  rebuildTrayMenu();
  tray.on('click', toggleWindow);
}

function rebuildTrayMenu() {
  tray.setContextMenu(Menu.buildFromTemplate([
    { label: '显示/隐藏', click: toggleWindow },
    { label: '刷新额度', click: () => mainWindow.webContents.send('quota:refresh') },
    {
      label: isAlwaysOnTop ? '取消置顶' : '置顶',
      click: () => setAlwaysOnTop(!isAlwaysOnTop)
    },
    { label: '切换 global/cn', click: () => mainWindow.webContents.send('quota:switchRegion') },
    { type: 'separator' },
    { label: '退出', click: () => app.quit() }
  ]));
}
```

### 7.3 刷新调度

```js
// 智能调度：拉完一次后，根据 resets_in 时间定时到下一次
let refreshTimer = null;

function scheduleNextRefresh(resetsInMs) {
  if (refreshTimer) clearTimeout(refreshTimer);
  // 5h 窗口快重置时（< 2min）立刻拉，否则常规 5 分钟
  const interval = (resetsInMs && resetsInMs < 120_000) ? 5_000 : 5 * 60_000;
  refreshTimer = setTimeout(() => refreshQuota(), interval);
}

async function refreshQuota() {
  try {
    const data = await getQuota(loadConfig());
    mainWindow.webContents.send('quota:update', { ok: true, data });
    // 找到 general 模型的 5h resets
    const general = data.find(m => m.modelName === 'general');
    scheduleNextRefresh(general?.intervalResetsInMs);
  } catch (err) {
    mainWindow.webContents.send('quota:update', { ok: false, error: err.message });
    scheduleNextRefresh(null);  // 失败时按默认 5 分钟重试
  }
}
```

---

## 八、配置与安全

### 8.1 配置文件存放（providers 统一结构）

- **优先读** `~/.mmx/config.json`（已有 mmx-cli 配置）
- **fallback** `~/.minimax-quota-widget/config.json`（独立配置）
- **格式**：`providers` 统一容器，每个 provider 为独立 key

```json
{
  "providers": {
    "minimax": {
      "enabled": true,
      "api_key": "sk-cp-...",
      "region": "cn"
    },
    "volcengine": {
      "enabled": true,
      "access_key_id": "AKLT...",
      "secret_access_key": "TVdK..."
    }
  }
}
```

- **自动迁移**：启动时检测旧格式（根级 `api_key` / `volcengine`），自动转换为 `providers` 结构
- **备份**：迁移时原文件备份为 `.bak`
- **设置入口**：标题栏⚙️按钮 + 托盘右键菜单「📂 打开配置文件」

### 8.2 隐私原则（沿用 codex 项目的设计）

- ✅ 只读必要数据（`token_plan/remains`）
- ❌ 不读 `~/.mmx/config.json` 里的 oauth tokens
- ❌ 不写日志（API Key 绝对不进 log）
- ❌ 不上传任何使用数据
- ✅ 数据仅在内存中保留

---

## 九、风险与降级

| 风险 | 触发场景 | 降级方案 |
|------|---------|---------|
| `token_plan/remains` 端点失效 | MiniMax 改 API | 1. 试 `mmx quota` CLI 作为回退 2. 用户手动粘贴 JSON |
| API Key 泄漏（widget 进程被 dump）| 恶意软件 | `safeStorage` 加密 + 不写磁盘明文 |
| MiniMax 5h 窗口临时提升 (`boost_permille`) | MiniMax 营销活动 | UI 显示"+20%" 标签，让用户知道是临时额度 |
| 多模型字段不一致（不同 model 返回字段不同）| API 字段漂移 | `normalizeModel` 用 `?? 100` 兜底所有字段 |
| Windows 没签名被 SmartScreen 拦截 | 首次运行 | README 写"更多信息 → 仍要运行"（codex 同款）|
| 用户装了多个 MiniMax 工具冲突（mmx-cli + widget）| 端口/IPC 冲突 | 零依赖，独立进程，无 IPC 风险 |

---

## 十、路线图

### v0.1（最小可用版） ✅ 已完成
- [x] 单窗口 / 5h + 周 / 多模型进度条（MiniMax）
- [x] 火山方舟 Coding Plan 三级配额（session/weekly/monthly）
- [x] 系统托盘（刷新 / 置顶 / 退出 / 打开配置文件）
- [x] 透明液态玻璃 UI
- [x] 自动读配置（`providers` 统一结构 + 旧格式自动迁移）
- [x] 智能刷新（5min + reset 时立即拉）
- [x] 标题栏⚙️设置按钮
- [x] 并行刷新多 provider（Promise.allSettled）
- [x] Windows portable 打包

### v0.2（扩展 provider）
- [ ] 智谱 Coding Plan
- [ ] Mimo Coding Plan
- [ ] Kimi Coding Plan
- [ ] 腾讯 Coding Plan
- [ ] 多账户切换（global + cn）
- [ ] 启动引导优化（分 provider 提示）

### v0.3（跨平台）
- [ ] macOS（vibrancy + dock badge）
- [ ] Linux AppImage

### v0.4（趋势分析）
- [ ] 本地 SQLite 存 7/30 天快照
- [ ] 折线图显示用量趋势
- [ ] 预测"按当前速度何时用完"

### v0.5（主动告警）
- [ ] < 20% 弹系统通知
- [ ] 5h reset 前 5min 提醒
- [ ] 跨设备同步（用 webhook 推微信）

### v0.6（开源发布）
- [ ] code signing（signpath.io / 自购证书）
- [ ] auto-update（electron-updater）
- [ ] GitHub Actions 自动构建

---

## 十一、参考资料

| 资料 | URL |
|------|-----|
| codex-led-widget 源码 | https://github.com/xicunwus2025-sys/codex-led-widget |
| mmx-cli 实际实现 | `/root/.nvm/versions/node/v22.22.2/lib/node_modules/mmx-cli/dist/mmx.mjs:251`（`getQuotaUrl` 函数）|
| MiniMax API 区域配置 | 同上，`K = {global: 'https://api.minimax.io', cn: 'https://api.minimaxi.com'}` |
| Electron safeStorage | https://www.electronjs.org/docs/latest/api/safe-storage |
| electron-builder portable | https://www.electron.build/configuration/win#portable |
| 实测响应 | 2026-06-05 09:08 `mmx quota` 返回 general 81% / 64%, video 100% |

---

## 十二、下一步

主人确认方案后，建议按以下顺序推进：

1. **v0.0.1 骨架**（30 分钟）—— `npm init` + 装 electron + 写 main.js + 空 renderer，能跑出空白窗口
2. **quota-service.js**（30 分钟）—— 读 `~/.mmx/config.json` + fetch + normalize，console.log 看输出
3. **最小 UI**（60 分钟）—— 静态 HTML + CSS 进度条，手动塞假数据
4. **接 IPC**（30 分钟）—— preload 暴露 API，main → renderer 推数据
5. **托盘 + 调度**（30 分钟）—— 5 分钟定时 + reset 触发
6. **打包测试**（20 分钟）—— electron-builder 出 .exe，主人本地双击验证

**总计 3.5 小时**完成 v0.1 跑通。

---

*文档创建：2026-06-05 15:38 (Asia/Shanghai)*
*最后更新：2026-06-17*
*基于对 codex-led-widget v0.1.0 (2025 末) 源码 + MiniMax mmx-cli 1.x 的逆向分析*
*所有结论都有源码佐证，置信度 🟢 EXTRACTED*

---

## 十三、火山方舟 Coding Plan 模块（v0.1.0 新增）

### 13.1 API 端点

```http
POST /?Action=GetCodingPlanUsage&Version=2024-01-01 HTTP/1.1
Host: open.volcengineapi.com
Content-Type: application/json
Authorization: HMAC-SHA256 Credential=AKxxx/..., SignedHeaders=..., Signature=...
X-Date: 20260617T093133Z
X-Content-Sha256: 44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a
```

### 13.2 响应格式

```json
{
  "ResponseMetadata": {
    "RequestId": "20260617093133...",
    "Action": "GetCodingPlanUsage",
    "Version": "2024-01-01",
    "Service": "ark",
    "Region": "cn-beijing"
  },
  "Result": {
    "Status": "Running",
    "UpdateTimestamp": 1781659893,
    "QuotaUsage": [
      { "Level": "session", "Percent": 4.02, "ResetTimestamp": 1781668954 },
      { "Level": "weekly",  "Percent": 14.93, "ResetTimestamp": 1782057600 },
      { "Level": "monthly", "Percent": 49.90, "ResetTimestamp": 1783699199 }
    ]
  }
}
```

### 13.3 V4 签名实现（src/main/volcengine-service.js）

- 用 `node:crypto` 零依赖实现火山引擎 HMAC-SHA256 V4 签名
- 签名流程：CanonicalRequest → StringToSign → SigningKey → Signature
- 与 AWS SigV4 结构相同，region=cn-beijing, service=ark

### 13.4 配置格式

```json
{
  "providers": {
    "volcengine": {
      "enabled": true,
      "access_key_id": "AKLTxxx",
      "secret_access_key": "TVdKxxx"
    }
  }
}
```

---

## 十四、配置架构演进（v0.1.0）

### 14.1 旧格式（v0.0.x）

```json
{
  "api_key": "sk-xxx",
  "region": "cn",
  "volcengine": {
    "access_key_id": "AKxxx",
    "secret_access_key": "SKxxx"
  }
}
```

### 14.2 新格式（v0.1.0+）

```json
{
  "providers": {
    "minimax": {
      "enabled": true,
      "api_key": "sk-xxx",
      "region": "cn"
    },
    "volcengine": {
      "enabled": true,
      "access_key_id": "AKxxx",
      "secret_access_key": "SKxxx"
    }
  }
}
```

### 14.3 迁移机制

- `migrateOldConfig()` 启动时自动检测旧格式
- 转换为 `providers` 结构并写回
- 原文件备份为 `.bak`
- `loadProviderConfig()` 同时支持新旧格式（fallback）
