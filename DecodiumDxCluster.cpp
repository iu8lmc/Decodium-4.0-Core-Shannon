#include "DecodiumDxCluster.h"
#include <QAbstractSocket>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDateTime>
#include <QSettings>
#include <QTimer>
#include <QDebug>

namespace
{
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

    m_loginSent = false;
    m_rxBuf.clear();

    setLastStatus(tr("Connessione a %1:%2 in corso…").arg(m_host).arg(m_port));
    emit statusUpdate(tr("Connecting to %1:%2 …").arg(m_host).arg(m_port));
    m_socket->connectToHost(m_host, static_cast<quint16>(m_port));

    // Async; onConnected() or onError() will fire via the event loop.
    // The socket internally applies a connection timeout (default ~30 s on
    // Windows). We request an explicit 10-second abort via a single-shot timer.
    QTimer::singleShot(10000, this, [this]() {
        if (m_socket &&
            m_socket->state() == QAbstractSocket::ConnectingState) {
            m_socket->abort();
            setLastStatus(tr("Timeout su %1:%2").arg(m_host).arg(m_port));
            emit errorOccurred(tr("Connection to %1:%2 timed out.").arg(m_host).arg(m_port));
        }
    });
}

void DecodiumDxCluster::disconnectCluster()
{
    if (!m_socket || m_socket->state() == QAbstractSocket::UnconnectedState) {
        return;
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
    m_connected = true;
    emit connectedChanged();
    setLastStatus(tr("Connesso a %1:%2, attendo il prompt di login…").arg(m_host).arg(m_port));
    emit statusUpdate(tr("Connected to %1:%2. Waiting for login prompt…").arg(m_host).arg(m_port));
}

void DecodiumDxCluster::onDisconnected()
{
    m_connected  = false;
    m_loginSent  = false;
    m_rxBuf.clear();
    emit connectedChanged();
    setLastStatus(tr("Disconnesso"));
    emit statusUpdate(tr("Disconnected from DX cluster."));
}

void DecodiumDxCluster::onReadyRead()
{
    if (!m_socket) return;

    // Append all available bytes to the receive buffer.
    m_rxBuf += QString::fromUtf8(m_socket->readAll());

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
    const QString fallback = m_socket ? m_socket->errorString() : tr("Unknown socket error");
    const QString msg = socketErrorMessage(socketError, fallback);
    setLastStatus(tr("Errore: %1").arg(msg));
    emit errorOccurred(tr("Socket error: %1").arg(msg));

    // Reset state; the socket stays alive so the user can retry.
    m_connected = false;
    m_loginSent = false;
    emit connectedChanged();
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

            QString loginLine = m_callsign.trimmed() + "\r\n";
            m_socket->write(loginLine.toUtf8());
            m_loginSent = true;
            setLastStatus(tr("Login inviato come %1").arg(m_callsign.trimmed()));
            emit statusUpdate(tr("Login sent (%1).").arg(m_callsign.trimmed()));
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
    if      (freqKhz <   2000.0) return "160M";
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
    s.sync();
}

void DecodiumDxCluster::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    beginConfiguredSettingsGroup(s);
    s.beginGroup("DXCluster");
    if (s.contains("host"))     setHost(s.value("host").toString());
    if (s.contains("port"))     setPort(s.value("port").toInt());
    if (s.contains("callsign")) setCallsign(s.value("callsign").toString());
    s.endGroup();
}

void DecodiumDxCluster::setLastStatus(const QString& msg)
{
    if (m_lastStatus == msg) {
        return;
    }
    m_lastStatus = msg;
    emit lastStatusChanged();
}
