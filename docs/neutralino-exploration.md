# Neutralino.js 替代 Electron 可行性探索

> 分支: `explore/neutralino`
> 日期: 2026-06-22
> 目的: 评估用 Neutralino.js 替代 Electron 实现 Coding Plan 配额小部件的可行性

## 一、Neutralino.js 概述

Neutralino.js 是轻量级跨平台桌面应用框架，与 Electron 的核心区别：

| 维度 | Electron | Neutralino.js |
|------|----------|---------------|
| 渲染引擎 | 内嵌 Chromium | 系统自带 WebView（Windows = WebView2） |
| 后端 | Node.js 进程 | C++ 轻量服务器 |
| 通信方式 | IPC（进程间） | WebSocket（本地） |
| 打包体积 | ~68MB（我们的 app） | ~5-10MB |
| 内存占用 | ~60-80MB | ~8-15MB |
| 语言 | JavaScript | JavaScript（前端）+ C++（框架） |
| 系统托盘 | Tray API | os.setTray() |
| 无框窗口 | frame: false | borderless: true |
| 透明窗口 | transparent: true | transparent: true |
| 置顶 | alwaysOnTop | setAlwaysOnTop() |

## 二、功能 API 映射

### 窗口控制

```js
// Electron
const win = new BrowserWindow({ frame: false, transparent: true, alwaysOnTop: true });
win.hide();
win.show();
win.setAlwaysOnTop(false);

// Neutralino.js
// 在 neutralino.config.json 中配置:
// "modes": { "window": { "borderless": true, "transparent": true, "alwaysOnTop": true } }
await Neutralino.window.hide();
await Neutralino.window.show();
await Neutralino.window.setAlwaysOnTop(false);
```

### 系统托盘

```js
// Electron
const tray = new Tray(icon);
tray.setContextMenu(Menu.buildFromTemplate([...items]));

// Neutralino.js
await Neutralino.os.setTray({
  icon: '/resources/icons/tray.png',
  menuItems: [
    { id: 'refresh', text: '刷新额度' },
    { id: 'pin', text: '置顶' },
    { text: '-' },
    { id: 'quit', text: '退出' }
  ]
});
// 监听点击
Neutralino.events.on('trayMenuItemClicked', (evt) => {
  if (evt.detail.id === 'refresh') refreshQuota();
  if (evt.detail.id === 'quit') Neutralino.app.exit();
});
```

### 自定义标题栏拖拽

```js
// Electron: -webkit-app-region: drag (CSS)

// Neutralino.js: 使用 setDraggableRegion
await Neutralino.window.setDraggableRegion('titlebar', {
  exclusions: ['btn-settings', 'btn-pin', 'btn-min', 'btn-close']
});
```

### HTTP 请求

```js
// Electron: Node.js fetch（内置）
const res = await fetch(url, { headers });

// Neutralino.js: 浏览器 fetch（WebView 内置）
const res = await fetch(url, { headers });
// 注意：火山方舟 V4 签名需要 HMAC-SHA256
// WebView 内置 crypto.subtle.digest 可用，但签名实现需要重写
```

### 文件系统

```js
// Electron: Node.js fs 模块
const fs = require('node:fs');
fs.readFileSync(configPath, 'utf8');

// Neutralino.js: Neutralino.filesystem API
const data = await Neutralino.filesystem.readFile(configPath);
// 注意：异步 API，无同步版本
```

### 配置路径

```js
// Electron: os.homedir() + path.join
const configPath = path.join(os.homedir(), '.minimax-quota-widget', 'config.json');

// Neutralino.js: os.getPath('home')
const home = await Neutralino.os.getPath('home');
const configPath = home + '/.minimax-quota-widget/config.json';
```

### 定时刷新

```js
// 两者相同：setInterval / setTimeout 均可
setInterval(() => refreshQuota(), 5 * 60_000);
```

## 三、neutralino.config.json 示例

```json
{
  "applicationId": "com.codingplan.quota.widget",
  "version": "0.1.0",
  "defaultMode": "window",
  "url": "/",
  "enableServer": true,
  "enableNativeAPI": true,
  "documentRoot": "/resources/",
  "nativeAllowList": [
    "app.*", "window.*", "os.*", "filesystem.*", "storage.*", "events.*"
  ],
  "modes": {
    "window": {
      "title": "Coding Plan 配额",
      "width": 460,
      "height": 340,
      "minWidth": 460,
      "minHeight": 340,
      "center": false,
      "borderless": true,
      "transparent": true,
      "alwaysOnTop": true,
      "resizable": true,
      "icon": "/resources/icons/icon.png"
    }
  },
  "cli": {
    "binaryName": "CodingPlan-Quota-Widget",
    "resourcesPath": "/resources/",
    "clientLibrary": "/resources/js/neutralino.js",
    "binaryVersion": "5.5.0",
    "frontendLibrary": {
      "patchFile": "/resources/index.html",
      "devServerUrl": "http://localhost:5173"
    }
  }
}
```

## 四、关键挑战

### 1. 火山方舟 V4 签名（最大挑战）

当前 `volcengine-service.js` 用 `node:crypto` 实现 HMAC-SHA256 签名。Neutralino.js 无 Node.js 运行时，需要：

- **方案 A**: 用 Web Crypto API（`crypto.subtle`）重写签名逻辑
  - 支持：`HMAC-SHA256` 签名 ✅
  - 限制：全异步，无 `createHmac` 链式调用
  - 工作量：中等（重写 ~50 行签名代码）

- **方案 B**: 引入纯 JS crypto 库（如 asmCrypto.js）
  - 优点：API 更接近 node:crypto
  - 缺点：额外依赖

### 2. 文件系统异步化

所有文件操作从同步（`fs.readFileSync`）变为异步（`await Neutralino.filesystem.readFile`）。需要重构 `quota-service.js` 的配置加载逻辑。

### 3. 透明窗口 + 边框

Neutralino.js 在 Windows 上开启 `transparent` 模式时自动变为 `borderless`（无窗口控制按钮）。这与我们的需求一致，但需要确保 WebView2 的 `backdrop-filter` 兼容性。

### 4. 无 Node.js 模块

以下 Node.js 内置模块在 Neutralino.js 中不可用：
- `node:crypto` → Web Crypto API
- `node:fs` → Neutralino.filesystem
- `node:path` → 手动字符串拼接
- `node:os` → Neutralino.os
- `node:http` → fetch（已有）

### 5. 打包产物

Neutralino.js 打包为：
- `framework-bin` + `resources/` 文件夹（~5MB）
- 可用 `neu build` 命令打包
- Windows 上无需额外签名

## 五、迁移工作量评估

| 模块 | 改动量 | 说明 |
|------|--------|------|
| `index.html` + `style.css` | **低** | 基本不变，标题栏拖拽改为 `setDraggableRegion` |
| `quota-service.js` | **中** | 文件读写异步化，去掉 `node:fs/path/os` |
| `volcengine-service.js` | **中高** | V4 签名改用 Web Crypto API 重写 |
| `main.js` | **高** | 全部重写为 Neutralino.js 启动脚本 |
| `preload.js` | **删除** | 不需要，Neutralino.js 直接暴露 `Neutralino` 全局 |
| `package.json` | **替换** | 改为 `neutralino.config.json` |
| 打包配置 | **低** | `neu build` 替代 `electron-builder` |

**总工时估计**: 2-3 天（含调试和测试）

## 六、收益与风险

### 收益
- 打包体积：68MB → **~5-8MB**（减少 90%）
- 内存占用：60-80MB → **~10-15MB**（减少 80%）
- 启动速度：更快（无 Chromium 加载）
- 跨平台：Windows/macOS/Linux 同一份代码

### 风险
- WebView2 兼容性：Windows 10 旧版可能缺少 WebView2
- 透明窗口效果：`backdrop-filter` 在 WebView2 中的表现需实测
- 社区规模：Neutralino.js 社区远小于 Electron
- 调试工具：不如 Electron DevTools 成熟

## 七、建议

**推荐渐进式迁移**：

1. 先新建 `neutralino/` 目录做 PoC（概念验证）
2. 优先验证：透明窗口 + backdrop-filter + 托盘菜单
3. V4 签名重写为 Web Crypto API 版本（可复用到 Electron 版）
4. 如果 PoC 成功，再全面迁移

**如果 WebView2 透明效果不理想**，可考虑 Tauri（Rust + WebView2，社区更活跃，打包也很小）。

## 八、参考资料

- [Neutralino.js 官方文档](https://neutralino.js.org/docs/)
- [Neutralino.js vs Electron 对比](https://github.com/neutralinojs/evaluation)
- [Window API](https://neutralino.js.org/docs/api/window)
- [OS API（托盘/通知）](https://neutralino.js.org/docs/api/os)
- [配置文件参考](https://neutralino.js.org/docs/configuration/neutralino.config.json)
