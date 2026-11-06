#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QTcpSocket;
class QTimer;

// Lightweight MQTT 3.1.1 subscriber for PSK Reporter. Keeping the protocol
// implementation here avoids adding a QtMqtt deployment dependency.
class MapPskFeedService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString callsign READ callsign NOTIFY stationChanged)
    Q_PROPERTY(QString grid READ grid NOTIFY stationChanged)
    Q_PROPERTY(QString topic READ topic NOTIFY stationChanged)
    Q_PROPERTY(QString clientId READ clientId NOTIFY stationChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString lastMessageUtc READ lastMessageUtc NOTIFY statusChanged)
    Q_PROPERTY(int receivedCount READ receivedCount NOTIFY statusChanged)

public:
    explicit MapPskFeedService(QObject* parent = nullptr);

    bool enabled() const { return m_enabled; }
    bool offlineMode() const { return m_offlineMode; }
    bool connected() const;
    QString endpoint() const { return m_endpoint; }
    QString callsign() const { return m_callsign; }
    QString grid() const { return m_grid; }
    QString topic() const;
    QString clientId() const { return m_clientId; }
    QString lastError() const { return m_lastError; }
    QString lastMessageUtc() const { return m_lastMessageUtc; }
    int receivedCount() const { return m_receivedCount; }

    void setEnabled(bool enabled);
    void setOfflineMode(bool offline);
    void setEndpoint(const QString& endpoint);

    Q_INVOKABLE void configureStation(const QString& callsign, const QString& grid);
    Q_INVOKABLE void reconnect();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool injectPayloadForTest(const QByteArray& payload);

signals:
    void enabledChanged();
    void offlineModeChanged();
    void connectionChanged();
    void endpointChanged();
    void stationChanged();
    void statusChanged();
    void spotsReceived(const QVariantList& rows, const QString& receiverCall,
                       const QString& receiverGrid);

private:
    void connectIfReady();
    void handleConnected();
    void handleReadyRead();
    void handleSocketError();
    void scheduleReconnect();
    void sendConnect();
    void sendSubscribe();
    void sendPing();
    bool processPayload(const QByteArray& payload);
    void setError(const QString& error);
    QByteArray mqttString(const QByteArray& value) const;
    QByteArray mqttPacket(quint8 header, const QByteArray& payload) const;
    QByteArray remainingLength(int length) const;
    static bool readRemainingLength(const QByteArray& bytes, int offset,
                                    int* value, int* lengthBytes);

    QTcpSocket* m_socket {nullptr};
    QTimer* m_reconnectTimer {nullptr};
    QTimer* m_keepAliveTimer {nullptr};
    QByteArray m_buffer;
    QString m_endpoint {QStringLiteral("mqtt://mqtt.pskreporter.info:1883")};
    QString m_callsign;
    QString m_grid;
    QString m_clientId;
    QString m_lastError;
    QString m_lastMessageUtc;
    bool m_enabled {false};
    bool m_offlineMode {false};
    bool m_subscribed {false};
    int m_receivedCount {0};
};
