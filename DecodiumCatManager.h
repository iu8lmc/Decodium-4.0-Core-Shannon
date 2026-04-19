#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QSerialPort>
#include <QTimer>

// CAT control via QSerialPort + comandi Kenwood nativi (niente Hamlib)
class DecodiumCatManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected       READ connected       NOTIFY connectedChanged)
    Q_PROPERTY(QString rigName      READ rigName         WRITE setRigName       NOTIFY rigNameChanged)
    Q_PROPERTY(QString serialPort   READ serialPort      WRITE setSerialPort    NOTIFY serialPortChanged)
    Q_PROPERTY(int baudRate         READ baudRate        WRITE setBaudRate      NOTIFY baudRateChanged)
    Q_PROPERTY(QString dataBits     READ dataBits        WRITE setDataBits      NOTIFY dataBitsChanged)
    Q_PROPERTY(QString stopBits     READ stopBits        WRITE setStopBits      NOTIFY stopBitsChanged)
    Q_PROPERTY(QString handshake    READ handshake       WRITE setHandshake     NOTIFY handshakeChanged)
    Q_PROPERTY(bool forceDtr        READ forceDtr        WRITE setForceDtr      NOTIFY forceDtrChanged)
    Q_PROPERTY(bool dtrHigh         READ dtrHigh         WRITE setDtrHigh       NOTIFY dtrHighChanged)
    Q_PROPERTY(bool forceRts        READ forceRts        WRITE setForceRts      NOTIFY forceRtsChanged)
    Q_PROPERTY(bool rtsHigh         READ rtsHigh         WRITE setRtsHigh       NOTIFY rtsHighChanged)
    Q_PROPERTY(QString networkPort  READ networkPort     WRITE setNetworkPort   NOTIFY networkPortChanged)
    Q_PROPERTY(QString tciPort      READ tciPort         WRITE setTciPort       NOTIFY tciPortChanged)
    Q_PROPERTY(QString pttMethod    READ pttMethod       WRITE setPttMethod     NOTIFY pttMethodChanged)
    Q_PROPERTY(QString pttPort      READ pttPort         WRITE setPttPort       NOTIFY pttPortChanged)
    Q_PROPERTY(int civAddress       READ civAddress      WRITE setCivAddress    NOTIFY civAddressChanged)
    Q_PROPERTY(QString splitMode    READ splitMode       WRITE setSplitMode     NOTIFY splitModeChanged)
    Q_PROPERTY(int pollInterval     READ pollInterval    WRITE setPollInterval  NOTIFY pollIntervalChanged)
    Q_PROPERTY(QString portType     READ portType        CONSTANT)
    Q_PROPERTY(QStringList rigList  READ rigList         CONSTANT)
    Q_PROPERTY(QStringList portList READ portList        NOTIFY portListChanged)
    Q_PROPERTY(QStringList baudList READ baudList        CONSTANT)
    Q_PROPERTY(QStringList pttMethodList READ pttMethodList CONSTANT)
    Q_PROPERTY(QStringList splitModeList READ splitModeList CONSTANT)
    Q_PROPERTY(double frequency     READ frequency       NOTIFY frequencyChanged)
    Q_PROPERTY(double txFrequency   READ txFrequency     CONSTANT)
    Q_PROPERTY(QString mode         READ mode            NOTIFY modeChanged)
    Q_PROPERTY(bool pttActive       READ pttActive       NOTIFY pttActiveChanged)
    Q_PROPERTY(bool split           READ split           CONSTANT)
    Q_PROPERTY(bool catAutoConnect  READ catAutoConnect  WRITE setCatAutoConnect  NOTIFY catAutoConnectChanged)
    Q_PROPERTY(bool audioAutoStart  READ audioAutoStart  WRITE setAudioAutoStart  NOTIFY audioAutoStartChanged)

public:
    explicit DecodiumCatManager(QObject* parent = nullptr);
    ~DecodiumCatManager();

    bool        connected()     const { return m_connected; }
    QString     rigName()       const { return m_rigName; }
    void        setRigName(const QString& v);
    QString     serialPort()    const { return m_serialPort; }
    void        setSerialPort(const QString& v)  { if (m_serialPort != v)  { m_serialPort = v;  emit serialPortChanged(); } }
    int         baudRate()      const { return m_baudRate; }
    void        setBaudRate(int v)               { if (m_baudRate != v)    { m_baudRate = v;    emit baudRateChanged(); } }
    QString     dataBits()      const { return m_dataBits; }
    void        setDataBits(const QString& v)    { if (m_dataBits != v)    { m_dataBits = v;    emit dataBitsChanged(); } }
    QString     stopBits()      const { return m_stopBits; }
    void        setStopBits(const QString& v)    { if (m_stopBits != v)    { m_stopBits = v;    emit stopBitsChanged(); } }
    QString     handshake()     const { return m_handshake; }
    void        setHandshake(const QString& v)   { if (m_handshake != v)   { m_handshake = v;   emit handshakeChanged(); } }
    bool        forceDtr()      const { return m_forceDtr; }
    void        setForceDtr(bool v);
    bool        dtrHigh()       const { return m_dtrHigh; }
    void        setDtrHigh(bool v);
    bool        forceRts()      const { return m_forceRts; }
    void        setForceRts(bool v);
    bool        rtsHigh()       const { return m_rtsHigh; }
    void        setRtsHigh(bool v);
    QString     networkPort()   const { return m_networkPort; }
    void        setNetworkPort(const QString& v) { if (m_networkPort != v) { m_networkPort = v; emit networkPortChanged(); } }
    QString     tciPort()       const { return m_tciPort; }
    void        setTciPort(const QString& v)     { if (m_tciPort != v)     { m_tciPort = v;     emit tciPortChanged(); } }
    QString     pttMethod()     const { return m_pttMethod; }
    void        setPttMethod(const QString& v);
    QString     pttPort()       const { return m_pttPort; }
    void        setPttPort(const QString& v)     { if (m_pttPort != v)     { m_pttPort = v;     emit pttPortChanged(); } }
    int         civAddress()    const { return m_civAddress; }
    void        setCivAddress(int v)             { if (m_civAddress != v)  { m_civAddress = v;  emit civAddressChanged(); } }
    QString     splitMode()     const { return "none"; }
    void        setSplitMode(const QString&)     {}
    int         pollInterval()  const { return m_pollInterval; }
    void        setPollInterval(int v)           { if (m_pollInterval != v){ m_pollInterval = v; emit pollIntervalChanged(); applyPollInterval(); } }
    QString     portType()      const { return "serial"; }
    QStringList rigList()       const;
    QStringList portList()      const { return m_portList; }
    QStringList baudList()      const { return {"1200","2400","4800","9600","19200","38400","57600","115200"}; }
    QStringList pttMethodList() const { return {"CAT","DTR","RTS"}; }
    QStringList splitModeList() const { return {"none"}; }
    double      frequency()     const { return m_frequency; }
    double      txFrequency()   const { return 0.0; }
    QString     mode()          const { return m_mode; }
    bool        pttActive()     const { return m_pttActive; }
    bool        split()         const { return false; }
    bool        catAutoConnect() const { return m_catAutoConnect; }
    void        setCatAutoConnect(bool v) { if (m_catAutoConnect != v) { m_catAutoConnect = v; emit catAutoConnectChanged(); } }
    bool        audioAutoStart() const { return m_audioAutoStart; }
    void        setAudioAutoStart(bool v) { if (m_audioAutoStart != v) { m_audioAutoStart = v; emit audioAutoStartChanged(); } }

    // PTT disponibile se: CAT connesso, oppure porta aperta con metodo DTR/RTS
    bool canPtt() const {
        if (m_pttMethod == "CAT") return m_connected;
        return m_serial && m_serial->isOpen();
    }

    Q_INVOKABLE void setRigFrequency(double hz);
    Q_INVOKABLE void setRigTxFrequency(double) {}
    Q_INVOKABLE void setRigMode(const QString& mode);
    Q_INVOKABLE void setRigPtt(bool on);

public slots:
    Q_INVOKABLE void connectRig();
    Q_INVOKABLE void disconnectRig();
    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

signals:
    void connectedChanged();
    void rigNameChanged();
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
    void pollIntervalChanged();
    void portListChanged();
    void frequencyChanged();
    void modeChanged();
    void pttActiveChanged();
    void errorOccurred(const QString& msg);
    void statusUpdate(const QString& msg);
    void catAutoConnectChanged();
    void audioAutoStartChanged();

private slots:
    void onReadyRead();
    void onPollTimer();
    void onSerialError(QSerialPort::SerialPortError error);
    void onConnectTimeout();

private:
    void enforceCatSerialDefaults();
    void applyRigDefaults(const QString& rigName);
    void sendCommand(const QByteArray& cmd);
    void processResponse(const QByteArray& resp);
    QString parseMode(char code);
    void applyPollInterval();
    bool tryNextAutoBaud();     // ritorna true se ha avviato un nuovo tentativo

    QSerialPort* m_serial      {nullptr};
    QTimer*      m_pollTimer   {nullptr};
    QTimer*      m_connectTimer{nullptr};  // timeout attesa prima risposta
    QByteArray   m_rxBuf;

    bool    m_connected   {false};
    QString m_rigName     {"Kenwood TS-590S"};
    QString m_serialPort  {"COM3"};
    int     m_baudRate    {57600};
    QString m_dataBits    {"8"};
    QString m_stopBits    {"1"};
    QString m_handshake   {"none"};
    bool    m_forceDtr    {false};
    bool    m_dtrHigh     {false};
    bool    m_forceRts    {false};
    bool    m_rtsHigh     {false};
    QString m_networkPort {};
    QString m_tciPort     {};
    QString m_pttMethod   {"CAT"};
    QString m_pttPort     {"CAT"};
    int     m_civAddress  {0};       // ICOM CI-V address (0 = non-ICOM)
    int     m_pollInterval{2};
    double  m_frequency   {0.0};
    QString m_mode;
    bool    m_pttActive   {false};
    QStringList m_portList;
    bool    m_catAutoConnect {false};
    bool    m_audioAutoStart {false};
    QSerialPort* m_pttSerial {nullptr};  // porta separata per PTT DTR/RTS

    // Auto-baud detection: si avvia alla connessione, itera su candidate bauds
    // finche' non arriva una risposta o la lista e' esaurita.
    QList<int> m_autoBaudCandidates;
    int        m_autoBaudIndex  {-1};
    bool       m_autoBaudActive {false};
};
