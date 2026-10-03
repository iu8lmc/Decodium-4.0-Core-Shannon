#include "MapPskFeedService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {

QString normalizedCall(QString call)
{
    return call.trimmed().toUpper().replace(QLatin1Char('.'), QLatin1Char('/'));
}

QString normalizedGrid(QString grid)
{
    grid = grid.trimmed().toUpper();
    return grid.size() >= 4 ? grid.left(6) : QString();
}

QVariantMap spotFromJson(const QJsonObject& object)
{
    auto text = [&object](const char* shortKey, const char* longKey = nullptr) {
        QJsonValue value = object.value(QLatin1String(shortKey));
        if (value.isUndefined() && longKey) value = object.value(QLatin1String(longKey));
        return value.toVariant().toString();
    };
    auto number = [&object](const char* shortKey, const char* longKey = nullptr) {
        QJsonValue value = object.value(QLatin1String(shortKey));
        if (value.isUndefined() && longKey) value = object.value(QLatin1String(longKey));
        return value.toVariant().toLongLong();
    };

    QVariantMap row;
    row.insert(QStringLiteral("call"), normalizedCall(text("rc", "call")));
    row.insert(QStringLiteral("grid"), normalizedGrid(text("rl", "grid")));
    row.insert(QStringLiteral("freq"), number("f", "frequency"));
    row.insert(QStringLiteral("snr"), number("rp", "snr"));
    row.insert(QStringLiteral("mode"), text("md", "mode").trimmed().toUpper());
    row.insert(QStringLiteral("band"), text("b", "band"));
    row.insert(QStringLiteral("timestamp"), number("t", "timestamp"));
    row.insert(QStringLiteral("provider"), QStringLiteral("PSK Reporter MQTT"));
    return row;
}

} // namespace

MapPskFeedService::MapPskFeedService(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_keepAliveTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    m_keepAliveTimer->setInterval(45000);
    connect(m_socket, &QTcpSocket::connected, this, &MapPskFeedService::handleConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MapPskFeedService::handleReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        handleSocketError();
    });
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_subscribed = false;
        emit connectionChanged();
        if (m_enabled && !m_offlineMode) scheduleReconnect();
    });
    connect(m_reconnectTimer, &QTimer::timeout, this, &MapPskFeedService::connectIfReady);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &MapPskFeedService::sendPing);
}

bool MapPskFeedService::connected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState && m_subscribed;
}

QString MapPskFeedService::topic() const
{
    if (m_callsign.isEmpty()) return {};
    QString dotted = m_callsign;
    dotted.replace(QLatin1Char('/'), QLatin1Char('.'));
    return QStringLiteral("pskr/filter/v2/+/+/%1/#").arg(dotted);
}

void MapPskFeedService::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    emit enabledChanged();
    if (m_enabled) {
        if (!m_offlineMode) connectIfReady();
    } else {
        stop();
    }
}

void MapPskFeedService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) return;
    m_offlineMode = offline;
    if (m_offlineMode) {
        stop();
    } else if (m_enabled) {
        connectIfReady();
    }
    emit offlineModeChanged();
}

void MapPskFeedService::setEndpoint(const QString& endpoint)
{
    QString normalized = endpoint.trimmed();
    if (!normalized.contains(QStringLiteral("://"))) normalized.prepend(QStringLiteral("mqtt://"));
    QUrl const url(normalized);
    if (!url.isValid() || url.host().isEmpty()) {
        setError(QStringLiteral("Invalid MQTT endpoint"));
        return;
    }
    if (m_endpoint == normalized) return;
    m_endpoint = normalized;
    emit endpointChanged();
    reconnect();
}

void MapPskFeedService::configureStation(const QString& callsign, const QString& grid)
{
    QString const stationCall = normalizedCall(callsign);
    QString const normalizedStationGrid = normalizedGrid(grid);
    if (m_callsign == stationCall && m_grid == normalizedStationGrid) return;
    m_callsign = stationCall;
    m_grid = normalizedStationGrid;
    QString dottedCall = m_callsign;
    dottedCall.replace(QLatin1Char('/'), QLatin1Char('.'));
    m_clientId = QStringLiteral("Decodium_%1_%2_%3")
                     .arg(dottedCall.isEmpty() ? QStringLiteral("listener")
                                                : dottedCall,
                          m_grid.isEmpty() ? QStringLiteral("nogrid") : m_grid,
                          QString::number(QRandomGenerator::global()->bounded(1000000)));
    emit stationChanged();
    reconnect();
}

void MapPskFeedService::reconnect()
{
    stop();
    if (m_enabled && !m_offlineMode) connectIfReady();
}

void MapPskFeedService::stop()
{
    m_reconnectTimer->stop();
    m_keepAliveTimer->stop();
    m_subscribed = false;
    m_buffer.clear();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) m_socket->abort();
    }
    emit connectionChanged();
}

bool MapPskFeedService::injectPayloadForTest(const QByteArray& payload)
{
    return processPayload(payload);
}

void MapPskFeedService::connectIfReady()
{
    if (!m_enabled || m_offlineMode || m_callsign.isEmpty()
        || m_socket->state() != QAbstractSocket::UnconnectedState) return;
    QUrl const url(m_endpoint);
    int const port = url.port(1883);
    m_socket->connectToHost(url.host(), port);
}

void MapPskFeedService::handleConnected()
{
    m_subscribed = false;
    m_lastError.clear();
    emit statusChanged();
    sendConnect();
}

void MapPskFeedService::handleSocketError()
{
    setError(m_socket->errorString());
    m_subscribed = false;
    emit connectionChanged();
    if (m_enabled && !m_offlineMode) scheduleReconnect();
}

void MapPskFeedService::scheduleReconnect()
{
    if (!m_enabled || m_offlineMode || m_reconnectTimer->isActive()) return;
    m_reconnectTimer->start(90000);
}

void MapPskFeedService::sendConnect()
{
    QByteArray payload;
    payload += mqttString("MQTT");
    payload.append(char(4));
    payload.append(char(0x02));
    payload.append(char(0));
    payload.append(char(60));
    payload += mqttString(m_clientId.toUtf8());
    m_socket->write(mqttPacket(0x10, payload));
}

void MapPskFeedService::sendSubscribe()
{
    QByteArray payload;
    payload.append(char(0));
    payload.append(char(1));
    payload += mqttString(topic().toUtf8());
    payload.append(char(0));
    m_socket->write(mqttPacket(0x82, payload));
}

void MapPskFeedService::sendPing()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QByteArray::fromHex("c000"));
    }
}

void MapPskFeedService::handleReadyRead()
{
    m_buffer += m_socket->readAll();
    while (m_buffer.size() >= 2) {
        int remaining = 0;
        int lengthBytes = 0;
        if (!readRemainingLength(m_buffer, 1, &remaining, &lengthBytes)) return;
        int const headerBytes = 1 + lengthBytes;
        if (m_buffer.size() < headerBytes + remaining) return;
        quint8 const header = static_cast<quint8>(m_buffer.at(0));
        QByteArray const payload = m_buffer.mid(headerBytes, remaining);
        m_buffer.remove(0, headerBytes + remaining);
        quint8 const type = header >> 4;
        if (type == 2 && payload.size() >= 2 && static_cast<quint8>(payload.at(1)) == 0) {
            sendSubscribe();
        } else if (type == 9) {
            m_subscribed = true;
            m_keepAliveTimer->start();
            emit connectionChanged();
        } else if (type == 3 && payload.size() >= 2) {
            int const topicLength = (static_cast<quint8>(payload.at(0)) << 8)
                                    | static_cast<quint8>(payload.at(1));
            int offset = 2 + topicLength;
            if ((header & 0x06) != 0) offset += 2;
            if (offset <= payload.size()) processPayload(payload.mid(offset));
        }
    }
}

bool MapPskFeedService::processPayload(const QByteArray& payload)
{
    QJsonParseError error;
    QJsonDocument const document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError) {
        setError(QStringLiteral("PSK MQTT JSON: %1").arg(error.errorString()));
        return false;
    }
    QJsonArray entries;
    if (document.isArray()) entries = document.array();
    else if (document.isObject()) {
        QJsonObject const root = document.object();
        entries = root.value(QStringLiteral("spots")).toArray();
        if (entries.isEmpty()) entries = root.value(QStringLiteral("data")).toArray();
        if (entries.isEmpty()) entries.append(root);
    }
    QVariantList rows;
    for (QJsonValue const& value : entries) {
        if (!value.isObject()) continue;
        QVariantMap row = spotFromJson(value.toObject());
        if (!row.value(QStringLiteral("call")).toString().isEmpty()) rows.append(row);
    }
    if (rows.isEmpty()) return false;
    m_receivedCount += rows.size();
    m_lastMessageUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_lastError.clear();
    emit statusChanged();
    emit spotsReceived(rows, m_callsign, m_grid);
    return true;
}

void MapPskFeedService::setError(const QString& error)
{
    if (m_lastError == error) return;
    m_lastError = error;
    emit statusChanged();
}

QByteArray MapPskFeedService::mqttString(const QByteArray& value) const
{
    QByteArray result;
    result.append(char((value.size() >> 8) & 0xff));
    result.append(char(value.size() & 0xff));
    result += value;
    return result;
}

QByteArray MapPskFeedService::mqttPacket(quint8 header, const QByteArray& payload) const
{
    QByteArray result;
    result.append(char(header));
    result += remainingLength(payload.size());
    result += payload;
    return result;
}

QByteArray MapPskFeedService::remainingLength(int length) const
{
    QByteArray result;
    do {
        quint8 byte = static_cast<quint8>(length % 128);
        length /= 128;
        if (length > 0) byte |= 0x80;
        result.append(char(byte));
    } while (length > 0);
    return result;
}

bool MapPskFeedService::readRemainingLength(const QByteArray& bytes, int offset,
                                            int* value, int* lengthBytes)
{
    int multiplier = 1;
    int decoded = 0;
    int count = 0;
    while (offset + count < bytes.size() && count < 4) {
        quint8 const byte = static_cast<quint8>(bytes.at(offset + count));
        decoded += (byte & 0x7f) * multiplier;
        ++count;
        if ((byte & 0x80) == 0) {
            *value = decoded;
            *lengthBytes = count;
            return true;
        }
        multiplier *= 128;
    }
    return false;
}
