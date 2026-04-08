#include "DecodiumCatManager.h"

#include <QSerialPortInfo>
#include <QSettings>

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

// --- connectRig ---

void DecodiumCatManager::connectRig()
{
    if (m_serial && m_serial->isOpen())
        disconnectRig();

    m_serial = new QSerialPort(this);
    m_serial->setPortName(m_serialPort);
    m_serial->setBaudRate(m_baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_serial, &QSerialPort::readyRead,
            this, &DecodiumCatManager::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &DecodiumCatManager::onSerialError);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        QString err = m_serial->errorString();
        emit errorOccurred("CAT: impossibile aprire " + m_serialPort + " — " + err);
        m_serial->deleteLater();
        m_serial = nullptr;
        return;
    }
    m_serial->setDataTerminalReady(true);
    m_serial->setRequestToSend(true);

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
    else if (resp.startsWith("MD") && resp.length() == 4) {
        QString newMode = parseMode(resp.at(2));
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
    case '6': return "FSK";
    case '7': return "CW-R";
    case '9': return "FSK-R";
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

static void catLog(const char* msg) {
    FILE* f = fopen("C:\\Users\\IU8LMC\\cat_log.txt", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

void DecodiumCatManager::setRigPtt(bool on)
{
    catLog(on ? "setRigPtt(true)" : "setRigPtt(false)");
    if (m_pttMethod == "CAT") {
        catLog(m_connected ? "CAT: m_connected=true, invio TX/RX" : "CAT: m_connected=false, skip");
        if (!m_connected) return;
        QByteArray cmd = on ? "TX1;" : "RX;";  // TX1 = data input (USB audio), non MIC
        bool written = (m_serial && m_serial->isOpen()) ? (m_serial->write(cmd) > 0) : false;
        m_serial->flush();
        catLog(written ? "TX/RX scritto OK" : "ERRORE scrittura seriale");
        emit statusUpdate("CAT PTT: " + QString(cmd));
    } else if (m_pttMethod == "DTR" && m_serial && m_serial->isOpen()) {
        // DTR/RTS funzionano appena la porta è aperta (no handshake necessario)
        m_serial->setDataTerminalReady(on);
    } else if (m_pttMethod == "RTS" && m_serial && m_serial->isOpen()) {
        m_serial->setRequestToSend(on);
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
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_Native");
    s.setValue("rigName",        m_rigName);
    s.setValue("serialPort",     m_serialPort);
    s.setValue("baudRate",       m_baudRate);
    s.setValue("pttMethod",      m_pttMethod);
    s.setValue("pttPort",        m_pttPort);
    s.setValue("pollInterval",   m_pollInterval);
    s.setValue("forceDtr",       m_forceDtr);
    s.setValue("dtrHigh",        m_dtrHigh);
    s.setValue("forceRts",       m_forceRts);
    s.setValue("rtsHigh",        m_rtsHigh);
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
    m_pttMethod      = s.value("pttMethod",       "CAT").toString();
    m_pttPort        = s.value("pttPort",         "CAT").toString();
    m_pollInterval   = s.value("pollInterval",    2).toInt();
    m_forceDtr       = s.value("forceDtr",        false).toBool();
    m_dtrHigh        = s.value("dtrHigh",         false).toBool();
    m_forceRts       = s.value("forceRts",        false).toBool();
    m_rtsHigh        = s.value("rtsHigh",         false).toBool();
    m_catAutoConnect = s.value("catAutoConnect",  false).toBool();
    m_audioAutoStart = s.value("audioAutoStart",  false).toBool();
    s.endGroup();
}
