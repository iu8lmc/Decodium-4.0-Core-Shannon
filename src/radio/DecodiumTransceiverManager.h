#pragma once
// DecodiumTransceiverManager — wrapper QML/QObject attorno a TransceiverFactory.
// Supporta Hamlib (100+ radio), OmniRig, HRD, DXLab Suite Commander, TCI.
// Stessa interfaccia pubblica di DecodiumCatManager + campi aggiuntivi.
#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <memory>

// Forward-declare per nascondere le dipendenze Boost/Hamlib dall'header
struct DecodiumTransceiverManagerPrivate;
class QThread;
class Transceiver;

class DecodiumTransceiverManager : public QObject
{
    Q_OBJECT

    // ── Connessione base ──────────────────────────────────────────────────
    Q_PROPERTY(bool    connected  READ connected  NOTIFY connectedChanged)
    Q_PROPERTY(bool    connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(QString rigName    READ rigName    WRITE setRigName    NOTIFY rigNameChanged)

    // ── Porta seriale ─────────────────────────────────────────────────────
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY serialPortChanged)
    Q_PROPERTY(int     baudRate   READ baudRate   WRITE setBaudRate   NOTIFY baudRateChanged)
    Q_PROPERTY(QString dataBits   READ dataBits   WRITE setDataBits   NOTIFY dataBitsChanged)
    Q_PROPERTY(QString stopBits   READ stopBits   WRITE setStopBits   NOTIFY stopBitsChanged)
    Q_PROPERTY(QString handshake  READ handshake  WRITE setHandshake  NOTIFY handshakeChanged)
    Q_PROPERTY(bool forceDtr      READ forceDtr   WRITE setForceDtr   NOTIFY forceDtrChanged)
    Q_PROPERTY(bool dtrHigh       READ dtrHigh    WRITE setDtrHigh    NOTIFY dtrHighChanged)
    Q_PROPERTY(bool forceRts      READ forceRts   WRITE setForceRts   NOTIFY forceRtsChanged)
    Q_PROPERTY(bool rtsHigh       READ rtsHigh    WRITE setRtsHigh    NOTIFY rtsHighChanged)

    // ── Porta rete / TCI ─────────────────────────────────────────────────
    Q_PROPERTY(QString networkPort READ networkPort WRITE setNetworkPort NOTIFY networkPortChanged)
    Q_PROPERTY(QString tciPort     READ tciPort     WRITE setTciPort     NOTIFY tciPortChanged)

    // ── PTT / Split ───────────────────────────────────────────────────────
    Q_PROPERTY(QString pttMethod  READ pttMethod   WRITE setPttMethod  NOTIFY pttMethodChanged)
    Q_PROPERTY(QString pttPort    READ pttPort     WRITE setPttPort    NOTIFY pttPortChanged)
    Q_PROPERTY(QString splitMode  READ splitMode   WRITE setSplitMode  NOTIFY splitModeChanged)
    Q_PROPERTY(int civAddress     READ civAddress  WRITE setCivAddress NOTIFY civAddressChanged)
    Q_PROPERTY(bool catKeepAlive  READ catKeepAlive WRITE setCatKeepAlive NOTIFY catKeepAliveChanged)
    Q_PROPERTY(int pollInterval   READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)

    // ── Tipo porta (derivato dal rig selezionato) ─────────────────────────
    Q_PROPERTY(QString portType   READ portType   NOTIFY portTypeChanged)  // "serial"|"network"|"usb"|"tci"|"none"

    // ── Stato corrente dal rig ────────────────────────────────────────────
    Q_PROPERTY(double  frequency   READ frequency   NOTIFY frequencyChanged)
    Q_PROPERTY(double  txFrequency READ txFrequency NOTIFY txFrequencyChanged)
    Q_PROPERTY(QString mode        READ mode        NOTIFY modeChanged)
    Q_PROPERTY(bool    pttActive   READ pttActive   NOTIFY pttActiveChanged)
    Q_PROPERTY(bool    split       READ split       NOTIFY splitChanged)
    Q_PROPERTY(double  powerWatts  READ powerWatts  NOTIFY powerWattsChanged)
    // S-meter in ricezione, in dB rispetto a S9 come lo da' Hamlib. Il flag
    // di validita' non e' un lusso: zero su questa scala vuol dire S9, quindi
    // un rig senza S-meter sembrerebbe ricevere un segnale pieno.
    Q_PROPERTY(int     strengthDb  READ strengthDb  NOTIFY strengthChanged)
    Q_PROPERTY(bool    strengthValid READ strengthValid NOTIFY strengthChanged)
    Q_PROPERTY(double  swr         READ swr         NOTIFY swrChanged)
    Q_PROPERTY(double  alc         READ alc         NOTIFY alcChanged)  // 1.0.323 — ALC meter 0..100
    Q_PROPERTY(bool    alcValid    READ alcValid    NOTIFY alcChanged)
    Q_PROPERTY(double  drainVoltage       READ drainVoltage       NOTIFY paMetersChanged)
    Q_PROPERTY(bool    drainVoltageValid  READ drainVoltageValid  NOTIFY paMetersChanged)
    Q_PROPERTY(double  drainCurrent       READ drainCurrent       NOTIFY paMetersChanged)
    Q_PROPERTY(bool    drainCurrentValid  READ drainCurrentValid  NOTIFY paMetersChanged)
    Q_PROPERTY(double  paTemperature      READ paTemperature      NOTIFY paMetersChanged)
    Q_PROPERTY(bool    paTemperatureValid READ paTemperatureValid NOTIFY paMetersChanged)
    Q_PROPERTY(double  compressionDb      READ compressionDb      NOTIFY paMetersChanged)
    Q_PROPERTY(bool    compressionValid   READ compressionValid   NOTIFY paMetersChanged)
    Q_PROPERTY(double  powerSettingPct    READ powerSettingPct    NOTIFY paMetersChanged)
    Q_PROPERTY(bool    powerSettingValid  READ powerSettingValid  NOTIFY paMetersChanged)

    // ── Liste per UI ──────────────────────────────────────────────────────
    Q_PROPERTY(QStringList rigList  READ rigList  NOTIFY rigListChanged)
    Q_PROPERTY(QStringList portList READ portList NOTIFY portListChanged)
    Q_PROPERTY(QStringList baudList READ baudList CONSTANT)
    Q_PROPERTY(QStringList pttMethodList READ pttMethodList CONSTANT)
    Q_PROPERTY(QStringList splitModeList READ splitModeList CONSTANT)

    // ── Comportamenti automatici ──────────────────────────────────────────
    Q_PROPERTY(bool catAutoConnect READ catAutoConnect WRITE setCatAutoConnect NOTIFY catAutoConnectChanged)
    Q_PROPERTY(bool audioAutoStart READ audioAutoStart WRITE setAudioAutoStart NOTIFY audioAutoStartChanged)
    Q_PROPERTY(bool tciAudioEnabled READ tciAudioEnabled WRITE setTciAudioEnabled NOTIFY tciAudioEnabledChanged)
    Q_PROPERTY(double tciRxGainDb READ tciRxGainDb WRITE setTciRxGainDb NOTIFY tciRxGainDbChanged)
    Q_PROPERTY(bool hrdStrictRadioMatch READ hrdStrictRadioMatch WRITE setHrdStrictRadioMatch NOTIFY hrdStrictRadioMatchChanged)

public:
    explicit DecodiumTransceiverManager(QObject* parent = nullptr);
    ~DecodiumTransceiverManager();

    // ── Lettura proprietà ─────────────────────────────────────────────────
    bool    connected()    const { return m_connected; }
    bool    connecting()   const { return m_connecting; }
    QString rigName()      const { return m_rigName; }
    QString serialPort()   const { return m_serialPort; }
    int     baudRate()     const { return m_baudRate; }
    QString dataBits()     const { return m_dataBits; }
    QString stopBits()     const { return m_stopBits; }
    QString handshake()    const { return m_handshake; }
    bool    forceDtr()     const { return m_forceDtr; }
    bool    dtrHigh()      const { return m_dtrHigh; }
    bool    forceRts()     const { return m_forceRts; }
    bool    rtsHigh()      const { return m_rtsHigh; }
    QString networkPort()  const { return m_networkPort; }
    QString tciPort()      const { return m_tciPort; }
    QString pttMethod()    const { return m_pttMethod; }
    QString pttPort()      const { return m_pttPort; }
    QString splitMode()    const { return m_splitMode; }
    int     civAddress()   const { return m_civAddress; }
    bool    catKeepAlive() const { return m_catKeepAlive; }
    int     pollInterval() const { return m_pollInterval; }
    QString portType()     const { return m_portType; }

    double  frequency()    const { return m_frequency; }
    double  txFrequency()  const { return m_txFrequency; }
    QString mode()         const { return m_mode; }
    bool    pttActive()    const { return m_pttActive; }
    bool    split()        const { return m_split; }
    double  powerWatts()   const { return m_powerWatts; }
    int     strengthDb()   const { return m_strengthDb; }
    bool    strengthValid() const { return m_strengthValid && m_connected && !m_pttActive; }
    double  swr()          const { return m_swr; }
    double  alc()          const { return m_alc; }
    bool    alcValid()     const { return m_alcValid; }
    // 1.0.581 — strumenti del finale, in unita' naturali. Il "valido" e' la
    // parte che conta: senza sensore non esiste un numero giusto da mostrare,
    // e uno zero su una tensione di alimentazione direbbe alimentatore spento.
    double  drainVoltage()      const { return m_drainVoltage; }
    bool    drainVoltageValid() const { return m_drainVoltageValid; }
    double  drainCurrent()      const { return m_drainCurrent; }
    bool    drainCurrentValid() const { return m_drainCurrentValid; }
    double  paTemperature()     const { return m_paTemperature; }
    bool    paTemperatureValid() const { return m_paTemperatureValid; }
    double  compressionDb()     const { return m_compressionDb; }
    bool    compressionValid()  const { return m_compressionValid; }
    // La manopola della potenza, in percento del massimo del rig.
    double  powerSettingPct()   const { return m_powerSettingPct; }
    bool    powerSettingValid() const { return m_powerSettingValid; }

    QStringList rigList()       const;
    QStringList portList()      const { return m_portList; }
    QStringList baudList()      const { return {"1200","2400","4800","9600","19200","38400","57600","115200"}; }
    QStringList pttMethodList() const { return {"CAT","DTR","RTS","VOX"}; }
    QStringList splitModeList() const { return {"none","rig","emulate"}; }

    bool catAutoConnect() const { return m_catAutoConnect; }
    bool audioAutoStart() const { return m_audioAutoStart; }
    bool tciAudioEnabled() const { return m_tciAudioEnabled; }
    double tciRxGainDb() const { return m_tciRxGainDb; }
    bool hrdStrictRadioMatch() const { return m_hrdStrictRadioMatch; }

    // ── Scrittura proprietà ───────────────────────────────────────────────
    void setRigName(const QString&);
    void setSerialPort(const QString& v);
    void setBaudRate(int v)               { if (m_baudRate != v)   { m_baudRate = v;   emit baudRateChanged(); } }
    void setDataBits(const QString& v);
    void setStopBits(const QString& v);
    void setHandshake(const QString& v);
    void setForceDtr(bool v);
    void setDtrHigh(bool v);
    void setForceRts(bool v);
    void setRtsHigh(bool v);
    void setNetworkPort(const QString& v);
    void setTciPort(const QString& v)     { if (m_tciPort != v)    { m_tciPort = v;    emit tciPortChanged(); } }
    void setPttMethod(const QString& v);
    void setPttPort(const QString& v);
    void setSplitMode(const QString& v);
    void setCivAddress(int v);
    void setCatKeepAlive(bool v)        { if (m_catKeepAlive != v){ m_catKeepAlive = v; emit catKeepAliveChanged(); } }
    void setPollInterval(int v)           { if (m_pollInterval != v){ m_pollInterval = v; emit pollIntervalChanged(); } }
    void setCatAutoConnect(bool v)        { if (m_catAutoConnect != v){ m_catAutoConnect = v; emit catAutoConnectChanged(); } }
    void setAudioAutoStart(bool v)        { if (m_audioAutoStart != v){ m_audioAutoStart = v; emit audioAutoStartChanged(); } }
    void setTciAudioEnabled(bool v);
    void setTciRxGainDb(double db);
    void setHrdStrictRadioMatch(bool v)   { if (m_hrdStrictRadioMatch != v){ m_hrdStrictRadioMatch = v; emit hrdStrictRadioMatchChanged(); } }

    // ── Comandi QML-invokable ─────────────────────────────────────────────
    // Compatibilita' con DecodiumCatManager: VOX e' audio-only.
    bool canPtt() const { return m_connected && m_pttMethod != QStringLiteral("VOX"); }

    Q_INVOKABLE void setRigFrequency(double hz);
    Q_INVOKABLE void setRigTxFrequency(double hz);
    // Satellite half-duplex is deliberately separate from the normal FT
    // audio-offset split path.  It programs two absolute VFO dial
    // frequencies, then leaves PTT to the caller after its CAT settle guard.
    bool prepareSatelliteHalfDuplex(double rxHz, double txHz);
    bool restoreSatelliteHalfDuplexRx(double rxHz);
    Q_INVOKABLE void setRigTxFrequencyAndPtt(double hz, bool on);
    void setRigTxFrequencyAndPttAsync(double hz, bool on);
    Q_INVOKABLE void setRigPtt(bool on);
    Q_INVOKABLE void setRigMode(const QString& mode);
    Q_INVOKABLE void setRigAudio(bool on, double periodSeconds = 15.0, int blockSize = 6912 / 2);
    Q_INVOKABLE void setRigTune(bool on);
    Q_INVOKABLE void startRigTxAudio(const QString& mode, unsigned symbolsLength,
                                     double framesPerSymbol, double frequency,
                                     double toneSpacing, bool synchronize,
                                     bool fastMode, double dbsnr, double trPeriod);
    Q_INVOKABLE void stopRigTxAudio(bool quick = true);

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

public slots:
    Q_INVOKABLE void connectRig();
    Q_INVOKABLE void disconnectRig();

signals:
    void connectedChanged();
    void connectingChanged();
    void rigNameChanged();
    void rigListChanged();
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
    void splitModeChanged();
    void civAddressChanged();
    void catKeepAliveChanged();
    void pollIntervalChanged();
    void portTypeChanged();
    void portListChanged();
    void frequencyChanged();
    void txFrequencyChanged();
    void modeChanged();
    void pttActiveChanged();
    void splitChanged();
    void powerWattsChanged();
    void strengthChanged();
    void swrChanged();
    void alcChanged();
    void paMetersChanged();   // 1.0.581 — tensione, corrente, temperatura, compressione
    void catAutoConnectChanged();
    void audioAutoStartChanged();
    void tciAudioEnabledChanged();
    void tciRxGainDbChanged();
    void hrdStrictRadioMatchChanged();
    void errorOccurred(const QString& msg);
    void statusUpdate(const QString& msg);
    // First producer boundary. Only bounded, DirectConnection consumers may
    // attach here; ordinary UI/decoder consumers use tciPcmSamplesReady.
    void tciPcmSamplesProduced(const QVector<short>& samples);
    void tciPcmSamplesReady(const QVector<short>& samples);
    void tciModActiveChanged(bool active);


private:
    void enforceForceLineAvailability();
    void updateTelemetry(double powerWatts, double swr, double alc = 0.0, bool alcValid = false);
    void reconnectRigForParameterChange(const QString& reason);
    void disconnectRigInternal(bool reconnectAfterDisconnect);
    void scheduleTransientReconnect(const QString& reason);
    void restartTransientCatConnectionNonBlocking();
    void setConnecting(bool v);
    void abortConnectingRigAfterTimeout(Transceiver* xcv, QThread* thread,
                                        const QString& shownReason);
    bool pttSharesCatPort() const;
    bool forceDtrAvailable() const;
    bool forceRtsAvailable() const;
    std::unique_ptr<DecodiumTransceiverManagerPrivate> d;

    bool    m_connected    {false};
    bool    m_connecting   {false};
    QString m_rigName      {"None"};   // "None" = Hamlib Dummy (basic_transceiver_name_)
    QString m_serialPort   {"COM3"};
    int     m_baudRate     {57600};
    QString m_dataBits     {"Default"};
    QString m_stopBits     {"Default"};
    QString m_handshake    {"Default"};
    bool    m_forceDtr     {false};
    bool    m_dtrHigh      {false};
    bool    m_forceRts     {false};
    bool    m_rtsHigh      {false};
    QString m_networkPort  {"localhost:4532"};
    QString m_tciPort      {"localhost:50001"};
    QString m_pttMethod    {"CAT"};
    QString m_pttPort      {"CAT"};
    QString m_splitMode    {"none"};
    int     m_civAddress   {0};
    bool    m_catKeepAlive {false};
    int     m_pollInterval {1};
    QString m_portType     {"serial"};

    double  m_frequency    {0.0};
    double  m_txFrequency  {0.0};
    QString m_mode;
    bool    m_pttActive    {false};
    bool    m_split        {false};
    double  m_powerWatts   {0.0};
    int     m_strengthDb   {0};
    bool    m_strengthValid {false};
    double  m_swr          {0.0};
    double  m_alc          {0.0};  // 1.0.323 — ALC meter 0..100
    bool    m_alcValid     {false};
    // 1.0.581 — strumenti del finale
    double  m_drainVoltage       {0.0};
    bool    m_drainVoltageValid  {false};
    double  m_drainCurrent       {0.0};
    bool    m_drainCurrentValid  {false};
    double  m_paTemperature      {0.0};
    bool    m_paTemperatureValid {false};
    double  m_compressionDb      {0.0};
    bool    m_compressionValid   {false};
    double  m_powerSettingPct    {0.0};
    bool    m_powerSettingValid  {false};

    QStringList m_portList;
    bool    m_catAutoConnect {false};
    bool    m_audioAutoStart {false};
    bool    m_tciAudioEnabled {true};
    // 1.0.537 iu8lmc - guadagno applicato all'audio RX che arriva via TCI.
    // I server TCI consegnano il flusso a fondo scala: misurato su AetherSDR
    // picco 0,999 e rms 0,45 (circa -7 dBFS), mentre il decodificatore lavora
    // bene intorno a -27 dBFS. -20 dB riporta il livello nell'intervallo utile
    // senza toccare il volume del software SDR.
    double  m_tciRxGainDb {-20.0};
    bool    m_hrdStrictRadioMatch {true};
    int     m_transientCatRetryCount {0};
    bool    m_transientCatReconnectPending {false};
    bool    m_reconnectAfterDisconnect {false};
    bool    m_disconnectInProgress {false};
    quint64 m_transientCatReconnectSerial {0};
    QElapsedTimer m_connectAttemptTimer;
};
