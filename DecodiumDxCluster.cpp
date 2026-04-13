#include "DecodiumDxCluster.h"
#include <QAbstractSocket>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

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
    qDebug() << "DxCluster::connectCluster() called — host:" << m_host << "port:" << m_port << "call:" << m_callsign;

    // Se il socket è in uno stato stale (non Unconnected), resettalo
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "DxCluster: socket state =" << m_socket->state() << "— resetting";
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
        m_connected = false;
        m_loginSent = false;
        emit connectedChanged();
    }

    if (m_callsign.trimmed().isEmpty()) {
        emit errorOccurred(tr("Callsign non impostato. Configura il callsign in Impostazioni → Stazione."));
        emit statusUpdate(tr("ERRORE: callsign vuoto — impossibile connettersi."));
        return;
    }

    emit statusUpdate(tr("Tentativo connessione a %1:%2 come %3...").arg(m_host).arg(m_port).arg(m_callsign));

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

    emit statusUpdate(tr("Connecting to %1:%2 …").arg(m_host).arg(m_port));
    m_socket->connectToHost(m_host, static_cast<quint16>(m_port));

    // Async; onConnected() or onError() will fire via the event loop.
    // Timeout 20s per server lenti (alcuni cluster hanno handshake lungo)
    QTimer::singleShot(20000, this, [this]() {
        if (m_socket &&
            m_socket->state() == QAbstractSocket::ConnectingState) {
            m_socket->abort();
            emit errorOccurred(tr("Connection to %1:%2 timed out (20s).").arg(m_host).arg(m_port));
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
    qDebug() << "DxCluster::onConnected() — TCP connection established to" << m_host << m_port;
    m_connected = true;
    emit connectedChanged();
    emit statusUpdate(tr("Connesso a %1:%2 — attendo prompt login…").arg(m_host).arg(m_port));
}

void DecodiumDxCluster::onDisconnected()
{
    m_connected  = false;
    m_loginSent  = false;
    m_rxBuf.clear();
    emit connectedChanged();
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

    // Il prompt "login: " spesso arriva SENZA newline.
    // Se abbiamo dati residui nel buffer che sembrano un prompt, processiamoli.
    if (!m_loginSent && !m_rxBuf.trimmed().isEmpty()) {
        QString tail = m_rxBuf.trimmed().toLower();
        if (tail.endsWith(":") || tail.endsWith(">") || tail.contains("login")) {
            processLine(m_rxBuf.trimmed());
            m_rxBuf.clear();
        }
    }
}

void DecodiumDxCluster::onError(QAbstractSocket::SocketError socketError)
{
    qDebug() << "DxCluster::onError() — socketError:" << socketError;
    const QString msg = m_socket ? m_socket->errorString() : tr("Unknown socket error");
    emit errorOccurred(tr("Errore socket: %1").arg(msg));
    emit statusUpdate(tr("ERRORE: %1").arg(msg));

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
    // Emetti ogni riga raw per il terminal telnet
    emit statusUpdate(line);

    // ---- Login prompt detection ----
    // Cluster servers tipicamente mandano un messaggio di benvenuto e poi
    // un prompt come "login: " o "callsign: ". Dobbiamo aspettare il prompt
    // finale (che termina con ": " o "> ") prima di mandare il callsign.
    // Esempio IZ7AUH: "Welcome to..." poi "Please enter..." poi "login: "
    if (!m_loginSent) {
        const QString trimmed = line.trimmed();
        const QString lower = trimmed.toLower();
        // Il prompt di login è tipicamente una riga corta che termina con ":" o ">"
        // Esempio: "login:", "callsign:", "Username:", ">"
        bool isLoginPrompt = (lower == "login:" ||
                              lower.endsWith("login:") ||
                              lower.endsWith("callsign:") ||
                              lower.endsWith("username:") ||
                              (lower.endsWith(":") && lower.length() < 40) ||
                              (lower.endsWith(">") && lower.length() < 20));
        if (isLoginPrompt) {
            QString loginLine = m_callsign.trimmed() + "\r\n";
            m_socket->write(loginLine.toUtf8());
            m_socket->flush();
            m_loginSent = true;
            emit statusUpdate(tr("Login: %1 → %2:%3").arg(m_callsign.trimmed()).arg(m_host).arg(m_port));
            // Invia SET/NOECHO per evitare double echo dal server
            QTimer::singleShot(2000, this, [this]() {
                if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                    m_socket->write("SET/NOECHO\r\n");
                    m_socket->flush();
                }
            });
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
    s.beginGroup("DXCluster");
    s.setValue("host",     m_host);
    s.setValue("port",     m_port);
    s.setValue("callsign", m_callsign);
    s.endGroup();
}

void DecodiumDxCluster::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("DXCluster");
    setHost(s.value("host", "dx.iz7auh.net").toString());
    int savedPort = s.value("port", 8000).toInt();
    // Migrazione: se la vecchia porta era 7300 (default WSJT-X), usa 8000
    if (savedPort == 7300 && m_host.contains("iz7auh")) savedPort = 8000;
    setPort(savedPort);
    if (s.contains("callsign")) setCallsign(s.value("callsign").toString());
    s.endGroup();
}
