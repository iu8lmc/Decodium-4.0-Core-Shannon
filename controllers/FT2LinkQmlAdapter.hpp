#ifndef DECODIUM_FT2LINK_QML_ADAPTER_HPP
#define DECODIUM_FT2LINK_QML_ADAPTER_HPP

#include "lib/ft2link/FT2LinkAppModel.hpp"
#include "lib/ft2link/FT2LinkAudio.hpp"
#include "lib/ft2link/FT2LinkSession.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <QByteArray>
#include <QEvent>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <deque>
#include <vector>

Q_DECLARE_METATYPE (decodium::ft2link::W2300DecodeMetrics)

// ============================================================================
// 1.0.458 iu8lmc — P0b worker-move (piano FT2-Link).
// FT2LinkDecodeWorker possiede TUTTO il decode RX live (narrow + W500/W2300):
// conversione PCM->float, misura energia, gating busy/throttle, buffer e DSP.
// CONTRATTO thread-boundary:
//   - worker (questo oggetto): m_narrowRx/m_w500Rx/m_w2300Rx, stato busy locale,
//     throttle. NON tocca MAI lo stato dell'adapter (sessioni/ARQ/mailbox/LBT).
//   - main (FT2LinkQmlAdapter): riceve via segnali SOLO byte di frame
//     serializzati + metriche W2300 + energia (rms/peak). ingestRadioFrameBytes,
//     observeLiveW2300Metrics e lo stato LBT restano sul main (Q_ASSERT armati).
// SYNC vs ASYNC: alla creazione il worker vive sul MAIN (segnali direct ->
// comportamento sincrono storico, usato dai test). startDecodeWorker() lo
// sposta su un QThread dedicato LowPriority (mai HighPriority su Windows) e le
// stesse connessioni Qt::AutoConnection diventano queued automaticamente.
// ============================================================================
class FT2LinkDecodeWorker : public QObject
{
  Q_OBJECT

public:
  explicit FT2LinkDecodeWorker (QObject* parent = nullptr);
  void setReplayProfileHint (decodium::ft2link::Profile profile);

  // Ritorna true se almeno un frame e' stato decodificato nel chunk (valore
  // osservabile solo in modalita' sincrona; in threaded il main riceve i frame
  // via segnali).
  bool processChunk (QVector<short> const& samples,
                     QString const& remoteCall,
                     quint64 nowMs,
                     bool wideSessionActive,
                     bool opportunisticWideDecode = false);

signals:
  void energyObserved (double rms, double peak, quint64 nowMs);
  void frameDecoded (QByteArray frameBytes, QString remoteCall, quint64 nowMs);
  void w2300FrameDecoded (QByteArray frameBytes,
                          decodium::ft2link::W2300DecodeMetrics metrics,
                          QString remoteCall,
                          quint64 nowMs);
  void resyncNeeded ();

private:
  bool stopRequested () const;
  bool busyNow (quint64 nowMs) const;
  void observeEnergy (std::vector<float> const& chunk, quint64 nowMs);
  void logDecodeFailure (quint64 nowMs,
                         bool wideSessionActive,
                         QString const& narrowError,
                         QString const& w2300Error,
                         QString const& w500Error);

  std::vector<float> m_narrowRx;
  decodium::ft2link::W500RxAudioBuffer m_w500Rx;
  decodium::ft2link::W2300RxAudioBuffer m_w2300Rx;
  bool m_hasReplayProfileHint {false};
  decodium::ft2link::Profile m_replayProfileHint {
    decodium::ft2link::Profile::Narrow};
  quint64 m_busyUntilMs {0};
  quint64 m_lastDecodeMs {0};
  quint64 m_lastWideDecodeMs {0};
  quint64 m_lastBusyLogMs {0};
  quint64 m_lastWorkerIngestLogMs {0};
  quint64 m_lastRxFailLogMs {0};
  quint64 m_lastNarrowTrimLogMs {0};
  quint64 m_lastNarrowScanLogMs {0};
  quint64 m_lastWideScanLogMs {0};
  bool m_narrowFailDumped {false};
  bool m_wasChannelBusy {false};
  double m_lastRms {0.0};
  double m_lastPeak {0.0};
};

class FT2LinkQmlAdapter : public QObject
{
  Q_OBJECT

  Q_PROPERTY(int stationCount READ stationCount NOTIFY stationCountChanged)
  Q_PROPERTY(int sessionCount READ sessionCount NOTIFY sessionCountChanged)
  Q_PROPERTY(quint16 activeSessionId READ activeSessionId NOTIFY activeSessionChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
  Q_PROPERTY(QString transportState READ transportState NOTIFY transportStateChanged)
  Q_PROPERTY(bool transportBusy READ transportBusy NOTIFY transportStateChanged)
  Q_PROPERTY(QVariantMap lastTransportMetrics READ lastTransportMetrics NOTIFY transportMetricsChanged)
  Q_PROPERTY(bool radioTxArmed READ radioTxArmed NOTIFY radioTxArmedChanged)
  Q_PROPERTY(QVariantMap lastRadioTxPlan READ lastRadioTxPlan NOTIFY radioTxPlanChanged)
  Q_PROPERTY(bool autoBeaconEnabled READ autoBeaconEnabled NOTIFY autoBeaconChanged)
  Q_PROPERTY(int autoBeaconIntervalSeconds READ autoBeaconIntervalSeconds NOTIFY autoBeaconChanged)
  Q_PROPERTY(bool autoBeaconCq READ autoBeaconCq NOTIFY autoBeaconChanged)
  Q_PROPERTY(bool liveChannelBusy READ liveChannelBusy NOTIFY liveChannelChanged)
  Q_PROPERTY(bool liveChannelLbtBusy READ liveChannelLbtBusy NOTIFY liveChannelChanged)
  Q_PROPERTY(double liveChannelRms READ liveChannelRms NOTIFY liveChannelChanged)
  Q_PROPERTY(double liveChannelPeak READ liveChannelPeak NOTIFY liveChannelChanged)
  Q_PROPERTY(int broadcastCount READ broadcastCount NOTIFY broadcastsChanged)
  Q_PROPERTY(int alertCount READ alertCount NOTIFY alertsChanged)
  Q_PROPERTY(int mailboxCount READ mailboxCount NOTIFY mailboxChanged)
  Q_PROPERTY(int mailboxUnreadCount READ mailboxUnreadCount NOTIFY mailboxChanged)
  Q_PROPERTY(int relayQueueCount READ relayQueueCount NOTIFY mailboxChanged)
  Q_PROPERTY(int formCount READ formCount NOTIFY formsChanged)
  Q_PROPERTY(int fileTransferCount READ fileTransferCount NOTIFY fileTransfersChanged)
  Q_PROPERTY(int bulletinCount READ bulletinCount NOTIFY bulletinsChanged)
  Q_PROPERTY(int bulletinUnreadCount READ bulletinUnreadCount NOTIFY bulletinsChanged)
  Q_PROPERTY(int qsoLogCount READ qsoLogCount NOTIFY qsoLogChanged)
  Q_PROPERTY(int logbookOutboxCount READ logbookOutboxCount NOTIFY logbookOutboxChanged)
  Q_PROPERTY(int contactCount READ contactCount NOTIFY contactHistoryChanged)
  Q_PROPERTY(int pingCount READ pingCount NOTIFY pingLogChanged)
  Q_PROPERTY(int pathReportCount READ pathReportCount NOTIFY pathReportsChanged)
  Q_PROPERTY(bool digipeaterEnabled READ digipeaterEnabled NOTIFY digipeaterChanged)
  Q_PROPERTY(int digipeaterMaxHops READ digipeaterMaxHops NOTIFY digipeaterChanged)
  Q_PROPERTY(int digipeaterEventCount READ digipeaterEventCount NOTIFY digipeaterChanged)
  Q_PROPERTY(bool bbsFileServerEnabled READ bbsFileServerEnabled NOTIFY bbsFileServerChanged)
  Q_PROPERTY(int bbsSharedFileCount READ bbsSharedFileCount NOTIFY bbsFileServerChanged)
  Q_PROPERTY(int beaconHistoryCount READ beaconHistoryCount NOTIFY beaconHistoryChanged)
  Q_PROPERTY(int clusterLastHeardCount READ clusterLastHeardCount NOTIFY clusterLastHeardChanged)
  Q_PROPERTY(int frequencyScheduleCount READ frequencyScheduleCount NOTIFY frequencyPlanChanged)
  Q_PROPERTY(QString localStorePath READ localStorePath NOTIFY localStoreChanged)
  Q_PROPERTY(bool localStoreLoaded READ localStoreLoaded NOTIFY localStoreChanged)
  Q_PROPERTY(QString lastLocalStoreError READ lastLocalStoreError NOTIFY localStoreChanged)
  Q_PROPERTY(bool awayEnabled READ awayEnabled NOTIFY presenceChanged)
  Q_PROPERTY(bool awayAcceptsQsy READ awayAcceptsQsy NOTIFY presenceChanged)
  Q_PROPERTY(QString awayMessage READ awayMessage NOTIFY presenceChanged)
  Q_PROPERTY(bool welcomeEnabled READ welcomeEnabled NOTIFY presenceChanged)
  Q_PROPERTY(QString welcomeMessage READ welcomeMessage NOTIFY presenceChanged)
  Q_PROPERTY(bool autoReplyEnabled READ autoReplyEnabled NOTIFY presenceChanged)
  Q_PROPERTY(bool autoAwayEnabled READ autoAwayEnabled NOTIFY presenceChanged)
  Q_PROPERTY(int autoAwayMinutes READ autoAwayMinutes NOTIFY presenceChanged)
  Q_PROPERTY(bool autoAwayActive READ autoAwayActive NOTIFY presenceChanged)
  Q_PROPERTY(int callIdIntervalMinutes READ callIdIntervalMinutes NOTIFY qsoAutomationChanged)
  Q_PROPERTY(int autoDisconnectMinutes READ autoDisconnectMinutes NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool incomingPingsEnabled READ incomingPingsEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool lastHeardPeekingEnabled READ lastHeardPeekingEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool lastConnectionsPeekingEnabled READ lastConnectionsPeekingEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool parkedVmailPeekingEnabled READ parkedVmailPeekingEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool vmailParkingEnabled READ vmailParkingEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool snrReportSendingEnabled READ snrReportSendingEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool verboseSnrAutoAcceptEnabled READ verboseSnrAutoAcceptEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(bool infoInquireEnabled READ infoInquireEnabled NOTIFY qsoAutomationChanged)
  Q_PROPERTY(int blockedCallCount READ blockedCallCount NOTIFY blockListChanged)
  Q_PROPERTY(int typingPeerCount READ typingPeerCount NOTIFY typingIndicatorsChanged)
  Q_PROPERTY(bool rfLabRecording READ rfLabRecording NOTIFY rfLabChanged)
  Q_PROPERTY(QString rfLabLastPath READ rfLabLastPath NOTIFY rfLabChanged)
  Q_PROPERTY(QVariantMap rfLabLastReport READ rfLabLastReport NOTIFY rfLabChanged)

public:
  explicit FT2LinkQmlAdapter (QObject* parent = nullptr);
  ~FT2LinkQmlAdapter () override;

  int stationCount () const;
  int sessionCount () const;
  quint16 activeSessionId () const;
  QString lastError () const;
  QString transportState () const;
  bool transportBusy () const;
  QVariantMap lastTransportMetrics () const;
  bool radioTxArmed () const;
  QVariantMap lastRadioTxPlan () const;
  bool autoBeaconEnabled () const;
  int autoBeaconIntervalSeconds () const;
  bool autoBeaconCq () const;
  bool liveChannelBusy () const;
  bool liveChannelLbtBusy () const;
  double liveChannelRms () const;
  double liveChannelPeak () const;
  int broadcastCount () const;
  int alertCount () const;
  int mailboxCount () const;
  int mailboxUnreadCount () const;
  int relayQueueCount () const;
  int formCount () const;
  int fileTransferCount () const;
  int bulletinCount () const;
  int bulletinUnreadCount () const;
  int qsoLogCount () const;
  int logbookOutboxCount () const;
  int contactCount () const;
  int pingCount () const;
  int pathReportCount () const;
  bool digipeaterEnabled () const;
  int digipeaterMaxHops () const;
  int digipeaterEventCount () const;
  bool bbsFileServerEnabled () const;
  int bbsSharedFileCount () const;
  int beaconHistoryCount () const;
  int clusterLastHeardCount () const;
  int frequencyScheduleCount () const;
  QString localStorePath () const;
  bool localStoreLoaded () const;
  QString lastLocalStoreError () const;
  bool awayEnabled () const;
  bool awayAcceptsQsy () const;
  QString awayMessage () const;
  bool welcomeEnabled () const;
  QString welcomeMessage () const;
  bool autoReplyEnabled () const;
  bool autoAwayEnabled () const;
  int autoAwayMinutes () const;
  bool autoAwayActive () const;
  int callIdIntervalMinutes () const;
  int autoDisconnectMinutes () const;
  bool incomingPingsEnabled () const;
  bool lastHeardPeekingEnabled () const;
  bool lastConnectionsPeekingEnabled () const;
  bool parkedVmailPeekingEnabled () const;
  bool vmailParkingEnabled () const;
  bool snrReportSendingEnabled () const;
  bool verboseSnrAutoAcceptEnabled () const;
  bool infoInquireEnabled () const;
  int blockedCallCount () const;
  int typingPeerCount () const;
  bool rfLabRecording () const;
  QString rfLabLastPath () const;
  QVariantMap rfLabLastReport () const;

  Q_INVOKABLE void setLocalStation (QString const& call,
                                    QString const& locator,
                                    QString const& name);
  Q_INVOKABLE void setLocalOperatorProfile (QString const& qth,
                                            QString const& email,
                                            QString const& ice,
                                            QString const& rig,
                                            QString const& antenna,
                                            QString const& power,
                                            QString const& gps);
  Q_INVOKABLE void setLocalCapabilities (bool supportsW500,
                                         bool supportsW2300,
                                         bool supportsW2300Fast,
                                         bool supportsW2300Robust,
                                         int preferredProfile,
                                         int preferredW2300RateMode);
  Q_INVOKABLE void setDeepRateEnabled (bool enabled);
  Q_INVOKABLE bool observeStation (QString const& call,
                                   QString const& locator,
                                   QString const& name,
                                   bool cq,
                                   bool supportsW500,
                                   bool supportsW2300,
                                   bool supportsW2300Fast,
                                   bool supportsW2300Robust,
                                   int preferredProfile,
                                   int preferredW2300RateMode,
                                   quint64 heardAtMs);
  Q_INVOKABLE QVariantList activeStations (quint64 nowMs,
                                           quint64 maxAgeMs,
                                           bool cqOnly = false) const;

  Q_INVOKABLE QByteArray startSessionHelloBytes (QString const& remoteCall,
                                                 quint64 nowMs);
  Q_INVOKABLE bool transmitBeaconRadio (bool cq, quint64 nowMs);
  Q_INVOKABLE bool transmitCqSlotRadio (int slotId,
                                        int slotSizeHz,
                                        quint64 nowMs);
  Q_INVOKABLE bool transmitSpecialCqRadio (QString const& cqType,
                                           QString const& cqLocator,
                                           int slotId,
                                           int slotSizeHz,
                                           quint64 nowMs);
  Q_INVOKABLE int beaconCooldownSeconds (quint64 nowMs) const;
  Q_INVOKABLE bool transmitBroadcastRadio (QString const& text, quint64 nowMs);
  Q_INVOKABLE bool transmitPathFinderRadio (QString const& targetCall,
                                            quint64 nowMs);
  Q_INVOKABLE bool transmitPathFinderResponseRadio (QString const& targetCall,
                                                    quint64 nowMs);
  Q_INVOKABLE QVariantMap pathFinderCandidate (QString const& targetCall,
                                               quint64 nowMs) const;
  Q_INVOKABLE QVariantMap digipeaterState (quint64 nowMs) const;
  Q_INVOKABLE QVariantList digipeaterEvents () const;
  Q_INVOKABLE QVariantMap configureDigipeater (bool enabled,
                                               int maxHops);
  Q_INVOKABLE bool clearDigipeaterEvents ();
  Q_INVOKABLE QString digipeaterEnvelopeText (QString const& targetCall,
                                              QString const& payloadText,
                                              int maxHops,
                                              quint64 nowMs) const;
  Q_INVOKABLE bool transmitDigipeaterRadio (QString const& targetCall,
                                            QString const& payloadText,
                                            int maxHops,
                                            quint64 nowMs);
  Q_INVOKABLE QVariantMap bbsFileServerState (quint64 nowMs) const;
  Q_INVOKABLE QVariantList bbsSharedFiles () const;
  Q_INVOKABLE QVariantMap configureBbsFileServer (bool enabled);
  Q_INVOKABLE QVariantMap publishBbsSharedFileText (QString const& fileName,
                                                    QString const& content,
                                                    quint64 nowMs);
  Q_INVOKABLE QVariantMap publishBbsSharedFileBytes (QString const& fileName,
                                                     QString const& contentBase64,
                                                     quint64 nowMs);
  Q_INVOKABLE bool removeBbsSharedFile (quint32 fileId);
  Q_INVOKABLE bool clearBbsSharedFiles ();
  Q_INVOKABLE bool requestBbsFileListRadio (quint16 sessionId,
                                            quint64 nowMs);
  Q_INVOKABLE bool requestBbsFileRadio (quint16 sessionId,
                                        QString const& fileName,
                                        quint64 nowMs);
  Q_INVOKABLE bool transmitBbsSharedFileListRadio (quint16 sessionId,
                                                   quint64 nowMs);
  Q_INVOKABLE bool transmitBbsSharedFileRadio (quint16 sessionId,
                                               QString const& fileName,
                                               quint64 nowMs);
  Q_INVOKABLE bool transmitPingRadio (QString const& remoteCall, quint64 nowMs);
  Q_INVOKABLE bool configureAutoBeacon (bool enabled,
                                        int intervalSeconds,
                                        bool cq,
                                        quint64 nowMs);
  Q_INVOKABLE bool startSessionRadioHandshake (QString const& remoteCall,
                                               quint64 nowMs);
  Q_INVOKABLE bool receiveHelloAckBytes (QByteArray const& helloAckBytes,
                                         quint64 nowMs);
  Q_INVOKABLE QByteArray answerHelloBytes (QString const& remoteCall,
                                           QByteArray const& helloBytes,
                                           quint64 nowMs);

  Q_INVOKABLE bool queueOutgoingText (quint16 sessionId,
                                      QString const& text,
                                      quint64 nowMs);
  Q_INVOKABLE bool transmitTextLocalAudio (quint16 sessionId,
                                           QString const& text,
                                           quint64 nowMs,
                                           bool modelAckAudio = true,
                                           bool dropFirstDataBurst = false,
                                           bool dropFirstAckBurst = false);
  Q_INVOKABLE bool transmitMailboxRadio (quint16 sessionId,
                                         QString const& toCall,
                                         QString const& subject,
                                         QString const& body,
                                         quint64 nowMs);
  Q_INVOKABLE bool transmitMailboxRadioTyped (quint16 sessionId,
                                              QString const& toCall,
                                              QString const& subject,
                                              QString const& body,
                                              bool urgent,
                                              bool emcomm,
                                              quint64 nowMs);
  Q_INVOKABLE bool parkMailbox (QString const& toCall,
                                QString const& subject,
                                QString const& body,
                                quint64 nowMs);
  Q_INVOKABLE bool parkMailboxTyped (QString const& toCall,
                                     QString const& subject,
                                     QString const& body,
                                     bool urgent,
                                     bool emcomm,
                                     quint64 nowMs);
  Q_INVOKABLE QVariantMap relayMailboxForSession (quint16 sessionId) const;
  Q_INVOKABLE bool transmitRelayMailboxRadio (quint16 sessionId,
                                              quint32 mailboxId,
                                              quint64 nowMs);
  Q_INVOKABLE bool transmitFormRadio (quint16 sessionId,
                                      QString const& toCall,
                                      QString const& formType,
                                      QVariantMap const& fields,
                                      quint64 nowMs);
  Q_INVOKABLE bool transmitFileRadio (quint16 sessionId,
                                      QString const& toCall,
                                      QString const& fileName,
                                      QString const& content,
                                      quint64 nowMs);
  Q_INVOKABLE bool transmitFileRadioBytes (quint16 sessionId,
                                           QString const& toCall,
                                           QString const& fileName,
                                           QString const& contentBase64,
                                           quint64 nowMs);
  Q_INVOKABLE bool applicationRadioTxReady (quint16 sessionId) const;
  Q_INVOKABLE bool transmitBulletinRadio (quint16 sessionId,
                                          QString const& group,
                                          QString const& title,
                                          QString const& body,
                                          quint64 nowMs);
  Q_INVOKABLE void armStrictListenBeforeTransmit (int cancelAfterMs = 24000);
  Q_INVOKABLE void setRadioTxArmed (bool armed);
  Q_INVOKABLE bool prepareRadioTxAudio (quint16 sessionId,
                                        QString const& text,
                                        quint64 nowMs);
  Q_INVOKABLE bool transmitPreparedRadioTxAudio (quint16 sessionId,
                                                 QString const& text,
                                                 quint64 nowMs);
  Q_INVOKABLE bool ingestRxSamples (QVector<short> const& samples,
                                    QString const& remoteCall,
                                    quint64 nowMs);
  Q_INVOKABLE QVariantMap startRfLabRecording (QString const& path = QString {});
  Q_INVOKABLE QVariantMap stopRfLabRecording ();
  Q_INVOKABLE QVariantMap replayRfLabWav (
      QString const& path,
      QVariantMap const& options = QVariantMap {});
  Q_INVOKABLE QVariantMap generateRfLabWav (
      QString const& path,
      QString const& profileName,
      QString const& text,
      QVariantMap const& options = QVariantMap {});
  Q_INVOKABLE QVariantMap runRfLabSelfTest (
      QString const& directory = QString {},
      QVariantMap const& options = QVariantMap {});
  Q_INVOKABLE QVariantMap runRfLabChannelSweep (
      QString const& directory = QString {},
      QVariantMap const& options = QVariantMap {});
  // P0b worker-move: sposta il decode live su un QThread dedicato (LowPriority).
  // Idempotente. L'app lo chiama all'avvio (main_qml); i test NON lo chiamano
  // e restano sul path sincrono storico (worker sul main, segnali direct).
  Q_INVOKABLE void startDecodeWorker ();
  void stopDecodeWorker ();
  // P0b TX closed-loop: fine REALE della TX radio (da DecodiumBridge::
  // ft2LinkTxFinished). Rilascia il busy della coda TX e logga il drift
  // stima-vs-reale; la stima audioSeconds+250ms resta come fallback.
  Q_INVOKABLE void notifyRadioTxFinished ();
  Q_INVOKABLE bool ingestRadioFrameBytes (QByteArray const& frameBytes,
                                          QString const& remoteCall,
                                          quint64 nowMs,
                                          bool autoAck = true);
  Q_INVOKABLE bool appendIncomingText (quint16 sessionId,
                                       QString const& text,
                                       quint64 nowMs);
  Q_INVOKABLE bool closeSession (quint16 sessionId, quint64 nowMs);
  Q_INVOKABLE QVariantList cannedMessages () const;
  Q_INVOKABLE QVariantList customCannedMessages () const;
  Q_INVOKABLE QVariantMap addOrUpdateCannedMessage (QString const& label,
                                                   QString const& templateText,
                                                   QString const& tip);
  Q_INVOKABLE QVariantMap deleteCannedMessage (QString const& label);
  Q_INVOKABLE QVariantMap resetCannedMessages ();
  Q_INVOKABLE QString checkInMessage (QString const& city,
                                      QString const& region,
                                      QString const& channel,
                                      QString const& weather,
                                      quint64 nowMs) const;
  Q_INVOKABLE QString expandCannedMessage (QString const& templateText,
                                           quint16 sessionId,
                                           quint64 nowMs) const;
  Q_INVOKABLE QVariantList qsySlots (int slotSizeHz,
                                     int slotsEachSide) const;
  Q_INVOKABLE QString qsyTagForOffset (int offsetHz) const;
  Q_INVOKABLE QString qsyFrequencyTag (qint64 dialFrequencyHz) const;
  Q_INVOKABLE QString qsyBroadcastText (qint64 dialFrequencyHz,
                                        QString const& label,
                                        QString const& reason) const;
  Q_INVOKABLE bool transmitQsyBroadcastRadio (qint64 dialFrequencyHz,
                                              QString const& label,
                                              QString const& reason,
                                              quint64 nowMs);
  Q_INVOKABLE QVariantMap qsyPlanForText (QString const& text,
                                          qint64 currentDialFrequencyHz) const;
  Q_INVOKABLE QVariantList frequencyPresets () const;
  Q_INVOKABLE QString frequencyPresetsText () const;
  Q_INVOKABLE QVariantMap setFrequencyPresets (QString const& text);
  Q_INVOKABLE QVariantMap resetFrequencyPresets ();
  Q_INVOKABLE QVariantList allowedQsyRanges () const;
  Q_INVOKABLE QString allowedQsyRangesText () const;
  Q_INVOKABLE QVariantMap setAllowedQsyRanges (QString const& text);
  Q_INVOKABLE QVariantMap resetAllowedQsyRanges ();
  Q_INVOKABLE QVariantList frequencySchedule () const;
  Q_INVOKABLE QString frequencyScheduleText () const;
  Q_INVOKABLE QVariantMap setFrequencySchedule (QString const& text);
  Q_INVOKABLE QVariantMap resetFrequencySchedule ();
  Q_INVOKABLE QVariantMap activeFrequencySchedule (quint64 nowMs) const;
  Q_INVOKABLE bool qsyFrequencyAllowed (qint64 dialFrequencyHz) const;
  Q_INVOKABLE QVariantMap callingFrequencyGuard (QString const& action,
                                                 qint64 dialFrequencyHz,
                                                 qint64 callingFrequencyHz) const;
  Q_INVOKABLE QVariantMap callingFrequencyGuardAt (QString const& action,
                                                   qint64 dialFrequencyHz,
                                                   qint64 callingFrequencyHz,
                                                   quint64 nowMs) const;
  Q_INVOKABLE QVariantMap presence () const;
  Q_INVOKABLE QVariantMap configurePresence (bool awayEnabled,
                                             bool awayAcceptsQsy,
                                             QString const& awayMessage,
                                             bool welcomeEnabled,
                                             QString const& welcomeMessage);
  Q_INVOKABLE QVariantMap configureAutoReply (bool enabled);
  Q_INVOKABLE QVariantMap configureAutoAway (bool enabled,
                                             int minutes,
                                             quint64 nowMs);
  Q_INVOKABLE QVariantMap noteOperatorActivity (quint64 nowMs);
  Q_INVOKABLE QVariantMap evaluateAutoAway (quint64 nowMs);
  Q_INVOKABLE QVariantMap qsoAutomation () const;
  Q_INVOKABLE QVariantMap privacyProfile () const;
  Q_INVOKABLE QVariantMap privacyPanel (quint64 nowMs) const;
  Q_INVOKABLE QVariantMap inquiryPreview (QString const& remoteCall,
                                          quint64 nowMs) const;
  Q_INVOKABLE QVariantMap applyPrivacyPreset (QString const& preset);
  Q_INVOKABLE QVariantMap configureInquiryPrivacy (
      bool incomingPings,
      bool lastHeardPeeking,
      bool lastConnectionsPeeking,
      bool parkedVmailPeeking,
      bool vmailParking,
      bool snrReportSending,
      bool verboseSnrAutoAccept,
      bool infoInquire,
      bool autoReply,
      bool welcome,
      quint64 nowMs);
  Q_INVOKABLE QVariantMap configureQsoAutomation (
      int callIdIntervalMinutes,
      int autoDisconnectMinutes);
  Q_INVOKABLE QVariantMap configureIncomingPings (bool enabled);
  Q_INVOKABLE QVariantMap configureLastHeardPeeking (bool enabled);
  Q_INVOKABLE QVariantMap configureLastConnectionsPeeking (bool enabled);
  Q_INVOKABLE QVariantMap configureParkedVmailPeeking (bool enabled);
  Q_INVOKABLE QVariantMap configureVmailParking (bool enabled);
  Q_INVOKABLE QVariantMap configureSnrReportSending (bool enabled);
  Q_INVOKABLE QVariantMap configureVerboseSnrAutoAccept (bool enabled);
  Q_INVOKABLE QVariantMap configureInfoInquire (bool enabled);
  Q_INVOKABLE QVariantMap evaluateQsoAutomation (quint64 nowMs);
  Q_INVOKABLE QStringList blockedCalls () const;
  Q_INVOKABLE QString blockedCallsText () const;
  Q_INVOKABLE QVariantMap setBlockedCalls (QString const& callsText);
  Q_INVOKABLE QVariantMap addBlockedCall (QString const& call);
  Q_INVOKABLE QVariantMap deleteBlockedCall (QString const& call);
  Q_INVOKABLE QVariantMap clearBlockedCalls ();
  Q_INVOKABLE bool isCallBlocked (QString const& call) const;
  Q_INVOKABLE QVariantList broadcasts () const;
  Q_INVOKABLE QVariantList alertEvents () const;
  Q_INVOKABLE QVariantList activeAlertEvents () const;
  Q_INVOKABLE QStringList alertTags () const;
  Q_INVOKABLE QStringList customAlertTags () const;
  Q_INVOKABLE QVariantMap setCustomAlertTags (QString const& tagsText);
  Q_INVOKABLE QVariantMap clearCustomAlertTags ();
  Q_INVOKABLE bool setContactTag (QString const& call,
                                  QString const& tag,
                                  quint64 nowMs);
  Q_INVOKABLE bool setContactDetails (QString const& call,
                                      QString const& locator,
                                      QString const& name,
                                      QString const& tag,
                                      QString const& comment,
                                      quint64 nowMs);
  Q_INVOKABLE QVariantList mailbox () const;
  Q_INVOKABLE QVariantList relayQueue (quint64 nowMs) const;
  Q_INVOKABLE QVariantList formTemplates () const;
  Q_INVOKABLE QVariantList forms () const;
  Q_INVOKABLE QVariantList fileTransfers () const;
  Q_INVOKABLE QVariantList receivedFiles () const;
  Q_INVOKABLE QVariantList bulletins () const;
  Q_INVOKABLE QVariantList qsoLog () const;
  Q_INVOKABLE QVariantList logbookOutbox () const;
  Q_INVOKABLE QVariantList contactHistory () const;
  Q_INVOKABLE QVariantList contactTimeline (QString const& call) const;
  Q_INVOKABLE QString qslCard (quint16 sessionId) const;
  Q_INVOKABLE QString adifRecord (quint16 sessionId) const;
  Q_INVOKABLE QString adifLog () const;
  Q_INVOKABLE QString adifLogPath () const;
  Q_INVOKABLE QVariantMap writeAdifLogFile (QString const& path = QString {}) const;
  Q_INVOKABLE QVariantMap queueLogbookUpload (quint16 sessionId,
                                              QString const& target,
                                              quint64 nowMs);
  Q_INVOKABLE QVariantMap queueAllLogbookUploads (QString const& target,
                                                  quint64 nowMs);
  Q_INVOKABLE QVariantMap markLogbookUpload (quint32 uploadId,
                                             QString const& state,
                                             QString const& detail,
                                             quint64 nowMs);
  Q_INVOKABLE QVariantMap logbookUploadPayload (quint32 uploadId) const;
  Q_INVOKABLE QString logbookOutboxText () const;
  Q_INVOKABLE QString chatHistoryLog () const;
  Q_INVOKABLE QString mailboxText () const;
  Q_INVOKABLE QString relayQueueText (quint64 nowMs) const;
  Q_INVOKABLE QVariantMap mailboxCenter (quint64 nowMs) const;
  Q_INVOKABLE bool markMailboxEmailGateway (quint32 messageId,
                                            QString const& state,
                                            QString const& detail,
                                            quint64 nowMs);
  Q_INVOKABLE QVariantMap mailboxEmailGateway (quint32 messageId,
                                               QString const& fallbackToEmail) const;
  Q_INVOKABLE QString mailboxEmailGatewayText (quint32 messageId,
                                               QString const& fallbackToEmail) const;
  Q_INVOKABLE QString operationalLog () const;
  Q_INVOKABLE QString localStoreJson () const;
  Q_INVOKABLE QString logsBundleText () const;
  Q_INVOKABLE QVariantList pingLog () const;
  Q_INVOKABLE QVariantList pathReports () const;
  Q_INVOKABLE QVariantList beaconHistory () const;
  Q_INVOKABLE QString beaconHistoryText () const;
  Q_INVOKABLE QVariantMap clusterConfig () const;
  Q_INVOKABLE QVariantMap configureCluster (bool enabled,
                                            QString const& nodeId,
                                            QString const& band,
                                            qint64 dialFrequencyHz);
  Q_INVOKABLE QVariantList clusterLastHeard () const;
  Q_INVOKABLE QString clusterLastHeardText () const;
  Q_INVOKABLE QString clusterExportJson () const;
  Q_INVOKABLE QVariantMap importClusterLastHeard (QString const& jsonText,
                                                  quint64 nowMs);
  Q_INVOKABLE QVariantMap writeClusterShareFile (QString const& path = QString {}) const;
  Q_INVOKABLE QVariantMap mergeClusterShareFile (QString const& path = QString {},
                                                 quint64 nowMs = 0);
  Q_INVOKABLE QVariantMap syncClusterShareFile (QString const& path = QString {},
                                                quint64 nowMs = 0);
  Q_INVOKABLE QVariantMap parkedMailboxSummaryForCall (QString const& call,
                                                       quint64 nowMs) const;
  Q_INVOKABLE QVariantMap pathRelayCandidate (QString const& targetCall,
                                              quint64 nowMs) const;
  Q_INVOKABLE QVariantMap relayWorkflowForStation (QString const& relayCall,
                                                   quint64 nowMs) const;
  Q_INVOKABLE QVariantMap pathAnalysis (QString const& call,
                                        QString const& locator) const;
  Q_INVOKABLE QVariantMap statistics () const;
  Q_INVOKABLE QString statisticsText () const;
  Q_INVOKABLE QVariantMap localStoreAudit () const;
  Q_INVOKABLE QVariantMap backupLocalStore (QString const& directory = QString {});
  Q_INVOKABLE QVariantMap fixLocalStore (bool makeBackup);
  Q_INVOKABLE void clearBroadcasts ();
  Q_INVOKABLE void clearAlertEvents ();
  Q_INVOKABLE bool markAlertRead (quint32 alertId,
                                  bool read,
                                  quint64 nowMs);
  Q_INVOKABLE bool archiveAlert (quint32 alertId,
                                 bool archived,
                                 quint64 nowMs);
  Q_INVOKABLE void clearArchivedAlertEvents ();
  Q_INVOKABLE bool markMailboxRead (quint32 messageId,
                                    bool read,
                                    quint64 nowMs);
  Q_INVOKABLE bool markMailboxRelayReady (quint32 messageId,
                                          quint64 nowMs);
  Q_INVOKABLE bool markMailboxPendingRelay (quint32 messageId,
                                            QString const& relayCall,
                                            quint64 nowMs);
  Q_INVOKABLE bool cancelMailboxRelay (quint32 messageId,
                                       quint64 nowMs);
  Q_INVOKABLE bool markReceivedFileRead (quint32 transferId,
                                         bool read,
                                         quint64 nowMs);
  Q_INVOKABLE bool deleteReceivedFile (quint32 transferId);
  Q_INVOKABLE int clearReceivedFiles (bool readOnly);
  Q_INVOKABLE bool markBulletinRead (quint32 bulletinId,
                                     bool read,
                                     quint64 nowMs);
  Q_INVOKABLE bool deleteMailboxMessage (quint32 messageId);
  Q_INVOKABLE void clearMailbox ();
  Q_INVOKABLE void clearForms ();
  Q_INVOKABLE void clearFileTransfers ();
  Q_INVOKABLE void clearBulletins ();
  Q_INVOKABLE void clearQsoLog ();
  Q_INVOKABLE void clearLogbookOutbox ();
  Q_INVOKABLE void clearContactHistory ();
  Q_INVOKABLE void clearPingLog ();
  Q_INVOKABLE void clearPathReports ();
  Q_INVOKABLE void clearBeaconHistory ();
  Q_INVOKABLE void clearClusterLastHeard ();
  Q_INVOKABLE void setLocalStorePath (QString const& path);
  Q_INVOKABLE bool loadLocalStore (QString const& path = QString {});
  Q_INVOKABLE bool saveLocalStore (QString const& path = QString {});

  Q_INVOKABLE QVariantMap sessionInfo (quint16 sessionId) const;
  Q_INVOKABLE QVariantList sessions () const;
  Q_INVOKABLE QVariantList messages (quint16 sessionId) const;
  Q_INVOKABLE QVariantList chatLog (quint16 sessionId) const;
  Q_INVOKABLE QVariantList typingIndicators (quint64 nowMs);
  Q_INVOKABLE QString typingSummary (quint64 nowMs);

  struct FrequencyPreset
  {
    qint64 dialFrequencyHz {0};
    QString band;
    QString label;
  };

  struct AllowedQsyRange
  {
    qint64 fromHz {0};
    qint64 toHz {0};
    QString label;
  };

  struct FrequencyScheduleEntry
  {
    int startMinute {0};
    int endMinute {0};
    QString action;
    qint64 dialFrequencyHz {0};
    QString label;
    QString cqType;
  };

signals:
  void stationCountChanged ();
  void sessionCountChanged ();
  void activeSessionChanged ();
  void sessionsChanged ();
  void messagesChanged (quint16 sessionId);
  void lastErrorChanged ();
  void transportStateChanged ();
  void transportMetricsChanged ();
  void radioTxArmedChanged ();
  void radioTxPlanChanged ();
  void autoBeaconChanged ();
  void liveChannelChanged ();
  void broadcastsChanged ();
  void alertsChanged ();
  void mailboxChanged ();
  void formsChanged ();
  void fileTransfersChanged ();
  void bulletinsChanged ();
  void qsoLogChanged ();
  void logbookOutboxChanged ();
  void contactHistoryChanged ();
  void pingLogChanged ();
  void pathReportsChanged ();
  void digipeaterChanged ();
  void bbsFileServerChanged ();
  void beaconHistoryChanged ();
  void clusterLastHeardChanged ();
  void localStoreChanged ();
  void cannedMessagesChanged ();
  void alertTagsChanged ();
  void frequencyPlanChanged ();
  void presenceChanged ();
  void qsoAutomationChanged ();
  void blockListChanged ();
  void typingIndicatorsChanged ();
  void rfLabChanged ();
  void radioTxAudioRequested (QString displayMessage,
                              QVector<float> samples,
                              QVariantMap plan);

protected:
  bool eventFilter (QObject* watched, QEvent* event) override;

private:
  void setLastError (QString const& error);
  void clearLastError ();
  void setTransportState (QString const& state);
  void setTransportBusy (bool busy);
  QString defaultLocalStorePath () const;
  QString resolvedLocalStorePath (QString const& path = QString {}) const;
  QString defaultAdifLogPath () const;
  QString resolvedAdifLogPath (QString const& path = QString {}) const;
  QByteArray serializeLocalStore () const;
  bool applyLocalStoreBytes (QByteArray const& bytes, QString* error);
  void setLocalStoreState (QString const& path,
                           bool loaded,
                           QString const& error);
  void persistLocalStore ();
  int knownStationCount () const;
  bool requestAckRadioTx (decodium::ft2link::Frame const& ack,
                          decodium::ft2link::AppSession const& session,
                          quint64 nowMs);
  void scheduleInboundAck (quint16 sessionId);
  void runPendingInboundAcks ();
  void schedulePendingInboundAckCheck (quint64 nowMs);
  bool queueLiveOutboundWindowAfterAck (quint16 sessionId,
                                        quint64 nowMs);
  bool queueBeaconRadio (bool cq,
                         quint64 nowMs,
                         bool requireArm,
                         bool automatic,
                         int cqSlotId = 0,
                         int cqSlotSizeHz = 750,
                         QString const& cqType = QString {},
                         QString const& cqLocator = QString {});
  bool requestControlRadioTx (decodium::ft2link::Frame const& frame,
                              QString const& kind,
                              QString const& remoteCall,
                              quint64 nowMs);
  bool hasPendingSessionRadioTraffic () const;
  bool shouldDeferBroadcastTx (QVariantMap const& plan,
                               quint16 sessionId) const;
  void enqueueRadioTx (QString const& displayMessage,
                       QVector<float> const& samples,
                       QVariantMap const& plan,
                       quint64 nowMs,
                       bool priority,
                       quint16 sessionId = 0,
                       bool cancelIfNoOutbound = false);
  void purgeQueuedHelloRetries (quint16 sessionId);
  void purgeQueuedLiveOutboundRetries (quint16 sessionId);
  void drainRadioTxQueue (quint64 nowMs);
  void scheduleRadioQueueDrain (quint64 nowMs);
  void scheduleLiveOutboundRetry (quint16 sessionId,
                                  QString const& displayMessage,
                                  QVector<float> const& samples,
                                  QVariantMap const& plan,
                                  std::vector<std::uint8_t> const& payload,
                                  decodium::ft2link::Profile profile,
                                  std::size_t messageIndex,
                                  quint64 nowMs);
  void runLiveOutboundRetryCheck ();
  void scheduleLiveOutboundRetryCheck (quint64 nowMs);
  void scheduleHelloRetry (decodium::ft2link::Frame const& hello,
                           QString const& remoteCall,
                           quint64 nowMs);
  void runHelloRetryCheck ();
  void scheduleHelloRetryCheck (quint64 nowMs);
  void runAutoBeaconTick ();
  void scheduleAutoBeacon (quint64 nowMs);
  decodium::ft2link::W2300RateMode currentLiveW2300RateMode (
      decodium::ft2link::AppSession const& session) const;
  decodium::ft2link::W2300RateMode effectiveW2300RateMode (
      decodium::ft2link::W2300RateMode mode) const;
  void observeLiveW2300Metrics (
      decodium::ft2link::Frame const& frame,
      decodium::ft2link::W2300DecodeMetrics const& metrics,
      quint64 nowMs);
  bool transmitApplicationPayloadRadio (quint16 sessionId,
                                        QString const& payloadText,
                                        QString const& logText,
                                        QString const& displayMessage,
                                        QString const& state,
                                        QVariantMap const& planExtras,
                                        quint64 nowMs);
  bool isLiveChannelBusy (quint64 nowMs) const;
  bool isLiveChannelLbtBusy (quint64 nowMs) const;
  // P0b: il calcolo dell'energia avviene sul worker; qui si applica il
  // risultato allo stato LBT (main). Sostituisce observeRxEnergy(samples).
  void applyObservedRxEnergy (double rms, double peak, quint64 nowMs);
  void onWorkerFrameDecoded (QByteArray const& frameBytes,
                             QString const& remoteCall,
                             quint64 nowMs);
  void onWorkerW2300FrameDecoded (QByteArray const& frameBytes,
                                  decodium::ft2link::W2300DecodeMetrics const& metrics,
                                  QString const& remoteCall,
                                  quint64 nowMs);
  QString defaultRfLabDirectory () const;
  QString resolvedRfLabPath (QString const& path,
                             QString const& defaultFileName) const;
  void setRfLabReport (QVariantMap const& report, QString const& path = QString {});

  // Harness di misura P0a (opt-in via env DECODIUM_FT2LINK_TIMING): registra il
  // costo main-thread di ingestRxSamples e logga p50/p99/max ogni 200 chiamate su
  // [Ft2Link]. Baseline per giustificare/validare il worker-move (P0b).
  void recordIngestTiming (std::chrono::steady_clock::time_point start);
  bool m_ingestTimingEnabled {false};
  std::vector<qint64> m_ingestDurationsUs;

  // P0b worker-move: buffer RX live, throttle e stato busy di decodifica sono
  // MIGRATI in FT2LinkDecodeWorker (unico proprietario). Vedi il contratto
  // thread-boundary sopra la classe worker.
  FT2LinkDecodeWorker* m_decodeWorker {nullptr};
  QThread m_decodeThread;
  bool m_decodeStopping {false};
  // P0b watchdog: rileva coda TX radio bloccata (item in testa piu' vecchio di
  // kRadioQueueStuckMs) e la sblocca forzando il drain. Rete di sicurezza
  // contro bug futuri di scheduling (LBT/busy), non un percorso normale.
  QTimer m_radioQueueWatchdog;
  QStringList detectAlertTags (QString const& text) const;
  void recordBroadcast (QString const& fromCall,
                        QString const& text,
                        quint64 nowMs,
                        QString const& source);
  void recordPathFinderAlert (QString const& fromCall,
                              QString const& text,
                              quint64 nowMs);
  bool handlePathFinderBroadcast (QString const& fromCall,
                                  QString const& text,
                                  quint64 nowMs);
  bool handleDigipeaterBroadcast (QString const& fromCall,
                                  QString const& text,
                                  quint64 nowMs);
  bool transmitDigipeaterEnvelopeRadio (QString const& envelopeText,
                                        quint64 nowMs,
                                        bool requireArm);
  void recordDigipeaterEvent (QString const& direction,
                              QString const& originCall,
                              QString const& targetCall,
                              QString const& viaCall,
                              QStringList const& path,
                              QString const& payloadText,
                              QString const& state,
                              int ttl,
                              QString const& detail,
                              quint64 nowMs);
  void pruneDigipeaterSeen (quint64 nowMs);
  quint32 recordMailbox (QString const& direction,
                         QString const& fromCall,
                         QString const& toCall,
                         QString const& subject,
                         QString const& body,
                         QString const& state,
                         quint64 nowMs,
                         bool urgent = false,
                         bool emcomm = false,
                         QString const& relayViaCall = QString {},
                         int relayHopCount = 0,
                         QString const& relayProtocol = QString {});
  bool updateMailboxState (quint32 messageId,
                           QString const& state,
                           quint64 nowMs);
  void notifyParkedMailboxForCall (QString const& call, quint64 nowMs);
  void recordBeaconHistory (
      QString const& direction,
      decodium::ft2link::StationAdvertisement const& advertisement,
      QString const& source,
      quint64 nowMs);
  void recordClusterLastHeard (
      QString const& call,
      QString const& locator,
      QString const& name,
      QString const& profileName,
      QString const& event,
      QString const& source,
      bool cq,
      QString const& cqType,
      quint64 nowMs);
  QString clusterBandLabel (qint64 dialFrequencyHz) const;
  QString effectiveClusterNodeId () const;
  QVariantMap frequencyScheduleMap (
      FrequencyScheduleEntry const& entry,
      bool active = false,
      quint64 nowMs = 0) const;
  FrequencyScheduleEntry const* activeFrequencyScheduleEntry (
      quint64 nowMs) const;
  QVariantMap parkedMailboxSummaryForCallInternal (QString const& call,
                                                   quint64 nowMs) const;
  QVariantMap pathRelayCandidateForStation (QString const& relayCall,
                                            quint64 nowMs) const;
  QVariantMap relayWorkflowForStationInternal (QString const& relayCall,
                                               quint64 nowMs) const;
  quint32 recordForm (QString const& direction,
                      QString const& fromCall,
                      QString const& toCall,
                      QString const& formType,
                      QVariantMap const& fields,
                      QString const& state,
                      quint64 nowMs);
  bool updateFormState (quint32 formId,
                        QString const& state,
                        quint64 nowMs);
  quint32 recordFileTransfer (QString const& direction,
                              QString const& fromCall,
                              QString const& toCall,
                              QString const& fileName,
                              QString const& content,
                              QString const& contentBase64,
                              QString const& sha256,
                              QString const& state,
                              bool binary,
                              quint64 nowMs);
  bool updateFileTransferState (quint32 transferId,
                                QString const& state,
                                quint64 nowMs);
  quint32 recordBulletin (QString const& direction,
                          QString const& fromCall,
                          QString const& group,
                          QString const& title,
                          QString const& body,
                          QString const& state,
                          quint64 nowMs);
  bool updateBulletinState (quint32 bulletinId,
                            QString const& state,
                            quint64 nowMs);
  void recordPing (QString const& direction,
                   QString const& remoteCall,
                   quint16 token,
                   QString const& state,
                   quint64 nowMs,
                   quint64 rttMs = 0);
  void recordPathReport (QString const& direction,
                         QString const& remoteCall,
                         QString const& locator,
                         bool snrValid,
                         int snrDb,
                         bool qualityValid,
                         double quality,
                         double frequencyOffsetHz,
                         QString const& profileName,
                         QString const& rateName,
                         QString const& source,
                         quint64 nowMs);
  void recordPathFinderReport (QString const& direction,
                               QString const& remoteCall,
                               QString const& targetCall,
                               QString const& relayCall,
                               QString const& locator,
                               QString const& kind,
                               QString const& detail,
                               quint64 nowMs);
  void recordSnrReportsForText (quint16 sessionId,
                                QString const& direction,
                                QString const& text,
                                QString const& source,
                                quint64 nowMs);
  bool appendSystemText (quint16 sessionId,
                         QString const& text,
                         quint64 nowMs);
  QString localPresenceMessage (quint16 sessionId, quint64 nowMs) const;
  bool queuePresenceMessage (quint16 sessionId, quint64 nowMs);
  bool queueSuggestedReplies (quint16 sessionId,
                              QStringList const& replies,
                              quint64 nowMs);
  bool handleIncomingControlTags (quint16 sessionId,
                                  QString const& text,
                                  quint64 nowMs,
                                  bool* disconnectRequested);
  QString lastHeardTagReply (quint64 nowMs) const;
  QString lastHeardSpecificTagReply (QString const& call,
                                     quint64 nowMs) const;
  bool rejectBlockedHello (QString const& remoteCall,
                           decodium::ft2link::Frame const& hello,
                           quint64 nowMs,
                           decodium::ft2link::Frame* helloAck,
                           QString* resolvedRemoteCall);
  struct BbsSharedFile;
  bool recordOperatorActivity (quint64 nowMs);
  QVariantMap autoAwayResult (bool changed,
                              bool activated,
                              bool cleared) const;
  QVariantMap qsoAutomationResult (bool changed,
                                   int callIdsQueued,
                                   int sessionsClosed) const;
  QString automaticCallIdText () const;
  bool isAutomaticCallIdText (QString const& text) const;
  quint64 sessionLastRealActivityMs (
      decodium::ft2link::AppSession const& session) const;
  QString frequencyScheduleTagReply () const;
  QString parkedMailboxTagReply (QString const& remoteCall) const;
  QString lastConnectionsTagReply () const;
  QString bbsFileListReply (quint64 nowMs) const;
  bool bbsFileAvailable (QString const& fileName) const;
  BbsSharedFile const* bbsSharedFileForName (QString const& fileName) const;
  bool queueBbsSharedFileListReply (quint16 sessionId,
                                    QString const& remoteCall,
                                    quint64 nowMs);
  bool queueBbsSharedFileDownload (quint16 sessionId,
                                   QString const& remoteCall,
                                   QString const& fileName,
                                   quint64 nowMs);
  void setTypingPeer (QString const& call, bool typing, quint64 nowMs);
  bool expireTypingIndicators (quint64 nowMs);
  void touchContact (QString const& call,
                     quint64 nowMs,
                     QString const& event,
                     QString const& locator = QString {},
                     QString const& name = QString {},
                     QString const& profileName = QString {});
  void recordQsoSession (quint16 sessionId,
                         quint64 nowMs,
                         QString const& event);
  void appendChatLogEntry (quint16 sessionId,
                           QString const& directionName,
                           QString const& deliveryName,
                           QString const& text,
                           quint64 nowMs);
  void updateLastOutgoingChatLogEntry (quint16 sessionId,
                                       QString const& deliveryName,
                                       quint64 nowMs);
  bool completePendingQsyOutbound (quint16 sessionId,
                                   bool accepted,
                                   quint64 nowMs);
  void clearChatLogForSession (quint16 sessionId);
  void pruneLogbookOutbox ();

  struct RadioTxQueueItem
  {
    QString displayMessage;
    QVector<float> samples;
    QVariantMap plan;
    quint16 sessionId {0};
    bool cancelIfNoOutbound {false};
    bool strictLbt {false};
    quint64 lbtCancelAtMs {0};
    bool priority {false};
    quint64 enqueuedAtMs {0};  // per hold-off LBT massimo
    quint64 notBeforeMs {0};   // pre-TX CCA/jitter gate
  };

  struct LiveOutboundRetry
  {
    QString displayMessage;
    QVector<float> samples;
    QVariantMap plan;
    std::vector<std::uint8_t> payload;
    decodium::ft2link::Profile profile {decodium::ft2link::Profile::Wide2300};
    std::size_t messageIndex {0};
    unsigned attempts {0};
    quint64 nextRetryMs {0};
  };

  struct HelloRetry
  {
    decodium::ft2link::Frame frame;
    QString remoteCall;
    unsigned attemptsSent {0};
    unsigned maxAttempts {4};
    quint64 nextRetryMs {0};
  };

  struct BroadcastMessage
  {
    QString fromCall;
    QString text;
    QString source;
    QStringList alertTags;
    quint64 atMs {0};
  };

  struct AlertEvent
  {
    quint32 id {0};
    QString fromCall;
    QString text;
    QString source;
    QString tag;
    bool read {false};
    bool archived {false};
    quint64 atMs {0};
    quint64 updatedAtMs {0};
  };

  struct MailboxMessage
  {
    quint32 id {0};
    QString direction;
    QString fromCall;
    QString toCall;
    QString subject;
    QString body;
    QString state;
    bool urgent {false};
    bool emcomm {false};
    QString relayViaCall;
    int relayHopCount {0};
    QString relayProtocol;
    QString emailGatewayState;
    QString emailGatewayDetail;
    quint64 emailGatewayAtMs {0};
    quint64 atMs {0};
    quint64 updatedAtMs {0};
    quint64 relayNotifiedAtMs {0};
  };

  struct FormMessage
  {
    quint32 id {0};
    QString direction;
    QString fromCall;
    QString toCall;
    QString formType;
    QVariantMap fields;
    QString state;
    quint64 atMs {0};
    quint64 updatedAtMs {0};
  };

  struct FileTransfer
  {
    quint32 id {0};
    QString direction;
    QString fromCall;
    QString toCall;
    QString fileName;
    QString content;
    QString contentBase64;
    QString sha256;
    QString state;
    bool binary {false};
    bool read {false};
    quint64 atMs {0};
    quint64 updatedAtMs {0};
  };

  struct BbsSharedFile
  {
    quint32 id {0};
    QString fileName;
    QString content;
    QString contentBase64;
    QString sha256;
    bool binary {false};
    bool enabled {true};
    quint64 atMs {0};
    quint64 updatedAtMs {0};
    quint64 lastRequestedAtMs {0};
    int requestCount {0};
  };

  struct Bulletin
  {
    quint32 id {0};
    QString direction;
    QString fromCall;
    QString group;
    QString title;
    QString body;
    QString state;
    bool read {false};
    quint64 atMs {0};
    quint64 updatedAtMs {0};
  };

  struct ContactHistory
  {
    QString call;
    QString locator;
    QString name;
    QString tag;
    QString comment;
    QString lastEvent;
    QString lastProfileName;
    quint64 firstHeardMs {0};
    quint64 lastHeardMs {0};
    int qsoCount {0};
    int messageCount {0};
    int mailCount {0};
    int formCount {0};
    int fileCount {0};
    int bulletinCount {0};
    int broadcastCount {0};
    int alertCount {0};
  };

  struct QsoLogEntry
  {
    quint16 sessionId {0};
    QString remoteCall;
    QString profileName;
    QString rateName;
    QString state;
    QString lastEvent;
    quint64 openedAtMs {0};
    quint64 updatedAtMs {0};
    quint64 closedAtMs {0};
    int messageCount {0};
  };

  struct ChatLogEntry
  {
    quint16 sessionId {0};
    QString remoteCall;
    QString directionName;
    QString deliveryName;
    QString text;
    quint64 atMs {0};
  };

  struct LogbookUpload
  {
    quint32 id {0};
    quint16 sessionId {0};
    QString remoteCall;
    QString target;
    QString state;
    QString detail;
    QString adif;
    QString adifSha256;
    quint64 queuedAtMs {0};
    quint64 updatedAtMs {0};
  };

  struct PingRecord
  {
    QString direction;
    QString remoteCall;
    QString state;
    quint16 token {0};
    quint64 atMs {0};
    quint64 rttMs {0};
  };

  struct PathReport
  {
    quint32 id {0};
    QString direction;
    QString remoteCall;
    QString locator;
    bool snrValid {false};
    int snrDb {0};
    bool qualityValid {false};
    double quality {0.0};
    double frequencyOffsetHz {0.0};
    QString profileName;
    QString rateName;
    QString source;
    QString kind;
    QString targetCall;
    QString relayCall;
    QString detail;
    quint64 atMs {0};
  };

  struct PathRelayHint
  {
    QString targetCall;
    QString relayCall;
    QString locator;
    int ageMinutes {-1};
    quint64 atMs {0};
  };

  struct DigipeaterEvent
  {
    quint32 id {0};
    QString direction;
    QString originCall;
    QString targetCall;
    QString viaCall;
    QStringList path;
    QString payloadText;
    QString state;
    int ttl {0};
    QString detail;
    quint64 atMs {0};
  };

  struct BeaconHistoryEntry
  {
    QString direction;
    QString call;
    QString locator;
    QString name;
    QString profileName;
    QString capabilitySummary;
    quint16 waveformCapabilityFlags {0};
    quint16 serviceCapabilityFlags {0};
    bool cq {false};
    QString cqType;
    QString cqLocator;
    int cqSlotId {0};
    int cqSlotSizeHz {0};
    QString source;
    quint64 atMs {0};
  };

  struct ClusterLastHeardEntry
  {
    QString call;
    QString locator;
    QString name;
    QString profileName;
    QString event;
    QString source;
    QString nodeId;
    QString band;
    qint64 dialFrequencyHz {0};
    bool cq {false};
    QString cqType;
    quint64 firstHeardMs {0};
    quint64 lastHeardMs {0};
    int heardCount {0};
  };

  struct LocalOperatorProfile
  {
    QString qth;
    QString email;
    QString ice;
    QString rig;
    QString antenna;
    QString power;
    QString gps;
  };

  struct CannedMessage
  {
    QString label;
    QString templateText;
    QString tip;
  };

  decodium::ft2link::FT2LinkAppModel m_model;
  LocalOperatorProfile m_localProfile;
  quint16 m_activeSessionId {0};
  QString m_lastError;
  QString m_transportState {QStringLiteral ("Idle")};
  bool m_transportBusy {false};
  QVariantMap m_lastTransportMetrics;
  bool m_radioTxArmed {false};
  bool m_deepRateEnabled {false};
  QVariantMap m_lastRadioTxPlan;
  std::vector<float> m_preparedRadioTxSamples;
  quint16 m_preparedRadioTxSessionId {0};
  QString m_preparedRadioTxText;
  decodium::ft2link::W2300RateMode m_preparedRadioTxW2300RateMode {
    decodium::ft2link::W2300RateMode::Fast
  };
  std::map<std::uint16_t, std::unique_ptr<decodium::ft2link::OutboundTransfer> > m_liveOutbound;
  std::map<std::uint16_t, std::size_t> m_liveOutboundMessageIndex;
  std::map<std::uint16_t, quint32> m_liveOutboundMailboxId;
  std::map<std::uint16_t, QString> m_liveOutboundMailboxDeliveredState;
  std::map<std::uint16_t, quint32> m_liveOutboundFormId;
  std::map<std::uint16_t, quint32> m_liveOutboundFileTransferId;
  std::map<std::uint16_t, quint32> m_liveOutboundBulletinId;
  std::map<std::uint16_t, std::unique_ptr<decodium::ft2link::InboundTransfer> > m_liveInbound;
  std::map<std::uint16_t, bool> m_liveInboundDelivered;
  std::map<std::uint16_t, std::uint64_t> m_liveInboundDeliveredAtMs;
  std::map<std::uint16_t, QString> m_liveInboundDeliveredHash;
  std::map<std::uint16_t, quint64> m_pendingInboundAckDueMs;
  std::map<std::uint16_t, LiveOutboundRetry> m_liveOutboundRetries;
  std::map<std::uint16_t, HelloRetry> m_helloRetries;
  std::map<std::uint16_t, decodium::ft2link::W2300RateController> m_liveW2300RateControllers;
  std::map<std::uint16_t, decodium::ft2link::W2300DecodeMetrics> m_lastLiveW2300Metrics;
  std::vector<BroadcastMessage> m_broadcasts;
  std::vector<AlertEvent> m_alerts;
  std::vector<MailboxMessage> m_mailbox;
  std::vector<FormMessage> m_forms;
  std::vector<FileTransfer> m_fileTransfers;
  std::vector<BbsSharedFile> m_bbsSharedFiles;
  std::vector<Bulletin> m_bulletins;
  std::map<QString, ContactHistory> m_contactHistory;
  std::map<quint16, QsoLogEntry> m_qsoLog;
  std::vector<ChatLogEntry> m_chatLog;
  std::vector<LogbookUpload> m_logbookOutbox;
  std::vector<PingRecord> m_pingLog;
  std::vector<PathReport> m_pathReports;
  std::vector<PathRelayHint> m_pathRelayHints;
  std::vector<DigipeaterEvent> m_digipeaterEvents;
  std::map<QString, quint64> m_digipeaterSeen;
  std::vector<BeaconHistoryEntry> m_beaconHistory;
  std::map<QString, ClusterLastHeardEntry> m_clusterLastHeard;
  std::vector<CannedMessage> m_customCannedMessages;
  QStringList m_customAlertTags;
  std::vector<FrequencyPreset> m_frequencyPresets;
  std::vector<AllowedQsyRange> m_allowedQsyRanges;
  std::vector<FrequencyScheduleEntry> m_frequencySchedule;
  bool m_awayEnabled {false};
  bool m_awayAcceptsQsy {false};
  QString m_awayMessage {QStringLiteral ("QRX DE <MYCALL>")};
  bool m_welcomeEnabled {false};
  QString m_welcomeMessage {QStringLiteral ("HELLO <CALL> DE <MYCALL>")};
  bool m_autoReplyEnabled {false};
  bool m_autoAwayEnabled {false};
  int m_autoAwayMinutes {10};
  bool m_autoAwayActivated {false};
  quint64 m_lastOperatorActivityMs {0};
  int m_callIdIntervalMinutes {0};
  int m_autoDisconnectMinutes {0};
  bool m_incomingPingsEnabled {true};
  bool m_lastHeardPeekingEnabled {true};
  bool m_lastConnectionsPeekingEnabled {true};
  bool m_parkedVmailPeekingEnabled {true};
  bool m_vmailParkingEnabled {true};
  bool m_snrReportSendingEnabled {true};
  bool m_verboseSnrAutoAcceptEnabled {false};
  bool m_infoInquireEnabled {true};
  bool m_digipeaterEnabled {false};
  int m_digipeaterMaxHops {2};
  bool m_bbsFileServerEnabled {false};
  bool m_clusterEnabled {true};
  QString m_clusterNodeId;
  QString m_clusterBand;
  qint64 m_clusterDialFrequencyHz {0};
  std::map<quint16, quint64> m_lastCallIdQueuedAtMs;
  QStringList m_blockedCalls;
  std::map<quint16, std::pair<QString, quint64> > m_pendingPings;
  std::map<QString, quint64> m_typingPeers;
  quint64 m_beaconsSent {0};
  quint64 m_beaconsReceived {0};
  quint64 m_cqsSent {0};
  quint64 m_cqsReceived {0};
  quint16 m_nextPingToken {1u};
  quint32 m_nextMailboxId {1u};
  quint32 m_nextFormId {1u};
  quint32 m_nextFileTransferId {1u};
  quint32 m_nextBbsSharedFileId {1u};
  quint32 m_nextBulletinId {1u};
  quint32 m_nextAlertId {1u};
  quint32 m_nextPathReportId {1u};
  quint32 m_nextDigipeaterEventId {1u};
  quint32 m_nextLogbookUploadId {1u};
  QString m_localStorePath;
  bool m_localStoreLoaded {false};
  QString m_lastLocalStoreError;
  bool m_localStorePersistenceEnabled {false};
  bool m_loadingLocalStore {false};
  std::deque<RadioTxQueueItem> m_radioTxQueue;
  QTimer m_radioTxQueueTimer;
  QTimer m_inboundAckTimer;
  QTimer m_liveOutboundRetryTimer;
  QTimer m_helloRetryTimer;
  QTimer m_autoBeaconTimer;
  QTimer m_liveChannelTimer;
  quint64 m_radioTxBusyUntilMs {0};
  quint16 m_lastRadioTxSessionId {0};
  bool m_nextRadioTxStrictLbt {false};
  int m_nextRadioTxStrictLbtCancelMs {24000};
  quint64 m_lastBeaconTxMs {0};
  quint64 m_liveChannelBusyUntilMs {0};
  bool m_liveChannelBusy {false};
  quint64 m_liveChannelLbtBusyUntilMs {0};
  bool m_liveChannelLbtBusy {false};
  quint64 m_liveChannelLastLbtEnergyMs {0};
  double m_liveChannelRms {0.0};
  double m_liveChannelPeak {0.0};
  bool m_autoBeaconEnabled {false};
  bool m_autoBeaconCq {true};
  int m_autoBeaconIntervalSeconds {180};
  bool m_rfLabRecording {false};
  QVector<short> m_rfLabRecordedSamples;
  QString m_rfLabRecordingPath;
  quint64 m_rfLabRecordingStartedMs {0};
  QString m_rfLabLastPath;
  QVariantMap m_rfLabLastReport;
  quint64 m_lastRxIngestLogMs {0};
};

#endif
