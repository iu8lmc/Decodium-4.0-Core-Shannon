#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include <QMutex>
#include <atomic>
#include <memory>

#include "DecodiumThemeManager.h"
#include "DecodiumSubManagers.h"
#include "DecodiumCatManager.h"
#include "DecodiumOmniRigManager.h"
#include "DecodiumTransceiverManager.h"

class ActiveStationsModel;
class DecodiumAlertManager;
#include "DecodiumDxCluster.h"
class DecodiumPskReporterLite;
class DecodiumCloudlogLite;
class DecodiumWsprUploader;
class DxccLookup;
class DecodiumLegacyBackend;
class DecodiumPropagationManager;
class MessageClient;

Q_DECLARE_OPAQUE_POINTER(DecodiumDxCluster*)

// Forward declarations
class SoundInput;
class SoundOutput;
class DecodiumAudioSink;
class Modulator;
class QAudioSink;
class QBuffer;
class NtpClient;
namespace decodium {
  namespace ft8     { class FT8DecodeWorker;       struct DecodeRequest; }
  namespace ft2     { class FT2DecodeWorker;       struct DecodeRequest; }
  namespace ft4     { class FT4DecodeWorker;       struct DecodeRequest; }
  namespace q65     { class Q65DecodeWorker;       struct DecodeRequest; }
  namespace msk144  { class MSK144DecodeWorker;    struct DecodeRequest; }
  namespace wspr    { class WSPRDecodeWorker;      struct DecodeRequest; }
  namespace legacyjt{ class LegacyJtDecodeWorker;  struct DecodeRequest; }
  namespace fst4    { class FST4DecodeWorker;      struct DecodeRequest; }
}

class RemoteCommandServer;

class DecodiumBridge : public QObject
{
    Q_OBJECT

    // === STATION ===
    Q_PROPERTY(QString callsign READ callsign WRITE setCallsign NOTIFY callsignChanged)
    Q_PROPERTY(QString grid READ grid WRITE setGrid NOTIFY gridChanged)
    Q_PROPERTY(double frequency READ frequency WRITE setFrequency NOTIFY frequencyChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)

    // === RX/TX STATE ===
    Q_PROPERTY(bool monitoring READ monitoring WRITE setMonitoring NOTIFY monitoringChanged)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY transmittingChanged)
    Q_PROPERTY(bool tuning READ tuning NOTIFY tuningChanged)
    Q_PROPERTY(bool decoding READ decoding NOTIFY decodingChanged)

    // === AUDIO FREQUENCIES ===
    Q_PROPERTY(int rxFrequency READ rxFrequency WRITE setRxFrequency NOTIFY rxFrequencyChanged)
    Q_PROPERTY(int txFrequency READ txFrequency WRITE setTxFrequency NOTIFY txFrequencyChanged)

    // === AUDIO LEVELS ===
    Q_PROPERTY(double audioLevel READ audioLevel NOTIFY audioLevelChanged)
    Q_PROPERTY(double sMeter READ sMeter NOTIFY sMeterChanged)
    Q_PROPERTY(bool legacyBackendActive READ legacyBackendActive CONSTANT)
    Q_PROPERTY(double rxInputLevel READ rxInputLevel WRITE setRxInputLevel NOTIFY rxInputLevelChanged)
    Q_PROPERTY(double txOutputLevel READ txOutputLevel WRITE setTxOutputLevel NOTIFY txOutputLevelChanged)
    Q_PROPERTY(QStringList audioInputDevices READ audioInputDevices NOTIFY audioInputDevicesChanged)
    Q_PROPERTY(QStringList audioOutputDevices READ audioOutputDevices NOTIFY audioOutputDevicesChanged)
    Q_PROPERTY(QString audioInputDevice READ audioInputDevice WRITE setAudioInputDevice NOTIFY audioInputDeviceChanged)
    Q_PROPERTY(QString audioOutputDevice READ audioOutputDevice WRITE setAudioOutputDevice NOTIFY audioOutputDeviceChanged)
    Q_PROPERTY(int audioInputChannel READ audioInputChannel WRITE setAudioInputChannel NOTIFY audioInputChannelChanged)
    Q_PROPERTY(int audioOutputChannel READ audioOutputChannel WRITE setAudioOutputChannel NOTIFY audioOutputChannelChanged)

    // === DECODE ===
    Q_PROPERTY(QVariantList decodeList READ decodeList NOTIFY decodeListChanged)
    Q_PROPERTY(QVariantList rxDecodeList READ rxDecodeList NOTIFY rxDecodeListChanged)
    Q_PROPERTY(int periodProgress READ periodProgress NOTIFY periodProgressChanged)
    Q_PROPERTY(QString utcTime READ utcTime NOTIFY utcTimeChanged)

    // === TX MESSAGES ===
    Q_PROPERTY(QString tx1 READ tx1 WRITE setTx1 NOTIFY tx1Changed)
    Q_PROPERTY(QString tx2 READ tx2 WRITE setTx2 NOTIFY tx2Changed)
    Q_PROPERTY(QString tx3 READ tx3 WRITE setTx3 NOTIFY tx3Changed)
    Q_PROPERTY(QString tx4 READ tx4 WRITE setTx4 NOTIFY tx4Changed)
    Q_PROPERTY(QString tx5 READ tx5 WRITE setTx5 NOTIFY tx5Changed)
    Q_PROPERTY(QString tx6 READ tx6 WRITE setTx6 NOTIFY tx6Changed)
    Q_PROPERTY(int currentTx READ currentTx WRITE setCurrentTx NOTIFY currentTxChanged)
    Q_PROPERTY(QString dxCall READ dxCall WRITE setDxCall NOTIFY dxCallChanged)
    Q_PROPERTY(QString dxGrid READ dxGrid WRITE setDxGrid NOTIFY dxGridChanged)
    // txMessages/currentTxMessage: usati da TxPanel (engine = bridge)
    Q_PROPERTY(QStringList txMessages READ txMessages NOTIFY txMessagesChanged)
    Q_PROPERTY(QString currentTxMessage READ currentTxMessage NOTIFY currentTxMessageChanged)

    // === APPENGINE STUB PROPERTIES (TxPanel/Main.qml usa engine = bridge) ===
    Q_PROPERTY(bool swlMode READ swlMode WRITE setSwlMode NOTIFY swlModeChanged)
    Q_PROPERTY(bool splitMode READ splitMode WRITE setSplitMode NOTIFY splitModeChanged)
    Q_PROPERTY(int txWatchdogMode READ txWatchdogMode WRITE setTxWatchdogMode NOTIFY txWatchdogModeChanged)
    Q_PROPERTY(int txWatchdogTime READ txWatchdogTime WRITE setTxWatchdogTime NOTIFY txWatchdogTimeChanged)
    Q_PROPERTY(int txWatchdogCount READ txWatchdogCount WRITE setTxWatchdogCount NOTIFY txWatchdogCountChanged)
    Q_PROPERTY(bool filterCqOnly READ filterCqOnly WRITE setFilterCqOnly NOTIFY filterCqOnlyChanged)
    Q_PROPERTY(bool filterMyCallOnly READ filterMyCallOnly WRITE setFilterMyCallOnly NOTIFY filterMyCallOnlyChanged)
    Q_PROPERTY(int contestType READ contestType WRITE setContestType NOTIFY contestTypeChanged)
    Q_PROPERTY(bool zapEnabled READ zapEnabled WRITE setZapEnabled NOTIFY zapEnabledChanged)
    Q_PROPERTY(bool deepSearchEnabled READ deepSearchEnabled WRITE setDeepSearchEnabled NOTIFY deepSearchEnabledChanged)

    // === QSO PROGRESS ===
    // QSOProgress: 0=IDLE, 1=CALLING_CQ, 2=REPLYING, 3=REPORT, 4=ROGER_REPORT, 5=SIGNOFF, 6=IDLE_QSO
    Q_PROPERTY(int qsoProgress READ qsoProgress NOTIFY qsoProgressChanged)
    Q_PROPERTY(QString reportSent     READ reportSent     NOTIFY reportSentChanged)
    Q_PROPERTY(QString reportReceived READ reportReceived WRITE setReportReceived NOTIFY reportReceivedChanged)
    Q_PROPERTY(bool    sendRR73       READ sendRR73       WRITE setSendRR73       NOTIFY sendRR73Changed)
    Q_PROPERTY(bool multiAnswerMode READ multiAnswerMode WRITE setMultiAnswerMode NOTIFY multiAnswerModeChanged)

    // === TX CONTROL ===
    Q_PROPERTY(bool autoSeq          READ autoSeq          WRITE setAutoSeq          NOTIFY autoSeqChanged)
    Q_PROPERTY(bool txEnabled        READ txEnabled        WRITE setTxEnabled        NOTIFY txEnabledChanged)
    Q_PROPERTY(bool autoCqRepeat     READ autoCqRepeat     WRITE setAutoCqRepeat     NOTIFY autoCqRepeatChanged)
    Q_PROPERTY(int  maxCallerRetries READ maxCallerRetries WRITE setMaxCallerRetries NOTIFY maxCallerRetriesChanged)
    Q_PROPERTY(int  autoCqMaxCycles  READ autoCqMaxCycles  WRITE setAutoCqMaxCycles  NOTIFY autoCqMaxCyclesChanged)
    Q_PROPERTY(int  autoCqPauseSec   READ autoCqPauseSec   WRITE setAutoCqPauseSec   NOTIFY autoCqPauseSecChanged)
    Q_PROPERTY(bool avgDecodeEnabled READ avgDecodeEnabled WRITE setAvgDecodeEnabled NOTIFY avgDecodeEnabledChanged)
    Q_PROPERTY(int  txPeriod         READ txPeriod         WRITE setTxPeriod         NOTIFY txPeriodChanged)
    Q_PROPERTY(bool alt12Enabled     READ alt12Enabled     WRITE setAlt12Enabled     NOTIFY alt12EnabledChanged)
    // FT2-specific: async TX (no period sync) e dual carrier
    Q_PROPERTY(bool asyncTxEnabled     READ asyncTxEnabled     WRITE setAsyncTxEnabled     NOTIFY asyncTxEnabledChanged)
    Q_PROPERTY(bool asyncDecodeEnabled READ asyncDecodeEnabled WRITE setAsyncDecodeEnabled NOTIFY asyncDecodeEnabledChanged)
    Q_PROPERTY(bool dualCarrierEnabled READ dualCarrierEnabled WRITE setDualCarrierEnabled NOTIFY dualCarrierEnabledChanged)
    Q_PROPERTY(bool quickQsoEnabled    READ quickQsoEnabled    WRITE setQuickQsoEnabled    NOTIFY quickQsoEnabledChanged)
    Q_PROPERTY(int  asyncSnrDb         READ asyncSnrDb                                     NOTIFY asyncSnrDbChanged)

    // === CAT/TRANSCEIVER ===
    Q_PROPERTY(bool catConnected READ catConnected NOTIFY catConnectedChanged)
    Q_PROPERTY(QString catRigName READ catRigName NOTIFY catRigNameChanged)
    Q_PROPERTY(QString catMode READ catMode NOTIFY catModeChanged)

    // === LED STATUS INDICATORS ===
    Q_PROPERTY(bool   ledCoherentAveraging READ ledCoherentAveraging NOTIFY ledCoherentAveragingChanged)
    Q_PROPERTY(bool   ledNeuralSync        READ ledNeuralSync        NOTIFY ledNeuralSyncChanged)
    Q_PROPERTY(bool   ledTurboFeedback     READ ledTurboFeedback     NOTIFY ledTurboFeedbackChanged)
    Q_PROPERTY(int    coherentCount        READ coherentCount        NOTIFY coherentCountChanged)
    Q_PROPERTY(double neuralScore          READ neuralScore          NOTIFY neuralScoreChanged)
    Q_PROPERTY(int    turboIterations      READ turboIterations      NOTIFY turboIterationsChanged)
    Q_PROPERTY(int    ftThreads            READ ftThreads            CONSTANT)

    // === PSK REPORTER ===
    Q_PROPERTY(bool       pskSearchFound      READ pskSearchFound      NOTIFY pskSearchFoundChanged)
    Q_PROPERTY(QString    pskSearchCallsign   READ pskSearchCallsign   NOTIFY pskSearchCallsignChanged)
    Q_PROPERTY(bool       pskSearching        READ pskSearching        NOTIFY pskSearchingChanged)
    Q_PROPERTY(QStringList pskSearchBands     READ pskSearchBands      NOTIFY pskSearchBandsChanged)
    Q_PROPERTY(bool       pskReporterEnabled  READ pskReporterEnabled  WRITE setPskReporterEnabled  NOTIFY pskReporterEnabledChanged)
    Q_PROPERTY(bool       pskReporterConnected READ pskReporterConnected NOTIFY pskReporterConnectedChanged)

    // === EXTRA FEATURES (appEngine migration) ===
    Q_PROPERTY(bool    alertOnCq         READ alertOnCq         WRITE setAlertOnCq         NOTIFY alertOnCqChanged)
    Q_PROPERTY(bool    alertOnMyCall     READ alertOnMyCall     WRITE setAlertOnMyCall     NOTIFY alertOnMyCallChanged)
    Q_PROPERTY(bool    recordRxEnabled   READ recordRxEnabled   WRITE setRecordRxEnabled   NOTIFY recordRxEnabledChanged)
    Q_PROPERTY(bool    recordTxEnabled   READ recordTxEnabled   WRITE setRecordTxEnabled   NOTIFY recordTxEnabledChanged)
    Q_PROPERTY(QString stationName       READ stationName       WRITE setStationName       NOTIFY stationNameChanged)
    Q_PROPERTY(QString stationQth        READ stationQth        WRITE setStationQth        NOTIFY stationQthChanged)
    Q_PROPERTY(QString stationRigInfo    READ stationRigInfo    WRITE setStationRigInfo    NOTIFY stationRigInfoChanged)
    Q_PROPERTY(QString stationAntenna    READ stationAntenna    WRITE setStationAntenna    NOTIFY stationAntennaChanged)
    Q_PROPERTY(int     stationPowerWatts READ stationPowerWatts WRITE setStationPowerWatts NOTIFY stationPowerWattsChanged)
    Q_PROPERTY(bool    autoStartMonitorOnStartup READ autoStartMonitorOnStartup WRITE setAutoStartMonitorOnStartup NOTIFY autoStartMonitorOnStartupChanged)
    Q_PROPERTY(bool    startFromTx2      READ startFromTx2      WRITE setStartFromTx2      NOTIFY startFromTx2Changed)
    Q_PROPERTY(bool    vhfUhfFeatures    READ vhfUhfFeatures    WRITE setVhfUhfFeatures    NOTIFY vhfUhfFeaturesChanged)
    Q_PROPERTY(bool    directLogQso      READ directLogQso      WRITE setDirectLogQso      NOTIFY directLogQsoChanged)
    Q_PROPERTY(bool    confirm73         READ confirm73         WRITE setConfirm73         NOTIFY confirm73Changed)
    Q_PROPERTY(QString contestExchange   READ contestExchange   WRITE setContestExchange   NOTIFY contestExchangeChanged)
    Q_PROPERTY(int     contestNumber     READ contestNumber     WRITE setContestNumber     NOTIFY contestNumberChanged)
    Q_PROPERTY(QStringList contestTypeNames READ contestTypeNames CONSTANT)
    Q_PROPERTY(QString logAllTxtPath     READ logAllTxtPath     CONSTANT)
    Q_PROPERTY(QObject* logManager READ logManager CONSTANT)
    Q_PROPERTY(QObject* propagationManager READ propagationManager CONSTANT)
    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY qsoCountChanged)

    // === ADIF / LOTW ===
    Q_PROPERTY(int  workedCount  READ workedCount  NOTIFY workedCountChanged)
    Q_PROPERTY(bool lotwEnabled  READ lotwEnabled  WRITE setLotwEnabled  NOTIFY lotwEnabledChanged)
    Q_PROPERTY(bool lotwUpdating READ lotwUpdating NOTIFY lotwUpdatingChanged)

    // === CLOUDLOG ===
    Q_PROPERTY(bool    cloudlogEnabled READ cloudlogEnabled WRITE setCloudlogEnabled NOTIFY cloudlogEnabledChanged)
    Q_PROPERTY(QString cloudlogUrl     READ cloudlogUrl     WRITE setCloudlogUrl     NOTIFY cloudlogUrlChanged)
    Q_PROPERTY(QString cloudlogApiKey  READ cloudlogApiKey  WRITE setCloudlogApiKey  NOTIFY cloudlogApiKeyChanged)

    // === THEME / UI ===
    Q_PROPERTY(DecodiumThemeManager* themeManager READ themeManager CONSTANT)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY fontScaleChanged)

    // === SUB-MANAGERS ===
    Q_PROPERTY(WavManager*          wavManager      READ wavManager      CONSTANT)
    Q_PROPERTY(MacroManager*        macroManager    READ macroManager    CONSTANT)
    Q_PROPERTY(BandManager*         bandManager     READ bandManager     CONSTANT)
    // catManager ritorna QObject* (può essere DecodiumCatManager o DecodiumTransceiverManager)
    Q_PROPERTY(QObject*                      catManager      READ catManagerObj   NOTIFY catManagerChanged)
    Q_PROPERTY(QString                       catBackend      READ catBackend      WRITE setCatBackend NOTIFY catBackendChanged)
    Q_PROPERTY(DecodiumCatManager*           nativeCat       READ nativeCat       CONSTANT)
    Q_PROPERTY(DecodiumOmniRigManager*       omniRigCat      READ omniRigCat      CONSTANT)
    Q_PROPERTY(DecodiumTransceiverManager*   hamlibCat       READ hamlibCat       CONSTANT)
    Q_PROPERTY(DecodiumDxCluster*   dxCluster       READ dxCluster       CONSTANT)
    Q_PROPERTY(bool dxClusterConnected READ dxClusterConnected NOTIFY dxClusterConnectedChanged)
    Q_PROPERTY(QVariantList dxClusterSpots READ dxClusterSpots NOTIFY dxClusterSpotsChanged)
    Q_PROPERTY(QString dxClusterHost READ dxClusterHost WRITE setDxClusterHost NOTIFY dxClusterHostChanged)
    Q_PROPERTY(int dxClusterPort READ dxClusterPort WRITE setDxClusterPort NOTIFY dxClusterPortChanged)
    // WSPR upload
    Q_PROPERTY(bool wsprUploadEnabled READ wsprUploadEnabled WRITE setWsprUploadEnabled NOTIFY wsprUploadEnabledChanged)

    // ── UI persistence properties (saved via saveSettings) ─────────────
    Q_PROPERTY(int   uiSpectrumHeight  READ uiSpectrumHeight  WRITE setUiSpectrumHeight  NOTIFY uiSpectrumHeightChanged)
    Q_PROPERTY(int   uiPaletteIndex    READ uiPaletteIndex    WRITE setUiPaletteIndex    NOTIFY uiPaletteIndexChanged)
    Q_PROPERTY(double uiZoomFactor     READ uiZoomFactor      WRITE setUiZoomFactor      NOTIFY uiZoomFactorChanged)
    Q_PROPERTY(int   uiWaterfallHeight READ uiWaterfallHeight WRITE setUiWaterfallHeight NOTIFY uiWaterfallHeightChanged)
    Q_PROPERTY(int   uiDecodeWinX      READ uiDecodeWinX      WRITE setUiDecodeWinX      NOTIFY uiDecodeWinXChanged)
    Q_PROPERTY(int   uiDecodeWinY      READ uiDecodeWinY      WRITE setUiDecodeWinY      NOTIFY uiDecodeWinYChanged)
    Q_PROPERTY(int   uiDecodeWinWidth  READ uiDecodeWinWidth  WRITE setUiDecodeWinWidth  NOTIFY uiDecodeWinWidthChanged)
    Q_PROPERTY(int   uiDecodeWinHeight READ uiDecodeWinHeight WRITE setUiDecodeWinHeight NOTIFY uiDecodeWinHeightChanged)

    // === DECODER SETTINGS ===
    Q_PROPERTY(int nfa READ nfa WRITE setNfa NOTIFY nfaChanged)
    Q_PROPERTY(int nfb READ nfb WRITE setNfb NOTIFY nfbChanged)
    Q_PROPERTY(int ndepth READ ndepth WRITE setNdepth NOTIFY ndepthChanged)
    Q_PROPERTY(int ncontest READ ncontest WRITE setNcontest NOTIFY ncontestChanged)

    // === SPECTRUM / WATERFALL ===
    Q_PROPERTY(int   spectrumColorPalette READ spectrumColorPalette WRITE setSpectrumColorPalette NOTIFY spectrumColorPaletteChanged)
    Q_PROPERTY(double spectrumRefLevel    READ spectrumRefLevel    WRITE setSpectrumRefLevel    NOTIFY spectrumRefLevelChanged)
    Q_PROPERTY(double spectrumDynRange    READ spectrumDynRange    WRITE setSpectrumDynRange    NOTIFY spectrumDynRangeChanged)
    Q_PROPERTY(bool  spectrumVisible      READ spectrumVisible     WRITE setSpectrumVisible     NOTIFY spectrumVisibleChanged)

    // === B6 — cty.dat AUTO-UPDATE ===
    Q_PROPERTY(bool    ctyDatUpdating  READ ctyDatUpdating  NOTIFY ctyDatUpdatingChanged)

    // === B7 — COLOR HIGHLIGHTING ===
    Q_PROPERTY(QString colorCQ        READ colorCQ        WRITE setColorCQ        NOTIFY colorCQChanged)
    Q_PROPERTY(QString colorMyCall    READ colorMyCall    WRITE setColorMyCall    NOTIFY colorMyCallChanged)
    Q_PROPERTY(QString colorDXEntity  READ colorDXEntity  WRITE setColorDXEntity  NOTIFY colorDXEntityChanged)
    Q_PROPERTY(QString color73        READ color73        WRITE setColor73        NOTIFY color73Changed)
    Q_PROPERTY(QString colorB4        READ colorB4        WRITE setColorB4        NOTIFY colorB4Changed)
    Q_PROPERTY(bool    b4Strikethrough READ b4Strikethrough WRITE setB4Strikethrough NOTIFY b4StrikethroughChanged)
    // Alias usato da DecodeList.qml
    Q_PROPERTY(bool    showB4Strikethrough READ b4Strikethrough NOTIFY b4StrikethroughChanged)

    // === B8 — ALERT SOUNDS ===
    Q_PROPERTY(bool alertSoundsEnabled READ alertSoundsEnabled WRITE setAlertSoundsEnabled NOTIFY alertSoundsEnabledChanged)

    // === C16 — UPDATE CHECKER ===
    Q_PROPERTY(bool    updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString latestVersion   READ latestVersion   NOTIFY latestVersionChanged)

    // === A2 — SOUNDCARD DRIFT (C13/A2) ===
    Q_PROPERTY(double  soundcardDriftPpm        READ soundcardDriftPpm        NOTIFY soundcardDriftChanged)
    Q_PROPERTY(double  soundcardDriftMsPerPeriod READ soundcardDriftMsPerPeriod NOTIFY soundcardDriftChanged)
    Q_PROPERTY(QString driftSeverity            READ driftSeverity            NOTIFY soundcardDriftChanged)

    // === A3 — TIME SYNC ===
    Q_PROPERTY(bool   ntpEnabled     READ ntpEnabled     NOTIFY ntpEnabledChanged)
    Q_PROPERTY(double ntpOffsetMs    READ ntpOffsetMs    NOTIFY ntpOffsetMsChanged)
    Q_PROPERTY(bool   ntpSynced      READ ntpSynced      NOTIFY ntpSyncedChanged)
    Q_PROPERTY(double avgDt          READ avgDt          NOTIFY avgDtChanged)
    Q_PROPERTY(double decodeLatencyMs READ decodeLatencyMs NOTIFY decodeLatencyMsChanged)
    Q_PROPERTY(int    timeSyncSampleCount READ timeSyncSampleCount NOTIFY timeSyncSampleCountChanged)

    // === A4 — FOX/HOUND MODE ===
    Q_PROPERTY(bool        foxMode        READ foxMode        WRITE setFoxMode        NOTIFY foxModeChanged)
    Q_PROPERTY(bool        houndMode      READ houndMode      WRITE setHoundMode      NOTIFY houndModeChanged)
    Q_PROPERTY(QStringList callerQueue    READ callerQueue    NOTIFY callerQueueChanged)
    Q_PROPERTY(int         callerQueueSize READ callerQueueSize NOTIFY callerQueueChanged)

    // === B9 — ACTIVE STATIONS MODEL ===
    Q_PROPERTY(QObject* activeStations READ activeStations CONSTANT)

public:
    explicit DecodiumBridge(QObject* parent = nullptr);
    ~DecodiumBridge();

    // Station
    QString callsign() const;
    void setCallsign(const QString&);
    QString grid() const;
    void setGrid(const QString&);
    double frequency() const;
    void setFrequency(double);
    QString mode() const;
    void setMode(const QString&);

    // RX/TX State
    bool monitoring() const;
    void setMonitoring(bool);
    bool transmitting() const;
    bool tuning() const { return m_tuning; }
    bool decoding() const;

    // Audio frequencies
    int rxFrequency() const { return m_rxFrequency; }
    void setRxFrequency(int f);
    int txFrequency() const { return m_txFrequency; }
    void setTxFrequency(int f);

    // Audio levels
    double audioLevel() const;
    double sMeter() const;
    bool legacyBackendActive() const { return usingLegacyBackendForTx(); }
    double rxInputLevel() const { return m_rxInputLevel; }
    void setRxInputLevel(double v);
    double txOutputLevel() const { return m_txOutputLevel; }
    void setTxOutputLevel(double v);
    QStringList audioInputDevices() const;
    QStringList audioOutputDevices() const;
    QString audioInputDevice() const;
    void setAudioInputDevice(const QString&);
    QString audioOutputDevice() const;
    void setAudioOutputDevice(const QString&);
    int audioInputChannel() const;
    void setAudioInputChannel(int);
    int audioOutputChannel() const;
    void setAudioOutputChannel(int);

    // Decode
    QVariantList decodeList() const;
    QVariantList rxDecodeList() const;
    int periodProgress() const;
    QString utcTime() const;

    // TX Messages
    QString tx1() const; void setTx1(const QString&);
    QString tx2() const; void setTx2(const QString&);
    QString tx3() const; void setTx3(const QString&);
    QString tx4() const; void setTx4(const QString&);
    QString tx5() const; void setTx5(const QString&);
    QString tx6() const; void setTx6(const QString&);
    int currentTx() const; void setCurrentTx(int);
    QString dxCall() const; void setDxCall(const QString&);
    QString dxGrid() const; void setDxGrid(const QString&);
    // txMessages / currentTxMessage — usati da TxPanel (engine alias)
    QStringList txMessages() const { return {m_tx1, m_tx2, m_tx3, m_tx4, m_tx5, m_tx6}; }
    QString currentTxMessage() const { return buildCurrentTxMessage(); }

    // QSO progress
    int qsoProgress() const { return m_qsoProgress; }
    QString reportSent() const { return m_reportSent; }
    void setReportSent(const QString& v);
    QString reportReceived() const { return m_reportReceived; }
    void setReportReceived(const QString& v);
    bool sendRR73() const { return m_sendRR73; }
    void setSendRR73(bool v);
    bool multiAnswerMode() const { return m_multiAnswerMode; }
    void setMultiAnswerMode(bool v) { if (m_multiAnswerMode != v) { m_multiAnswerMode = v; emit multiAnswerModeChanged(); } }

    bool autoSeq()           const { return m_autoSeq; }
    void setAutoSeq(bool v);
    bool txEnabled()         const { return m_txEnabled; }
    void setTxEnabled(bool v);
    bool autoCqRepeat()      const { return m_autoCqRepeat; }
    void setAutoCqRepeat(bool v);
    int  maxCallerRetries()  const { return m_maxCallerRetries; }
    void setMaxCallerRetries(int v) { if (m_maxCallerRetries != v) { m_maxCallerRetries = qBound(1, v, 99); emit maxCallerRetriesChanged(); } }
    int  autoCqMaxCycles()   const { return m_autoCqMaxCycles; }
    void setAutoCqMaxCycles(int v) { if (m_autoCqMaxCycles != v) { m_autoCqMaxCycles = qBound(0, v, 999); emit autoCqMaxCyclesChanged(); } }
    int  autoCqPauseSec()    const { return m_autoCqPauseSec; }
    void setAutoCqPauseSec(int v) { if (m_autoCqPauseSec != v) { m_autoCqPauseSec = qBound(0, v, 300); emit autoCqPauseSecChanged(); } }
    bool avgDecodeEnabled()  const { return m_avgDecodeEnabled; }
    void setAvgDecodeEnabled(bool v){ if (m_avgDecodeEnabled != v) { m_avgDecodeEnabled = v; emit avgDecodeEnabledChanged(); } }
    int  txPeriod()          const { return m_txPeriod; }
    void setTxPeriod(int v);
    bool alt12Enabled()      const { return m_alt12Enabled; }
    void setAlt12Enabled(bool v);
    bool asyncTxEnabled()      const { return m_asyncTxEnabled; }
    void setAsyncTxEnabled(bool v)    { if (m_mode == "FT2") v = true;  /* FT2: sempre attivo */
                                        if (m_asyncTxEnabled != v)      { m_asyncTxEnabled = v;      emit asyncTxEnabledChanged(); } }
    bool asyncDecodeEnabled()  const { return m_asyncDecodeEnabled; }
    void setAsyncDecodeEnabled(bool v){ if (m_asyncDecodeEnabled != v)  { m_asyncDecodeEnabled = v;  emit asyncDecodeEnabledChanged(); } }
    bool dualCarrierEnabled()  const { return m_dualCarrierEnabled; }
    void setDualCarrierEnabled(bool v){ if (m_dualCarrierEnabled!=v){ m_dualCarrierEnabled=v; emit dualCarrierEnabledChanged(); } }
    bool quickQsoEnabled()     const { return m_quickQsoEnabled; }
    void setQuickQsoEnabled(bool v)   { if (m_quickQsoEnabled != v) { m_quickQsoEnabled = v; emit quickQsoEnabledChanged(); } }
    int  asyncSnrDb()          const { return m_asyncSnrDb; }
    void setAsyncSnrDb(int v)         { if (m_asyncSnrDb != v)          { m_asyncSnrDb = v;          emit asyncSnrDbChanged(); } }

    // appEngine migrated Q_PROPERTY declarations (keep together for MOC)
    // (actual getters/setters above; Q_PROPERTYs below for QML binding)

    // appEngine stub properties
    bool swlMode() const { return m_swlMode; }
    void setSwlMode(bool v) { if (m_swlMode!=v){m_swlMode=v;emit swlModeChanged();} }
    bool splitMode() const { return m_splitMode; }
    void setSplitMode(bool v) { if (m_splitMode!=v){m_splitMode=v;emit splitModeChanged();} }
    int txWatchdogMode() const { return m_txWatchdogMode; }
    void setTxWatchdogMode(int v) { if (m_txWatchdogMode!=v){m_txWatchdogMode=v;emit txWatchdogModeChanged();} }
    int txWatchdogTime() const { return m_txWatchdogTime; }
    void setTxWatchdogTime(int v) { if (m_txWatchdogTime!=v){m_txWatchdogTime=v;emit txWatchdogTimeChanged();} }
    int txWatchdogCount() const { return m_txWatchdogCount; }
    void setTxWatchdogCount(int v) { if (m_txWatchdogCount!=v){m_txWatchdogCount=v;emit txWatchdogCountChanged();} }
    bool filterCqOnly() const { return m_filterCqOnly; }
    void setFilterCqOnly(bool v) { if (m_filterCqOnly!=v){m_filterCqOnly=v;emit filterCqOnlyChanged();} }
    bool filterMyCallOnly() const { return m_filterMyCallOnly; }
    void setFilterMyCallOnly(bool v) { if (m_filterMyCallOnly!=v){m_filterMyCallOnly=v;emit filterMyCallOnlyChanged();} }
    int contestType() const { return m_contestType; }
    void setContestType(int v) { if (m_contestType!=v){m_contestType=v;emit contestTypeChanged();} }
    bool zapEnabled() const { return m_zapEnabled; }
    void setZapEnabled(bool v) { if (m_zapEnabled!=v){m_zapEnabled=v;emit zapEnabledChanged();} }
    bool deepSearchEnabled() const { return m_deepSearchEnabled; }
    void setDeepSearchEnabled(bool v) { if (m_deepSearchEnabled!=v){m_deepSearchEnabled=v;emit deepSearchEnabledChanged();} }

    // CAT
    bool catConnected() const;
    QString catRigName() const;
    QString catMode() const;

    // LED status
    bool   ledCoherentAveraging() const { return m_ledCoherentAveraging; }
    bool   ledNeuralSync()        const { return m_ledNeuralSync; }
    bool   ledTurboFeedback()     const { return m_ledTurboFeedback; }
    int    coherentCount()        const { return m_coherentCount; }
    double neuralScore()          const { return m_neuralScore; }
    int    turboIterations()      const { return m_turboIterations; }
    int    ftThreads()            const { return 3; }  // FT8+FT2+FT4 workers

    // PSK Reporter
    bool        pskSearchFound()       const { return m_pskSearchFound; }
    QString     pskSearchCallsign()    const { return m_pskSearchCallsign; }
    bool        pskSearching()         const { return m_pskSearching; }
    QStringList pskSearchBands()       const { return m_pskSearchBands; }
    bool        pskReporterEnabled()   const { return m_pskReporterEnabled; }
    void setPskReporterEnabled(bool v)       { if (m_pskReporterEnabled!=v){m_pskReporterEnabled=v;emit pskReporterEnabledChanged();} }
    bool        pskReporterConnected() const;

    // Theme
    DecodiumThemeManager* themeManager() const { return m_themeManager; }
    double fontScale() const { return m_fontScale; }

    // Sub-managers
    WavManager*          wavManager()   const { return m_wavManager; }
    MacroManager*        macroManager() const { return m_macroManager; }
    BandManager*         bandManager()  const { return m_bandManager; }
    QObject*                     catManagerObj() const {
        if (m_catBackend == "native")   return (QObject*)m_nativeCat;
        if (m_catBackend == "omnirig")  return (QObject*)m_omniRigCat;
        return (QObject*)m_hamlibCat;
    }
    QString                      catBackend()    const { return m_catBackend; }
    void                         setCatBackend(const QString& v);
    DecodiumCatManager*          nativeCat()     const { return m_nativeCat; }
    DecodiumOmniRigManager*      omniRigCat()    const { return m_omniRigCat; }
    DecodiumTransceiverManager*  hamlibCat()     const { return m_hamlibCat; }
    DecodiumDxCluster*   dxCluster()    const { return m_dxCluster; }
    bool wsprUploadEnabled() const { return m_wsprUploadEnabled; }
    void setWsprUploadEnabled(bool v) { if (m_wsprUploadEnabled!=v){m_wsprUploadEnabled=v;emit wsprUploadEnabledChanged();} }

    // UI persistence getters/setters
    int    uiSpectrumHeight()  const { return m_uiSpectrumHeight; }
    void   setUiSpectrumHeight(int v)   { if(m_uiSpectrumHeight!=v){m_uiSpectrumHeight=v;emit uiSpectrumHeightChanged();} }
    int    uiPaletteIndex()    const { return m_uiPaletteIndex; }
    void   setUiPaletteIndex(int v);
    double uiZoomFactor()      const { return m_uiZoomFactor; }
    void   setUiZoomFactor(double v)    { if(m_uiZoomFactor!=v){m_uiZoomFactor=v;emit uiZoomFactorChanged();} }
    int    uiWaterfallHeight() const { return m_uiWaterfallHeight; }
    void   setUiWaterfallHeight(int v)  { if(m_uiWaterfallHeight!=v){m_uiWaterfallHeight=v;emit uiWaterfallHeightChanged();} }
    int    uiDecodeWinX()      const { return m_uiDecodeWinX; }
    void   setUiDecodeWinX(int v)       { if(m_uiDecodeWinX!=v){m_uiDecodeWinX=v;emit uiDecodeWinXChanged();} }
    int    uiDecodeWinY()      const { return m_uiDecodeWinY; }
    void   setUiDecodeWinY(int v)       { if(m_uiDecodeWinY!=v){m_uiDecodeWinY=v;emit uiDecodeWinYChanged();} }
    int    uiDecodeWinWidth()  const { return m_uiDecodeWinWidth; }
    void   setUiDecodeWinWidth(int v)   { if(m_uiDecodeWinWidth!=v){m_uiDecodeWinWidth=v;emit uiDecodeWinWidthChanged();} }
    int    uiDecodeWinHeight() const { return m_uiDecodeWinHeight; }
    void   setUiDecodeWinHeight(int v)  { if(m_uiDecodeWinHeight!=v){m_uiDecodeWinHeight=v;emit uiDecodeWinHeightChanged();} }

    // appEngine-migrated properties (stub — saved in settings)
    bool    alertOnCq()      const { return m_alertOnCq; }
    void    setAlertOnCq(bool v)    { if (m_alertOnCq!=v){m_alertOnCq=v;emit alertOnCqChanged();} }
    bool    alertOnMyCall()  const { return m_alertOnMyCall; }
    void    setAlertOnMyCall(bool v){ if (m_alertOnMyCall!=v){m_alertOnMyCall=v;emit alertOnMyCallChanged();} }
    bool    recordRxEnabled()const;
    void    setRecordRxEnabled(bool v);
    bool    recordTxEnabled()const { return m_recordTxEnabled; }
    void    setRecordTxEnabled(bool v){ if (m_recordTxEnabled!=v){m_recordTxEnabled=v;emit recordTxEnabledChanged();} }
    QString stationName() const { return m_stationName; }
    void    setStationName(const QString& v){ if (m_stationName!=v){m_stationName=v;emit stationNameChanged();} }
    QString stationQth() const { return m_stationQth; }
    void    setStationQth(const QString& v){ if (m_stationQth!=v){m_stationQth=v;emit stationQthChanged();} }
    QString stationRigInfo() const { return m_stationRigInfo; }
    void    setStationRigInfo(const QString& v){ if (m_stationRigInfo!=v){m_stationRigInfo=v;emit stationRigInfoChanged();} }
    QString stationAntenna() const { return m_stationAntenna; }
    void    setStationAntenna(const QString& v){ if (m_stationAntenna!=v){m_stationAntenna=v;emit stationAntennaChanged();} }
    int     stationPowerWatts() const { return m_stationPowerWatts; }
    void    setStationPowerWatts(int v){ v = qBound(0, v, 9999); if (m_stationPowerWatts!=v){m_stationPowerWatts=v;emit stationPowerWattsChanged();} }
    bool    autoStartMonitorOnStartup() const { return m_autoStartMonitorOnStartup; }
    void    setAutoStartMonitorOnStartup(bool v){ if (m_autoStartMonitorOnStartup!=v){m_autoStartMonitorOnStartup=v;emit autoStartMonitorOnStartupChanged();} }
    bool    startFromTx2()   const { return m_startFromTx2; }
    void    setStartFromTx2(bool v) { if (m_startFromTx2!=v){m_startFromTx2=v;emit startFromTx2Changed();} }
    bool    vhfUhfFeatures() const { return m_vhfUhfFeatures; }
    void    setVhfUhfFeatures(bool v){ if (m_vhfUhfFeatures!=v){m_vhfUhfFeatures=v;emit vhfUhfFeaturesChanged();} }
    bool    directLogQso()   const { return m_directLogQso; }
    void    setDirectLogQso(bool v) { if (m_directLogQso!=v){m_directLogQso=v;emit directLogQsoChanged();} }
    bool    confirm73()      const { return m_confirm73; }
    void    setConfirm73(bool v)    { if (m_confirm73!=v){m_confirm73=v;emit confirm73Changed();} }
    QString contestExchange()const { return m_contestExchange; }
    void    setContestExchange(const QString& v){ if (m_contestExchange!=v){m_contestExchange=v;emit contestExchangeChanged();} }
    int     contestNumber()  const { return m_contestNumber; }
    void    setContestNumber(int v) { if (m_contestNumber!=v){m_contestNumber=v;emit contestNumberChanged();} }
    QStringList contestTypeNames() const {
        return {"None","ARRL DX","CQ WW","WAE","IARU HF","DARC","FD","SOTA","Custom"};
    }
    QString logAllTxtPath()  const; // implementato in .cpp

    // Settings
    int nfa() const; void setNfa(int);
    int nfb() const; void setNfb(int);
    int ndepth() const; void setNdepth(int);
    int ncontest() const; void setNcontest(int);

    // Spectrum
    int    spectrumColorPalette() const { return m_spectrumColorPalette; }
    void   setSpectrumColorPalette(int v) { if (m_spectrumColorPalette!=v){m_spectrumColorPalette=v;emit spectrumColorPaletteChanged();} }
    double spectrumRefLevel()    const { return m_spectrumRefLevel; }
    void   setSpectrumRefLevel(double v) { if (m_spectrumRefLevel!=v){m_spectrumRefLevel=v;emit spectrumRefLevelChanged();} }
    double spectrumDynRange()    const { return m_spectrumDynRange; }
    void   setSpectrumDynRange(double v) { if (m_spectrumDynRange!=v){m_spectrumDynRange=v;emit spectrumDynRangeChanged();} }
    bool   spectrumVisible()     const { return m_spectrumVisible; }
    void   setSpectrumVisible(bool v) { if (m_spectrumVisible!=v){m_spectrumVisible=v;emit spectrumVisibleChanged();} }

    // B6 — cty.dat
    bool ctyDatUpdating() const { return m_ctyDatUpdating; }

    // B7 — Color highlighting
    QString colorCQ()       const { return m_colorCQ; }
    void setColorCQ(const QString& v)       { if (m_colorCQ!=v){m_colorCQ=v;emit colorCQChanged();} }
    QString colorMyCall()   const { return m_colorMyCall; }
    void setColorMyCall(const QString& v)   { if (m_colorMyCall!=v){m_colorMyCall=v;emit colorMyCallChanged();} }
    QString colorDXEntity() const { return m_colorDXEntity; }
    void setColorDXEntity(const QString& v) { if (m_colorDXEntity!=v){m_colorDXEntity=v;emit colorDXEntityChanged();} }
    QString color73()       const { return m_color73; }
    void setColor73(const QString& v)       { if (m_color73!=v){m_color73=v;emit color73Changed();} }
    QString colorB4()       const { return m_colorB4; }
    void setColorB4(const QString& v)       { if (m_colorB4!=v){m_colorB4=v;emit colorB4Changed();} }
    bool b4Strikethrough()  const { return m_b4Strikethrough; }
    void setB4Strikethrough(bool v) { if (m_b4Strikethrough!=v){m_b4Strikethrough=v;emit b4StrikethroughChanged();} }

    // B8 — Alert sounds
    bool alertSoundsEnabled() const { return m_alertSoundsEnabled; }
    void setAlertSoundsEnabled(bool v) { if (m_alertSoundsEnabled!=v){m_alertSoundsEnabled=v;emit alertSoundsEnabledChanged();} }

    // C16 — Update checker
    bool    updateAvailable() const { return m_updateAvailable; }
    QString latestVersion()   const { return m_latestVersion; }

    // A2 — Soundcard Drift
    double  soundcardDriftPpm() const { return m_soundcardDriftPpm; }
    double  soundcardDriftMsPerPeriod() const {
        return m_soundcardDriftPpm * periodMsForMode(m_mode) / 1.0e6;
    }
    QString driftSeverity() const {
        double a = qAbs(m_soundcardDriftPpm);
        if (a < 10.0) return QStringLiteral("green");
        if (a < 50.0) return QStringLiteral("yellow");
        return QStringLiteral("red");
    }

    // A3 — Time Sync
    bool   ntpEnabled()      const { return m_ntpEnabled; }
    double ntpOffsetMs()     const { return m_ntpOffsetMs; }
    bool   ntpSynced()       const { return m_ntpSynced; }
    double avgDt()           const { return m_avgDt; }
    double decodeLatencyMs() const { return m_decodeLatencyMs; }
    int    timeSyncSampleCount() const { return m_dtLastSampleCount; }

    // A4 — Fox/Hound
    bool        foxMode()        const { return m_foxMode; }
    void        setFoxMode(bool v)   { if (m_foxMode!=v){m_foxMode=v; emit foxModeChanged();} }
    bool        houndMode()      const { return m_houndMode; }
    void        setHoundMode(bool v) { if (m_houndMode!=v){m_houndMode=v; emit houndModeChanged();} }
    QStringList callerQueue()    const { return m_callerQueue; }
    int         callerQueueSize()const { return m_callerQueue.size(); }
    QObject*    logManager() { return this; }
    QObject*    propagationManager() const;
    int         qsoCount() const;

    // B9 — Active Stations
    QObject* activeStations() const;

    // Modes
    Q_INVOKABLE QStringList availableModes() const;

public slots:
    // Monitor / TX
    Q_INVOKABLE void startRx();
    Q_INVOKABLE void stopRx();
    Q_INVOKABLE void startMonitor() { startRx(); }
    Q_INVOKABLE void stopMonitor()  { stopRx(); }
    Q_INVOKABLE void startTx();
    Q_INVOKABLE void stopTx();
    Q_INVOKABLE void triggerManualTx() { startTx(); }  // PTT manuale FT2
    Q_INVOKABLE void sendTx(int n);    // usato da TxPanel: seleziona messaggio n e trasmette
    Q_INVOKABLE void clearTxMessages();
    Q_INVOKABLE void startTune();      // tono continuo fino a stopTune()
    Q_INVOKABLE void stopTune();
    Q_INVOKABLE void halt();           // ferma TX e Tune immediatamente
    Q_INVOKABLE void logQso();
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE void advanceQsoState(int txNum); // GitHub TxController clone

    // Audio
    Q_INVOKABLE void refreshAudioDevices();
    Q_INVOKABLE void setTxAudioFreqFromClick(int f) { setTxFrequency(f); }
    Q_INVOKABLE void setRxAudioFreqFromClick(int f) { setRxFrequency(f); }
    Q_INVOKABLE QVariantMap worldClockSnapshot(const QString& timeZoneId) const;

    // DX Cluster
    Q_INVOKABLE void connectDxCluster();
    Q_INVOKABLE void connectDxCluster(const QString& host, int port);
    Q_INVOKABLE void disconnectDxCluster();
    bool dxClusterConnected() const { return m_dxCluster && m_dxCluster->connected(); }
    QVariantList dxClusterSpots() const { return m_dxCluster ? m_dxCluster->spots() : QVariantList{}; }
    QString dxClusterHost() const { return m_dxCluster ? m_dxCluster->host() : QString{}; }
    int dxClusterPort() const { return m_dxCluster ? m_dxCluster->port() : 8000; }
    void setDxClusterHost(const QString& h) { if (m_dxCluster) { m_dxCluster->setHost(h); emit dxClusterHostChanged(); } }
    void setDxClusterPort(int p) { if (m_dxCluster) { m_dxCluster->setPort(p); emit dxClusterPortChanged(); } }

    // Decode
    Q_INVOKABLE void clearDecodeList();
    Q_INVOKABLE void clearDecodes() { clearDecodeList(); }
    Q_INVOKABLE void clearRxDecodes();
    Q_INVOKABLE void processDecodeDoubleClick(const QString& message, const QString& timeStr,
                                              const QString& db, int audioFreq);

    // WAV
    Q_INVOKABLE void openWavForDecode(const QString& path);
    Q_INVOKABLE void openWavFolderDecode(const QString& folderPath);

    // CAT
    Q_INVOKABLE void openSetupSettings(int tabIndex = -1);
    Q_INVOKABLE void openTimeSyncSettings();
    Q_INVOKABLE void syncNtpNow();
    Q_INVOKABLE void openCatSettings();
    Q_INVOKABLE void retryRigConnection();

    // PSK Reporter
    Q_INVOKABLE void searchPskReporter(const QString& callsign);
    Q_INVOKABLE void sendPskReporterNow();

    // LED
    Q_INVOKABLE void refreshLedStatus() {}
    Q_INVOKABLE void resetLedStatus();

    // Diagnostic log
    Q_INVOKABLE QString diagnosticLogPath() const;
    Q_INVOKABLE void openDiagnosticLog() const;

    // Font scale
    Q_INVOKABLE void increaseFontScale();
    Q_INVOKABLE void decreaseFontScale();
    Q_INVOKABLE void setFontScale(double s);

    // B6 — cty.dat / CALL3.TXT update
    Q_INVOKABLE void checkCtyDatUpdate();
    Q_INVOKABLE void downloadCall3Txt();

    // B8 — Alert sounds
    Q_INVOKABLE void playAlert(const QString& alertType);

    // B11 — Cabrillo export
    Q_INVOKABLE bool exportCabrillo(const QString& filename);

    // C15 — QSY
    Q_INVOKABLE void qsyTo(double freqHz, const QString& newMode);

    // C16 — Update checker
    Q_INVOKABLE void checkForUpdates();

    // C17 — OTP Fox verification
    Q_INVOKABLE void verifyOtp(const QString& callsign, const QString& code, int audioHz);

    // A4 — Fox/Hound queue management
    Q_INVOKABLE void enqueueStation(const QString& call);
    Q_INVOKABLE void dequeueStation(const QString& call);
    Q_INVOKABLE void clearCallerQueue();

    // C13 — Grid distance/bearing
    Q_INVOKABLE double calcDistance(const QString& myGrid, const QString& dxGrid) const;
    Q_INVOKABLE double calcBearing(const QString& myGrid, const QString& dxGrid) const;

    // C14 — Grid to lat/lon (per AstroPanel)
    Q_INVOKABLE double latFromGrid(const QString& grid) const;
    Q_INVOKABLE double lonFromGrid(const QString& grid) const;

    // Settings
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE QVariant getSetting(const QString& key, const QVariant& defaultValue = {}) const;
    Q_INVOKABLE void setSetting(const QString& key, const QVariant& value);
    Q_INVOKABLE int remoteWebSocketPort() const;
    Q_INVOKABLE QStringList networkInterfaceNames() const;
    Q_INVOKABLE QString udpInterfaceName() const;
    Q_INVOKABLE void setUdpInterfaceName(const QString& name);
    Q_INVOKABLE QVariantMap loadWindowState(const QString& key) const;
    Q_INVOKABLE void saveWindowState(const QString& key,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     bool detached,
                                     bool minimized);
    Q_INVOKABLE QString version() const;

    // ADIF import/export
    Q_INVOKABLE bool exportAdif(const QString& filename);
    Q_INVOKABLE bool importAdif(const QString& filename);
    Q_INVOKABLE QVariantList searchQsos(const QString& search,
                                        const QString& band,
                                        const QString& mode,
                                        const QString& fromDate,
                                        const QString& toDate) const;
    Q_INVOKABLE QVariantMap getQsoStats() const;
    Q_INVOKABLE int importFromAdif(const QString& filename);
    Q_INVOKABLE bool exportToAdif(const QString& filename);
    Q_INVOKABLE bool deleteQso(const QString& call, const QString& dateTime);
    Q_INVOKABLE bool editQso(const QString& call, const QString& dateTime, const QVariantMap& newData);
    Q_INVOKABLE QStringList workedCallsigns() const;
    int workedCount() const;

    // LotW lite
    bool lotwEnabled()  const { return m_lotwEnabled; }
    void setLotwEnabled(bool v) { if (m_lotwEnabled!=v){m_lotwEnabled=v;emit lotwEnabledChanged();} }
    bool lotwUpdating() const { return m_lotwUpdating; }
    Q_INVOKABLE bool isLotwUser(const QString& call) const;
    Q_INVOKABLE void updateLotwUsers();

    // Cloudlog
    bool    cloudlogEnabled() const { return m_cloudlogEnabled; }
    void    setCloudlogEnabled(bool v)       { if (m_cloudlogEnabled!=v){m_cloudlogEnabled=v;emit cloudlogEnabledChanged();} }
    QString cloudlogUrl()      const { return m_cloudlogUrl; }
    void    setCloudlogUrl(const QString& v) { if (m_cloudlogUrl!=v){m_cloudlogUrl=v;emit cloudlogUrlChanged();} }
    QString cloudlogApiKey()   const { return m_cloudlogApiKey; }
    void    setCloudlogApiKey(const QString& v) { if (m_cloudlogApiKey!=v){m_cloudlogApiKey=v;emit cloudlogApiKeyChanged();} }
    Q_INVOKABLE void testCloudlogApi();

signals:
    void spectrumDataReady(QVector<float> data);
    // Alta risoluzione: dB raw + range + frequenze exact — per PanadapterItem
    void panadapterDataReady(QVector<float> dbValues, float minDb, float maxDb,
                             float freqMinHz, float freqMaxHz);
    // TX — collegano bridge → Modulator (via QueuedConnection)
    void transmitFrequency(double freq);
    void sendMessage(QString mode, unsigned symbolsLength, double framesPerSymbol,
                     double frequency, double toneSpacing, SoundOutput*,
                     int channel, bool synchronize, bool fastMode,
                     double dBSNR, double TRperiod);
    void endTransmitMessage(bool quick = false);
    void sendPrecomputedWave(QString mode, QVector<float> wave);
    void callsignChanged();
    void gridChanged();
    void frequencyChanged();
    void modeChanged();
    void monitoringChanged();
    void transmittingChanged();
    void tuningChanged();
    void decodingChanged();
    void rxFrequencyChanged();
    void txFrequencyChanged();
    void audioLevelChanged();
    void sMeterChanged();
    void rxInputLevelChanged();
    void txOutputLevelChanged();
    void audioInputDevicesChanged();
    void audioOutputDevicesChanged();
    void audioInputDeviceChanged();
    void audioOutputDeviceChanged();
    void audioInputChannelChanged();
    void audioOutputChannelChanged();
    void decodeListChanged();
    void rxDecodeListChanged();
    void periodProgressChanged();
    void utcTimeChanged();
    void tx1Changed(); void tx2Changed(); void tx3Changed();
    void tx4Changed(); void tx5Changed(); void tx6Changed();
    void txMessagesChanged();
    void currentTxChanged();
    void currentTxMessageChanged();
    void dxCallChanged(); void dxGridChanged();
    void qsoProgressChanged();
    void reportSentChanged();
    void reportReceivedChanged();
    void sendRR73Changed();
    void multiAnswerModeChanged();
    void autoSeqChanged();
    void txEnabledChanged();
    void autoCqRepeatChanged();
    void maxCallerRetriesChanged();
    void autoCqMaxCyclesChanged();
    void autoCqPauseSecChanged();
    void avgDecodeEnabledChanged();
    void txPeriodChanged();
    void alt12EnabledChanged();
    void asyncTxEnabledChanged();
    void dualCarrierEnabledChanged();
    void quickQsoEnabledChanged();
    void asyncSnrDbChanged();
    void catConnectedChanged();
    void catRigNameChanged();
    void catModeChanged();
    void dxClusterConnectedChanged();
    void dxClusterSpotsChanged();
    void dxClusterHostChanged();
    void dxClusterPortChanged();
    void ledCoherentAveragingChanged();
    void ledNeuralSyncChanged();
    void ledTurboFeedbackChanged();
    void coherentCountChanged();
    void pskSearchFoundChanged();
    void pskSearchCallsignChanged();
    void pskSearchingChanged();
    void pskSearchBandsChanged();
    void fontScaleChanged();
    void nfaChanged(); void nfbChanged();
    void ndepthChanged(); void ncontestChanged();
    void spectrumColorPaletteChanged();
    void spectrumRefLevelChanged();
    void spectrumDynRangeChanged();
    void spectrumVisibleChanged();
    void statusMessage(const QString& msg);
    void errorMessage(const QString& msg);
    void warningRaised(const QString& title, const QString& summary, const QString& details);
    void setupSettingsRequested(int tabIndex);
    void timeSyncSettingsRequested();
    void catSettingsRequested();
    void rigErrorRaised(const QString& title, const QString& summary, const QString& details);
    // B6 — cty.dat
    void ctyDatUpdatingChanged();
    // B7 — Color highlighting
    void colorCQChanged();
    void colorMyCallChanged();
    void colorDXEntityChanged();
    void color73Changed();
    void colorB4Changed();
    void b4StrikethroughChanged();
    // B8 — Alert sounds
    void alertSoundsEnabledChanged();
    // C16 — Update checker
    void updateAvailableChanged();
    void latestVersionChanged();
    // A1 — Decoder watchdog
    void decoderWatchdogTriggered();
    // A2 — Soundcard drift
    void soundcardDriftChanged();
    // A3 — Time sync
    void ntpEnabledChanged();
    void ntpOffsetMsChanged();
    void ntpSyncedChanged();
    void avgDtChanged();
    void decodeLatencyMsChanged();
    void timeSyncSampleCountChanged();
    // A4 — Fox/Hound
    void foxModeChanged();
    void houndModeChanged();
    void callerQueueChanged();
    // appEngine stub signals
    void swlModeChanged();
    void splitModeChanged();
    void txWatchdogModeChanged();
    void txWatchdogTimeChanged();
    void txWatchdogCountChanged();
    void filterCqOnlyChanged();
    void filterMyCallOnlyChanged();
    void contestTypeChanged();
    void zapEnabledChanged();
    void deepSearchEnabledChanged();
    // new stub signals
    void asyncDecodeEnabledChanged();
    void neuralScoreChanged();
    void turboIterationsChanged();
    void alertOnCqChanged();
    void alertOnMyCallChanged();
    void recordRxEnabledChanged();
    void recordTxEnabledChanged();
    void stationNameChanged();
    void stationQthChanged();
    void stationRigInfoChanged();
    void stationAntennaChanged();
    void stationPowerWattsChanged();
    void autoStartMonitorOnStartupChanged();
    void startFromTx2Changed();
    void vhfUhfFeaturesChanged();
    void directLogQsoChanged();
    void confirm73Changed();
    void contestExchangeChanged();
    void contestNumberChanged();
    void contestTypeNamesChanged();
    void logAllTxtPathChanged();
    void pskReporterEnabledChanged();
    void pskReporterConnectedChanged();
    // ADIF / LotW / Cloudlog
    void qsoCountChanged();
    void workedCountChanged();
    void lotwEnabledChanged();
    void lotwUpdatingChanged();
    void cloudlogEnabledChanged();
    void cloudlogUrlChanged();
    void cloudlogApiKeyChanged();
    void wsprUploadEnabledChanged();
    void catManagerChanged();
    void catBackendChanged();
    void uiSpectrumHeightChanged();
    void uiPaletteIndexChanged();
    void uiZoomFactorChanged();
    void uiWaterfallHeightChanged();
    void uiDecodeWinXChanged();
    void uiDecodeWinYChanged();
    void uiDecodeWinWidthChanged();
    void uiDecodeWinHeightChanged();

private slots:
    void onFt8DecodeReady(quint64 serial, QStringList rows);
    void onFt2DecodeReady(quint64 serial, QStringList rows);
    void onFt2AsyncDecodeReady(QStringList rows);   // path async 100ms
    void onFt4DecodeReady(quint64 serial, QStringList rows);
    void onQ65DecodeReady(quint64 serial, QStringList rows);
    void onMsk144DecodeReady(quint64 serial, QStringList rows);
    void onWsprDecodeReady(quint64 serial, QStringList rows, QStringList diagnostics, int exitCode);
    void onLegacyJtDecodeReady(quint64 serial, QStringList rows);
    void onFst4DecodeReady(quint64 serial, QStringList rows);
    void onPeriodTimer();
    void onUtcTimer();
    void onSpectrumTimer();
    void onLegacyWaterfallRow(QByteArray const& rowLevels,
                              int startFrequencyHz,
                              int spanHz,
                              int rxFrequencyHz,
                              int txFrequencyHz,
                              QString const& mode);
    void onAsyncDecodeTimer();   // FT2 turbo async ogni 100ms
    void regenerateTxMessages();  // auto-genera TX6 (CQ) e TX1-5 da callsign/grid/dxCall
    void processNextInQueue();   // mainwindow processNextInQueue: auto-handoff al prossimo caller

private:
    bool isTimeSyncDecodeMode(const QString& mode) const;
    void applyNtpSettings();
    void configureNtpClientForMode(const QString& mode);
    void resetStartupTransientQsoState();
    void purgePersistentTransientQsoState();
    void resetTimeSyncDecodeMetrics();
    void finalizeTimeSyncDecodeCycle(quint64 serial, const QString& decodeMode,
                                     const QVector<double>& dtSamples);
    bool shouldTrackDtSample(int snr, double dtValue, const QString& message,
                             const QString& decodeMode) const;
    QString autoCqBandKeyForFrequency(double freqHz) const;
    QString startupModeForFrequency(double dialFrequency) const;
    void maybeApplyStartupModeFromRigFrequency(double dialFrequency);
    QString effectiveAdifLogPath() const;
    QString ensureAdifLogPath();
    bool isRecentAutoCqDuplicate(const QString& call,
                                 double freqHz = -1.0,
                                 const QString& mode = QString()) const;
    void rememberRecentAutoCqAbandoned(const QString& call, double freqHz, const QString& mode);
    void rememberRecentAutoCqWorked(const QString& call, double freqHz, const QString& mode);
    void removeCallerFromQueue(const QString& call);
    QString inferredPartnerForAutolog() const;
    void capturePendingAutoLogSnapshot();
    void clearPendingAutoLogSnapshot();
    void armLateAutoLogSnapshot();
    void clearLateAutoLogSnapshot();
    void engageManualTxHold(const QString& reason, bool clearQueue = false);
    void clearManualTxHold(const QString& reason);
    void clearAutoCqPartnerLock();
    void updateAutoCqPartnerLock();
    void restoreAutoCqPartnerLock();
    void enqueueCallerInternal(const QString& call, int freq = -1, int snr = -99);

    // Legacy INI sync helpers for UDP settings
    QString legacyIniPath() const;
    QString legacyConfigGroupName() const;
    bool isLegacySyncKey(const QString& key) const;
    void syncSettingToLegacyIni(const QString& key, const QVariant& value);
    QVariant readSettingFromLegacyIni(const QString& key) const;

    // Standalone UDP MessageClient for WSJT-X protocol
    void initUdpMessageClient();
    void udpSendStatus();
    void udpSendDecode(bool isNew, const QString& rawLine, quint64 serial);

    QString m_callsign {"IU8LMC"};
    QString m_grid {"JN70"};
    double m_frequency {14074000.0};
    QString m_mode {"FT2"};
    bool m_startupModeAutoPending {true};
    qint64 m_startupModeAutoUntilMs {0};
    bool m_preserveFrequencyOnModeChange {false};
    bool m_shuttingDown {false};
    bool m_lastSuccessfulCatConnected {false};
    QString m_lastSuccessfulCatBackend;
    bool m_monitoring {false};
    bool m_transmitting {false};
    bool m_tuning {false};
    bool m_decoding {false};
    int m_rxFrequency {1500};
    int m_txFrequency {1500};
    double m_audioLevel {0.0};
    double m_sMeter {0.0};
    double m_rxInputLevel {50.0};
    double m_txOutputLevel {0.0};
    QStringList m_audioInputDevices;
    QStringList m_audioOutputDevices;
    QString m_audioInputDevice;
    QString m_audioOutputDevice;
    int m_audioInputChannel {0};
    int m_audioOutputChannel {0};
    QVariantList m_decodeList;
    QVariantList m_rxDecodeList;
    int m_periodProgress {0};
    QString m_utcTime;
    QString m_tx1, m_tx2, m_tx3, m_tx4, m_tx5, m_tx6;
    int m_currentTx {1};
    QString m_dxCall, m_dxGrid;
    int m_qsoProgress {0};
    QString m_reportSent;
    QString m_reportReceived {"-10"};  // report ricevuto dal partner (default -10 dB)
    bool    m_sendRR73 {true};         // true=RR73, false=RRR (come mainwindow m_sendRR73)
    int     m_autoCQPeriodsMissed {0}; // periodi CQ senza risposta (watchdog count-based)
    bool m_multiAnswerMode {false};
    bool m_autoSeq          {true};
    bool m_txEnabled        {false};
    bool m_manualTxHold     {false};
    bool m_autoCqRepeat     {false};
    bool m_avgDecodeEnabled {false};
    int  m_txPeriod         {0};   // 0=even periods, 1=odd periods
    bool m_alt12Enabled     {false};
    bool    m_asyncTxEnabled   {false};  // FT2 async TX (no periodo sync, come GitHub cbAsyncDecode)
    qint64  m_asyncLastTxEndMs {0};      // timestamp fine ultima TX FT2 async (per guard timer)
    bool m_dualCarrierEnabled{false}; // FT2 dual carrier mode
    bool m_quickQsoEnabled   {false}; // FT2 Quick QSO: salta TX1, flusso Ultra2 (2 messaggi)
    int  m_asyncSnrDb          {-99};   // SNR ultimo decode FT2 (per AsyncModeWidget S-meter)
    // FT2 QSO cooldown: evita re-triggering sullo stesso 73 decodificato ogni ~4s (Shannon m_qsoCooldown)
    QMap<QString, qint64> m_qsoCooldown;  // callsign → timestamp msec UTC
    bool m_catConnected {false};
    QString m_catRigName;
    QString m_catMode;
    bool   m_ledCoherentAveraging {false};
    bool   m_ledNeuralSync {false};
    bool   m_ledTurboFeedback {false};
    int    m_coherentCount {0};
    double m_neuralScore {0.0};
    int    m_turboIterations {0};
    bool        m_pskSearchFound {false};
    QString     m_pskSearchCallsign;
    bool        m_pskSearching {false};
    QStringList m_pskSearchBands {"160m","80m","40m","20m","15m","10m"};
    bool        m_pskReporterEnabled {false};
    double m_fontScale {1.0};
    int m_nfa {200}, m_nfb {4000}, m_ndepth {1}, m_ncontest {0};

    // Spectrum/Waterfall settings
    int    m_spectrumColorPalette {0};
    double m_spectrumRefLevel {-10.0};
    double m_spectrumDynRange {70.0};
    bool   m_spectrumVisible {true};

    DecodiumThemeManager* m_themeManager  {nullptr};
    DecodiumPropagationManager* m_propagationManager {nullptr};
    WavManager*           m_wavManager    {nullptr};
    MacroManager*         m_macroManager  {nullptr};
    BandManager*          m_bandManager   {nullptr};
    DecodiumCatManager*           m_nativeCat     {nullptr};
    DecodiumOmniRigManager*       m_omniRigCat    {nullptr};
    DecodiumTransceiverManager*   m_hamlibCat     {nullptr};
    QString                       m_catBackend    {"native"};
    bool                          m_suppressCatErrors {false};
    RemoteCommandServer*          m_remoteServer {nullptr};
    DecodiumDxCluster*    m_dxCluster     {nullptr};
    DecodiumPskReporterLite* m_pskReporter {nullptr};
    DecodiumCloudlogLite*    m_cloudlog    {nullptr};
    DecodiumWsprUploader*    m_wsprUploader{nullptr};
    bool                  m_wsprUploadEnabled {false};

    // UI persistence state
    int    m_uiSpectrumHeight  {150};
    int    m_uiPaletteIndex    {3};
    double m_uiZoomFactor      {1.0};
    int    m_uiWaterfallHeight {350};
    int    m_uiDecodeWinX      {0};
    int    m_uiDecodeWinY      {0};
    int    m_uiDecodeWinWidth  {1100};
    int    m_uiDecodeWinHeight {600};

    QVector<short> m_audioBuffer;
    // Protegge m_audioBuffer rispetto al callback DecodiumAudioSink::writeData,
    // che in alcune piattaforme Qt6 (PulseAudio/ALSA) viene invocato dal thread
    // interno del backend audio anche quando il sink ha parent nel main thread.
    // QMutex perché QVector NON è thread-safe per scritture concorrenti.
    mutable QMutex m_audioBufferMutex;
    quint64 m_decodeSerial {0};
    // Period timer ticks at 250ms; mode determines how many ticks = 1 period
    int m_periodTicks {0};
    int m_periodTicksMax {60};   // FT8=60 (15s), FT4=30 (7.5s), FT2=15 (3.75s)
    static constexpr int TIMER_MS = 250;
    static constexpr int SAMPLE_RATE = 12000;

    QThread* m_workerThread {nullptr};
    decodium::ft8::FT8DecodeWorker*    m_ft8Worker    {nullptr};
    QThread* m_workerThreadFt2 {nullptr};
    decodium::ft2::FT2DecodeWorker*    m_ft2Worker    {nullptr};
    QThread* m_workerThreadFt4 {nullptr};
    decodium::ft4::FT4DecodeWorker*    m_ft4Worker    {nullptr};
    QThread* m_workerThreadQ65 {nullptr};
    decodium::q65::Q65DecodeWorker*    m_q65Worker    {nullptr};
    QThread* m_workerThreadMsk {nullptr};
    decodium::msk144::MSK144DecodeWorker* m_mskWorker {nullptr};
    QThread* m_workerThreadWspr{nullptr};
    decodium::wspr::WSPRDecodeWorker*  m_wsprWorker   {nullptr};
    QThread* m_workerThreadLegacyJt{nullptr};
    decodium::legacyjt::LegacyJtDecodeWorker* m_legacyJtWorker {nullptr};
    QThread* m_workerThreadFst4{nullptr};
    decodium::fst4::FST4DecodeWorker*  m_fst4Worker   {nullptr};

    QTimer* m_periodTimer      {nullptr};
    QTimer* m_utcTimer         {nullptr};
    QTimer* m_spectrumTimer    {nullptr};
    QTimer* m_asyncDecodeTimer {nullptr};   // FT2 turbo async 100ms
    QTimer* m_legacyStateTimer {nullptr};
    SoundInput*        m_soundInput  {nullptr};
    SoundOutput*       m_soundOutput {nullptr};
    Modulator*         m_modulator   {nullptr};
    DecodiumAudioSink* m_audioSink   {nullptr};
    QAudioSink*        m_txAudioSink  {nullptr};
    QBuffer*           m_txPcmBuffer  {nullptr};
    QByteArray         m_txPcmData;
    QTimer*            m_tuneTimer    {nullptr};
    DecodiumLegacyBackend* m_legacyBackend {nullptr};
    bool m_useLegacyTxBackend {false};
    MessageClient* m_udpMessageClient {nullptr};
    int  m_legacyBandActivityRevision {-1};
    int  m_legacyRxFrequencyRevision {-1};

    // === GitHub TxController clone ===
    int  m_nTx73            {0};   // TX5 (73) repeat counter: >=2 → QSO completo, ferma
    int  m_txRetryCount     {0};   // quante volte abbiamo inviato m_lastNtx senza risposta
    int  m_lastNtx          {-1};  // ultimo TX number inviato
    int  m_lastCqPidx       {-1};  // period index dell'ultimo CQ inviato (evita CQ consecutivi)
    QString m_lastAutoSeqKey;      // deduplicazione autoSequenceStep
    qint64  m_lastAutoSeqMs {0};   // timestamp ultima deduplicazione
    QString m_lastTransmittedMessage;
    QDateTime m_qsoStartedOn;
    bool    m_logAfterOwn73 {false};
    bool    m_ft2DeferredLogPending {false};
    bool    m_quickPeerSignaled {false};
    bool    m_qsoLogged {false};   // flag anti-doppio log per QSO corrente
    int  m_maxCallerRetries {3};   // max tentativi TX per step prima di fermarsi
    int  m_autoCqMaxCycles  {0};   // 0 = infinito, >0 = max cicli CQ
    int  m_autoCqPauseSec   {0};   // pausa (s) tra cicli CQ (0 = nessuna pausa)
    int  m_autoCqCycleCount {0};   // contatore cicli CQ corrente
    int  m_txWatchdogTicks  {0};   // tick watchdog a 250ms (240 tick = 60s)
    static constexpr int TX_WATCHDOG_MAX = 240; // 60s @ 250ms

    // appEngine stub members
    bool    m_swlMode {false};
    bool    m_splitMode {false};
    int     m_txWatchdogMode {0};
    int     m_txWatchdogTime {5};
    int     m_txWatchdogCount {3};
    bool    m_filterCqOnly {false};
    bool    m_filterMyCallOnly {false};
    int     m_contestType {0};
    bool    m_zapEnabled {false};
    bool    m_deepSearchEnabled {false};
    bool    m_asyncDecodeEnabled {false};
    bool    m_alertOnCq {false};
    bool    m_alertOnMyCall {false};
    bool    m_recordRxEnabled {false};
    bool    m_recordTxEnabled {false};
    QString m_stationName;
    QString m_stationQth;
    QString m_stationRigInfo;
    QString m_stationAntenna;
    int     m_stationPowerWatts {100};
    bool    m_autoStartMonitorOnStartup {true};
    bool    m_startFromTx2 {false};
    bool    m_vhfUhfFeatures {false};
    bool    m_directLogQso {false};
    bool    m_confirm73 {true};
    QString m_contestExchange;
    int     m_contestNumber {1};
    bool    m_pendingAutoLogValid {false};
    QString m_pendingAutoLogCall;
    QString m_pendingAutoLogGrid;
    QString m_pendingAutoLogRptSent;
    QString m_pendingAutoLogRptRcvd;
    QDateTime m_pendingAutoLogOn;
    double    m_pendingAutoLogDialFreq {0.0};
    bool    m_lateAutoLogValid {false};
    QString m_lateAutoLogCall;
    QString m_lateAutoLogGrid;
    QString m_lateAutoLogRptSent;
    QString m_lateAutoLogRptRcvd;
    QDateTime m_lateAutoLogOn;
    double    m_lateAutoLogDialFreq {0.0};
    QDateTime m_lateAutoLogExpires;
    QString m_autoCqLockedCall;
    QString m_autoCqLockedGrid;
    int     m_autoCqLockedNtx {6};
    int     m_autoCqLockedProgress {0};
    QHash<QString, QDateTime> m_recentAutoCqAbandonedUtcByKey;
    QHash<QString, QDateTime> m_recentAutoCqWorkedUtcByKey;
    QHash<QString, QDateTime> m_recentQsoLogUtcByKey;

    // B6 — cty.dat
    bool   m_ctyDatUpdating {false};

    // B7 — Color highlighting (default WSJT-X colors)
    QString m_colorCQ       {"#33FF33"};
    QString m_colorMyCall   {"#FF5555"};
    QString m_colorDXEntity {"#FFAA33"};
    QString m_color73       {"#5599FF"};
    QString m_colorB4       {"#888888"};
    bool    m_b4Strikethrough {true};

    // B8 — Alert sounds
    bool                 m_alertSoundsEnabled {false};
    DecodiumAlertManager* m_alertManager {nullptr};
    DxccLookup*           m_dxccLookup  {nullptr};

    // C16 — Update checker
    bool    m_updateAvailable {false};
    QString m_latestVersion;

    // A2 — Soundcard drift detection
    double        m_soundcardDriftPpm {0.0};
    qint64        m_driftFrameCount   {0};
    qint64        m_driftExpectedFrames {0};
    QElapsedTimer m_driftClock;

    // A3 — Time sync state
    NtpClient* m_ntpClient      {nullptr};
    bool   m_ntpEnabled         {false};
    QString m_ntpCustomServer;
    double m_ntpOffsetMs    {0.0};
    bool   m_ntpSynced      {false};
    double m_avgDt          {0.0};
    double m_decodeLatencyMs{0.0};
    QHash<quint64, qint64> m_decodeStartMsBySerial;
    QHash<quint64, QString> m_decodeModeBySerial;
    int    m_dtMinSamples   {3};
    int    m_dtLastSampleCount {0};
    int    m_totalDecodesForDt {0};
    double m_dtSmoothFactor {0.5};

    // A4 — Fox/Hound
    bool        m_foxMode   {false};
    bool        m_houndMode {false};
    QStringList m_callerQueue;
    static constexpr int FOX_QUEUE_MAX = 20;

    // B9 — Active Stations model
    ActiveStationsModel* m_activeStations {nullptr};

    // ADIF — log lavorato + import
    QSet<QString>    m_workedCalls;   // callsign già lavorati (B4 check)
    QString          m_adifLogPath;   // path file .adi
    void appendAdifRecord(const QString& dxCall, const QString& dxGrid,
                          double freqHz, const QString& mode, const QDateTime& utc,
                          const QString& rstSent, const QString& rstRcvd);

    // LotW lite
    bool          m_lotwEnabled  {false};
    bool          m_lotwUpdating {false};
    QSet<QString> m_lotwUsers;

    // Cloudlog
    bool    m_cloudlogEnabled {false};
    QString m_cloudlogUrl;
    QString m_cloudlogApiKey;

    // FT2 async ring buffer: ultimi 7.5s di audio (90000 campioni a 12kHz)
    static constexpr int ASYNC_BUF_SIZE = 90000;
    short m_asyncAudio[ASYNC_BUF_SIZE] {};
    // Modificato dal callback audio (DecodiumAudioSink::setSampleCallback) e
    // letto dal timer FT2 (onAsyncDecodeTimer). Atomico per garantire una read
    // coerente dello snapshot della write position; il callback usa fetch_add
    // implicito via operator++ per l'incremento.
    std::atomic<int> m_asyncAudioPos {0};   // posizione assoluta (mai resettata, mod ASYNC_BUF_SIZE)
    bool  m_asyncDecodePending {false};  // previene overlap

    // Spectrum ring buffer — buffer circolare separato per waterfall (non consumato dal decoder)
    static constexpr int WF_RING_SIZE = 16384;  // ~1.37s a 12kHz — sufficiente per FFT 8192
    short m_wfRing[WF_RING_SIZE] {};
    int   m_wfRingPos {0};
    QVector<short> m_spectrumBuf;
    static constexpr int SPECTRUM_FFT_SIZE    = 512;   // legacy WaterfallItem
    static constexpr int PANADAPTER_FFT_SIZE  = 8192;  // massima risoluzione (~1.46 Hz/bin)
    QVector<float> m_lastPanadapterData;   // ultimo spettro valido (evita fasce nere)
    float m_lastPanMinDb {0.f};
    float m_lastPanMaxDb {0.f};
    float m_lastPanFreqMin {0.f};
    float m_lastPanFreqMax {0.f};

    QStringList ctyDatSearchPaths() const;
    bool reloadDxccLookup(QString* loadedPath = nullptr);
    QString extractDecodedCallsign(const QString& msg, bool isCQ) const;
    QString extractDecodedGrid(const QString& msg) const;
    void enrichDecodeEntry(QVariantMap& entry) const;
    void refreshDecodeListDxcc();
    QStringList parseFt8Row(const QString& row) const;
    QStringList parseJt65Row(const QString& row) const;
    void startAudioCapture();
    void stopAudioCapture();
    void feedAudioToDecoder();
    void enumerateAudioDevices();
    void updatePeriodTicksMax();
    QVector<float> computeSpectrum() const;
    QVector<float> computePanadapter(float& outMinDb, float& outMaxDb) const;
    void initTxDevices();
    QString buildCurrentTxMessage() const;
    bool legacyBackendAvailable() const;
    bool ensureLegacyBackendAvailable();
    bool usingLegacyBackendForTx() const;
    bool useModernSpectrumFeedWithLegacy() const;
    void syncLegacyBackendDialogState();
    void syncLegacyBackendTxState();
    void syncLegacyBackendState();
    void scheduleLegacyStateRefreshBurst();
    void migrateActiveMonitoringToLegacyBackend();
    void teardownAudioCapture();
    void reloadBridgeSettingsFromPersistentStore();
    void syncLegacyBackendDecodeList();
    QVariantList mirrorLegacyDecodeLines(QStringList const& lines, bool rxPane) const;
    bool shouldMirrorToRxPane(const QVariantMap& entry) const;
    void appendRxDecodeEntry(const QVariantMap& entry);
    void rebuildRxDecodeList();
    void genStdMsgs(const QString& hisCall, const QString& hisGrid);
    void checkAndStartPeriodicTx();
    void autoSequenceStep(const QStringList& parsedFields);
    static int periodMsForMode(const QString& mode) {
        if (mode=="FT2")      return 3750;
        if (mode=="FT4")      return 7500;
        if (mode=="Q65")      return 60000;
        if (mode=="MSK144")   return 15000;
        if (mode=="WSPR")     return 120000;
        if (mode=="JT65")     return 60000;
        if (mode=="JT9")      return 60000;
        if (mode=="JT4")      return 60000;
        if (mode=="FST4")     return 60000;
        if (mode=="FST4W")    return 120000;
        // C12 — FST4/FST4W multi-period
        if (mode=="FST4-15")  return 15000;
        if (mode=="FST4-30")  return 30000;
        if (mode=="FST4-60")  return 60000;
        if (mode=="FST4-120") return 120000;
        if (mode=="FST4-300") return 300000;
        if (mode=="FST4-900") return 900000;
        if (mode=="FST4W-120") return 120000;
        if (mode=="FST4W-300") return 300000;
        if (mode=="FST4W-900") return 900000;
        return 15000;  // FT8 default
    }
    void updateSoundOutputDevice();
    bool launchTuneAudio();
};
