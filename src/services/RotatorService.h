#pragma once

#include <QHostAddress>
#include <QObject>
#include <QStringList>
#include <QVector>

class QTimer;
class QTcpSocket;
class QUdpSocket;

class RotatorService final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString protocol READ protocol WRITE setProtocol NOTIFY configurationChanged)
    Q_PROPERTY(QStringList protocols READ protocols CONSTANT)
    Q_PROPERTY(QString transport READ transport NOTIFY configurationChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY configurationChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY configurationChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool transportReady READ transportReady NOTIFY transportChanged)
    Q_PROPERTY(bool feedbackSupported READ feedbackSupported NOTIFY configurationChanged)
    Q_PROPERTY(bool feedbackAvailable READ feedbackAvailable NOTIFY feedbackChanged)
    Q_PROPERTY(qint64 lastFeedbackMs READ lastFeedbackMs NOTIFY feedbackChanged)
    Q_PROPERTY(double currentAzimuth READ currentAzimuth NOTIFY feedbackChanged)
    Q_PROPERTY(double currentElevation READ currentElevation NOTIFY feedbackChanged)
    Q_PROPERTY(double targetAzimuth READ targetAzimuth NOTIFY targetChanged)
    Q_PROPERTY(double targetElevation READ targetElevation NOTIFY targetChanged)
    Q_PROPERTY(bool targetHasElevation READ targetHasElevation NOTIFY targetChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY trackingChanged)
    Q_PROPERTY(int trackingIntervalMs READ trackingIntervalMs WRITE setTrackingIntervalMs NOTIFY configurationChanged)
    Q_PROPERTY(bool safetyEnabled READ safetyEnabled WRITE setSafetyEnabled NOTIFY safetyChanged)
    Q_PROPERTY(double minAzimuth READ minAzimuth WRITE setMinAzimuth NOTIFY safetyChanged)
    Q_PROPERTY(double maxAzimuth READ maxAzimuth WRITE setMaxAzimuth NOTIFY safetyChanged)
    Q_PROPERTY(double minElevation READ minElevation WRITE setMinElevation NOTIFY safetyChanged)
    Q_PROPERTY(double maxElevation READ maxElevation WRITE setMaxElevation NOTIFY safetyChanged)
    Q_PROPERTY(bool parkOnStop READ parkOnStop WRITE setParkOnStop NOTIFY safetyChanged)
    Q_PROPERTY(double parkAzimuth READ parkAzimuth WRITE setParkAzimuth NOTIFY safetyChanged)
    Q_PROPERTY(double parkElevation READ parkElevation WRITE setParkElevation NOTIFY safetyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit RotatorService(QObject* parent = nullptr);
    ~RotatorService() override;

    QString protocol() const { return m_protocol; }
    QStringList protocols() const;
    static int defaultPortForProtocol(const QString& protocol);
    QString transport() const;
    QString host() const { return m_host; }
    int port() const { return m_port; }
    bool enabled() const { return m_enabled; }
    bool transportReady() const;
    bool feedbackSupported() const;
    bool feedbackAvailable() const { return m_feedbackAvailable; }
    qint64 lastFeedbackMs() const { return m_lastFeedbackMs; }
    double currentAzimuth() const { return m_currentAzimuth; }
    double currentElevation() const { return m_currentElevation; }
    double targetAzimuth() const { return m_targetAzimuth; }
    double targetElevation() const { return m_targetElevation; }
    bool targetHasElevation() const { return m_targetHasElevation; }
    bool tracking() const { return m_tracking; }
    int trackingIntervalMs() const { return m_trackingIntervalMs; }
    bool safetyEnabled() const { return m_safetyEnabled; }
    double minAzimuth() const { return m_minAzimuth; }
    double maxAzimuth() const { return m_maxAzimuth; }
    double minElevation() const { return m_minElevation; }
    double maxElevation() const { return m_maxElevation; }
    bool parkOnStop() const { return m_parkOnStop; }
    double parkAzimuth() const { return m_parkAzimuth; }
    double parkElevation() const { return m_parkElevation; }
    QString status() const { return m_status; }

    void setProtocol(const QString& protocol);
    void setHost(const QString& host);
    void setPort(int port);
    void setEnabled(bool enabled);
    void setTrackingIntervalMs(int intervalMs);
    void setSafetyEnabled(bool enabled);
    void setMinAzimuth(double value);
    void setMaxAzimuth(double value);
    void setMinElevation(double value);
    void setMaxElevation(double value);
    void setParkOnStop(bool enabled);
    void setParkAzimuth(double value);
    void setParkElevation(double value);

    bool commandTarget(double azimuth, double elevation = 0.0,
                       bool hasElevation = true);
    bool trackTarget(double azimuth, double elevation = 0.0,
                     bool hasElevation = true);
    void stopTracking();
    void emergencyStop();
    Q_INVOKABLE void park();
    void pollFeedback();

    Q_INVOKABLE bool setTarget(double azimuth, double elevation = 0.0,
                               bool hasElevation = true)
    {
        return commandTarget(azimuth, elevation, hasElevation);
    }
    Q_INVOKABLE bool startTracking(double azimuth, double elevation = 0.0,
                                   bool hasElevation = true)
    {
        return trackTarget(azimuth, elevation, hasElevation);
    }
    Q_INVOKABLE void stop() { emergencyStop(); }

signals:
    void configurationChanged();
    void enabledChanged();
    void transportChanged();
    void feedbackChanged();
    void targetChanged();
    void trackingChanged();
    void safetyChanged();
    void statusChanged();

private slots:
    void onTrackingTick();
    void onFeedbackTick();
    void onReadyRead();
    void onTcpReadyRead();

private:
    QByteArray wrapCommand(const QByteArray& body) const;
    bool validateTarget(double* azimuth, double* elevation,
                        bool hasElevation, QString* reason) const;
    void sendCurrentTarget();
    void sendStopCommand();
    void sendParkCommand();
    void sendPayload(const QByteArray& payload);
    void sendTcpCommand(const QByteArray& command);
    void ensureTcpConnected();
    void flushTcpCommand();
    void closeTcpConnection();
    void writePayload(const QByteArray& payload, const QHostAddress& address);
    void configureFeedbackSocket();
    void closeFeedbackSocket();
    void setStatus(const QString& status);
    void setFeedback(double azimuth, bool hasAzimuth,
                     double elevation, bool hasElevation);
    static QString normalizedProtocol(const QString& protocol);
    static bool isHamlibProtocol(const QString& protocol);
    static double normalizeAzimuth(double value);

    QString m_protocol {QStringLiteral("PSTRotator")};
    QString m_host {QStringLiteral("127.0.0.1")};
    int m_port {12000};
    bool m_enabled {false};
    bool m_feedbackAvailable {false};
    qint64 m_lastFeedbackMs {0};
    double m_currentAzimuth {0.0};
    double m_currentElevation {0.0};
    double m_targetAzimuth {0.0};
    double m_targetElevation {0.0};
    bool m_targetHasElevation {false};
    bool m_tracking {false};
    int m_trackingIntervalMs {1000};
    bool m_safetyEnabled {true};
    double m_minAzimuth {0.0};
    double m_maxAzimuth {360.0};
    double m_minElevation {0.0};
    double m_maxElevation {180.0};
    bool m_parkOnStop {false};
    double m_parkAzimuth {0.0};
    double m_parkElevation {0.0};
    QString m_status {QStringLiteral("Rotator disabled")};
    qint64 m_lastCommandMs {0};
    quint64 m_resolutionGeneration {0};
    bool m_resolutionInFlight {false};
    QString m_resolvedHost;
    QHostAddress m_resolvedAddress;
    QByteArray m_pendingPayload;
    QByteArray m_pendingTcpCommand;
    QByteArray m_tcpReadBuffer;
    QVector<double> m_tcpNumericFeedback;

    QUdpSocket* m_commandSocket {nullptr};
    QUdpSocket* m_feedbackSocket {nullptr};
    QTcpSocket* m_tcpSocket {nullptr};
    QTimer* m_trackingTimer {nullptr};
    QTimer* m_feedbackTimer {nullptr};
};
