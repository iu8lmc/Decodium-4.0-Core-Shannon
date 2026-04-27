#include "DecodiumDxCluster.h"
#include <QAbstractSocket>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDateTime>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <QUrl>

namespace
{
QString const kPrimaryClusterHost {QStringLiteral("dx.iz7auh.net")};
int const kPrimaryClusterPort = 8000;
QString const kSecondaryClusterHost {QStringLiteral("iq8do.aricaserta.it")};
int const kSecondaryClusterPort = 7300;
int const kConnectTimeoutMs = 8000;

void beginConfiguredSettingsGroup(QSettings& settings)
{
    auto const* app = QCoreApplication::instance();
    if (!app) {
        return;
    }
    QString const configName = app->property("decodiumConfigName").toString().trimmed();
    if (configName.isEmpty()) {
        return;
    }
    settings.beginGroup(QStringLiteral("MultiSettings"));
    settings.beginGroup(configName);
}

QString socketErrorMessage(QAbstractSocket::SocketError error, const QString& fallback)
{
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        return QObject::tr("Connessione rifiutata");
    case QAbstractSocket::HostNotFoundError:
        return QObject::tr("Host non trovato");
    case QAbstractSocket::NetworkError:
        return QObject::tr("Errore di rete");
    case QAbstractSocket::SocketTimeoutError:
        return QObject::tr("Timeout di connessione");
    default:
        return fallback;
    }
}

QString normalizeClusterHost(QString host, int* portFromHost = nullptr)
{
    if (portFromHost) {
        *portFromHost = 0;
    }

    host = host.trimmed();
    if (host.isEmpty()) {
        return {};
    }

    QString probe = host;
    probe.remove(QRegularExpression(QStringLiteral("\\s+")));
    if (!probe.contains(QStringLiteral("://"))) {
        probe.prepend(QStringLiteral("telnet://"));
    }

    QUrl const url = QUrl::fromUserInput(probe);
    if (url.isValid() && !url.host().isEmpty()) {
        if (portFromHost && url.port() > 0) {
            *portFromHost = url.port();
        }
        host = url.host().trimmed();
    } else {
        int slashPos = host.indexOf(QLatin1Char('/'));
        if (slashPos >= 0) {
            host = host.left(slashPos);
        }

        int const colonPos = host.lastIndexOf(QLatin1Char(':'));
        if (colonPos > 0 && host.indexOf(QLatin1Char(':')) == colonPos) {
            bool ok = false;
            int const parsedPort = host.mid(colonPos + 1).toInt(&ok);
            if (ok && portFromHost) {
                *portFromHost = parsedPort;
            }
            if (ok) {
                host = host.left(colonPos);
            }
        }
    }

    host = host.trimmed();
    if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']')) && host.size() > 2) {
        host = host.mid(1, host.size() - 2);
    }
    while (host.endsWith(QLatin1Char('.'))) {
        host.chop(1);
    }
    return host;
}

bool isDeprecatedClusterEndpoint(const QString& host, int port)
{
    return (host.compare(QStringLiteral("www.hamqth.com"), Qt::CaseInsensitive) == 0 && port == 443)
        || (host.compare(QStringLiteral("ik5pwj-6.dyndns.org"), Qt::CaseInsensitive) == 0 && port == 8000)
        || (host.compare(QStringLiteral("dxc.sv5fri.eu"), Qt::CaseInsensitive) == 0 && port == 7300);
}

bool bufferContainsClusterPrompt(const QString& buffer)
{
    QString const text = buffer.simplified();
    QString const lower = text.toLower();
    return lower.contains(QStringLiteral("login:"))
        || lower.contains(QStringLiteral("call:"))
        || lower.contains(QStringLiteral("callsign"))
        || lower.contains(QStringLiteral("enter your"))
        || lower.contains(QStringLiteral("please enter"))
        || text.endsWith(QLatin1Char(':'))
        || text.endsWith(QLatin1Char('>'))
        || (lower.contains(QStringLiteral(" de ")) && text.endsWith(QLatin1Char('>')));
}

void appendUniqueEndpoint(QList<QPair<QString, int>>& endpoints, const QString& host, int port)
{
    QString const normalizedHost = normalizeClusterHost(host);
    int const normalizedPort = qBound(1, port, 65535);
    if (normalizedHost.isEmpty()) {
        return;
    }

    QPair<QString, int> const endpoint {normalizedHost, normalizedPort};
    if (!endpoints.contains(endpoint)) {
        endpoints.append(endpoint);
    }
}
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

DecodiumDxCluster::DecodiumDxCluster(QObject* parent)
    : QObject(parent)
{
    loadSettings();
}

DecodiumDxCluster::~DecodiumDxCluster()
{
    // Disconnect cleanly without emitting Qt signals during destruction.
    if (m_socket) {
        m_socket->disconnect();  // disconnect all Qt signal-slot connections
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        delete m_socket;
        m_socket = nullptr;
    }
}

void DecodiumDxCluster::ensureSocket()
{
    if (!m_socket) {
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::connected,
                this,     &DecodiumDxCluster::onConnected);
        connect(m_socket, &QTcpSocket::disconnected,
                this,     &DecodiumDxCluster::onDisconnected);
        connect(m_socket, &QTcpSocket::readyRead,
                this,     &DecodiumDxCluster::onReadyRead);
        connect(m_socket, &QAbstractSocket::errorOccurred,
                this,     &DecodiumDxCluster::onError);
    }

    if (!m_connectTimeoutTimer) {
        m_connectTimeoutTimer = new QTimer(this);
        m_connectTimeoutTimer->setSingleShot(true);
        connect(m_connectTimeoutTimer, &QTimer::timeout, this, [this]() {
            if (!m_connectSequenceActive || !m_socket
                || m_socket->state() != QAbstractSocket::ConnectingState) {
                return;
            }

            m_ignoreNextSocketError = true;
            QString const reason = tr("Timeout di connessione");
            m_socket->abort();
            if (!tryNextConnectionCandidate(reason)) {
                setLastStatus(tr("Errore: %1").arg(reason));
                emit errorOccurred(tr("DX Cluster non raggiungibile: %1").arg(reason));
            }
        });
    }
}

void DecodiumDxCluster::sendLogin()
{
    if (!m_socket || m_loginSent || m_callsign.trimmed().isEmpty()) {
        return;
    }

    QString const login = m_callsign.trimmed().toUpper();
    m_socket->write((login + QStringLiteral("\r\n")).toUtf8());
    m_loginSent = true;
    setLastStatus(tr("Login inviato come %1").arg(login));
    emit statusUpdate(tr("Login sent (%1).").arg(login));
}

QString DecodiumDxCluster::endpointLabel(const QString& host, int port) const
{
    return QStringLiteral("%1:%2").arg(host, QString::number(port));
}

QList<QPair<QString, int>> DecodiumDxCluster::buildConnectionCandidates(const QString& host, int port) const
{
    QList<QPair<QString, int>> endpoints;

    QString const normalizedHost = normalizeClusterHost(host);
    int const normalizedPort = qBound(1, port, 65535);

    auto appendCommonPorts = [&endpoints](const QString& endpointHost, int preferredPort) {
        appendUniqueEndpoint(endpoints, endpointHost, preferredPort);
        if (preferredPort != 8000) {
            appendUniqueEndpoint(endpoints, endpointHost, 8000);
        }
        if (preferredPort != 7300) {
            appendUniqueEndpoint(endpoints, endpointHost, 7300);
        }
    };

    if (!normalizedHost.isEmpty() && !isDeprecatedClusterEndpoint(normalizedHost, normalizedPort)) {
        bool const knownNode = normalizedHost.compare(kPrimaryClusterHost, Qt::CaseInsensitive) == 0
            || normalizedHost.compare(kSecondaryClusterHost, Qt::CaseInsensitive) == 0;
        if (knownNode) {
            appendCommonPorts(normalizedHost, normalizedPort);
        } else {
            appendUniqueEndpoint(endpoints, normalizedHost, normalizedPort);
        }
    }

    appendCommonPorts(kPrimaryClusterHost, kPrimaryClusterPort);
    appendCommonPorts(kSecondaryClusterHost, kSecondaryClusterPort);

    return endpoints;
}

bool DecodiumDxCluster::tryNextConnectionCandidate(const QString& failureReason)
{
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }

    if (!m_connectSequenceActive) {
        return false;
    }

    if (m_connectionCandidateIndex >= 0
        && m_connectionCandidateIndex < m_connectionCandidates.size()) {
        emit statusUpdate(tr("Connection to %1 failed: %2")
                              .arg(endpointLabel(m_activeHost, m_activePort), failureReason));
    }

    ++m_connectionCandidateIndex;
    if (m_connectionCandidateIndex >= m_connectionCandidates.size()) {
        m_connectSequenceActive = false;
        m_activeHost.clear();
        m_activePort = 0;
        return false;
    }

    ensureSocket();

    auto const& endpoint = m_connectionCandidates.at(m_connectionCandidateIndex);
    m_activeHost = endpoint.first;
    m_activePort = endpoint.second;
    m_loginSent = false;
    m_rxBuf.clear();
    m_ignoreNextSocketError = false;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    setLastStatus(tr("Connessione a %1 in corso…").arg(endpointLabel(m_activeHost, m_activePort)));
    emit statusUpdate(tr("Connecting to %1 …").arg(endpointLabel(m_activeHost, m_activePort)));
    m_socket->connectToHost(m_activeHost, static_cast<quint16>(m_activePort));
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->start(kConnectTimeoutMs);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Public slots
// ---------------------------------------------------------------------------

void DecodiumDxCluster::connectCluster()
{
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        setLastStatus(tr("Già connesso o in connessione."));
        emit statusUpdate(tr("Already connected or connecting."));
        return;
    }

    if (m_callsign.trimmed().isEmpty()) {
        setLastStatus(tr("Nominativo mancante. Imposta il callsign in Stazione."));
        emit errorOccurred(tr("Callsign not set. Please set your callsign before connecting."));
        return;
    }

    ensureSocket();

    int portFromHost = 0;
    QString normalizedHost = normalizeClusterHost(m_host, &portFromHost);
    int normalizedPort = portFromHost > 0 ? portFromHost : m_port;
    normalizedPort = qBound(1, normalizedPort > 0 ? normalizedPort : kPrimaryClusterPort, 65535);

    if (isDeprecatedClusterEndpoint(normalizedHost, normalizedPort)) {
        normalizedHost = kPrimaryClusterHost;
        normalizedPort = kPrimaryClusterPort;
        emit statusUpdate(tr("Configured cluster endpoint is legacy/read-only. Using %1 instead.")
                              .arg(endpointLabel(normalizedHost, normalizedPort)));
    }

    if (normalizedHost.isEmpty()) {
        normalizedHost = kPrimaryClusterHost;
    }

    bool settingsChanged = false;
    if (m_host.compare(normalizedHost, Qt::CaseInsensitive) != 0) {
        m_host = normalizedHost;
        emit hostChanged();
        settingsChanged = true;
    }
    if (m_port != normalizedPort) {
        m_port = normalizedPort;
        emit portChanged();
        settingsChanged = true;
    }
    if (settingsChanged) {
        saveSettings();
    }

    m_manualDisconnect = false;
    m_connected = false;
    emit connectedChanged();
    m_connectionCandidates = buildConnectionCandidates(m_host, m_port);
    m_connectionCandidateIndex = -1;
    m_connectSequenceActive = true;
    if (!tryNextConnectionCandidate(tr("Nessun motivo specificato"))) {
        m_connectSequenceActive = false;
        setLastStatus(tr("Errore: nessun endpoint DX Cluster valido"));
        emit errorOccurred(tr("DX Cluster configuration is invalid."));
    }
}

void DecodiumDxCluster::disconnectCluster()
{
    if (!m_socket || m_socket->state() == QAbstractSocket::UnconnectedState) {
        return;
    }

    m_manualDisconnect = true;
    m_connectSequenceActive = false;
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }

    // Politely say goodbye first (best-effort, non-blocking).
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QByteArray("BYE\r\n"));
        m_socket->flush();
    }

    m_socket->disconnectFromHost();
    // onDisconnected() will reset m_connected.
}

void DecodiumDxCluster::sendCommand(const QString& cmd)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(tr("Cannot send command: not connected."));
        return;
    }
    QString line = cmd.trimmed() + "\r\n";
    m_socket->write(line.toUtf8());
}

bool DecodiumDxCluster::sendSpot(const QString& dxCall, double freqKhz, const QString& comment)
{
    QString call = dxCall.trimmed().toUpper();
    if (call.isEmpty() || freqKhz <= 0.0) {
        emit errorOccurred(tr("Cannot send spot: invalid call or frequency."));
        return false;
    }

    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState || !m_connected) {
        emit errorOccurred(tr("Cannot send spot: not connected."));
        return false;
    }

    QString spotComment = comment.simplified();
    spotComment.replace(QRegularExpression(QStringLiteral("[\\r\\n]+")), QStringLiteral(" "));
    if (spotComment.isEmpty()) {
        spotComment = QStringLiteral("Decodium");
    }
    if (spotComment.size() > 80) {
        spotComment = spotComment.left(80).trimmed();
    }

    QString const freq = QString::number(freqKhz, 'f', 1);
    QString const line = QStringLiteral("DX %1 %2 %3\r\n").arg(freq, call, spotComment);
    qint64 const written = m_socket->write(line.toUtf8());
    if (written < 0) {
        QString const msg = tr("Cannot send spot: %1").arg(m_socket->errorString());
        emit errorOccurred(msg);
        return false;
    }
    m_socket->flush();
    setLastStatus(tr("Spot inviato: %1 %2 kHz").arg(call, freq));
    emit statusUpdate(tr("Spot sent: %1 %2 kHz").arg(call, freq));
    return true;
}

void DecodiumDxCluster::clearSpots()
{
    m_spots.clear();
    emit spotsChanged();
}

// ---------------------------------------------------------------------------
// Private slots — socket events
// ---------------------------------------------------------------------------

void DecodiumDxCluster::onConnected()
{
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }
    m_connectSequenceActive = false;
    m_connected = true;
    emit connectedChanged();
    setLastStatus(tr("Connesso a %1, attendo il prompt di login…")
                      .arg(endpointLabel(m_activeHost, m_activePort)));
    emit statusUpdate(tr("Connected to %1. Waiting for login prompt…")
                          .arg(endpointLabel(m_activeHost, m_activePort)));
}

void DecodiumDxCluster::onDisconnected()
{
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }

    bool const wasConnected = m_connected;
    m_connected = false;
    m_loginSent = false;
    m_rxBuf.clear();
    if (wasConnected) {
        emit connectedChanged();
    }

    if (m_connectSequenceActive) {
        return;
    }

    if (m_manualDisconnect) {
        m_manualDisconnect = false;
    }

    setLastStatus(tr("Disconnesso"));
    emit statusUpdate(tr("Disconnected from DX cluster."));
}

void DecodiumDxCluster::onReadyRead()
{
    if (!m_socket) return;

    // Append all available bytes to the receive buffer.
    m_rxBuf += QString::fromUtf8(m_socket->readAll());

    if (!m_loginSent && bufferContainsClusterPrompt(m_rxBuf)) {
        sendLogin();
    }

    // Process every complete line (terminated by \n).
    // We handle both \r\n and bare \n.
    int pos = 0;
    while (true) {
        int lf = m_rxBuf.indexOf('\n', pos);
        if (lf < 0) break;

        // Extract the line, stripping trailing \r if present.
        QString line = m_rxBuf.mid(pos, lf - pos);
        if (line.endsWith('\r'))
            line.chop(1);

        pos = lf + 1;

        if (!line.isEmpty())
            processLine(line);
    }

    // Keep only the unprocessed tail in the buffer.
    m_rxBuf = m_rxBuf.mid(pos);
}

void DecodiumDxCluster::onError(QAbstractSocket::SocketError socketError)
{
    if (m_ignoreNextSocketError) {
        m_ignoreNextSocketError = false;
        return;
    }

    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->stop();
    }

    const QString fallback = m_socket ? m_socket->errorString() : tr("Unknown socket error");
    const QString msg = socketErrorMessage(socketError, fallback);

    if (m_connectSequenceActive && !m_connected && tryNextConnectionCandidate(msg)) {
        return;
    }

    bool const wasConnected = m_connected;
    m_connectSequenceActive = false;
    m_connected = false;
    m_loginSent = false;
    if (wasConnected) {
        emit connectedChanged();
    }
    setLastStatus(tr("Errore: %1").arg(msg));
    emit errorOccurred(tr("Socket error: %1").arg(msg));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void DecodiumDxCluster::processLine(const QString& line)
{
    // ---- Login prompt detection ----
    // Cluster servers typically send something like:
    //   "login: ", "Please enter your call: ", "your callsign: ", etc.
    if (!m_loginSent) {
        const QString lower = line.toLower();
        if (lower.contains("login")   ||
            lower.contains("call")    ||
            lower.contains("please")) {
            sendLogin();
            return;
        }
    }

    // ---- DX spot line ----
    if (line.startsWith("DX de ", Qt::CaseInsensitive)) {
        QVariantMap spot = parseSpotLine(line);
        if (!spot.isEmpty()) {
            // Maintain a capped ring-buffer: remove oldest entries from front.
            while (m_spots.size() >= k_maxSpots)
                m_spots.removeFirst();

            m_spots.append(spot);
            emit newSpot(spot);
            emit spotsChanged();
        }
        return;
    }

    // ---- Everything else → plain status update ----
    emit statusUpdate(line);
}

QVariantMap DecodiumDxCluster::parseSpotLine(const QString& line) const
{
    // Canonical DX-cluster spot format:
    //   DX de IK1XXX:    14074.0  JA1YYY        FT8 -12 dB  1234 Hz        1234Z JN12
    //          ^^^^^^    ^^^^^^^  ^^^^^^         ^^^^^^^^^^^^^^^^^^^^^^^^    ^^^^^
    //          spotter   freq     dxCall         comment                     time
    //
    // The regex is intentionally tolerant of variable whitespace.
    static const QRegularExpression re(
        R"(^DX de\s+(\S+?):\s+(\d+\.?\d*)\s+(\S+)\s+(.*?)\s+(\d{4}Z)\s*(.*)$)",
        QRegularExpression::CaseInsensitiveOption
    );

    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch()) {
        qDebug() << "[DxCluster] Could not parse spot line:" << line;
        return {};
    }

    const QString spotter   = m.captured(1).toUpper();
    const double  freqKhz   = m.captured(2).toDouble();
    if (freqKhz <= 0.0) return {};   // reject malformed frequency
    const QString dxCall    = m.captured(3).toUpper();
    const QString comment   = m.captured(4).trimmed();
    const QString time      = m.captured(5);          // e.g. "1234Z"
    // m.captured(6) → extra trailing info (locator, etc.) — kept in comment

    // Detect digital mode from the comment field.
    QString mode;
    const QString commentUpper = comment.toUpper();
    if      (commentUpper.contains("FT8"))  mode = "FT8";
    else if (commentUpper.contains("FT4"))  mode = "FT4";
    else if (commentUpper.contains("FT2"))  mode = "FT2";
    else if (commentUpper.contains("JS8"))  mode = "JS8";
    else if (commentUpper.contains("JT65")) mode = "JT65";
    else if (commentUpper.contains("JT9"))  mode = "JT9";
    else if (commentUpper.contains("Q65"))  mode = "Q65";
    else if (commentUpper.contains("CW"))   mode = "CW";
    else if (commentUpper.contains("SSB"))  mode = "SSB";
    else                                    mode = "DATA";

    QVariantMap spot;
    spot["spotter"]   = spotter;
    spot["dxCall"]    = dxCall;
    spot["frequency"] = freqKhz;
    spot["comment"]   = comment;
    spot["time"]      = time;
    spot["band"]      = bandFromFreq(freqKhz);
    spot["mode"]      = mode;
    // Use current UTC wall-clock time for sorting / display in the UI.
    spot["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    return spot;
}

QString DecodiumDxCluster::bandFromFreq(double freqKhz) const
{
    // Boundaries chosen so that any frequency that falls within a standard
    // amateur allocation maps to the correct band name.
    if      (freqKhz <   1800.0) return "UNK";   // below 160m or invalid
    else if (freqKhz <   2000.0) return "160M";
    else if (freqKhz <   4000.0) return "80M";
    else if (freqKhz <   6000.0) return "60M";
    else if (freqKhz <   8000.0) return "40M";
    else if (freqKhz <  11000.0) return "30M";
    else if (freqKhz <  15000.0) return "20M";
    else if (freqKhz <  19000.0) return "17M";
    else if (freqKhz <  22000.0) return "15M";
    else if (freqKhz <  25000.0) return "12M";
    else if (freqKhz <  30000.0) return "10M";
    else if (freqKhz <  52000.0) return "6M";
    else if (freqKhz < 150000.0) return "2M";
    else if (freqKhz < 440000.0) return "70CM";
    else                          return "UNK";
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------

void DecodiumDxCluster::saveSettings()
{
    QSettings s("Decodium", "Decodium3");
    beginConfiguredSettingsGroup(s);
    s.beginGroup("DXCluster");
    s.setValue("host",     m_host);
    s.setValue("port",     m_port);
    s.setValue("callsign", m_callsign);
    s.endGroup();
    s.setValue("DXClusterHost", m_host);
    s.setValue("DXClusterPort", m_port);
    if (!m_callsign.trimmed().isEmpty()) {
        s.setValue("DXClusterViewLogin", m_callsign.trimmed().toUpper());
    }
    s.sync();
}

void DecodiumDxCluster::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    beginConfiguredSettingsGroup(s);
    QString loadedHost;
    int loadedPort = 0;
    QString loadedCallsign;

    s.beginGroup("DXCluster");
    if (s.contains("host")) {
        loadedHost = s.value("host").toString().trimmed();
    }
    if (s.contains("port")) {
        loadedPort = s.value("port").toInt();
    }
    if (s.contains("callsign")) {
        loadedCallsign = s.value("callsign").toString().trimmed();
    }
    s.endGroup();

    if (loadedHost.isEmpty()) {
        loadedHost = s.value("DXClusterHost").toString().trimmed();
    }
    if (loadedPort <= 0) {
        loadedPort = s.value("DXClusterPort", kPrimaryClusterPort).toInt();
    }
    if (loadedCallsign.isEmpty()) {
        loadedCallsign = s.value("DXClusterViewLogin").toString().trimmed();
    }
    if (loadedCallsign.isEmpty()) {
        loadedCallsign = s.value("MyCall").toString().trimmed();
    }

    int portFromHost = 0;
    loadedHost = normalizeClusterHost(loadedHost, &portFromHost);
    if (portFromHost > 0) {
        loadedPort = portFromHost;
    }
    if (loadedHost.isEmpty()) {
        loadedHost = kPrimaryClusterHost;
    }
    loadedPort = qBound(1, loadedPort > 0 ? loadedPort : kPrimaryClusterPort, 65535);

    if (isDeprecatedClusterEndpoint(loadedHost, loadedPort)) {
        loadedHost = kPrimaryClusterHost;
        loadedPort = kPrimaryClusterPort;
    }

    setHost(loadedHost);
    setPort(loadedPort);
    if (!loadedCallsign.isEmpty()) {
        setCallsign(loadedCallsign.trimmed().toUpper());
    }
}

void DecodiumDxCluster::setLastStatus(const QString& msg)
{
    if (m_lastStatus == msg) {
        return;
    }
    m_lastStatus = msg;
    emit lastStatusChanged();
}
