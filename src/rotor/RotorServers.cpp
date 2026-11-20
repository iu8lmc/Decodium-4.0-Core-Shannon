#include "RotorServers.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

namespace decodium::rotor {

namespace {

const QByteArray kRprtOk = "RPRT 0\n";
const QByteArray kRprtEinval = "RPRT -1\n";
const QByteArray kRprtEproto = "RPRT -8\n";
constexpr double kJogStepDeg = 15.0;   // il Prosistel non ha rotazione continua: un passo per volta
constexpr int kMoveUp = 2, kMoveDown = 4, kMoveLeft = 8, kMoveRight = 16;

std::optional<double> optDouble(const QVariant& v)
{
    if (!v.isValid() || v.isNull())
        return std::nullopt;
    bool ok = false;
    const double d = v.toDouble(&ok);
    if (!ok)
        throw RotorError(QStringLiteral("not a number: %1").arg(v.toString()));
    return d;
}

QByteArray toJson(const QVariantMap& map)
{
    return QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact);
}

double stateNumber(const QVariantMap& state, const char* key, double fallback = 0.0)
{
    const QVariant v = state.value(QLatin1String(key));
    return (v.isValid() && !v.isNull()) ? v.toDouble() : fallback;
}

}  // namespace

// ── rotctld ────────────────────────────────────────────────────────────────

RotctldServer::RotctldServer(RotorController* rotor, QObject* parent)
    : QObject(parent)
    , m_rotor(rotor)
{
    connect(&m_server, &QTcpServer::newConnection, this, &RotctldServer::onConnection);
}

bool RotctldServer::listen(const QHostAddress& address, quint16 port, QString* error)
{
    if (!m_server.listen(address, port)) {
        if (error)
            *error = m_server.errorString();
        return false;
    }
    return true;
}

void RotctldServer::close()
{
    m_server.close();
    for (QTcpSocket* s : m_server.findChildren<QTcpSocket*>())
        s->close();
}

void RotctldServer::onConnection()
{
    while (QTcpSocket* socket = m_server.nextPendingConnection()) {
        m_rotor->clientAttached();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onData(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            m_rotor->clientDetached();
            socket->deleteLater();
        });
    }
}

void RotctldServer::onData(QTcpSocket* socket)
{
    while (socket->canReadLine()) {
        const QString line = QString::fromLatin1(socket->readLine()).trimmed();
        const std::optional<QByteArray> response = execute(line);
        if (!response) {
            socket->disconnectFromHost();
            return;
        }
        socket->write(*response);
    }
}

std::optional<QByteArray> RotctldServer::execute(const QString& lineIn)
{
    QString line = lineIn.trimmed();
    if (line.isEmpty())
        return QByteArray();
    const bool extended = line.startsWith(QLatin1Char('+'));
    if (extended)
        line.remove(0, 1);
    const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QByteArray();
    const QString command = parts.first();
    const QStringList args = parts.mid(1);

    if (command == QLatin1String("q") || command == QLatin1String("Q") || command == QLatin1String("quit"))
        return std::nullopt;
    if (command == QLatin1String("p") || command == QLatin1String("get_pos"))
        return getPosition(extended);
    if (command == QLatin1String("P") || command == QLatin1String("set_pos"))
        return setPosition(args);
    if (command == QLatin1String("S") || command == QLatin1String("stop")) {
        m_rotor->halt();
        return kRprtOk;
    }
    if (command == QLatin1String("K") || command == QLatin1String("park")) {
        try {
            m_rotor->park();
        } catch (const RotorError&) {
            return kRprtEinval;
        }
        return kRprtOk;
    }
    if (command == QLatin1String("R") || command == QLatin1String("reset")) {
        m_rotor->halt(QStringLiteral("all"), true);
        return kRprtOk;
    }
    if (command == QLatin1String("M") || command == QLatin1String("move"))
        return move(args);
    if (command == QLatin1String("_") || command == QLatin1String("get_info")) {
        const QVariantMap state = m_rotor->snapshot();
        return QStringLiteral("Model: DecoRotor / %1\n").arg(state.value(QStringLiteral("model_label")).toString()).toLatin1();
    }
    if (command == QLatin1String("\\dump_state") || command == QLatin1String("dump_state")
        || command == QLatin1String("\\dump_caps") || command == QLatin1String("dump_caps")
        || command == QLatin1String("1"))
        return dumpState();
    return kRprtEproto;
}

QByteArray RotctldServer::getPosition(bool extended) const
{
    const QVariantMap state = m_rotor->snapshot();
    const double az = stateNumber(state, "az");
    const double el = stateNumber(state, "el");
    if (extended)
        return QStringLiteral("Azimuth: %1\nElevation: %2\n").arg(az, 0, 'f', 2).arg(el, 0, 'f', 2).toLatin1();
    return QStringLiteral("%1\n%2\n").arg(az, 0, 'f', 2).arg(el, 0, 'f', 2).toLatin1();
}

QByteArray RotctldServer::setPosition(const QStringList& args)
{
    if (args.isEmpty())
        return kRprtEinval;
    bool okAz = false;
    const double az = args.at(0).toDouble(&okAz);
    if (!okAz)
        return kRprtEinval;
    std::optional<double> el;
    if (args.size() > 1) {
        bool okEl = false;
        const double e = args.at(1).toDouble(&okEl);
        if (!okEl)
            return kRprtEinval;
        el = e;
    }
    const QVariantMap state = m_rotor->snapshot();
    try {
        m_rotor->goTo(state.value(QStringLiteral("has_az")).toBool() ? std::optional<double>(az) : std::nullopt,
                      (state.value(QStringLiteral("has_el")).toBool() && el) ? el : std::nullopt);
    } catch (const RotorError&) {
        return kRprtEinval;
    }
    return kRprtOk;
}

// Traduce il movimento manuale in un goto incrementale: l'unico modo di
// muovere "a mano" un Prosistel e' spostare il target di un passo.
QByteArray RotctldServer::move(const QStringList& args)
{
    if (args.isEmpty())
        return kRprtEinval;
    bool ok = false;
    const int direction = args.at(0).toInt(&ok);
    if (!ok)
        return kRprtEinval;
    const QVariantMap state = m_rotor->snapshot();
    const QVariant az = state.value(QStringLiteral("az"));
    const QVariant el = state.value(QStringLiteral("el"));
    const bool haveAz = az.isValid() && !az.isNull();
    const bool haveEl = el.isValid() && !el.isNull();
    try {
        if (direction == kMoveRight && haveAz)
            m_rotor->goTo(az.toDouble() + kJogStepDeg, std::nullopt);
        else if (direction == kMoveLeft && haveAz)
            m_rotor->goTo(std::max(0.0, az.toDouble() - kJogStepDeg), std::nullopt);
        else if (direction == kMoveUp && haveEl)
            m_rotor->goTo(std::nullopt, el.toDouble() + kJogStepDeg);
        else if (direction == kMoveDown && haveEl)
            m_rotor->goTo(std::nullopt, el.toDouble() - kJogStepDeg);
        else
            return kRprtEinval;
    } catch (const RotorError&) {
        return kRprtEinval;
    }
    return kRprtOk;
}

QByteArray RotctldServer::dumpState() const
{
    const Config cfg = m_rotor->config();
    const QVariantMap state = m_rotor->snapshot();
    const bool hasAz = state.value(QStringLiteral("has_az")).toBool();
    const bool hasEl = state.value(QStringLiteral("has_el")).toBool();
    const QString rotType = (hasAz && hasEl) ? QStringLiteral("AzEl") : (hasAz ? QStringLiteral("Az") : QStringLiteral("El"));
    const QStringList lines {
        QStringLiteral("1"),   // versione del protocollo
        QStringLiteral("2"),   // ROT_MODEL_NETROTCTL
        QString::number(cfg.limits.azMin, 'f', 6),
        QString::number(cfg.limits.azMax, 'f', 6),
        QString::number(cfg.limits.elMin, 'f', 6),
        QString::number(cfg.limits.elMax, 'f', 6),
        QStringLiteral("0"),   // south_zero: lo zero e' a nord
        QStringLiteral("rot_type=") + rotType,
        QStringLiteral("done"),
    };
    return (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toLatin1();
}

// ── WebSocket ──────────────────────────────────────────────────────────────

WsServer::WsServer(RotorController* rotor, ServerDelegate delegate, QObject* parent)
    : QObject(parent)
    , m_rotor(rotor)
    , m_delegate(std::move(delegate))
{
    connect(m_rotor, &RotorController::stateUpdated, this, &WsServer::broadcastState);
}

bool WsServer::listen(const QHostAddress& address, quint16 port, const QString& token, QString* error)
{
    close();
    m_token = token;
    m_server = new QWebSocketServer(QStringLiteral("DecoRotor"), QWebSocketServer::NonSecureMode, this);
    if (!m_server->listen(address, port)) {
        if (error)
            *error = m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QWebSocketServer::newConnection, this, &WsServer::onConnection);
    return true;
}

void WsServer::close()
{
    if (!m_server)
        return;
    const auto sockets = m_clients.keys();
    m_server->close();
    for (QWebSocket* s : sockets)
        s->close();
    m_clients.clear();
    delete m_server;
    m_server = nullptr;
}

void WsServer::onConnection()
{
    while (QWebSocket* socket = m_server->nextPendingConnection()) {
        const bool needsAuth = !m_token.isEmpty();
        const QString offered = QUrlQuery(socket->requestUrl()).queryItemValue(QStringLiteral("token"));
        Client client;
        client.authorized = !needsAuth || offered == m_token;
        m_clients.insert(socket, client);
        m_rotor->clientAttached();
        connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString& t) { onText(socket, t); });
        connect(socket, &QWebSocket::disconnected, this, [this, socket] {
            if (m_clients.remove(socket))
                m_rotor->clientDetached();
            socket->deleteLater();
        });
        send(socket, {{QStringLiteral("type"), QStringLiteral("hello")},
                      {QStringLiteral("version"), QStringLiteral("1.0")},
                      {QStringLiteral("auth"), needsAuth}});
        send(socket, m_rotor->snapshot());
    }
}

void WsServer::send(QWebSocket* socket, const QVariantMap& message)
{
    if (socket && socket->isValid())
        socket->sendTextMessage(QString::fromUtf8(toJson(message)));
}

void WsServer::broadcastState()
{
    if (m_clients.isEmpty())
        return;
    const QString payload = QString::fromUtf8(toJson(m_rotor->snapshot()));
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        if (it.key()->isValid())
            it.key()->sendTextMessage(payload);
    }
}

void WsServer::onText(QWebSocket* socket, const QString& text)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        send(socket, {{QStringLiteral("type"), QStringLiteral("error")}, {QStringLiteral("message"), QStringLiteral("invalid JSON")}});
        return;
    }
    const QVariantMap request = doc.object().toVariantMap();
    const QString command = request.value(QStringLiteral("cmd")).toString();
    Client& client = m_clients[socket];
    if (command == QLatin1String("auth")) {
        client.authorized = request.value(QStringLiteral("token")).toString() == m_token;
        send(socket, {{QStringLiteral("type"), QStringLiteral("ack")}, {QStringLiteral("cmd"), QStringLiteral("auth")},
                      {QStringLiteral("ok"), client.authorized}});
        return;
    }
    if (!client.authorized) {
        send(socket, {{QStringLiteral("type"), QStringLiteral("error")},
                      {QStringLiteral("message"), QStringLiteral("token missing or wrong")}});
        return;
    }
    dispatch(socket, command, request, &client.authorized);
}

void WsServer::dispatch(QWebSocket* socket, const QString& command, const QVariantMap& request, bool*)
{
    auto ack = [&](const QVariantMap& extra = {}) {
        QVariantMap m {{QStringLiteral("type"), QStringLiteral("ack")}, {QStringLiteral("cmd"), command}};
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
            m.insert(it.key(), it.value());
        send(socket, m);
    };
    try {
        if (command == QLatin1String("goto")) {
            ack({{QStringLiteral("applied"),
                  m_rotor->goTo(optDouble(request.value(QStringLiteral("az"))), optDouble(request.value(QStringLiteral("el"))))}});
        } else if (command == QLatin1String("goto_locator")) {
            ack({{QStringLiteral("applied"),
                  m_rotor->goToLocator(request.value(QStringLiteral("locator")).toString(),
                                       request.value(QStringLiteral("long_path")).toBool())}});
        } else if (command == QLatin1String("bearing")) {
            ack({{QStringLiteral("bearing"), m_rotor->bearingTo(request.value(QStringLiteral("locator")).toString()).toMap()}});
        } else if (command == QLatin1String("stop")) {
            m_rotor->halt(request.contains(QStringLiteral("axis")) ? request.value(QStringLiteral("axis")).toString() : QStringLiteral("all"),
                          request.value(QStringLiteral("fast")).toBool());
            ack();
        } else if (command == QLatin1String("park")) {
            ack({{QStringLiteral("applied"), m_rotor->park()}});
        } else if (command == QLatin1String("presets")) {
            ack({{QStringLiteral("presets"), m_rotor->presets()}});
        } else if (command == QLatin1String("preset_save")) {
            ack({{QStringLiteral("presets"),
                  m_rotor->savePreset(request.value(QStringLiteral("name")).toString(),
                                      request.value(QStringLiteral("az")).toDouble(),
                                      optDouble(request.value(QStringLiteral("el"))))}});
        } else if (command == QLatin1String("preset_delete")) {
            ack({{QStringLiteral("presets"), m_rotor->deletePreset(request.value(QStringLiteral("name")).toString())}});
        } else if (command == QLatin1String("preset_recall")) {
            ack({{QStringLiteral("applied"), m_rotor->recallPreset(request.value(QStringLiteral("name")).toString())}});
        } else if (command == QLatin1String("traffic")) {
            ack({{QStringLiteral("traffic"), m_rotor->recentTraffic(request.value(QStringLiteral("limit"), 50).toInt())}});
        } else if (command == QLatin1String("history")) {
            ack({{QStringLiteral("history"), m_rotor->recentHistory(request.value(QStringLiteral("limit"), 300).toInt())}});
        } else if (command == QLatin1String("config")) {
            ack({{QStringLiteral("config"), m_delegate.configPayload ? m_delegate.configPayload() : QVariantMap()}});
        } else if (command == QLatin1String("config_set")) {
            const QVariantMap values = request.value(QStringLiteral("values")).toMap();
            ack({{QStringLiteral("config"), m_delegate.configApply ? m_delegate.configApply(values) : QVariantMap()}});
        } else if (command == QLatin1String("state")) {
            send(socket, m_rotor->snapshot());
        } else if (command == QLatin1String("ping")) {
            send(socket, {{QStringLiteral("type"), QStringLiteral("pong")}});
        } else {
            send(socket, {{QStringLiteral("type"), QStringLiteral("error")},
                          {QStringLiteral("message"), QStringLiteral("unknown command: %1").arg(command)}});
        }
    } catch (const RotorError& e) {
        send(socket, {{QStringLiteral("type"), QStringLiteral("error")}, {QStringLiteral("message"), e.message()}});
    }
}

// ── riquadri ───────────────────────────────────────────────────────────────

TileCache::TileCache(const QString& directory, QObject* parent)
    : QObject(parent)
    , m_directory(directory)
{
}

void TileCache::configure(const QString& urlTemplate, double maxAgeDays, double maxMegabytes)
{
    m_template = urlTemplate;
    m_maxAgeSeconds = static_cast<qint64>(std::max(0.0, maxAgeDays) * 86400.0);
    m_maxBytes = static_cast<qint64>(std::max(0.0, maxMegabytes) * 1024.0 * 1024.0);
}

bool TileCache::valid(int z, int x, int y)
{
    if (z < 0 || z > kMaxZoom)
        return false;
    const int side = 1 << z;
    return x >= 0 && x < side && y >= 0 && y < side;
}

QString TileCache::path(int z, int x, int y) const
{
    return QStringLiteral("%1/%2/%3/%4.img").arg(m_directory).arg(z).arg(x).arg(y);
}

QByteArray TileCache::readFresh(const QString& p) const
{
    QFileInfo info(p);
    if (!info.exists())
        return {};
    if (m_maxAgeSeconds && info.lastModified().secsTo(QDateTime::currentDateTime()) > m_maxAgeSeconds)
        return {};
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

void TileCache::tile(int z, int x, int y, std::function<void(const QByteArray&)> done)
{
    if (!valid(z, x, y) || m_template.isEmpty()) {
        done({});
        return;
    }
    const QString p = path(z, x, y);
    const QByteArray cached = readFresh(p);
    if (!cached.isEmpty()) {
        done(cached);
        return;
    }
    // Un riquadro alla volta: durante una panoramica la stessa immagine viene
    // chiesta da piu' connessioni insieme.
    auto pending = m_pending.find(p);
    if (pending != m_pending.end()) {
        pending->append(std::move(done));
        return;
    }
    m_pending.insert(p, {std::move(done)});

    QString url = m_template;
    url.replace(QStringLiteral("{z}"), QString::number(z));
    url.replace(QStringLiteral("{x}"), QString::number(x));
    url.replace(QStringLiteral("{y}"), QString::number(y));
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium-Rotor/1.0 (amateur radio antenna rotator)"));
    request.setTransferTimeout(12000);
    QNetworkReply* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, p] {
        QByteArray data;
        if (reply->error() == QNetworkReply::NoError)
            data = reply->read(kMaxTileBytes + 1);
        reply->deleteLater();
        if (data.isEmpty() || data.size() > kMaxTileBytes)
            data.clear();
        else
            store(p, data);
        const auto callbacks = m_pending.take(p);
        for (const auto& cb : callbacks)
            cb(data);
    });
}

void TileCache::store(const QString& p, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    const QString temporary = p + QStringLiteral(".part");
    QFile f(temporary);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(data);
    f.close();
    QFile::remove(p);
    QFile::rename(temporary, p);
    if (++m_written % 200 == 0)
        trim();
}

// Sopra il tetto di spazio, i riquadri piu' vecchi lasciano il posto.
void TileCache::trim()
{
    if (!m_maxBytes)
        return;
    struct Entry { QString path; qint64 size; QDateTime when; };
    QList<Entry> all;
    qint64 total = 0;
    QDirIterator it(m_directory, {QStringLiteral("*.img")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        all.append({it.filePath(), it.fileInfo().size(), it.fileInfo().lastModified()});
        total += it.fileInfo().size();
    }
    if (total <= m_maxBytes)
        return;
    std::sort(all.begin(), all.end(), [](const Entry& a, const Entry& b) { return a.when < b.when; });
    for (const Entry& e : all) {
        if (total <= static_cast<qint64>(m_maxBytes * 0.8))
            break;
        if (QFile::remove(e.path))
            total -= e.size;
    }
}

// ── HTTP ───────────────────────────────────────────────────────────────────

HttpServer::HttpServer(RotorController* rotor, TileCache* tiles, ServerDelegate delegate, QObject* parent)
    : QObject(parent)
    , m_rotor(rotor)
    , m_tiles(tiles)
    , m_delegate(std::move(delegate))
{
    connect(&m_server, &QTcpServer::newConnection, this, &HttpServer::onConnection);
}

bool HttpServer::listen(const QHostAddress& address, quint16 port, const QString& token,
                        int wsPort, int rotctldPort, QString* error)
{
    m_token = token;
    m_wsPort = wsPort;
    m_rotctldPort = rotctldPort;
    if (!m_server.listen(address, port)) {
        if (error)
            *error = m_server.errorString();
        return false;
    }
    return true;
}

void HttpServer::close()
{
    m_server.close();
    for (QTcpSocket* s : m_server.findChildren<QTcpSocket*>())
        s->abort();
}

void HttpServer::onConnection()
{
    while (QTcpSocket* socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onData(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void HttpServer::onData(QTcpSocket* socket)
{
    QByteArray buffer = socket->property("buffer").toByteArray();
    buffer += socket->readAll();
    if (buffer.size() > 1'000'000) {       // una richiesta cosi' non e' per noi
        socket->abort();
        return;
    }
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        socket->setProperty("buffer", buffer);
        return;
    }
    const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
    const QList<QByteArray> request = lines.first().trimmed().split(' ');
    if (request.size() < 2) {
        socket->abort();
        return;
    }
    QHash<QString, QString> headers;
    for (int i = 1; i < lines.size(); ++i) {
        const int colon = lines.at(i).indexOf(':');
        if (colon > 0)
            headers.insert(QString::fromLatin1(lines.at(i).left(colon)).trimmed().toLower(),
                           QString::fromLatin1(lines.at(i).mid(colon + 1)).trimmed());
    }
    const int length = headers.value(QStringLiteral("content-length")).toInt();
    if (buffer.size() < headerEnd + 4 + length) {
        socket->setProperty("buffer", buffer);
        return;
    }
    socket->setProperty("buffer", QByteArray());
    handle(socket, QString::fromLatin1(request.at(0)), QString::fromLatin1(request.at(1)), headers,
           buffer.mid(headerEnd + 4, length));
}

bool HttpServer::authorized(const QHash<QString, QString>& headers, const QString& queryToken) const
{
    if (m_token.isEmpty())
        return true;
    return headers.value(QStringLiteral("x-token")) == m_token || queryToken == m_token;
}

void HttpServer::sendResponse(QTcpSocket* socket, int status, const QByteArray& contentType,
                              const QByteArray& body, const QByteArray& extraHeaders)
{
    static const QHash<int, QByteArray> reasons {
        {200, "OK"}, {204, "No Content"}, {400, "Bad Request"}, {401, "Unauthorized"}, {404, "Not Found"}};
    QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + " " + reasons.value(status, "OK") + "\r\n";
    out += "Content-Type: " + contentType + "\r\n";
    out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    out += "Connection: close\r\n";
    out += extraHeaders;
    out += "\r\n";
    out += body;
    socket->write(out);
    socket->disconnectFromHost();
}

void HttpServer::sendJson(QTcpSocket* socket, const QVariantMap& payload, int status)
{
    sendResponse(socket, status, "application/json; charset=utf-8", toJson(payload),
                 "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type, X-Token\r\n");
}

void HttpServer::handle(QTcpSocket* socket, const QString& method, const QString& target,
                        const QHash<QString, QString>& headers, const QByteArray& body)
{
    const QUrl url(target);
    const QString route = url.path();
    const QString queryToken = QUrlQuery(url).queryItemValue(QStringLiteral("token"));

    if (method == QLatin1String("OPTIONS")) {
        sendResponse(socket, 204, "text/plain", {},
                     "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                     "Access-Control-Allow-Headers: Content-Type, X-Token\r\n");
        return;
    }

    if (method == QLatin1String("GET")) {
        if (route == QLatin1String("/api/info")) {
            sendJson(socket, {{QStringLiteral("product"), QStringLiteral("DecoRotor")},
                              {QStringLiteral("version"), QStringLiteral("1.0")},
                              {QStringLiteral("ws_port"), m_wsPort},
                              {QStringLiteral("rotctld_port"), m_rotctldPort},
                              {QStringLiteral("auth"), !m_token.isEmpty()}});
            return;
        }
        if (route == QLatin1String("/api/state")) {
            if (!authorized(headers, queryToken)) {
                sendJson(socket, {{QStringLiteral("error"), QStringLiteral("unauthorized")}}, 401);
                return;
            }
            sendJson(socket, m_rotor->snapshot());
            return;
        }
        if (route == QLatin1String("/api/spots")) {
            if (!authorized(headers, queryToken)) {
                sendJson(socket, {{QStringLiteral("error"), QStringLiteral("unauthorized")}}, 401);
                return;
            }
            sendJson(socket, m_delegate.spotsPayload ? m_delegate.spotsPayload() : QVariantMap());
            return;
        }
        if (route.startsWith(QLatin1String("/tiles/"))) {
            // /tiles/<zoom>/<x>/<y>.png, come lo chiede Qt Location.
            const QStringList parts = route.mid(7).split(QLatin1Char('/'));
            bool okZ = false, okX = false, okY = false;
            const int z = parts.value(0).toInt(&okZ);
            const int x = parts.value(1).toInt(&okX);
            const int y = parts.value(2).section(QLatin1Char('.'), 0, 0).toInt(&okY);
            if (parts.size() != 3 || !okZ || !okX || !okY || !m_tiles) {
                sendJson(socket, {{QStringLiteral("error"), QStringLiteral("bad tile")}}, 400);
                return;
            }
            QPointer<QTcpSocket> guard(socket);
            m_tiles->tile(z, x, y, [this, guard](const QByteArray& image) {
                if (!guard)
                    return;
                if (image.isEmpty())
                    sendResponse(guard, 404, "text/plain", {});
                else
                    sendResponse(guard, 200, "image/jpeg", image,
                                 "Cache-Control: public, max-age=604800\r\nAccess-Control-Allow-Origin: *\r\n");
            });
            return;
        }
        if (route.startsWith(QLatin1String("/api/"))) {
            sendJson(socket, {{QStringLiteral("error"), QStringLiteral("unknown endpoint")}}, 404);
            return;
        }
        if (route == QLatin1String("/") || route.isEmpty() || route == QLatin1String("/index.html")) {
            QFile page(QStringLiteral(":/rotor/web/index.html"));
            if (page.open(QIODevice::ReadOnly)) {
                sendResponse(socket, 200, "text/html; charset=utf-8", page.readAll(), "Cache-Control: no-cache\r\n");
                return;
            }
        }
        sendJson(socket, {{QStringLiteral("error"), QStringLiteral("not found")}}, 404);
        return;
    }

    if (method == QLatin1String("POST")) {
        if (!authorized(headers, queryToken)) {
            sendJson(socket, {{QStringLiteral("error"), QStringLiteral("unauthorized")}}, 401);
            return;
        }
        const QVariantMap payload = QJsonDocument::fromJson(body).object().toVariantMap();
        try {
            if (route == QLatin1String("/api/goto")) {
                const QVariantMap applied = m_rotor->goTo(optDouble(payload.value(QStringLiteral("az"))),
                                                          optDouble(payload.value(QStringLiteral("el"))));
                sendJson(socket, {{QStringLiteral("ok"), true}, {QStringLiteral("applied"), applied}});
            } else if (route == QLatin1String("/api/stop")) {
                m_rotor->halt(payload.contains(QStringLiteral("axis")) ? payload.value(QStringLiteral("axis")).toString()
                                                                       : QStringLiteral("all"),
                              payload.value(QStringLiteral("fast")).toBool());
                sendJson(socket, {{QStringLiteral("ok"), true}});
            } else if (route == QLatin1String("/api/park")) {
                sendJson(socket, {{QStringLiteral("ok"), true}, {QStringLiteral("applied"), m_rotor->park()}});
            } else {
                sendJson(socket, {{QStringLiteral("error"), QStringLiteral("unknown endpoint")}}, 404);
            }
        } catch (const RotorError& e) {
            sendJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("error"), e.message()}}, 400);
        }
        return;
    }
    sendJson(socket, {{QStringLiteral("error"), QStringLiteral("method not allowed")}}, 400);
}

}  // namespace decodium::rotor
