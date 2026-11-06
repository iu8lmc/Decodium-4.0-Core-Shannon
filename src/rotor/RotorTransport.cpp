#include "RotorTransport.h"

#include <QMutexLocker>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace decodium::rotor {

// ── seriale reale ──────────────────────────────────────────────────────────

SerialTransport::SerialTransport(const QString& port, int baudrate, double timeoutSeconds)
    : m_portName(port)
    , m_baud(baudrate)
    , m_timeoutMs(static_cast<int>(timeoutSeconds * 1000.0))
{
}

SerialTransport::~SerialTransport()
{
    close();
}

void SerialTransport::open()
{
    m_port = std::make_unique<QSerialPort>();
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baud);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        const QString why = m_port->errorString();
        m_port.reset();
        throw TransportError(QStringLiteral("cannot open %1: %2").arg(m_portName, why));
    }
    m_port->clear(QSerialPort::Input);
}

void SerialTransport::close()
{
    if (m_port) {
        if (m_port->isOpen())
            m_port->close();
        m_port.reset();
    }
}

bool SerialTransport::isOpen() const
{
    return m_port && m_port->isOpen();
}

void SerialTransport::write(const QByteArray& frame)
{
    if (!m_port)
        throw TransportError(QStringLiteral("port closed"));
    if (m_port->write(frame) != frame.size() || !m_port->waitForBytesWritten(m_timeoutMs))
        throw TransportError(QStringLiteral("write failed on %1: %2").arg(m_portName, m_port->errorString()));
}

QByteArray SerialTransport::readFrame()
{
    if (!m_port)
        throw TransportError(QStringLiteral("port closed"));
    QElapsedTimer clock;
    clock.start();
    QByteArray frame;
    bool started = false;
    while (clock.elapsed() < m_timeoutMs) {
        if (m_port->bytesAvailable() == 0) {
            const int remaining = m_timeoutMs - static_cast<int>(clock.elapsed());
            if (remaining <= 0 || !m_port->waitForReadyRead(remaining)) {
                if (m_port->error() != QSerialPort::NoError && m_port->error() != QSerialPort::TimeoutError)
                    throw TransportError(QStringLiteral("read failed on %1: %2")
                                             .arg(m_portName, m_port->errorString()));
                return {};
            }
        }
        const QByteArray chunk = m_port->readAll();
        for (char c : chunk) {
            if (!started) {
                if (c != kStx)
                    continue;      // rumore o coda di un frame precedente
                started = true;
            }
            frame.append(c);
            if (c == kCr)
                return frame;
        }
    }
    return {};
}

// ── control box simulato ───────────────────────────────────────────────────

SimulatedTransport::SimulatedTransport(const QString& modelKey, double az, double el)
{
    const Model* model = modelByKey(modelKey);
    m_model = model ? *model : *modelByKey(QStringLiteral("d_azel"));
    if (m_model.hasAz())
        m_position.insert(m_model.azId, az);
    if (m_model.hasEl())
        m_position.insert(m_model.elId, el);
    m_target = m_position;
    m_clock.start();
}

void SimulatedTransport::advance()
{
    const qint64 now = m_clock.elapsed();
    const double step = (now - m_lastMs) / 1000.0 * kSpeedDegPerSecond;
    m_lastMs = now;
    for (auto it = m_target.begin(); it != m_target.end(); ++it) {
        const double delta = it.value() - m_position[it.key()];
        m_position[it.key()] += std::max(-step, std::min(step, delta));
    }
}

void SimulatedTransport::write(const QByteArray& frame)
{
    QMutexLocker locker(&m_lock);
    advance();
    QByteArray text = frame;
    while (!text.isEmpty() && (text.front() == kStx || text.front() == kCr))
        text.remove(0, 1);
    while (!text.isEmpty() && (text.back() == kStx || text.back() == kCr))
        text.chop(1);
    if (text.size() < 2)
        return;
    const char axis = text.at(0);
    const char verb = text.at(1);
    const QByteArray argument = text.mid(2);
    if (!m_position.contains(axis))
        return;     // asse inesistente su questo control box: nessuna risposta
    const int mult = m_model.multiplier;
    if (verb == '?') {
        const long value = std::lround(std::nearbyint(m_position[axis] * mult));
        const bool moving = std::abs(m_target[axis] - m_position[axis]) > 0.5;
        m_pending = QByteArray(1, kStx)
                    + QStringLiteral("%1,?,%2,%3").arg(QChar(axis)).arg(value).arg(moving ? 'M' : 'R').toLatin1()
                    + QByteArray(1, kCr);
    } else if (verb == 'G' && !argument.isEmpty()) {
        bool ok = false;
        const int raw = argument.toInt(&ok);
        if (!ok)
            return;
        const bool stop = (mult == 10) ? (raw == 9777 || raw == 9999)
                                       : (raw == kStopSoft || raw == kStopFast);
        m_target[axis] = stop ? m_position[axis] : static_cast<double>(raw) / mult;
    }
}

QByteArray SimulatedTransport::readFrame()
{
    QMutexLocker locker(&m_lock);
    QByteArray frame;
    frame.swap(m_pending);
    return frame;
}

// ── transazioni ────────────────────────────────────────────────────────────

Link::Link(std::unique_ptr<Transport> transport, int retries, FrameHook onFrame)
    : m_transport(std::move(transport))
    , m_retries(std::max(1, retries))
    , m_onFrame(std::move(onFrame))
{
}

void Link::notify(const QString& direction, const QByteArray& frame)
{
    if (m_onFrame)
        m_onFrame(direction, frame);
}

void Link::send(const QByteArray& frame)
{
    m_transport->write(frame);
    notify(QStringLiteral("tx"), frame);
}

bool Link::transact(const QByteArray& frame, int multiplier, Reply* reply)
{
    for (int attempt = 0; attempt < m_retries; ++attempt) {
        m_transport->write(frame);
        const QByteArray raw = m_transport->readFrame();
        notify(QStringLiteral("tx"), frame);
        if (!raw.isEmpty()) {
            notify(QStringLiteral("rx"), raw);
            Reply decoded;
            if (decodeReply(raw, multiplier, &decoded)) {
                if (reply)
                    *reply = decoded;
                return true;
            }
        }
        QThread::msleep(50);
    }
    return false;
}

}  // namespace decodium::rotor
