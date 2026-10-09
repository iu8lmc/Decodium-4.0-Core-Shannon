#include "WinKeyer.h"

#include <QCoreApplication>
#include <QSerialPort>

namespace decodium::cw {

namespace winkeyer {

QByteArray textBytes(const QString& text)
{
    QByteArray out;
    for (const QChar c : text.toUpper()) {
        const char ch = c.toLatin1();
        // Lettere, cifre, spazio e la punteggiatura che il WinKeyer conosce.
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' '
            || QByteArrayView("/?.,=+-@:'()\"&!;$").contains(ch))
            out += ch;
    }
    return out;
}

QByteArray hostOpen() { return QByteArray("\x00\x02", 2); }
QByteArray hostClose() { return QByteArray("\x00\x03", 2); }
QByteArray speed(int wpm) { return QByteArray(1, '\x02') + char(qBound(5, wpm, 99)); }
QByteArray clearBuffer() { return QByteArray(1, '\x0A'); }

} // namespace winkeyer

WinKeyer::WinKeyer(QObject* parent)
    : QObject(parent)
{
}

WinKeyer::~WinKeyer()
{
    close();
}

bool WinKeyer::isOpen() const
{
    return m_port && m_port->isOpen();
}

bool WinKeyer::open(const QString& portName)
{
    close();
    m_port = new QSerialPort(this);
    m_port->setPortName(portName);
    m_port->setBaudRate(1200);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::TwoStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        emit failed(QCoreApplication::translate("WinKeyer", "WinKeyer: cannot open %1: %2").arg(portName, m_port->errorString()));
        delete m_port;
        m_port = nullptr;
        return false;
    }
    // Il WinKeyer si alimenta dal DTR su molti modelli USB.
    m_port->setDataTerminalReady(true);
    m_port->setRequestToSend(false);
    connect(m_port, &QSerialPort::readyRead, this, &WinKeyer::onReadyRead);
    m_version = 0;
    m_port->write(winkeyer::hostOpen());
    if (m_wpm > 0)
        m_port->write(winkeyer::speed(m_wpm));
    return true;
}

void WinKeyer::close()
{
    if (m_port) {
        if (m_port->isOpen()) {
            m_port->write(winkeyer::hostClose());
            m_port->waitForBytesWritten(200);
            m_port->close();
        }
        m_port->deleteLater();
        m_port = nullptr;
    }
}

void WinKeyer::setSpeed(int wpm)
{
    if (wpm <= 0)
        return;
    m_wpm = wpm;
    if (isOpen())
        m_port->write(winkeyer::speed(wpm));
}

void WinKeyer::send(const QString& text, int wpm)
{
    if (!isOpen())
        return;
    if (wpm > 0 && wpm != m_wpm)
        setSpeed(wpm);
    QByteArray bytes = winkeyer::textBytes(text);
    if (bytes.isEmpty())
        return;
    // Uno spazio in fondo separa dal messaggio che viene dopo.
    if (!bytes.endsWith(' '))
        bytes += ' ';
    m_port->write(bytes);
}

void WinKeyer::stop()
{
    if (isOpen()) {
        // Elimina prima i caratteri che Qt non ha ancora passato al dispositivo;
        // poi 0x0A arriva subito al WinKeyer e svuota anche il suo buffer.
        m_port->clear(QSerialPort::Output);
        m_port->write(winkeyer::clearBuffer());
        // 0x0A deve precedere subito i caratteri gia' accodati: flush evita
        // che il pulsante Ferma aspetti il prossimo giro dell'event loop.
        m_port->flush();
        m_port->waitForBytesWritten(100);
    }
}

void WinKeyer::onReadyRead()
{
    const QByteArray data = m_port->readAll();
    for (const char c : data) {
        const unsigned char b = static_cast<unsigned char>(c);
        if (m_version == 0 && b < 0x40 && b > 0) {
            // La prima risposta dopo "host open" e' la versione del firmware.
            m_version = b;
            emit versionReceived(b);
        } else if ((b & 0xC0) == 0xC0) {
            // Stato: il bit 2 dice che sta ancora trasmettendo.
            const bool busy = (b & 0x04) != 0;
            if (busy != m_busy) {
                m_busy = busy;
                emit busyChanged(busy);
            }
        }
    }
}

} // namespace decodium::cw
