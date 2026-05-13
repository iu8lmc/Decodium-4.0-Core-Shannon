// -*- Mode: C++ -*-
#include "DecodiumWebServer.hpp"
#include "../DecodiumBridge.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QVariantMap>
#include <QWebSocketServer>
#include <QWebSocket>

namespace
{
    QString clientIpAddressString()
    {
        QString preferred;
        const auto ifaces = QNetworkInterface::allInterfaces();
        for (auto const& iface : ifaces) {
            if (!(iface.flags() & QNetworkInterface::IsRunning)) continue;
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            for (auto const& entry : iface.addressEntries()) {
                QHostAddress addr = entry.ip();
                if (addr.protocol() != QAbstractSocket::IPv4Protocol) continue;
                if (addr.isLoopback()) continue;
                QString s = addr.toString();
                // Preferenza per 192.168.x / 10.x / 172.16-31.x (LAN private)
                if (s.startsWith("192.168.") || s.startsWith("10.")) {
                    return s;
                }
                if (preferred.isEmpty()) preferred = s;
            }
        }
        return preferred;
    }
}

DecodiumWebServer::DecodiumWebServer(DecodiumBridge* bridge, QObject* parent)
    : QObject(parent)
    , m_bridge(bridge)
{}

DecodiumWebServer::~DecodiumWebServer()
{
    stop();
}

bool DecodiumWebServer::start(quint16 port)
{
    if (m_server && m_server->isListening() && m_port == port) {
        return true;  // gia' OK
    }
    stop();

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &DecodiumWebServer::onNewConnection);

    bool const ok = m_server->listen(QHostAddress::Any, port);
    if (!ok) {
        emit errorOccurred(QStringLiteral("WebServer listen failed on port %1: %2")
                               .arg(port).arg(m_server->errorString()));
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }
    m_port = port;

    // Fase 2 — WebSocket server sulla porta successiva per push real-time.
    m_wsServer = new QWebSocketServer(QStringLiteral("Decodium WS"),
                                       QWebSocketServer::NonSecureMode, this);
    if (m_wsServer->listen(QHostAddress::Any, quint16(port + 1))) {
        connect(m_wsServer, &QWebSocketServer::newConnection,
                this, &DecodiumWebServer::onWebSocketConnected);
    } else {
        emit errorOccurred(QStringLiteral("WS listen failed on port %1: %2")
                               .arg(port + 1).arg(m_wsServer->errorString()));
        m_wsServer->deleteLater();
        m_wsServer = nullptr;
    }

    emit runningChanged(true);
    return true;
}

void DecodiumWebServer::stop()
{
    if (m_wsServer) {
        for (QWebSocket* ws : m_wsClients) {
            if (ws) { ws->close(); ws->deleteLater(); }
        }
        m_wsClients.clear();
        m_wsServer->close();
        m_wsServer->deleteLater();
        m_wsServer = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    if (m_port != 0) {
        m_port = 0;
        emit runningChanged(false);
    }
}

void DecodiumWebServer::onWebSocketConnected()
{
    if (!m_wsServer) return;
    while (m_wsServer->hasPendingConnections()) {
        QWebSocket* ws = m_wsServer->nextPendingConnection();
        if (!ws) continue;
        connect(ws, &QWebSocket::disconnected,
                this, &DecodiumWebServer::onWebSocketDisconnected);
        // 1.0.171 fase 4: comandi remoti dal client (iPad/mobile)
        connect(ws, &QWebSocket::textMessageReceived,
                this, &DecodiumWebServer::onWebSocketTextMessage);
        m_wsClients.append(ws);
        // Snapshot iniziale: invia state + decodes appena connesso.
        QJsonObject env;
        env[QStringLiteral("type")] = QStringLiteral("state");
        env[QStringLiteral("payload")] =
            QJsonDocument::fromJson(buildStateJson()).object();
        ws->sendTextMessage(QString::fromUtf8(
            QJsonDocument(env).toJson(QJsonDocument::Compact)));
        QJsonObject env2;
        env2[QStringLiteral("type")] = QStringLiteral("decodes");
        env2[QStringLiteral("payload")] =
            QJsonDocument::fromJson(buildDecodesJson()).object();
        ws->sendTextMessage(QString::fromUtf8(
            QJsonDocument(env2).toJson(QJsonDocument::Compact)));
        emit clientCountChanged(m_wsClients.size());
    }
}

void DecodiumWebServer::onWebSocketDisconnected()
{
    QWebSocket* ws = qobject_cast<QWebSocket*>(sender());
    if (!ws) return;
    m_wsClients.removeAll(ws);
    ws->deleteLater();
    emit clientCountChanged(m_wsClients.size());
}

void DecodiumWebServer::broadcastStateUpdate()
{
    if (m_wsClients.isEmpty()) return;
    QJsonObject env;
    env[QStringLiteral("type")] = QStringLiteral("state");
    env[QStringLiteral("payload")] =
        QJsonDocument::fromJson(buildStateJson()).object();
    QString const msg = QString::fromUtf8(
        QJsonDocument(env).toJson(QJsonDocument::Compact));
    for (QWebSocket* ws : m_wsClients) {
        if (ws && ws->isValid()) ws->sendTextMessage(msg);
    }
}

void DecodiumWebServer::broadcastDecodesUpdate()
{
    if (m_wsClients.isEmpty()) return;
    QJsonObject env;
    env[QStringLiteral("type")] = QStringLiteral("decodes");
    env[QStringLiteral("payload")] =
        QJsonDocument::fromJson(buildDecodesJson()).object();
    QString const msg = QString::fromUtf8(
        QJsonDocument(env).toJson(QJsonDocument::Compact));
    for (QWebSocket* ws : m_wsClients) {
        if (ws && ws->isValid()) ws->sendTextMessage(msg);
    }
}

int DecodiumWebServer::connectedClients() const
{
    return m_wsClients.size();
}

void DecodiumWebServer::onWebSocketTextMessage(QString const& msg)
{
    // Fase 4 — comandi remoti TX/RX dal client (iPad PWA).
    // Protocollo: {type:"cmd", action:"<name>", payload:{...}}
    // Whitelist di azioni: rifiuto qualsiasi altra cosa per safety.
    if (!m_bridge) return;
    QJsonParseError err{};
    QJsonDocument const doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit errorOccurred(QStringLiteral("WS cmd parse: %1").arg(err.errorString()));
        return;
    }
    QJsonObject const env = doc.object();
    if (env.value(QStringLiteral("type")).toString() != QStringLiteral("cmd")) return;

    QString const action = env.value(QStringLiteral("action")).toString();
    QJsonObject const payload = env.value(QStringLiteral("payload")).toObject();

    // Audit log: ogni comando remoto e' loggato.
    // (qInfo finisce nel decodium_diagnostic.log via message handler)
    qInfo().noquote() << "[WS] cmd from client action=" << action
                      << "payload=" << QJsonDocument(payload).toJson(QJsonDocument::Compact);

    if (action == QStringLiteral("setRxFreq")) {
        int const hz = payload.value(QStringLiteral("hz")).toInt(0);
        if (hz > 0) m_bridge->setRxFrequency(hz);
    } else if (action == QStringLiteral("setTxFreq")) {
        int const hz = payload.value(QStringLiteral("hz")).toInt(0);
        if (hz > 0) m_bridge->setTxFrequency(hz);
    } else if (action == QStringLiteral("setDxCall")) {
        QString const call = payload.value(QStringLiteral("call")).toString().trimmed();
        m_bridge->setDxCall(call);
    } else if (action == QStringLiteral("setMonitoring")) {
        bool const on = payload.value(QStringLiteral("on")).toBool();
        m_bridge->setMonitoring(on);
    } else if (action == QStringLiteral("syncNtpNow")) {
        m_bridge->syncNtpNow();
    } else if (action == QStringLiteral("requestState")) {
        // Client requests snapshot — utile dopo reconnect
        QJsonObject envOut;
        envOut[QStringLiteral("type")] = QStringLiteral("state");
        envOut[QStringLiteral("payload")] =
            QJsonDocument::fromJson(buildStateJson()).object();
        QWebSocket* sender_ws = qobject_cast<QWebSocket*>(sender());
        if (sender_ws && sender_ws->isValid()) {
            sender_ws->sendTextMessage(QString::fromUtf8(
                QJsonDocument(envOut).toJson(QJsonDocument::Compact)));
        }
    } else {
        emit errorOccurred(QStringLiteral("WS unknown action: %1").arg(action));
    }
}

bool DecodiumWebServer::isRunning() const
{
    return m_server && m_server->isListening();
}

QString DecodiumWebServer::accessUrl() const
{
    if (!isRunning()) return QString();
    QString const ip = clientIpAddressString();
    if (ip.isEmpty()) return QString();
    return QStringLiteral("http://%1:%2/").arg(ip).arg(m_port);
}

void DecodiumWebServer::onNewConnection()
{
    if (!m_server) return;
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        if (!sock) continue;
        connect(sock, &QTcpSocket::readyRead,
                this, &DecodiumWebServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &DecodiumWebServer::onDisconnected);
    }
}

void DecodiumWebServer::onDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) sock->deleteLater();
}

void DecodiumWebServer::onReadyRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    QByteArray const raw = sock->readAll();
    QString const requestLine = QString::fromLatin1(raw.left(raw.indexOf('\n'))).trimmed();
    QStringList const parts = requestLine.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        writeHttpResponse(sock, 400, "text/plain", "Bad Request");
        return;
    }
    QString const method = parts.at(0);
    QString fullPath = parts.at(1);
    QString queryString;
    int const qpos = fullPath.indexOf('?');
    if (qpos >= 0) {
        queryString = fullPath.mid(qpos + 1);
        fullPath = fullPath.left(qpos);
    }
    handleRequest(sock, method, fullPath, queryString);
}

void DecodiumWebServer::handleRequest(QTcpSocket* socket, QString const& method,
                                       QString const& path, QString const& query)
{
    Q_UNUSED(query)
    if (method != QStringLiteral("GET")) {
        writeHttpResponse(socket, 405, "text/plain", "Method Not Allowed");
        return;
    }

    if (path == QStringLiteral("/") || path == QStringLiteral("/index.html")) {
        writeHttpResponse(socket, 200, "text/html; charset=utf-8", buildIndexHtml());
    } else if (path == QStringLiteral("/qr")) {
        // 1.0.170 fase 3 PWA — pagina QR per scan da iPad
        QString const url = accessUrl();
        QByteArray simpleQrHtml;
        simpleQrHtml += "<!DOCTYPE html><html lang=\"it\"><head><meta charset=\"utf-8\">"
               "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
               "<title>Decodium QR</title>"
               "<style>body{margin:0;padding:24px;background:#0e1014;color:#e8edf2;"
               "font-family:-apple-system,sans-serif;text-align:center;}"
               ".card{background:#161a21;border:1px solid rgba(58,142,220,0.5);"
               "border-radius:12px;padding:24px;max-width:380px;margin:24px auto;}"
               "h1{color:#3a8edc;font-size:18px;margin:0 0 16px;}"
               ".qr{margin:16px auto;background:white;padding:12px;width:300px;height:300px;display:flex;align-items:center;justify-content:center;}"
               ".qr canvas{width:280px;height:280px;}"
               ".url{font-family:Consolas,monospace;font-size:13px;color:#9aa4af;"
               "word-break:break-all;margin:12px 0;}"
               "p{color:#9aa4af;font-size:13px;line-height:1.5;}"
               "</style></head><body>"
               "<div class=\"card\">"
               "<h1>Scansiona da iPad / iPhone</h1>"
               "<div class=\"qr\"><canvas id=\"qrcanvas\" width=\"280\" height=\"280\"></canvas></div>";
        simpleQrHtml += "<div class=\"url\">" + url.toUtf8() + "</div>";
        simpleQrHtml += "<p>Apri la fotocamera iOS e inquadra il QR. Tocca il banner per aprire Decodium Remote in Safari, poi <b>Condividi &rarr; Aggiungi a Home Screen</b> per la PWA standalone.</p>"
               "</div>"
               "<script src=\"https://cdn.jsdelivr.net/npm/qrcode-generator@1.4.4/qrcode.min.js\"></script>"
               "<script>"
               "var url='" + url.toUtf8() + "';"
               "var qr=qrcode(0,'L');qr.addData(url);qr.make();"
               "var cnv=document.getElementById('qrcanvas');"
               "var ctx=cnv.getContext('2d');"
               "var n=qr.getModuleCount();var sz=280/n;"
               "ctx.fillStyle='white';ctx.fillRect(0,0,280,280);"
               "ctx.fillStyle='black';"
               "for(var r=0;r<n;r++)for(var c=0;c<n;c++)"
               "if(qr.isDark(r,c))ctx.fillRect(Math.floor(c*sz),Math.floor(r*sz),Math.ceil(sz),Math.ceil(sz));"
               "</script></body></html>";
        writeHttpResponse(socket, 200, "text/html; charset=utf-8", simpleQrHtml);
    } else if (path == QStringLiteral("/api/state")) {
        writeHttpResponse(socket, 200, "application/json; charset=utf-8", buildStateJson());
    } else if (path == QStringLiteral("/api/decodes")) {
        writeHttpResponse(socket, 200, "application/json; charset=utf-8", buildDecodesJson());
    } else if (path == QStringLiteral("/manifest.json")) {
        writeHttpResponse(socket, 200, "application/json; charset=utf-8", buildManifestJson());
    } else if (path == QStringLiteral("/favicon.ico")) {
        writeHttpResponse(socket, 200, "image/gif", buildFavicon());
    } else {
        writeHttpResponse(socket, 404, "text/plain", "Not Found");
    }
}

QByteArray DecodiumWebServer::buildStateJson() const
{
    QJsonObject root;
    if (!m_bridge) {
        root[QStringLiteral("error")] = QStringLiteral("bridge unavailable");
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }
    root[QStringLiteral("callsign")] = m_bridge->callsign();
    root[QStringLiteral("dxCall")]   = m_bridge->dxCall();
    root[QStringLiteral("mode")]     = m_bridge->mode();
    root[QStringLiteral("dialFrequency")] = m_bridge->frequency();
    root[QStringLiteral("rxFrequency")] = m_bridge->rxFrequency();
    root[QStringLiteral("txFrequency")] = m_bridge->txFrequency();
    root[QStringLiteral("monitoring")] = m_bridge->monitoring();
    root[QStringLiteral("transmitting")] = m_bridge->transmitting();
    root[QStringLiteral("ntpSynced")] = m_bridge->ntpSynced();
    root[QStringLiteral("ntpOffsetMs")] = m_bridge->ntpOffsetMs();
    root[QStringLiteral("decodesCount")] = m_bridge->decodeList().size();
    root[QStringLiteral("serverTimeMs")] =
        static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray DecodiumWebServer::buildDecodesJson() const
{
    QJsonArray arr;
    if (m_bridge) {
        QVariantList const decodes = m_bridge->decodeList();
        // Limit a 200 entries piu' recenti per non saturare il payload
        int const start = qMax(0, decodes.size() - 200);
        for (int i = start; i < decodes.size(); ++i) {
            QVariantMap const m = decodes.at(i).toMap();
            QJsonObject o;
            o[QStringLiteral("time")]    = m.value(QStringLiteral("time")).toString();
            o[QStringLiteral("db")]      = m.value(QStringLiteral("db")).toString();
            o[QStringLiteral("dt")]      = m.value(QStringLiteral("dt")).toString();
            o[QStringLiteral("freq")]    = m.value(QStringLiteral("freq")).toString();
            o[QStringLiteral("message")] = m.value(QStringLiteral("message")).toString();
            o[QStringLiteral("isTx")]    = m.value(QStringLiteral("isTx")).toBool();
            o[QStringLiteral("isCQ")]    = m.value(QStringLiteral("isCQ")).toBool();
            o[QStringLiteral("isMyCall")] = m.value(QStringLiteral("isMyCall")).toBool();
            o[QStringLiteral("dxCallsign")] = m.value(QStringLiteral("dxCallsign")).toString();
            o[QStringLiteral("dxCountry")]  = m.value(QStringLiteral("dxCountry")).toString();
            o[QStringLiteral("dxDistance")] = m.value(QStringLiteral("dxDistance")).toDouble();
            arr.append(o);
        }
    }
    QJsonObject root;
    root[QStringLiteral("decodes")] = arr;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray DecodiumWebServer::buildManifestJson() const
{
    QJsonObject m;
    m[QStringLiteral("name")] = QStringLiteral("Decodium Remote");
    m[QStringLiteral("short_name")] = QStringLiteral("Decodium");
    m[QStringLiteral("display")] = QStringLiteral("standalone");
    m[QStringLiteral("background_color")] = QStringLiteral("#0e1014");
    m[QStringLiteral("theme_color")]      = QStringLiteral("#3a8edc");
    m[QStringLiteral("orientation")]      = QStringLiteral("any");
    m[QStringLiteral("start_url")] = QStringLiteral("/");
    QJsonArray icons;
    icons.append(QJsonObject{
        {"src", "/favicon.ico"}, {"sizes", "64x64"}, {"type", "image/gif"}});
    m[QStringLiteral("icons")] = icons;
    return QJsonDocument(m).toJson(QJsonDocument::Compact);
}

QByteArray DecodiumWebServer::buildFavicon() const
{
    // GIF 1x1 trasparente (43 byte).
    static unsigned char const gif1x1[] = {
        0x47,0x49,0x46,0x38,0x39,0x61,0x01,0x00,0x01,0x00,0x80,0x00,0x00,0xFF,0xFF,0xFF,
        0x00,0x00,0x00,0x21,0xF9,0x04,0x01,0x00,0x00,0x00,0x00,0x2C,0x00,0x00,0x00,0x00,
        0x01,0x00,0x01,0x00,0x00,0x02,0x02,0x44,0x01,0x00,0x3B
    };
    return QByteArray(reinterpret_cast<char const*>(gif1x1), sizeof(gif1x1));
}

QByteArray DecodiumWebServer::buildIndexHtml() const
{
    static const char* kHtml = R"HTML(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta name="theme-color" content="#3a8edc">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="Decodium">
<link rel="manifest" href="/manifest.json">
<title>Decodium Remote</title>
<style>
:root {
  --bg-deep: #0e1014;
  --bg-card: #161a21;
  --txt: #e8edf2;
  --txt-dim: #9aa4af;
  --accent: #3a8edc;
  --green: #46c46e;
  --red: #e04545;
  --amber: #f5a623;
  --border: rgba(58,142,220,0.4);
}
* { box-sizing: border-box; }
html, body {
  margin: 0; padding: 0; height: 100%;
  background: var(--bg-deep); color: var(--txt);
  font-family: -apple-system,BlinkMacSystemFont,"Helvetica Neue",sans-serif;
  -webkit-font-smoothing: antialiased;
}
header {
  position: sticky; top: 0; z-index: 10;
  background: linear-gradient(180deg, #0e1014, rgba(14,16,20,0.9));
  border-bottom: 1px solid var(--border);
  padding: 8px 12px;
  font-size: 13px;
}
header .row { display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
header .pill {
  padding: 2px 8px; border-radius: 8px;
  background: rgba(58,142,220,0.15);
  border: 1px solid var(--border);
  font-size: 11px;
}
header .pill.sync { background: rgba(70,196,110,0.18); border-color: rgba(70,196,110,0.5); }
header .pill.tx { background: rgba(224,69,69,0.25); border-color: rgba(224,69,69,0.55); animation: pulse 1s infinite; }
@keyframes pulse { 50% { opacity: 0.55; } }
main { padding: 0; }
table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
  font-variant-numeric: tabular-nums;
}
th, td {
  text-align: left; padding: 5px 8px;
  border-bottom: 1px solid rgba(154,164,175,0.10);
}
th {
  position: sticky; top: 48px;
  background: var(--bg-card);
  color: var(--txt-dim);
  font-weight: 600; font-size: 10px; text-transform: uppercase;
  letter-spacing: 0.5px;
}
tr.cq td { background: rgba(70,196,110,0.06); cursor: pointer; }
tr.mycall td { background: rgba(224,69,69,0.10); cursor: pointer; }
tr.tx td { background: rgba(245,166,35,0.12); }
tr:not(.tx) td { cursor: pointer; }
tr:not(.tx):hover td { background: rgba(58,142,220,0.18); }
td.msg { font-family: "SF Mono","Consolas",monospace; font-weight: 600; }
td.country { color: var(--txt-dim); }
.error { padding: 24px; color: var(--red); text-align: center; }
/* Footer controls bar */
footer {
  position: fixed; bottom: 0; left: 0; right: 0; z-index: 20;
  background: rgba(14,16,20,0.96);
  border-top: 1px solid var(--border);
  padding: 8px 12px;
  display: flex; flex-wrap: wrap; gap: 8px; align-items: center;
  -webkit-backdrop-filter: blur(8px);
}
footer button {
  background: rgba(58,142,220,0.18); color: var(--txt);
  border: 1px solid var(--border); border-radius: 6px;
  padding: 6px 10px; font-size: 12px; font-weight: 600;
  cursor: pointer; -webkit-tap-highlight-color: transparent;
}
footer button:hover { background: rgba(58,142,220,0.32); }
footer button:active { background: rgba(58,142,220,0.5); }
footer button.danger { background: rgba(224,69,69,0.2); border-color: rgba(224,69,69,0.5); }
footer button.danger:hover { background: rgba(224,69,69,0.35); }
footer button.success { background: rgba(70,196,110,0.2); border-color: rgba(70,196,110,0.5); }
footer .spacer { flex: 1; }
footer .dxcall { color: var(--amber); font-weight: 700; padding: 4px 8px; }
main { padding-bottom: 60px; } /* leave space for footer */
@media (max-width: 600px) {
  th.country, td.country, th.dist, td.dist { display: none; }
}
</style>
</head>
<body>
<header>
  <div class="row">
    <span class="pill" id="callsign">—</span>
    <span class="pill" id="mode">—</span>
    <span class="pill" id="dial">— kHz</span>
    <span class="pill" id="rxfreq">RX —</span>
    <span class="pill sync" id="ntp" style="display:none">NTP</span>
    <span class="pill tx" id="tx" style="display:none">TX</span>
    <span class="pill" id="count">0 dec</span>
    <span class="pill" id="lag" style="background:transparent;border:none;color:var(--txt-dim)">—</span>
  </div>
</header>
<main>
  <table>
    <thead>
      <tr>
        <th>UTC</th><th>SNR</th><th>DT</th><th>Hz</th><th>Msg</th>
        <th class="country">Country</th><th class="dist">km</th>
      </tr>
    </thead>
    <tbody id="decodes"></tbody>
  </table>
  <div id="error" class="error" style="display:none"></div>
</main>
<footer>
  <button id="btnMonitor">Monitor ON/OFF</button>
  <button id="btnSync">⏱ Sync</button>
  <span class="dxcall" id="dxcallShow">DX: —</span>
  <span class="spacer"></span>
  <button id="btnClear" class="danger">Clear DX</button>
</footer>
<script>
const $ = (id) => document.getElementById(id);
let pollTimer = null;
let ws = null;
let wsLastMsgMs = 0;

function renderState(s) {
  $("error").style.display = "none";
  $("callsign").textContent = s.callsign || "—";
  $("mode").textContent     = s.mode || "—";
  const dialKHz = (s.dialFrequency / 1000).toFixed(3);
  $("dial").textContent  = dialKHz + " kHz";
  $("rxfreq").textContent = "RX " + s.rxFrequency + " Hz";
  $("ntp").style.display = s.ntpSynced ? "inline" : "none";
  if (s.ntpSynced) $("ntp").textContent = "NTP " + s.ntpOffsetMs.toFixed(0) + "ms";
  $("tx").style.display = s.transmitting ? "inline" : "none";
  $("count").textContent = s.decodesCount + " dec";
  $("dxcallShow").textContent = "DX: " + (s.dxCall || "—");
}
// Command sender. Tutti i client commands passano da qui.
function sendCmd(action, payload) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({type: "cmd", action: action, payload: payload || {}}));
  } else {
    alert("WebSocket non connesso — riprova fra un attimo");
  }
}
// Click su row decode → set RX freq + DX call. Solo non-TX.
function decodeRowClick(e) {
  const row = e.target.closest("tr");
  if (!row || row.classList.contains("tx")) return;
  const freq = parseInt(row.dataset.freq) || 0;
  const dx = row.dataset.dxcall || "";
  if (freq > 0) sendCmd("setRxFreq", {hz: freq});
  if (dx) sendCmd("setDxCall", {call: dx});
}
function renderDecodes(payload) {
  const tbody = $("decodes");
  const rows = [];
  const list = (payload.decodes || []).slice(-100).reverse();
  for (const e of list) {
    let cls = "";
    if (e.isTx) cls = "tx";
    else if (e.isMyCall) cls = "mycall";
    else if (e.isCQ) cls = "cq";
    const distStr = e.dxDistance > 0 ? Math.round(e.dxDistance) : "";
    rows.push(`<tr class="${cls}" data-freq="${e.freq}" data-dxcall="${escapeHtml(e.dxCallsign||"")}">
      <td>${e.time}</td>
      <td>${e.db}</td>
      <td>${e.dt}</td>
      <td>${e.freq}</td>
      <td class="msg">${escapeHtml(e.message)}</td>
      <td class="country">${escapeHtml(e.dxCountry||"")}</td>
      <td class="dist">${distStr}</td>
    </tr>`);
  }
  tbody.innerHTML = rows.join("");
}
async function pollOnce() {
  try {
    const t0 = Date.now();
    const [s, d] = await Promise.all([
      fetch("/api/state").then(r => r.json()),
      fetch("/api/decodes").then(r => r.json())
    ]);
    renderState(s);
    renderDecodes(d);
    $("lag").textContent = (Date.now() - t0) + " ms (poll)";
  } catch (err) {
    $("error").style.display = "block";
    $("error").textContent = "Connessione persa: " + err.message;
  }
}
function startPolling() {
  if (pollTimer) return;
  pollOnce();
  pollTimer = setInterval(pollOnce, 2000);
}
function stopPolling() {
  if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
}
function connectWS() {
  try {
    const wsUrl = "ws://" + location.hostname + ":" + (parseInt(location.port||80)+1) + "/";
    ws = new WebSocket(wsUrl);
    ws.onopen = () => {
      stopPolling();
      $("lag").textContent = "live (ws)";
    };
    ws.onmessage = (ev) => {
      wsLastMsgMs = Date.now();
      try {
        const env = JSON.parse(ev.data);
        if (env.type === "state") renderState(env.payload);
        else if (env.type === "decodes") renderDecodes(env.payload);
      } catch (e) { /* ignore parse err */ }
    };
    ws.onclose = () => { ws = null; startPolling(); setTimeout(connectWS, 5000); };
    ws.onerror = () => { try { ws.close(); } catch(e){} };
  } catch (e) {
    startPolling();
    setTimeout(connectWS, 5000);
  }
}
function escapeHtml(s) {
  return String(s||"").replace(/[&<>"']/g, c => ({
    "&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#39;"
  }[c]));
}
// All'avvio prova WS prima; fallback polling automatico se WS fallisce.
connectWS();
// Watchdog: se nessun WS message per 10s, forza polling.
setInterval(() => {
  if (ws && (Date.now() - wsLastMsgMs > 10000)) {
    try { ws.close(); } catch(e){}
  }
}, 5000);
// Wire footer buttons + delegated click on decode rows.
document.getElementById("decodes").addEventListener("click", decodeRowClick);
document.getElementById("btnMonitor").addEventListener("click", () => {
  // toggle: leggi state attuale dalla pillola TX (se TX visible è monitor=on+transmitting)
  // più semplice: chiediamo state poi togglare.
  fetch("/api/state").then(r=>r.json()).then(s => {
    sendCmd("setMonitoring", {on: !s.monitoring});
  });
});
document.getElementById("btnSync").addEventListener("click", () => sendCmd("syncNtpNow", {}));
document.getElementById("btnClear").addEventListener("click", () => sendCmd("setDxCall", {call: ""}));
</script>
</body>
</html>
)HTML";
    return QByteArray(kHtml);
}

void DecodiumWebServer::writeHttpResponse(QTcpSocket* socket, int statusCode,
                                           QByteArray const& contentType,
                                           QByteArray const& body,
                                           bool keepAlive) const
{
    Q_UNUSED(keepAlive)
    if (!socket || !socket->isOpen()) return;
    QByteArray reason = "OK";
    if (statusCode == 400) reason = "Bad Request";
    else if (statusCode == 404) reason = "Not Found";
    else if (statusCode == 405) reason = "Method Not Allowed";
    else if (statusCode == 500) reason = "Internal Server Error";

    QByteArray header;
    header += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + reason + "\r\n";
    header += "Content-Type: " + contentType + "\r\n";
    header += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    header += "Cache-Control: no-store\r\n";
    header += "Access-Control-Allow-Origin: *\r\n";
    header += "Connection: close\r\n";
    header += "\r\n";

    socket->write(header);
    socket->write(body);
    socket->disconnectFromHost();
}
