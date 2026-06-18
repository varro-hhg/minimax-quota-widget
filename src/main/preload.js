// src/main/preload.js
// contextBridge: expose secure IPC API to renderer

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // Receive from main
  onQuotaUpdate:   (cb) => ipcRenderer.on('quota:update',        (_, data) => cb(data)),
  onVolcUpdate:    (cb) => ipcRenderer.on('volc:update',         (_, data) => cb(data)),
  onRefresh:       (cb) => ipcRenderer.on('quota:refresh',       ()        => cb()),
  onSwitchRegion:  (cb) => ipcRenderer.on('quota:switchRegion',  ()        => cb()),

  // Send to main
  requestRefresh:    () => ipcRenderer.send('quota:requestRefresh'),
  toggleAlwaysOnTop: () => ipcRenderer.send('window:toggleAlwaysOnTop'),
  closeWindow:       () => ipcRenderer.send('window:close'),
  minimizeWindow:    () => ipcRenderer.send('window:minimize'),
  openConfigFile:    () => ipcRenderer.send('config:openFile'),
  openConfigDir:     () => ipcRenderer.send('config:openDir')
});
