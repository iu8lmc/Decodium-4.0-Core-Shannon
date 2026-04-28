#pragma once
// DecodiumTransceiverManager — wrapper QML/QObject attorno a TransceiverFactory.
// Supporta Hamlib (100+ radio), OmniRig, HRD, DXLab Suite Commander, TCI.
// Stessa interfaccia pubblica di DecodiumCatManager + campi aggiuntivi.
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <memory>

// Forward-declare per nascondere le dipendenze Boost/Hamlib dall'header
struct DecodiumTransceiverManagerPrivate;

class DecodiumTransceiverManager : public QObject
{
    Q_OBJECT

    // ── Connessione base ──────────────────────────────────────────────────
    Q_PROPERTY(bool    connected  READ connected  NOTIFY connectedChanged)
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
    Q_PROPERTY(double  swr         READ swr         NOTIFY swrChanged)

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

public:
    explicit DecodiumTransceiverManager(QObject* parent = nullptr);
    ~DecodiumTransceiverManager();

    // ── Lettura proprietà ─────────────────────────────────────────────────
    bool    connected()    const { return m_connected; }
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
    int     pollInterval() const { return m_pollInterval; }
    QString portType()     const { return m_portType; }

    double  frequency()    const { return m_frequency; }
    double  txFrequency()  const { return m_txFrequency; }
    QString mode()         const { return m_mode; }
    bool    pttActive()    const { return m_pttActive; }
    bool    split()        const { return m_split; }
    double  powerWatts()   const { return m_powerWatts; }
    double  swr()          const { return m_swr; }

    QStringList rigList()       const;
    QStringList portList()      const { return m_portList; }
    QStringList baudList()      const { return {"1200","2400","4800","9600","19200","38400","57600","115200"}; }
    QStringList pttMethodList() const { return {"CAT","DTR","RTS","VOX"}; }
    QStringList splitModeList() const { return {"none","rig","emulate"}; }

    bool catAutoConnect() const { return m_catAutoConnect; }
    bool audioAutoStart() const { return m_audioAutoStart; }
    bool tciAudioEnabled() const { return m_tciAudioEnabled; }

    // ── Scrittura proprietà ───────────────────────────────────────────────
    void setRigName(const QString&);
    void setSerialPort(const QString& v);
    void setBaudRate(int v)               { if (m_baudRate != v)   { m_baudRate = v;   emit baudRateChanged(); } }
    void setDataBits(const QString& v)    { if (m_dataBits != v)   { m_dataBits = v;   emit dataBitsChanged(); } }
    void setStopBits(const QString& v)    { if (m_stopBits != v)   { m_stopBits = v;   emit stopBitsChanged(); } }
    void setHandshake(const QString& v);
    void setForceDtr(bool v);
    void setDtrHigh(bool v);
    void setForceRts(bool v);
    void setRtsHigh(bool v);
    void setNetworkPort(const QString& v) { if (m_networkPort != v){ m_networkPort = v; emit networkPortChanged(); } }
    void setTciPort(const QString& v)     { if (m_tciPort != v)    { m_tciPort = v;    emit tciPortChanged(); } }
    void setPttMethod(const QString& v);
    void setPttPort(const QString& v);
    void setSplitMode(const QString& v);
    void setPollInterval(int v)           { if (m_pollInterval != v){ m_pollInterval = v; emit pollIntervalChanged(); } }
    void setCatAutoConnect(bool v)        { if (m_catAutoConnect != v){ m_catAutoConnect = v; emit catAutoConnectChanged(); } }
    void setAudioAutoStart(bool v)        { if (m_audioAutoStart != v){ m_audioAutoStart = v; emit audioAutoStartChanged(); } }
    void setTciAudioEnabled(bool v);

    // ── Comandi QML-invokable ─────────────────────────────────────────────
    // Compatibilità con DecodiumCatManager: PTT disponibile se connesso
    bool canPtt() const { return m_connected; }

    Q_INVOKABLE void setRigFrequency(double hz);
    Q_INVOKABLE void setRigTxFrequency(double hz);
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
    void pollIntervalChanged();
    void portTypeChanged();
    void portListChanged();
    void frequencyChanged();
    void txFrequencyChanged();
    void modeChanged();
    void pttActiveChanged();
    void splitChanged();
    void powerWattsChanged();
    void swrChanged();
    void catAutoConnectChanged();
    void audioAutoStartChanged();
    void tciAudioEnabledChanged();
    void errorOccurred(const QString& msg);
    void statusUpdate(const QString& msg);
    void tciPcmSamplesReady(const QVector<short>& samples);
    void tciModActiveChanged(bool active);


private:
    void enforceForceLineAvailability();
    void updateTelemetry(double powerWatts, double swr);
    void reconnectRigForParameterChange(const QString& reason);
    void scheduleTransientReconnect(const QString& reason);
    bool pttSharesCatPort() const;
    bool forceDtrAvailable() const;
    bool forceRtsAvailable() const;
    std::unique_ptr<DecodiumTransceiverManagerPrivate> d;

    bool    m_connected    {false};
    QString m_rigName      {"None"};   // "None" = Hamlib Dummy (basic_transceiver_name_)
    QString m_serialPort   {"COM3"};
    int     m_baudRate     {57600};
    QString m_dataBits     {"8"};
    QString m_stopBits     {"1"};
    QString m_handshake    {"none"};
    bool    m_forceDtr     {false};
    bool    m_dtrHigh      {false};
    bool    m_forceRts     {false};
    bool    m_rtsHigh      {false};
    QString m_networkPort  {"localhost:4532"};
    QString m_tciPort      {"localhost:50001"};
    QString m_pttMethod    {"CAT"};
    QString m_pttPort      {"CAT"};
    QString m_splitMode    {"none"};
    int     m_pollInterval {1};
    QString m_portType     {"serial"};

    double  m_frequency    {0.0};
    double  m_txFrequency  {0.0};
    QString m_mode;
    bool    m_pttActive    {false};
    bool    m_split        {false};
    double  m_powerWatts   {0.0};
    double  m_swr          {0.0};

    QStringList m_portList;
    bool    m_catAutoConnect {false};
    bool    m_audioAutoStart {false};
    bool    m_tciAudioEnabled {true};
    int     m_transientCatRetryCount {0};
    bool    m_transientCatReconnectPending {false};
};
