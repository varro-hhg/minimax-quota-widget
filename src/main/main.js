// src/main/main.js
// Electron entry point — window, tray, IPC, refresh scheduling

const {
  app, BrowserWindow, Tray, Menu, ipcMain, screen, nativeImage, shell
} = require('electron');
const path = require('node:path');
const {
  ensureConfigFile, loadProviderConfig, getQuota, sortByModelOrder,
  WIDGET_CONFIG_PATH, MMX_CONFIG_PATH
} = require('./quota-service');
const {
  getCodingPlanUsage, normalizeVolcUsage
} = require('./volcengine-service');

// ============================================================
// State
// ============================================================

let mainWindow    = null;
let tray          = null;
let refreshTimer  = null;
let isAlwaysOnTop = true;
app.isQuitting    = false;

// ============================================================
// Window
// ============================================================

function createWindow() {
  mainWindow = new BrowserWindow({
    width:           460,
    height:          500,
    minWidth:        420,
    minHeight:       280,
    frame:           false,
    transparent:     true,
    resizable:       true,
    alwaysOnTop:     isAlwaysOnTop,
    skipTaskbar:     false,
    show:            false,
    backgroundColor: '#00000000',
    webPreferences: {
      preload:          path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration:  false
    }
  });

  mainWindow.loadFile(path.join(__dirname, '../renderer/index.html'));

  // Only call refreshQuota AFTER the renderer has finished loading,
  // otherwise IPC messages are sent before the renderer listener is attached.
  mainWindow.webContents.on('did-finish-load', () => {
    console.log('[main] renderer did-finish-load → calling refreshQuota');
    refreshQuota();
    // Send initial pin state to renderer
    mainWindow.webContents.send('window:pinStateChanged', isAlwaysOnTop);
  });

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
    placeWindowTopRight();
  });

  // Intercept close → hide to tray
  mainWindow.on('close', (e) => {
    if (!app.isQuitting) {
      e.preventDefault();
      mainWindow.hide();
    }
  });
}

function placeWindowTopRight() {
  if (!mainWindow) return;
  try {
    const { workArea } = screen.getPrimaryDisplay();
    const { width, height } = mainWindow.getBounds();
    mainWindow.setBounds({
      x: workArea.x + workArea.width  - width  - 24,
      y: workArea.y + 24,
      width,
      height
    });
  } catch (_) { /* multi-monitor edge case */ }
}

// ============================================================
// Tray
// ============================================================

function createTray() {
  // Use icon.ico if exists, otherwise create a tiny placeholder
  const icoPath = path.join(__dirname, '../assets/icon.ico');
  let icon;
  try {
    icon = nativeImage.createFromPath(icoPath);
    if (icon.isEmpty()) throw new Error('empty');
  } catch (_) {
    // 16x16 transparent placeholder
    icon = nativeImage.createEmpty();
  }
  tray = new Tray(icon);
  tray.setToolTip('Coding Plan 配额');
  rebuildTrayMenu();

  tray.on('click', toggleWindow);
}

function rebuildTrayMenu() {
  if (!tray) return;
  tray.setContextMenu(Menu.buildFromTemplate([
    { label: '显示 / 隐藏', click: toggleWindow },
    {
      label:    '刷新额度',
      click:    () => mainWindow?.webContents.send('quota:refresh') || refreshQuota()
    },
    {
      label: isAlwaysOnTop ? '取消置顶' : '置顶',
      click: () => setAlwaysOnTop(!isAlwaysOnTop)
    },
    { type: 'separator' },
    {
      label: '📂 打开配置文件',
      click: () => {
        const fs = require('node:fs');
        const cfgPath = fs.existsSync(MMX_CONFIG_PATH())
          ? MMX_CONFIG_PATH()
          : WIDGET_CONFIG_PATH();
        if (fs.existsSync(cfgPath)) {
          shell.openPath(cfgPath);
        } else {
          shell.openPath(path.dirname(cfgPath));
        }
      }
    },
    { type: 'separator' },
    {
      label: '退出',
      click: () => { app.isQuitting = true; app.quit(); }
    }
  ]));
}

function toggleWindow() {
  if (!mainWindow) return;
  if (mainWindow.isVisible()) {
    mainWindow.hide();
  } else {
    mainWindow.show();
    mainWindow.focus();
  }
}

function setAlwaysOnTop(value) {
  isAlwaysOnTop = value;
  if (mainWindow) {
    mainWindow.setAlwaysOnTop(isAlwaysOnTop);
    mainWindow.webContents.send('window:pinStateChanged', isAlwaysOnTop);
  }
  rebuildTrayMenu();
}

// ============================================================
// Refresh scheduling
// ============================================================

function scheduleNextRefresh(resetsInMs) {
  if (refreshTimer) clearTimeout(refreshTimer);
  // If 5h window resets within 2 min → poll in 5 s; otherwise every 5 min
  const interval = (resetsInMs && resetsInMs < 120_000)
    ? 5_000
    : 5 * 60_000;
  refreshTimer = setTimeout(() => refreshQuota(), interval);
}

async function refreshQuota() {
  // Ensure config exists (and migrate old format if needed)
  ensureConfigFile();

  const minimaxCfg = loadProviderConfig('minimax');
  const volcCfg    = loadProviderConfig('volcengine');

  if (!minimaxCfg && !volcCfg) {
    // No providers configured at all
    const cfgResult = ensureConfigFile();
    mainWindow?.webContents.send('quota:update', {
      ok:         false,
      error:      'NO_CONFIG',
      configPath: cfgResult.configPath,
      created:    cfgResult.created
    });
    mainWindow?.webContents.send('volc:update', {
      ok: false, error: 'NO_VOLC_CONFIG'
    });
    scheduleNextRefresh(null);
    return;
  }

  // Fire MiniMax and Volcengine requests in parallel
  const minimaxPromise = minimaxCfg
    ? getQuota(minimaxCfg).then(models => sortByModelOrder(models))
    : Promise.reject(new Error('MiniMax not configured'));
  const volcPromise = refreshVolc();

  const [mmResult, volcResult] = await Promise.allSettled([minimaxPromise, volcPromise]);

  if (mmResult.status === 'fulfilled') {
    mainWindow?.webContents.send('quota:update', { ok: true, data: mmResult.value });
    const general = mmResult.value.find(m => m.modelName === 'general');
    scheduleNextRefresh(general?.intervalResetsInMs);
  } else {
    mainWindow?.webContents.send('quota:update', {
      ok: false, error: mmResult.reason?.message || String(mmResult.reason)
    });
    scheduleNextRefresh(null);
  }
}

/**
 * Fetch and send Volcengine Coding Plan usage to renderer.
 * Returns a promise that resolves when done (used by Promise.allSettled).
 */
async function refreshVolc() {
  const volcCfg = loadProviderConfig('volcengine');
  if (!volcCfg) {
    mainWindow?.webContents.send('volc:update', {
      ok: false, error: 'NO_VOLC_CONFIG'
    });
    return;
  }
  try {
    const raw = await getCodingPlanUsage(volcCfg);
    const data = normalizeVolcUsage(raw);
    if (!data) throw new Error('Invalid volc response');
    mainWindow?.webContents.send('volc:update', { ok: true, data });
  } catch (err) {
    console.error('[volc] refresh error:', err.message);
    mainWindow?.webContents.send('volc:update', {
      ok: false, error: err.message
    });
  }
}

// ============================================================
// IPC handlers (renderer → main)
// ============================================================

function registerIPC() {
  ipcMain.on('quota:requestRefresh', () => {
    refreshQuota();
  });

  ipcMain.on('window:toggleAlwaysOnTop', () => {
    setAlwaysOnTop(!isAlwaysOnTop);
  });

  ipcMain.on('window:close', () => {
    if (mainWindow) mainWindow.hide();
  });

  ipcMain.on('window:minimize', () => {
    if (mainWindow) mainWindow.minimize();
  });

  ipcMain.on('config:openFile', () => {
    const fs = require('node:fs');
    const cfgPath = fs.existsSync(MMX_CONFIG_PATH())
      ? MMX_CONFIG_PATH()
      : WIDGET_CONFIG_PATH();
    if (fs.existsSync(cfgPath)) {
      shell.openPath(cfgPath);
    } else {
      shell.openPath(path.dirname(cfgPath));
    }
  });

  ipcMain.on('config:openDir', () => {
    const dir = path.dirname(WIDGET_CONFIG_PATH());
    shell.openPath(dir);
  });
}

// ============================================================
// App lifecycle
// ============================================================

app.whenReady().then(() => {
  registerIPC();
  createWindow();
  createTray();
  // NOTE: refreshQuota() is now called from mainWindow.webContents 'did-finish-load'
  //       to avoid race condition (renderer listeners not yet attached).
});

app.on('before-quit', () => { app.isQuitting = true; });

app.on('window-all-closed', (e) => {
  // Tray app: don't quit when window is hidden
  // e.preventDefault() keeps the app alive
});

app.on('activate', () => {
  // macOS dock click
  if (mainWindow === null) createWindow();
  else mainWindow.show();
});
