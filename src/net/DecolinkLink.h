// Decolink: una radio remota raggiunta tramite il relay con accesso controllato.
//
// Chi sta accanto alla radio fa girare Decolink (il gateway); chi la usa da
// lontano entra qui, con le credenziali del suo account. Il relay controlla il
// ruolo: un ascoltatore sente e basta, un operatore o il titolare puo' anche
// sintonizzare e trasmettere.
//
// Il collegamento ha due strade, come nell'app mobile:
//   - HTTPS  POST /api/login  (email, password, stazione)  -> token firmato
//   - UDP    relay, protocollo HFGW v2 (DecolinkPacket.h): registrazione col
//            token, audio ricevuto, comandi CAT nel dialetto di rigctl, audio da
//            trasmettere.
// Il token dura un'ora e si rinnova da solo; la password resta in memoria finche'
// serve a rinnovarlo e non viene mai scritta qui (se la conserva chi chiama).
#pragma once

#include "DecolinkLossless.h"
#include "DecolinkPacket.h"
#include "DecolinkV3.h"
#include "DecoPortPacket.h"
#include "RemoteRadioLink.h"

#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QMap>
#include <QPair>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QUdpSocket;

class DecolinkLink : public RemoteRadioLink {
    Q_OBJECT
    Q_PROPERTY(bool linked READ isLinked NOTIFY linkedChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY authChanged)
    Q_PROPERTY(QString callsign READ callsign NOTIFY authChanged)
    Q_PROPERTY(QString station READ station NOTIFY authChanged)
    Q_PROPERTY(QString stationName READ stationName NOTIFY authChanged)
    Q_PROPERTY(QString role READ role NOTIFY authChanged)
    Q_PROPERTY(bool canTransmit READ canTransmit NOTIFY authChanged)
    Q_PROPERTY(QString txHolder READ txHolder NOTIFY stateChanged)
    Q_PROPERTY(QVariantList stationList READ stationList NOTIFY authChanged)
    Q_PROPERTY(QString rigLabel READ rigLabel NOTIFY stateChanged)
    Q_PROPERTY(double frequencyHz READ frequencyHz NOTIFY stateChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY stateChanged)
    Q_PROPERTY(bool ptt READ ptt NOTIFY stateChanged)
    Q_PROPERTY(double sMeterDbm READ sMeterDbm NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int latencyMs READ latencyMs NOTIFY stateChanged)
    Q_PROPERTY(int txAudioLeadMs READ txAudioLeadMs NOTIFY stateChanged)
    Q_PROPERTY(QString peerAddress READ peerAddress NOTIFY linkedChanged)
    // Profilo audio: -1 segue quello che il gateway manda, 0 chiede il PCM
    // (v2), 3 chiede i digitali senza perdite (v3).
    Q_PROPERTY(int audioProfile READ audioProfile WRITE setAudioProfile NOTIFY profileChanged)
    Q_PROPERTY(int activeProfile READ activeProfile NOTIFY profileChanged)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY profileChanged)
    Q_PROPERTY(int lostBlocks READ lostBlocks NOTIFY stateChanged)
    Q_PROPERTY(int recoveredBlocks READ recoveredBlocks NOTIFY stateChanged)

public:
    explicit DecolinkLink(QObject* parent = nullptr);
    ~DecolinkLink() override;

    // ── RadioLink ──────────────────────────────────────────────────────────
    bool    isLinked() const override { return m_registered && m_gatewayUp; }
    QString rigLabel() const override { return m_state.rigLabel; }
    decoport::Context state() const override { return m_state; }

    void setFrequency(qint64 hz) override;
    void setMode(decoport::Mode mode) override;
    void setPtt(bool on, quint64 whenNs = 0) override;
    // `samples` e' audio a 12 kHz mono: e' il formato in cui l'applicazione
    // produce il segnale da trasmettere.
    void sendTxAudio(const QVector<short>& samples, quint64 playAtNs) override;

    // ── RemoteRadioLink ────────────────────────────────────────────────────
    double  frequencyHz() const override { return static_cast<double>(m_state.frequencyHz); }
    QString modeName() const override { return decoport::modeToString(m_state.mode); }
    bool    ptt() const override { return m_state.ptt; }
    double  sMeterDbm() const override { return m_state.sMeterDbm(); }
    QString status() const override { return m_status; }
    int     txAudioLeadMs() const override { return m_txLeadMs; }
    QString peerAddress() const override;
    bool    canTransmit() const override { return m_canTx; }
    // Nominativo dell'altro operatore che ha il PTT, o vuoto se e' libero o e' nostro.
    QString txHolder() const { return m_txHolder; }
    QString txBlockedReason() const override;

    void tune(double hz) override { setFrequency(qint64(hz)); }
    void setModeName(const QString& name) override;
    void key(bool on) override { setPtt(on); }

    // ── account ────────────────────────────────────────────────────────────
    bool        loggedIn() const { return !m_token.isEmpty(); }
    QString     callsign() const { return m_callsign; }
    QString     station() const { return m_station; }
    QString     stationName() const { return m_stationName; }
    QString     role() const { return m_role; }
    QVariantList stationList() const { return m_stations; }
    int         latencyMs() const { return m_rttMs; }

    // ── profilo audio ──────────────────────────────────────────────────────
    int     audioProfile() const { return m_wantProfile; }
    void    setAudioProfile(int profile);
    int     activeProfile() const { return m_activeProfile; }
    QString activeProfileName() const;
    int     lostBlocks() const { return int(m_v3Lost); }
    int     recoveredBlocks() const { return int(m_v3Recovered); }

    // Solo il login: utile per elencare le stazioni prima di sceglierne una.
    Q_INVOKABLE void login(const QString& authHost, const QString& email,
                           const QString& password, const QString& station = QString());
    // Login e poi collegamento al relay. relayHost vuoto = lo stesso nome del
    // server di accesso. relayPort <= 0 = quella predefinita.
    Q_INVOKABLE void connectTo(const QString& authHost, const QString& relayHost,
                               int relayPort, const QString& email,
                               const QString& password, const QString& station = QString());
    Q_INVOKABLE void disconnectFromRelay();


signals:
    void authChanged();
    void profileChanged();

private slots:
    void onDatagrams();
    void onKeepAlive();
    void onPing();
    void onPoll();
    void onWatch();
    void onTxTick();
    void onV3Tick();

private:
    enum class Kind { Freq, Mode, Ptt, Strength, Swr, Alc, Power, Other };
    struct Pending { Kind kind; qint64 sentMs; };

    static qint64 nowMs();
    void openSocket();
    void setStatus(const QString& s);
    void recomputeLinked();
    void setAuth(const QString& token, const QString& callsign, const QString& station,
                 const QString& stationName, const QString& role, bool canTx, int expiresIn);
    void clearAuth();
    void startRelay();
    void closeRelay(bool releasePtt);
    void sendRegister();
    void sendPacket(quint8 flag, quint32 seq, const QByteArray& body, quint32 rate = 48000);
    void sendCat(const QString& line, Kind kind);
    void handleAudio(const decolink::Header& h, const QByteArray& body);
    void handleCatResponse(quint32 seq, const QByteArray& body);
    void handleTxState(const QString& text);
    void loseTurn();
    void handleV3(const QByteArray& dg);
    void handleV3Audio(const decolink::v3::Header& h, const QByteArray& body);
    void handleV3Ctrl(const QByteArray& body);
    void handleV3Nack(const QByteArray& body);
    void sendV3(quint8 type, quint8 profile, quint16 seq, quint32 time, const QByteArray& body);
    void ensureProfile();
    void setActiveProfile(int profile);
    void deliverV3(const QVector<short>& samples);
    void flushV3(bool force);
    void sendV3Nack(quint16 from, quint16 until);
    void resetV3Rx();
    void handleDenied(const QString& reason);
    void releasePttNow();
    static decoport::Mode modeFromRigctl(const QString& name);
    static QString rigctlModeName(decoport::Mode m);

    QNetworkAccessManager* m_nam {nullptr};
    QNetworkReply*         m_loginReply {nullptr};
    QUdpSocket*            m_socket {nullptr};
    QTimer*                m_keepAlive {nullptr};
    QTimer*                m_pingTimer {nullptr};
    QTimer*                m_pollTimer {nullptr};
    QTimer*                m_watchTimer {nullptr};
    QTimer*                m_txTimer {nullptr};
    QTimer*                m_v3Timer {nullptr};
    QTimer*                m_renewTimer {nullptr};

    // credenziali: la password serve solo a rinnovare il token
    QString m_authHost;
    QString m_email;
    QString m_password;
    QString m_stationWanted;
    QString m_relayHost;
    int     m_relayPort {decolink::kDefaultRelayPort};
    bool    m_wantRelay {false};
    qint64  m_lastRelogin {0};

    // account
    QString     m_token;
    QString     m_callsign;
    QString     m_station;
    QString     m_stationName;
    QString     m_role;
    QVariantList m_stations;
    bool        m_canTx {false};
    qint64      m_tokenExpiresMs {0};

    // relay
    QHostAddress m_relayAddr;
    bool    m_registered {false};
    bool    m_gatewayUp {false};
    bool    m_linkedReported {false};
    quint32 m_regSeq {0};
    qint64  m_lastPacketMs {0};
    qint64  m_lastAudioMs {0};

    // audio ricevuto
    decolink::Decimator m_dec;
    quint32 m_decRate {0};
    quint32 m_lastRxSeq {0};
    bool    m_haveRxSeq {false};
    int     m_frameSamples {120};    // campioni a 12 kHz in un pacchetto, per riempire i buchi
    qint64  m_rxGaps {0};
    qint64  m_rxSamples {0};

    // CAT
    quint32 m_catSeq {0};
    QHash<quint32, Pending> m_pending;
    int     m_pollTick {0};
    qint64  m_lastCatMs {0};

    // stato osservato
    decoport::Context m_state;
    QString m_status;
    int     m_rttMs {0};
    int     m_txLeadMs {120};

    // il PTT della stazione, come lo annuncia il relay
    QString m_txHolder;

    // profilo audio v3
    int     m_wantProfile {-1};
    int     m_activeProfile {-1};
    bool    m_helloSent {false};
    qint64  m_chooseSentMs {0};
    // ricezione dei digitali: blocchi da 40 ms, riordinati e richiesti indietro
    bool    m_v3Have {false};
    quint16 m_v3Expected {0};
    QMap<quint16, QVector<short>> m_v3Hold;
    qint64  m_v3GapSinceMs {0};
    qint64  m_v3NackMs {0};
    int     m_v3NackTries {0};
    quint64 m_v3Lost {0};
    quint64 m_v3Recovered {0};
    // trasmissione dei digitali
    bool    m_txDigi {false};
    quint16 m_v3TxSeq {0};
    quint32 m_v3TxTime {0};
    QMap<quint16, QPair<QByteArray, qint64>> m_v3TxWindow;

    // trasmissione
    QVector<short> m_txSamples;
    int     m_txPos {0};
    qint64  m_txStartMs {0};
    quint32 m_txSeq {0};
    bool    m_pttRequested {false};
    qint64  m_pttOnMs {0};
    qint64  m_lastTxPacketMs {0};

};
