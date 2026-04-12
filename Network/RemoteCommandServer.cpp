// -*- Mode: C++ -*-
#include "RemoteCommandServer.hpp"

#include <QDateTime>
#include <QBuffer>
#include <QHostInfo>
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
#include "revision_utils.hpp"

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

bool is_loopback_origin_host(QString const& host)
{
  auto normalized = host.trimmed().toLower();
  return normalized == QStringLiteral("localhost")
      || normalized == QStringLiteral("127.0.0.1")
      || normalized == QStringLiteral("::1")
      || normalized == QStringLiteral("[::1]");
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
      <h2 id="login_title">Remote Access</h2>
      <p id="login_caption">Insert username and password to unlock dashboard</p>
      <form id="login_form" autocomplete="on">
        <label id="lbl_login_user" for="login_user">Username</label>
        <input id="login_user" name="username" type="text" autocomplete="username" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="admin" />
        <label id="lbl_login_token" for="login_token">Password</label>
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
        <h1 id="app_title">Decodium Remote Console</h1>
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
          <div id="lbl_dial" class="lbl">Dial Frequency</div>
          <div id="st_dial" class="dial">-</div>
        </div>
        <div class="kv-grid">
          <div class="kv"><span id="lbl_mode">Mode</span><strong id="st_mode">-</strong></div>
          <div class="kv"><span id="lbl_band">Band</span><strong id="st_band">-</strong></div>
          <div class="kv"><span id="lbl_rx">Rx</span><strong id="st_rx">-</strong></div>
          <div class="kv"><span id="lbl_tx">Tx</span><strong id="st_tx">-</strong></div>
          <div class="kv"><span id="lbl_mycall">My Call</span><strong id="st_mycall">-</strong></div>
          <div class="kv"><span id="lbl_dxcall">DX Call</span><strong id="st_dxcall">-</strong></div>
        <div class="kv"><span id="lbl_tx_enabled">TX Enabled</span><strong id="st_txen">-</strong></div>
        <div class="kv"><span id="lbl_transmitting">Transmitting</span><strong id="st_trx">-</strong></div>
        <div class="kv"><span id="lbl_monitoring">Monitoring</span><strong id="st_mon">-</strong></div>
        </div>
      </div>

      <div class="mode-row">
        <div id="ttl_mode" class="group-title">Mode</div>
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
        <div id="ttl_band" class="group-title">Band</div>
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
          <label for="rxfreq"><span id="lbl_rxtx_freq">Rx / Tx Frequency (Hz)</span></label>
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
          <label for="mode_preset_select"><span id="lbl_mode_preset">Mode Preset (Rx/Tx)</span></label>
          <div class="inline mode-preset-inline">
            <select id="mode_preset_select"></select>
            <button id="btn_apply_mode_preset" type="button">Apply Preset</button>
          </div>
          <div class="inline mode-preset-actions">
            <button id="btn_save_mode_preset" type="button">Save Current</button>
          </div>
        </div>
        <div class="quick-item">
          <label><span id="lbl_tx_control">TX Control</span></label>
          <div class="inline tx-inline">
            <button id="btn_tx_on" class="ok">Enable TX</button>
            <button id="btn_auto_cq" type="button">Auto CQ</button>
            <button id="btn_auto_spot" type="button">AutoSpot</button>
            <button id="btn_monitor" type="button">Monitoring</button>
            <button id="btn_tx_off" class="warn">Disable TX</button>
          </div>
        </div>
      </div>

      <div class="mode-row emission-row">
        <div id="ttl_emission" class="group-title">Emission Options</div>
        <div id="ft2_controls" class="ft2-stack hidden">
          <label class="ft2-check">
            <input id="chk_manual_tx" type="checkbox" />
            <span id="lbl_manual_tx">Manual TX</span>
          </label>
          <label class="ft2-check">
            <input id="chk_speedy" type="checkbox" />
            <span id="lbl_speedy">Speedy</span>
          </label>
          <label class="ft2-check">
            <input id="chk_digital_morse" type="checkbox" />
            <span id="lbl_digital_morse">D-CW</span>
          </label>
          <div id="async_card" class="async-card" role="status" aria-live="polite">
            <div class="async-card-title">ASYNC</div>
            <div class="async-card-wave" aria-hidden="true">
              <span></span><span></span><span></span>
            </div>
            <div id="async_card_meta" class="async-card-meta">--- dB</div>
          </div>
          <label class="ft2-qso-label" for="ft2_qso_count" id="lbl_qso">QSO:</label>
          <button id="btn_quick_qso" type="button">Quick QSO</button>
          <select id="ft2_qso_count">
            <option value="2">2 msg</option>
            <option value="3">3 msg</option>
            <option value="5">5 msg</option>
          </select>
          <div class="btn-grid emission-grid">
            <button id="btn_async_l2" type="button">Async L2</button>
            <button id="btn_dual_carrier" type="button">Dual Carrier</button>
          </div>
        </div>
        <div id="alt_controls" class="btn-grid emission-grid hidden">
          <button id="btn_alt_12" type="button">Alt 1/2</button>
        </div>
      </div>
    </section>

    <section class="panel waterfall-panel">
      <div class="panel-title-row">
        <div id="ttl_waterfall" class="panel-title">Waterfall (experimental)</div>
        <button id="btn_wf_toggle" type="button">Waterfall OFF</button>
      </div>
      <div class="waterfall-wrap">
        <canvas id="waterfall_canvas" width="960" height="260"></canvas>
        <canvas id="waterfall_overlay" width="960" height="260"></canvas>
      </div>
      <div id="waterfall_hint" class="waterfall-hint">Tap/click on waterfall to set Rx frequency.</div>
    </section>

    <section class="split">
      <article class="panel activity">
        <div class="panel-title-row">
          <div id="ttl_activity" class="panel-title">RX/TX Activity</div>
          <div class="activity-controls">
            <button id="btn_filter_cq" type="button">Hide CQ</button>
            <button id="btn_filter_73" type="button">Hide 73</button>
            <button id="btn_activity_pause" type="button">Pausa</button>
            <button id="btn_activity_clear" type="button">Pulisci</button>
          </div>
        </div>
        <table id="activity_table">
          <thead>
            <tr><th id="th_utc">UTC</th><th id="th_db">dB</th><th id="th_dt">DT</th><th id="th_freq">Freq</th><th id="th_msg">Message</th></tr>
          </thead>
          <tbody id="activity_body"></tbody>
        </table>
      </article>

      <article class="panel control">
        <div id="ttl_remote_action" class="panel-title">Remote Action</div>
        <label><span id="lbl_call">Call</span>
          <input id="caller_call" type="text" placeholder="K1ABC" />
        </label>
        <label><span id="lbl_grid">Grid</span>
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
.activity-controls{display:flex;gap:6px;flex-wrap:wrap}
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
.rxtx-actions button{width:100%;min-height:36px}
.mode-preset-inline{grid-template-columns:minmax(120px,1fr) auto}
.mode-preset-actions{grid-template-columns:1fr;margin-top:6px}
.mode-preset-actions button{width:100%;min-height:36px}
.tx-inline{grid-template-columns:repeat(auto-fit,minmax(92px,1fr))}
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
.ft2-stack{
  display:grid;
  gap:10px;
  max-width:340px;
}
.ft2-check{
  display:flex;
  align-items:center;
  gap:10px;
  font-size:1rem;
  color:var(--text);
}
.ft2-check input{
  width:18px;
  height:18px;
  accent-color:#2ce59b;
}
.ft2-qso-label{
  color:var(--text);
  font-size:1rem;
}
#btn_quick_qso,
#ft2_qso_count{
  width:100%;
}
#ft2_qso_count{
  background:#0d1828;
  color:var(--text);
  border:1px solid var(--line);
  border-radius:12px;
  padding:12px 14px;
  font-size:1rem;
}
.async-card{
  width:176px;
  min-height:102px;
  padding:8px 10px 10px;
  border-radius:14px;
  border:1px solid #334561;
  background:linear-gradient(180deg,#151d31 0%,#11182a 100%);
  box-shadow:inset 0 0 0 1px rgba(255,255,255,.03);
}
.async-card.active{
  border-color:#00d27c;
  box-shadow:0 0 0 1px rgba(0,210,124,.3), 0 14px 30px rgba(0,0,0,.25);
}
.async-card-title{
  color:#00e98a;
  font-weight:800;
  letter-spacing:.12em;
  font-size:.82rem;
  margin-bottom:10px;
}
.async-card-wave{
  height:44px;
  display:flex;
  align-items:flex-end;
  gap:10px;
  overflow:hidden;
}
.async-card-wave span{
  flex:1 1 0;
  min-width:0;
  border-radius:999px;
  background:linear-gradient(180deg,#15f0a0 0%,#00c676 100%);
  animation:asyncPulse 1.35s ease-in-out infinite;
  opacity:.92;
}
.async-card-wave span:nth-child(1){height:22px;animation-delay:0s}
.async-card-wave span:nth-child(2){height:36px;animation-delay:.18s}
.async-card-wave span:nth-child(3){height:28px;animation-delay:.32s}
.async-card.idle .async-card-wave span{
  animation-play-state:paused;
  opacity:.35;
}
.async-card-meta{
  margin-top:10px;
  color:#dfe8ff;
  font-size:.86rem;
}
@keyframes asyncPulse{
  0%,100%{transform:translateY(0) scaleY(.82)}
  50%{transform:translateY(-2px) scaleY(1.08)}
}
@media (max-width:1220px){
  .freq-row{grid-template-columns:1fr}
  .kv-grid{grid-template-columns:repeat(3,minmax(0,1fr))}
  .btn-grid{grid-template-columns:repeat(8,minmax(0,1fr))}
  .emission-grid{grid-template-columns:repeat(2,minmax(0,1fr));max-width:none}
  #alt_controls.emission-grid{grid-template-columns:1fr}
  .ft2-stack{max-width:none}
  .split{grid-template-columns:1fr}
}
@media (max-width:760px){
  .top{align-items:flex-start}
  .top-actions{align-self:flex-end}
  .kv-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .btn-grid{grid-template-columns:repeat(5,minmax(0,1fr))}
  .emission-grid{grid-template-columns:repeat(2,minmax(0,1fr));max-width:none}
  #alt_controls.emission-grid{grid-template-columns:1fr}
  .async-card{width:100%}
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
  return QString::fromLatin1(
R"FT2JS((() => {
  const el = (id) => document.getElementById(id);
  const conn = el('conn');
  const trxLive = el('trx_live');
  const trxPeer = el('trx_peer');
  const actionStatus = el('action_status');
  const activityBody = el('activity_body');
  const btnFilterCq = el('btn_filter_cq');
  const btnFilter73 = el('btn_filter_73');
  const btnActivityPause = el('btn_activity_pause');
  const btnActivityClear = el('btn_activity_clear');
  const btnWfToggle = el('btn_wf_toggle');
  const btnAutoCQ = el('btn_auto_cq');
  const btnAutoSpot = el('btn_auto_spot');
  const btnMonitor = el('btn_monitor');
  const btnTxOn = el('btn_tx_on');
  const btnTxOff = el('btn_tx_off');
  const btnTxFreq = el('btn_txfreq');
  const btnSetRxTx = el('btn_set_rxtx');
  const rxFreqInput = el('rxfreq');
  const txFreqInput = el('txfreq');
  const modePresetSelect = el('mode_preset_select');
  const btnApplyModePreset = el('btn_apply_mode_preset');
  const btnSaveModePreset = el('btn_save_mode_preset');
  const btnAsyncL2 = el('btn_async_l2');
  const btnDualCarrier = el('btn_dual_carrier');
  const btnAlt12 = el('btn_alt_12');
  const chkManualTx = el('chk_manual_tx');
  const chkSpeedy = el('chk_speedy');
  const chkDigitalMorse = el('chk_digital_morse');
  const btnQuickQso = el('btn_quick_qso');
  const ft2QsoCount = el('ft2_qso_count');
  const asyncCard = el('async_card');
  const asyncCardMeta = el('async_card_meta');
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
  const appleWebAppTitle = document.querySelector('meta[name="apple-mobile-web-app-title"]');
  const UI_TEXT_EN = {
    lang: 'en',
    pageTitle: 'Decodium Remote Console',
    shortTitle: 'Decodium Remote',
    remoteAccess: 'Remote Access',
    loginCaptionAuth: 'Insert username and password to unlock dashboard',
    loginCaptionNoAuth: 'Authentication not required',
    username: 'Username',
    password: 'Password',
    accessPassword: 'Access password',
    unlock: 'Unlock',
    tokenMinHint: 'The access password must be at least 12 characters.',
    installApp: 'Install App',
    dialFrequency: 'Dial Frequency',
    mode: 'Mode',
    band: 'Band',
    rx: 'Rx',
    tx: 'Tx',
    myCall: 'My Call',
    dxCall: 'DX Call',
    txEnabled: 'TX Enabled',
    transmitting: 'Transmitting',
    monitoring: 'Monitoring',
    rxTxFrequency: 'Rx / Tx Frequency (Hz)',
    setRxTx: 'Set Rx+Tx',
    setRx: 'Set Rx',
    setTx: 'Set Tx',
    modePreset: 'Mode Preset (Rx/Tx)',
    applyPreset: 'Apply Preset',
    saveCurrent: 'Save Current',
    txControl: 'TX Control',
    enableTx: 'Enable TX',
    disableTx: 'Disable TX',
    autoCq: 'Auto CQ',
    autoSpot: 'AutoSpot',
    emissionOptions: 'Emission Options',
    manualTx: 'Manual TX',
    speedy: 'Speedy',
    digitalMorse: 'D-CW',
    qso: 'QSO:',
    quickQso: 'Quick QSO',
    asyncL2: 'Async L2',
    dualCarrier: 'Dual Carrier',
    alt12: 'Alt 1/2',
    waterfallTitle: 'Waterfall (experimental)',
    waterfallOn: 'Waterfall ON',
    waterfallOff: 'Waterfall OFF',
    waterfallHint: 'Tap/click on waterfall to set Rx frequency.',
    activityTitle: 'RX/TX Activity',
    hideCq: 'Hide CQ',
    cqHidden: 'CQ hidden',
    hide73: 'Hide 73',
    d73Hidden: '73 hidden',
    pause: 'Pause',
    resume: 'Resume',
    clear: 'Clear',
    remoteAction: 'Remote Action',
    call: 'Call',
    grid: 'Grid',
    frequency: 'Freq',
    message: 'Message',
    callStation: 'Call Station (slot-aware)',
    connected: 'connected',
    connectedPoll: 'connected (poll)',
    disconnected: 'disconnected',
    authenticating: 'authenticating...',
    locked: 'locked',
    wsError: 'ws error',
    authFailed: 'auth failed',
    invalidCredentials: 'Invalid username or password',
    passwordRequired: 'Password required',
    yes: 'yes',
    no: 'no',
    monitoringOn: 'Monitoring ON',
    monitoringOff: 'Monitoring OFF',
    autoCqOn: 'Auto CQ ON',
    autoCqOff: 'Auto CQ OFF',
    autoSpotOn: 'AutoSpot ON',
    autoSpotOff: 'AutoSpot OFF',
    quickQsoOn: 'Quick QSO ON',
    quickQsoOff: 'Quick QSO',
    asyncL2On: 'Async L2 ON',
    asyncL2Off: 'Async L2',
    dualCarrierOn: 'Dual Carrier ON',
    dualCarrierOff: 'Dual Carrier',
    alt12On: 'Alt 1/2 ON',
    alt12Off: 'Alt 1/2',
    ft2Only: 'FT2 only',
    txRunning: 'TX running',
    rxIdle: 'RX idle',
    asyncReady: 'Async ready',
    txOnAir: 'TX ON AIR',
    rxTxArm: 'RX (TX ARM)',
    rxOnly: 'RX',
    dxPrefix: 'DX',
    txWith: 'TX with',
    lastTx: 'Last TX',
    txStarted: 'TX STARTED',
    txEndedRx: 'TX ENDED (RX)',
    ready: 'Ready',
    commandFailed: 'Command failed',
    bandActivityPaused: 'Band activity paused',
    bandActivityResumed: 'Band activity resumed',
    bandActivityCleared: 'Band activity cleared',
    installHintPrompt: 'Tap Install App to add Decodium to your home screen.',
    installHintIos: 'iPhone/iPad: Safari -> Share -> Add to Home Screen.',
    installHintInsecure: 'For full app install on Android, use HTTPS or localhost.',
    installHintAndroid: 'Android: Browser menu -> Add to Home screen.',
    initError: 'init error',
    backendUnreachable: 'backend unreachable',
    msgSuffix: 'msg',
    noPresetSaved: 'no preset saved for',
    invalidPresetValues: 'invalid preset values for',
    presetSaved: 'preset saved',
    presetLabel: 'preset',
    rxTxSet: 'Rx/Tx set'
  };
  const UI_TEXT_IT = {
    lang: 'it',
    pageTitle: 'Console Remota Decodium',
    shortTitle: 'Decodium Remote',
    remoteAccess: 'Accesso Remoto',
    loginCaptionAuth: 'Inserisci username e password per sbloccare la dashboard',
    loginCaptionNoAuth: 'Autenticazione non richiesta',
    username: 'Nome utente',
    password: 'Password',
    accessPassword: 'Password di accesso',
    unlock: 'Sblocca',
    tokenMinHint: 'La password di accesso deve avere almeno 12 caratteri.',
    installApp: 'Installa App',
    dialFrequency: 'Frequenza Dial',
    mode: 'Modo',
    band: 'Banda',
    rx: 'Rx',
    tx: 'Tx',
    myCall: 'Mio Call',
    dxCall: 'Call DX',
    txEnabled: 'TX Abilitato',
    transmitting: 'In Trasmissione',
    monitoring: 'Monitoring',
    rxTxFrequency: 'Frequenza Rx / Tx (Hz)',
    setRxTx: 'Imposta Rx+Tx',
    setRx: 'Imposta Rx',
    setTx: 'Imposta Tx',
    modePreset: 'Preset Modo (Rx/Tx)',
    applyPreset: 'Applica Preset',
    saveCurrent: 'Salva Attuale',
    txControl: 'Controllo TX',
    enableTx: 'Abilita TX',
    disableTx: 'Disabilita TX',
    autoCq: 'Auto CQ',
    autoSpot: 'AutoSpot',
    emissionOptions: 'Opzioni Emissione',
    manualTx: 'TX Manuale',
    speedy: 'Speedy',
    digitalMorse: 'D-CW',
    qso: 'QSO:',
    quickQso: 'QSO Rapido',
    asyncL2: 'Async L2',
    dualCarrier: 'Dual Carrier',
    alt12: 'Alt 1/2',
    waterfallTitle: 'Waterfall (sperimentale)',
    waterfallOn: 'Waterfall ON',
    waterfallOff: 'Waterfall OFF',
    waterfallHint: 'Tocca/clicca sul waterfall per impostare la frequenza Rx.',
    activityTitle: 'Attivita RX/TX',
    hideCq: 'Nascondi CQ',
    cqHidden: 'CQ nascosti',
    hide73: 'Nascondi 73',
    d73Hidden: '73 nascosti',
    pause: 'Pausa',
    resume: 'Riprendi',
    clear: 'Pulisci',
    remoteAction: 'Azione Remota',
    call: 'Call',
    grid: 'Grid',
    frequency: 'Freq',
    message: 'Messaggio',
    callStation: 'Chiama Stazione (slot-aware)',
    connected: 'connesso',
    connectedPoll: 'connesso (poll)',
    disconnected: 'disconnesso',
    authenticating: 'autenticazione...',
    locked: 'bloccato',
    wsError: 'errore ws',
    authFailed: 'autenticazione fallita',
    invalidCredentials: 'Username o password non validi',
    passwordRequired: 'Password richiesta',
    yes: 'si',
    no: 'no',
    monitoringOn: 'Monitoring ON',
    monitoringOff: 'Monitoring OFF',
    autoCqOn: 'Auto CQ ON',
    autoCqOff: 'Auto CQ OFF',
    autoSpotOn: 'AutoSpot ON',
    autoSpotOff: 'AutoSpot OFF',
    quickQsoOn: 'QSO Rapido ON',
    quickQsoOff: 'QSO Rapido',
    asyncL2On: 'Async L2 ON',
    asyncL2Off: 'Async L2',
    dualCarrierOn: 'Dual Carrier ON',
    dualCarrierOff: 'Dual Carrier',
    alt12On: 'Alt 1/2 ON',
    alt12Off: 'Alt 1/2',
    ft2Only: 'Solo FT2',
    txRunning: 'TX in corso',
    rxIdle: 'RX inattivo',
    asyncReady: 'Async pronto',
    txOnAir: 'TX ON AIR',
    rxTxArm: 'RX (TX ARM)',
    rxOnly: 'RX',
    dxPrefix: 'DX',
    txWith: 'TX con',
    lastTx: 'Ultimo TX',
    txStarted: 'TX AVVIATO',
    txEndedRx: 'TX TERMINATO (RX)',
    ready: 'Pronto',
    commandFailed: 'Comando fallito',
    bandActivityPaused: 'Attivita banda in pausa',
    bandActivityResumed: 'Attivita banda ripresa',
    bandActivityCleared: 'Attivita banda pulita',
    installHintPrompt: 'Tocca Installa App per aggiungere Decodium alla schermata Home.',
    installHintIos: 'iPhone/iPad: Safari -> Condividi -> Aggiungi a Home.',
    installHintInsecure: 'Per installare l app completa su Android usa HTTPS o localhost.',
    installHintAndroid: 'Android: menu del browser -> Aggiungi a schermata Home.',
    initError: 'errore init',
    backendUnreachable: 'backend non raggiungibile',
    msgSuffix: 'msg',
    noPresetSaved: 'nessun preset salvato per',
    invalidPresetValues: 'valori preset non validi per',
    presetSaved: 'preset salvato',
    presetLabel: 'preset',
    rxTxSet: 'Rx/Tx impostati'
  };
  const buildUiText = (lang, overrides = {}) => ({...UI_TEXT_EN, lang, ...overrides});
  const UI_TEXT_EN_GB = buildUiText('en-GB', {});
  const UI_TEXT_CA = buildUiText('ca', {
    pageTitle: 'Consola remota de Decodium',
    remoteAccess: 'Acces remot',
    loginCaptionAuth: 'Introdueix usuari i contrasenya per desbloquejar el tauler',
    loginCaptionNoAuth: 'Autenticacio no requerida',
    username: 'Usuari',
    password: 'Contrasenya',
    accessPassword: "Contrasenya d'acces",
    unlock: 'Desbloqueja',
    tokenMinHint: "La contrasenya d'acces ha de tenir com a minim 12 caracters.",
    installApp: "Instal la l'app",
    dialFrequency: 'Frequencia dial',
    band: 'Banda',
    myCall: 'El meu indicatiu',
    dxCall: 'Indicatiu DX',
    txEnabled: 'TX habilitat',
    transmitting: 'En transmissio',
    monitoring: 'Monitoratge',
    rxTxFrequency: 'Frequencia Rx / Tx (Hz)',
    setRxTx: 'Defineix Rx+Tx',
    setRx: 'Defineix Rx',
    setTx: 'Defineix Tx',
    modePreset: 'Preset de mode (Rx/Tx)',
    applyPreset: 'Aplica preset',
    saveCurrent: "Desa l'actual",
    txControl: 'Control TX',
    enableTx: 'Habilita TX',
    disableTx: 'Deshabilita TX',
    emissionOptions: "Opcions d'emissio",
    manualTx: 'TX manual',
    waterfallTitle: 'Waterfall (experimental)',
    waterfallHint: 'Toca o clica al waterfall per definir la frequencia Rx.',
    activityTitle: 'Activitat RX/TX',
    hideCq: 'Amaga CQ',
    cqHidden: 'CQ amagats',
    hide73: 'Amaga 73',
    d73Hidden: '73 amagats',
    pause: 'Pausa',
    resume: 'Continua',
    clear: 'Neteja',
    remoteAction: 'Accio remota',
    callStation: 'Crida estacio (slot-aware)',
    connected: 'connectat',
    connectedPoll: 'connectat (poll)',
    disconnected: 'desconnectat',
    authenticating: 'autenticant...',
    locked: 'bloquejat',
    wsError: 'error ws',
    authFailed: "autenticacio fallida",
    invalidCredentials: 'Usuari o contrasenya no valids',
    passwordRequired: 'Cal contrasenya',
    yes: 'si',
    no: 'no',
    monitoringOn: 'Monitoratge ON',
    monitoringOff: 'Monitoratge OFF',
    quickQsoOn: 'QSO rapid ON',
    quickQsoOff: 'QSO rapid',
    ft2Only: 'Nomes FT2',
    txRunning: 'TX en curs',
    rxIdle: 'RX inactiu',
    asyncReady: 'Async preparat',
    txWith: 'TX amb',
    lastTx: 'Ultim TX',
    ready: 'Preparat',
    commandFailed: 'Ordre fallida',
    bandActivityPaused: 'Activitat de banda en pausa',
    bandActivityResumed: 'Activitat de banda represa',
    bandActivityCleared: 'Activitat de banda netejada',
    installHintPrompt: "Toca Instal la App per afegir Decodium a la pantalla d'inici.",
    installHintIos: "iPhone/iPad: Safari -> Comparteix -> Afegeix a la pantalla d'inici.",
    installHintInsecure: "Per instal lar l'app completa a Android, fes servir HTTPS o localhost.",
    installHintAndroid: "Android: menu del navegador -> Afegeix a la pantalla d'inici.",
    initError: 'error init',
    backendUnreachable: 'backend inabastable',
    noPresetSaved: 'cap preset desat per a',
    invalidPresetValues: 'valors de preset no valids per a',
    presetSaved: 'preset desat',
    rxTxSet: 'Rx/Tx definit'
  });
  const UI_TEXT_DA = buildUiText('da', {
    pageTitle: 'Decodium fjernkonsol',
    remoteAccess: 'Fjernadgang',
    loginCaptionAuth: 'Indtast brugernavn og adgangskode for at lase dashboard op',
    loginCaptionNoAuth: 'Godkendelse ikke pakraevet',
    username: 'Brugernavn',
    password: 'Adgangskode',
    accessPassword: 'Adgangskode',
    unlock: 'Las op',
    tokenMinHint: 'Adgangskoden skal vaere mindst 12 tegn.',
    installApp: 'Installer app',
    dialFrequency: 'Dialfrekvens',
    band: 'Band',
    myCall: 'Mit kaldesignal',
    dxCall: 'DX-kaldesignal',
    txEnabled: 'TX aktiveret',
    transmitting: 'Sender',
    monitoring: 'Overvagning',
    rxTxFrequency: 'Rx / Tx-frekvens (Hz)',
    setRxTx: 'Saet Rx+Tx',
    setRx: 'Saet Rx',
    setTx: 'Saet Tx',
    modePreset: 'Tilstands-preset (Rx/Tx)',
    applyPreset: 'Anvend preset',
    saveCurrent: 'Gem nuvaerende',
    txControl: 'TX-kontrol',
    enableTx: 'Aktiver TX',
    disableTx: 'Deaktiver TX',
    emissionOptions: 'Sendemuligheder',
    manualTx: 'Manuel TX',
    waterfallTitle: 'Waterfall (eksperimentel)',
    waterfallHint: 'Tryk/klik pa waterfall for at saette Rx-frekvens.',
    activityTitle: 'RX/TX-aktivitet',
    hideCq: 'Skjul CQ',
    cqHidden: 'CQ skjult',
    hide73: 'Skjul 73',
    d73Hidden: '73 skjult',
    pause: 'Pause',
    resume: 'Genoptag',
    clear: 'Ryd',
    remoteAction: 'Fjernhandling',
    callStation: 'Kald station (slot-aware)',
    connected: 'forbundet',
    connectedPoll: 'forbundet (poll)',
    disconnected: 'frakoblet',
    authenticating: 'godkender...',
    locked: 'last',
    wsError: 'ws-fejl',
    authFailed: 'godkendelse mislykkedes',
    invalidCredentials: 'Ugyldigt brugernavn eller adgangskode',
    passwordRequired: 'Adgangskode pakraevet',
    yes: 'ja',
    no: 'nej',
    monitoringOn: 'Overvagning ON',
    monitoringOff: 'Overvagning OFF',
    quickQsoOn: 'Hurtig QSO ON',
    quickQsoOff: 'Hurtig QSO',
    ft2Only: 'Kun FT2',
    txRunning: 'TX korer',
    rxIdle: 'RX inaktiv',
    asyncReady: 'Async klar',
    txWith: 'TX med',
    lastTx: 'Sidste TX',
    ready: 'Klar',
    commandFailed: 'Kommando mislykkedes',
    bandActivityPaused: 'Bandaktivitet sat pa pause',
    bandActivityResumed: 'Bandaktivitet genoptaget',
    bandActivityCleared: 'Bandaktivitet ryddet',
    installHintPrompt: 'Tryk pa Installer app for at tilfoje Decodium til hjemmeskaermen.',
    installHintIos: 'iPhone/iPad: Safari -> Del -> Føj til hjemmeskærm.',
    installHintInsecure: 'For fuld app-installation pa Android skal du bruge HTTPS eller localhost.',
    installHintAndroid: 'Android: browsermenu -> Føj til hjemmeskærm.',
    initError: 'init-fejl',
    backendUnreachable: 'backend utilgaengelig',
    noPresetSaved: 'ingen preset gemt for',
    invalidPresetValues: 'ugyldige preset-vaerdier for',
    presetSaved: 'preset gemt',
    rxTxSet: 'Rx/Tx sat'
  });
  const UI_TEXT_DE = buildUiText('de', {
    pageTitle: 'Decodium Fernkonsole',
    remoteAccess: 'Fernzugriff',
    loginCaptionAuth: 'Benutzername und Passwort eingeben, um das Dashboard zu entsperren',
    loginCaptionNoAuth: 'Keine Authentifizierung erforderlich',
    username: 'Benutzername',
    password: 'Passwort',
    accessPassword: 'Zugangspasswort',
    unlock: 'Entsperren',
    tokenMinHint: 'Das Zugangspasswort muss mindestens 12 Zeichen lang sein.',
    installApp: 'App installieren',
    dialFrequency: 'Dial-Frequenz',
    band: 'Band',
    myCall: 'Mein Rufzeichen',
    dxCall: 'DX-Rufzeichen',
    txEnabled: 'TX aktiviert',
    transmitting: 'Sendet',
    monitoring: 'Monitoring',
    rxTxFrequency: 'Rx / Tx-Frequenz (Hz)',
    setRxTx: 'Rx+Tx setzen',
    setRx: 'Rx setzen',
    setTx: 'Tx setzen',
    modePreset: 'Mode-Preset (Rx/Tx)',
    applyPreset: 'Preset anwenden',
    saveCurrent: 'Aktuelles speichern',
    txControl: 'TX-Steuerung',
    enableTx: 'TX aktivieren',
    disableTx: 'TX deaktivieren',
    emissionOptions: 'Sendeoptionen',
    manualTx: 'Manuelles TX',
    waterfallTitle: 'Waterfall (experimentell)',
    waterfallHint: 'Tippen/klicken Sie auf den Waterfall, um die Rx-Frequenz zu setzen.',
    activityTitle: 'RX/TX-Aktivitat',
    hideCq: 'CQ ausblenden',
    cqHidden: 'CQ ausgeblendet',
    hide73: '73 ausblenden',
    d73Hidden: '73 ausgeblendet',
    pause: 'Pause',
    resume: 'Fortsetzen',
    clear: 'Leeren',
    remoteAction: 'Fernaktion',
    callStation: 'Station rufen (slot-aware)',
    connected: 'verbunden',
    connectedPoll: 'verbunden (poll)',
    disconnected: 'getrennt',
    authenticating: 'authentifiziere...',
    locked: 'gesperrt',
    wsError: 'ws-Fehler',
    authFailed: 'Authentifizierung fehlgeschlagen',
    invalidCredentials: 'Ungultiger Benutzername oder Passwort',
    passwordRequired: 'Passwort erforderlich',
    yes: 'ja',
    no: 'nein',
    monitoringOn: 'Monitoring EIN',
    monitoringOff: 'Monitoring AUS',
    quickQsoOn: 'Quick QSO EIN',
    quickQsoOff: 'Quick QSO',
    ft2Only: 'Nur FT2',
    txRunning: 'TX lauft',
    rxIdle: 'RX inaktiv',
    asyncReady: 'Async bereit',
    txWith: 'TX mit',
    lastTx: 'Letztes TX',
    ready: 'Bereit',
    commandFailed: 'Befehl fehlgeschlagen',
    bandActivityPaused: 'Bandaktivitat pausiert',
    bandActivityResumed: 'Bandaktivitat fortgesetzt',
    bandActivityCleared: 'Bandaktivitat geleert',
    installHintPrompt: 'Tippen Sie auf App installieren, um Decodium zum Homescreen hinzuzufugen.',
    installHintIos: 'iPhone/iPad: Safari -> Teilen -> Zum Home-Bildschirm.',
    installHintInsecure: 'Fur die vollstandige App-Installation auf Android HTTPS oder localhost verwenden.',
    installHintAndroid: 'Android: Browsermenu -> Zum Startbildschirm hinzufugen.',
    initError: 'Init-Fehler',
    backendUnreachable: 'Backend nicht erreichbar',
    noPresetSaved: 'kein Preset gespeichert fur',
    invalidPresetValues: 'ungultige Preset-Werte fur',
    presetSaved: 'Preset gespeichert',
    rxTxSet: 'Rx/Tx gesetzt'
  });
  const UI_TEXT_ES = buildUiText('es', {
    pageTitle: 'Consola remota de Decodium',
    remoteAccess: 'Acceso remoto',
    loginCaptionAuth: 'Introduce usuario y contrasena para desbloquear el panel',
    loginCaptionNoAuth: 'Autenticacion no requerida',
    username: 'Usuario',
    password: 'Contrasena',
    accessPassword: 'Contrasena de acceso',
    unlock: 'Desbloquear',
    tokenMinHint: 'La contrasena de acceso debe tener al menos 12 caracteres.',
    installApp: 'Instalar app',
    dialFrequency: 'Frecuencia dial',
    band: 'Banda',
    myCall: 'Mi indicativo',
    dxCall: 'Indicativo DX',
    txEnabled: 'TX habilitado',
    transmitting: 'Transmitiendo',
    monitoring: 'Monitorizacion',
    rxTxFrequency: 'Frecuencia Rx / Tx (Hz)',
    setRxTx: 'Ajustar Rx+Tx',
    setRx: 'Ajustar Rx',
    setTx: 'Ajustar Tx',
    modePreset: 'Preset de modo (Rx/Tx)',
    applyPreset: 'Aplicar preset',
    saveCurrent: 'Guardar actual',
    txControl: 'Control TX',
    enableTx: 'Habilitar TX',
    disableTx: 'Deshabilitar TX',
    emissionOptions: 'Opciones de emision',
    manualTx: 'TX manual',
    waterfallTitle: 'Waterfall (experimental)',
    waterfallHint: 'Toca/haz clic en el waterfall para ajustar la frecuencia Rx.',
    activityTitle: 'Actividad RX/TX',
    hideCq: 'Ocultar CQ',
    cqHidden: 'CQ ocultos',
    hide73: 'Ocultar 73',
    d73Hidden: '73 ocultos',
    pause: 'Pausa',
    resume: 'Reanudar',
    clear: 'Limpiar',
    remoteAction: 'Accion remota',
    callStation: 'Llamar estacion (slot-aware)',
    connected: 'conectado',
    connectedPoll: 'conectado (poll)',
    disconnected: 'desconectado',
    authenticating: 'autenticando...',
    locked: 'bloqueado',
    wsError: 'error ws',
    authFailed: 'autenticacion fallida',
    invalidCredentials: 'Usuario o contrasena no validos',
    passwordRequired: 'Contrasena requerida',
    yes: 'si',
    no: 'no',
    monitoringOn: 'Monitorizacion ON',
    monitoringOff: 'Monitorizacion OFF',
    quickQsoOn: 'QSO rapido ON',
    quickQsoOff: 'QSO rapido',
    ft2Only: 'Solo FT2',
    txRunning: 'TX en curso',
    rxIdle: 'RX inactivo',
    asyncReady: 'Async listo',
    txWith: 'TX con',
    lastTx: 'Ultimo TX',
    ready: 'Listo',
    commandFailed: 'Comando fallido',
    bandActivityPaused: 'Actividad de banda en pausa',
    bandActivityResumed: 'Actividad de banda reanudada',
    bandActivityCleared: 'Actividad de banda limpiada',
    installHintPrompt: 'Toca Instalar app para anadir Decodium a tu pantalla de inicio.',
    installHintIos: 'iPhone/iPad: Safari -> Compartir -> Anadir a inicio.',
    installHintInsecure: 'Para instalar la app completa en Android usa HTTPS o localhost.',
    installHintAndroid: 'Android: menu del navegador -> Anadir a pantalla de inicio.',
    initError: 'error init',
    backendUnreachable: 'backend no accesible',
    noPresetSaved: 'no hay preset guardado para',
    invalidPresetValues: 'valores de preset no validos para',
    presetSaved: 'preset guardado',
    rxTxSet: 'Rx/Tx ajustados'
  });
  const UI_TEXT_FR = buildUiText('fr', {
    pageTitle: 'Console distante Decodium',
    remoteAccess: 'Acces distant',
    loginCaptionAuth: "Saisissez le nom d'utilisateur et le mot de passe pour deverrouiller le tableau de bord",
    loginCaptionNoAuth: 'Authentification non requise',
    username: "Nom d'utilisateur",
    password: 'Mot de passe',
    accessPassword: "Mot de passe d'acces",
    unlock: 'Debloquer',
    tokenMinHint: "Le mot de passe d'acces doit contenir au moins 12 caracteres.",
    installApp: "Installer l'app",
    dialFrequency: 'Frequence dial',
    band: 'Bande',
    myCall: 'Mon indicatif',
    dxCall: 'Indicatif DX',
    txEnabled: 'TX active',
    transmitting: 'En emission',
    monitoring: 'Surveillance',
    rxTxFrequency: 'Frequence Rx / Tx (Hz)',
    setRxTx: 'Definir Rx+Tx',
    setRx: 'Definir Rx',
    setTx: 'Definir Tx',
    modePreset: 'Preset mode (Rx/Tx)',
    applyPreset: 'Appliquer le preset',
    saveCurrent: "Enregistrer l'actuel",
    txControl: 'Controle TX',
    enableTx: 'Activer TX',
    disableTx: 'Desactiver TX',
    emissionOptions: "Options d'emission",
    manualTx: 'TX manuel',
    waterfallTitle: 'Waterfall (experimental)',
    waterfallHint: 'Touchez/cliquez sur le waterfall pour regler la frequence Rx.',
    activityTitle: 'Activite RX/TX',
    hideCq: 'Masquer CQ',
    cqHidden: 'CQ masques',
    hide73: 'Masquer 73',
    d73Hidden: '73 masques',
    pause: 'Pause',
    resume: 'Reprendre',
    clear: 'Effacer',
    remoteAction: 'Action distante',
    callStation: 'Appeler la station (slot-aware)',
    connected: 'connecte',
    connectedPoll: 'connecte (poll)',
    disconnected: 'deconnecte',
    authenticating: 'authentification...',
    locked: 'verrouille',
    wsError: 'erreur ws',
    authFailed: "echec d'authentification",
    invalidCredentials: "Nom d'utilisateur ou mot de passe invalide",
    passwordRequired: 'Mot de passe requis',
    yes: 'oui',
    no: 'non',
    monitoringOn: 'Surveillance ON',
    monitoringOff: 'Surveillance OFF',
    quickQsoOn: 'QSO rapide ON',
    quickQsoOff: 'QSO rapide',
    ft2Only: 'FT2 seulement',
    txRunning: 'TX en cours',
    rxIdle: 'RX inactif',
    asyncReady: 'Async pret',
    txWith: 'TX avec',
    lastTx: 'Dernier TX',
    ready: 'Pret',
    commandFailed: 'Commande echouee',
    bandActivityPaused: 'Activite bande en pause',
    bandActivityResumed: 'Activite bande reprise',
    bandActivityCleared: 'Activite bande effacee',
    installHintPrompt: "Touchez Installer l'app pour ajouter Decodium a l'ecran d'accueil.",
    installHintIos: "iPhone/iPad: Safari -> Partager -> Sur l'ecran d'accueil.",
    installHintInsecure: "Pour installer l'app complete sur Android, utilisez HTTPS ou localhost.",
    installHintAndroid: "Android: menu du navigateur -> Ajouter a l'ecran d'accueil.",
    initError: 'erreur init',
    backendUnreachable: 'backend inaccessible',
    noPresetSaved: 'aucun preset enregistre pour',
    invalidPresetValues: 'valeurs de preset invalides pour',
    presetSaved: 'preset enregistre',
    rxTxSet: 'Rx/Tx definis'
  });
  const UI_TEXT_JA = buildUiText('ja', {
    lang: 'ja',
    pageTitle: 'Decodium Remote コンソール',
    remoteAccess: 'リモートアクセス',
    loginCaptionAuth: 'ダッシュボードを解除するためにユーザー名とパスワードを入力してください',
    loginCaptionNoAuth: '認証は不要です',
    username: 'ユーザー名',
    password: 'パスワード',
    accessPassword: 'アクセス用パスワード',
    unlock: '解除',
    tokenMinHint: 'アクセス用パスワードは12文字以上必要です。',
    installApp: 'アプリをインストール',
    dialFrequency: 'ダイヤル周波数',
    band: 'バンド',
    myCall: '自局コール',
    dxCall: 'DXコール',
    txEnabled: 'TX有効',
    transmitting: '送信中',
    monitoring: 'モニタリング',
    rxTxFrequency: 'Rx / Tx 周波数 (Hz)',
    setRxTx: 'Rx+Tx設定',
    setRx: 'Rx設定',
    setTx: 'Tx設定',
    modePreset: 'モードプリセット (Rx/Tx)',
    applyPreset: 'プリセット適用',
    saveCurrent: '現在値を保存',
    txControl: 'TX制御',
    enableTx: 'TX有効',
    disableTx: 'TX無効',
    emissionOptions: '送信オプション',
    manualTx: '手動TX',
    waterfallTitle: 'Waterfall (実験的)',
    waterfallHint: 'Waterfallをタップ/クリックしてRx周波数を設定します。',
    activityTitle: 'RX/TXアクティビティ',
    hideCq: 'CQを隠す',
    cqHidden: 'CQ非表示',
    hide73: '73を隠す',
    d73Hidden: '73非表示',
    pause: '一時停止',
    resume: '再開',
    clear: 'クリア',
    remoteAction: 'リモート操作',
    callStation: '局を呼ぶ (slot-aware)',
    connected: '接続済み',
    connectedPoll: '接続済み (poll)',
    disconnected: '未接続',
    authenticating: '認証中...',
    locked: 'ロック中',
    wsError: 'ws エラー',
    authFailed: '認証失敗',
    invalidCredentials: 'ユーザー名またはパスワードが無効です',
    passwordRequired: 'パスワードが必要です',
    yes: 'はい',
    no: 'いいえ',
    monitoringOn: 'Monitoring ON',
    monitoringOff: 'Monitoring OFF',
    quickQsoOn: 'Quick QSO ON',
    quickQsoOff: 'Quick QSO',
    ft2Only: 'FT2のみ',
    txRunning: 'TX動作中',
    rxIdle: 'RX待機中',
    asyncReady: 'Async準備完了',
    txWith: 'TX相手',
    lastTx: '最後のTX',
    ready: '準備完了',
    commandFailed: 'コマンド失敗',
    bandActivityPaused: 'バンドアクティビティを一時停止しました',
    bandActivityResumed: 'バンドアクティビティを再開しました',
    bandActivityCleared: 'バンドアクティビティを消去しました',
    installHintPrompt: 'アプリをインストールを押してDecodiumをホーム画面に追加します。',
    installHintIos: 'iPhone/iPad: Safari -> 共有 -> ホーム画面に追加。',
    installHintInsecure: 'Androidで完全なアプリとしてインストールするにはHTTPSまたはlocalhostを使用してください。',
    installHintAndroid: 'Android: ブラウザメニュー -> ホーム画面に追加。',
    initError: '初期化エラー',
    backendUnreachable: 'バックエンドに接続できません',
    noPresetSaved: '保存されたプリセットがありません:',
    invalidPresetValues: 'プリセット値が無効です:',
    presetSaved: 'プリセット保存',
    presetLabel: 'プリセット',
    rxTxSet: 'Rx/Tx設定'
  });
  const UI_TEXT_RU = buildUiText('ru', {
    lang: 'ru',
    pageTitle: 'Удаленная консоль Decodium',
    remoteAccess: 'Удаленный доступ',
    loginCaptionAuth: 'Введите имя пользователя и пароль для разблокировки панели',
    loginCaptionNoAuth: 'Аутентификация не требуется',
    username: 'Имя пользователя',
    password: 'Пароль',
    accessPassword: 'Пароль доступа',
    unlock: 'Разблокировать',
    tokenMinHint: 'Пароль доступа должен содержать не менее 12 символов.',
    installApp: 'Установить приложение',
    dialFrequency: 'Частота dial',
    band: 'Диапазон',
    myCall: 'Мой позывной',
    dxCall: 'DX позывной',
    txEnabled: 'TX включен',
    transmitting: 'Передача',
    monitoring: 'Мониторинг',
    rxTxFrequency: 'Частота Rx / Tx (Hz)',
    setRxTx: 'Установить Rx+Tx',
    setRx: 'Установить Rx',
    setTx: 'Установить Tx',
    modePreset: 'Пресет режима (Rx/Tx)',
    applyPreset: 'Применить пресет',
    saveCurrent: 'Сохранить текущее',
    txControl: 'Управление TX',
    enableTx: 'Включить TX',
    disableTx: 'Выключить TX',
    emissionOptions: 'Параметры передачи',
    manualTx: 'Ручной TX',
    waterfallTitle: 'Waterfall (экспериментально)',
    waterfallHint: 'Нажмите на waterfall, чтобы установить частоту Rx.',
    activityTitle: 'Активность RX/TX',
    hideCq: 'Скрыть CQ',
    cqHidden: 'CQ скрыты',
    hide73: 'Скрыть 73',
    d73Hidden: '73 скрыты',
    pause: 'Пауза',
    resume: 'Продолжить',
    clear: 'Очистить',
    remoteAction: 'Удаленное действие',
    callStation: 'Вызвать станцию (slot-aware)',
    connected: 'подключено',
    connectedPoll: 'подключено (poll)',
    disconnected: 'отключено',
    authenticating: 'аутентификация...',
    locked: 'заблокировано',
    wsError: 'ошибка ws',
    authFailed: 'ошибка аутентификации',
    invalidCredentials: 'Неверное имя пользователя или пароль',
    passwordRequired: 'Требуется пароль',
    yes: 'да',
    no: 'нет',
    monitoringOn: 'Мониторинг ON',
    monitoringOff: 'Мониторинг OFF',
    quickQsoOn: 'Quick QSO ON',
    quickQsoOff: 'Quick QSO',
    ft2Only: 'Только FT2',
    txRunning: 'TX выполняется',
    rxIdle: 'RX неактивен',
    asyncReady: 'Async готов',
    txWith: 'TX с',
    lastTx: 'Последний TX',
    ready: 'Готово',
    commandFailed: 'Команда не выполнена',
    bandActivityPaused: 'Активность диапазона приостановлена',
    bandActivityResumed: 'Активность диапазона возобновлена',
    bandActivityCleared: 'Активность диапазона очищена',
    installHintPrompt: 'Нажмите Установить приложение, чтобы добавить Decodium на главный экран.',
    installHintIos: 'iPhone/iPad: Safari -> Поделиться -> На экран Домой.',
    installHintInsecure: 'Для полной установки на Android используйте HTTPS или localhost.',
    installHintAndroid: 'Android: меню браузера -> Добавить на главный экран.',
    initError: 'ошибка инициализации',
    backendUnreachable: 'backend недоступен',
    noPresetSaved: 'нет сохраненного пресета для',
    invalidPresetValues: 'неверные значения пресета для',
    presetSaved: 'пресет сохранен',
    rxTxSet: 'Rx/Tx установлены'
  });
  const UI_TEXT_ZH = buildUiText('zh-CN', {
    lang: 'zh-CN',
    pageTitle: 'Decodium 远程控制台',
    remoteAccess: '远程访问',
    loginCaptionAuth: '请输入用户名和密码以解锁面板',
    loginCaptionNoAuth: '无需身份验证',
    username: '用户名',
    password: '密码',
    accessPassword: '访问密码',
    unlock: '解锁',
    tokenMinHint: '访问密码至少需要 12 个字符。',
    installApp: '安装应用',
    dialFrequency: '拨号频率',
    band: '波段',
    myCall: '我的呼号',
    dxCall: 'DX 呼号',
    txEnabled: 'TX 已启用',
    transmitting: '发射中',
    monitoring: '监控',
    rxTxFrequency: 'Rx / Tx 频率 (Hz)',
    setRxTx: '设置 Rx+Tx',
    setRx: '设置 Rx',
    setTx: '设置 Tx',
    modePreset: '模式预设 (Rx/Tx)',
    applyPreset: '应用预设',
    saveCurrent: '保存当前值',
    txControl: 'TX 控制',
    enableTx: '启用 TX',
    disableTx: '禁用 TX',
    emissionOptions: '发射选项',
    manualTx: '手动 TX',
    waterfallTitle: 'Waterfall（实验性）',
    waterfallHint: '点击 waterfall 以设置 Rx 频率。',
    activityTitle: 'RX/TX 活动',
    hideCq: '隐藏 CQ',
    cqHidden: '已隐藏 CQ',
    hide73: '隐藏 73',
    d73Hidden: '已隐藏 73',
    pause: '暂停',
    resume: '继续',
    clear: '清除',
    remoteAction: '远程操作',
    callStation: '呼叫电台（slot-aware）',
    connected: '已连接',
    connectedPoll: '已连接 (poll)',
    disconnected: '未连接',
    authenticating: '认证中...',
    locked: '已锁定',
    wsError: 'ws 错误',
    authFailed: '认证失败',
    invalidCredentials: '用户名或密码无效',
    passwordRequired: '需要密码',
    yes: '是',
    no: '否',
    monitoringOn: '监控 ON',
    monitoringOff: '监控 OFF',
    quickQsoOn: 'Quick QSO ON',
    quickQsoOff: 'Quick QSO',
    ft2Only: '仅 FT2',
    txRunning: 'TX 运行中',
    rxIdle: 'RX 空闲',
    asyncReady: 'Async 就绪',
    txWith: 'TX 对象',
    lastTx: '上次 TX',
    ready: '就绪',
    commandFailed: '命令失败',
    bandActivityPaused: '频段活动已暂停',
    bandActivityResumed: '频段活动已恢复',
    bandActivityCleared: '频段活动已清除',
    installHintPrompt: '点击安装应用，将 Decodium 添加到主屏幕。',
    installHintIos: 'iPhone/iPad: Safari -> 分享 -> 添加到主屏幕。',
    installHintInsecure: '若要在 Android 上完整安装，请使用 HTTPS 或 localhost。',
    installHintAndroid: 'Android: 浏览器菜单 -> 添加到主屏幕。',
    initError: '初始化错误',
    backendUnreachable: '后端不可达',
    noPresetSaved: '没有已保存的预设:',
    invalidPresetValues: '预设值无效:',
    presetSaved: '预设已保存',
    presetLabel: '预设',
    rxTxSet: 'Rx/Tx 已设置'
  });
  const UI_TEXT_ZH_TW = buildUiText('zh-TW', {
    lang: 'zh-TW',
    pageTitle: 'Decodium 遠端控制台',
    remoteAccess: '遠端存取',
    loginCaptionAuth: '請輸入使用者名稱與密碼以解鎖面板',
    loginCaptionNoAuth: '不需要驗證',
    username: '使用者名稱',
    password: '密碼',
    accessPassword: '存取密碼',
    unlock: '解鎖',
    tokenMinHint: '存取密碼至少需要 12 個字元。',
    installApp: '安裝 App',
    dialFrequency: '撥號頻率',
    band: '波段',
    myCall: '我的呼號',
    dxCall: 'DX 呼號',
    txEnabled: 'TX 已啟用',
    transmitting: '發射中',
    monitoring: '監控',
    rxTxFrequency: 'Rx / Tx 頻率 (Hz)',
    setRxTx: '設定 Rx+Tx',
    setRx: '設定 Rx',
    setTx: '設定 Tx',
    modePreset: '模式預設 (Rx/Tx)',
    applyPreset: '套用預設',
    saveCurrent: '儲存目前值',
    txControl: 'TX 控制',
    enableTx: '啟用 TX',
    disableTx: '停用 TX',
    emissionOptions: '發射選項',
    manualTx: '手動 TX',
    waterfallTitle: 'Waterfall（實驗性）',
    waterfallHint: '點擊 waterfall 以設定 Rx 頻率。',
    activityTitle: 'RX/TX 活動',
    hideCq: '隱藏 CQ',
    cqHidden: 'CQ 已隱藏',
    hide73: '隱藏 73',
    d73Hidden: '73 已隱藏',
    pause: '暫停',
    resume: '繼續',
    clear: '清除',
    remoteAction: '遠端操作',
    callStation: '呼叫電台（slot-aware）',
    connected: '已連線',
    connectedPoll: '已連線 (poll)',
    disconnected: '未連線',
    authenticating: '驗證中...',
    locked: '已鎖定',
    wsError: 'ws 錯誤',
    authFailed: '驗證失敗',
    invalidCredentials: '使用者名稱或密碼無效',
    passwordRequired: '需要密碼',
    yes: '是',
    no: '否',
    monitoringOn: '監控 ON',
    monitoringOff: '監控 OFF',
    quickQsoOn: 'Quick QSO ON',
    quickQsoOff: 'Quick QSO',
    ft2Only: '僅限 FT2',
    txRunning: 'TX 執行中',
    rxIdle: 'RX 閒置',
    asyncReady: 'Async 就緒',
    txWith: 'TX 對象',
    lastTx: '上次 TX',
    ready: '就緒',
    commandFailed: '指令失敗',
    bandActivityPaused: '波段活動已暫停',
    bandActivityResumed: '波段活動已恢復',
    bandActivityCleared: '波段活動已清除',
    installHintPrompt: '點擊安裝 App，將 Decodium 加到主畫面。',
    installHintIos: 'iPhone/iPad: Safari -> 分享 -> 加到主畫面。',
    installHintInsecure: '若要在 Android 上完整安裝，請使用 HTTPS 或 localhost。',
    installHintAndroid: 'Android: 瀏覽器選單 -> 加到主畫面。',
    initError: '初始化錯誤',
    backendUnreachable: '後端無法連線',
    noPresetSaved: '沒有已儲存的預設:',
    invalidPresetValues: '預設值無效:',
    presetSaved: '預設已儲存',
    presetLabel: '預設',
    rxTxSet: 'Rx/Tx 已設定'
  });
  const UI_TEXT_BY_LANG = {
    en: UI_TEXT_EN,
    en_us: UI_TEXT_EN,
    en_gb: UI_TEXT_EN_GB,
    it: UI_TEXT_IT,
    ca: UI_TEXT_CA,
    da: UI_TEXT_DA,
    de: UI_TEXT_DE,
    es: UI_TEXT_ES,
    fr: UI_TEXT_FR,
    ja: UI_TEXT_JA,
    ru: UI_TEXT_RU,
    zh: UI_TEXT_ZH,
    zh_cn: UI_TEXT_ZH,
    zh_tw: UI_TEXT_ZH_TW
  };
  let appUiLanguage = 'en';
  let uiText = UI_TEXT_EN;
  let ws = null;
  let health = null;
  const activityRows = [];
  const ACTIVITY_MAX_ROWS = 150;
  const ACTIVITY_WINDOW_MS = 15 * 60 * 1000;
  const AUTH_TTL_MS = 30 * 60 * 1000;
  const AUTH_STORAGE_KEY = 'ft2_remote_auth_v1';
  const MODE_FREQ_STORAGE_KEY = 'ft2_remote_mode_freq_v1';
  const SW_VERSION = '%1';
  const MODE_PRESET_MODES = ['FT2','FT8','FT4','MSK144','Q65','JT65','JT9','FST4','WSPR'];
  let activeMode = '';
  let activeBand = '';
  let authUser = 'admin';
  let authToken = '';
  let requiresAuth = false;
  let wsAuthed = false;
  let activityPaused = false;
  let activityHideCq = false;
  let activityHide73 = false;
  let lastTransmitting = null;
  let lastTxPeer = '';
  let txPeerFromRows = '';
  let txEnabledState = false;
  let autoCqEnabled = false;
  let autoSpotEnabled = false;
  let asyncL2Enabled = false;
  let dualCarrierEnabled = false;
  let alt12Enabled = false;
  let manualTxEnabled = false;
  let speedyContestEnabled = false;
  let digitalMorseEnabled = false;
  let quickQsoEnabled = false;
  let ft2QsoMessageCount = 5;
  let asyncSnrDb = -99;
  let waterfallEnabled = false;
  let waterfallMeta = {startHz:0, spanHz:0, width:0};
  let currentMode = '';
  let monitoringState = false;
  let transmittingState = false;
  let currentRxHz = 0;
  let currentTxHz = 0;
  let pendingRxHz = null;
  let pendingTxHz = null;
  let pendingFreqSetDeadlineMs = 0;
  const PENDING_FREQ_GRACE_MS = 2200;
  let rxInputDirty = false;
  let txInputDirty = false;
  let rxInputEditedAtMs = 0;
  let txInputEditedAtMs = 0;
  const INPUT_EDIT_GRACE_MS = 7000;
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
  const normalizeUiLanguage = (v) => (v || '').toString().trim().replace(/-/g, '_').toLowerCase();
  const sanitizeFrequency = (v) => {
    const n = Number(v);
    if (!Number.isFinite(n)) return null;
    return Math.max(0, Math.min(5000, Math.round(n)));
  };
  const setText = (id, v) => {
    const node = el(id);
    if (node) node.textContent = v ?? '';
  };
  const setPlaceholder = (id, v) => {
    const node = el(id);
    if (node) node.setAttribute('placeholder', v ?? '');
  };
  const selectUiText = (code) => {
    const normalized = normalizeUiLanguage(code);
    if (!normalized) return UI_TEXT_EN;
    return UI_TEXT_BY_LANG[normalized]
      || UI_TEXT_BY_LANG[normalized.split('_')[0]]
      || UI_TEXT_EN;
  };
  const boolText = (v) => v ? uiText.yes : uiText.no;
  const msgCountText = (count) => `${count} ${uiText.msgSuffix}`;

  function applyLocalization() {
    document.documentElement.lang = uiText.lang || 'en';
    document.title = uiText.pageTitle;
    if (appleWebAppTitle) {
      appleWebAppTitle.setAttribute('content', uiText.shortTitle);
    }
    setText('login_title', uiText.remoteAccess);
    setText('lbl_login_user', uiText.username);
    setText('lbl_login_token', uiText.password);
    setPlaceholder('login_token', uiText.accessPassword);
    setText('btn_login', uiText.unlock);
    setText('app_title', uiText.pageTitle);
    setText('btn_install', uiText.installApp);
    setText('lbl_dial', uiText.dialFrequency);
    setText('lbl_mode', uiText.mode);
    setText('lbl_band', uiText.band);
    setText('lbl_rx', uiText.rx);
    setText('lbl_tx', uiText.tx);
    setText('lbl_mycall', uiText.myCall);
    setText('lbl_dxcall', uiText.dxCall);
    setText('lbl_tx_enabled', uiText.txEnabled);
    setText('lbl_transmitting', uiText.transmitting);
    setText('lbl_monitoring', uiText.monitoring);
    setText('ttl_mode', uiText.mode);
    setText('ttl_band', uiText.band);
    setText('lbl_rxtx_freq', uiText.rxTxFrequency);
    setText('btn_set_rxtx', uiText.setRxTx);
    setText('btn_rxfreq', uiText.setRx);
    setText('btn_txfreq', uiText.setTx);
    setText('lbl_mode_preset', uiText.modePreset);
    setText('btn_apply_mode_preset', uiText.applyPreset);
    setText('btn_save_mode_preset', uiText.saveCurrent);
    setText('lbl_tx_control', uiText.txControl);
    setText('btn_tx_on', uiText.enableTx);
    setText('btn_tx_off', uiText.disableTx);
    setText('ttl_emission', uiText.emissionOptions);
    setText('lbl_manual_tx', uiText.manualTx);
    setText('lbl_speedy', uiText.speedy);
    setText('lbl_digital_morse', uiText.digitalMorse);
    setText('lbl_qso', uiText.qso);
    setText('ttl_waterfall', uiText.waterfallTitle);
    setText('waterfall_hint', uiText.waterfallHint);
    setText('ttl_activity', uiText.activityTitle);
    setText('th_utc', 'UTC');
    setText('th_db', 'dB');
    setText('th_dt', 'DT');
    setText('th_freq', uiText.frequency);
    setText('th_msg', uiText.message);
    setText('ttl_remote_action', uiText.remoteAction);
    setText('lbl_call', uiText.call);
    setText('lbl_grid', uiText.grid);
    setText('btn_call', uiText.callStation);
    if (ft2QsoCount && ft2QsoCount.options.length >= 3) {
      ft2QsoCount.options[0].textContent = msgCountText(2);
      ft2QsoCount.options[1].textContent = msgCountText(3);
      ft2QsoCount.options[2].textContent = msgCountText(5);
    }
    if (loginCaption) {
      loginCaption.textContent = requiresAuth ? uiText.loginCaptionAuth : uiText.loginCaptionNoAuth;
    }
    if (loginTokenHint) {
      loginTokenHint.textContent = uiText.tokenMinHint;
    }
    applyWaterfallButtonState();
    applyAutoCqButtonState();
    applyAutoSpotButtonState();
    applyMonitoringButtonState();
    applyEmissionButtonsState();
    applyActivityFilterButtonsState();
    applyFt2ControlsState();
    if (btnActivityPause) {
      btnActivityPause.textContent = activityPaused ? uiText.resume : uiText.pause;
    }
    if (btnActivityClear) {
      btnActivityClear.textContent = uiText.clear;
    }
    updateTxBadge(transmittingState, txEnabledState);
    updateTxPeer(transmittingState, '');
    if (conn) {
      const connText = (conn.textContent || '').trim().toLowerCase();
      if (!conn.classList.contains('on')
          && (!connText || connText === 'disconnected' || connText === 'disconnesso')) {
        conn.textContent = uiText.disconnected;
      }
    }
  }

  function setAppUiLanguage(code) {
    appUiLanguage = normalizeUiLanguage(code || 'en') || 'en';
    uiText = selectUiText(appUiLanguage);
    applyLocalization();
  }

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

  function getCurrentPresetMode() {
    const modeNow = normalizeModeKey(currentMode || activeMode);
    if (modeNow && MODE_PRESET_MODES.includes(modeNow)) return modeNow;
    return 'FT2';
  }

  function updateModePresetSelect() {
    if (!modePresetSelect) return;
    const mode = getCurrentPresetMode();
    if (modePresetSelect.options.length !== 1) {
      modePresetSelect.innerHTML = '';
      modePresetSelect.appendChild(document.createElement('option'));
    }
    const preset = modeFrequencyPresets[mode];
    const option = modePresetSelect.options[0];
    option.value = mode;
    option.textContent = preset ? `${mode} (${preset.rx}/${preset.tx})` : `${mode} (---/---)`;
    modePresetSelect.value = mode;
    modePresetSelect.disabled = true;
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
    if (!preset) {
      if (!silent) setActionStatus(`${uiText.noPresetSaved} ${key}`, true);
      return false;
    }
    const rx = sanitizeFrequency(preset.rx);
    const tx = sanitizeFrequency(preset.tx);
    if (rx === null || tx === null) {
      if (!silent) setActionStatus(`${uiText.invalidPresetValues} ${key}`, true);
      return false;
    }
    if (rxFreqInput) rxFreqInput.value = rx;
    if (txFreqInput) txFreqInput.value = tx;
    rxInputDirty = false;
    txInputDirty = false;
    rxInputEditedAtMs = 0;
    txInputEditedAtMs = 0;
    await sendCommand({type:'set_rx_frequency', rx_frequency_hz:rx});
    await sendCommand({type:'set_tx_frequency', tx_frequency_hz:tx});
    if (!silent) {
      setActionStatus(`${uiText.presetLabel} ${key}: Rx ${rx} / Tx ${tx}`, false);
    }
    return true;
  }

  loadSavedAuth();
  loadModeFrequencyPresets();
  if (loginUser) loginUser.value = authUser || 'admin';
  if (loginToken && authToken) loginToken.value = authToken;
  applyLocalization();
  updateModePresetSelect();
  if (rxFreqInput) {
    const markRxEdited = () => {
      rxInputDirty = true;
      rxInputEditedAtMs = Date.now();
    };
    rxFreqInput.addEventListener('input', markRxEdited);
    rxFreqInput.addEventListener('change', markRxEdited);
  }
  if (txFreqInput) {
    const markTxEdited = () => {
      txInputDirty = true;
      txInputEditedAtMs = Date.now();
    };
    txFreqInput.addEventListener('input', markTxEdited);
    txFreqInput.addEventListener('change', markTxEdited);
  }

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
    const upperMessage = message.toUpperCase();
    const tokens = upperMessage.split(/[^A-Z0-9\/]+/).filter(Boolean);
    const isCqRow = /^(?:CQ(?:_[A-Z0-9]+)?|QRZ)\b/.test(upperMessage);
    const is73Row = tokens.includes('73') || tokens.includes('RR73');
    let call = '';
    for (const tok of tokens) {
      if (blacklist.has(tok)) continue;
      if (/^[A-Z0-9\/]{3,}$/.test(tok) && !/^\d+$/.test(tok)) {
        call = tok;
        break;
      }
    }
    return {utc, db, dt, freq, message, call, grid, isTxRow, isCqRow, is73Row, arrivedMs};
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
      if (!row.sep) {
        if (activityHideCq && row.isCqRow) continue;
        if (activityHide73 && row.is73Row) continue;
      }
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
    btnWfToggle.textContent = waterfallEnabled ? uiText.waterfallOn : uiText.waterfallOff;
  }

  function applyAutoCqButtonState() {
    if (!btnAutoCQ) return;
    btnAutoCQ.classList.toggle('active', autoCqEnabled);
    btnAutoCQ.textContent = autoCqEnabled ? uiText.autoCqOn : uiText.autoCqOff;
  }

  function applyAutoSpotButtonState() {
    if (!btnAutoSpot) return;
    btnAutoSpot.classList.toggle('active', autoSpotEnabled);
    btnAutoSpot.textContent = autoSpotEnabled ? uiText.autoSpotOn : uiText.autoSpotOff;
  }

  function applyMonitoringButtonState() {
    if (!btnMonitor) return;
    btnMonitor.classList.toggle('active', monitoringState);
    btnMonitor.textContent = monitoringState ? uiText.monitoringOn : uiText.monitoringOff;
  }

  function applyActivityFilterButtonsState() {
    if (btnFilterCq) {
      btnFilterCq.classList.toggle('active', activityHideCq);
      btnFilterCq.textContent = activityHideCq ? uiText.cqHidden : uiText.hideCq;
    }
    if (btnFilter73) {
      btnFilter73.classList.toggle('active', activityHide73);
      btnFilter73.textContent = activityHide73 ? uiText.d73Hidden : uiText.hide73;
    }
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
      btnAsyncL2.textContent = asyncL2Enabled ? uiText.asyncL2On : uiText.asyncL2Off;
    }
    if (btnDualCarrier) {
      btnDualCarrier.classList.toggle('active', dualCarrierEnabled);
      btnDualCarrier.textContent = dualCarrierEnabled ? uiText.dualCarrierOn : uiText.dualCarrierOff;
    }
    if (btnAlt12) {
      btnAlt12.classList.toggle('active', alt12Enabled);
      btnAlt12.textContent = alt12Enabled ? uiText.alt12On : uiText.alt12Off;
    }
  }

  function applyFt2ControlsState() {
    if (chkManualTx) chkManualTx.checked = !!manualTxEnabled;
    if (chkSpeedy) chkSpeedy.checked = !!speedyContestEnabled;
    if (chkDigitalMorse) chkDigitalMorse.checked = !!digitalMorseEnabled;
    if (btnQuickQso) {
      btnQuickQso.classList.toggle('active', !!quickQsoEnabled);
      btnQuickQso.textContent = quickQsoEnabled ? uiText.quickQsoOn : uiText.quickQsoOff;
    }
    if (ft2QsoCount) {
      ft2QsoCount.value = String(ft2QsoMessageCount || 5);
    }
    if (asyncCard) {
      const isFt2 = (currentMode || '').toUpperCase() === 'FT2';
      const active = isFt2 && !!asyncL2Enabled;
      asyncCard.classList.toggle('active', active);
      asyncCard.classList.toggle('idle', !active);
    }
    if (asyncCardMeta) {
      const isFt2 = (currentMode || '').toUpperCase() === 'FT2';
      if (!isFt2) {
        asyncCardMeta.textContent = uiText.ft2Only;
      } else if (asyncSnrDb > -99) {
        asyncCardMeta.textContent = `${asyncSnrDb} dB`;
      } else if (transmittingState) {
        asyncCardMeta.textContent = uiText.txRunning;
      } else if (monitoringState) {
        asyncCardMeta.textContent = asyncL2Enabled ? uiText.asyncL2On : uiText.rxIdle;
      } else {
        asyncCardMeta.textContent = asyncL2Enabled ? uiText.asyncReady : '--- dB';
      }
    }
  }

  function updateAutoCqState(enabled) {
    autoCqEnabled = !!enabled;
    applyAutoCqButtonState();
  }

  function updateAutoSpotState(enabled) {
    autoSpotEnabled = !!enabled;
    applyAutoSpotButtonState();
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
    if (typeof s.manual_tx_enabled === 'boolean') manualTxEnabled = s.manual_tx_enabled;
    if (typeof s.speedy_contest_enabled === 'boolean') speedyContestEnabled = s.speedy_contest_enabled;
    if (typeof s.digital_morse_enabled === 'boolean') digitalMorseEnabled = s.digital_morse_enabled;
    if (typeof s.quick_qso_enabled === 'boolean') quickQsoEnabled = s.quick_qso_enabled;
    if (typeof s.ft2_qso_message_count === 'number') ft2QsoMessageCount = Number(s.ft2_qso_message_count) || 5;
    applyEmissionControlVisibility();
    applyEmissionButtonsState();
    applyFt2ControlsState();
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
  applyMonitoringButtonState();
  applyTxButtonsState();
  applyEmissionControlVisibility();
  applyEmissionButtonsState();
  applyActivityFilterButtonsState();
  applyFt2ControlsState();

  function setConnectionState(isConnected, text) {
    conn.textContent = text || (isConnected ? uiText.connected : uiText.disconnected);
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
      return `${uiText.initError}: ${uiText.backendUnreachable} (${host}${port})`;
    }
    return `${uiText.initError}: ${message || 'unknown error'}`;
  }

  function updateTxBadge(transmitting, txEnabled) {
    if (!trxLive) return;
    const isTx = !!transmitting;
    trxLive.classList.toggle('tx', isTx);
    trxLive.classList.toggle('rx', !isTx);
    trxLive.textContent = isTx ? uiText.txOnAir : (txEnabled ? uiText.rxTxArm : uiText.rxOnly);
  }

  function updateTxPeer(transmitting, dxCall) {
    if (!trxPeer) return;
    const peer = (dxCall || txPeerFromRows || '').trim();
    if (transmitting && peer) {
      lastTxPeer = peer;
    }
    trxPeer.classList.toggle('onair', !!transmitting);
    if (transmitting) {
      trxPeer.textContent = peer ? (`${uiText.txWith} ${peer}`) : `${uiText.txWith} -`;
    } else {
      trxPeer.textContent = lastTxPeer ? (`${uiText.lastTx}: ${lastTxPeer}`) : `${uiText.dxPrefix}: -`;
    }
  }

  function appendTxEvent(transmitting, peer = '') {
    const now = Date.now();
    activityRows.push({
      sep: true,
      txEvent: true,
      text: transmitting ? (`${uiText.txStarted}${peer ? ' -> ' + peer : ''}`) : uiText.txEndedRx,
      arrivedMs: now
    });
    trimActivityRows();
    renderActivity();
  }

  function setActionStatus(message, isError = false) {
    if (!actionStatus) return;
    const txt = (message || '').toString().trim();
    actionStatus.textContent = txt || uiText.ready;
    actionStatus.classList.toggle('ok', !!txt && !isError);
    actionStatus.classList.toggle('err', !!txt && isError);
  }

  function commandErrorMessage(err) {
    if (!err) return uiText.commandFailed;
    if (typeof err === 'string') return err;
    const msg = (err.message || '').toString().trim();
    if (msg) return msg;
    if (err.payload && typeof err.payload === 'object') {
      const p = err.payload;
      const status = (p.status || '').toString().trim();
      const reason = (p.reason || p.error || '').toString().trim();
      if (status && reason) return `${status}: ${reason}`;
      if (status) return status;
      if (reason) return reason;
    }
    return uiText.commandFailed;
  }

  function isAuthErrorMessage(message) {
    const t = (message || '').toString().toLowerCase();
    return t.includes('not authorized')
      || t.includes('not_authorized')
      || t.includes('auth')
      || t.includes('token missing')
      || t.includes('token mismatch')
      || t.includes('username mismatch');
  }

  function handleCommandError(err) {
    const message = commandErrorMessage(err);
    if (isAuthErrorMessage(message)) {
      wsAuthed = false;
      clearAuth();
      stopStatePolling();
      showLogin(true, uiText.invalidCredentials);
      return;
    }
    setActionStatus(message, true);
  }

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
          setConnectionState(true, uiText.connectedPoll);
        }
      } catch {
        if (!wsIsUsable()) {
          setConnectionState(false, uiText.disconnected);
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
      setInstallHint(uiText.installHintPrompt, true);
      return;
    }

    btnInstall.classList.add('hidden');
    if (isIOS()) {
      setInstallHint(uiText.installHintIos, true);
    } else if (!secureEligible) {
      setInstallHint(uiText.installHintInsecure, true);
    } else {
      setInstallHint(uiText.installHintAndroid, true);
    }
  }

  function initPwaSupport() {
    if ('serviceWorker' in navigator) {
      window.addEventListener('load', () => {
        navigator.serviceWorker.register(`/sw.js?v=${encodeURIComponent(SW_VERSION)}`).catch(() => {});
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
    if (typeof health.ui_language === 'string' && health.ui_language.trim()) {
      setAppUiLanguage(health.ui_language);
    }
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
    const incomingBand = String(s.band || '').trim();
    if (incomingBand) activeBand = incomingBand;

    const nowMs = Date.now();
    const pendingGuardActive = pendingFreqSetDeadlineMs > nowMs;
    const reportedRxHz = (typeof s.rx_frequency_hz === 'number') ? Number(s.rx_frequency_hz) : null;
    const reportedTxHz = (typeof s.tx_frequency_hz === 'number') ? Number(s.tx_frequency_hz) : null;

    if (reportedRxHz !== null) {
      const keepPendingRx = pendingRxHz !== null && pendingGuardActive && reportedRxHz !== pendingRxHz;
      if (!keepPendingRx) {
        currentRxHz = reportedRxHz;
        if (pendingRxHz !== null && reportedRxHz === pendingRxHz) {
          pendingRxHz = null;
        }
      }
    }
    if (reportedTxHz !== null) {
      const keepPendingTx = pendingTxHz !== null && pendingGuardActive && reportedTxHz !== pendingTxHz;
      if (!keepPendingTx) {
        currentTxHz = reportedTxHz;
        if (pendingTxHz !== null && reportedTxHz === pendingTxHz) {
          pendingTxHz = null;
        }
      }
    }
    if (!pendingGuardActive) {
      pendingRxHz = null;
      pendingTxHz = null;
      pendingFreqSetDeadlineMs = 0;
    }

    const displayRxHz = (pendingGuardActive && pendingRxHz !== null) ? pendingRxHz : currentRxHz;
    const displayTxHz = (pendingGuardActive && pendingTxHz !== null) ? pendingTxHz : currentTxHz;

    updateModePresetSelect();
    if (typeof s.ui_language === 'string' && s.ui_language.trim()) {
      setAppUiLanguage(s.ui_language);
    }
    set('st_mode', s.mode);
    set('st_band', incomingBand || activeBand || '-');
    set('st_dial', fmtHz(s.dial_frequency_hz) + ' Hz');
    set('st_rx', fmtHz(displayRxHz) + ' Hz');
    set('st_tx', fmtHz(displayTxHz) + ' Hz');
    if (typeof s.tx_enabled === 'boolean') {
      updateTxEnabledState(s.tx_enabled);
    }
    set('st_txen', boolText(!!s.tx_enabled));
    set('st_mon', boolText(!!s.monitoring));
    set('st_trx', boolText(!!s.transmitting));
    asyncSnrDb = (typeof s.async_snr_db === 'number') ? Math.round(Number(s.async_snr_db)) : -99;
    monitoringState = !!s.monitoring;
    transmittingState = !!s.transmitting;
    applyMonitoringButtonState();
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
    const rxFocused = !!rxFreqInput && document.activeElement === rxFreqInput;
    const txFocused = !!txFreqInput && document.activeElement === txFreqInput;
    const rxRecentlyEdited = rxInputDirty && (nowMs - rxInputEditedAtMs) < INPUT_EDIT_GRACE_MS;
    const txRecentlyEdited = txInputDirty && (nowMs - txInputEditedAtMs) < INPUT_EDIT_GRACE_MS;
    if (Number.isFinite(displayRxHz) && rxFreqInput && !rxFocused && !rxRecentlyEdited) {
      rxFreqInput.value = displayRxHz;
      rxInputDirty = false;
    }
    if (Number.isFinite(displayTxHz) && txFreqInput && !txFocused && !txRecentlyEdited) {
      txFreqInput.value = displayTxHz;
      txInputDirty = false;
    }
    if (typeof s.auto_cq_enabled === 'boolean') {
      updateAutoCqState(s.auto_cq_enabled);
    }
    if (typeof s.auto_spot_enabled === 'boolean') {
      updateAutoSpotState(s.auto_spot_enabled);
    }
    if (typeof s.waterfall_enabled === 'boolean') {
      updateWaterfallState(s.waterfall_enabled);
    }
    updateModeSpecificControlState(s);
    applyFt2ControlsState();
    drawWaterfallOverlay();
    refreshButtonHighlights();
  }

  async function sendCommand(payload) {
    payload.command_id = payload.command_id || ('cmd-' + Date.now() + '-' + Math.floor(Math.random()*100000));
    payload.client_sent_ms = Date.now();
    if (requiresAuth && !authToken) throw new Error('token missing');
    const headers = buildHeaders({'Content-Type':'application/json'});
    const r = await fetch('/api/v1/commands', {method:'POST', headers, body:JSON.stringify(payload)});
    let j = {};
    try {
      j = await r.json();
    } catch {
      throw new Error('invalid server response');
    }
    const status = (j.status || '').toString();
    const type = (j.type || payload.type || 'command').toString();
    const isErr = status.startsWith('rejected') || !!j.error;
    const reason = (j.reason || j.error || '').toString().trim();
    const statusText = status || (isErr ? 'error' : 'ok');
    const detail = reason ? `${type}: ${statusText} (${reason})` : `${type}: ${statusText}`;
    setActionStatus(detail, isErr);
    if (isErr) {
      const err = new Error(detail);
      err.payload = j;
      throw err;
    }
    if (type === 'set_waterfall_enabled' && typeof j.enabled === 'boolean') {
      updateWaterfallState(j.enabled);
    }
    if (type === 'set_auto_cq' && typeof j.enabled === 'boolean') {
      updateAutoCqState(j.enabled);
    }
    if (type === 'set_auto_spot' && typeof j.enabled === 'boolean') {
      updateAutoSpotState(j.enabled);
    }
    if (type === 'set_monitoring' && typeof j.enabled === 'boolean') {
      monitoringState = !!j.enabled;
      set('st_mon', boolText(monitoringState));
      applyMonitoringButtonState();
      applyFt2ControlsState();
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
    if (type === 'set_manual_tx' && typeof j.enabled === 'boolean') {
      manualTxEnabled = !!j.enabled;
      applyFt2ControlsState();
    }
    if (type === 'set_speedy_contest' && typeof j.enabled === 'boolean') {
      speedyContestEnabled = !!j.enabled;
      applyFt2ControlsState();
    }
    if (type === 'set_digital_morse' && typeof j.enabled === 'boolean') {
      digitalMorseEnabled = !!j.enabled;
      applyFt2ControlsState();
    }
    if (type === 'set_quick_qso' && typeof j.enabled === 'boolean') {
      quickQsoEnabled = !!j.enabled;
      if (quickQsoEnabled) ft2QsoMessageCount = 2;
      applyFt2ControlsState();
    }
    if (type === 'set_ft2_qso_msg_count' && typeof j.count === 'number') {
      ft2QsoMessageCount = Number(j.count) || 5;
      quickQsoEnabled = ft2QsoMessageCount === 2;
      applyFt2ControlsState();
    }
    if (type === 'set_rx_frequency' && typeof j.rx_frequency_hz === 'number') {
      currentRxHz = Number(j.rx_frequency_hz);
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
        setConnectionState(false, uiText.authenticating);
        ws.send(JSON.stringify({type:'auth', username:authUser, token:authToken}));
      } else {
        setConnectionState(true, uiText.connected);
      }
      refreshWaterfallPoller();
    };
    ws.onclose = () => {
      setConnectionState(false, uiText.disconnected);
      refreshWaterfallPoller();
      if (!loginOverlay || loginOverlay.classList.contains('hidden')) {
        setTimeout(connectWs, 1500);
      }
    };
    ws.onerror = () => {
      setConnectionState(false, uiText.wsError);
      refreshWaterfallPoller();
    };
    ws.onmessage = (ev) => {
      let m = {};
      try { m = JSON.parse(ev.data); } catch { return; }
      if (m.event === 'hello' && typeof m.requires_auth === 'boolean') {
        requiresAuth = !!m.requires_auth;
        if (typeof m.ui_language === 'string' && m.ui_language.trim()) {
          setAppUiLanguage(m.ui_language);
        }
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
        if (typeof m.auto_spot_enabled === 'boolean') {
          updateAutoSpotState(m.auto_spot_enabled);
        }
        if (typeof m.monitoring === 'boolean') {
          monitoringState = !!m.monitoring;
          applyMonitoringButtonState();
        }
        if (typeof m.async_snr_db === 'number') {
          asyncSnrDb = Math.round(Number(m.async_snr_db));
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
        setConnectionState(true, uiText.connected);
        getState(false).catch(() => {});
        refreshWaterfallPoller();
      } else if (m.event === 'auth_failed') {
        wsAuthed = false;
        setConnectionState(false, uiText.authFailed);
        stopStatePolling();
        clearAuth();
        showLogin(true, uiText.invalidCredentials);
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
        if (type === 'set_auto_spot' && typeof m.enabled === 'boolean') {
          updateAutoSpotState(m.enabled);
        }
        if (type === 'set_monitoring' && typeof m.enabled === 'boolean') {
          monitoringState = !!m.enabled;
          set('st_mon', boolText(monitoringState));
          applyMonitoringButtonState();
          applyFt2ControlsState();
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
        if (type === 'set_manual_tx' && typeof m.enabled === 'boolean') {
          manualTxEnabled = !!m.enabled;
          applyFt2ControlsState();
        }
        if (type === 'set_speedy_contest' && typeof m.enabled === 'boolean') {
          speedyContestEnabled = !!m.enabled;
          applyFt2ControlsState();
        }
        if (type === 'set_digital_morse' && typeof m.enabled === 'boolean') {
          digitalMorseEnabled = !!m.enabled;
          applyFt2ControlsState();
        }
        if (type === 'set_quick_qso' && typeof m.enabled === 'boolean') {
          quickQsoEnabled = !!m.enabled;
          if (quickQsoEnabled) ft2QsoMessageCount = 2;
          applyFt2ControlsState();
        }
        if (type === 'set_ft2_qso_msg_count' && typeof m.count === 'number') {
          ft2QsoMessageCount = Number(m.count) || 5;
          quickQsoEnabled = ft2QsoMessageCount === 2;
          applyFt2ControlsState();
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
    const rx = sanitizeFrequency(rxFreqInput ? rxFreqInput.value : null);
    if (rx === null) return;
    pendingRxHz = rx;
    pendingFreqSetDeadlineMs = Date.now() + PENDING_FREQ_GRACE_MS;
    currentRxHz = rx;
    if (rxFreqInput) rxFreqInput.value = rx;
    rxInputDirty = false;
    rxInputEditedAtMs = 0;
    sendCommand({type:'set_rx_frequency', rx_frequency_hz:rx})
      .catch((e) => {
        pendingRxHz = null;
        pendingFreqSetDeadlineMs = 0;
        handleCommandError(e);
      });
  };
  if (btnTxFreq) {
    btnTxFreq.onclick = () => {
      const tx = sanitizeFrequency(txFreqInput ? txFreqInput.value : null);
      if (tx === null) return;
      pendingTxHz = tx;
      pendingFreqSetDeadlineMs = Date.now() + PENDING_FREQ_GRACE_MS;
      currentTxHz = tx;
      if (txFreqInput) txFreqInput.value = tx;
      txInputDirty = false;
      txInputEditedAtMs = 0;
      sendCommand({type:'set_tx_frequency', tx_frequency_hz:tx})
        .catch((e) => {
          pendingTxHz = null;
          pendingFreqSetDeadlineMs = 0;
          handleCommandError(e);
        });
    };
  }
  if (btnSetRxTx) {
    btnSetRxTx.onclick = async () => {
      const rx = sanitizeFrequency(rxFreqInput ? rxFreqInput.value : null);
      const tx = sanitizeFrequency(txFreqInput ? txFreqInput.value : null);
      if (rx === null || tx === null) return;
      pendingRxHz = rx;
      pendingTxHz = tx;
      pendingFreqSetDeadlineMs = Date.now() + PENDING_FREQ_GRACE_MS;
      currentRxHz = rx;
      currentTxHz = tx;
      if (rxFreqInput) rxFreqInput.value = rx;
      if (txFreqInput) txFreqInput.value = tx;
      rxInputDirty = false;
      txInputDirty = false;
      rxInputEditedAtMs = 0;
      txInputEditedAtMs = 0;
      try {
        await sendCommand({type:'set_rx_frequency', rx_frequency_hz:rx});
        await sendCommand({type:'set_tx_frequency', tx_frequency_hz:tx});
        setActionStatus(`${uiText.rxTxSet}: ${rx}/${tx} Hz`, false);
      } catch (e) {
        pendingRxHz = null;
        pendingTxHz = null;
        pendingFreqSetDeadlineMs = 0;
        handleCommandError(e);
      }
    };
  }
  if (btnSaveModePreset) {
    btnSaveModePreset.onclick = () => {
      const mode = getCurrentPresetMode();
      const rx = sanitizeFrequency((rxFreqInput ? rxFreqInput.value : null) || currentRxHz);
      const tx = sanitizeFrequency(txFreqInput ? (txFreqInput.value || currentTxHz) : currentTxHz);
      if (rx === null || tx === null) return;
      if (savePresetForMode(mode, rx, tx)) {
        setActionStatus(`${uiText.presetSaved}: ${mode} (${rx}/${tx})`, false);
      }
    };
  }
  if (btnApplyModePreset) {
    btnApplyModePreset.onclick = () => {
      applyPresetForMode(getCurrentPresetMode(), false).catch(handleCommandError);
    };
  }
  if (btnTxOn) {
    btnTxOn.onclick = () => sendCommand({type:'set_tx_enabled', enabled:true}).catch(handleCommandError);
  }
  if (btnTxOff) {
    btnTxOff.onclick = () => sendCommand({type:'set_tx_enabled', enabled:false}).catch(handleCommandError);
  }
  if (btnAutoCQ) {
    btnAutoCQ.onclick = () => sendCommand({type:'set_auto_cq', enabled:!autoCqEnabled}).catch(handleCommandError);
  }
  if (btnAutoSpot) {
    btnAutoSpot.onclick = () => sendCommand({type:'set_auto_spot', enabled:!autoSpotEnabled}).catch(handleCommandError);
  }
  if (btnMonitor) {
    btnMonitor.onclick = () => sendCommand({type:'set_monitoring', enabled:!monitoringState}).catch(handleCommandError);
  }
  if (btnAsyncL2) {
    btnAsyncL2.onclick = () => sendCommand({type:'set_async_l2', enabled:!asyncL2Enabled}).catch(handleCommandError);
  }
  if (btnDualCarrier) {
    btnDualCarrier.onclick = () => sendCommand({type:'set_dual_carrier', enabled:!dualCarrierEnabled}).catch(handleCommandError);
  }
  if (btnAlt12) {
    btnAlt12.onclick = () => sendCommand({type:'set_alt_12', enabled:!alt12Enabled}).catch(handleCommandError);
  }
  if (chkManualTx) {
    chkManualTx.onchange = () => sendCommand({type:'set_manual_tx', enabled:!!chkManualTx.checked}).catch(handleCommandError);
  }
  if (chkSpeedy) {
    chkSpeedy.onchange = () => sendCommand({type:'set_speedy_contest', enabled:!!chkSpeedy.checked}).catch(handleCommandError);
  }
  if (chkDigitalMorse) {
    chkDigitalMorse.onchange = () => sendCommand({type:'set_digital_morse', enabled:!!chkDigitalMorse.checked}).catch(handleCommandError);
  }
  if (btnQuickQso) {
    btnQuickQso.onclick = () => sendCommand({type:'set_quick_qso', enabled:!quickQsoEnabled}).catch(handleCommandError);
  }
  if (ft2QsoCount) {
    ft2QsoCount.onchange = () => {
      const count = Number(ft2QsoCount.value || 5);
      sendCommand({type:'set_ft2_qso_msg_count', count}).catch(handleCommandError);
    };
  }
  el('btn_call').onclick = () => sendCommand({type:'select_caller', target_call:el('caller_call').value, target_grid:el('caller_grid').value}).catch(handleCommandError);
  if (btnWfToggle) {
    btnWfToggle.onclick = () => sendCommand({type:'set_waterfall_enabled', enabled:!waterfallEnabled}).catch(handleCommandError);
  }
  if (wfCanvas) {
    wfCanvas.addEventListener('click', (ev) => {
      if (!waterfallMeta.width || waterfallMeta.spanHz <= 0) return;
      const rect = wfCanvas.getBoundingClientRect();
      const relX = Math.max(0, Math.min(rect.width, ev.clientX - rect.left));
      const ratio = rect.width > 0 ? relX / rect.width : 0;
      const rxHz = Math.round(waterfallMeta.startHz + ratio * waterfallMeta.spanHz);
      pendingRxHz = rxHz;
      pendingFreqSetDeadlineMs = Date.now() + PENDING_FREQ_GRACE_MS;
      drawWaterfallOverlay();
      sendCommand({type:'set_rx_frequency', rx_frequency_hz:rxHz}).catch((e) => {
        pendingRxHz = null;
        pendingFreqSetDeadlineMs = 0;
        drawWaterfallOverlay();
        handleCommandError(e);
      });
    });
  }
  if (btnFilterCq) {
    btnFilterCq.onclick = () => {
      activityHideCq = !activityHideCq;
      applyActivityFilterButtonsState();
      renderActivity();
    };
  }
  if (btnFilter73) {
    btnFilter73.onclick = () => {
      activityHide73 = !activityHide73;
      applyActivityFilterButtonsState();
      renderActivity();
    };
  }
  document.querySelectorAll('.mode-btn').forEach((b) => {
    b.addEventListener('click', () => sendCommand({type:'set_mode', mode:b.dataset.mode}).catch(handleCommandError));
  });
  document.querySelectorAll('.band-btn').forEach((b) => {
    b.addEventListener('click', () => sendCommand({type:'set_band', band:b.dataset.band}).catch(handleCommandError));
  });

  if (btnActivityPause) {
    btnActivityPause.addEventListener('click', () => {
      activityPaused = !activityPaused;
      btnActivityPause.classList.toggle('active', activityPaused);
      btnActivityPause.textContent = activityPaused ? uiText.resume : uiText.pause;
      setActionStatus(activityPaused ? uiText.bandActivityPaused : uiText.bandActivityResumed, false);
    });
  }
  if (btnActivityClear) {
    btnActivityClear.addEventListener('click', () => {
      activityRows.length = 0;
      renderActivity();
      setActionStatus(uiText.bandActivityCleared, false);
    });
  }

  async function attemptUnlock() {
    authUser = normalizeAuthUser(loginUser ? loginUser.value : '');
    authToken = normalizeAuthToken(loginToken ? loginToken.value : '');
    if (!authToken) {
      showLogin(true, uiText.passwordRequired);
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
      showLogin(true, uiText.invalidCredentials);
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
          setConnectionState(false, uiText.locked);
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
        setConnectionState(false, uiText.locked);
        showLogin(true);
      } else {
        setConnectionState(false, describeInitError(e));
      }
    }
  })();
})();
)FT2JS").arg(fork_release_version()).toUtf8();
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
  return QStringLiteral(
R"FT2SW(const CACHE_NAME = 'decodium-remote-shell-%1';
const APP_SHELL = [
  '/',
  '/index.html',
  '/app.css',
  '/app.js',
  '/manifest.webmanifest',
  '/icon-192.png',
  '/icon-512.png'
];
const APP_SHELL_SET = new Set(APP_SHELL);

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

  if (APP_SHELL_SET.has(url.pathname)) {
    event.respondWith(
      fetch(req)
        .then((response) => {
          if (response && response.status === 200) {
            const copy = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(req, copy));
          }
          return response;
        })
        .catch(() => caches.match(req))
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
)FT2SW").arg(fork_release_version()).toUtf8();
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
  auto const token = authToken_.trimmed();
  if (remoteExposed)
    {
      if (token.isEmpty())
        {
          Q_EMIT logMessage(QStringLiteral("Remote WS disabled: non-loopback bind requires token authentication."));
          return false;
        }
      else if (token.size() < 12)
        {
          Q_EMIT logMessage(QStringLiteral("Remote WS disabled: token must be at least 12 characters on LAN/WAN bind."));
          return false;
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

      if (!isAllowedWebSocketOrigin(client))
        {
          auto const origin = client->origin().trimmed();
          Q_EMIT logMessage(QString {"Remote WS rejected origin \"%1\""}
                              .arg(origin.isEmpty() ? QStringLiteral("<empty>") : origin));
          client->close();
          client->deleteLater();
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
        {"ui_language", rt.uiLanguage},
        {"tx_enabled", rt.txEnabled},
        {"monitoring", rt.monitoring},
        {"transmitting", rt.transmitting},
        {"async_l2_enabled", rt.asyncL2Enabled},
        {"async_snr_db", rt.asyncSnrDb},
        {"dual_carrier_enabled", rt.dualCarrierEnabled},
        {"alt_12_enabled", rt.alt12Enabled},
        {"manual_tx_enabled", rt.manualTxEnabled},
        {"speedy_contest_enabled", rt.speedyContestEnabled},
        {"digital_morse_enabled", rt.digitalMorseEnabled},
        {"quick_qso_enabled", rt.quickQsoEnabled},
        {"ft2_qso_message_count", rt.ft2QsoMessageCount},
        {"auto_cq_enabled", rt.autoCqEnabled},
        {"auto_spot_enabled", rt.autoSpotEnabled},
        {"waterfall_enabled", waterfallEnabled_},
      };
      sendJson(client, hello);
    }

  Q_EMIT logMessage(QString {"Remote WS client connected (%1 clients)"}.arg(clients_.size()));
}

bool RemoteCommandServer::isAllowedWebSocketOrigin(QWebSocket const* client) const
{
  if (!client)
    {
      return false;
    }

  bool const loopbackOnly = bindAddress_.isLoopback()
                            || bindAddress_ == QHostAddress::LocalHost
                            || bindAddress_ == QHostAddress::LocalHostIPv6;

  auto const originText = client->origin().trimmed();
  if (originText.isEmpty())
    {
      return loopbackOnly;
    }

  QUrl const originUrl {originText};
  if (!originUrl.isValid())
    {
      return false;
    }

  auto const scheme = originUrl.scheme().trimmed().toLower();
  if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
    {
      return false;
    }

  int const defaultPort = (scheme == QStringLiteral("https")) ? 443 : 80;
  int const originPort = originUrl.port(defaultPort);
  int const expectedPort = httpPort_ > 0 ? static_cast<int>(httpPort_) : static_cast<int>(wsPort_);
  if (originPort != expectedPort)
    {
      return false;
    }

  auto const originHost = originUrl.host().trimmed().toLower();
  if (originHost.isEmpty())
    {
      return false;
    }

  if (loopbackOnly)
    {
      return is_loopback_origin_host(originHost);
    }

  if (!(bindAddress_ == QHostAddress::Any
        || bindAddress_ == QHostAddress::AnyIPv4
        || bindAddress_ == QHostAddress::AnyIPv6))
    {
      return originHost == bindAddress_.toString().trimmed().toLower();
    }

  if (is_loopback_origin_host(originHost))
    {
      return true;
    }

  QHostAddress parsedOriginHost;
  if (parsedOriginHost.setAddress(originHost))
    {
      return true;
    }

  auto const localHostName = QHostInfo::localHostName().trimmed().toLower();
  return !localHostName.isEmpty() && originHost == localHostName;
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

  if (commandType == QStringLiteral("set_auto_spot"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setAutoSpotRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_auto_spot")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_monitoring"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      if (enabled && state.transmitting)
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("cannot enable monitoring while transmitting"));
          return result;
        }
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setMonitoringRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_monitoring")},
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

  if (commandType == QStringLiteral("set_manual_tx"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("Manual TX is available only in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setManualTxRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_manual_tx")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_speedy_contest"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("Speedy is available only in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setSpeedyContestRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_speedy_contest")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_digital_morse"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("D-CW is available only in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setDigitalMorseRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_digital_morse")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_quick_qso"))
    {
      if (!object.contains(QStringLiteral("enabled")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("enabled is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("Quick QSO is available only in FT2"));
          return result;
        }
      auto enabled = object.value(QStringLiteral("enabled")).toBool(false);
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setQuickQsoRequested(commandId, enabled);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_quick_qso")},
        {"status", QStringLiteral("accepted_immediate")},
        {"enabled", enabled},
        {"server_now_ms", nowUtcMs},
      };
      return result;
    }

  if (commandType == QStringLiteral("set_ft2_qso_msg_count"))
    {
      if (!object.contains(QStringLiteral("count")))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("count is required"));
          return result;
        }
      auto const mode = state.mode.trimmed().toUpper();
      if (mode != QStringLiteral("FT2"))
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_state"), QStringLiteral("FT2 QSO profile is available only in FT2"));
          return result;
        }
      auto const count = object.value(QStringLiteral("count")).toInt(0);
      if (count != 2 && count != 3 && count != 5)
        {
          result.payload = makeRejectPayload(commandId, QStringLiteral("rejected_invalid_request"), QStringLiteral("count must be 2, 3, or 5"));
          return result;
        }
      seenCommandIds_.insert(commandId, nowUtcMs);
      Q_EMIT setFt2QsoMessageCountRequested(commandId, count);
      result.accepted = true;
      result.payload = QJsonObject {
        {"event", QStringLiteral("command_ack")},
        {"command_id", commandId},
        {"type", QStringLiteral("set_ft2_qso_msg_count")},
        {"status", QStringLiteral("accepted_immediate")},
        {"count", count},
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
      response += "Cache-Control: no-store, max-age=0\r\n";
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
      response += "Cache-Control: no-store, max-age=0\r\n";
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
      response += "Cache-Control: no-store, max-age=0\r\n";
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
      response += "Cache-Control: no-store, max-age=0\r\n";
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
          health.insert(QStringLiteral("ui_language"), rt.uiLanguage);
          health.insert(QStringLiteral("dial_frequency_hz"), static_cast<double>(rt.dialFrequencyHz));
          health.insert(QStringLiteral("rx_frequency_hz"), rt.rxFrequencyHz);
          health.insert(QStringLiteral("tx_frequency_hz"), rt.txFrequencyHz);
          health.insert(QStringLiteral("period_ms"), static_cast<double>(rt.periodMs));
          health.insert(QStringLiteral("tx_enabled"), rt.txEnabled);
          health.insert(QStringLiteral("auto_cq_enabled"), rt.autoCqEnabled);
          health.insert(QStringLiteral("auto_spot_enabled"), rt.autoSpotEnabled);
          health.insert(QStringLiteral("async_l2_enabled"), rt.asyncL2Enabled);
          health.insert(QStringLiteral("dual_carrier_enabled"), rt.dualCarrierEnabled);
          health.insert(QStringLiteral("alt_12_enabled"), rt.alt12Enabled);
          health.insert(QStringLiteral("manual_tx_enabled"), rt.manualTxEnabled);
          health.insert(QStringLiteral("speedy_contest_enabled"), rt.speedyContestEnabled);
          health.insert(QStringLiteral("digital_morse_enabled"), rt.digitalMorseEnabled);
          health.insert(QStringLiteral("quick_qso_enabled"), rt.quickQsoEnabled);
          health.insert(QStringLiteral("ft2_qso_message_count"), rt.ft2QsoMessageCount);
          health.insert(QStringLiteral("async_snr_db"), rt.asyncSnrDb);
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
                      {"ui_language", rt.uiLanguage},
                      {"dial_frequency_hz", static_cast<double>(rt.dialFrequencyHz)},
                      {"rx_frequency_hz", rt.rxFrequencyHz},
                      {"tx_frequency_hz", rt.txFrequencyHz},
                      {"period_ms", static_cast<double>(periodMs)},
                      {"tx_enabled", rt.txEnabled},
                      {"auto_cq_enabled", rt.autoCqEnabled},
                      {"auto_spot_enabled", rt.autoSpotEnabled},
                      {"async_l2_enabled", rt.asyncL2Enabled},
                      {"dual_carrier_enabled", rt.dualCarrierEnabled},
                      {"alt_12_enabled", rt.alt12Enabled},
                      {"manual_tx_enabled", rt.manualTxEnabled},
                      {"speedy_contest_enabled", rt.speedyContestEnabled},
                      {"digital_morse_enabled", rt.digitalMorseEnabled},
                      {"quick_qso_enabled", rt.quickQsoEnabled},
                      {"ft2_qso_message_count", rt.ft2QsoMessageCount},
                      {"async_snr_db", rt.asyncSnrDb},
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
    {"ui_language", state.uiLanguage},
    {"dial_frequency_hz", static_cast<double>(state.dialFrequencyHz)},
    {"rx_frequency_hz", state.rxFrequencyHz},
    {"tx_frequency_hz", state.txFrequencyHz},
    {"period_ms", static_cast<double>(periodMs)},
    {"tx_enabled", state.txEnabled},
    {"auto_cq_enabled", state.autoCqEnabled},
    {"auto_spot_enabled", state.autoSpotEnabled},
    {"async_l2_enabled", state.asyncL2Enabled},
    {"dual_carrier_enabled", state.dualCarrierEnabled},
    {"alt_12_enabled", state.alt12Enabled},
    {"manual_tx_enabled", state.manualTxEnabled},
    {"speedy_contest_enabled", state.speedyContestEnabled},
    {"digital_morse_enabled", state.digitalMorseEnabled},
    {"quick_qso_enabled", state.quickQsoEnabled},
    {"ft2_qso_message_count", state.ft2QsoMessageCount},
    {"async_snr_db", state.asyncSnrDb},
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
