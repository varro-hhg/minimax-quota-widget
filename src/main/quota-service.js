// src/main/quota-service.js
// MiniMax Token Plan quota HTTP API client

const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');

// ============================================================
// Config paths
// ============================================================

const MMX_CONFIG_PATH   = path.join(os.homedir(), '.mmx', 'config.json');
const WIDGET_CONFIG_DIR = path.join(os.homedir(), '.minimax-quota-widget');
const WIDGET_CONFIG_PATH= path.join(WIDGET_CONFIG_DIR, 'config.json');

// ============================================================
// Placeholder constants
// ============================================================

const MINIMAX_PLACEHOLDER_KEY = 'sk-your-minimax-api-key-here';
const VOLC_PLACEHOLDER_AK = 'your-volcengine-access-key-id';
const VOLC_PLACEHOLDER_SK = 'your-volcengine-secret-access-key';

// ============================================================
// Example config (new providers format)
// ============================================================

const EXAMPLE_CONFIG = JSON.stringify({
  providers: {
    minimax: {
      enabled: true,
      api_key: MINIMAX_PLACEHOLDER_KEY,
      region:  'cn'
    },
    volcengine: {
      enabled: true,
      access_key_id:     VOLC_PLACEHOLDER_AK,
      secret_access_key: VOLC_PLACEHOLDER_SK
    }
  }
}, null, 2);

// ============================================================
// Internal helpers
// ============================================================

/**
 * Find the active config file path (mmx > widget).
 * Returns null if no config exists.
 */
function findConfigPath() {
  if (fs.existsSync(MMX_CONFIG_PATH)) return MMX_CONFIG_PATH;
  if (fs.existsSync(WIDGET_CONFIG_PATH)) return WIDGET_CONFIG_PATH;
  return null;
}

/**
 * Read and parse the config file. Returns raw object or null.
 */
function readConfig() {
  const cfgPath = findConfigPath();
  if (!cfgPath) return null;
  try {
    return JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
  } catch (_) {
    return null;
  }
}

// ============================================================
// Migration: old format → new providers format
// ============================================================

/**
 * Detect if config uses old format (root-level api_key / volcengine)
 * and migrate to new providers format. Backs up old file as .bak.
 * Returns true if migration was performed.
 */
function migrateOldConfig() {
  const cfgPath = findConfigPath();
  if (!cfgPath) return false;

  let cfg;
  try { cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8')); } catch (_) { return false; }

  // Already new format?
  if (cfg.providers && typeof cfg.providers === 'object') return false;

  // Check if old format has anything useful
  const hasOldMinimax  = cfg.api_key || cfg.apiKey;
  const hasOldVolc     = cfg.volcengine || cfg.volc;
  if (!hasOldMinimax && !hasOldVolc) return false;

  console.log('[config] Detected old format, migrating to providers format...');

  const providers = {};

  // Migrate MiniMax
  if (hasOldMinimax) {
    const key = cfg.apiKey || cfg.api_key;
    providers.minimax = {
      enabled: true,
      api_key: key,
      region: cfg.region || 'cn'
    };
  }

  // Migrate Volcengine
  if (hasOldVolc) {
    const vc = cfg.volcengine || cfg.volc;
    const ak = vc.access_key_id || vc.accessKeyId;
    const sk = vc.secret_access_key || vc.secretAccessKey;
    if (ak && sk) {
      providers.volcengine = {
        enabled: true,
        access_key_id: ak,
        secret_access_key: sk
      };
    }
  }

  const newConfig = { providers };

  // Backup
  try {
    fs.copyFileSync(cfgPath, cfgPath + '.bak');
    console.log('[config] Backup saved to', cfgPath + '.bak');
  } catch (_) { /* best effort */ }

  // Write migrated
  try {
    fs.writeFileSync(cfgPath, JSON.stringify(newConfig, null, 2), 'utf8');
    console.log('[config] Migration complete');
    return true;
  } catch (err) {
    console.error('[config] Migration write failed:', err.message);
    return false;
  }
}

// ============================================================
// ensureConfigFile
// ============================================================

/**
 * Ensure a config file exists.
 * If neither ~/.mmx/config.json nor ~/.minimax-quota-widget/config.json exists,
 * create the widget config with example content.
 * After ensuring, run migration if old format detected.
 *
 * Returns: { configPath: string, created: boolean }
 */
function ensureConfigFile() {
  // mmx-cli config already present
  if (fs.existsSync(MMX_CONFIG_PATH)) {
    migrateOldConfig();
    return { configPath: MMX_CONFIG_PATH, created: false };
  }

  // Widget config already present
  if (fs.existsSync(WIDGET_CONFIG_PATH)) {
    migrateOldConfig();
    return { configPath: WIDGET_CONFIG_PATH, created: false };
  }

  // Neither exists → create example config
  try {
    if (!fs.existsSync(WIDGET_CONFIG_DIR)) {
      fs.mkdirSync(WIDGET_CONFIG_DIR, { recursive: true });
    }
    fs.writeFileSync(WIDGET_CONFIG_PATH, EXAMPLE_CONFIG, 'utf8');
    return { configPath: WIDGET_CONFIG_PATH, created: true };
  } catch (err) {
    console.error('[config] failed to create example config:', err.message);
    return { configPath: WIDGET_CONFIG_PATH, created: false };
  }
}

// ============================================================
// loadProviderConfig — universal provider config loader
// ============================================================

/**
 * Load config for a specific provider.
 *
 * @param {'minimax' | 'volcengine'} name - Provider name
 * @returns {Object|null} Provider-specific config, or null if not configured.
 *
 * For 'minimax' returns:  { apiKey, region, baseUrl }
 * For 'volcengine' returns: { accessKeyId, secretAccessKey }
 */
function loadProviderConfig(name) {
  const cfg = readConfig();
  if (!cfg) return null;

  const providers = cfg.providers || {};
  const prov = providers[name];

  // New format: providers.<name>
  if (prov) {
    // Check enabled flag (default true)
    if (prov.enabled === false) return null;

    if (name === 'minimax') {
      const key = prov.apiKey || prov.api_key;
      if (!key || key === MINIMAX_PLACEHOLDER_KEY) return null;
      const region = prov.region || 'cn';
      return {
        apiKey: key,
        region,
        baseUrl: region === 'global'
          ? 'https://api.minimax.io'
          : 'https://api.minimaxi.com'
      };
    }

    if (name === 'volcengine') {
      const ak = prov.access_key_id || prov.accessKeyId;
      const sk = prov.secret_access_key || prov.secretAccessKey;
      if (!ak || !sk || ak === VOLC_PLACEHOLDER_AK || sk === VOLC_PLACEHOLDER_SK) return null;
      return { accessKeyId: ak, secretAccessKey: sk };
    }

    // Generic provider: return as-is
    return prov;
  }

  // Fallback: old format (backward compat for configs that weren't migrated)
  if (name === 'minimax') {
    const key = cfg.apiKey || cfg.api_key;
    if (key && key !== MINIMAX_PLACEHOLDER_KEY) {
      const region = cfg.region || 'cn';
      return {
        apiKey: key,
        region,
        baseUrl: region === 'global'
          ? 'https://api.minimax.io'
          : 'https://api.minimaxi.com'
      };
    }
  }

  if (name === 'volcengine') {
    const vc = cfg.volcengine || cfg.volc;
    if (vc) {
      const ak = vc.access_key_id || vc.accessKeyId;
      const sk = vc.secret_access_key || vc.secretAccessKey;
      if (ak && sk && ak !== VOLC_PLACEHOLDER_AK && sk !== VOLC_PLACEHOLDER_SK) {
        return { accessKeyId: ak, secretAccessKey: sk };
      }
    }
  }

  return null;
}

// ============================================================
// Quota API
// ============================================================

async function getQuota({ baseUrl, apiKey }, { timeoutMs = 10_000 } = {}) {
  const url = `${baseUrl}/v1/token_plan/remains`;
  const headers = {
    'Authorization': `Bearer ${apiKey}`,
    'x-api-key': apiKey,
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

// ============================================================
// Normalization
// ============================================================

function normalizeModel(m) {
  return {
    modelName:              m.model_name                        || 'unknown',
    intervalRemainPercent:  clampPercent(m.current_interval_remaining_percent ?? 100),
    weeklyRemainPercent:    clampPercent(m.current_weekly_remaining_percent   ?? 100),
    intervalUsed:           m.current_interval_usage_count      ?? 0,
    intervalTotal:          m.current_interval_total_count      ?? 0,
    weeklyUsed:             m.current_weekly_usage_count        ?? 0,
    weeklyTotal:            m.current_weekly_total_count        ?? 0,
    intervalResetsInMs:     m.remains_time                      ?? null,
    intervalBoostPermille:  m.interval_boost_permille           ?? 0,
    weeklyBoostPermille:    m.weekly_boost_permille             ?? 0,
    intervalStatus:         m.current_interval_status           ?? null,
    weeklyStatus:           m.current_weekly_status             ?? null,
    weeklyStart:            m.weekly_start_time                 ?? null,
    weeklyEnd:              m.weekly_end_time                   ?? null,
    fetchedAt:              new Date().toISOString()
  };
}

function clampPercent(v) {
  if (!Number.isFinite(v)) return 0;
  return Math.max(0, Math.min(100, Math.round(v)));
}

// ============================================================
// Display helpers
// ============================================================

/**
 * Color threshold:
 *   green  >= 50%
 *   yellow 10% – 49%
 *   red    < 10% or 0%
 */
function colorOf(percent) {
  if (percent <= 0)  return 'red';
  if (percent < 10)  return 'red';
  if (percent < 50)  return 'yellow';
  return 'green';
}

/** Format milliseconds into human-readable "Xh Ym" or "Ym" */
function formatDuration(ms) {
  if (ms == null || ms <= 0) return '—';
  const h = Math.floor(ms / 3_600_000);
  const m = Math.floor((ms % 3_600_000) / 60_000);
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
}

/** Format boost permille (1000 = base, 2000 = +100%) into "+X%" label */
function formatBoost(permille) {
  if (!permille || permille <= 1000) return null;
  const pct = Math.round((permille - 1000) / 10);
  return `+${pct}%`;
}

/** Format Unix-ms timestamp to "MM-DD" */
function formatDateShort(ts) {
  if (!ts) return '—';
  const d = new Date(ts);
  return `${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

// ============================================================
// Model sort order (fixed, not API order)
// ============================================================

const MODEL_ORDER = ['general', 'video', 'speech', 'image'];

function sortByModelOrder(models) {
  return [...models].sort((a, b) => {
    const ia = MODEL_ORDER.indexOf(a.modelName);
    const ib = MODEL_ORDER.indexOf(b.modelName);
    return (ia === -1 ? 99 : ia) - (ib === -1 ? 99 : ib);
  });
}

// ============================================================
// Exports
// ============================================================

module.exports = {
  ensureConfigFile,
  loadProviderConfig,
  migrateOldConfig,
  getQuota,
  colorOf,
  formatDuration,
  formatBoost,
  formatDateShort,
  sortByModelOrder,
  WIDGET_CONFIG_PATH: () => WIDGET_CONFIG_PATH,
  MMX_CONFIG_PATH:   () => MMX_CONFIG_PATH
};
