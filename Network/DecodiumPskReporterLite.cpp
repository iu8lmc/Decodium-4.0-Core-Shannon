#include "DecodiumPskReporterLite.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QIODevice>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

namespace {
constexpr auto kHost = "report.pskreporter.info";
constexpr quint16 kTemplateSender = 0x50e3;
constexpr quint16 kTemplateReceiver = 0x50e2;
constexpr quint32 kPskReporterEnterprise = 30351u;

bool pskReporterDebugEnabled()
{
    static const bool enabled = []() {
        QByteArray value = qgetenv("DECODIUM_PSK_DEBUG").trimmed().toLower();
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }();
    return enabled;
}

QString cacheKey(const QString& call, quint64 freqHz, const QString& mode)
{
    return call + QLatin1Char('|')
        + mode + QLatin1Char('|')
        + QString::number(freqHz / 1000u);
}
}

DecodiumPskReporterLite::DecodiumPskReporterLite(QObject* parent)
    : QObject(parent)
    , m_flushTimer(new QTimer(this))
    , m_descriptorTimer(new QTimer(this))
    , m_sendTimer(new QTimer(this))
    , m_observationId(QRandomGenerator::global()->generate())
{
    m_flushTimer->setInterval(FLUSH_INTERVAL_MS);
    m_descriptorTimer->setInterval(DESCRIPTOR_INTERVAL_MS);
    m_sendTimer->setSingleShot(true);

    connect(m_flushTimer, &QTimer::timeout,
            this, &DecodiumPskReporterLite::onFlushTimer);
    connect(m_descriptorTimer, &QTimer::timeout,
            this, &DecodiumPskReporterLite::onDescriptorTimer);
    connect(m_sendTimer, &QTimer::timeout,
            this, [this]() { sendReport(false); });
}

DecodiumPskReporterLite::~DecodiumPskReporterLite()
{
    sendReport(true);
    stopSocket();
}

void DecodiumPskReporterLite::setEnabled(bool v)
{
    if (m_enabled == v)
        return;

    if (!v) {
        sendReport(true);
        m_enabled = false;
        m_hasSentSpotPacket = false;
        m_flushTimer->stop();
        m_descriptorTimer->stop();
        m_sendTimer->stop();
        stopSocket();
        emit connectedChanged();
        if (pskReporterDebugEnabled())
            qInfo().noquote() << "[PSKDBG] PSK Reporter disabled";
        return;
    }

    m_enabled = true;
    m_autoDisabled = false;
    m_hasSentSpotPacket = false;
    m_consecutiveFailures = 0;
    markDescriptorsDirty();
    connectSocketIfNeeded();
    m_flushTimer->start();
    m_descriptorTimer->start();
    emit connectedChanged();
    if (pskReporterDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter enabled protocol="
                          << (m_useTcpIp ? QStringLiteral("TCP") : QStringLiteral("UDP"))
                          << "host=" << QString::fromLatin1(kHost)
                          << "port=" << SERVICE_PORT;
    }
}

void DecodiumPskReporterLite::setUseTcpIp(bool v)
{
    if (m_useTcpIp == v)
        return;

    if (m_enabled)
        sendReport(true);

    m_useTcpIp = v;
    m_hasSentSpotPacket = false;
    resetSocket();
    markDescriptorsDirty();

    if (m_enabled)
        connectSocketIfNeeded();
    if (!m_spots.isEmpty())
        scheduleSend(m_hasSentSpotPacket ? SEND_DELAY_MS : FIRST_SEND_DELAY_MS);

    emit connectedChanged();
    if (pskReporterDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter transport changed protocol="
                          << (m_useTcpIp ? QStringLiteral("TCP") : QStringLiteral("UDP"));
    }
}

bool DecodiumPskReporterLite::isConnected() const
{
    return m_enabled
        && !m_autoDisabled
        && m_socket
        && m_socket->state() == QAbstractSocket::ConnectedState;
}

void DecodiumPskReporterLite::setLocalStation(const QString& callsign,
                                              const QString& grid,
                                              const QString& programInfo,
                                              const QString& antennaInfo,
                                              const QString& rigInfo)
{
    QString const call = callsign.trimmed().toUpper();
    QString const loc = grid.trimmed().toUpper();
    QString const program = programInfo.trimmed().isEmpty()
        ? QStringLiteral("Decodium4")
        : programInfo.trimmed();
    QString const antenna = antennaInfo.trimmed();
    QString const rig = rigInfo.trimmed().isEmpty()
        ? QStringLiteral("No CAT control")
        : rigInfo.trimmed();

    if (m_myCall == call
        && m_myGrid == loc
        && m_programInfo == program
        && m_antennaInfo == antenna
        && m_rigInfo == rig) {
        return;
    }

    m_myCall = call;
    m_myGrid = loc;
    m_programInfo = program;
    m_antennaInfo = antenna;
    m_rigInfo = rig;
    markDescriptorsDirty();
    connectSocketIfNeeded();
    if (!m_spots.isEmpty())
        scheduleSend(m_hasSentSpotPacket ? SEND_DELAY_MS : FIRST_SEND_DELAY_MS);

    if (pskReporterDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter local station call="
                          << (m_myCall.isEmpty() ? QStringLiteral("<empty>") : m_myCall)
                          << "grid="
                          << (m_myGrid.isEmpty() ? QStringLiteral("<empty>") : m_myGrid)
                          << "program=" << m_programInfo
                          << "antenna=" << (m_antennaInfo.isEmpty() ? QStringLiteral("<empty>") : m_antennaInfo)
                          << "rig=" << m_rigInfo;
    }
}

void DecodiumPskReporterLite::addSpot(const QString& call,
                                      const QString& grid,
                                      quint64 freqHz,
                                      const QString& mode,
                                      int snr)
{
    if (!m_enabled || m_autoDisabled)
        return;

    Spot spot;
    spot.call = call.trimmed().toUpper();
    spot.grid = grid.trimmed().toUpper();
    spot.mode = mode.trimmed().toUpper();
    spot.freqHz = freqHz;
    spot.snr = qBound(-128, snr, 127);
    spot.time = QDateTime::currentDateTimeUtc();

    if (spot.call.isEmpty() || spot.call == m_myCall || spot.freqHz == 0 || spot.mode.isEmpty())
        return;

    qint64 const nowSecs = spot.time.toSecsSinceEpoch();
    pruneSpotCache(nowSecs);
    QString const key = cacheKey(spot.call, spot.freqHz, spot.mode);
    if (m_spotCache.contains(key) && nowSecs - m_spotCache.value(key) < CACHE_TIMEOUT_SEC)
        return;

    m_spotCache.insert(key, nowSecs);
    m_spots.append(spot);
    connectSocketIfNeeded();
    int const batchDelayMs = m_spots.size() >= FULL_PACKET_SPOT_THRESHOLD
        ? 0
        : (m_hasSentSpotPacket ? SEND_DELAY_MS : FIRST_SEND_DELAY_MS);
    scheduleSend(batchDelayMs);

    if (pskReporterDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter queued spot call=" << spot.call
                          << "grid=" << (spot.grid.isEmpty() ? QStringLiteral("<empty>") : spot.grid)
                          << "freqHz=" << spot.freqHz
                          << "mode=" << spot.mode
                          << "snr=" << spot.snr
                          << "queued=" << m_spots.size()
                          << "batchDelayMs=" << batchDelayMs;
    }
}

void DecodiumPskReporterLite::sendReport(bool last)
{
    if (last) {
        m_flushTimer->stop();
        m_sendTimer->stop();
    }

    if (!m_enabled || m_autoDisabled)
        return;
    if (m_myCall.isEmpty()) {
        emit errorOccurred(QStringLiteral("PSK Reporter: local callsign not set."));
        return;
    }
    if (m_spots.isEmpty() && !last && !receiverInfoPending())
        return;

    connectSocketIfNeeded();
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        if (pskReporterDebugEnabled()) {
            qInfo().noquote() << "[PSKDBG] PSK Reporter send deferred protocol="
                              << (m_useTcpIp ? QStringLiteral("TCP") : QStringLiteral("UDP"))
                              << "state=" << (m_socket ? int(m_socket->state()) : -1)
                              << "queued=" << m_spots.size()
                              << "receiverPending=" << (receiverInfoPending() ? 1 : 0);
        }
        return;
    }

    bool receiverOnly = m_spots.isEmpty() && receiverInfoPending();
    while (!m_spots.isEmpty() || last || receiverOnly) {
        QList<Spot> sentSpots;
        QByteArray const payload = buildPacket(sentSpots, last || receiverOnly);
        last = false;
        receiverOnly = false;

        if (payload.isEmpty())
            break;

        qint64 const written = m_socket->write(payload);
        if (written < 0) {
            for (auto it = sentSpots.crbegin(); it != sentSpots.crend(); ++it)
                m_spots.prepend(*it);
            ++m_consecutiveFailures;
            QString const msg = QStringLiteral("PSK Reporter write failed: %1").arg(m_socket->errorString());
            emit errorOccurred(msg);
            qWarning().noquote() << "[PSKDBG]" << msg;
            break;
        }

        m_socket->flush();
        m_consecutiveFailures = 0;
        if (!sentSpots.isEmpty())
            m_hasSentSpotPacket = true;
        emit spotsUploaded(sentSpots.size());
        emit connectedChanged();
        if (pskReporterDebugEnabled()) {
            qInfo().noquote() << "[PSKDBG] PSK Reporter packet sent protocol="
                              << (m_useTcpIp ? QStringLiteral("TCP") : QStringLiteral("UDP"))
                              << "spots=" << sentSpots.size()
                              << "bytes=" << written
                              << "remaining=" << m_spots.size();
        }

        if (sentSpots.isEmpty())
            break;
    }

}

void DecodiumPskReporterLite::connectSocketIfNeeded()
{
    if (!m_enabled || m_autoDisabled)
        return;

    QAbstractSocket::SocketType const wantedType =
        m_useTcpIp ? QAbstractSocket::TcpSocket : QAbstractSocket::UdpSocket;

    if (m_socket && m_socket->socketType() == wantedType) {
        if (m_socket->state() == QAbstractSocket::ConnectedState
            || m_socket->state() == QAbstractSocket::ConnectingState) {
            return;
        }
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->disconnectFromHost();
            return;
        }
    } else {
        resetSocket();
        m_socket = m_useTcpIp
            ? static_cast<QAbstractSocket*>(new QTcpSocket(this))
            : static_cast<QAbstractSocket*>(new QUdpSocket(this));
        connect(m_socket, &QAbstractSocket::connected,
                this, &DecodiumPskReporterLite::onSocketConnected);
        connect(m_socket, &QAbstractSocket::errorOccurred,
                this, &DecodiumPskReporterLite::onSocketError);
    }

    m_socket->connectToHost(QString::fromLatin1(kHost), SERVICE_PORT, QIODevice::WriteOnly);
    if (pskReporterDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter connecting protocol="
                          << (m_useTcpIp ? QStringLiteral("TCP") : QStringLiteral("UDP"))
                          << "host=" << QString::fromLatin1(kHost)
                          << "port=" << SERVICE_PORT;
    }
}

void DecodiumPskReporterLite::resetSocket()
{
    if (!m_socket)
        return;

    m_socket->disconnect(this);
    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
}

void DecodiumPskReporterLite::stopSocket()
{
    if (!m_socket)
        return;

    m_socket->disconnectFromHost();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->waitForDisconnected(250);
    resetSocket();
}

void DecodiumPskReporterLite::markDescriptorsDirty()
{
    m_sendDescriptors = m_useTcpIp ? 1 : 3;
    m_sendReceiverData = m_useTcpIp ? 1 : 3;
}

bool DecodiumPskReporterLite::receiverInfoPending() const
{
    return m_sendDescriptors > 0 || m_sendReceiverData > 0;
}

void DecodiumPskReporterLite::scheduleSend(int delayMs)
{
    if (!m_enabled || m_autoDisabled)
        return;

    int const clampedDelay = qMax(0, delayMs);
    if (m_sendTimer->isActive()) {
        int const remaining = m_sendTimer->remainingTime();
        if (remaining >= 0 && remaining <= clampedDelay)
            return;
    }
    m_sendTimer->start(clampedDelay);
}

void DecodiumPskReporterLite::pruneSpotCache(qint64 nowSecs)
{
    for (auto it = m_spotCache.begin(); it != m_spotCache.end();) {
        if (nowSecs - it.value() > CACHE_TIMEOUT_SEC * 2)
            it = m_spotCache.erase(it);
        else
            ++it;
    }
}

QByteArray DecodiumPskReporterLite::buildPacket(QList<Spot>& sentSpots, bool forceFlush)
{
    QByteArray payload;
    QDataStream message(&payload, QIODevice::WriteOnly);
    buildPreamble(message, payload);

    QList<Spot> packetSpots;
    while (!m_spots.isEmpty()) {
        Spot const spot = m_spots.takeFirst();
        QList<Spot> candidate = packetSpots;
        candidate.append(spot);
        QByteArray const senderSet = makeSenderSet(candidate);
        int const projected = payload.size() + senderSet.size();
        if (!packetSpots.isEmpty() && projected + paddedBytes(projected) > MAX_PAYLOAD_LENGTH) {
            m_spots.prepend(spot);
            break;
        }
        packetSpots.append(spot);
    }

    if (!packetSpots.isEmpty()) {
        QByteArray senderSet = makeSenderSet(packetSpots);
        message.writeRawData(senderSet.constData(), senderSet.size());
    } else if (!forceFlush) {
        return {};
    }

    setIpfixLength(message, payload);
    message.device()->seek(2 * int(sizeof(quint16)));
    message << static_cast<quint32>(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    sentSpots = packetSpots;
    return payload;
}

void DecodiumPskReporterLite::buildPreamble(QDataStream& message, QByteArray& payload)
{
    message
        << quint16(10u)
        << quint16(0u)
        << quint32(0u)
        << ++m_sequenceNumber
        << m_observationId;

    if (m_sendDescriptors > 0) {
        --m_sendDescriptors;

        QByteArray senderDescriptor;
        QDataStream senderOut(&senderDescriptor, QIODevice::WriteOnly);
        senderOut
            << quint16(2u)
            << quint16(0u)
            << kTemplateSender
            << quint16(7u)
            << quint16(0x8000 + 1u) << quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 5u) << quint16(5u)      << kPskReporterEnterprise
            << quint16(0x8000 + 6u) << quint16(1u)      << kPskReporterEnterprise
            << quint16(0x8000 + 10u)<< quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 3u) << quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 11u)<< quint16(1u)      << kPskReporterEnterprise
            << quint16(150u)        << quint16(4u);
        setIpfixLength(senderOut, senderDescriptor);
        message.writeRawData(senderDescriptor.constData(), senderDescriptor.size());

        QByteArray receiverDescriptor;
        QDataStream receiverOut(&receiverDescriptor, QIODevice::WriteOnly);
        receiverOut
            << quint16(3u)
            << quint16(0u)
            << kTemplateReceiver
            << quint16(5u)
            << quint16(1u)
            << quint16(0x8000 + 2u) << quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 4u) << quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 8u) << quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 9u) << quint16(0xffff) << kPskReporterEnterprise
            << quint16(0x8000 + 13u)<< quint16(0xffff) << kPskReporterEnterprise;
        setIpfixLength(receiverOut, receiverDescriptor);
        message.writeRawData(receiverDescriptor.constData(), receiverDescriptor.size());
    }

    Q_UNUSED(payload);

    QByteArray receiverData;
    QDataStream receiverOut(&receiverData, QIODevice::WriteOnly);
    receiverOut << kTemplateReceiver << quint16(0u);
    writeUtfString(receiverOut, m_myCall);
    writeUtfString(receiverOut, m_myGrid);
    writeUtfString(receiverOut, m_programInfo);
    writeUtfString(receiverOut, m_antennaInfo);
    writeUtfString(receiverOut, m_rigInfo);
    setIpfixLength(receiverOut, receiverData);
    message.writeRawData(receiverData.constData(), receiverData.size());

    if (m_sendReceiverData > 0)
        --m_sendReceiverData;
}

QByteArray DecodiumPskReporterLite::makeSenderSet(const QList<Spot>& spots)
{
    QByteArray senderSet;
    QDataStream out(&senderSet, QIODevice::WriteOnly);
    out << kTemplateSender << quint16(0u);

    for (const Spot& spot : spots) {
        writeUtfString(out, spot.call);

        quint64 const freq = spot.freqHz;
        out << quint8((freq >> 32) & 0xff)
            << quint8((freq >> 24) & 0xff)
            << quint8((freq >> 16) & 0xff)
            << quint8((freq >> 8) & 0xff)
            << quint8(freq & 0xff)
            << static_cast<qint8>(spot.snr);

        writeUtfString(out, spot.mode);
        writeUtfString(out, spot.grid);
        out << quint8(1u)
            << static_cast<quint32>(spot.time.toSecsSinceEpoch());
    }

    setIpfixLength(out, senderSet);
    return senderSet;
}

void DecodiumPskReporterLite::writeUtfString(QDataStream& out, const QString& value)
{
    QByteArray const utf = value.toUtf8().left(254);
    out << quint8(utf.size());
    out.writeRawData(utf.constData(), utf.size());
}

int DecodiumPskReporterLite::paddedBytes(int length)
{
    return (4 - length % 4) % 4;
}

void DecodiumPskReporterLite::setIpfixLength(QDataStream& out, QByteArray& data)
{
    int const padLen = paddedBytes(data.size());
    if (padLen > 0)
        out.writeRawData(QByteArray(padLen, '\0').constData(), padLen);

    qint64 const pos = out.device()->pos();
    out.device()->seek(int(sizeof(quint16)));
    out << static_cast<quint16>(data.size());
    out.device()->seek(pos);
}

void DecodiumPskReporterLite::onFlushTimer()
{
    sendReport(false);
}

void DecodiumPskReporterLite::onDescriptorTimer()
{
    if (!m_enabled)
        return;

    markDescriptorsDirty();
    if (pskReporterDebugEnabled())
        qInfo().noquote() << "[PSKDBG] PSK Reporter descriptors marked for resend";
    if (!m_spots.isEmpty())
        scheduleSend(SEND_DELAY_MS);
}

void DecodiumPskReporterLite::onSocketConnected()
{
    m_consecutiveFailures = 0;
    emit connectedChanged();
    if (pskReporterDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter connected protocol="
                          << (m_useTcpIp ? QStringLiteral("TCP") : QStringLiteral("UDP"));
    }
    if (receiverInfoPending()) {
        sendReport(false);
    } else if (!m_spots.isEmpty()) {
        sendReport(false);
    }
}

void DecodiumPskReporterLite::onSocketError(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::RemoteHostClosedError)
        return;

    ++m_consecutiveFailures;
    QString const msg = QStringLiteral("PSK Reporter socket error: %1").arg(
        m_socket ? m_socket->errorString() : QStringLiteral("unknown socket error"));
    qWarning().noquote() << "[PSKDBG]" << msg;

    if (m_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES && !m_autoDisabled) {
        m_autoDisabled = true;
        m_enabled = false;
        m_flushTimer->stop();
        m_descriptorTimer->stop();
        m_sendTimer->stop();
        emit errorOccurred(QStringLiteral("PSK Reporter auto-disabled after repeated network failures."));
    } else {
        emit errorOccurred(msg);
    }
    emit connectedChanged();
}
