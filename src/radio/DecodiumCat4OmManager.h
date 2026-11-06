#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QQueue>
#include <QString>
#include <QStringList>

class QTimer;
class QUrl;
class QWebSocket;

// Native CAT4OM JSON/WebSocket client.
//
// The public surface intentionally mirrors the other Decodium CAT managers so
// the bridge and the shared QML settings UI can switch backend without any
// blocking adapter calls.  CAT4OM itself is push based: frequency, mode, PTT,
// split and metering are updated only from welcome/stateUpdate snapshots.
class DecodiumCat4OmManager final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(QString rigName READ rigName WRITE setRigName NOTIFY rigNameChanged)

    Q_PROPERTY(QString managementEndpoint READ managementEndpoint WRITE setManagementEndpoint NOTIFY managementEndpointChanged)
    Q_PROPERTY(QString controlEndpoint READ controlEndpoint WRITE setControlEndpoint NOTIFY controlEndpointChanged)
    Q_PROPERTY(QString groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
    Q_PROPERTY(QString radioId READ radioId WRITE setRadioId NOTIFY radioIdChanged)
    Q_PROPERTY(QString radioDisplayName READ radioDisplayName NOTIFY radioDisplayNameChanged)
    Q_PROPERTY(QStringList groupList READ groupList NOTIFY groupListChanged)
    Q_PROPERTY(QStringList radioList READ radioList NOTIFY radioListChanged)
    Q_PROPERTY(QString ownershipState READ ownershipState NOTIFY ownershipStateChanged)
    Q_PROPERTY(QString connectionDetail READ connectionDetail NOTIFY connectionDetailChanged)
    Q_PROPERTY(bool autoRequestOwnership READ autoRequestOwnership WRITE setAutoRequestOwnership NOTIFY autoRequestOwnershipChanged)
    Q_PROPERTY(QString protocolVersion READ protocolVersion CONSTANT)
    Q_PROPERTY(QStringList availableCommands READ availableCommands NOTIFY capabilitiesChanged)
    Q_PROPERTY(QStringList supportedModes READ supportedModes NOTIFY capabilitiesChanged)

    // Shared CAT settings surface. Serial-only fields are inert for CAT4OM.
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY serialPortChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(QString dataBits READ dataBits WRITE setDataBits NOTIFY dataBitsChanged)
    Q_PROPERTY(QString stopBits READ stopBits WRITE setStopBits NOTIFY stopBitsChanged)
    Q_PROPERTY(QString handshake READ handshake WRITE setHandshake NOTIFY handshakeChanged)
    Q_PROPERTY(bool forceDtr READ forceDtr WRITE setForceDtr NOTIFY forceDtrChanged)
    Q_PROPERTY(bool dtrHigh READ dtrHigh WRITE setDtrHigh NOTIFY dtrHighChanged)
    Q_PROPERTY(bool forceRts READ forceRts WRITE setForceRts NOTIFY forceRtsChanged)
    Q_PROPERTY(bool rtsHigh READ rtsHigh WRITE setRtsHigh NOTIFY rtsHighChanged)
    Q_PROPERTY(QString networkPort READ networkPort WRITE setNetworkPort NOTIFY networkPortChanged)
    Q_PROPERTY(QString tciPort READ tciPort WRITE setTciPort NOTIFY tciPortChanged)
    Q_PROPERTY(QString pttMethod READ pttMethod WRITE setPttMethod NOTIFY pttMethodChanged)
    Q_PROPERTY(QString pttPort READ pttPort WRITE setPttPort NOTIFY pttPortChanged)
    Q_PROPERTY(int civAddress READ civAddress NOTIFY civAddressChanged)
    Q_PROPERTY(QString splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)
    Q_PROPERTY(bool catKeepAlive READ catKeepAlive WRITE setCatKeepAlive NOTIFY catKeepAliveChanged)
    Q_PROPERTY(int pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)
    Q_PROPERTY(QString portType READ portType CONSTANT)
    Q_PROPERTY(QStringList rigList READ rigList NOTIFY rigListChanged)
    Q_PROPERTY(QStringList portList READ portList NOTIFY portListChanged)
    Q_PROPERTY(QStringList baudList READ baudList CONSTANT)
    Q_PROPERTY(QStringList pttMethodList READ pttMethodList CONSTANT)
    Q_PROPERTY(QStringList splitModeList READ splitModeList CONSTANT)

    Q_PROPERTY(double frequency READ frequency NOTIFY frequencyChanged)
    Q_PROPERTY(double txFrequency READ txFrequency NOTIFY txFrequencyChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool pttActive READ pttActive NOTIFY pttActiveChanged)
    Q_PROPERTY(bool split READ split NOTIFY splitChanged)
    Q_PROPERTY(double powerWatts READ powerWatts NOTIFY powerWattsChanged)
    Q_PROPERTY(double swr READ swr NOTIFY swrChanged)
    Q_PROPERTY(double alc READ alc NOTIFY alcChanged)
    Q_PROPERTY(bool alcValid READ alcValid NOTIFY alcChanged)

    Q_PROPERTY(bool catAutoConnect READ catAutoConnect WRITE setCatAutoConnect NOTIFY catAutoConnectChanged)
    Q_PROPERTY(bool audioAutoStart READ audioAutoStart WRITE setAudioAutoStart NOTIFY audioAutoStartChanged)

public:
    explicit DecodiumCat4OmManager(QObject* parent = nullptr);
    ~DecodiumCat4OmManager() override;

    bool connected() const { return m_connected; }
    bool connecting() const { return m_connecting; }
    QString rigName() const { return m_radioId; }
    void setRigName(QString const& value) { setRadioId(value); }

    QString managementEndpoint() const { return m_managementEndpoint; }
    void setManagementEndpoint(QString const& value);
    QString controlEndpoint() const { return m_controlEndpoint; }
    void setControlEndpoint(QString const& value);
    QString groupId() const { return m_groupId; }
    void setGroupId(QString const& value);
    QString radioId() const { return m_radioId; }
    void setRadioId(QString const& value);
    QString radioDisplayName() const { return m_radioDisplayName; }
    QStringList groupList() const { return m_groupList; }
    QStringList radioList() const { return m_radioList; }
    QString ownershipState() const { return m_ownershipState; }
    QString connectionDetail() const { return m_connectionDetail; }
    bool autoRequestOwnership() const { return m_autoRequestOwnership; }
    void setAutoRequestOwnership(bool value);
    QString protocolVersion() const { return QStringLiteral("1.0.0"); }
    QStringList availableCommands() const { return m_availableCommands; }
    QStringList supportedModes() const { return m_supportedModes; }

    QString serialPort() const { return {}; }
    void setSerialPort(QString const&) {}
    int baudRate() const { return 0; }
    void setBaudRate(int) {}
    QString dataBits() const { return QStringLiteral("Default"); }
    void setDataBits(QString const&) {}
    QString stopBits() const { return QStringLiteral("Default"); }
    void setStopBits(QString const&) {}
    QString handshake() const { return QStringLiteral("Default"); }
    void setHandshake(QString const&) {}
    bool forceDtr() const { return false; }
    void setForceDtr(bool) {}
    bool dtrHigh() const { return false; }
    void setDtrHigh(bool) {}
    bool forceRts() const { return false; }
    void setForceRts(bool) {}
    bool rtsHigh() const { return false; }
    void setRtsHigh(bool) {}
    QString networkPort() const { return m_controlEndpoint; }
    void setNetworkPort(QString const& value) { setControlEndpoint(value); }
    QString tciPort() const { return {}; }
    void setTciPort(QString const&) {}
    QString pttMethod() const { return QStringLiteral("CAT"); }
    void setPttMethod(QString const&) {}
    QString pttPort() const { return QStringLiteral("CAT"); }
    void setPttPort(QString const&) {}
    int civAddress() const { return 0; }
    QString splitMode() const { return m_splitMode; }
    void setSplitMode(QString const& value);
    bool catKeepAlive() const { return true; }
    void setCatKeepAlive(bool) {}
    int pollInterval() const { return 10; }
    void setPollInterval(int) {}
    QString portType() const { return QStringLiteral("cat4om"); }
    QStringList rigList() const { return m_radioList; }
    QStringList portList() const { return {}; }
    QStringList baudList() const { return {}; }
    QStringList pttMethodList() const { return {QStringLiteral("CAT")}; }
    QStringList splitModeList() const {
        return {QStringLiteral("none"), QStringLiteral("rig"), QStringLiteral("emulate")};
    }

    double frequency() const { return m_frequency; }
    double txFrequency() const { return m_txFrequency; }
    QString mode() const { return m_mode; }
    bool pttActive() const { return m_pttActive; }
    bool split() const { return m_split; }
    double powerWatts() const { return m_powerWatts; }
    double swr() const { return m_swr; }
    double alc() const { return m_alc; }
    bool alcValid() const { return m_alcValid; }

    bool catAutoConnect() const { return m_catAutoConnect; }
    void setCatAutoConnect(bool value);
    bool audioAutoStart() const { return m_audioAutoStart; }
    void setAudioAutoStart(bool value);

    bool canPtt() const;
    bool isMaster() const;

    Q_INVOKABLE void setRigFrequency(double hz);
    Q_INVOKABLE void setRigTxFrequency(double hz);
    Q_INVOKABLE void setRigTxFrequencyAndPtt(double hz, bool on);
    void setRigTxFrequencyAndPttAsync(double hz, bool on);
    Q_INVOKABLE void setRigMode(QString const& mode);
    Q_INVOKABLE void setRigPtt(bool on);
    Q_INVOKABLE void discover();
    Q_INVOKABLE void refreshPorts() { discover(); }
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

public slots:
    Q_INVOKABLE void connectRig();
    Q_INVOKABLE void disconnectRig();

signals:
    void connectedChanged();
    void connectingChanged();
    void rigNameChanged();
    void managementEndpointChanged();
    void controlEndpointChanged();
    void groupIdChanged();
    void radioIdChanged();
    void radioDisplayNameChanged();
    void groupListChanged();
    void radioListChanged();
    void ownershipStateChanged();
    void connectionDetailChanged();
    void autoRequestOwnershipChanged();
    void capabilitiesChanged();

    void serialPortChanged();
    void baudRateChanged();
    void dataBitsChanged();
    void stopBitsChanged();
    void handshakeChanged();
    void forceDtrChanged();
    void dtrHighChanged();
    void forceRtsChanged();
    void rtsHighChanged();
    void networkPortChanged();
    void tciPortChanged();
    void pttMethodChanged();
    void pttPortChanged();
    void civAddressChanged();
    void splitModeChanged();
    void catKeepAliveChanged();
    void pollIntervalChanged();
    void rigListChanged();
    void portListChanged();

    void frequencyChanged();
    void txFrequencyChanged();
    void modeChanged();
    void pttActiveChanged();
    void splitChanged();
    void powerWattsChanged();
    void swrChanged();
    void alcChanged();
    void errorOccurred(QString const& message);
    void statusUpdate(QString const& message);
    void catAutoConnectChanged();
    void audioAutoStartChanged();

private:
    struct PendingRequest {
        QString action;
        qint64 deadlineMs {0};
    };
    struct QueuedWrite {
        QString action;
        QString radioId;
        QJsonObject params;
    };

    void setupSockets();
    void openManagement();
    void openControl(bool directFallback);
    void closeSockets(bool abortImmediately = false);
    void resetSessionState();
    void scheduleReconnect(QString const& reason);
    void failConnection(QString const& message, bool allowDirectControlFallback);
    void setConnected(bool value);
    void setConnecting(bool value);
    void setOwnershipState(QString const& value);
    void setConnectionDetail(QString const& value);

    void sendHello(QWebSocket* socket);
    QString sendManagementRequest(QString const& action, QJsonObject const& params = {});
    QString sendControlRequestNow(QString const& action, QString const& radioId,
                                  QJsonObject const& params = {});
    void sendJson(QWebSocket* socket, QJsonObject const& object);
    void queueOrSendWrite(QString const& action, QJsonObject const& params);
    void requestOwnership();
    void flushQueuedWrites();
    void clearQueuedWrites(QString const& reason);
    void checkRequestTimeouts();

    void handleManagementMessage(QString const& text);
    void handleControlMessage(QString const& text);
    void handleServiceStatus(QJsonObject const& status);
    void handleControlWelcome(QJsonObject const& message);
    void handleRadioSnapshots(QJsonArray const& radios);
    void applySelectedRadioState();
    QString selectSupportedMode(QString const& requested) const;
    bool commandAvailable(QString const& action) const;
    QString controlTargetVfo(bool transmit) const;
    void queueFrequencyOnVfo(double hz, QString const& targetVfo);
    void clearFrequencySequencePending();

    static QUrl endpointUrl(QString const& endpoint, int defaultPort);
    static QString normalizedEndpoint(QString const& endpoint, int defaultPort);
    static QString errorText(QJsonObject const& response);

    QWebSocket* m_managementSocket {nullptr};
    QWebSocket* m_controlSocket {nullptr};
    QTimer* m_connectTimeout {nullptr};
    QTimer* m_reconnectTimer {nullptr};
    QTimer* m_requestTimer {nullptr};
    QTimer* m_safetyCloseTimer {nullptr};
    QElapsedTimer m_requestClock;

    QHash<QString, PendingRequest> m_managementRequests;
    QHash<QString, PendingRequest> m_controlRequests;
    QQueue<QueuedWrite> m_queuedWrites;
    QJsonArray m_lastRadios;

    QString m_managementEndpoint {QStringLiteral("127.0.0.1:5000")};
    QString m_controlEndpoint {QStringLiteral("127.0.0.1:5001")};
    QString m_groupId;
    QString m_radioId;
    QString m_radioDisplayName;
    QStringList m_groupList;
    QStringList m_radioList;
    QString m_managementClientId;
    QString m_controlClientId;
    QString m_masterId;
    QString m_role;
    QString m_ownershipState {QStringLiteral("disconnected")};
    QString m_connectionDetail {QStringLiteral("Disconnected")};
    QStringList m_availableCommands;
    QStringList m_supportedModes;
    QStringList m_availableVfos;
    QString m_activeVfo;
    QString m_txVfo;
    QString m_radioConnectionStatus;
    bool m_frequencySequencePending {false};
    double m_frequencySequenceTargetHz {0.0};
    QString m_frequencySequenceTargetVfo;
    QString m_frequencySequenceRestoreVfo;

    bool m_connected {false};
    bool m_connecting {false};
    bool m_controlReady {false};
    bool m_shouldReconnect {false};
    bool m_userDisconnect {false};
    bool m_directFallbackAttempted {false};
    bool m_managementComplete {false};
    bool m_ownershipRequestPending {false};
    bool m_autoRequestOwnership {true};
    bool m_closeAfterPttOff {false};
    bool m_catAutoConnect {false};
    bool m_audioAutoStart {false};
    int m_reconnectAttempt {0};

    QString m_splitMode {QStringLiteral("none")};
    double m_frequency {0.0};
    double m_txFrequency {0.0};
    QString m_mode;
    bool m_pttActive {false};
    bool m_split {false};
    double m_powerWatts {0.0};
    double m_swr {0.0};
    double m_alc {0.0};
    bool m_alcValid {false};
};
