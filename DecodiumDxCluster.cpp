#include "DecodiumDxCluster.h"
#include <QAbstractSocket>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QPointer>

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

void appendDxClusterTrace(QString const& text)
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dirPath.isEmpty()) {
        dirPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    if (dirPath.isEmpty()) {
        return;
    }
    QDir().mkpath(dirPath);
    QFile f(QDir(dirPath).absoluteFilePath(QStringLiteral("autospot.log")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        return;
    }
    QTextStream out(&f);
    out << QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyMMdd_hhmmss "))
        << text
        << Qt::endl;
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

bool payloadContainsClusterLoginPrompt(QByteArray const& payload)
{
    auto const text = QString::fromLatin1(payload).simplified();
    auto const lower = text.toLower();
    return lower.contains(QStringLiteral("login:"))
        || lower.contains(QStringLiteral("call:"))
        || lower.contains(QStringLiteral("callsign"))
        || lower.contains(QStringLiteral("enter your"))
        || lower.contains(QStringLiteral("please enter"))
        || text.endsWith(QLatin1Char(':'));
}

bool payloadContainsClusterPrompt(QByteArray const& payload)
{
    auto const text = QString::fromLatin1(payload).simplified();
    auto const lower = text.toLower();
    return payloadContainsClusterLoginPrompt(payload)
        || text.endsWith(QLatin1Char('>'))
        || (lower.contains(QStringLiteral(" de ")) && text.endsWith(QLatin1Char('>')));
}

QString clusterPayloadSummary(QByteArray const& payload)
{
    auto text = QString::fromLatin1(payload);
    text.replace(QRegularExpression(QStringLiteral("[\\x00-\\x1F ]+")), QStringLiteral(" "));
    return text.simplified().left(240);
}

QString clusterPayloadTrace(QByteArray const& payload)
{
    auto text = QString::fromLatin1(payload);
    text.replace(QStringLiteral("\r\n"), QStringLiteral(" | "));
    text.replace(QChar {'\r'}, QChar {' '});
    text.replace(QChar {'\n'}, QStringLiteral(" | "));
    text.replace(QRegularExpression(QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]+")),
                 QStringLiteral(" "));
    return text.simplified().left(900);
}

bool clusterPayloadHasRejection(QByteArray const& payload, QString* detail = nullptr)
{
    auto const summary = clusterPayloadSummary(payload);
    auto const lower = summary.toLower();
    bool const rejected = lower.contains(QStringLiteral("need a callsign"))
        || lower.contains(QStringLiteral("usage:"))
        || lower.contains(QStringLiteral("invalid callsign"))
        || lower.contains(QStringLiteral("invalid frequency"))
        || lower.contains(QStringLiteral("bad spot"))
        || lower.contains(QStringLiteral("unknown command"))
        || lower.contains(QStringLiteral("not allowed"))
        || lower.contains(QStringLiteral("not permitted"))
        || lower.contains(QStringLiteral("cannot spot"))
        || lower.contains(QStringLiteral("self spot"))
        || lower.contains(QStringLiteral("too many spots"))
        || lower.contains(QStringLiteral("sorry"))
        || lower.contains(QStringLiteral("reconnected as"))
        || lower.contains(QStringLiteral("instance is disconnected"));
    if (detail) {
        *detail = summary;
    }
    return rejected;
}

QString baseCallForClusterCheck(QString call)
{
    call = call.trimmed().toUpper();
    auto const parts = call.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() > 1) {
        QString best;
        for (auto const& part : parts) {
            bool hasDigit = false;
            for (auto const ch : part) {
                if (ch.isDigit()) {
                    hasDigit = true;
                    break;
                }
            }
            if (hasDigit && part.size() > best.size()) {
                best = part;
            }
        }
        if (!best.isEmpty()) {
            return best;
        }
        return parts.first();
    }
    return call;
}

bool clusterPayloadShowsSubmittedSpot(QString const& myCall, QString const& dxCall, QByteArray const& payload)
{
    QString const fullMyCall = myCall.trimmed().toUpper();
    QString const baseMyCall = baseCallForClusterCheck(fullMyCall);
    QString const expectedDx = dxCall.trimmed().toUpper();
    auto const lines = QString::fromLatin1(payload)
                           .split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                  Qt::SkipEmptyParts);
    for (auto const& line : lines) {
        auto const text = line.toUpper();
        if (!text.contains(expectedDx)) {
            continue;
        }
        if (text.contains(QStringLiteral("<%1>").arg(fullMyCall))
            || (!baseMyCall.isEmpty() && text.contains(QStringLiteral("<%1>").arg(baseMyCall)))
            || text.contains(QStringLiteral("DX DE %1").arg(fullMyCall))
            || (!baseMyCall.isEmpty() && text.contains(QStringLiteral("DX DE %1").arg(baseMyCall)))) {
            return true;
        }
    }
    return false;
}

void consumeTelnetClusterPayload(QTcpSocket* socket,
                                 QByteArray* pending,
                                 QByteArray* buffer,
                                 QByteArray raw)
{
    if (!socket || !buffer) {
        return;
    }
    if (pending && !pending->isEmpty()) {
        raw.prepend(*pending);
        pending->clear();
    }

    for (int i = 0; i < raw.size();) {
        auto const byte = static_cast<unsigned char>(raw.at(i));
        if (byte != 0xFF) {
            buffer->append(raw.at(i));
            ++i;
            continue;
        }

        if (i + 1 >= raw.size()) {
            if (pending) {
                *pending = raw.mid(i);
            }
            break;
        }

        auto const cmd = static_cast<unsigned char>(raw.at(i + 1));
        if (cmd == 0xFF) {
            buffer->append(char(0xFF));
            i += 2;
            continue;
        }

        if (cmd == 0xFA) {
            int end = -1;
            for (int j = i + 2; j + 1 < raw.size(); ++j) {
                if (static_cast<unsigned char>(raw.at(j)) == 0xFF
                    && static_cast<unsigned char>(raw.at(j + 1)) == 0xF0) {
                    end = j + 2;
                    break;
                }
            }
            if (end < 0) {
                if (pending) {
                    *pending = raw.mid(i);
                }
                break;
            }
            i = end;
            continue;
        }

        if (cmd >= 0xFB && cmd <= 0xFE) {
            if (i + 2 >= raw.size()) {
                if (pending) {
                    *pending = raw.mid(i);
                }
                break;
            }

            auto const opt = static_cast<unsigned char>(raw.at(i + 2));
            char reply[3] = {char(0xFF), 0, char(opt)};
            bool const accepted = opt == 1 || opt == 3;
            if (cmd == 0xFB) {
                reply[1] = accepted ? char(0xFD) : char(0xFE);
            } else if (cmd == 0xFD) {
                reply[1] = accepted ? char(0xFB) : char(0xFC);
            } else {
                i += 3;
                continue;
            }

            socket->write(reply, 3);
            socket->flush();
            i += 3;
            continue;
        }

        i += 2;
    }
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

bool DecodiumDxCluster::submitSpotVerified(const QString& dxCall, double freqKhz, const QString& comment)
{
    QString call = baseCallForClusterCheck(dxCall);
    if (call.isEmpty() || freqKhz <= 0.0) {
        emit errorOccurred(tr("Cannot send spot: invalid call or frequency."));
        return false;
    }

    QString myCall = m_callsign.trimmed().toUpper();
    if (myCall.isEmpty()) {
        emit errorOccurred(tr("Cannot send spot: callsign not set."));
        return false;
    }

    int portFromHost = 0;
    QString host = m_activeHost.isEmpty() ? normalizeClusterHost(m_host, &portFromHost) : m_activeHost;
    int port = m_activePort > 0 ? m_activePort : (portFromHost > 0 ? portFromHost : m_port);
    port = qBound(1, port > 0 ? port : kPrimaryClusterPort, 65535);
    if (host.isEmpty()) {
        host = kPrimaryClusterHost;
    }

    if (isDeprecatedClusterEndpoint(host, port)) {
        QString const msg = tr("AutoSpot skipped: %1:%2 is read-only. Configure a writable DX cluster endpoint.")
                                .arg(host, QString::number(port));
        setLastStatus(msg);
        emit errorOccurred(msg);
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
    QString const spotLine = QStringLiteral("DX %1 %2 %3\r\n").arg(freq, call, spotComment);
    QString const verifyCommand = QStringLiteral("show/dx 200 by %1\r\n").arg(myCall);

    auto* socket = new QTcpSocket(this);
    auto* timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    timeout->setInterval(22000);

    socket->setProperty("decodium_verified_spot", true);
    socket->setProperty("spot_done", false);
    socket->setProperty("login_sent", false);
    socket->setProperty("login_prompt_seen", false);
    socket->setProperty("spot_sent", false);
    socket->setProperty("spot_response_logged", false);
    socket->setProperty("verify_pending", false);
    socket->setProperty("verify_sent", false);
    socket->setProperty("verify_response_logged", false);
    socket->setProperty("cluster_buffer", QByteArray {});
    socket->setProperty("cluster_telnet_pending", QByteArray {});
    socket->setProperty("spot_call", call);
    socket->setProperty("spot_host", host);
    socket->setProperty("spot_port", port);

    QPointer<DecodiumDxCluster> guard {this};
    auto finishSpot = [guard, socket, timeout](bool success, QString const& detail, QString const& traceTag) {
        if (!socket || socket->property("spot_done").toBool()) {
            return;
        }

        socket->setProperty("spot_done", true);
        timeout->stop();

        QString const callText = socket->property("spot_call").toString();
        QString const hostText = socket->property("spot_host").toString();
        int const portValue = socket->property("spot_port").toInt();
        QString message = success
            ? QObject::tr("AutoSpot verified for %1 on %2:%3")
                  .arg(callText, hostText, QString::number(portValue))
            : QObject::tr("AutoSpot rejected for %1 on %2:%3")
                  .arg(callText, hostText, QString::number(portValue));
        if (!detail.isEmpty()) {
            message += QStringLiteral(" (%1)").arg(detail);
        }

        appendDxClusterTrace(QStringLiteral("%1 %2").arg(traceTag, message));
        qInfo().noquote() << "[DxCluster]" << traceTag << message;
        if (guard) {
            guard->setLastStatus(message);
            if (success) {
                emit guard->statusUpdate(message);
            } else {
                emit guard->errorOccurred(message);
            }
        }

        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->write(QByteArrayLiteral("bye\r\n"));
            socket->flush();
            socket->disconnectFromHost();
        } else {
            socket->deleteLater();
        }
    };

    connect(timeout, &QTimer::timeout, socket, [socket, finishSpot] {
        QString const detail = clusterPayloadSummary(socket->property("cluster_buffer").toByteArray());
        finishSpot(false,
                   detail.isEmpty()
                       ? QObject::tr("timeout waiting for cluster response")
                       : QObject::tr("timeout waiting for cluster response: %1").arg(detail),
                   QStringLiteral("TIMEOUT"));
    });

    connect(socket, &QAbstractSocket::errorOccurred, socket,
            [socket, finishSpot](QAbstractSocket::SocketError) {
        if (socket->property("spot_done").toBool()) {
            return;
        }
        finishSpot(false, socket->errorString(), QStringLiteral("ERROR"));
    });

    connect(socket, &QTcpSocket::connected, socket, [timeout, host, port, call, freq, spotLine] {
        appendDxClusterTrace(QStringLiteral("SUBMIT %1:%2 | %3")
                                 .arg(host, QString::number(port), spotLine.trimmed()));
        qInfo().noquote() << "[DxCluster] AutoSpot submit"
                          << QStringLiteral("%1:%2").arg(host, QString::number(port))
                          << call << freq;
        timeout->start();
    });

    connect(socket, &QTcpSocket::readyRead, socket,
            [socket, timeout, myCall, call, spotLine, verifyCommand, finishSpot] {
        QByteArray buffer = socket->property("cluster_buffer").toByteArray();
        QByteArray pending = socket->property("cluster_telnet_pending").toByteArray();
        consumeTelnetClusterPayload(socket, &pending, &buffer, socket->readAll());
        socket->setProperty("cluster_telnet_pending", pending);
        socket->setProperty("cluster_buffer", buffer);

        QString rejectionDetail;
        if (clusterPayloadHasRejection(buffer, &rejectionDetail)) {
            finishSpot(false, rejectionDetail, QStringLiteral("REJECT"));
            return;
        }

        if (!socket->property("login_sent").toBool()) {
            if (!payloadContainsClusterLoginPrompt(buffer) && !payloadContainsClusterPrompt(buffer)) {
                return;
            }
            QString const trace = clusterPayloadTrace(buffer);
            appendDxClusterTrace(QStringLiteral("LOGIN-PROMPT %1").arg(trace));
            qInfo().noquote() << "[DxCluster] LOGIN-PROMPT" << trace;
            socket->write((myCall + QStringLiteral("\r\n")).toUtf8());
            socket->flush();
            socket->setProperty("login_sent", true);
            socket->setProperty("cluster_buffer", QByteArray {});
            timeout->start();
            return;
        }

        if (!socket->property("login_prompt_seen").toBool()) {
            if (!payloadContainsClusterPrompt(buffer)) {
                return;
            }
            QString const trace = clusterPayloadTrace(buffer);
            appendDxClusterTrace(QStringLiteral("LOGIN-OK %1").arg(trace));
            qInfo().noquote() << "[DxCluster] LOGIN-OK" << trace;
            socket->write(spotLine.toUtf8());
            socket->flush();
            appendDxClusterTrace(QStringLiteral("SPOT-TX %1").arg(spotLine.trimmed()));
            qInfo().noquote() << "[DxCluster] SPOT-TX" << spotLine.trimmed();
            socket->setProperty("login_prompt_seen", true);
            socket->setProperty("spot_sent", true);
            socket->setProperty("cluster_buffer", QByteArray {});
            timeout->start();
            return;
        }

        if (!socket->property("spot_sent").toBool()) {
            return;
        }

        if (!socket->property("verify_sent").toBool()) {
            if (socket->property("verify_pending").toBool()) {
                return;
            }
            if (!socket->property("spot_response_logged").toBool()) {
                QString const trace = clusterPayloadTrace(buffer);
                if (!trace.isEmpty()) {
                    appendDxClusterTrace(QStringLiteral("SPOT-RX %1").arg(trace));
                    qInfo().noquote() << "[DxCluster] SPOT-RX" << trace;
                    socket->setProperty("spot_response_logged", true);
                }
            }
            if (!payloadContainsClusterPrompt(buffer)) {
                return;
            }

            timeout->start();
            socket->setProperty("verify_pending", true);

            QTimer::singleShot(5000, socket, [socket, timeout, verifyCommand, myCall, call, finishSpot] {
                if (socket->property("spot_done").toBool()) {
                    return;
                }
                socket->write(verifyCommand.toUtf8());
                socket->flush();
                appendDxClusterTrace(QStringLiteral("VERIFY-TX %1").arg(verifyCommand.trimmed()));
                qInfo().noquote() << "[DxCluster] VERIFY-TX" << verifyCommand.trimmed();
                socket->setProperty("verify_pending", false);
                socket->setProperty("verify_sent", true);
                socket->setProperty("verify_response_logged", false);
                socket->setProperty("cluster_buffer", QByteArray {});
                timeout->start();

                QTimer::singleShot(6000, socket, [socket, myCall, call, finishSpot] {
                    if (socket->property("spot_done").toBool()) {
                        return;
                    }
                    QByteArray const verifyBuffer = socket->property("cluster_buffer").toByteArray();
                    if (!socket->property("verify_response_logged").toBool()) {
                        QString const trace = clusterPayloadTrace(verifyBuffer);
                        if (!trace.isEmpty()) {
                            appendDxClusterTrace(QStringLiteral("VERIFY-RX %1").arg(trace));
                            qInfo().noquote() << "[DxCluster] VERIFY-RX" << trace;
                        }
                        socket->setProperty("verify_response_logged", true);
                    }
                    if (clusterPayloadShowsSubmittedSpot(myCall, call, verifyBuffer)) {
                        finishSpot(true, QObject::tr("published in show/dx"), QStringLiteral("VERIFIED"));
                    } else {
                        QString const detail = clusterPayloadTrace(verifyBuffer);
                        finishSpot(false,
                                   detail.isEmpty()
                                       ? QObject::tr("node accepted the command, but the spot is not visible in show/dx")
                                       : QObject::tr("node accepted the command, but the spot is not visible in show/dx: %1").arg(detail),
                                   QStringLiteral("UNVERIFIED"));
                    }
                });
            });
            return;
        }

        if (!socket->property("verify_response_logged").toBool()) {
            QString const trace = clusterPayloadTrace(buffer);
            if (!trace.isEmpty()) {
                appendDxClusterTrace(QStringLiteral("VERIFY-RX %1").arg(trace));
                qInfo().noquote() << "[DxCluster] VERIFY-RX" << trace;
                socket->setProperty("verify_response_logged", true);
            }
        }
        if (clusterPayloadShowsSubmittedSpot(myCall, call, buffer)) {
            finishSpot(true, QObject::tr("published in show/dx"), QStringLiteral("VERIFIED"));
        }
    });

    connect(socket, &QTcpSocket::disconnected, socket, [socket, finishSpot] {
        if (!socket->property("spot_done").toBool()) {
            QString const detail = clusterPayloadSummary(socket->property("cluster_buffer").toByteArray());
            finishSpot(false,
                       detail.isEmpty()
                           ? QObject::tr("connection closed before cluster confirmation")
                           : QObject::tr("connection closed before cluster confirmation: %1").arg(detail),
                       QStringLiteral("DROP"));
            return;
        }
        socket->deleteLater();
    });

    setLastStatus(tr("AutoSpot verification started for %1 on %2:%3")
                      .arg(call, host, QString::number(port)));
    socket->connectToHost(host, static_cast<quint16>(port));
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
