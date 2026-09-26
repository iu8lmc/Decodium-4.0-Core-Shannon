#include "RotatorService.h"

#include <QDateTime>
#include <QHostInfo>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QtMath>

#include <cmath>

namespace {

constexpr qint64 kFeedbackTimeoutMs = 5000;

bool isFiniteValue(double value)
{
    return qIsFinite(value);
}

}

RotatorService::RotatorService(QObject* parent)
    : QObject(parent),
      m_commandSocket(new QUdpSocket(this)),
      m_feedbackSocket(new QUdpSocket(this)),
      m_tcpSocket(new QTcpSocket(this)),
      m_trackingTimer(new QTimer(this)),
      m_feedbackTimer(new QTimer(this))
{
    m_trackingTimer->setInterval(m_trackingIntervalMs);
    m_feedbackTimer->setInterval(1500);
    connect(m_trackingTimer, &QTimer::timeout,
            this, &RotatorService::onTrackingTick);
    connect(m_feedbackTimer, &QTimer::timeout,
            this, &RotatorService::onFeedbackTick);
    connect(m_feedbackSocket, &QUdpSocket::readyRead,
            this, &RotatorService::onReadyRead);
    connect(m_tcpSocket, &QTcpSocket::connected, this, [this]() {
        emit transportChanged();
        setStatus(QStringLiteral("Hamlib rotctld connected"));
        flushTcpCommand();
    });
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, [this]() {
        m_tcpReadBuffer.clear();
        m_tcpNumericFeedback.clear();
        if (m_feedbackAvailable) {
            m_feedbackAvailable = false;
            emit feedbackChanged();
        }
        emit transportChanged();
        if (m_enabled && isHamlibProtocol(m_protocol))
            setStatus(QStringLiteral("Hamlib rotctld disconnected"));
    });
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        if (m_enabled && isHamlibProtocol(m_protocol)) {
            emit transportChanged();
            setStatus(QStringLiteral("Hamlib rotctld: %1").arg(m_tcpSocket->errorString()));
        }
    });
    connect(m_tcpSocket, &QTcpSocket::readyRead,
            this, &RotatorService::onTcpReadyRead);
}

RotatorService::~RotatorService()
{
    if (m_tracking) {
        m_tracking = false;
        m_trackingTimer->stop();
    }
}

QStringList RotatorService::protocols() const
{
    return {QStringLiteral("PSTRotator"), QStringLiteral("CatRotator"),
            QStringLiteral("Hamlib rotctld")};
}

QString RotatorService::normalizedProtocol(const QString& protocol)
{
    if (protocol.contains(QStringLiteral("hamlib"), Qt::CaseInsensitive)
        || protocol.contains(QStringLiteral("rotctld"), Qt::CaseInsensitive)) {
        return QStringLiteral("Hamlib rotctld");
    }
    if (protocol.compare(QStringLiteral("CatRotator"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("CatRotator");
    return QStringLiteral("PSTRotator");
}

bool RotatorService::isHamlibProtocol(const QString& protocol)
{
    return normalizedProtocol(protocol) == QStringLiteral("Hamlib rotctld");
}

int RotatorService::defaultPortForProtocol(const QString& protocol)
{
    return isHamlibProtocol(protocol) ? 4533 : 12000;
}

QString RotatorService::transport() const
{
    return isHamlibProtocol(m_protocol) ? QStringLiteral("TCP")
                                        : QStringLiteral("UDP");
}

bool RotatorService::transportReady() const
{
    if (!m_enabled) return false;
    if (isHamlibProtocol(m_protocol)) {
        return m_tcpSocket
            && m_tcpSocket->state() == QAbstractSocket::ConnectedState;
    }
    return m_commandSocket != nullptr;
}

bool RotatorService::feedbackSupported() const
{
    return m_protocol != QStringLiteral("CatRotator");
}

double RotatorService::normalizeAzimuth(double value)
{
    double normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) normalized += 360.0;
    return normalized;
}

void RotatorService::setProtocol(const QString& protocol)
{
    QString const normalized = normalizedProtocol(protocol);
    if (normalized == m_protocol) return;
    if (m_tracking) stopTracking();
    bool const wasEnabled = m_enabled;
    m_protocol = normalized;
    m_feedbackAvailable = false;
    m_lastFeedbackMs = 0;
    closeFeedbackSocket();
    closeTcpConnection();
    m_tcpReadBuffer.clear();
    m_tcpNumericFeedback.clear();
    m_pendingTcpCommand.clear();
    if (wasEnabled) {
        configureFeedbackSocket();
        if (isHamlibProtocol(m_protocol)) ensureTcpConnected();
    }
    emit configurationChanged();
    emit transportChanged();
    emit feedbackChanged();
    if (m_protocol == QStringLiteral("PSTRotator")) {
        setStatus(QStringLiteral("PSTRotator selected; UDP %1").arg(m_port));
    } else if (m_protocol == QStringLiteral("CatRotator")) {
        setStatus(QStringLiteral("CatRotator selected; UDP feedback unavailable"));
    } else {
        setStatus(QStringLiteral("Hamlib rotctld selected; TCP %1").arg(m_port));
    }
}

void RotatorService::setHost(const QString& host)
{
    QString const normalized = host.trimmed();
    if (normalized.isEmpty() || normalized == m_host) return;
    m_host = normalized;
    m_resolvedHost.clear();
    m_resolvedAddress.clear();
    ++m_resolutionGeneration;
    if (isHamlibProtocol(m_protocol)) {
        closeTcpConnection();
        if (m_enabled) ensureTcpConnected();
    }
    emit configurationChanged();
}

void RotatorService::setPort(int port)
{
    int const bounded = qBound(1, port, 65535);
    if (bounded == m_port) return;
    if (m_tracking) stopTracking();
    m_port = bounded;
    m_feedbackAvailable = false;
    m_lastFeedbackMs = 0;
    closeFeedbackSocket();
    closeTcpConnection();
    configureFeedbackSocket();
    if (isHamlibProtocol(m_protocol) && m_enabled) ensureTcpConnected();
    emit configurationChanged();
    emit transportChanged();
    emit feedbackChanged();
}

void RotatorService::setEnabled(bool enabled)
{
    if (enabled == m_enabled) return;
    if (!enabled && m_enabled) {
        stopTracking();
    }
    m_enabled = enabled;
    if (m_enabled) {
        configureFeedbackSocket();
        m_feedbackTimer->start();
        if (isHamlibProtocol(m_protocol)) {
            ensureTcpConnected();
            setStatus(QStringLiteral("Hamlib rotctld connecting to %1:%2")
                          .arg(m_host).arg(m_port));
        } else {
            setStatus(m_protocol == QStringLiteral("PSTRotator")
                          ? QStringLiteral("PSTRotator ready on UDP %1").arg(m_port)
                          : QStringLiteral("CatRotator ready on UDP %1; feedback unavailable")
                                .arg(m_port));
        }
    } else {
        m_feedbackTimer->stop();
        closeFeedbackSocket();
        closeTcpConnection();
        m_pendingTcpCommand.clear();
        m_feedbackAvailable = false;
        m_lastFeedbackMs = 0;
        setStatus(QStringLiteral("Rotator disabled"));
    }
    emit enabledChanged();
    emit transportChanged();
    emit feedbackChanged();
}

void RotatorService::setTrackingIntervalMs(int intervalMs)
{
    int const bounded = qBound(250, intervalMs, 10000);
    if (bounded == m_trackingIntervalMs) return;
    m_trackingIntervalMs = bounded;
    m_trackingTimer->setInterval(m_trackingIntervalMs);
    emit configurationChanged();
}

void RotatorService::setSafetyEnabled(bool enabled)
{
    if (enabled == m_safetyEnabled) return;
    m_safetyEnabled = enabled;
    emit safetyChanged();
}

void RotatorService::setMinAzimuth(double value)
{
    double const bounded = qBound(0.0, value, 360.0);
    if (qFuzzyCompare(bounded, m_minAzimuth)) return;
    m_minAzimuth = bounded;
    emit safetyChanged();
}

void RotatorService::setMaxAzimuth(double value)
{
    double const bounded = qBound(0.0, value, 360.0);
    if (qFuzzyCompare(bounded, m_maxAzimuth)) return;
    m_maxAzimuth = bounded;
    emit safetyChanged();
}

void RotatorService::setMinElevation(double value)
{
    double const bounded = qBound(-10.0, value, 180.0);
    if (qFuzzyCompare(bounded, m_minElevation)) return;
    m_minElevation = bounded;
    emit safetyChanged();
}

void RotatorService::setMaxElevation(double value)
{
    double const bounded = qBound(-10.0, value, 180.0);
    if (qFuzzyCompare(bounded, m_maxElevation)) return;
    m_maxElevation = bounded;
    emit safetyChanged();
}

void RotatorService::setParkOnStop(bool enabled)
{
    if (enabled == m_parkOnStop) return;
    m_parkOnStop = enabled;
    emit safetyChanged();
}

void RotatorService::setParkAzimuth(double value)
{
    if (!isFiniteValue(value)) return;
    double const normalized = normalizeAzimuth(value);
    if (qFuzzyCompare(normalized, m_parkAzimuth)) return;
    m_parkAzimuth = normalized;
    emit safetyChanged();
}

void RotatorService::setParkElevation(double value)
{
    double const bounded = qBound(-10.0, value, 180.0);
    if (qFuzzyCompare(bounded, m_parkElevation)) return;
    m_parkElevation = bounded;
    emit safetyChanged();
}

bool RotatorService::validateTarget(double* azimuth, double* elevation,
                                    bool hasElevation, QString* reason) const
{
    if (!azimuth || !elevation || !isFiniteValue(*azimuth)
        || (hasElevation && !isFiniteValue(*elevation))) {
        if (reason) *reason = QStringLiteral("Non-finite rotator target");
        return false;
    }
    *azimuth = normalizeAzimuth(*azimuth);
    if (hasElevation) *elevation = qBound(-10.0, *elevation, 180.0);
    if (!m_safetyEnabled) return true;
    if (m_minAzimuth > m_maxAzimuth
        || *azimuth < m_minAzimuth || *azimuth > m_maxAzimuth) {
        if (reason) {
            *reason = QStringLiteral("Azimuth %1 outside safety limits %2..%3")
                          .arg(*azimuth, 0, 'f', 1)
                          .arg(m_minAzimuth, 0, 'f', 1)
                          .arg(m_maxAzimuth, 0, 'f', 1);
        }
        return false;
    }
    if (hasElevation && (m_minElevation > m_maxElevation
                         || *elevation < m_minElevation
                         || *elevation > m_maxElevation)) {
        if (reason) {
            *reason = QStringLiteral("Elevation %1 outside safety limits %2..%3")
                          .arg(*elevation, 0, 'f', 1)
                          .arg(m_minElevation, 0, 'f', 1)
                          .arg(m_maxElevation, 0, 'f', 1);
        }
        return false;
    }
    return true;
}

QByteArray RotatorService::wrapCommand(const QByteArray& body) const
{
    if (m_protocol == QStringLiteral("PSTRotator")) {
        return QByteArray("<PST>") + body + QByteArray("</PST>");
    }
    return body;
}

bool RotatorService::commandTarget(double azimuth, double elevation,
                                   bool hasElevation)
{
    if (!m_enabled) {
        setStatus(QStringLiteral("Rotator disabled"));
        return false;
    }
    QString reason;
    if (!validateTarget(&azimuth, &elevation, hasElevation, &reason)) {
        setStatus(QStringLiteral("Rotator safety stop: %1").arg(reason));
        return false;
    }
    bool const changed = !qFuzzyCompare(azimuth + 1.0, m_targetAzimuth + 1.0)
        || !qFuzzyCompare(elevation + 1.0, m_targetElevation + 1.0)
        || hasElevation != m_targetHasElevation;
    m_targetAzimuth = azimuth;
    m_targetElevation = elevation;
    m_targetHasElevation = hasElevation;
    if (changed) emit targetChanged();
    sendCurrentTarget();
    return true;
}

bool RotatorService::trackTarget(double azimuth, double elevation,
                                 bool hasElevation)
{
    if (!commandTarget(azimuth, elevation, hasElevation)) return false;
    if (!m_tracking) {
        m_tracking = true;
        m_trackingTimer->start();
        emit trackingChanged();
    }
    setStatus(QStringLiteral("Rotator tracking %1°%2")
                  .arg(m_targetAzimuth, 0, 'f', 1)
                  .arg(m_targetHasElevation
                           ? QStringLiteral(" / %1°").arg(m_targetElevation, 0, 'f', 1)
                           : QString()));
    return true;
}

void RotatorService::stopTracking()
{
    bool const wasTracking = m_tracking;
    m_tracking = false;
    m_trackingTimer->stop();
    if (wasTracking) emit trackingChanged();
    if (m_enabled) {
        sendStopCommand();
        if (m_parkOnStop) sendParkCommand();
    }
    if (wasTracking) setStatus(QStringLiteral("Rotator tracking stopped"));
}

void RotatorService::emergencyStop()
{
    m_tracking = false;
    m_trackingTimer->stop();
    if (m_enabled) sendStopCommand();
    emit trackingChanged();
    setStatus(QStringLiteral("Rotator emergency stop sent"));
}

void RotatorService::park()
{
    if (!m_enabled) {
        setStatus(QStringLiteral("Rotator disabled"));
        return;
    }
    m_tracking = false;
    m_trackingTimer->stop();
    sendParkCommand();
    emit trackingChanged();
    setStatus(QStringLiteral("Rotator park command sent"));
}

void RotatorService::sendCurrentTarget()
{
    if (isHamlibProtocol(m_protocol)) {
        sendPayload(QByteArray("P ")
                    + QByteArray::number(m_targetAzimuth, 'f', 1)
                    + QByteArray(" ")
                    + QByteArray::number(m_targetHasElevation
                                             ? m_targetElevation : 0.0, 'f', 1)
                    + QByteArray("\n"));
        m_lastCommandMs = QDateTime::currentMSecsSinceEpoch();
        return;
    }
    QByteArray const azimuthText = m_targetHasElevation
        ? QByteArray::number(m_targetAzimuth, 'f', 1)
        : QByteArray::number(qRound(m_targetAzimuth));
    QByteArray body = QByteArray("<AZIMUTH>")
        + azimuthText
        + QByteArray("</AZIMUTH>");
    if (m_targetHasElevation) {
        body += QByteArray("<ELEVATION>")
            + QByteArray::number(m_targetElevation, 'f', 1)
            + QByteArray("</ELEVATION>");
    }
    sendPayload(wrapCommand(body));
    m_lastCommandMs = QDateTime::currentMSecsSinceEpoch();
}

void RotatorService::sendStopCommand()
{
    if (isHamlibProtocol(m_protocol)) {
        sendPayload(QByteArray("S\n"));
        return;
    }
    sendPayload(wrapCommand(QByteArray("<STOP>1</STOP>")));
}

void RotatorService::sendParkCommand()
{
    if (isHamlibProtocol(m_protocol)) {
        sendPayload(QByteArray("K\n"));
        return;
    }
    if (m_protocol == QStringLiteral("PSTRotator")) {
        sendPayload(wrapCommand(QByteArray("<PARK>1</PARK>")));
        return;
    }
    // CatRotator's UDP listener accepts PARK as a command; the explicit
    // configured position is also sent first for rotators without a park
    // preset, subject to the same safety limits as every other target.
    double azimuth = m_parkAzimuth;
    double elevation = m_parkElevation;
    QString reason;
    if (validateTarget(&azimuth, &elevation, true, &reason)) {
        sendPayload(wrapCommand(QByteArray("<AZIMUTH>")
                                + QByteArray::number(azimuth, 'f', 1)
                                + QByteArray("</AZIMUTH><ELEVATION>")
                                + QByteArray::number(elevation, 'f', 1)
                                + QByteArray("</ELEVATION>")));
    }
    sendPayload(wrapCommand(QByteArray("<PARK>1</PARK>")));
}

void RotatorService::onTrackingTick()
{
    if (!m_tracking || !m_enabled) return;
    sendCurrentTarget();
}

void RotatorService::pollFeedback()
{
    if (!m_enabled) return;
    if (isHamlibProtocol(m_protocol)) {
        ensureTcpConnected();
        sendPayload(QByteArray("p\n"));
        return;
    }
    if (m_protocol != QStringLiteral("PSTRotator")) return;
    configureFeedbackSocket();
    sendPayload(wrapCommand(QByteArray("AZ?")));
    sendPayload(wrapCommand(QByteArray("EL?")));
}

void RotatorService::onFeedbackTick()
{
    if (!m_enabled || !feedbackSupported()) return;
    pollFeedback();
    if (m_feedbackAvailable
        && QDateTime::currentMSecsSinceEpoch() - m_lastFeedbackMs > kFeedbackTimeoutMs) {
        m_feedbackAvailable = false;
        emit feedbackChanged();
        setStatus(QStringLiteral("%1 feedback timeout").arg(m_protocol));
    }
}

void RotatorService::onReadyRead()
{
    while (m_feedbackSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_feedbackSocket->pendingDatagramSize()));
        m_feedbackSocket->readDatagram(datagram.data(), datagram.size());
        QString const text = QString::fromUtf8(datagram);
        QRegularExpression const azimuthExpression(
            QStringLiteral("(?:^|\\s|:)AZ(?:IMUTH)?\\s*[:=]\\s*(-?\\d+(?:[.,]\\d+)?)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpression const elevationExpression(
            QStringLiteral("(?:^|\\s|:)EL(?:EVATION)?\\s*[:=]\\s*(-?\\d+(?:[.,]\\d+)?)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch const azimuthMatch = azimuthExpression.match(text);
        QRegularExpressionMatch const elevationMatch = elevationExpression.match(text);
        bool const hasAzimuth = azimuthMatch.hasMatch();
        bool const hasElevation = elevationMatch.hasMatch();
        if (!hasAzimuth && !hasElevation) continue;
        double azimuth = hasAzimuth
            ? azimuthMatch.captured(1).replace(',', '.').toDouble() : m_currentAzimuth;
        double elevation = hasElevation
            ? elevationMatch.captured(1).replace(',', '.').toDouble() : m_currentElevation;
        setFeedback(azimuth, hasAzimuth, elevation, hasElevation);
    }
}

void RotatorService::onTcpReadyRead()
{
    if (!m_tcpSocket) return;
    m_tcpReadBuffer.append(m_tcpSocket->readAll());
    while (true) {
        int const newline = m_tcpReadBuffer.indexOf('\n');
        if (newline < 0) break;
        QByteArray line = m_tcpReadBuffer.left(newline).trimmed();
        m_tcpReadBuffer.remove(0, newline + 1);
        if (line.isEmpty()) continue;

        QString const text = QString::fromUtf8(line).trimmed();
        if (text.startsWith(QStringLiteral("RPRT"), Qt::CaseInsensitive)) {
            bool ok = false;
            int const code = text.section(QLatin1Char(' '), 1, 1).toInt(&ok);
            if (ok && code != 0) {
                m_tcpNumericFeedback.clear();
                setStatus(QStringLiteral("Hamlib rotctld error %1").arg(code));
            }
            continue;
        }

        QString valueText = text;
        if (text.startsWith(QStringLiteral("Azimuth:"), Qt::CaseInsensitive)) {
            valueText = text.section(QLatin1Char(':'), 1).trimmed();
        } else if (text.startsWith(QStringLiteral("Elevation:"), Qt::CaseInsensitive)) {
            valueText = text.section(QLatin1Char(':'), 1).trimmed();
        }
        bool ok = false;
        double const value = valueText.toDouble(&ok);
        if (!ok || !qIsFinite(value)) continue;
        m_tcpNumericFeedback.append(value);
        if (m_tcpNumericFeedback.size() >= 2) {
            setFeedback(m_tcpNumericFeedback.at(0), true,
                        m_tcpNumericFeedback.at(1), true);
            m_tcpNumericFeedback.clear();
        }
    }
}

void RotatorService::setFeedback(double azimuth, bool hasAzimuth,
                                 double elevation, bool hasElevation)
{
    if (hasAzimuth) m_currentAzimuth = normalizeAzimuth(azimuth);
    if (hasElevation) m_currentElevation = elevation;
    m_feedbackAvailable = true;
    m_lastFeedbackMs = QDateTime::currentMSecsSinceEpoch();
    emit feedbackChanged();
    setStatus(QStringLiteral("Rotator feedback AZ %1° / EL %2°")
                  .arg(m_currentAzimuth, 0, 'f', 1)
                  .arg(m_currentElevation, 0, 'f', 1));
}

void RotatorService::configureFeedbackSocket()
{
    if (!m_enabled || m_protocol != QStringLiteral("PSTRotator") || m_port >= 65535) {
        return;
    }
    if (m_feedbackSocket->state() != QAbstractSocket::UnconnectedState) return;
    if (!m_feedbackSocket->bind(QHostAddress::AnyIPv4,
                                static_cast<quint16>(m_port + 1),
                                QUdpSocket::ShareAddress
                                    | QUdpSocket::ReuseAddressHint)) {
        setStatus(QStringLiteral("Cannot listen for PSTRotator feedback on UDP %1")
                      .arg(m_port + 1));
    }
}

void RotatorService::closeFeedbackSocket()
{
    if (!m_feedbackSocket) return;
    if (m_feedbackSocket->state() != QAbstractSocket::UnconnectedState) {
        m_feedbackSocket->close();
    }
}

void RotatorService::ensureTcpConnected()
{
    if (!m_enabled || !isHamlibProtocol(m_protocol) || !m_tcpSocket) return;
    QAbstractSocket::SocketState const state = m_tcpSocket->state();
    if (state == QAbstractSocket::ConnectedState
        || state == QAbstractSocket::ConnectingState) {
        return;
    }
    m_tcpSocket->connectToHost(m_host, static_cast<quint16>(m_port));
    emit transportChanged();
}

void RotatorService::flushTcpCommand()
{
    if (!m_tcpSocket
        || m_tcpSocket->state() != QAbstractSocket::ConnectedState
        || m_pendingTcpCommand.isEmpty()) {
        return;
    }
    QByteArray command = m_pendingTcpCommand;
    m_pendingTcpCommand.clear();
    m_tcpSocket->write(command);
}

void RotatorService::sendTcpCommand(const QByteArray& command)
{
    if (!m_tcpSocket || command.isEmpty()) return;
    if (m_tcpSocket->state() != QAbstractSocket::ConnectedState) {
        // Keep only the newest command while the asynchronous connection is
        // being established. This avoids a backlog of stale tracking targets.
        m_pendingTcpCommand = command;
        ensureTcpConnected();
        return;
    }
    m_tcpSocket->write(command);
}

void RotatorService::closeTcpConnection()
{
    if (!m_tcpSocket) return;
    m_pendingTcpCommand.clear();
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState)
        m_tcpSocket->abort();
    emit transportChanged();
}

void RotatorService::sendPayload(const QByteArray& payload)
{
    if (isHamlibProtocol(m_protocol)) {
        sendTcpCommand(payload);
        return;
    }
    if (!m_commandSocket || payload.isEmpty() || m_host.trimmed().isEmpty()) return;
    QHostAddress address;
    if (address.setAddress(m_host)) {
        writePayload(payload, address);
        return;
    }
    if (m_resolvedHost == m_host && !m_resolvedAddress.isNull()) {
        writePayload(payload, m_resolvedAddress);
        return;
    }
    m_pendingPayload = payload;
    if (m_resolutionInFlight) return;
    m_resolutionInFlight = true;
    QString const host = m_host;
    quint64 const generation = ++m_resolutionGeneration;
    QHostInfo::lookupHost(host, this,
                          [this, host, generation](const QHostInfo& info) {
        if (generation != m_resolutionGeneration || host != m_host) return;
        m_resolutionInFlight = false;
        if (info.addresses().isEmpty()) {
            setStatus(QStringLiteral("Rotator host lookup failed: %1").arg(host));
            return;
        }
        m_resolvedHost = host;
        m_resolvedAddress = info.addresses().first();
        QByteArray const pending = m_pendingPayload;
        m_pendingPayload.clear();
        writePayload(pending, m_resolvedAddress);
    });
}

void RotatorService::writePayload(const QByteArray& payload,
                                  const QHostAddress& address)
{
    qint64 const written = m_commandSocket->writeDatagram(
        payload, address, static_cast<quint16>(m_port));
    if (written != payload.size()) {
        setStatus(QStringLiteral("Rotator UDP send failed"));
    }
}

void RotatorService::setStatus(const QString& status)
{
    if (status == m_status) return;
    m_status = status;
    emit statusChanged();
}
