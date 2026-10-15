// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoPortSerialLine.h"

#include <QSerialPort>
#include <QTcpSocket>

DecoPortSerialLine::DecoPortSerialLine(QObject* parent)
    : QObject(parent)
{
}

DecoPortSerialLine::~DecoPortSerialLine()
{
    close();
}

bool DecoPortSerialLine::isNetworkSpec(const QString& spec)
{
    return spec.startsWith(QLatin1String("tcp:"), Qt::CaseInsensitive);
}

bool DecoPortSerialLine::open(const QString& spec, const DecoPortLineSettings& s,
                              int connectTimeoutMs, const QString& owner)
{
    close();
    m_spec = spec;
    m_error.clear();

    if (isNetworkSpec(spec)) {
        const QString rest = spec.mid(4);
        const int colon = rest.lastIndexOf(QLatin1Char(':'));
        bool ok = false;
        const uint port = colon > 0 ? rest.mid(colon + 1).toUInt(&ok) : 0;
        if (!ok || port == 0 || port > 65535) {
            m_error = tr("network line must be tcp:host:port, not %1").arg(spec);
            return false;
        }
        m_tcp = new QTcpSocket(this);
        m_tcp->connectToHost(rest.left(colon), static_cast<quint16>(port));
        if (!m_tcp->waitForConnected(connectTimeoutMs)) {
            m_error = m_tcp->errorString();
            delete m_tcp;
            m_tcp = nullptr;
            return false;
        }
        // Un byte alla volta, subito: il CAT e' fatto di messaggi corti.
        m_tcp->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connect(m_tcp, &QTcpSocket::readyRead, this, &DecoPortSerialLine::readyRead);
        connect(m_tcp, &QTcpSocket::disconnected, this, [this] {
            emit lost(tr("connection closed by %1").arg(m_spec));
        });
        return true;
    }

    QString conflictPort;
    QString conflictOwner;
    const QString leaseOwner = owner.trimmed().isEmpty()
        ? QStringLiteral("DecoPort serial line") : owner.trimmed();
    m_serialLease = decodium::ports::SerialPortRegistry::acquire(
        {spec}, leaseOwner, &conflictPort, &conflictOwner);
    if (!m_serialLease.isValid()) {
        m_error = tr("%1 is already reserved inside Decodium by %2")
                      .arg(spec, conflictOwner.isEmpty() ? tr("another component") : conflictOwner);
        return false;
    }

    m_serial = new QSerialPort(this);
#if defined(Q_OS_WIN)
    // Qt mette il prefisso dei dispositivi (\\.\) solo ai nomi che cominciano
    // con COM: "CNCB0", l'estremo nascosto di com0com, diventerebbe un file
    // della cartella corrente e non si aprirebbe mai.
    if (!spec.startsWith(QLatin1String("COM"), Qt::CaseInsensitive)
        && !spec.startsWith(QLatin1String("\\\\.\\")))
        m_serial->setPortName(QStringLiteral("\\\\.\\") + spec);
    else
        m_serial->setPortName(spec);
#else
    m_serial->setPortName(spec);
#endif
    if (!m_serial->open(QIODevice::ReadWrite)) {
        m_error = m_serial->errorString();
        delete m_serial;
        m_serial = nullptr;
        m_serialLease.release();
        return false;
    }
    m_serial->setBaudRate(s.baud > 0 ? s.baud : 9600);
    m_serial->setDataBits(s.dataBits == 7 ? QSerialPort::Data7
                          : s.dataBits == 6 ? QSerialPort::Data6
                          : s.dataBits == 5 ? QSerialPort::Data5 : QSerialPort::Data8);
    m_serial->setStopBits(s.stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
    const QChar p = s.parity.toUpper();
    m_serial->setParity(p == QLatin1Char('E') ? QSerialPort::EvenParity
                        : p == QLatin1Char('O') ? QSerialPort::OddParity
                        : p == QLatin1Char('M') ? QSerialPort::MarkParity
                        : p == QLatin1Char('S') ? QSerialPort::SpaceParity
                        : QSerialPort::NoParity);
    m_serial->setFlowControl(s.handshake == QLatin1String("hardware") ? QSerialPort::HardwareControl
                             : s.handshake == QLatin1String("xonxoff") ? QSerialPort::SoftwareControl
                             : QSerialPort::NoFlowControl);
    // Con l'handshake hardware RTS lo guida il driver; altrimenti si tocca
    // solo se qualcuno l'ha chiesto esplicitamente.
    if (s.rts >= 0 && s.handshake != QLatin1String("hardware"))
        m_serial->setRequestToSend(s.rts == 1);
    if (s.dtr >= 0)
        m_serial->setDataTerminalReady(s.dtr == 1);

    connect(m_serial, &QSerialPort::readyRead, this, &DecoPortSerialLine::readyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        // ResourceError e' la porta che sparisce: il cavo staccato, la radio
        // spenta con l'USB alimentata da lei.
        if (e == QSerialPort::ResourceError || e == QSerialPort::PermissionError)
            emit lost(m_serial ? m_serial->errorString() : tr("port lost"));
    });
    return true;
}

void DecoPortSerialLine::close()
{
    if (m_serial) {
        m_serial->disconnect(this);
        m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
        m_serialLease.release();
    }
    if (m_tcp) {
        m_tcp->disconnect(this);
        m_tcp->abort();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
}

bool DecoPortSerialLine::isOpen() const
{
    if (m_serial)
        return m_serial->isOpen();
    if (m_tcp)
        return m_tcp->state() == QAbstractSocket::ConnectedState;
    return false;
}

QIODevice* DecoPortSerialLine::device() const
{
    if (m_serial)
        return m_serial;
    return m_tcp;
}

int DecoPortSerialLine::dsr() const
{
    if (!m_serial || !m_serial->isOpen())
        return -1;
    return (m_serial->pinoutSignals() & QSerialPort::DataSetReadySignal) ? 1 : 0;
}

int DecoPortSerialLine::dcd() const
{
    if (!m_serial || !m_serial->isOpen())
        return -1;
    return (m_serial->pinoutSignals() & QSerialPort::DataCarrierDetectSignal) ? 1 : 0;
}
