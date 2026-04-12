// -*- Mode: C++ -*-
#ifndef REMOTE_COMMAND_SERVER_HPP__
#define REMOTE_COMMAND_SERVER_HPP__

#include <QObject>
#include <QHostAddress>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QVector>
#include <QSet>
#include <QByteArray>

#include <functional>

class QWebSocket;
class QWebSocketServer;
class QTcpServer;
class QTcpSocket;

class RemoteCommandServer : public QObject
{
  Q_OBJECT

public:
  struct RuntimeState {
    QString mode;
    QString band;
    qint64 dialFrequencyHz {0};
    qint32 rxFrequencyHz {0};
    qint32 txFrequencyHz {0};
    qint64 periodMs {0};
    bool txEnabled {false};
    bool autoCqEnabled {false};
    bool autoSpotEnabled {false};
    bool asyncL2Enabled {false};
    bool dualCarrierEnabled {false};
    bool alt12Enabled {false};
    bool manualTxEnabled {false};
    bool speedyContestEnabled {false};
    bool digitalMorseEnabled {false};
    bool quickQsoEnabled {false};
    int ft2QsoMessageCount {5};
    qint32 asyncSnrDb {-99};
    QString uiLanguage;
    bool monitoring {false};
    bool transmitting {false};
    QString myCall;
    QString dxCall;
    qint64 nowUtcMs {0};
  };

  using RuntimeStateProvider = std::function<RuntimeState ()>;

  explicit RemoteCommandServer(QObject * parent = nullptr);
  ~RemoteCommandServer() override;

  bool start(quint16 wsPort, QHostAddress const& address = QHostAddress::LocalHost, quint16 httpPort = 0);
  void stop();
  bool isRunning() const;
  quint16 wsPort() const { return wsPort_; }
  quint16 httpPort() const { return httpPort_; }

  void setRuntimeStateProvider(RuntimeStateProvider provider);
  void setGuardPreMs(int ms);
  void setMaxCommandAgeMs(int ms);
  void setAuthUser(QString const& user);
  void setAuthToken(QString const& token);
  void publishBandActivityLine(QString const& line);
  void publishWaterfallRow(QByteArray const& rowLevels,
                           int startFrequencyHz,
                           int spanHz,
                           int rxFrequencyHz,
                           int txFrequencyHz,
                           QString const& mode);
  bool waterfallEnabled() const { return waterfallEnabled_; }

Q_SIGNALS:
  void selectCallerDue(QString const& commandId, QString const& call, QString const& grid);
  void setModeRequested(QString const& commandId, QString const& mode);
  void setBandRequested(QString const& commandId, QString const& band);
  void setRxFrequencyRequested(QString const& commandId, int rxFrequencyHz);
  void setTxFrequencyRequested(QString const& commandId, int txFrequencyHz);
  void setTxEnabledRequested(QString const& commandId, bool enabled);
  void setAutoCqRequested(QString const& commandId, bool enabled);
  void setAutoSpotRequested(QString const& commandId, bool enabled);
  void setMonitoringRequested(QString const& commandId, bool enabled);
  void setAsyncL2Requested(QString const& commandId, bool enabled);
  void setDualCarrierRequested(QString const& commandId, bool enabled);
  void setAlt12Requested(QString const& commandId, bool enabled);
  void setManualTxRequested(QString const& commandId, bool enabled);
  void setSpeedyContestRequested(QString const& commandId, bool enabled);
  void setDigitalMorseRequested(QString const& commandId, bool enabled);
  void setQuickQsoRequested(QString const& commandId, bool enabled);
  void setFt2QsoMessageCountRequested(QString const& commandId, int count);
  void waterfallStreamingChanged(bool enabled);
  void logMessage(QString const& message) const;

private Q_SLOTS:
  void onNewConnection();
  void onSocketDisconnected();
  void onTextMessageReceived(QString const& payload);
  void onScheduleTimeout();
  void onTelemetryTick();
  void onHttpNewConnection();
  void onHttpSocketReadyRead();
  void onHttpSocketDisconnected();

private:
  struct CommandResult {
    QJsonObject payload;
    bool accepted {false};
  };

  struct PendingCommand {
    QString commandId;
    QString targetCall;
    QString targetGrid;
    qint64 receivedUtcMs {0};
    qint64 scheduledUtcMs {0};
  };

  struct HttpConnectionState {
    QByteArray buffer;
    bool headersParsed {false};
    qint64 contentLength {0};
    QString method;
    QString path;
    QHash<QString, QString> headers;
  };

  RuntimeState runtimeState() const;
  qint64 currentUtcMs() const;
  qint64 nextSlotUtcMs(qint64 nowUtcMs, qint64 periodMs) const;
  void enqueueCommand(PendingCommand const& command);
  void armScheduleTimer();
  void pruneSeenIds(qint64 nowUtcMs);
  void removeClient(QWebSocket * client);

  void sendJson(QWebSocket * client, class QJsonObject const& object);
  void broadcastJson(class QJsonObject const& object);
  void sendReject(QWebSocket * client, QString const& commandId, QString const& status, QString const& reason);
  CommandResult processCommandObject(class QJsonObject const& object,
                                     QString const& tokenOverride = QString {},
                                     QString const& userOverride = QString {});
  class QJsonObject makeRejectPayload(QString const& commandId, QString const& status, QString const& reason) const;
  void sendHttpJson(QTcpSocket * socket, int statusCode, class QJsonObject const& object);
  void sendHttpNoContent(QTcpSocket * socket, int statusCode, QByteArray const& extraHeaders = QByteArray {});
  QString httpHeaderValue(HttpConnectionState const& state, QString const& key) const;
  QString httpAuthUser(HttpConnectionState const& state) const;
  QString httpBearerToken(HttpConnectionState const& state) const;
  bool authUserMatches(QString const& candidate) const;
  bool isHttpAuthorized(HttpConnectionState const& state) const;
  bool isAllowedWebSocketOrigin(QWebSocket const* client) const;
  bool isAuthRequired() const { return !authToken_.isEmpty(); }
  bool isClientAuthenticated(QWebSocket * client) const;
  void markClientAuthenticated(QWebSocket * client);
  void setWaterfallEnabled(bool enabled, QString const& commandId = QString {});
  void broadcastWaterfallState(QString const& commandId = QString {});

  QPointer<QWebSocketServer> server_;
  QPointer<QTcpServer> httpServer_;
  QList<QPointer<QWebSocket>> clients_;
  QSet<QWebSocket *> authenticatedClients_;
  QHash<QTcpSocket *, HttpConnectionState> httpStates_;
  QTimer scheduleTimer_;
  QTimer telemetryTimer_;
  QVector<PendingCommand> pending_;
  QHash<QString, qint64> seenCommandIds_;
  QStringList recentBandActivity_;
  int maxRecentBandActivity_ {200};

  RuntimeStateProvider runtimeProvider_;
  int guardPreMs_ {300};
  int maxCommandAgeMs_ {7500};
  bool waterfallEnabled_ {false};
  qint64 lastWaterfallRowUtcMs_ {0};
  int waterfallMinFrameIntervalMs_ {125};
  QByteArray lastWaterfallRowB64_;
  int lastWaterfallWidth_ {0};
  int lastWaterfallStartFrequencyHz_ {0};
  int lastWaterfallSpanHz_ {0};
  int lastWaterfallRxFrequencyHz_ {0};
  int lastWaterfallTxFrequencyHz_ {0};
  QString lastWaterfallMode_;
  QString authUser_ {QStringLiteral("admin")};
  QString authToken_;
  quint16 wsPort_ {0};
  quint16 httpPort_ {0};
  QHostAddress bindAddress_ {QHostAddress::LocalHost};
};

#endif // REMOTE_COMMAND_SERVER_HPP__
