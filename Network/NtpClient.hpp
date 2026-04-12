// -*- Mode: C++ -*-
#ifndef NTP_CLIENT_HPP__
#define NTP_CLIENT_HPP__

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QVector>
#include <QString>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class NtpClient : public QObject
{
  Q_OBJECT

public:
  explicit NtpClient(QObject *parent = nullptr);
  ~NtpClient() override;

  void setEnabled(bool enabled);
  void syncNow();
  void setCustomServer(QString const& server);
  void setRefreshInterval(int ms);
  void setMaxRtt(double ms) { m_maxRttMs = qBound(10.0, ms, 500.0); }
  double offsetMs() const { return m_offsetMs; }
  bool isSynced() const { return m_synced; }
  int lastServerCount() const { return m_lastServerCount; }
  void setInitialOffset(double offsetMs);

Q_SIGNALS:
  void offsetUpdated(double offsetMs);
  void syncStatusChanged(bool synced, QString const& statusText);
  void errorOccurred(QString const& errorMsg);

private Q_SLOTS:
  void onRefreshTimeout();
  void onQueryTimeout();
  void onReadyRead();
  void onDnsLookupResult(QHostInfo hostInfo);
  void onHttpReply(QNetworkReply *reply);

private:
  static constexpr quint64 NTP_EPOCH_OFFSET = 2208988800ULL;
  static constexpr int NTP_PACKET_SIZE = 48;
  static constexpr int NTP_PORT = 123;
  static constexpr int REFRESH_INTERVAL_MS = 300000;  // 5 minutes
  static constexpr int REFRESH_RETRY_MS = 30000;      // 30 seconds when not synced
  static constexpr int QUERY_TIMEOUT_MS = 7000;       // 7 seconds
  static constexpr double MAX_OFFSET_MS = 3600000.0;  // 1 hour sanity gate
  static constexpr int SERVERS_PER_QUERY = 8;          // max servers per sync cycle

  struct PendingQuery {
    qint64 t1Ms;  // client send time (ms since Unix epoch)
    quint32 t1Sec;
    quint32 t1Frac;
  };

  struct OffsetSample {
    double offsetMs {0.0};
    double rttMs {0.0};
  };

  void sendQuery(QHostAddress const& address);
  void httpTimeFallback();
  bool weakSyncShouldApply(double candidateOffsetMs, QString *reason);
  void resetWeakSyncState();
  void resetSparseJumpState();
  static double ntpTimestampToMs(quint32 seconds, quint32 fraction);
  static void msToNtpTimestamp(qint64 msEpoch, quint32 &seconds, quint32 &fraction);

  QUdpSocket m_socket;
  QTimer m_refreshTimer;
  QTimer m_queryTimeoutTimer;

  QStringList m_servers;
  QHash<QString, PendingQuery> m_pendingQueries;  // key = address.toString()
  QVector<OffsetSample> m_collectedOffsets;

  // HTTP time fallback
  QNetworkAccessManager *m_nam {nullptr};
  QVector<double> m_httpOffsets;
  QHash<QNetworkReply*, qint64> m_httpSendTimes;  // reply → send time
  int m_httpPendingCount {0};
  int m_ntpConsecutiveFailures {0};

  double m_offsetMs {0.0};
  bool m_synced {false};
  int m_lastServerCount {0};
  bool m_enabled {false};
  int m_pendingDnsLookups {0};
  QSet<int> m_activeLookupIds;
  QSet<QString> m_queriedAddresses;
  QString m_customServer;
  int m_refreshIntervalMs {REFRESH_INTERVAL_MS};
  double m_maxRttMs {250.0};  // RTT filter threshold (default 250ms)
  bool m_autoPinnedCustomServer {false};

  // Weak-sync guard (deadband + confirmation) to suppress jittery offset jumps.
  bool m_weakSyncEnabled {true};
  bool m_hasOffsetLock {false};
  double m_weakDeadbandMs {20.0};
  double m_weakStrongStepMs {350.0};
  double m_weakEmergencyStepMs {2000.0};
  double m_clusterWindowMs {180.0};
  double m_lockWindowMs {180.0};
  double m_httpLockToleranceMs {220.0};
  double m_sparseJumpMs {50.0};
  int m_weakConfirmNeeded {2};
  int m_weakStrongConfirmNeeded {4};
  int m_sparseJumpConfirmNeeded {4};
  int m_httpHoldoffMs {900000};
  int m_weakConfirmCount {0};
  int m_weakLastDirection {0};  // -1 / +1
  int m_sparseJumpConfirmCount {0};
  int m_sparseJumpDirection {0};
  int m_singleServerConfirmCount {0};
  bool m_singleServerHasCandidate {false};
  double m_singleServerLastCandidateMs {0.0};
  qint64 m_lastGoodSyncMs {0};
};

#endif // NTP_CLIENT_HPP__
