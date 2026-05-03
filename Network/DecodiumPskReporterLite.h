#ifndef DECODIUM_PSK_REPORTER_LITE_H
#define DECODIUM_PSK_REPORTER_LITE_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QAbstractSocket>
#include <QString>

class QDataStream;
class QTimer;

class DecodiumPskReporterLite : public QObject
{
    Q_OBJECT

public:
    explicit DecodiumPskReporterLite(QObject* parent = nullptr);
    ~DecodiumPskReporterLite() override;

    void setLocalStation(const QString& callsign,
                         const QString& grid,
                         const QString& programInfo = QStringLiteral("Decodium4"),
                         const QString& antennaInfo = QString(),
                         const QString& rigInfo = QString());

    void addSpot(const QString& call,
                 const QString& grid,
                 quint64 freqHz,
                 const QString& mode,
                 int snr);

    void sendReport(bool last = false);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool v);

    bool useTcpIp() const { return m_useTcpIp; }
    void setUseTcpIp(bool v);

    bool isConnected() const;

signals:
    void connectedChanged();
    void errorOccurred(const QString& msg);
    void spotsUploaded(int count);

private:
    struct Spot {
        QString call;
        QString grid;
        QString mode;
        quint64 freqHz {};
        int snr {};
        QDateTime time;
    };

    void connectSocketIfNeeded();
    void resetSocket();
    void stopSocket();
    void markDescriptorsDirty();
    bool receiverInfoPending() const;
    void scheduleSend(int delayMs = SEND_DELAY_MS);
    void pruneSpotCache(qint64 nowSecs);

    QByteArray buildPacket(QList<Spot>& sentSpots, bool forceFlush);
    void buildPreamble(QDataStream& out, QByteArray& payload);

    static QByteArray makeSenderSet(const QList<Spot>& spots);
    static void writeUtfString(QDataStream& out, const QString& value);
    static int paddedBytes(int length);
    static void setIpfixLength(QDataStream& out, QByteArray& data);

private slots:
    void onFlushTimer();
    void onDescriptorTimer();
    void onSocketConnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    QAbstractSocket* m_socket {nullptr};
    QTimer* m_flushTimer {nullptr};
    QTimer* m_descriptorTimer {nullptr};
    QTimer* m_sendTimer {nullptr};
    QList<Spot> m_spots;
    QHash<QString, qint64> m_spotCache;

    QString m_myCall;
    QString m_myGrid;
    QString m_programInfo {QStringLiteral("Decodium4")};
    QString m_antennaInfo;
    QString m_rigInfo {QStringLiteral("No CAT control")};

    bool m_enabled {false};
    bool m_useTcpIp {false};
    bool m_autoDisabled {false};
    bool m_hasSentSpotPacket {false};
    int m_consecutiveFailures {0};
    quint32 m_sequenceNumber {0};
    quint32 m_observationId {0};
    int m_sendDescriptors {0};
    int m_sendReceiverData {0};

    static constexpr int SERVICE_PORT = 4739;
    static constexpr int FLUSH_INTERVAL_MS = 300'000;
    static constexpr int DESCRIPTOR_INTERVAL_MS = 60 * 60 * 1000;
    static constexpr int FIRST_SEND_DELAY_MS = 15'000;
    static constexpr int SEND_DELAY_MS = 121'000;
    static constexpr int FULL_PACKET_SPOT_THRESHOLD = 75;
    static constexpr int CACHE_TIMEOUT_SEC = 300;
    static constexpr int MAX_PAYLOAD_LENGTH = 10'000;
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;
};

#endif // DECODIUM_PSK_REPORTER_LITE_H
