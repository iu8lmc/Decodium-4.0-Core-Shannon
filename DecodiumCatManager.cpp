#include "DecodiumCatManager.h"
#include "DecodiumLogging.hpp"

#include <QSerialPortInfo>
#include <QSettings>

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

bool supportsYaesuDataPtt(QString const& rigName)
{
    QString const normalized = normalizedRigName(rigName);
    return normalized.contains(QStringLiteral("FT991"))
        || normalized.contains(QStringLiteral("FTDX10"))
        || normalized.contains(QStringLiteral("FTDX101"));
}

bool isLikelyDataMode(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized.contains(QStringLiteral("DATA"))
        || normalized.contains(QStringLiteral("DIG"))
        || normalized.contains(QStringLiteral("PKT"))
        || normalized.contains(QStringLiteral("RTTY"));
}

bool isCatPttMethod(QString const& value)
{
    return value.trimmed().compare(QStringLiteral("CAT"), Qt::CaseInsensitive) == 0;
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
        "Icom IC-7300", "Icom IC-7600", "Icom IC-7610", "Icom IC-9700",
        "Yaesu FT-991", "Yaesu FT-991A", "Yaesu FT-DX10", "Yaesu FT-DX101D",
        "Elecraft K3", "Elecraft KX3",
    };
}

// --- ctor/dtor ---

DecodiumCatManager::DecodiumCatManager(QObject* parent)
    : QObject(parent)
{
    refreshPorts();
    loadSettings();
}

DecodiumCatManager::~DecodiumCatManager()
{
    disconnectRig();
}

void DecodiumCatManager::enforceCatSerialDefaults()
{
    if (!isCatPttMethod(m_pttMethod))
        return;

    if (m_forceDtr) {
        m_forceDtr = false;
        emit forceDtrChanged();
    }
    if (m_dtrHigh) {
        m_dtrHigh = false;
        emit dtrHighChanged();
    }
    if (m_forceRts) {
        m_forceRts = false;
        emit forceRtsChanged();
    }
    if (m_rtsHigh) {
        m_rtsHigh = false;
        emit rtsHighChanged();
    }
}

void DecodiumCatManager::setForceDtr(bool v)
{
    bool const effective = isCatPttMethod(m_pttMethod) ? false : v;
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
    bool const effective = (isCatPttMethod(m_pttMethod) || !m_forceDtr) ? false : v;
    if (m_dtrHigh != effective) {
        m_dtrHigh = effective;
        emit dtrHighChanged();
    }
}

void DecodiumCatManager::setForceRts(bool v)
{
    bool const effective = isCatPttMethod(m_pttMethod) ? false : v;
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
    bool const effective = (isCatPttMethod(m_pttMethod) || !m_forceRts) ? false : v;
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
    enforceCatSerialDefaults();
}

// --- connectRig ---

void DecodiumCatManager::connectRig()
{
    if (m_serial)
        disconnectRig();

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
    if (m_forceDtr)
        serial->setDataTerminalReady(m_dtrHigh);
    if (m_forceRts)
        serial->setRequestToSend(m_rtsHigh);

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
    if (m_connectTimer) { m_connectTimer->stop(); m_connectTimer->deleteLater(); m_connectTimer = nullptr; }
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
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

// --- onConnectTimeout ---

void DecodiumCatManager::onConnectTimeout()
{
    if (m_connected) return;  // già connesso, ignora
    emit errorOccurred("CAT: nessuna risposta da " + m_rigName +
                       " su " + m_serialPort + " a " + QString::number(m_baudRate) +
                       " baud. Verifica porta COM e baud rate nel menu radio.");
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
    QString cmd = QString("FA%1;").arg(static_cast<long long>(hz), 11, 10, QChar('0'));
    sendCommand(cmd.toLatin1());
}

// --- setRigPtt ---

// Nota: in passato qui esisteva una catLog() che apriva "C:\Users\IU8LMC\cat_log.txt"
// — path hard-coded della macchina dev, sempre NULL sulle installazioni utente.
// Sostituita con DIAG_CAT() che instrada al logger diagnostico unificato.

void DecodiumCatManager::setRigPtt(bool on)
{
    DIAG_CAT(on ? QStringLiteral("setRigPtt(true)") : QStringLiteral("setRigPtt(false)"));
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
    m_portList.clear();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
        m_portList.append(info.portName());
    emit portListChanged();
}

// --- settings ---

void DecodiumCatManager::saveSettings()
{
    bool const catPtt = isCatPttMethod(m_pttMethod);
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_Native");
    s.setValue("rigName",        m_rigName);
    s.setValue("serialPort",     m_serialPort);
    s.setValue("baudRate",       m_baudRate);
    s.setValue("pttMethod",      catPtt ? QStringLiteral("CAT") : m_pttMethod);
    s.setValue("pttPort",        m_pttPort);
    s.setValue("pollInterval",   m_pollInterval);
    s.setValue("forceDtr",       catPtt ? false : m_forceDtr);
    s.setValue("dtrHigh",        catPtt ? false : m_dtrHigh);
    s.setValue("forceRts",       catPtt ? false : m_forceRts);
    s.setValue("rtsHigh",        catPtt ? false : m_rtsHigh);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
    s.endGroup();
}

void DecodiumCatManager::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_Native");
    m_rigName        = s.value("rigName",        "Kenwood TS-590S").toString();
    m_serialPort     = s.value("serialPort",     "COM3").toString();
    m_baudRate       = s.value("baudRate",        57600).toInt();
    m_pttMethod      = s.value("pttMethod",       "CAT").toString().trimmed().toUpper();
    if (m_pttMethod.isEmpty())
        m_pttMethod = QStringLiteral("CAT");
    m_pttPort        = s.value("pttPort",         "CAT").toString();
    m_pollInterval   = s.value("pollInterval",    2).toInt();
    m_forceDtr       = s.value("forceDtr",        false).toBool();
    m_dtrHigh        = s.value("dtrHigh",         false).toBool();
    m_forceRts       = s.value("forceRts",        false).toBool();
    m_rtsHigh        = s.value("rtsHigh",         false).toBool();
    m_catAutoConnect = s.value("catAutoConnect",  false).toBool();
    m_audioAutoStart = s.value("audioAutoStart",  false).toBool();
    s.endGroup();
    enforceCatSerialDefaults();
}
