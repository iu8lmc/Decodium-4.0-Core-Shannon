// -*- Mode: C++ -*-
#include "RemoteCommandServer.hpp"

#include <QDateTime>
#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPainter>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtMath>
#include <QUuid>

#include <algorithm>
#include <limits>

#include "PrecisionTime.hpp"

namespace
{
constexpr qint64 kMaxHttpHeaderBytes = 8 * 1024;
constexpr qint64 kMaxHttpBodyBytes = 64 * 1024;
constexpr int kHttpConnectionTimeoutMs = 10000;

QString normalize_call(QString call)
{
  call = call.trimmed().toUpper();
  call.remove('<');
  call.remove('>');
  return call;
}

QString normalize_grid(QString grid)
{
  return grid.trimmed().toUpper();
}

QByteArray dashboard_html()
{
  return QByteArrayLiteral(
R"FT2HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no" />
  <meta name="theme-color" content="#0b1220" />
  <meta name="mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
  <meta name="apple-mobile-web-app-title" content="Decodium Remote" />
  <title>Decodium Remote Console</title>
  <link rel="manifest" href="/manifest.webmanifest" />
  <link rel="icon" type="image/png" sizes="192x192" href="/icon-192.png" />
  <link rel="apple-touch-icon" sizes="192x192" href="/icon-192.png" />
  <link rel="stylesheet" href="/app.css" />
</head>
<body>
  <div id="login_overlay" class="login-overlay hidden">
    <div class="login-card">
      <h2>Remote Access</h2>
      <p id="login_caption">Insert username and password to unlock dashboard</p>
      <form id="login_form" autocomplete="on">
        <label for="login_user">Username</label>
        <input id="login_user" name="username" type="text" autocomplete="username" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="admin" />
        <label for="login_token">Password</label>
        <input id="login_token" name="token" type="password" autocomplete="current-password" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="Access password" />
        <div id="login_token_hint" class="login-token-hint"></div>
        <button id="btn_login" type="submit" class="primary">Unlock</button>
      </form>
      <div id="login_error"></div>
    </div>
  </div>

  <div class="app">
    <header class="top">
      <div>
        <h1>Decodium Remote Console</h1>
        <div class="sub">Fork by 9H1SR Salvatore Raccampo</div>
      </div>
      <div class="top-actions">
        <button id="btn_install" class="install-btn hidden" type="button">Install App</button>
        <div id="trx_live" class="badge trx rx">RX</div>
        <div id="trx_peer" class="trx-peer">DX: -</div>
        <div id="conn" class="badge off">disconnected</div>
      </div>
    </header>
    <div id="install_hint" class="install-hint hidden"></div>

    <section class="panel main-controls">
      <div class="freq-row">
        <div class="dial-box">
          <div class="lbl">Dial Frequency</div>
          <div id="st_dial" class="dial">-</div>
        </div>
        <div class="kv-grid">
          <div class="kv"><span>Mode</span><strong id="st_mode">-</strong></div>
          <div class="kv"><span>Band</span><strong id="st_band">-</strong></div>
          <div class="kv"><span>Rx</span><strong id="st_rx">-</strong></div>
          <div class="kv"><span>Tx</span><strong id="st_tx">-</strong></div>
          <div class="kv"><span>My Call</span><strong id="st_mycall">-</strong></div>
          <div class="kv"><span>DX Call</span><strong id="st_dxcall">-</strong></div>
        <div class="kv"><span>TX Enabled</span><strong id="st_txen">-</strong></div>
        <div class="kv"><span>Transmitting</span><strong id="st_trx">-</strong></div>
        <div class="kv"><span>Monitoring</span><strong id="st_mon">-</strong></div>
        </div>
      </div>

      <div class="mode-row">
        <div class="group-title">Mode</div>
        <div class="btn-grid" id="mode_buttons">
          <button class="mode-btn" data-mode="FT2">FT2</button>
          <button class="mode-btn" data-mode="FT8">FT8</button>
          <button class="mode-btn" data-mode="FT4">FT4</button>
          <button class="mode-btn" data-mode="MSK144">MSK</button>
          <button class="mode-btn" data-mode="Q65">Q65</button>
          <button class="mode-btn" data-mode="JT65">JT65</button>
          <button class="mode-btn" data-mode="JT9">JT9</button>
          <button class="mode-btn" data-mode="FST4">FST4</button>
          <button class="mode-btn" data-mode="WSPR">WSPR</button>
        </div>
      </div>

      <div class="band-row">
        <div class="group-title">Band</div>
        <div class="btn-grid" id="band_buttons">
          <button class="band-btn" data-band="160m">160</button>
          <button class="band-btn" data-band="80m">80</button>
          <button class="band-btn" data-band="60m">60</button>
          <button class="band-btn" data-band="40m">40</button>
          <button class="band-btn" data-band="30m">30</button>
          <button class="band-btn" data-band="20m">20</button>
          <button class="band-btn" data-band="17m">17</button>
          <button class="band-btn" data-band="15m">15</button>
          <button class="band-btn" data-band="12m">12</button>
          <button class="band-btn" data-band="10m">10</button>
          <button class="band-btn" data-band="6m">6</button>
          <button class="band-btn" data-band="2m">2</button>
          <button class="band-btn" data-band="70cm">70</button>
        </div>
      </div>

      <div class="quick-row">
        <div class="quick-item">
          <label for="rxfreq">Rx / Tx Frequency (Hz)</label>
          <div class="inline rxtx-inline">
            <input id="rxfreq" type="number" min="0" max="5000" step="1" />
            <input id="txfreq" type="number" min="0" max="5000" step="1" />
            <button id="btn_set_rxtx">Set Rx+Tx</button>
          </div>
          <div class="inline rxtx-actions">
            <button id="btn_rxfreq">Set Rx</button>
            <button id="btn_txfreq">Set Tx</button>
          </div>
        </div>
        <div class="quick-item">
          <label for="mode_preset_select">Mode Preset (Rx/Tx)</label>
          <div class="inline mode-preset-inline">
            <select id="mode_preset_select"></select>
            <button id="btn_apply_mode_preset" type="button">Apply Preset</button>
            <button id="btn_save_mode_preset" type="button">Save Current</button>
          </div>
        </div>
        <div class="quick-item">
          <label>TX Control</label>
          <div class="inline tx-inline">
            <button id="btn_tx_on" class="ok">Enable TX</button>
            <button id="btn_auto_cq" type="button">Auto CQ</button>
            <button id="btn_tx_off" class="warn">Disable TX</button>
          </div>
        </div>
      </div>

      <div class="mode-row emission-row">
        <div class="group-title">Emission Options</div>
        <div id="ft2_controls" class="btn-grid emission-grid hidden">
          <button id="btn_async_l2" type="button">Async L2</button>
          <button id="btn_dual_carrier" type="button">Dual Carrier</button>
        </div>
        <div id="alt_controls" class="btn-grid emission-grid hidden">
          <button id="btn_alt_12" type="button">Alt 1/2</button>
        </div>
      </div>
    </section>

    <section class="panel waterfall-panel">
      <div class="panel-title-row">
        <div class="panel-title">Waterfall (experimental)</div>
        <button id="btn_wf_toggle" type="button">Waterfall OFF</button>
      </div>
      <div class="waterfall-wrap">
        <canvas id="waterfall_canvas" width="960" height="260"></canvas>
        <canvas id="waterfall_overlay" width="960" height="260"></canvas>
      </div>
      <div class="waterfall-hint">Tap/click on waterfall to set Rx frequency.</div>
    </section>

    <section class="split">
      <article class="panel activity">
        <div class="panel-title-row">
          <div class="panel-title">RX/TX Activity</div>
          <div class="activity-controls">
            <button id="btn_activity_pause" type="button">Pausa</button>
            <button id="btn_activity_clear" type="button">Pulisci</button>
          </div>
        </div>
        <table id="activity_table">
          <thead>
            <tr><th>UTC</th><th>dB</th><th>DT</th><th>Freq</th><th>Message</th></tr>
          </thead>
          <tbody id="activity_body"></tbody>
        </table>
      </article>

      <article class="panel control">
        <div class="panel-title">Remote Action</div>
        <label>Call
          <input id="caller_call" type="text" placeholder="K1ABC" />
        </label>
        <label>Grid
          <input id="caller_grid" type="text" placeholder="FN31" />
        </label>
        <button id="btn_call" class="primary">Call Station (slot-aware)</button>
      </article>
    </section>
  </div>
  <script src="/app.js"></script>
</body>
</html>)FT2HTML");
}

QByteArray dashboard_css()
{
  return QByteArrayLiteral(
R"FT2CSS(:root{
  --bg:#090f1a;
  --bg2:#0f1726;
  --panel:#121d2f;
  --panel2:#0f1a2a;
  --line:#2a3f60;
  --text:#eef4ff;
  --muted:#99acd0;
  --ok:#20cf78;
  --warn:#ff4a4a;
  --accent:#37a8ff;
}
*{box-sizing:border-box;font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
body{margin:0;background:radial-gradient(1600px 800px at 10% -10%,#1a2c4e 0%,var(--bg) 48%,#060b12 100%);color:var(--text)}
.hidden{display:none !important}
.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}
.login-overlay{position:fixed;inset:0;background:rgba(4,8,16,.86);backdrop-filter:blur(2px);z-index:1000;display:flex;align-items:center;justify-content:center;padding:18px}
.login-card{width:min(460px,100%);background:linear-gradient(180deg,#12233d,#0c1627);border:1px solid #2e4b73;border-radius:14px;padding:16px}
.login-card h2{margin:0 0 8px 0;font-size:1.1rem}
.login-card p{margin:0 0 10px 0;color:var(--muted);font-size:.88rem}
#login_form{display:grid;grid-template-columns:1fr;gap:7px}
#login_form label{font-size:.78rem;color:var(--muted)}
#login_form input{font-size:16px}
.login-token-hint{font-size:.76rem;color:#b9cdee;line-height:1.35;margin:2px 0 4px 0}
#login_error{min-height:1.2em;margin-top:8px;color:#ff9a9a;font-size:.82rem}
.app{max-width:1600px;margin:0 auto;padding:14px;padding-bottom:calc(14px + env(safe-area-inset-bottom))}
.top{display:flex;justify-content:space-between;align-items:center;gap:12px;margin-bottom:10px}
.top-actions{display:flex;align-items:center;gap:8px;flex-wrap:wrap;justify-content:flex-end}
h1{font-size:1.15rem;margin:0;font-weight:700;letter-spacing:.2px}
.sub{font-size:.82rem;color:var(--muted)}
.install-btn{padding:6px 10px;border-radius:999px;background:#194f80;border-color:#3f79af;font-size:.76rem}
.install-hint{margin:-2px 0 10px 0;padding:8px 10px;border:1px solid #34557b;border-radius:9px;background:#0b1525;color:#c9dcff;font-size:.78rem}
.badge{padding:6px 10px;border:1px solid var(--line);border-radius:999px;font-size:.8rem;background:#0b1526}
.badge.on{color:#06150d;background:var(--ok);border-color:transparent}
.badge.off{color:#ffd6d6;background:#2a1111;border-color:#592222}
.badge.trx{min-width:66px;text-align:center;font-weight:700}
.badge.trx.rx{color:#d6f8e6;background:#10492f;border-color:#2b8d62}
.badge.trx.tx{color:#fff2f2;background:#8d1f1f;border-color:#d14c4c;animation:txpulse .9s infinite ease-in-out}
.trx-peer{padding:6px 9px;border:1px solid #2e4f7a;border-radius:8px;background:#0b1526;color:#d5e6ff;font-size:.76rem;max-width:240px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.trx-peer.onair{border-color:#d14c4c;background:#3a1313;color:#ffdede;font-weight:700}
@keyframes txpulse{0%{filter:brightness(1)}50%{filter:brightness(1.2)}100%{filter:brightness(1)}}

.panel{background:linear-gradient(180deg,var(--panel),var(--panel2));border:1px solid var(--line);border-radius:12px;padding:10px}
.panel-title{font-size:.95rem;font-weight:700;margin-bottom:8px}
.panel-title-row{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:8px}
.activity-controls{display:flex;gap:6px}
.activity-controls button{padding:5px 9px;font-size:.74rem;border-radius:7px}
.activity-controls button.active{background:var(--warn);color:#fff}

.main-controls{margin-bottom:10px}
.freq-row{display:grid;grid-template-columns:280px 1fr;gap:10px}
.dial-box{background:#050a14;border:1px solid #2d466a;border-radius:10px;padding:10px}
.dial-box .lbl{font-size:.75rem;color:var(--muted);margin-bottom:5px}
.dial{font:700 1.45rem/1.1 ui-monospace,SFMono-Regular,Menlo,monospace;letter-spacing:.8px;color:#ffe66b}
.kv-grid{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:6px}
.kv{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:6px 8px;border:1px solid #223754;border-radius:8px;background:#0b1524}
.kv span{font-size:.72rem;color:var(--muted)}
.kv strong{font-size:.82rem}

.group-title{font-size:.76rem;color:var(--muted);margin:8px 0 4px}
.btn-grid{display:grid;grid-template-columns:repeat(13,minmax(0,1fr));gap:5px}
button{cursor:pointer;border:1px solid #3a5782;background:#1a2e4d;color:var(--text);border-radius:8px;padding:8px 6px;font-size:.79rem}
button:hover{filter:brightness(1.07)}
button.active{background:var(--ok);color:#001509;border-color:transparent;font-weight:700}
button.primary{background:#255489}
button.ok{background:#0f4f34;border-color:#2d8b65}
button.warn{background:#4c1e1e;border-color:#8a3939}
button.ok.active{background:#1fd27b;color:#04170f;border-color:#78ffbf;box-shadow:0 0 0 1px rgba(6,44,27,.5) inset,0 0 12px rgba(32,207,120,.45)}
button.warn.active{background:#ff4a4a;color:#fff;border-color:#ffc0c0;box-shadow:0 0 0 1px rgba(64,10,10,.35) inset,0 0 12px rgba(255,74,74,.45)}
button,input{-webkit-tap-highlight-color:transparent;touch-action:manipulation}

.quick-row{display:grid;grid-template-columns:1.4fr 1fr 1fr;gap:10px;margin-top:8px}
.quick-item label{display:block;font-size:.76rem;color:var(--muted);margin-bottom:4px}
.inline{display:grid;grid-template-columns:1fr auto;gap:6px}
.rx-inline{grid-template-columns:1fr auto}
.rxtx-inline{grid-template-columns:1fr 1fr auto}
.rxtx-actions{grid-template-columns:repeat(2,minmax(0,1fr));margin-top:6px}
.mode-preset-inline{grid-template-columns:minmax(120px,1fr) auto auto}
.tx-inline{grid-template-columns:repeat(3,minmax(96px,1fr))}
.quick-row .inline button{padding:6px 6px;font-size:.75rem}
input,select{width:100%;padding:8px;border-radius:8px;border:1px solid #37567f;background:#0a1322;color:var(--text)}
.emission-row{margin-top:8px}
.emission-grid{grid-template-columns:repeat(2,minmax(0,180px));max-width:420px}
#alt_controls.emission-grid{grid-template-columns:repeat(1,minmax(0,180px))}

.waterfall-panel{margin-bottom:10px}
.waterfall-wrap{position:relative;border:1px solid #243b5f;border-radius:10px;background:#040914;overflow:hidden}
.waterfall-wrap canvas{display:block;width:100%;height:260px}
#waterfall_canvas{background:#02050d;image-rendering:pixelated;cursor:crosshair}
#waterfall_overlay{position:absolute;inset:0;pointer-events:none;background:transparent;image-rendering:pixelated}
.waterfall-hint{margin-top:6px;font-size:.76rem;color:var(--muted)}

.split{display:grid;grid-template-columns:1.6fr .8fr;gap:10px}
.activity{min-height:430px}
#activity_table{width:100%;border-collapse:collapse;table-layout:fixed}
#activity_table th,#activity_table td{padding:4px 6px;border-bottom:1px solid #1f2f48;font-size:.78rem}
#activity_table th{position:sticky;top:0;background:#0a1322;color:#9bb0d8;text-align:left;z-index:1}
#activity_table th:nth-child(1),#activity_table td:nth-child(1){width:72px;font-family:ui-monospace,SFMono-Regular,Menlo,monospace}
#activity_table th:nth-child(2),#activity_table td:nth-child(2){width:44px}
#activity_table th:nth-child(3),#activity_table td:nth-child(3){width:52px}
#activity_table th:nth-child(4),#activity_table td:nth-child(4){width:62px}
#activity_table td:nth-child(5){font-family:ui-monospace,SFMono-Regular,Menlo,monospace;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#activity_body tr{background:#0a1422}
#activity_body tr:nth-child(2n){background:#0d1828}
#activity_body tr:hover{background:#173152}
#activity_body tr.sep td{color:#8aa2cc;background:#0a1120;font-style:italic}
#activity_body tr.sep.tx-event td{color:#ffe7ad;background:#392a09;font-style:normal;font-weight:700}
#activity_body tr.tx-row td{background:#fff36f;color:#171717;font-weight:700}
#activity_body tr.tx-row:hover td{background:#fff9a8}

.control label{display:block;font-size:.8rem;color:var(--muted);margin:8px 0 4px}
.action-status{margin-top:10px;padding:8px;border-radius:8px;border:1px solid #2b425f;background:#08101d;font-size:.8rem;color:#cde0ff}
.action-status.ok{border-color:#2d8b65;color:#9af1c8}
.action-status.err{border-color:#8a3939;color:#ffc3c3}

@media (max-width:1220px){
  .freq-row{grid-template-columns:1fr}
  .kv-grid{grid-template-columns:repeat(3,minmax(0,1fr))}
  .btn-grid{grid-template-columns:repeat(8,minmax(0,1fr))}
  .emission-grid{grid-template-columns:repeat(2,minmax(0,1fr));max-width:none}
  #alt_controls.emission-grid{grid-template-columns:1fr}
  .split{grid-template-columns:1fr}
}
@media (max-width:760px){
  .top{align-items:flex-start}
  .top-actions{align-self:flex-end}
  .kv-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .btn-grid{grid-template-columns:repeat(5,minmax(0,1fr))}
  .emission-grid{grid-template-columns:repeat(2,minmax(0,1fr));max-width:none}
  #alt_controls.emission-grid{grid-template-columns:1fr}
  .quick-row{grid-template-columns:1fr}
  #activity_table th,#activity_table td{padding:3px 4px;font-size:.74rem}
  #activity_table th:nth-child(1),#activity_table td:nth-child(1){width:62px}
  #activity_table th:nth-child(2),#activity_table td:nth-child(2){width:36px}
  #activity_table th:nth-child(3),#activity_table td:nth-child(3){width:44px}
  #activity_table th:nth-child(4),#activity_table td:nth-child(4){width:52px}
  #activity_table td:nth-child(5){white-space:normal;overflow:visible;text-overflow:clip;line-height:1.05}
}
@media (display-mode: standalone){
  body{padding-top:env(safe-area-inset-top)}
}
)FT2CSS");
}

QByteArray dashboard_js()
{
  return QByteArrayLiteral(
R"FT2JS((() => {
  const el = (id) => document.getElementById(id);
  const conn = el('conn');
  const trxLive = el('trx_live');
  const trxPeer = el('trx_peer');
  const activityBody = el('activity_body');
  const btnActivityPause = el('btn_activity_pause');
  const btnActivityClear = el('btn_activity_clear');
  const btnWfToggle = el('btn_wf_toggle');
  const btnAutoCQ = el('btn_auto_cq');
  const btnTxOn = el('btn_tx_on');
  const btnTxOff = el('btn_tx_off');
  const btnTxFreq = el('btn_txfreq');
  const btnSetRxTx = el('btn_set_rxtx');
  const txFreqInput = el('txfreq');
  const modePresetSelect = el('mode_preset_select');
  const btnApplyModePreset = el('btn_apply_mode_preset');
  const btnSaveModePreset = el('btn_save_mode_preset');
  const btnAsyncL2 = el('btn_async_l2');
  const btnDualCarrier = el('btn_dual_carrier');
  const btnAlt12 = el('btn_alt_12');
  const ft2Controls = el('ft2_controls');
  const altControls = el('alt_controls');
  const btnInstall = el('btn_install');
  const installHint = el('install_hint');
  const wfCanvas = el('waterfall_canvas');
  const wfCtx = wfCanvas ? wfCanvas.getContext('2d', {alpha:false}) : null;
  const wfOverlay = el('waterfall_overlay');
  const wfOverlayCtx = wfOverlay ? wfOverlay.getContext('2d', {alpha:true}) : null;
  const loginOverlay = el('login_overlay');
  const loginForm = el('login_form');
  const loginUser = el('login_user');
  const loginToken = el('login_token');
  const loginTokenHint = el('login_token_hint');
  const loginError = el('login_error');
  const loginCaption = el('login_caption');
  const isItalianUi = ((navigator.language || navigator.userLanguage || 'en').toLowerCase().startsWith('it'));
  const uiText = isItalianUi
    ? {
        loginCaptionAuth: 'Inserisci username e password per sbloccare la dashboard',
        loginCaptionNoAuth: 'Autenticazione non richiesta',
        tokenMinHint: 'La password di accesso deve avere almeno 12 caratteri.'
      }
    : {
        loginCaptionAuth: 'Insert username and password to unlock dashboard',
        loginCaptionNoAuth: 'Authentication not required',
        tokenMinHint: 'The access password must be at least 12 characters.'
      };
  let ws = null;
  let health = null;
  const activityRows = [];
  const ACTIVITY_MAX_ROWS = 150;
  const ACTIVITY_WINDOW_MS = 15 * 60 * 1000;
  const AUTH_TTL_MS = 30 * 60 * 1000;
  const AUTH_STORAGE_KEY = 'ft2_remote_auth_v1';
  const MODE_FREQ_STORAGE_KEY = 'ft2_remote_mode_freq_v1';
  const MODE_PRESET_MODES = ['FT2','FT8','FT4','MSK144','Q65','JT65','JT9','FST4','WSPR'];
  let activeMode = '';
  let activeBand = '';
  let authUser = 'admin';
  let authToken = '';
  let requiresAuth = false;
  let wsAuthed = false;
  let activityPaused = false;
  let lastTransmitting = null;
  let lastTxPeer = '';
  let txPeerFromRows = '';
  let txEnabledState = false;
  let autoCqEnabled = false;
  let asyncL2Enabled = false;
  let dualCarrierEnabled = false;
  let alt12Enabled = false;
  let waterfallEnabled = false;
  let waterfallMeta = {startHz:0, spanHz:0, width:0};
  let currentMode = '';
  let currentRxHz = 0;
  let currentTxHz = 0;
  let pendingRxHz = null;
  let deferredInstallPrompt = null;
  let statePollTimer = null;
  let waterfallPollTimer = null;
  let modeFrequencyPresets = {};

  const isIOS = () => /iphone|ipad|ipod/i.test(navigator.userAgent)
    || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  const isStandalone = () => (window.matchMedia && window.matchMedia('(display-mode: standalone)').matches)
    || window.navigator.standalone === true;
  const wsIsUsable = () => !!ws && ws.readyState === WebSocket.OPEN && (!requiresAuth || wsAuthed);
  const normalizeAuthUser = (v) => {
    const t = (v || '').toString().trim();
    return t || 'admin';
  };
  const normalizeAuthToken = (v) => (v || '').toString().replace(/[\r\n]+/g, '').trim();
  const normalizeModeKey = (v) => (v || '').toString().trim().toUpperCase();
  const sanitizeFrequency = (v) => {
    const n = Number(v);
    if (!Number.isFinite(n)) return null;
    return Math.max(0, Math.min(5000, Math.round(n)));
  };

  document.addEventListener('gesturestart', (ev) => ev.preventDefault(), {passive:false});
  document.addEventListener('gesturechange', (ev) => ev.preventDefault(), {passive:false});
  document.addEventListener('gestureend', (ev) => ev.preventDefault(), {passive:false});

  function loadSavedAuth() {
    try {
      const raw = localStorage.getItem(AUTH_STORAGE_KEY);
      if (!raw) return;
      const parsed = JSON.parse(raw);
      if (!parsed || typeof parsed !== 'object') return;
      const expiresAt = Number(parsed.expires_at || 0);
      if (!expiresAt || Date.now() >= expiresAt) {
        localStorage.removeItem(AUTH_STORAGE_KEY);
        return;
      }
      authUser = normalizeAuthUser(parsed.user);
      authToken = normalizeAuthToken(parsed.token);
    } catch {
      localStorage.removeItem(AUTH_STORAGE_KEY);
    }
  }

  function saveAuth() {
    const token = normalizeAuthToken(authToken);
    if (!token) return;
    const payload = {
      user: normalizeAuthUser(authUser),
      token,
      expires_at: Date.now() + AUTH_TTL_MS
    };
    try { localStorage.setItem(AUTH_STORAGE_KEY, JSON.stringify(payload)); } catch {}
  }

  function clearAuth() {
    authToken = '';
    authUser = 'admin';
    try { localStorage.removeItem(AUTH_STORAGE_KEY); } catch {}
  }

  function loadModeFrequencyPresets() {
    modeFrequencyPresets = {};
    try {
      const raw = localStorage.getItem(MODE_FREQ_STORAGE_KEY);
      if (!raw) return;
      const parsed = JSON.parse(raw);
      if (!parsed || typeof parsed !== 'object') return;
      Object.keys(parsed).forEach((key) => {
        const mode = normalizeModeKey(key);
        if (!MODE_PRESET_MODES.includes(mode)) return;
        const entry = parsed[key] || {};
        const rx = sanitizeFrequency(entry.rx);
        const tx = sanitizeFrequency(entry.tx);
        if (rx === null || tx === null) return;
        modeFrequencyPresets[mode] = {rx, tx};
      });
    } catch {
      modeFrequencyPresets = {};
      try { localStorage.removeItem(MODE_FREQ_STORAGE_KEY); } catch {}
    }
  }

  function saveModeFrequencyPresets() {
    try {
      localStorage.setItem(MODE_FREQ_STORAGE_KEY, JSON.stringify(modeFrequencyPresets));
    } catch {}
  }

  function getSelectedPresetMode() {
    const modeFromSelect = modePresetSelect ? normalizeModeKey(modePresetSelect.value) : '';
    if (modeFromSelect) return modeFromSelect;
    const modeNow = normalizeModeKey(currentMode || activeMode);
    return modeNow || 'FT2';
  }

  function updateModePresetSelect() {
    if (!modePresetSelect) return;
    const desired = getSelectedPresetMode();
    modePresetSelect.innerHTML = '';
    MODE_PRESET_MODES.forEach((mode) => {
      const preset = modeFrequencyPresets[mode];
      const option = document.createElement('option');
      option.value = mode;
      option.textContent = preset ? `${mode} (${preset.rx}/${preset.tx})` : `${mode} (---/---)`;
      modePresetSelect.appendChild(option);
    });
    if (MODE_PRESET_MODES.includes(desired)) {
      modePresetSelect.value = desired;
    }
  }

  function savePresetForMode(mode, rxHz, txHz) {
    const key = normalizeModeKey(mode);
    const rx = sanitizeFrequency(rxHz);
    const tx = sanitizeFrequency(txHz);
    if (!key || !MODE_PRESET_MODES.includes(key) || rx === null || tx === null) {
      return false;
    }
    modeFrequencyPresets[key] = {rx, tx};
    saveModeFrequencyPresets();
    updateModePresetSelect();
    return true;
  }

  async function applyPresetForMode(mode, silent = false) {
    const key = normalizeModeKey(mode);
    if (!key) return false;
    const preset = modeFrequencyPresets[key];
    if (!preset) return false;
    const rx = sanitizeFrequency(preset.rx);
    const tx = sanitizeFrequency(preset.tx);
    if (rx === null || tx === null) return false;
    if (el('rxfreq')) el('rxfreq').value = rx;
    if (txFreqInput) txFreqInput.value = tx;
    await sendCommand({type:'set_rx_frequency', rx_frequency_hz:rx});
    await sendCommand({type:'set_tx_frequency', tx_frequency_hz:tx});
    if (!silent) {
      setActionStatus(`preset ${key}: Rx ${rx} / Tx ${tx}`, false);
    }
    return true;
  }

  loadSavedAuth();
  loadModeFrequencyPresets();
  if (loginUser) loginUser.value = authUser || 'admin';
  if (loginToken && authToken) loginToken.value = authToken;
  if (loginCaption) loginCaption.textContent = uiText.loginCaptionAuth;
  if (loginTokenHint) loginTokenHint.textContent = uiText.tokenMinHint;
  updateModePresetSelect();

  const set = (id, v) => { const n = el(id); if (n) n.textContent = v ?? '-'; };
  const fmtHz = (v) => Number(v || 0).toLocaleString('en-US');
  const isNearBottom = (node) => node.scrollTop + node.clientHeight >= node.scrollHeight - 32;

  function parseActivityLine(line, arrivedMs = Date.now()) {
    const t = (line || '').trim();
    if (!t) return null;
    if (t.startsWith('-') || t.includes('------')) return {sep:true, text:t, arrivedMs};

    const parts = t.split(/\s+/);
    if (parts.length < 5) return {sep:true, text:t, arrivedMs};
    const utc = parts[0];
    const db = parts[1];
    const dt = parts[2];
    const freq = parts[3];
    const message = parts.slice(4).join(' ');
    const isTxRow = db.toUpperCase() === 'TX';

    const gridMatch = message.match(/\b([A-R]{2}\d{2}(?:[A-X]{2})?)\b/i);
    const grid = gridMatch ? gridMatch[1].toUpperCase() : '';
    const blacklist = new Set(['CQ','QRZ','DE','RR73','73','R','RRR','TU','PSE']);
    const tokens = message.toUpperCase().split(/[^A-Z0-9\/]+/).filter(Boolean);
    let call = '';
    for (const tok of tokens) {
      if (blacklist.has(tok)) continue;
      if (/^[A-Z0-9\/]{3,}$/.test(tok) && !/^\d+$/.test(tok)) {
        call = tok;
        break;
      }
    }
    return {utc, db, dt, freq, message, call, grid, isTxRow, arrivedMs};
  }

  function trimActivityRows() {
    const cutoff = Date.now() - ACTIVITY_WINDOW_MS;
    while (activityRows.length && (activityRows[0].arrivedMs || 0) < cutoff) {
      activityRows.shift();
    }
    if (activityRows.length > ACTIVITY_MAX_ROWS) {
      activityRows.splice(0, activityRows.length - ACTIVITY_MAX_ROWS);
    }
  }

  function renderActivity() {
    const stick = isNearBottom(activityBody.parentElement);
    activityBody.innerHTML = '';
    for (const row of activityRows) {
      const tr = document.createElement('tr');
      if (row.sep) {
        tr.className = row.txEvent ? 'sep tx-event' : 'sep';
        const td = document.createElement('td');
        td.colSpan = 5;
        td.textContent = row.text;
        tr.appendChild(td);
      } else {
        if (row.isTxRow) tr.classList.add('tx-row');
        const c1 = document.createElement('td'); c1.textContent = row.utc;
        const c2 = document.createElement('td'); c2.textContent = row.db;
        const c3 = document.createElement('td'); c3.textContent = row.dt;
        const c4 = document.createElement('td'); c4.textContent = row.freq;
        const c5 = document.createElement('td'); c5.textContent = row.message;
        tr.append(c1,c2,c3,c4,c5);
        if (row.call) {
          tr.dataset.call = row.call;
          tr.dataset.grid = row.grid || '';
          tr.title = row.grid ? `${row.call} ${row.grid}` : row.call;
          tr.addEventListener('click', () => {
            el('caller_call').value = row.call;
            if (row.grid) el('caller_grid').value = row.grid;
          });
          tr.addEventListener('dblclick', () => {
            sendCommand({
              type:'select_caller',
              target_call:row.call,
              target_grid:row.grid || ''
            });
          });
        }
      }
      activityBody.appendChild(tr);
    }
    if (stick) {
      activityBody.parentElement.scrollTop = activityBody.parentElement.scrollHeight;
    }
  }

  function applyWaterfallButtonState() {
    if (!btnWfToggle) return;
    btnWfToggle.classList.toggle('active', waterfallEnabled);
    btnWfToggle.textContent = waterfallEnabled ? 'Waterfall ON' : 'Waterfall OFF';
  }

  function applyAutoCqButtonState() {
    if (!btnAutoCQ) return;
    btnAutoCQ.classList.toggle('active', autoCqEnabled);
    btnAutoCQ.textContent = autoCqEnabled ? 'Auto CQ ON' : 'Auto CQ OFF';
  }

  function applyTxButtonsState() {
    if (btnTxOn) btnTxOn.classList.toggle('active', txEnabledState);
    if (btnTxOff) btnTxOff.classList.toggle('active', !txEnabledState);
  }

  function applyEmissionControlVisibility() {
    const isFt2 = (currentMode || '').toUpperCase() === 'FT2';
    if (ft2Controls) ft2Controls.classList.toggle('hidden', !isFt2);
    if (altControls) altControls.classList.toggle('hidden', isFt2);
  }

  function applyEmissionButtonsState() {
    if (btnAsyncL2) {
      btnAsyncL2.classList.toggle('active', asyncL2Enabled);
      btnAsyncL2.textContent = asyncL2Enabled ? 'Async L2 ON' : 'Async L2';
    }
    if (btnDualCarrier) {
      btnDualCarrier.classList.toggle('active', dualCarrierEnabled);
      btnDualCarrier.textContent = dualCarrierEnabled ? 'Dual Carrier ON' : 'Dual Carrier';
    }
    if (btnAlt12) {
      btnAlt12.classList.toggle('active', alt12Enabled);
      btnAlt12.textContent = alt12Enabled ? 'Alt 1/2 ON' : 'Alt 1/2';
    }
  }

  function updateAutoCqState(enabled) {
    autoCqEnabled = !!enabled;
    applyAutoCqButtonState();
  }

  function updateTxEnabledState(enabled) {
    txEnabledState = !!enabled;
    applyTxButtonsState();
  }

  function updateModeSpecificControlState(s) {
    if (!s || typeof s !== 'object') return;
    if (typeof s.async_l2_enabled === 'boolean') asyncL2Enabled = s.async_l2_enabled;
    if (typeof s.dual_carrier_enabled === 'boolean') dualCarrierEnabled = s.dual_carrier_enabled;
    if (typeof s.alt_12_enabled === 'boolean') alt12Enabled = s.alt_12_enabled;
    applyEmissionControlVisibility();
    applyEmissionButtonsState();
  }

  function updateWaterfallState(enabled) {
    waterfallEnabled = !!enabled;
    applyWaterfallButtonState();
    drawWaterfallOverlay();
    refreshWaterfallPoller();
  }

  function modeBandwidthHz(mode) {
    const m = (mode || '').toUpperCase();
    if (m === 'FT8') return 50;
    if (m === 'FT4') return 90;
    if (m === 'FT2') return 70;
    if (m === 'JT65') return 180;
    if (m === 'JT9') return 20;
    if (m === 'Q65') return 90;
    if (m === 'MSK144') return 200;
    if (m === 'FST4') return 50;
    if (m === 'FST4W') return 10;
    if (m === 'WSPR') return 6;
    return 50;
  }

  function hzToWaterfallX(hz) {
    if (!waterfallMeta.width || waterfallMeta.spanHz <= 0) return null;
    return ((hz - waterfallMeta.startHz) / waterfallMeta.spanHz) * waterfallMeta.width;
  }

  function drawBracketPair(startHz, bwHz, color) {
    if (!wfOverlayCtx || !wfOverlay) return;
    const h = wfOverlay.height;
    const x1 = hzToWaterfallX(startHz);
    const x2 = hzToWaterfallX(startHz + bwHz);
    if (x1 === null || x2 === null) return;
    const left = Math.max(0, Math.min(wfOverlay.width - 1, Math.round(Math.min(x1, x2)) + 0.5));
    const right = Math.max(0, Math.min(wfOverlay.width - 1, Math.round(Math.max(x1, x2)) + 0.5));
    wfOverlayCtx.strokeStyle = color;
    wfOverlayCtx.lineWidth = 1.5;
    wfOverlayCtx.beginPath();
    wfOverlayCtx.moveTo(left, 0);
    wfOverlayCtx.lineTo(left, h);
    wfOverlayCtx.moveTo(right, 0);
    wfOverlayCtx.lineTo(right, h);
    wfOverlayCtx.stroke();
    wfOverlayCtx.fillStyle = color;
    wfOverlayCtx.fillRect(left, 0, Math.max(1, right - left), 2);
  }

  function drawWaterfallOverlay() {
    if (!wfOverlay || !wfOverlayCtx) return;
    wfOverlayCtx.clearRect(0, 0, wfOverlay.width, wfOverlay.height);
    if (!waterfallEnabled) return;
    if (!waterfallMeta.width || waterfallMeta.spanHz <= 0) return;

    const bwHz = modeBandwidthHz(currentMode);
    const rxHz = (pendingRxHz !== null && pendingRxHz !== undefined) ? pendingRxHz : currentRxHz;
    drawBracketPair(rxHz, bwHz, 'rgba(64, 224, 255, 0.95)');
    drawBracketPair(currentTxHz, bwHz, 'rgba(255, 82, 82, 0.95)');
  }

  function drawWaterfallRow(msg) {
    if (!wfCanvas || !wfCtx || !msg || !msg.row_b64) return;

    let bytes;
    try {
      const raw = atob(String(msg.row_b64));
      bytes = new Uint8Array(raw.length);
      for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i) & 0xff;
    } catch {
      return;
    }
    if (!bytes.length) return;

    if (wfCanvas.width !== bytes.length) {
      wfCanvas.width = bytes.length;
      if (wfCanvas.height <= 1) wfCanvas.height = 260;
      if (wfOverlay) {
        wfOverlay.width = wfCanvas.width;
        wfOverlay.height = wfCanvas.height;
      }
      wfCtx.fillStyle = '#02050d';
      wfCtx.fillRect(0, 0, wfCanvas.width, wfCanvas.height);
    }

    wfCtx.drawImage(
      wfCanvas,
      0, 0, wfCanvas.width, wfCanvas.height - 1,
      0, 1, wfCanvas.width, wfCanvas.height - 1
    );

    const row = wfCtx.createImageData(wfCanvas.width, 1);
    for (let i = 0; i < bytes.length; i++) {
      const v = bytes[i];
      const t = v / 255.0;
      const r = Math.min(255, Math.round(255 * Math.max(0, (t - 0.35) * 1.8)));
      const g = Math.min(255, Math.round(220 * Math.max(0, (t - 0.10) * 1.5)));
      const b = Math.min(255, Math.round(255 * (0.10 + t * 0.95)));
      const o = i * 4;
      row.data[o + 0] = r;
      row.data[o + 1] = g;
      row.data[o + 2] = b;
      row.data[o + 3] = 255;
    }
    wfCtx.putImageData(row, 0, 0);

    const startHz = Number(msg.start_frequency_hz || 0);
    const spanHz = Number(msg.span_hz || 0);
    if (spanHz > 0) {
      waterfallMeta = {startHz, spanHz, width: bytes.length};
      drawWaterfallOverlay();
    }
  }

  const pushActivity = (line) => {
    if (activityPaused) return;
    const parsed = parseActivityLine(line);
    if (!parsed) return;
    if (parsed.isTxRow && parsed.call) {
      txPeerFromRows = parsed.call;
    }
    activityRows.push(parsed);
    trimActivityRows();
    renderActivity();
  };

  applyWaterfallButtonState();
  applyAutoCqButtonState();
  applyTxButtonsState();
  applyEmissionControlVisibility();
  applyEmissionButtonsState();

  function setConnectionState(isConnected, text) {
    conn.textContent = text || (isConnected ? 'connesso' : 'disconnesso');
    conn.classList.toggle('on', isConnected);
    conn.classList.toggle('off', !isConnected);
  }

  function describeInitError(err) {
    const message = (err && err.message ? String(err.message) : 'unknown error').trim();
    const lowered = message.toLowerCase();
    if (lowered.includes('failed to fetch')
        || lowered.includes('networkerror')
        || lowered.includes('load failed')) {
      const host = location.hostname || 'localhost';
      const port = location.port ? (':' + location.port) : '';
      return `init error: backend unreachable (${host}${port})`;
    }
    return 'init error: ' + (message || 'unknown error');
  }

  function updateTxBadge(transmitting, txEnabled) {
    if (!trxLive) return;
    const isTx = !!transmitting;
    trxLive.classList.toggle('tx', isTx);
    trxLive.classList.toggle('rx', !isTx);
    trxLive.textContent = isTx ? 'TX ON AIR' : (txEnabled ? 'RX (TX ARM)' : 'RX');
  }

  function updateTxPeer(transmitting, dxCall) {
    if (!trxPeer) return;
    const peer = (dxCall || txPeerFromRows || '').trim();
    if (transmitting && peer) {
      lastTxPeer = peer;
    }
    trxPeer.classList.toggle('onair', !!transmitting);
    if (transmitting) {
      trxPeer.textContent = peer ? ('TX with ' + peer) : 'TX with -';
    } else {
      trxPeer.textContent = lastTxPeer ? ('Last TX: ' + lastTxPeer) : 'DX: -';
    }
  }

  function appendTxEvent(transmitting, peer = '') {
    const now = Date.now();
    activityRows.push({
      sep: true,
      txEvent: true,
      text: transmitting ? (`TX STARTED${peer ? ' -> ' + peer : ''}`) : 'TX ENDED (RX)',
      arrivedMs: now
    });
    trimActivityRows();
    renderActivity();
  }

  function setActionStatus() {}

  async function fetchLatestWaterfall() {
    if (!waterfallEnabled) return;
    try {
      const r = await fetch('/api/v1/waterfall/latest', {headers: buildHeaders()});
      if (!r.ok) return;
      const j = await r.json();
      if (j && j.row_b64) drawWaterfallRow(j);
    } catch {}
  }

  function refreshWaterfallPoller() {
    const shouldPoll = waterfallEnabled && !wsIsUsable();
    if (shouldPoll && !waterfallPollTimer) {
      waterfallPollTimer = window.setInterval(fetchLatestWaterfall, 350);
      fetchLatestWaterfall();
      return;
    }
    if (!shouldPoll && waterfallPollTimer) {
      window.clearInterval(waterfallPollTimer);
      waterfallPollTimer = null;
    }
  }

  function startStatePolling() {
    if (statePollTimer) window.clearInterval(statePollTimer);
    statePollTimer = window.setInterval(async () => {
      try {
        const replaceActivity = !wsIsUsable() && !activityPaused;
        await getState(replaceActivity);
        if (!wsIsUsable()) {
          setConnectionState(true, 'connesso (poll)');
        }
      } catch {
        if (!wsIsUsable()) {
          setConnectionState(false, 'disconnected');
        }
      }
      refreshWaterfallPoller();
    }, 1000);
  }

  function stopStatePolling() {
    if (statePollTimer) {
      window.clearInterval(statePollTimer);
      statePollTimer = null;
    }
    if (waterfallPollTimer) {
      window.clearInterval(waterfallPollTimer);
      waterfallPollTimer = null;
    }
  }

  function setInstallHint(text = '', visible = false) {
    if (!installHint) return;
    installHint.textContent = text;
    installHint.classList.toggle('hidden', !visible);
  }

  function refreshInstallUi() {
    if (!btnInstall) return;
    const standalone = isStandalone();
    const secureEligible = window.isSecureContext || location.hostname === 'localhost';
    if (standalone) {
      btnInstall.classList.add('hidden');
      setInstallHint('', false);
      return;
    }

    if (deferredInstallPrompt) {
      btnInstall.classList.remove('hidden');
      setInstallHint('Tap Install App to add Decodium to your home screen.', true);
      return;
    }

    btnInstall.classList.add('hidden');
    if (isIOS()) {
      setInstallHint('iPhone/iPad: Safari -> Share -> Add to Home Screen.', true);
    } else if (!secureEligible) {
      setInstallHint('For full app install on Android, use HTTPS or localhost.', true);
    } else {
      setInstallHint('Android: Browser menu -> Add to Home screen.', true);
    }
  }

  function initPwaSupport() {
    if ('serviceWorker' in navigator) {
      window.addEventListener('load', () => {
        navigator.serviceWorker.register('/sw.js').catch(() => {});
      });
    }

    window.addEventListener('beforeinstallprompt', (ev) => {
      ev.preventDefault();
      deferredInstallPrompt = ev;
      refreshInstallUi();
    });

    window.addEventListener('appinstalled', () => {
      deferredInstallPrompt = null;
      refreshInstallUi();
    });

    if (btnInstall) {
      btnInstall.addEventListener('click', async () => {
        if (!deferredInstallPrompt) return;
        deferredInstallPrompt.prompt();
        try {
          await deferredInstallPrompt.userChoice;
        } catch (_) {}
        deferredInstallPrompt = null;
        refreshInstallUi();
      });
    }

    refreshInstallUi();
  }

  function showLogin(show, errorText = '') {
    if (!loginOverlay) return;
    loginOverlay.classList.toggle('hidden', !show);
    if (loginError) loginError.textContent = errorText || '';
    if (show) {
      if (loginUser && !loginUser.value) loginUser.value = authUser || 'admin';
      if (loginToken) {
        loginToken.focus();
        loginToken.select();
      } else if (loginUser) {
        loginUser.focus();
        loginUser.select();
      }
    }
  }

  function dismissMobileKeyboard() {
    const active = document.activeElement;
    if (active && typeof active.blur === 'function') active.blur();
    if (loginUser) loginUser.blur();
    if (loginToken) loginToken.blur();
    window.setTimeout(() => window.scrollTo(0, 0), 0);
  }

  function buildHeaders(base = {}) {
    const headers = {...base};
    const token = normalizeAuthToken(authToken);
    const user = normalizeAuthUser(authUser);
    if (token) headers['Authorization'] = 'Bearer ' + token;
    if (user) headers['X-Auth-User'] = user;
    return headers;
  }

  function refreshButtonHighlights() {
    document.querySelectorAll('.mode-btn').forEach((b) => {
      b.classList.toggle('active', b.dataset.mode === activeMode);
    });
    document.querySelectorAll('.band-btn').forEach((b) => {
      const cur = (activeBand || '').toLowerCase();
      const val = (b.dataset.band || '').toLowerCase();
      b.classList.toggle('active', cur === val);
    });
  };

  async function getHealth() {
    const r = await fetch('/api/v1/health', {headers: buildHeaders()});
    if (!r.ok) throw new Error('health http ' + r.status);
    health = await r.json();
    requiresAuth = !!health.requires_auth;
    if (typeof health.auth_user === 'string' && health.auth_user.trim()) {
      authUser = normalizeAuthUser(health.auth_user);
      if (loginUser && !loginUser.value) loginUser.value = authUser;
    }
    if (loginCaption) {
      loginCaption.textContent = requiresAuth
        ? uiText.loginCaptionAuth
        : uiText.loginCaptionNoAuth;
    }
    return health;
  }

  async function getState(replaceActivity = true) {
    const r = await fetch('/api/v1/state', {headers: buildHeaders()});
    if (r.status === 401) throw new Error('not authorized');
    if (!r.ok) throw new Error('state http ' + r.status);
    const s = await r.json();
    renderState(s);
    if (replaceActivity) {
      const preservedTxEvents = activityRows.filter((row) => row && row.txEvent);
      activityRows.length = 0;
      (s.recent_band_activity || []).forEach((line) => {
        const parsed = parseActivityLine(line);
        if (!parsed) return;
        if (parsed.isTxRow && parsed.call) {
          txPeerFromRows = parsed.call;
        }
        activityRows.push(parsed);
      });
      if (preservedTxEvents.length) {
        preservedTxEvents.forEach((row) => activityRows.push(row));
      }
      trimActivityRows();
      renderActivity();
    }
  }

  function renderState(s) {
    activeMode = (s.mode || '').toUpperCase();
    currentMode = activeMode;
    activeBand = (s.band || '').trim();
    updateModePresetSelect();
    set('st_mode', s.mode);
    set('st_band', s.band);
    set('st_dial', fmtHz(s.dial_frequency_hz) + ' Hz');
    set('st_rx', fmtHz(s.rx_frequency_hz) + ' Hz');
    set('st_tx', fmtHz(s.tx_frequency_hz) + ' Hz');
    if (typeof s.rx_frequency_hz === 'number') currentRxHz = Number(s.rx_frequency_hz);
    if (typeof s.tx_frequency_hz === 'number') currentTxHz = Number(s.tx_frequency_hz);
    pendingRxHz = null;
    if (typeof s.tx_enabled === 'boolean') {
      updateTxEnabledState(s.tx_enabled);
    }
    set('st_txen', s.tx_enabled ? 'yes' : 'no');
    set('st_mon', s.monitoring ? 'yes' : 'no');
    set('st_trx', s.transmitting ? 'yes' : 'no');
    updateTxBadge(s.transmitting, s.tx_enabled);
    updateTxPeer(s.transmitting, (s.dx_call || '').toString().trim().toUpperCase());
    if (typeof s.transmitting === 'boolean') {
      const isTx = !!s.transmitting;
      const peer = (s.dx_call || txPeerFromRows || '').toString().trim().toUpperCase();
      if (lastTransmitting === null) {
        if (isTx) {
          appendTxEvent(true, peer);
        }
      } else if (lastTransmitting !== isTx) {
        appendTxEvent(isTx, peer);
      }
      lastTransmitting = isTx;
    }
    set('st_mycall', s.my_call || '-');
    set('st_dxcall', s.dx_call || '-');
    if (typeof s.rx_frequency_hz === 'number') el('rxfreq').value = s.rx_frequency_hz;
    if (typeof s.tx_frequency_hz === 'number' && txFreqInput) txFreqInput.value = s.tx_frequency_hz;
    if (typeof s.auto_cq_enabled === 'boolean') {
      updateAutoCqState(s.auto_cq_enabled);
    }
    if (typeof s.waterfall_enabled === 'boolean') {
      updateWaterfallState(s.waterfall_enabled);
    }
    updateModeSpecificControlState(s);
    drawWaterfallOverlay();
    refreshButtonHighlights();
  }

  async function sendCommand(payload) {
    payload.command_id = payload.command_id || ('cmd-' + Date.now() + '-' + Math.floor(Math.random()*100000));
    payload.client_sent_ms = Date.now();
    if (requiresAuth && !authToken) throw new Error('token missing');
    const headers = buildHeaders({'Content-Type':'application/json'});
    const r = await fetch('/api/v1/commands', {method:'POST', headers, body:JSON.stringify(payload)});
    const j = await r.json();
    const status = (j.status || '').toString();
    const type = (j.type || payload.type || 'command').toString();
    const isErr = status.startsWith('rejected') || !!j.error;
    setActionStatus(`${type}: ${status || (isErr ? 'error' : 'ok')}`, isErr);
    if (type === 'set_waterfall_enabled' && typeof j.enabled === 'boolean') {
      updateWaterfallState(j.enabled);
    }
    if (type === 'set_auto_cq' && typeof j.enabled === 'boolean') {
      updateAutoCqState(j.enabled);
    }
    if (type === 'set_tx_enabled' && typeof j.enabled === 'boolean') {
      updateTxEnabledState(j.enabled);
    }
    if (type === 'set_async_l2' && typeof j.enabled === 'boolean') {
      asyncL2Enabled = !!j.enabled;
      applyEmissionButtonsState();
    }
    if (type === 'set_dual_carrier' && typeof j.enabled === 'boolean') {
      dualCarrierEnabled = !!j.enabled;
      applyEmissionButtonsState();
    }
    if (type === 'set_alt_12' && typeof j.enabled === 'boolean') {
      alt12Enabled = !!j.enabled;
      applyEmissionButtonsState();
    }
    if (type === 'set_rx_frequency' && typeof j.rx_frequency_hz === 'number') {
      currentRxHz = Number(j.rx_frequency_hz);
      pendingRxHz = null;
      drawWaterfallOverlay();
    }
    if (type === 'set_tx_frequency' && typeof j.tx_frequency_hz === 'number') {
      currentTxHz = Number(j.tx_frequency_hz);
      drawWaterfallOverlay();
    }
    if (type === 'set_mode' && typeof j.mode === 'string') {
      currentMode = j.mode.toUpperCase();
      activeMode = currentMode;
      updateModePresetSelect();
      applyEmissionControlVisibility();
      drawWaterfallOverlay();
      refreshButtonHighlights();
      setTimeout(() => {
        applyPresetForMode(currentMode, true).catch(() => {});
      }, 220);
    }
    setTimeout(() => { getState(false).catch(() => {}); }, 120);
    return j;
  }

  function connectWs() {
    if (!health || !health.ws_port) return;
    if (requiresAuth && !authToken) return;
    const scheme = location.protocol === 'https:' ? 'wss' : 'ws';
    const url = `${scheme}://${location.hostname}:${health.ws_port}`;
    wsAuthed = !requiresAuth;
    ws = new WebSocket(url);
    ws.onopen = () => {
      if (requiresAuth) {
        setConnectionState(false, 'autenticazione...');
        ws.send(JSON.stringify({type:'auth', username:authUser, token:authToken}));
      } else {
        setConnectionState(true, 'connesso');
      }
      refreshWaterfallPoller();
    };
    ws.onclose = () => {
      setConnectionState(false, 'disconnected');
      refreshWaterfallPoller();
      if (!loginOverlay || loginOverlay.classList.contains('hidden')) {
        setTimeout(connectWs, 1500);
      }
    };
    ws.onerror = () => {
      setConnectionState(false, 'ws error');
      refreshWaterfallPoller();
    };
    ws.onmessage = (ev) => {
      let m = {};
      try { m = JSON.parse(ev.data); } catch { return; }
      if (m.event === 'hello' && typeof m.requires_auth === 'boolean') {
        requiresAuth = !!m.requires_auth;
        if (typeof m.auth_user === 'string' && m.auth_user.trim()) authUser = normalizeAuthUser(m.auth_user);
        if (loginUser && !loginUser.value) loginUser.value = authUser || 'admin';
        if (typeof m.mode === 'string') {
          currentMode = m.mode.toUpperCase();
          activeMode = currentMode;
          updateModePresetSelect();
          refreshButtonHighlights();
        }
        if (typeof m.auto_cq_enabled === 'boolean') {
          updateAutoCqState(m.auto_cq_enabled);
        }
        if (typeof m.tx_enabled === 'boolean') {
          updateTxEnabledState(m.tx_enabled);
        }
        updateModeSpecificControlState(m);
        if (typeof m.waterfall_enabled === 'boolean') {
          updateWaterfallState(m.waterfall_enabled);
        }
        refreshWaterfallPoller();
      } else if (m.event === 'auth_ok') {
        wsAuthed = true;
        setConnectionState(true, 'connesso');
        getState(false).catch(() => {});
        refreshWaterfallPoller();
      } else if (m.event === 'auth_failed') {
        wsAuthed = false;
        setConnectionState(false, 'auth failed');
        stopStatePolling();
        clearAuth();
        showLogin(true, 'Invalid username or password');
        if (ws) ws.close();
      } else if (m.event === 'telemetry') {
        if (!requiresAuth || wsAuthed) renderState(m);
      } else if (m.event === 'band_activity') {
        if (!requiresAuth || wsAuthed) pushActivity(m.line);
      } else if (m.event === 'waterfall_state') {
        if (!requiresAuth || wsAuthed) updateWaterfallState(m.enabled);
      } else if (m.event === 'waterfall_row') {
        if (!requiresAuth || wsAuthed) drawWaterfallRow(m);
      } else if (m.event === 'command_ack' || m.event === 'command_execute') {
        const status = (m.status || (m.event === 'command_execute' ? 'executed' : 'ack')).toString();
        const type = (m.type || 'command').toString();
        const isErr = status.startsWith('rejected');
        setActionStatus(`${type}: ${status}`, isErr);
        if (type === 'set_waterfall_enabled' && typeof m.enabled === 'boolean') {
          updateWaterfallState(m.enabled);
        }
        if (type === 'set_auto_cq' && typeof m.enabled === 'boolean') {
          updateAutoCqState(m.enabled);
        }
        if (type === 'set_tx_enabled' && typeof m.enabled === 'boolean') {
          updateTxEnabledState(m.enabled);
        }
        if (type === 'set_async_l2' && typeof m.enabled === 'boolean') {
          asyncL2Enabled = !!m.enabled;
          applyEmissionButtonsState();
        }
        if (type === 'set_dual_carrier' && typeof m.enabled === 'boolean') {
          dualCarrierEnabled = !!m.enabled;
          applyEmissionButtonsState();
        }
        if (type === 'set_alt_12' && typeof m.enabled === 'boolean') {
          alt12Enabled = !!m.enabled;
          applyEmissionButtonsState();
        }
        if (type === 'set_tx_frequency' && typeof m.tx_frequency_hz === 'number') {
          currentTxHz = Number(m.tx_frequency_hz);
          if (txFreqInput) txFreqInput.value = m.tx_frequency_hz;
          drawWaterfallOverlay();
        }
        if (type === 'set_mode' && typeof m.mode === 'string') {
          currentMode = m.mode.toUpperCase();
          activeMode = currentMode;
          updateModePresetSelect();
          refreshButtonHighlights();
        }
      }
    };
  }

  el('btn_rxfreq').onclick = () => {
    const rx = sanitizeFrequency(el('rxfreq').value);
    if (rx === null) return;
    sendCommand({type:'set_rx_frequency', rx_frequency_hz:rx})
      .then(() => savePresetForMode(currentMode || activeMode, rx, txFreqInput ? (txFreqInput.value || currentTxHz) : currentTxHz))
      .catch((e) => showLogin(true, e.message));
  };
  if (btnTxFreq) {
    btnTxFreq.onclick = () => {
      const tx = sanitizeFrequency(txFreqInput ? txFreqInput.value : null);
      if (tx === null) return;
      sendCommand({type:'set_tx_frequency', tx_frequency_hz:tx})
        .then(() => savePresetForMode(currentMode || activeMode, el('rxfreq').value || currentRxHz, tx))
        .catch((e) => showLogin(true, e.message));
    };
  }
  if (btnSetRxTx) {
    btnSetRxTx.onclick = async () => {
      const rx = sanitizeFrequency(el('rxfreq').value);
      const tx = sanitizeFrequency(txFreqInput ? txFreqInput.value : null);
      if (rx === null || tx === null) return;
      try {
        await sendCommand({type:'set_rx_frequency', rx_frequency_hz:rx});
        await sendCommand({type:'set_tx_frequency', tx_frequency_hz:tx});
        savePresetForMode(currentMode || activeMode, rx, tx);
        setActionStatus(`Rx/Tx set: ${rx}/${tx} Hz`, false);
      } catch (e) {
        showLogin(true, e.message);
      }
    };
  }
  if (modePresetSelect) {
    modePresetSelect.addEventListener('change', () => {
      const mode = getSelectedPresetMode();
      const preset = modeFrequencyPresets[mode];
      if (!preset) return;
      if (el('rxfreq')) el('rxfreq').value = preset.rx;
      if (txFreqInput) txFreqInput.value = preset.tx;
    });
  }
  if (btnSaveModePreset) {
    btnSaveModePreset.onclick = () => {
      const mode = getSelectedPresetMode();
      const rx = sanitizeFrequency(el('rxfreq').value || currentRxHz);
      const tx = sanitizeFrequency(txFreqInput ? (txFreqInput.value || currentTxHz) : currentTxHz);
      if (rx === null || tx === null) return;
      if (savePresetForMode(mode, rx, tx)) {
        setActionStatus(`preset saved: ${mode} (${rx}/${tx})`, false);
      }
    };
  }
  if (btnApplyModePreset) {
    btnApplyModePreset.onclick = () => {
      applyPresetForMode(getSelectedPresetMode(), false).catch((e) => showLogin(true, e.message));
    };
  }
  if (btnTxOn) {
    btnTxOn.onclick = () => sendCommand({type:'set_tx_enabled', enabled:true}).catch((e) => showLogin(true, e.message));
  }
  if (btnTxOff) {
    btnTxOff.onclick = () => sendCommand({type:'set_tx_enabled', enabled:false}).catch((e) => showLogin(true, e.message));
  }
  if (btnAutoCQ) {
    btnAutoCQ.onclick = () => sendCommand({type:'set_auto_cq', enabled:!autoCqEnabled}).catch((e) => showLogin(true, e.message));
  }
  if (btnAsyncL2) {
    btnAsyncL2.onclick = () => sendCommand({type:'set_async_l2', enabled:!asyncL2Enabled}).catch((e) => showLogin(true, e.message));
  }
  if (btnDualCarrier) {
    btnDualCarrier.onclick = () => sendCommand({type:'set_dual_carrier', enabled:!dualCarrierEnabled}).catch((e) => showLogin(true, e.message));
  }
  if (btnAlt12) {
    btnAlt12.onclick = () => sendCommand({type:'set_alt_12', enabled:!alt12Enabled}).catch((e) => showLogin(true, e.message));
  }
  el('btn_call').onclick = () => sendCommand({type:'select_caller', target_call:el('caller_call').value, target_grid:el('caller_grid').value}).catch((e) => showLogin(true, e.message));
  if (btnWfToggle) {
    btnWfToggle.onclick = () => sendCommand({type:'set_waterfall_enabled', enabled:!waterfallEnabled}).catch((e) => showLogin(true, e.message));
  }
  if (wfCanvas) {
    wfCanvas.addEventListener('click', (ev) => {
      if (!waterfallMeta.width || waterfallMeta.spanHz <= 0) return;
      const rect = wfCanvas.getBoundingClientRect();
      const relX = Math.max(0, Math.min(rect.width, ev.clientX - rect.left));
      const ratio = rect.width > 0 ? relX / rect.width : 0;
      const rxHz = Math.round(waterfallMeta.startHz + ratio * waterfallMeta.spanHz);
      pendingRxHz = rxHz;
      drawWaterfallOverlay();
      sendCommand({type:'set_rx_frequency', rx_frequency_hz:rxHz}).catch((e) => {
        pendingRxHz = null;
        drawWaterfallOverlay();
        showLogin(true, e.message);
      });
    });
  }
  document.querySelectorAll('.mode-btn').forEach((b) => {
    b.addEventListener('click', () => sendCommand({type:'set_mode', mode:b.dataset.mode}).catch((e) => showLogin(true, e.message)));
  });
  document.querySelectorAll('.band-btn').forEach((b) => {
    b.addEventListener('click', () => sendCommand({type:'set_band', band:b.dataset.band}).catch((e) => showLogin(true, e.message)));
  });

  if (btnActivityPause) {
    btnActivityPause.addEventListener('click', () => {
      activityPaused = !activityPaused;
      btnActivityPause.classList.toggle('active', activityPaused);
      btnActivityPause.textContent = activityPaused ? 'Riprendi' : 'Pausa';
      setActionStatus(activityPaused ? 'Band activity in pausa' : 'Band activity ripresa', false);
    });
  }
  if (btnActivityClear) {
    btnActivityClear.addEventListener('click', () => {
      activityRows.length = 0;
      renderActivity();
      setActionStatus('Band activity pulita', false);
    });
  }

  async function attemptUnlock() {
    authUser = normalizeAuthUser(loginUser ? loginUser.value : '');
    authToken = normalizeAuthToken(loginToken ? loginToken.value : '');
    if (!authToken) {
      showLogin(true, 'Password required');
      return;
    }
    try {
      await getState(true);
      saveAuth();
      dismissMobileKeyboard();
      showLogin(false);
      startStatePolling();
      connectWs();
    } catch (e) {
      stopStatePolling();
      clearAuth();
      showLogin(true, 'Invalid username or password');
    }
  }

  if (loginForm) {
    loginForm.addEventListener('submit', (ev) => {
      ev.preventDefault();
      attemptUnlock().catch(() => {});
    });
  }

  initPwaSupport();

  (async () => {
    try {
      await getHealth();

      if (requiresAuth) {
        if (!authToken) {
          stopStatePolling();
          setConnectionState(false, 'locked');
          if (loginUser) loginUser.value = authUser || 'admin';
          showLogin(true);
          return;
        }
        await getState(true);
        saveAuth();
        showLogin(false);
      } else {
        await getState(true);
      }

      startStatePolling();
      connectWs();
    } catch (e) {
      if (requiresAuth) {
        stopStatePolling();
        setConnectionState(false, 'locked');
        showLogin(true);
      } else {
        setConnectionState(false, describeInitError(e));
      }
    }
  })();
})();
)FT2JS");
}

QByteArray dashboard_manifest()
{
  return QByteArrayLiteral(
R"FT2MAN({
  "name": "Decodium Remote Console",
  "short_name": "Decodium",
  "description": "Fork by 9H1SR Salvatore Raccampo",
  "start_url": "/",
  "scope": "/",
  "display": "standalone",
  "background_color": "#090f1a",
  "theme_color": "#0b1220",
  "icons": [
    {
      "src": "/icon-192.png",
      "sizes": "192x192",
      "type": "image/png",
      "purpose": "any maskable"
    },
    {
      "src": "/icon-512.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any maskable"
    }
  ]
})FT2MAN");
}

QByteArray dashboard_service_worker_js()
{
  return QByteArrayLiteral(
R"FT2SW(const CACHE_NAME = 'decodium-remote-shell-v2';
const APP_SHELL = [
  '/',
  '/index.html',
  '/app.css',
  '/app.js',
  '/manifest.webmanifest',
  '/icon-192.png',
  '/icon-512.png'
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => cache.addAll(APP_SHELL))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE_NAME).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (event) => {
  const req = event.request;
  const url = new URL(req.url);

  if (url.pathname.startsWith('/api/')) return;

  if (req.mode === 'navigate') {
    event.respondWith(
      fetch(req).catch(() => caches.match('/index.html'))
    );
    return;
  }

  event.respondWith(
    caches.match(req).then((cached) => {
      if (cached) return cached;
      return fetch(req).then((response) => {
        if (!response || response.status !== 200 || response.type !== 'basic') {
          return response;
        }
        const copy = response.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(req, copy));
        return response;
      });
    })
  );
});
)FT2SW");
}

QByteArray dashboard_icon_png(int size)
{
  size = qBound(64, size, 1024);
  QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(9, 15, 26));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QRect const frame = image.rect().adjusted(size / 12, size / 12, -size / 12, -size / 12);
  painter.setPen(QPen(QColor(37, 168, 255), qMax(2, size / 40)));
  painter.setBrush(QColor(18, 29, 47));
  painter.drawRoundedRect(frame, size / 8, size / 8);

  QFont font = painter.font();
  font.setBold(true);
  font.setPixelSize(size / 3);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(frame, Qt::AlignCenter, QStringLiteral("FT2"));

  painter.end();

  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return png;
}
}

RemoteCommandServer::RemoteCommandServer(QObject * parent)
  : QObject {parent}
{
  scheduleTimer_.setSingleShot(true);
  connect(&scheduleTimer_, &QTimer::timeout, this, &RemoteCommandServer::onScheduleTimeout);

  telemetryTimer_.setInterval(1000);
  connect(&telemetryTimer_, &QTimer::timeout, this, &RemoteCommandServer::onTelemetryTick);
}

RemoteCommandServer::~RemoteCommandServer()
{
  stop();
}

bool RemoteCommandServer::start(quint16 wsPort, QHostAddress const& address, quint16 httpPort)
{
  stop();
  wsPort_ = wsPort;
  bindAddress_ = address;
  httpPort_ = 0;

  bool const remoteExposed = !(address.isLoopback()
                               || address == QHostAddress::LocalHost
                               || address == QHostAddress::LocalHostIPv6);
  if (remoteExposed)
    {
      auto const token = authToken_.trimmed();
      if (token.isEmpty())
        {
          Q_EMIT logMessage(QStringLiteral("Remote WS warning: LAN/WAN bind without token authentication."));
        }
      else if (token.size() < 12)
        {
          Q_EMIT logMessage(QStringLiteral("Remote WS warning: token shorter than 12 characters on LAN/WAN bind."));
        }
    }

  server_ = new QWebSocketServer(QStringLiteral("Decodium Remote WS"), QWebSocketServer::NonSecureMode, this);
  if (!server_->listen(address, wsPort))
    {
      Q_EMIT logMessage(QString {"Remote WS failed on %1:%2 (%3)"}
                          .arg(address.toString())
                          .arg(wsPort)
                          .arg(server_->errorString()));
      server_->deleteLater();
      server_.clear();
      return false;
    }

  connect(server_.data(), &QWebSocketServer::newConnection, this, &RemoteCommandServer::onNewConnection);
  connect(server_.data(), &QWebSocketServer::closed, this, [this] {
      Q_EMIT logMessage(QStringLiteral("Remote WS closed"));
    });

  telemetryTimer_.start();
  Q_EMIT logMessage(QString {"Remote WS listening on ws://%1:%2"}
                      .arg(server_->serverAddress().toString())
                      .arg(server_->serverPort()));

  if (0 == httpPort)
    {
      if (wsPort < std::numeric_limits<quint16>::max ())
        {
          httpPort = static_cast<quint16>(wsPort + 1);
        }
      else
        {
          httpPort = wsPort;
        }
    }

  if (httpPort > 0)
    {
      httpServer_ = new QTcpServer {this};
      if (httpServer_->listen(address, httpPort))
        {
          httpPort_ = httpPort;
          connect(httpServer_.data(), &QTcpServer::newConnection, this, &RemoteCommandServer::onHttpNewConnection);
          Q_EMIT logMessage(QString {"Remote HTTP listening on http://%1:%2/api/v1/commands"}
                              .arg(httpServer_->serverAddress().toString())
                              .arg(httpServer_->serverPort()));
        }
      else
        {
          Q_EMIT logMessage(QString {"Remote HTTP disabled: bind failed on %1:%2 (%3)"}
                              .arg(address.toString())
                              .arg(httpPort)
                              .arg(httpServer_->errorString()));
          httpServer_->deleteLater();
          httpServer_.clear();
        }
    }
  return true;
}

void RemoteCommandServer::stop()
{
  telemetryTimer_.stop();
  scheduleTimer_.stop();
  pending_.clear();
  seenCommandIds_.clear();
  recentBandActivity_.clear();
  authenticatedClients_.clear();
  waterfallEnabled_ = false;
  lastWaterfallRowUtcMs_ = 0;
  lastWaterfallRowB64_.clear();
  lastWaterfallWidth_ = 0;
  lastWaterfallStartFrequencyHz_ = 0;
  lastWaterfallSpanHz_ = 0;
  lastWaterfallRxFrequencyHz_ = 0;
  lastWaterfallTxFrequencyHz_ = 0;
  lastWaterfallMode_.clear();

  for (auto const& clientPtr : clients_)
    {
      auto * client = clientPtr.data();
      if (client)
        {
          client->disconnect(this);
          client->close();
          client->deleteLater();
        }
    }
  clients_.clear();

  if (server_)
    {
      server_->close();
      server_->deleteLater();
      server_.clear();
    }

  for (auto it = httpStates_.begin(); it != httpStates_.end(); ++it)
    {
      if (it.key())
        {
          it.key()->disconnect(this);
          it.key()->close();
          it.key()->deleteLater();
        }
    }
  httpStates_.clear();

  if (httpServer_)
    {
      httpServer_->close();
      httpServer_->deleteLater();
      httpServer_.clear();
    }

  wsPort_ = 0;
  httpPort_ = 0;
}

bool RemoteCommandServer::isRunning() const
{
  return server_ && server_->isListening();
}

void RemoteCommandServer::setRuntimeStateProvider(RuntimeStateProvider provider)
{
  runtimeProvider_ = provider;
}

void RemoteCommandServer::setGuardPreMs(int ms)
{
  guardPreMs_ = qBound(50, ms, 1500);
}

void RemoteCommandServer::setMaxCommandAgeMs(int ms)
{
  maxCommandAgeMs_ = qBound(1000, ms, 60000);
}

void RemoteCommandServer::setAuthUser(QString const& user)
{
  auto normalized = user.trimmed();
  if (normalized.isEmpty())
    {
      normalized = QStringLiteral("admin");
    }
  authUser_ = normalized;
}

void RemoteCommandServer::setAuthToken(QString const& token)
{
  authToken_ = token.trimmed();
}

void RemoteCommandServer::publishBandActivityLine(QString const& line)
{
  auto normalized = line.trimmed();
  if (normalized.isEmpty())
    {
      return;
    }

  recentBandActivity_.append(normalized);
  while (recentBandActivity_.size() > maxRecentBandActivity_)
    {
      recentBandActivity_.removeFirst();
    }

  QJsonObject event {
    {"event", QStringLiteral("band_activity")},
    {"line", normalized},
    {"server_now_ms", currentUtcMs()},
  };
  broadcastJson(event);
}

void RemoteCommandServer::publishWaterfallRow(QByteArray const& rowLevels,
                                              int startFrequencyHz,
                                              int spanHz,
                                              int rxFrequencyHz,
                                              int txFrequencyHz,
                                              QString const& mode)
{
  if (!waterfallEnabled_ || rowLevels.isEmpty())
    {
      return;
    }

  qint64 const nowUtcMs = currentUtcMs();
  if (lastWaterfallRowUtcMs_ > 0
      && (nowUtcMs - lastWaterfallRowUtcMs_) < waterfallMinFrameIntervalMs_)
    {
      return;
    }
  lastWaterfallRowUtcMs_ = nowUtcMs;
  lastWaterfallRowB64_ = rowLevels.toBase64();
  lastWaterfallWidth_ = rowLevels.size();
  lastWaterfallStartFrequencyHz_ = startFrequencyHz;
  lastWaterfallSpanHz_ = qMax(1, spanHz);
  lastWaterfallRxFrequencyHz_ = rxFrequencyHz;
  lastWaterfallTxFrequencyHz_ = txFrequencyHz;
  lastWaterfallMode_ = mode;

  QJsonObject event {
    {"event", QStringLiteral("waterfall_row")},
    {"row_b64", QString::fromLatin1(lastWaterfallRowB64_)},
    {"width", lastWaterfallWidth_},
    {"start_frequency_hz", lastWaterfallStartFrequencyHz_},
    {"span_hz", lastWaterfallSpanHz_},
    {"rx_frequency_hz", lastWaterfallRxFrequencyHz_},
    {"tx_frequency_hz", lastWaterfallTxFrequencyHz_},
    {"mode", lastWaterfallMode_},
    {"server_now_ms", nowUtcMs},
  };
  broadcastJson(event);
}

void RemoteCommandServer::setWaterfallEnabled(bool enabled, QString const& commandId)
{
  bool const changed = (waterfallEnabled_ != enabled);
  waterfallEnabled_ = enabled;
  if (!waterfallEnabled_)
    {
      lastWaterfallRowUtcMs_ = 0;
    }

  if (changed)
    {
      Q_EMIT waterfallStreamingChanged(waterfallEnabled_);
    }

  broadcastWaterfallState(commandId);
}

void RemoteCommandServer::broadcastWaterfallState(QString const& commandId)
{
  QJsonObject event {
    {"event", QStringLiteral("waterfall_state")},
    {"enabled", waterfallEnabled_},
    {"server_now_ms", currentUtcMs()},
  };
  if (!commandId.isEmpty())
    {
      event.insert(QStringLiteral("command_id"), commandId);
    }
  broadcastJson(event);
}

RemoteCommandServer::RuntimeState RemoteCommandServer::runtimeState() const
{
  RuntimeState state;
  if (runtimeProvider_)
    {
      state = runtimeProvider_();
    }
  if (state.nowUtcMs <= 0)
    {
      state.nowUtcMs = ntpCorrectedCurrentMSecsSinceEpoch();
    }
  if (state.periodMs <= 0)
    {
      state.periodMs = 3750;
    }
  return state;
}

qint64 RemoteCommandServer::currentUtcMs() const
{
  return runtimeState().nowUtcMs;
}

qint64 RemoteCommandServer::nextSlotUtcMs(qint64 nowUtcMs, qint64 periodMs) const
{
  if (periodMs <= 0)
    {
      periodMs = 3750;
    }
  return ((nowUtcMs / periodMs) + 1) * periodMs;
}

void RemoteCommandServer::onNewConnection()
{
  if (!server_)
    {
      return;
    }
  while (server_->hasPendingConnections())
    {
      auto * client = server_->nextPendingConnection();
      if (!client)
        {
          continue;
        }

      clients_.append(client);
      connect(client, &QWebSocket::textMessageReceived, this, &RemoteCommandServer::onTextMessageReceived);
      connect(client, &QWebSocket::disconnected, this, &RemoteCommandServer::onSocketDisconnected);

      auto const rt = runtimeState();
      QJsonObject hello {
        {"event", QStringLiteral("hello")},
        {"server", QStringLiteral("decodium-remote-ws")},
        {"protocol", 1},
        {"requires_auth", isAuthRequired()},
        {"auth_user", authUser_},
        {"mode", rt.mode},
        {"tx_enabled", rt.txEnabled},
        {"async_l2_enabled", rt.asyncL2Enabled},
        {"dual_carrier_enabled", rt.dualCarrierEnabled},
        {"alt_12_enabled", rt.alt12Enabled},
        {"auto_cq_enabled", rt.autoCqEnabled},
        {"waterfall_enabled", waterfallEnabled_},
      };
      sendJson(client, hello);
    }

  Q_EMIT logMessage(QString {"Remote WS client connected (%1 clients)"}.arg(clients_.size()));
}

void RemoteCommandServer::removeClient(QWebSocket * client)
{
  authenticatedClients_.remove(client);
  for (int i = clients_.size() - 1; i >= 0; --i)
    {
      if (!clients_[i] || clients_[i].data() == client)
        {
          clients_.removeAt(i);
        }
    }
}

void RemoteCommandServer::onSocketDisconnected()
{
  auto * client = qobject_cast<QWebSocket *> (sender());
  removeClient(client);
  if (client)
    {
      client->deleteLater();
    }
}

void RemoteCommandServer::sendJson(QWebSocket * client, QJsonObject const& object)
{
  if (!client)
    {
      return;
    }
  QJsonDocument doc {object};
  client->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void RemoteCommandServer::broadcastJson(QJsonObject const& object)
{
  for (int i = clients_.size() - 1; i >= 0; --i)
    {
      auto * client = clients_[i].data();
      if (!client)
        {
          clients_.removeAt(i);
          continue;
        }
      if (isAuthRequired() && !authenticatedClients_.contains(client))
        {
          continue;
        }
      sendJson(client, object);
    }
}

void RemoteCommandServer::sendReject(QWebSocket * client, QString const& commandId, QString const& status, QString const& reason)
{
  sendJson(client, makeRejectPayload(commandId, status, reason));
}

QJsonObject RemoteCommandServer::makeRejectPayload(QString const& commandId, QString const& status, QString const& reason) const
{
  return QJsonObject {
    {"event", QStringLiteral("command_ack")},
    {"command_id", commandId},
    {"status", status},
    {"reason", reason},
    {"server_now_ms", currentUtcMs()},
  };
}

void RemoteCommandServer::pruneSeenIds(qint64 nowUtcMs)
{
  constexpr qint64 kKeepMs = 3600000;
  for (auto it = seenCommandIds_.begin(); it != seenCommandIds_.end(); )
    {
      if (nowUtcMs - it.value() > kKeepMs)
        {
          it = seenCommandIds_.erase(it);
        }
      else
        {
          ++it;
        }
    }
}

void RemoteCommandServer::enqueueCommand(PendingCommand const& command)
{
  pending_.append(command);
  std::sort(pending_.begin(), pending_.end(), [] (PendingCommand const& a, PendingCommand const& b) {
      return a.scheduledUtcMs < b.scheduledUtcMs;
    });
  armScheduleTimer();
}

void RemoteCommandServer::armScheduleTimer()
{
  if (pending_.isEmpty())
    {
      scheduleTimer_.stop();
      return;
    }

  qint64 const nowUtcMs = currentUtcMs();
  qint64 delayMs = pending_.front().scheduledUtcMs - nowUtcMs;
  if (delayMs < 1)
    {
      delayMs = 1;
    }
  scheduleTimer_.start(static_cast<int>(qMin<qint64>(delayMs, std::numeric_limits<int>::max())));
}

void RemoteCommandServer::onTextMessageReceived(QString const& payload)
{
  auto * client = qobject_cast<QWebSocket *> (sender());
  if (!client)
    {
      return;
    }

  QJsonParseError parseError;
  auto doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
      sendReject(client, QString {}, QStringLiteral("rejected_invalid_json"), parseError.errorString());
      return;
    }

  auto const object = doc.object();
  if (isAuthRequired() && !isClientAuthenticated(client))
    {
      auto const type = object.value(QStringLiteral("type")).toString().trimmed().toLower();
      if (type == QStringLiteral("auth"))
        {
          auto const username = object.value(QStringLiteral("username")).toString().trimmed();
          auto const token = object.value(QStringLiteral("token")).toString().trimmed();
          if (token == authToken_ && authUserMatches(username))
            {
              markClientAuthenticated(client);
              sendJson(client, QJsonObject {
                          {"event", QStringLiteral("auth_ok")},
                          {"server_now_ms", currentUtcMs()},
                        });
              return;
            }

          sendJson(client, QJsonObject {
                      {"event", QStringLiteral("auth_failed")},
                      {"reason", QStringLiteral("invalid username/password")},
                      {"server_now_ms", currentUtcMs()},
                    });
          return;
        }

      sendReject(client, QString {}, QStringLiteral("rejected_not_authorized"), QStringLiteral("authentication required"));
      return;
    }

  auto result = isAuthRequired()
    ? processCommandObject(object, authToken_, authUser_)
    : processCommandObject(object);
  sendJson(client, result.payload);
}

RemoteCommandServer::CommandResult RemoteCommandServer::processCommandObject(QJsonObject const& object,
                                                                              QString const& tokenOverride,
                                                                              QString const& userOverride)
{
  CommandResult result;

  auto const commandType = object.value(QStringLiteral("type")).toString().trimmed().toLower();
  QString commandId = object.value(QStringLiteral("command_id")).toString().trimmed();
  if (commandId.isEmpty())
    {
      commandId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

  qint64 const nowUtcMs = currentUtcMs();
  pruneSeenIds(nowUtcMs);

  if (!authToken_.isEmpty())
    {
      QString token = tokenOverride.trimmed();
      if (token.isEmpty())
        {
          token = object.value(QStringLiteral("token")).toString().trimmed();
        }
      QString user = userOverride.trimmed();
      if (user.isEmpty())
        {
          user = object.value(QStringLiteral("username")).toString().trimmed();
        }
      if (token != authToken_)
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_not_authorized"), QStringLiteral("token mismatch"));
          return result;
        }
      if (!authUserMatches(user))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_not_authorized"), QStringLiteral("username mismatch"));
          return result;
        }
    }

  if (seenCommandIds_.contains(commandId))
    {
      result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_duplicate"), QStringLiteral("command_id already seen"));
      return result;
    }

  auto const state = runtimeState();
  auto clientSentMs = static_cast<qint64>(object.value(QStringLiteral("client_sent_ms")).toDouble(0.0));
  if (clientSentMs > 0 && (nowUtcMs - clientSentMs > maxCommandAgeMs_))
    {
      result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_stale"), QStringLiteral("command too old"));
      return result;
    }

  if (commandType == QStringLiteral("select_caller"))
    {
      auto targetCall = normalize_call(object.value(QStringLiteral("target_call")).toString());
      auto targetGrid = normalize_grid(object.value(QStringLiteral("target_grid")).toString());
      if (targetCall.isEmpty())
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("target_call is required"));
          return result;
        }

      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("mode is not FT2"));
          return result;
        }

      qint64 periodMs = state.periodMs;
      if (periodMs <= 0)
        {
          periodMs = 3750;
        }

      qint64 const nextSlot = nextSlotUtcMs(nowUtcMs, periodMs);
      qint64 scheduledSlot = nextSlot;
      QString ackStatus {QStringLiteral("accepted_current_slot")};
      if (nowUtcMs > (nextSlot - guardPreMs_))
        {
          scheduledSlot = nextSlot + periodMs;
          ackStatus = QStringLiteral("deferred_next_slot");
        }

      PendingCommand command;
      command.commandId = commandId;
      command.targetCall = targetCall;
      command.targetGrid = targetGrid;
      command.receivedUtcMs = nowUtcMs;
      command.scheduledUtcMs = scheduledSlot;

      seenCommandIds_.insert(command.commandId, nowUtcMs);
      enqueueCommand(command);

      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", command.commandId},
        {"type", QStringLiteral("select_caller")},
        {"status", ackStatus},
        {"scheduled_slot_utc_ms", command.scheduledUtcMs},
        {"slot_index", static_cast<double>(command.scheduledUtcMs / periodMs)},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_mode"))
    {
      auto mode = object.value(QStringLiteral("mode")).toString().trimmed().toUpper();
      if (mode.isEmpty())
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("mode is required"));
          return result;
        }
      static const QSet<QString> validModes {
        "FT2", "FT8", "FT4", "MSK144", "Q65", "JT65", "JT9", "JT4", "FST4", "FST4W", "WSPR", "ECHO", "FREQCAL"
      };
      if (!validModes.contains(mode))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("unsupported mode"));
          return result;
        }
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setModeRequested(commandId, mode);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_mode")},
        {"status", QStringLiteral("accepted_immediate")},
        {"mode", mode},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_band"))
    {
      auto band = object.value(QStringLiteral("band")).toString().trimmed();
      if (band.isEmpty())
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("band is required"));
          return result;
        }
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setBandRequested(commandId, band);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_band")},
        {"status", QStringLiteral("accepted_immediate")},
        {"band", band},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_rx_frequency"))
    {
      auto const value = object.value(QStringLiteral("rx_frequency_hz"));
      if (!value.isDouble())
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("rx_frequency_hz must be integer"));
          return result;
        }
      auto rxFrequency = qRound(value.toDouble());
      if (rxFrequency < 0 || rxFrequency > 5000)
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("rx_frequency_hz out of range 0..5000"));
          return result;
        }
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setRxFrequencyRequested(commandId, rxFrequency);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_rx_frequency")},
        {"status", QStringLiteral("accepted_immediate")},
        {"rx_frequency_hz", rxFrequency},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_tx_frequency"))
    {
      auto const value = object.value(QStringLiteral("tx_frequency_hz"));
      if (!value.isDouble())
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("tx_frequency_hz must be integer"));
          return result;
        }
      auto txFrequency = qRound(value.toDouble());
      if (txFrequency < 0 || txFrequency > 5000)
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("tx_frequency_hz out of range 0..5000"));
          return result;
        }
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setTxFrequencyRequested(commandId, txFrequency);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_tx_frequency")},
        {"status", QStringLiteral("accepted_immediate")},
        {"tx_frequency_hz", txFrequency},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_tx_enabled"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setTxEnabledRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_tx_enabled")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_auto_cq"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setAutoCqRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_auto_cq")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_async_l2"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("Async L2 is available only in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setAsyncL2Requested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_async_l2")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_dual_carrier"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("Dual Carrier is available only in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setDualCarrierRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_dual_carrier")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_alt_12"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode == QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("Alt 1/2 is unavailable in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setAlt12Requested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_alt_12")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_waterfall_enabled"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }

      bool const enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      setWaterfallEnabled(enabled, commandId);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_waterfall_enabled")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_unsupported"), QStringLiteral("unsupported command type"));
  return result;
}

bool RemoteCommandServer::isClientAuthenticated(QWebSocket * client) const
{
  if (!isAuthRequired())
    {
      return true;
    }
  return authenticatedClients_.contains(client);
}

void RemoteCommandServer::markClientAuthenticated(QWebSocket * client)
{
  if (client)
    {
      authenticatedClients_.insert(client);
    }
}

QString RemoteCommandServer::httpHeaderValue(HttpConnectionState const& state, QString const& key) const
{
  return state.headers.value(key.trimmed().toLower(), QString {});
}

QString RemoteCommandServer::httpAuthUser(HttpConnectionState const& state) const
{
  auto user = httpHeaderValue(state, QStringLiteral("x-auth-user")).trimmed();
  if (user.isEmpty())
    {
      auto const basic = httpHeaderValue(state, QStringLiteral("authorization"));
      if (basic.startsWith(QStringLiteral("Basic "), Qt::CaseInsensitive))
        {
          auto decoded = QByteArray::fromBase64(basic.mid(6).trimmed().toLatin1());
          int sep = decoded.indexOf(':');
          if (sep > 0)
            {
              user = QString::fromUtf8(decoded.left(sep)).trimmed();
            }
        }
    }
  return user;
}

QString RemoteCommandServer::httpBearerToken(HttpConnectionState const& state) const
{
  QString tokenFromHeader;
  auto const auth = httpHeaderValue(state, QStringLiteral("authorization"));
  if (auth.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive))
    {
      tokenFromHeader = auth.mid(7).trimmed();
    }
  if (tokenFromHeader.isEmpty())
    {
      tokenFromHeader = httpHeaderValue(state, QStringLiteral("x-auth-token")).trimmed();
    }
  return tokenFromHeader;
}

bool RemoteCommandServer::authUserMatches(QString const& candidate) const
{
  auto expected = authUser_.trimmed();
  if (expected.isEmpty())
    {
      expected = QStringLiteral("admin");
    }

  auto provided = candidate.trimmed();
  if (provided.isEmpty())
    {
      provided = QStringLiteral("admin");
    }
  return 0 == provided.compare(expected, Qt::CaseInsensitive);
}

bool RemoteCommandServer::isHttpAuthorized(HttpConnectionState const& state) const
{
  if (!isAuthRequired())
    {
      return true;
    }
  return httpBearerToken(state) == authToken_ && authUserMatches(httpAuthUser(state));
}

void RemoteCommandServer::sendHttpJson(QTcpSocket * socket, int statusCode, QJsonObject const& object)
{
  if (!socket)
    {
      return;
    }

  static const QHash<int, QByteArray> statusText {
    {200, "OK"},
    {400, "Bad Request"},
    {408, "Request Timeout"},
    {401, "Unauthorized"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {413, "Payload Too Large"},
    {500, "Internal Server Error"},
  };

  QByteArray const body = QJsonDocument {object}.toJson(QJsonDocument::Compact);
  QByteArray response;
  response += "HTTP/1.1 ";
  response += QByteArray::number(statusCode);
  response += " ";
  response += statusText.value(statusCode, "OK");
  response += "\r\n";
  response += "Content-Type: application/json; charset=utf-8\r\n";
  response += "Content-Length: ";
  response += QByteArray::number(body.size());
  response += "\r\n";
  response += "Connection: close\r\n";
  response += "Access-Control-Allow-Origin: *\r\n";
  response += "Access-Control-Allow-Headers: Content-Type, Authorization, X-Auth-Token, X-Auth-User\r\n";
  response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
  response += "\r\n";
  response += body;
  socket->write(response);
}

void RemoteCommandServer::sendHttpNoContent(QTcpSocket * socket, int statusCode, QByteArray const& extraHeaders)
{
  if (!socket)
    {
      return;
    }

  static const QHash<int, QByteArray> statusText {
    {204, "No Content"},
    {405, "Method Not Allowed"},
  };

  QByteArray response;
  response += "HTTP/1.1 ";
  response += QByteArray::number(statusCode);
  response += " ";
  response += statusText.value(statusCode, "No Content");
  response += "\r\n";
  response += "Content-Length: 0\r\n";
  response += "Connection: close\r\n";
  response += "Access-Control-Allow-Origin: *\r\n";
  response += "Access-Control-Allow-Headers: Content-Type, Authorization, X-Auth-Token, X-Auth-User\r\n";
  response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
  if (!extraHeaders.isEmpty())
    {
      response += extraHeaders;
      if (!extraHeaders.endsWith("\r\n"))
        {
          response += "\r\n";
        }
    }
  response += "\r\n";
  socket->write(response);
}

void RemoteCommandServer::onHttpNewConnection()
{
  if (!httpServer_)
    {
      return;
    }
  while (httpServer_->hasPendingConnections())
    {
      auto * socket = httpServer_->nextPendingConnection();
      if (!socket)
        {
          continue;
        }

      httpStates_.insert(socket, HttpConnectionState {});
      socket->setReadBufferSize(kMaxHttpHeaderBytes + kMaxHttpBodyBytes);
      QPointer<QTcpSocket> guarded_socket {socket};
      QTimer::singleShot(kHttpConnectionTimeoutMs, this, [this, guarded_socket] {
          if (!guarded_socket)
            {
              return;
            }
          auto it = httpStates_.find(guarded_socket.data());
          if (it == httpStates_.end())
            {
              return;
            }
          auto const& state = it.value();
          if (!state.headersParsed || state.buffer.size() < state.contentLength)
            {
              sendHttpJson(guarded_socket.data(), 408, QJsonObject {{"error", "request_timeout"}});
              guarded_socket->disconnectFromHost();
            }
        });
      connect(socket, &QTcpSocket::readyRead, this, &RemoteCommandServer::onHttpSocketReadyRead);
      connect(socket, &QTcpSocket::disconnected, this, &RemoteCommandServer::onHttpSocketDisconnected);
    }
}

void RemoteCommandServer::onHttpSocketDisconnected()
{
  auto * socket = qobject_cast<QTcpSocket *> (sender());
  if (!socket)
    {
      return;
    }
  httpStates_.remove(socket);
  socket->deleteLater();
}

void RemoteCommandServer::onHttpSocketReadyRead()
{
  auto * socket = qobject_cast<QTcpSocket *> (sender());
  if (!socket)
    {
      return;
    }

  auto it = httpStates_.find(socket);
  if (it == httpStates_.end())
    {
      return;
    }

  auto & state = it.value();
  state.buffer.append(socket->readAll());

  if (!state.headersParsed)
    {
      int headerEnd = state.buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0)
        {
          if (state.buffer.size() > kMaxHttpHeaderBytes)
            {
              sendHttpJson(socket, 413, QJsonObject {{"error", "headers_too_large"}});
              socket->disconnectFromHost();
            }
          return;
        }
      if (headerEnd + 4 > kMaxHttpHeaderBytes)
        {
          sendHttpJson(socket, 413, QJsonObject {{"error", "headers_too_large"}});
          socket->disconnectFromHost();
          return;
        }

      auto headerBytes = state.buffer.left(headerEnd);
      state.buffer.remove(0, headerEnd + 4);
      auto lines = headerBytes.split('\n');
      if (lines.isEmpty())
        {
          sendHttpJson(socket, 400, QJsonObject {{"error", "invalid_request_line"}});
          socket->disconnectFromHost();
          return;
        }

      auto requestLine = QString::fromUtf8(lines.takeFirst()).trimmed();
      auto requestParts = requestLine.split(' ', Qt::SkipEmptyParts);
      if (requestParts.size() < 2)
        {
          sendHttpJson(socket, 400, QJsonObject {{"error", "invalid_request_line"}});
          socket->disconnectFromHost();
          return;
        }

      state.method = requestParts[0].trimmed().toUpper();
      state.path = requestParts[1].trimmed();
      state.headers.clear();

      for (auto const& rawLine : lines)
        {
          auto line = QString::fromUtf8(rawLine).trimmed();
          if (line.isEmpty())
            {
              continue;
            }
          int sep = line.indexOf(':');
          if (sep <= 0)
            {
              continue;
            }
          auto key = line.left(sep).trimmed().toLower();
          auto value = line.mid(sep + 1).trimmed();
          state.headers.insert(key, value);
        }

      bool lenOk {true};
      state.contentLength = httpHeaderValue(state, QStringLiteral("content-length")).toLongLong(&lenOk);
      if (!lenOk || state.contentLength < 0)
        {
          state.contentLength = 0;
        }
      if (state.contentLength > kMaxHttpBodyBytes)
        {
          sendHttpJson(socket, 413, QJsonObject {{"error", "payload_too_large"}});
          socket->disconnectFromHost();
          return;
        }
      state.headersParsed = true;
    }

  if (state.buffer.size() < state.contentLength)
    {
      return;
    }

  auto body = state.buffer.left(state.contentLength);
  auto route = state.path.section('?', 0, 0);

  if (state.method == QStringLiteral("OPTIONS"))
    {
      sendHttpNoContent(socket, 204);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && (route == QStringLiteral("/") || route == QStringLiteral("/index.html")))
    {
      QByteArray response;
      auto const body = dashboard_html();
      response += "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: text/html; charset=utf-8\r\n";
      response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      socket->write(response);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/app.css"))
    {
      QByteArray response;
      auto const body = dashboard_css();
      response += "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: text/css; charset=utf-8\r\n";
      response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      socket->write(response);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/app.js"))
    {
      QByteArray response;
      auto const body = dashboard_js();
      response += "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: application/javascript; charset=utf-8\r\n";
      response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      socket->write(response);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/manifest.webmanifest"))
    {
      QByteArray response;
      auto const body = dashboard_manifest();
      response += "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: application/manifest+json; charset=utf-8\r\n";
      response += "Cache-Control: no-cache\r\n";
      response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      socket->write(response);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/sw.js"))
    {
      QByteArray response;
      auto const body = dashboard_service_worker_js();
      response += "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: application/javascript; charset=utf-8\r\n";
      response += "Cache-Control: no-cache\r\n";
      response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      socket->write(response);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET")
      && (route == QStringLiteral("/icon-192.png")
          || route == QStringLiteral("/icon-512.png")))
    {
      int const iconSize = (route == QStringLiteral("/icon-512.png")) ? 512 : 192;
      QByteArray response;
      auto const body = dashboard_icon_png(iconSize);
      response += "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: image/png\r\n";
      response += "Cache-Control: public, max-age=86400\r\n";
      response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      socket->write(response);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/api/v1/health"))
    {
      auto const rt = runtimeState();
      auto const authorized = isHttpAuthorized(state);
      QJsonObject health {
        {"status", QStringLiteral("ok")},
        {"authorized", authorized},
        {"requires_auth", isAuthRequired()},
        {"auth_user", authUser_},
        {"ws_port", static_cast<int>(wsPort_)},
        {"http_port", static_cast<int>(httpPort_)},
        {"waterfall_enabled", waterfallEnabled_},
        {"server_now_ms", rt.nowUtcMs},
      };
      if (authorized || !isAuthRequired())
        {
          health.insert(QStringLiteral("mode"), rt.mode);
          health.insert(QStringLiteral("band"), rt.band);
          health.insert(QStringLiteral("dial_frequency_hz"), static_cast<double>(rt.dialFrequencyHz));
          health.insert(QStringLiteral("rx_frequency_hz"), rt.rxFrequencyHz);
          health.insert(QStringLiteral("tx_frequency_hz"), rt.txFrequencyHz);
          health.insert(QStringLiteral("period_ms"), static_cast<double>(rt.periodMs));
          health.insert(QStringLiteral("tx_enabled"), rt.txEnabled);
          health.insert(QStringLiteral("auto_cq_enabled"), rt.autoCqEnabled);
          health.insert(QStringLiteral("async_l2_enabled"), rt.asyncL2Enabled);
          health.insert(QStringLiteral("dual_carrier_enabled"), rt.dualCarrierEnabled);
          health.insert(QStringLiteral("alt_12_enabled"), rt.alt12Enabled);
          health.insert(QStringLiteral("monitoring"), rt.monitoring);
          health.insert(QStringLiteral("transmitting"), rt.transmitting);
          health.insert(QStringLiteral("my_call"), rt.myCall);
          health.insert(QStringLiteral("dx_call"), rt.dxCall);
        }

      sendHttpJson(socket, 200, health);
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/api/v1/state"))
    {
      if (!isHttpAuthorized(state))
        {
          sendHttpJson(socket, 401, QJsonObject {
                        {"error", QStringLiteral("not_authorized")},
                        {"requires_auth", isAuthRequired()},
                      });
          socket->disconnectFromHost();
          return;
        }

      auto const rt = runtimeState();
      qint64 const periodMs = qMax<qint64>(1, rt.periodMs);
      qint64 const nextSlot = nextSlotUtcMs(rt.nowUtcMs, periodMs);

      QJsonArray recent;
      for (auto const& line : recentBandActivity_)
        {
          recent.append(line);
        }

      sendHttpJson(socket, 200, QJsonObject {
                      {"mode", rt.mode},
                      {"band", rt.band},
                      {"dial_frequency_hz", static_cast<double>(rt.dialFrequencyHz)},
                      {"rx_frequency_hz", rt.rxFrequencyHz},
                      {"tx_frequency_hz", rt.txFrequencyHz},
                      {"period_ms", static_cast<double>(periodMs)},
                      {"tx_enabled", rt.txEnabled},
                      {"auto_cq_enabled", rt.autoCqEnabled},
                      {"async_l2_enabled", rt.asyncL2Enabled},
                      {"dual_carrier_enabled", rt.dualCarrierEnabled},
                      {"alt_12_enabled", rt.alt12Enabled},
                      {"monitoring", rt.monitoring},
                      {"transmitting", rt.transmitting},
                      {"my_call", rt.myCall},
                      {"dx_call", rt.dxCall},
                      {"next_slot_utc_ms", static_cast<double>(nextSlot)},
                      {"pending_commands", pending_.size()},
                      {"recent_band_activity", recent},
                      {"waterfall_enabled", waterfallEnabled_},
                      {"requires_auth", isAuthRequired()},
                      {"server_now_ms", rt.nowUtcMs},
                    });
      socket->disconnectFromHost();
      return;
    }

  if (state.method == QStringLiteral("GET") && route == QStringLiteral("/api/v1/waterfall/latest"))
    {
      if (!isHttpAuthorized(state))
        {
          sendHttpJson(socket, 401, QJsonObject {
                        {"error", QStringLiteral("not_authorized")},
                        {"requires_auth", isAuthRequired()},
                      });
          socket->disconnectFromHost();
          return;
        }

      QJsonObject payload {
        {"waterfall_enabled", waterfallEnabled_},
        {"server_now_ms", currentUtcMs()},
      };
      if (!lastWaterfallRowB64_.isEmpty())
        {
          payload.insert(QStringLiteral("row_b64"), QString::fromLatin1(lastWaterfallRowB64_));
          payload.insert(QStringLiteral("width"), lastWaterfallWidth_);
          payload.insert(QStringLiteral("start_frequency_hz"), lastWaterfallStartFrequencyHz_);
          payload.insert(QStringLiteral("span_hz"), lastWaterfallSpanHz_);
          payload.insert(QStringLiteral("rx_frequency_hz"), lastWaterfallRxFrequencyHz_);
          payload.insert(QStringLiteral("tx_frequency_hz"), lastWaterfallTxFrequencyHz_);
          payload.insert(QStringLiteral("mode"), lastWaterfallMode_);
        }

      sendHttpJson(socket, 200, payload);
      socket->disconnectFromHost();
      return;
    }

  if (route != QStringLiteral("/api/v1/commands"))
    {
      sendHttpJson(socket, 404, QJsonObject {{"error", "not_found"}});
      socket->disconnectFromHost();
      return;
    }

  if (state.method != QStringLiteral("POST"))
    {
      sendHttpNoContent(socket, 405, QByteArray {"Allow: POST, OPTIONS\r\n"});
      socket->disconnectFromHost();
      return;
    }

  QJsonParseError parseError;
  auto doc = QJsonDocument::fromJson(body, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
      sendHttpJson(socket, 400, QJsonObject {
                    {"error", "invalid_json"},
                    {"reason", parseError.errorString()},
                  });
      socket->disconnectFromHost();
      return;
    }

  auto const tokenFromHeader = httpBearerToken(state);
  auto const userFromHeader = httpAuthUser(state);

  auto result = processCommandObject(doc.object(), tokenFromHeader, userFromHeader);
  sendHttpJson(socket, 200, result.payload);
  socket->disconnectFromHost();
}

void RemoteCommandServer::onScheduleTimeout()
{
  if (pending_.isEmpty())
    {
      return;
    }

  qint64 const nowUtcMs = currentUtcMs();
  QVector<PendingCommand> due;
  QVector<PendingCommand> remaining;
  due.reserve(pending_.size());
  remaining.reserve(pending_.size());

  for (auto const& cmd : pending_)
    {
      if (cmd.scheduledUtcMs <= nowUtcMs + 25)
        {
          due.append(cmd);
        }
      else
        {
          remaining.append(cmd);
        }
    }
  pending_ = remaining;

  for (auto const& cmd : due)
    {
      QJsonObject execEvent {
        {"event", QStringLiteral("command_execute")},
        {"command_id", cmd.commandId},
        {"type", QStringLiteral("select_caller")},
        {"scheduled_slot_utc_ms", cmd.scheduledUtcMs},
        {"server_now_ms", nowUtcMs},
      };
      broadcastJson(execEvent);
      Q_EMIT selectCallerDue(cmd.commandId, cmd.targetCall, cmd.targetGrid);
    }

  armScheduleTimer();
}

void RemoteCommandServer::onTelemetryTick()
{
  if (clients_.isEmpty())
    {
      return;
    }
  auto const state = runtimeState();
  qint64 const periodMs = qMax<qint64>(1, state.periodMs);
  qint64 const nextSlot = nextSlotUtcMs(state.nowUtcMs, periodMs);

  QJsonObject telemetry {
    {"event", QStringLiteral("telemetry")},
    {"mode", state.mode},
    {"band", state.band},
    {"dial_frequency_hz", static_cast<double>(state.dialFrequencyHz)},
    {"rx_frequency_hz", state.rxFrequencyHz},
    {"tx_frequency_hz", state.txFrequencyHz},
    {"period_ms", static_cast<double>(periodMs)},
    {"tx_enabled", state.txEnabled},
    {"auto_cq_enabled", state.autoCqEnabled},
    {"async_l2_enabled", state.asyncL2Enabled},
    {"dual_carrier_enabled", state.dualCarrierEnabled},
    {"alt_12_enabled", state.alt12Enabled},
    {"monitoring", state.monitoring},
    {"transmitting", state.transmitting},
    {"my_call", state.myCall},
    {"dx_call", state.dxCall},
    {"next_slot_utc_ms", static_cast<double>(nextSlot)},
    {"pending_commands", pending_.size()},
    {"waterfall_enabled", waterfallEnabled_},
    {"connected_clients", isAuthRequired() ? authenticatedClients_.size() : clients_.size()},
    {"server_now_ms", state.nowUtcMs},
  };
  broadcastJson(telemetry);
}
