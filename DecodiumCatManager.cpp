#include "DecodiumCatManager.h"
#include "DecodiumLogging.hpp"

#include <cmath>
#include <QDebug>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSettings>

#include <algorithm>

namespace
{
QString normalizedRigName(QString const& rigName)
{
    QString normalized = rigName.trimmed().toUpper();
    normalized.remove(' ');
    return normalized;
}

bool isYaesuRig(QString const& rigName)
{
    QString const normalized = normalizedRigName(rigName);
    return normalized.contains(QStringLiteral("YAESU"))
        || normalized.contains(QStringLiteral("FT-"))
        || normalized.contains(QStringLiteral("FT"));
}

bool isKenwoodRig(QString const& rigName)
{
    QString const normalized = normalizedRigName(rigName);
    return normalized.contains(QStringLiteral("KENWOOD"))
        || normalized.contains(QStringLiteral("TS-"));
}

QString comparablePortName(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || 0 == value.compare(QStringLiteral("CAT"), Qt::CaseInsensitive))
        return QStringLiteral("CAT");
    if (value.startsWith(QStringLiteral("\\\\.\\")))
        value.remove(0, 4);
    if (value.startsWith(QStringLiteral("/dev/")))
        value.remove(0, 5);
    return value.toLower();
}

bool supportsYaesuDataPtt(QString const& rigName)
{
    QString const normalized = normalizedRigName(rigName);
    return normalized.contains(QStringLiteral("FT991"))
        || normalized.contains(QStringLiteral("FTDX10"))
        || normalized.contains(QStringLiteral("FTDX101"));
}

QString normalizedSerialPortName(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()
        || 0 == value.compare(QStringLiteral("CAT"), Qt::CaseInsensitive)
        || 0 == value.compare(QStringLiteral("None"), Qt::CaseInsensitive)) {
        return {};
    }

    if (value.startsWith(QStringLiteral("\\\\.\\")))
        value.remove(0, 4);

#if defined(Q_OS_WIN)
    static QRegularExpression const rx(QStringLiteral(R"(^COM(\d+)$)"),
                                       QRegularExpression::CaseInsensitiveOption);
    auto const match = rx.match(value);
    if (match.hasMatch())
        return QStringLiteral("COM") + match.captured(1);
#endif

    return value;
}

void appendUniqueSerialPort(QStringList& ports, QString const& rawPort)
{
    QString const port = normalizedSerialPortName(rawPort);
    if (!port.isEmpty() && !ports.contains(port, Qt::CaseInsensitive))
        ports << port;
}

int serialPortNumber(QString const& port)
{
    static QRegularExpression const rx(QStringLiteral(R"(^COM(\d+)$)"),
                                       QRegularExpression::CaseInsensitiveOption);
    auto const match = rx.match(port.trimmed());
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

void sortSerialPorts(QStringList& ports)
{
    std::sort(ports.begin(), ports.end(), [](QString const& a, QString const& b) {
        int const an = serialPortNumber(a);
        int const bn = serialPortNumber(b);
        if (an >= 0 && bn >= 0)
            return an < bn;
        if (an >= 0 || bn >= 0)
            return an >= 0;
        return QString::localeAwareCompare(a, b) < 0;
    });
}

QStringList enumerateSerialPorts(QString const& savedSerialPort, QString const& savedPttPort)
{
    QStringList ports;
    for (QSerialPortInfo const& info : QSerialPortInfo::availablePorts()) {
        appendUniqueSerialPort(ports, info.portName());
        appendUniqueSerialPort(ports, info.systemLocation());
    }

#if defined(Q_OS_WIN)
    QSettings serialMap(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM"),
                        QSettings::NativeFormat);
    for (QString const& key : serialMap.allKeys())
        appendUniqueSerialPort(ports, serialMap.value(key).toString());
#endif

    appendUniqueSerialPort(ports, savedSerialPort);
    appendUniqueSerialPort(ports, savedPttPort);
    sortSerialPorts(ports);
    return ports;
}

bool isLikelyDataMode(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized.contains(QStringLiteral("DATA"))
        || normalized.contains(QStringLiteral("DIG"))
        || normalized.contains(QStringLiteral("PKT"))
        || normalized.contains(QStringLiteral("RTTY"));
}

// Mappa il nome testuale del modo (USB, DATA-U, CW, ...) al codice a 1
// carattere usato dal comando Yaesu/Kenwood "MD<code>;". Restituisce 0 se
// il modo non è riconosciuto.
//
// Yaesu FT-991/FT-DX10/FT-DX101 codes:
//   1=LSB 2=USB 3=CW 4=FM 5=AM 6=RTTY-L 7=CW-R 8=DATA-L 9=RTTY-U
//   A=DATA-FM B=FM-N C=DATA-U D=AM-N
char yaesuModeCode(QString const& modeIn)
{
    QString const m = modeIn.trimmed().toUpper();
    if (m == "LSB")            return '1';
    if (m == "USB")            return '2';
    if (m == "CW")             return '3';
    if (m == "FM")             return '4';
    if (m == "AM")             return '5';
    if (m == "RTTY-L" || m == "RTTY") return '6';
    if (m == "CW-R")           return '7';
    if (m == "DATA-L" || m == "DATA-LSB" || m == "PKT-L") return '8';
    if (m == "RTTY-U")         return '9';
    if (m == "DATA-FM")        return 'A';
    if (m == "FM-N")           return 'B';
    if (m == "DATA-U" || m == "DATA-USB" || m == "DATA" || m == "PKT" || m == "PKT-U") return 'C';
    if (m == "AM-N")           return 'D';
    return 0;
}

// Tabella defaults per rig: baud/dataBits/stopBits/handshake/pttMethod + civAddress (ICOM).
// Valori allineati alle impostazioni di fabbrica piu' comuni per la porta CAT in modo DATA.
struct RigDefaults
{
    const char* match;     // substring case-insensitive sul nome rig
    int baud;
    const char* dataBits;
    const char* stopBits;
    const char* handshake;
    const char* pttMethod;
    int civAddress;        // 0 se non ICOM
};

// Ordine: match piu' specifici prima (es. "TS-590SG" prima di "TS-590S").
static const RigDefaults kRigDefaultsTable[] = {
    // Kenwood
    {"TS-590SG",   57600,  "8", "1", "none", "CAT", 0x00},
    {"TS-590S",    57600,  "8", "1", "none", "CAT", 0x00},
    {"TS-890S",   115200,  "8", "1", "none", "CAT", 0x00},
    {"TS-480",     57600,  "8", "1", "none", "CAT", 0x00},
    {"TS-2000",    57600,  "8", "1", "none", "CAT", 0x00},
    // Yaesu
    {"FT-DX101",   38400,  "8", "1", "none", "CAT", 0x00},
    {"FTDX101",    38400,  "8", "1", "none", "CAT", 0x00},
    {"FT-DX10",    38400,  "8", "1", "none", "CAT", 0x00},
    {"FTDX10",     38400,  "8", "1", "none", "CAT", 0x00},
    {"FT-991A",    38400,  "8", "1", "none", "CAT", 0x00},
    {"FT-991",     38400,  "8", "1", "none", "CAT", 0x00},
    {"FT991",      38400,  "8", "1", "none", "CAT", 0x00},
    // Icom (CI-V). I match piu' specifici devono precedere quelli generici.
    {"IC-7300MK2", 19200,  "8", "1", "none", "CAT", 0xB6},
    {"IC-7300",    19200,  "8", "1", "none", "CAT", 0x94},
    {"IC-7600",    19200,  "8", "1", "none", "CAT", 0x7A},
    {"IC-7610",    19200,  "8", "1", "none", "CAT", 0x98},
    {"IC-9700",    19200,  "8", "1", "none", "CAT", 0xA2},
    {"IC-705",     19200,  "8", "1", "none", "CAT", 0xA4},
    // Elecraft
    {"K3",         38400,  "8", "1", "none", "CAT", 0x00},
    {"KX3",        38400,  "8", "1", "none", "CAT", 0x00},
};

const RigDefaults* findRigDefaults(QString const& rigName)
{
    QString const upper = rigName.toUpper();
    for (auto const& e : kRigDefaultsTable) {
        if (upper.contains(QString::fromLatin1(e.match).toUpper()))
            return &e;
    }
    return nullptr;
}

QByteArray nativePttOnCommand(QString const& rigName, QString const& mode)
{
    // Kenwood TS-590S/SG, TS-480, TS-2000, TS-890S:
    //   TX;  / TX0; = trasmette da MIC
    //   TX1; = trasmette da DATA (USB audio / connettore DATA posteriore)
    // Decodium invia sempre l'audio via USB/sound card → percorso DATA.
    // Serve TX1; in qualsiasi modo (USB, DATA-U, DATA-L) per instradare
    // l'audio dal codec USB invece che dal MIC.
    if (isKenwoodRig(rigName)) {
        return QByteArrayLiteral("TX1;");
    }

    if (isYaesuRig(rigName)) {
        if (supportsYaesuDataPtt(rigName) && isLikelyDataMode(mode)) {
            return QByteArrayLiteral("TX1;");
        }
        return QByteArrayLiteral("TX;");
    }

    return QByteArrayLiteral("TX;");
}
}

// --- rigList ---

QStringList DecodiumCatManager::rigList() const
{
    return {
        "Kenwood TS-590S", "Kenwood TS-590SG", "Kenwood TS-480",
        "Kenwood TS-2000", "Kenwood TS-890S",
        "Icom IC-7300", "Icom IC-7300MK2", "Icom IC-7600", "Icom IC-7610", "Icom IC-9700", "Icom IC-705",
        "Yaesu FT-991", "Yaesu FT-991A", "Yaesu FT-DX10", "Yaesu FT-DX101D",
        "Elecraft K3", "Elecraft KX3",
    };
}

// --- setRigName + auto-apply defaults ---

void DecodiumCatManager::setRigName(const QString& v)
{
    if (m_rigName == v)
        return;
    m_rigName = v;
    emit rigNameChanged();
    applyRigDefaults(v);
}

void DecodiumCatManager::applyRigDefaults(const QString& rigName)
{
    const RigDefaults* d = findRigDefaults(rigName);
    if (!d) {
        DIAG_CAT(QStringLiteral("applyRigDefaults: nessun default per '%1' (usare hamlib)").arg(rigName));
        return;
    }

    DIAG_CAT(QStringLiteral("applyRigDefaults: '%1' -> baud=%2 data=%3 stop=%4 handshake=%5 ptt=%6 civ=0x%7")
                 .arg(rigName)
                 .arg(d->baud)
                 .arg(QLatin1String(d->dataBits))
                 .arg(QLatin1String(d->stopBits))
                 .arg(QLatin1String(d->handshake))
                 .arg(QLatin1String(d->pttMethod))
                 .arg(d->civAddress, 2, 16, QChar('0')));

    setBaudRate(d->baud);
    setDataBits(QString::fromLatin1(d->dataBits));
    setStopBits(QString::fromLatin1(d->stopBits));
    setHandshake(QString::fromLatin1(d->handshake));
    setPttMethod(QString::fromLatin1(d->pttMethod));
    setCivAddress(d->civAddress);
}

// --- ctor/dtor ---

DecodiumCatManager::DecodiumCatManager(QObject* parent)
    : QObject(parent)
{
    loadSettings();
}

DecodiumCatManager::~DecodiumCatManager()
{
    disconnectRig();
}

bool DecodiumCatManager::pttSharesCatPort() const
{
    QString const pttPort = m_pttPort.trimmed();
    if (pttPort.isEmpty() || 0 == pttPort.compare(QStringLiteral("CAT"), Qt::CaseInsensitive))
        return true;

    return comparablePortName(pttPort) == comparablePortName(m_serialPort);
}

bool DecodiumCatManager::forceDtrAvailable() const
{
    return !(0 == m_pttMethod.compare(QStringLiteral("DTR"), Qt::CaseInsensitive) && pttSharesCatPort());
}

bool DecodiumCatManager::forceRtsAvailable() const
{
    return !(0 == m_pttMethod.compare(QStringLiteral("RTS"), Qt::CaseInsensitive) && pttSharesCatPort());
}

void DecodiumCatManager::enforceForceLineAvailability()
{
    if (!forceDtrAvailable() && m_forceDtr) {
        m_forceDtr = false;
        emit forceDtrChanged();
    }
    if ((!forceDtrAvailable() || !m_forceDtr) && m_dtrHigh) {
        m_dtrHigh = false;
        emit dtrHighChanged();
    }
    if (!forceRtsAvailable() && m_forceRts) {
        m_forceRts = false;
        emit forceRtsChanged();
    }
    if ((!forceRtsAvailable() || !m_forceRts) && m_rtsHigh) {
        m_rtsHigh = false;
        emit rtsHighChanged();
    }
}

void DecodiumCatManager::setSerialPort(const QString& v)
{
    if (m_serialPort != v) {
        m_serialPort = v;
        emit serialPortChanged();
    }
    enforceForceLineAvailability();
}

void DecodiumCatManager::setHandshake(const QString& v)
{
    if (m_handshake != v) {
        m_handshake = v;
        emit handshakeChanged();
    }
    enforceForceLineAvailability();
}

void DecodiumCatManager::setForceDtr(bool v)
{
    bool const effective = forceDtrAvailable() ? v : false;
    if (m_forceDtr != effective) {
        m_forceDtr = effective;
        emit forceDtrChanged();
    }
    if (!m_forceDtr && m_dtrHigh) {
        m_dtrHigh = false;
        emit dtrHighChanged();
    }
}

void DecodiumCatManager::setDtrHigh(bool v)
{
    bool const effective = (forceDtrAvailable() && m_forceDtr) ? v : false;
    if (m_dtrHigh != effective) {
        m_dtrHigh = effective;
        emit dtrHighChanged();
    }
}

void DecodiumCatManager::setForceRts(bool v)
{
    bool const effective = forceRtsAvailable() ? v : false;
    if (m_forceRts != effective) {
        m_forceRts = effective;
        emit forceRtsChanged();
    }
    if (!m_forceRts && m_rtsHigh) {
        m_rtsHigh = false;
        emit rtsHighChanged();
    }
}

void DecodiumCatManager::setRtsHigh(bool v)
{
    bool const effective = (forceRtsAvailable() && m_forceRts) ? v : false;
    if (m_rtsHigh != effective) {
        m_rtsHigh = effective;
        emit rtsHighChanged();
    }
}

void DecodiumCatManager::setPttMethod(const QString& v)
{
    QString const normalized = v.trimmed().toUpper();
    QString const effective = normalized.isEmpty() ? QStringLiteral("CAT") : normalized;
    if (m_pttMethod != effective) {
        m_pttMethod = effective;
        emit pttMethodChanged();
    }
    enforceForceLineAvailability();
}

void DecodiumCatManager::setPttPort(const QString& v)
{
    if (m_pttPort != v) {
        m_pttPort = v;
        emit pttPortChanged();
    }
    enforceForceLineAvailability();
}

// --- connectRig ---

void DecodiumCatManager::connectRig()
{
    if (m_serial)
        disconnectRig();

    // Inizializza auto-baud: baud corrente + candidate comuni.
    // Si attiva solo al primo tentativo utente (indice -1); dopo un fail
    // iteriamo tramite tryNextAutoBaud() che chiama di nuovo connectRig().
    if (m_autoBaudIndex < 0) {
        m_autoBaudCandidates.clear();
        m_autoBaudCandidates << m_baudRate;
        for (int b : {9600, 19200, 38400, 57600, 115200}) {
            if (!m_autoBaudCandidates.contains(b))
                m_autoBaudCandidates << b;
        }
        m_autoBaudIndex = 0;
        m_autoBaudActive = true;
        QStringList seq;
        for (int b : m_autoBaudCandidates) seq << QString::number(b);
        DIAG_CAT(QStringLiteral("auto-baud: seq = %1").arg(seq.join(',')));
    }

    auto* serial = new QSerialPort(this);
    serial->setPortName(m_serialPort);
    serial->setBaudRate(m_baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        const QString err = serial->errorString();
        emit errorOccurred("CAT: impossibile aprire " + m_serialPort + " — " + err);
        serial->deleteLater();
        return;
    }
#if defined(Q_OS_LINUX)
    bool const autoDtrLow = forceDtrAvailable()
        && !m_forceDtr
        && !(0 == m_pttMethod.compare(QStringLiteral("DTR"), Qt::CaseInsensitive) && pttSharesCatPort());
    bool const autoRtsLow = forceRtsAvailable()
        && !m_forceRts
        && !(0 == m_pttMethod.compare(QStringLiteral("RTS"), Qt::CaseInsensitive) && pttSharesCatPort());
#else
    bool const autoDtrLow = false;
    bool const autoRtsLow = false;
#endif
    if (forceDtrAvailable() && (m_forceDtr || autoDtrLow))
        serial->setDataTerminalReady(m_forceDtr ? m_dtrHigh : false);
    if (forceRtsAvailable() && (m_forceRts || autoRtsLow))
        serial->setRequestToSend(m_forceRts ? m_rtsHigh : false);

    m_serial = serial;
    connect(m_serial, &QSerialPort::readyRead,
            this, &DecodiumCatManager::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &DecodiumCatManager::onSerialError);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &DecodiumCatManager::onPollTimer);
    m_rxBuf.clear();

    // Attesa 500ms: porte COM USB hanno bisogno di warm-up
    QTimer::singleShot(500, this, [this]() {
        if (m_serial && m_serial->isOpen())
            sendCommand("FA;");
    });
    m_pollTimer->start();

    // Timeout 4s: se il radio non risponde riporta errore chiaro
    if (m_connectTimer) { m_connectTimer->stop(); m_connectTimer->deleteLater(); }
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(4000);
    connect(m_connectTimer, &QTimer::timeout, this, &DecodiumCatManager::onConnectTimeout);
    m_connectTimer->start();

    emit statusUpdate("CAT: connessione a " + m_rigName + " su " + m_serialPort + "...");
    saveSettings();
}

// --- disconnectRig ---

void DecodiumCatManager::disconnectRig()
{
    // Quando disconnectRig e' chiamato dall'auto-baud iterator, m_autoBaudActive
    // rimane true e il tryNextAutoBaud ha gia' schedulato il reconnect.
    // Se invece e' disconnect utente, lasciamo che connectRig successivo
    // reinizializzi la sequenza.
    if (!m_autoBaudActive) {
        m_autoBaudIndex = -1;
        m_autoBaudCandidates.clear();
    }
    if (m_connectTimer) { m_connectTimer->stop(); m_connectTimer->deleteLater(); m_connectTimer = nullptr; }
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    if (m_pttSerial) {
        if (m_pttSerial->isOpen())
            m_pttSerial->close();
        delete m_pttSerial;
        m_pttSerial = nullptr;
    }
    if (m_serial) {
        m_serial->blockSignals(true);
        if (m_serial->isOpen())
            m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
    }
    m_rxBuf.clear();
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
    emit statusUpdate("CAT: disconnesso");
}

// --- sendCommand ---

void DecodiumCatManager::sendCommand(const QByteArray& cmd)
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->write(cmd);
        m_serial->flush();
    }
}

// --- onPollTimer ---

void DecodiumCatManager::onPollTimer()
{
    sendCommand("FA;");
    sendCommand("MD;");
}

// --- onReadyRead ---

void DecodiumCatManager::onReadyRead()
{
    if (!m_serial)
        return;

    m_rxBuf += m_serial->readAll();

    // Guard against unbounded buffer growth from corrupted serial data
    if (m_rxBuf.size() > 4096) {
        DIAG_CAT("CAT buffer overflow (" + QString::number(m_rxBuf.size()) + " bytes) — flushing");
        m_rxBuf.clear();
        return;
    }

    int idx;
    while ((idx = m_rxBuf.indexOf(';')) >= 0) {
        QByteArray msg = m_rxBuf.left(idx + 1);
        m_rxBuf.remove(0, idx + 1);
        processResponse(msg);
    }
}

// --- processResponse ---

void DecodiumCatManager::processResponse(const QByteArray& resp)
{
    if (resp.length() < 2) return;

    // FA<11 cifre>; — frequenza VFO A
    if (resp.startsWith("FA") && resp.length() == 14) {
        bool ok;
        double freq = resp.mid(2, 11).toDouble(&ok);
        if (ok) {
            if (!m_connected) {
                // Risposta ricevuta: cancella timeout
                if (m_connectTimer) { m_connectTimer->stop(); m_connectTimer->deleteLater(); m_connectTimer = nullptr; }
                // Auto-baud: disabilita e persisti il baud vincente.
                if (m_autoBaudActive) {
                    DIAG_CAT(QStringLiteral("auto-baud: riuscito a %1 baud").arg(m_baudRate));
                    m_autoBaudActive = false;
                    m_autoBaudIndex = -1;
                    saveSettings();
                }
                m_connected = true;
                emit connectedChanged();
                emit statusUpdate("CAT: connesso a " + m_rigName +
                                  " — " + QString::number(freq / 1e6, 'f', 6) + " MHz");
            }
            if (freq != m_frequency) {
                m_frequency = freq;
                emit frequencyChanged();
            }
        }
    }
    // MD<1 char>; — modo
    else if (resp.startsWith("MD") && resp.length() >= 4 && resp.endsWith(';')) {
        QString newMode = parseMode(resp.at(resp.length() - 2));
        if (newMode != m_mode) {
            m_mode = newMode;
            emit modeChanged();
        }
    }
}

// --- parseMode ---

QString DecodiumCatManager::parseMode(char code)
{
    switch (code) {
    case '1': return "LSB";
    case '2': return "USB";
    case '3': return "CW";
    case '4': return "FM";
    case '5': return "AM";
    case '6': return "RTTY-L";
    case '7': return "CW-R";
    case '8': return "DATA-L";
    case '9': return "RTTY-U";
    case 'A': return "DATA-FM";
    case 'B': return "FM-N";
    case 'C': return "DATA-U";
    case 'D': return "AM-N";
    default:  return "---";
    }
}

// --- auto-baud ---

bool DecodiumCatManager::tryNextAutoBaud()
{
    if (!m_autoBaudActive) return false;
    int next = m_autoBaudIndex + 1;
    if (next >= m_autoBaudCandidates.size()) {
        m_autoBaudActive = false;
        m_autoBaudIndex = -1;
        return false;
    }
    m_autoBaudIndex = next;
    int const baud = m_autoBaudCandidates[next];
    DIAG_CAT(QStringLiteral("auto-baud: tentativo %1/%2 a %3 baud")
             .arg(next + 1).arg(m_autoBaudCandidates.size()).arg(baud));
    // Aggiorna temporaneamente il baud ma non chiamare saveSettings (lo salva
    // solo in caso di successo in processResponse).
    m_baudRate = baud;
    emit baudRateChanged();
    // Chiude la porta corrente e riapre con il nuovo baud.
    // connectRig() non re-inizializza la sequenza perche' m_autoBaudIndex >= 0.
    QTimer::singleShot(100, this, [this]{ connectRig(); });
    return true;
}

// --- onConnectTimeout ---

void DecodiumCatManager::onConnectTimeout()
{
    if (m_connected) return;  // già connesso, ignora
    // Prova il prossimo baud della sequenza auto-detect prima di arrendersi.
    if (tryNextAutoBaud()) {
        disconnectRig();
        return;
    }
    emit errorOccurred("CAT: nessuna risposta da " + m_rigName +
                       " su " + m_serialPort + " dopo tentativi a " +
                       QString::number(m_autoBaudCandidates.isEmpty() ? m_baudRate : m_autoBaudCandidates.last()) +
                       " baud. Verifica porta COM, cavo USB e che il rig sia acceso.");
    disconnectRig();
}

// --- onSerialError ---

void DecodiumCatManager::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    QString msg = m_serial ? m_serial->errorString() : "errore sconosciuto";
    emit errorOccurred("CAT errore: " + msg);
    disconnectRig();
}

// --- setRigFrequency ---

void DecodiumCatManager::setRigFrequency(double hz)
{
    if (!m_connected) return;
    QString cmd = QString("FA%1;").arg(static_cast<long long>(std::round(hz)), 11, 10, QChar('0'));
    sendCommand(cmd.toLatin1());
}

// --- setRigMode ---
//
// Invia il comando CAT per impostare il modo operativo del rig.
// Mappa il nome testuale al codice Yaesu/Kenwood del comando "MD<code>;".
// Su Kenwood per DATA-USB serve anche il flag "DA1;" dopo "MD2;".
// No-op se il rig non è connesso o non è Yaesu/Kenwood.

void DecodiumCatManager::setRigMode(const QString& mode)
{
    DIAG_CAT(QStringLiteral("setRigMode(\"%1\")").arg(mode));
    if (!m_connected) {
        DIAG_CAT(QStringLiteral("setRigMode: skip (non connesso)"));
        return;
    }
    if (!(isYaesuRig(m_rigName) || isKenwoodRig(m_rigName))) {
        DIAG_CAT(QStringLiteral("setRigMode: rig '%1' non gestito nativamente, skip").arg(m_rigName));
        return;
    }
    char const code = yaesuModeCode(mode);
    if (!code) {
        DIAG_CAT(QStringLiteral("setRigMode: mode '%1' non mappato, skip").arg(mode));
        return;
    }
    QByteArray cmd;
    cmd.reserve(5);
    cmd.append("MD");
    cmd.append(code);
    cmd.append(';');
    sendCommand(cmd);
    // Kenwood: per DATA-USB serve anche DA1; dopo MD2;
    if (isKenwoodRig(m_rigName) && (code == '2' || code == '1')) {
        QString const normalized = mode.trimmed().toUpper();
        bool const wantData = normalized.contains("DATA") || normalized.contains("PKT")
                              || normalized.contains("DIG");
        sendCommand(wantData ? QByteArrayLiteral("DA1;") : QByteArrayLiteral("DA0;"));
    }
}

// --- setRigPtt ---


void DecodiumCatManager::setRigPtt(bool on)
{
    DIAG_CAT(on ? QStringLiteral("setRigPtt(true)") : QStringLiteral("setRigPtt(false)"));
    if (m_pttMethod == "VOX") {
        DIAG_CAT(on
                 ? QStringLiteral("VOX: nessun PTT CAT/DTR/RTS; la radio deve commutare con audio TX")
                 : QStringLiteral("VOX: nessun rilascio PTT CAT/DTR/RTS richiesto"));
        if (m_pttActive) {
            m_pttActive = false;
            emit pttActiveChanged();
        }
        return;
    }
    // Pre-PTT warning: su Yaesu, se il rig è in voice mode il PTT native
    // userà TX; (percorso MIC) e l'audio digitale da USB codec non
    // raggiunge l'RF. Log diagnostico per aiutare a diagnosticare "nessuno
    // risponde alle TX". Non forziamo il cambio mode automaticamente per
    // evitare effetti sorpresa: l'utente deve selezionare DATA-USB o
    // chiamare setRigMode() esplicitamente.
    if (on && isYaesuRig(m_rigName) && !isLikelyDataMode(m_mode) && !m_mode.isEmpty()) {
        DIAG_CAT(QStringLiteral(
            "ATTENZIONE: rig Yaesu in modo '%1' (non DATA). Il PTT userà "
            "il percorso MIC (TX;) invece di USB codec (TX1;). Per FT8/FT4/FT2 "
            "imposta il rig in DATA-USB (MD0C;) o chiama setRigMode(\"DATA-U\")."
            ).arg(m_mode));
    }
    if (m_pttMethod == "CAT") {
        DIAG_CAT(m_connected ? QStringLiteral("CAT: m_connected=true, invio TX/RX")
                             : QStringLiteral("CAT: m_connected=false, skip"));
        if (!m_connected) return;
        QByteArray const cmd = on ? nativePttOnCommand(m_rigName, m_mode)
                                  : QByteArrayLiteral("RX;");
        DIAG_CAT(QStringLiteral("CAT cmd=[%1] rig=[%2] mode=[%3]")
                     .arg(QString::fromLatin1(cmd))
                     .arg(m_rigName)
                     .arg(m_mode));
        bool written = (m_serial && m_serial->isOpen()) ? (m_serial->write(cmd) > 0) : false;
        if (m_serial)
            m_serial->flush();
        DIAG_CAT(written ? QStringLiteral("TX/RX scritto OK")
                         : QStringLiteral("ERRORE scrittura seriale"));
        emit statusUpdate("CAT PTT: " + QString::fromLatin1(cmd));
    } else if (m_pttMethod == "DTR" || m_pttMethod == "RTS") {
        // DTR/RTS PTT: usa porta separata se configurata, altrimenti la porta CAT
        bool useSeparatePort = (!m_pttPort.isEmpty() && m_pttPort != "CAT" && m_pttPort != m_serialPort);
        QSerialPort* pttSerial = m_serial;

        if (useSeparatePort) {
            // Apri/chiudi porta PTT separata
            if (!m_pttSerial) {
                m_pttSerial = new QSerialPort(this);
                m_pttSerial->setPortName(m_pttPort);
                m_pttSerial->setBaudRate(9600);
                m_pttSerial->setDataBits(QSerialPort::Data8);
                m_pttSerial->setParity(QSerialPort::NoParity);
                m_pttSerial->setStopBits(QSerialPort::OneStop);
                m_pttSerial->setFlowControl(QSerialPort::NoFlowControl);
                if (!m_pttSerial->open(QIODevice::ReadWrite)) {
                    emit errorOccurred("PTT: impossibile aprire " + m_pttPort + " — " + m_pttSerial->errorString());
                    delete m_pttSerial; m_pttSerial = nullptr;
                    return;
                }
                DIAG_CAT("PTT porta separata aperta: " + m_pttPort);
            }
            pttSerial = m_pttSerial;
        }

        if (pttSerial && pttSerial->isOpen()) {
            if (m_pttMethod == "DTR")
                pttSerial->setDataTerminalReady(on);
            else
                pttSerial->setRequestToSend(on);
            DIAG_CAT(on ? QStringLiteral("DTR/RTS PTT ON") : QStringLiteral("DTR/RTS PTT OFF"));
        }

        // Chiudi porta PTT separata quando si spegne PTT
        if (!on && m_pttSerial && useSeparatePort) {
            m_pttSerial->close();
            delete m_pttSerial;
            m_pttSerial = nullptr;
        }
    }
    if (m_pttActive != on) { m_pttActive = on; emit pttActiveChanged(); }
}

void DecodiumCatManager::applyPollInterval()
{
    if (m_pollTimer)
        m_pollTimer->setInterval(m_pollInterval * 1000);
}

// --- refreshPorts ---

void DecodiumCatManager::refreshPorts()
{
    QElapsedTimer timer;
    timer.start();
    QStringList const ports = enumerateSerialPorts(m_serialPort, m_pttPort);
    if (ports != m_portList) {
        m_portList = ports;
        emit portListChanged();
    }
    if (timer.elapsed() > 1000) {
        qWarning("CAT serial port enumeration took %lld ms (%lld ports)",
                 timer.elapsed(), static_cast<long long>(m_portList.size()));
    }
}

// --- settings ---

void DecodiumCatManager::saveSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_Native");
    s.setValue("rigName",        m_rigName);
    s.setValue("serialPort",     m_serialPort);
    s.setValue("baudRate",       m_baudRate);
    s.setValue("dataBits",       m_dataBits);
    s.setValue("stopBits",       m_stopBits);
    s.setValue("handshake",      m_handshake);
    s.setValue("pttMethod",      m_pttMethod);
    s.setValue("pttPort",        m_pttPort);
    s.setValue("civAddress",     m_civAddress);
    s.setValue("pollInterval",   m_pollInterval);
    s.setValue("forceDtr",       forceDtrAvailable() && m_forceDtr);
    s.setValue("dtrHigh",        forceDtrAvailable() && m_dtrHigh);
    s.setValue("forceRts",       forceRtsAvailable() && m_forceRts);
    s.setValue("rtsHigh",        forceRtsAvailable() && m_rtsHigh);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
    s.endGroup();
}

void DecodiumCatManager::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_Native");
    m_rigName        = s.value("rigName",        "Kenwood TS-590S").toString();
    m_serialPort     = s.value("serialPort",     QString{}).toString();
    m_baudRate       = s.value("baudRate",        57600).toInt();
    m_dataBits       = s.value("dataBits",        m_dataBits).toString();
    m_stopBits       = s.value("stopBits",        m_stopBits).toString();
    m_handshake      = s.value("handshake",       m_handshake).toString();
    m_pttMethod      = s.value("pttMethod",       "CAT").toString().trimmed().toUpper();
    if (m_pttMethod.isEmpty())
        m_pttMethod = QStringLiteral("CAT");
    m_pttPort        = s.value("pttPort",         "CAT").toString();
    m_civAddress     = s.value("civAddress",      0).toInt();
    if (auto const* defaults = findRigDefaults(m_rigName)) {
        m_civAddress = defaults->civAddress;
    }
    m_pollInterval   = s.value("pollInterval",    2).toInt();
    m_forceDtr       = s.value("forceDtr",        false).toBool();
    m_dtrHigh        = s.value("dtrHigh",         false).toBool();
    m_forceRts       = s.value("forceRts",        false).toBool();
    m_rtsHigh        = s.value("rtsHigh",         false).toBool();
    m_catAutoConnect = s.value("catAutoConnect",  false).toBool();
    m_audioAutoStart = s.value("audioAutoStart",  false).toBool();
    s.endGroup();
    enforceForceLineAvailability();
}
