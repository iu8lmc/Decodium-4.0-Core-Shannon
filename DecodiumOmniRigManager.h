#pragma once
// DecodiumOmniRigManager — backend OmniRig per Decodium 4.0 Core Shannon
// Usa QAxObject (COM/ActiveX) per connettersi a OmniRig.exe (Windows only)
// Stessa interfaccia pubblica di DecodiumCatManager / DecodiumTransceiverManager.
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class QAxObject;

class DecodiumOmniRigManager : public QObject
{
    Q_OBJECT

    // ── Connessione ───────────────────────────────────────────────────────────
    Q_PROPERTY(bool    connected      READ connected      NOTIFY connectedChanged)
    Q_PROPERTY(QString rigName        READ rigName        WRITE setRigName        NOTIFY rigNameChanged)

    // ── Porta seriale (stub — OmniRig gestisce la porta internamente) ─────────
    Q_PROPERTY(QString serialPort     READ serialPort     WRITE setSerialPort     NOTIFY serialPortChanged)
    Q_PROPERTY(int     baudRate       READ baudRate       WRITE setBaudRate       NOTIFY baudRateChanged)
    Q_PROPERTY(QString dataBits       READ dataBits       WRITE setDataBits       NOTIFY dataBitsChanged)
    Q_PROPERTY(QString stopBits       READ stopBits       WRITE setStopBits       NOTIFY stopBitsChanged)
    Q_PROPERTY(QString handshake      READ handshake      WRITE setHandshake      NOTIFY handshakeChanged)
    Q_PROPERTY(bool    forceDtr       READ forceDtr       WRITE setForceDtr       NOTIFY forceDtrChanged)
    Q_PROPERTY(bool    dtrHigh        READ dtrHigh        WRITE setDtrHigh        NOTIFY dtrHighChanged)
    Q_PROPERTY(bool    forceRts       READ forceRts       WRITE setForceRts       NOTIFY forceRtsChanged)
    Q_PROPERTY(bool    rtsHigh        READ rtsHigh        WRITE setRtsHigh        NOTIFY rtsHighChanged)
    Q_PROPERTY(QString networkPort    READ networkPort    WRITE setNetworkPort    NOTIFY networkPortChanged)
    Q_PROPERTY(QString tciPort        READ tciPort        WRITE setTciPort        NOTIFY tciPortChanged)

    // ── PTT ───────────────────────────────────────────────────────────────────
    Q_PROPERTY(QString pttMethod      READ pttMethod      WRITE setPttMethod      NOTIFY pttMethodChanged)
    Q_PROPERTY(QString pttPort        READ pttPort        WRITE setPttPort        NOTIFY pttPortChanged)
    Q_PROPERTY(QString splitMode      READ splitMode      WRITE setSplitMode      NOTIFY splitModeChanged)
    Q_PROPERTY(int     pollInterval   READ pollInterval   WRITE setPollInterval   NOTIFY pollIntervalChanged)

    // ── Tipo porta (OmniRig non usa porta seriale diretta) ────────────────────
    Q_PROPERTY(QString portType       READ portType       CONSTANT)

    // ── Stato frequenza/modo ──────────────────────────────────────────────────
    Q_PROPERTY(double  frequency      READ frequency      NOTIFY frequencyChanged)
    Q_PROPERTY(double  txFrequency    READ txFrequency    NOTIFY txFrequencyChanged)
    Q_PROPERTY(QString mode           READ mode           NOTIFY modeChanged)
    Q_PROPERTY(bool    pttActive      READ pttActive      NOTIFY pttActiveChanged)
    Q_PROPERTY(bool    split          READ split          CONSTANT)

    // ── Liste per UI ──────────────────────────────────────────────────────────
    Q_PROPERTY(QStringList rigList       READ rigList       CONSTANT)
    Q_PROPERTY(QStringList portList      READ portList      NOTIFY portListChanged)
    Q_PROPERTY(QStringList baudList      READ baudList      CONSTANT)
    Q_PROPERTY(QStringList pttMethodList READ pttMethodList CONSTANT)
    Q_PROPERTY(QStringList splitModeList READ splitModeList CONSTANT)

    // ── Auto-comportamento ────────────────────────────────────────────────────
    Q_PROPERTY(bool catAutoConnect    READ catAutoConnect WRITE setCatAutoConnect NOTIFY catAutoConnectChanged)
    Q_PROPERTY(bool audioAutoStart    READ audioAutoStart WRITE setAudioAutoStart NOTIFY audioAutoStartChanged)

public:
    explicit DecodiumOmniRigManager(QObject* parent = nullptr);
    ~DecodiumOmniRigManager();

    // ── Getter ────────────────────────────────────────────────────────────────
    bool        connected()     const { return m_connected; }
    QString     rigName()       const { return m_rigName; }
    QString     serialPort()    const { return {}; }
    int         baudRate()      const { return 0; }
    QString     dataBits()      const { return "8"; }
    QString     stopBits()      const { return "1"; }
    QString     handshake()     const { return "none"; }
    bool        forceDtr()      const { return false; }
    bool        dtrHigh()       const { return false; }
    bool        forceRts()      const { return false; }
    bool        rtsHigh()       const { return false; }
    QString     networkPort()   const { return {}; }
    QString     tciPort()       const { return {}; }
    QString     pttMethod()     const { return m_pttMethod; }
    QString     pttPort()       const { return "CAT"; }
    QString     splitMode()     const { return "none"; }
    int         pollInterval()  const { return m_pollInterval; }
    QString     portType()      const { return "none"; }   // nessuna porta diretta

    double      frequency()     const { return m_frequency; }
    double      txFrequency()   const { return m_txFrequency; }
    QString     mode()          const { return m_mode; }
    bool        pttActive()     const { return m_pttActive; }
    bool        split()         const { return false; }

    QStringList rigList()       const { return {"OmniRig Rig 1","OmniRig Rig 2"}; }
    QStringList portList()      const { return {}; }
    QStringList baudList()      const { return {}; }
    QStringList pttMethodList() const { return {"CAT","DTR","RTS"}; }
    QStringList splitModeList() const { return {"none"}; }

    bool catAutoConnect()       const { return m_catAutoConnect; }
    bool audioAutoStart()       const { return m_audioAutoStart; }

    bool canPtt()               const { return m_connected; }

    // ── Setter (stub per compatibilità interfaccia) ───────────────────────────
    void setRigName(const QString& v)     { if (m_rigName != v) { m_rigName = v; emit rigNameChanged(); } }
    void setSerialPort(const QString&)    {}
    void setBaudRate(int)                 {}
    void setDataBits(const QString&)      {}
    void setStopBits(const QString&)      {}
    void setHandshake(const QString&)     {}
    void setForceDtr(bool)                {}
    void setDtrHigh(bool)                 {}
    void setForceRts(bool)                {}
    void setRtsHigh(bool)                 {}
    void setNetworkPort(const QString&)   {}
    void setTciPort(const QString&)       {}
    void setPttMethod(const QString& v)   { if (m_pttMethod != v) { m_pttMethod = v; emit pttMethodChanged(); } }
    void setPttPort(const QString&)       {}
    void setSplitMode(const QString&)     {}
    void setPollInterval(int v)           { if (m_pollInterval != v) { m_pollInterval = v; emit pollIntervalChanged(); applyPollInterval(); } }
    void setCatAutoConnect(bool v)        { if (m_catAutoConnect != v) { m_catAutoConnect = v; emit catAutoConnectChanged(); } }
    void setAudioAutoStart(bool v)        { if (m_audioAutoStart != v) { m_audioAutoStart = v; emit audioAutoStartChanged(); } }

    // ── Comandi invokable ─────────────────────────────────────────────────────
    Q_INVOKABLE void setRigFrequency(double hz);
    Q_INVOKABLE void setRigTxFrequency(double)    {}
    Q_INVOKABLE void setRigMode(const QString&)   {}
    Q_INVOKABLE void setRigPtt(bool on);
    Q_INVOKABLE void refreshPorts()               {}
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

public slots:
    Q_INVOKABLE void connectRig();
    Q_INVOKABLE void disconnectRig();

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
    void splitModeChanged();
    void pollIntervalChanged();
    void portListChanged();
    void frequencyChanged();
    void txFrequencyChanged();
    void modeChanged();
    void pttActiveChanged();
    void errorOccurred(const QString& msg);
    void statusUpdate(const QString& msg);
    void catAutoConnectChanged();
    void audioAutoStartChanged();

private slots:
    void onPollTimer();

private:
    void applyPollInterval();
    QString modeFromParam(int param) const;

    QAxObject* m_omniRig   {nullptr};
    QAxObject* m_rig        {nullptr};
    QTimer*    m_pollTimer  {nullptr};

    bool    m_connected     {false};
    QString m_rigName       {"OmniRig Rig 1"};
    QString m_pttMethod     {"CAT"};
    int     m_pollInterval  {2};

    double  m_frequency     {0.0};
    double  m_txFrequency   {0.0};
    QString m_mode;
    bool    m_pttActive     {false};

    bool    m_catAutoConnect{false};
    bool    m_audioAutoStart{false};
};
