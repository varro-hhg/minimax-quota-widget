// src/main/volcengine-service.js
// Volcengine Ark Coding Plan quota API client — zero-dep V4 signing

const crypto = require('node:crypto');

// ============================================================
// Constants
// ============================================================

const SERVICE  = 'ark';
const REGION   = 'cn-beijing';
const HOST     = 'open.volcengineapi.com';
const ENDPOINT = `https://${HOST}`;
const ALGORITHM = 'HMAC-SHA256';

// ============================================================
// V4 Signing helpers  (Volcengine signature v4, same as AWS SigV4)
// ============================================================

function hmac(key, data) {
  return crypto.createHmac('sha256', key).update(data, 'utf8').digest();
}

function sha256Hex(data) {
  return crypto.createHash('sha256').update(data || '', 'utf8').digest('hex');
}

function uriEncode(str) {
  return encodeURIComponent(str).replace(/[!'()*]/g,
    c => '%' + c.charCodeAt(0).toString(16).toUpperCase().padStart(2, '0'));
}

/**
 * Format Date to Volcengine X-Date format: YYYYMMDD'T'HHMMSS'Z'
 */
function toXDate(date) {
  return date.toISOString().replace(/[-:]/g, '').replace(/\.\d{3}/, '');
}

/**
 * Format Date to date stamp: YYYYMMDD
 */
function toDateStamp(date) {
  return toXDate(date).slice(0, 8);
}

/**
 * Sign a Volcengine OpenAPI request using V4 signature.
 * Mutates `headers` in-place to add Authorization, X-Date, X-Content-Sha256.
 */
function signRequest({ method, path, queryString, headers, body, accessKeyId, secretAccessKey }) {
  const now       = new Date();
  const xDate     = toXDate(now);
  const dateStamp = toDateStamp(now);
  const bodyHash  = sha256Hex(body || '');

  headers['X-Date']           = xDate;
  headers['X-Content-Sha256'] = bodyHash;
  headers['Host']             = HOST;

  // Canonical headers (lowercase, sorted)
  const signedHeaderKeys = Object.keys(headers)
    .map(k => k.toLowerCase())
    .sort();
  const canonicalHeaders = signedHeaderKeys
    .map(k => `${k}:${headers[Object.keys(headers).find(h => h.toLowerCase() === k)].trim()}\n`)
    .join('');
  const signedHeaders = signedHeaderKeys.join(';');

  // Canonical query string (sorted by key)
  const canonicalQueryString = buildCanonicalQuery(queryString);

  // Canonical URI
  const canonicalURI = path || '/';

  // 1. Canonical Request
  const canonicalRequest = [
    method.toUpperCase(),
    canonicalURI,
    canonicalQueryString,
    canonicalHeaders,
    signedHeaders,
    bodyHash
  ].join('\n');

  // 2. String to Sign
  const credentialScope = `${dateStamp}/${REGION}/${SERVICE}/request`;
  const stringToSign = [
    ALGORITHM,
    xDate,
    credentialScope,
    sha256Hex(canonicalRequest)
  ].join('\n');

  // 3. Signing key
  const kDate    = hmac(secretAccessKey, dateStamp);
  const kRegion  = hmac(kDate, REGION);
  const kService = hmac(kRegion, SERVICE);
  const kSigning = hmac(kService, 'request');

  // 4. Signature
  const signature = crypto.createHmac('sha256', kSigning)
    .update(stringToSign, 'utf8')
    .digest('hex');

  // 5. Authorization header
  headers['Authorization'] =
    `${ALGORITHM} Credential=${accessKeyId}/${credentialScope}, ` +
    `SignedHeaders=${signedHeaders}, Signature=${signature}`;

  return headers;
}

function buildCanonicalQuery(params) {
  if (!params || Object.keys(params).length === 0) return '';
  return Object.keys(params)
    .sort()
    .map(k => `${uriEncode(k)}=${uriEncode(params[k])}`)
    .join('&');
}

// ============================================================
// API: GetCodingPlanUsage
// ============================================================

/**
 * Fetch Coding Plan usage from Volcengine Ark API.
 * @param {{ accessKeyId: string, secretAccessKey: string }} credentials
 * @param {{ timeoutMs?: number }} opts
 * @returns {Promise<Object>} — raw Result object { Status, QuotaUsage, UpdateTimestamp }
 */
async function getCodingPlanUsage({ accessKeyId, secretAccessKey }, { timeoutMs = 10_000 } = {}) {
  const action  = 'GetCodingPlanUsage';
  const version = '2024-01-01';

  const queryParams = { Action: action, Version: version };
  const queryString = `Action=${uriEncode(action)}&Version=${uriEncode(version)}`;
  const url         = `${ENDPOINT}/?${queryString}`;

  const body    = '{}';
  const headers = {
    'Content-Type': 'application/json'
  };

  signRequest({
    method: 'POST',
    path: '/',
    queryString: queryParams,
    headers,
    body,
    accessKeyId,
    secretAccessKey
  });

  console.log('[volc] requesting:', url);

  const res = await fetch(url, {
    method:  'POST',
    headers,
    body,
    signal:  AbortSignal.timeout(timeoutMs)
  });

  const text = await res.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch (_) {
    throw new Error(`HTTP ${res.status}: ${text.slice(0, 300)}`);
  }

  // Volcengine error handling
  if (json.ResponseMetadata?.Error) {
    const err = json.ResponseMetadata.Error;
    throw new Error(`${err.Code || 'VolcError'}: ${err.Message || JSON.stringify(err)}`);
  }

  if (!res.ok) {
    throw new Error(`HTTP ${res.status}: ${text.slice(0, 300)}`);
  }

  return json.Result || null;
}

// ============================================================
// Normalization
// ============================================================

const LEVEL_LABELS = {
  session: '会话',
  weekly:  '本周',
  monthly: '本月'
};

const LEVEL_ORDER = ['session', 'weekly', 'monthly'];

/**
 * Normalize the raw Result into a renderer-friendly shape.
 */
function normalizeVolcUsage(result) {
  if (!result || !Array.isArray(result.QuotaUsage)) {
    return null;
  }

  const quotas = result.QuotaUsage
    .map(q => {
      const usedPct    = Math.round(q.Percent * 100) / 100;  // keep 2 decimals
      const remainPct  = Math.max(0, Math.round((100 - usedPct) * 100) / 100);
      const resetsAt   = q.ResetTimestamp ? q.ResetTimestamp * 1000 : null; // ms
      const resetsInMs = resetsAt ? Math.max(0, resetsAt - Date.now()) : null;
      return {
        level:         (q.Level || 'unknown').toLowerCase(),
        label:         LEVEL_LABELS[(q.Level || '').toLowerCase()] || q.Level,
        usedPercent:   usedPct,
        remainPercent: remainPct,
        resetsAt,
        resetsInMs
      };
    })
    .sort((a, b) => {
      const ia = LEVEL_ORDER.indexOf(a.level);
      const ib = LEVEL_ORDER.indexOf(b.level);
      return (ia === -1 ? 99 : ia) - (ib === -1 ? 99 : ib);
    });

  return {
    status:    result.Status || 'Unknown',
    quotas,
    updatedAt: result.UpdateTimestamp
      ? new Date(result.UpdateTimestamp * 1000).toISOString()
      : new Date().toISOString()
  };
}

// ============================================================
// Exports
// ============================================================

module.exports = {
  getCodingPlanUsage,
  normalizeVolcUsage
};
