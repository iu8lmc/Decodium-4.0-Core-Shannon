// DecoPort — il lato gateway: mette in rete la radio che trova, senza chiedere
// a nessuno quale sia.
//
// Protocollo: doc/DECOPORT_PROTOCOL.md.
//
// Il gateway non conosce nessun dialetto e non conosce nemmeno il backend CAT.
// Per sapere COSA c'e' attaccato usa DecodiumRigDetector, che legge l'identita'
// USB delle porte e le abbina alle schede audio dello stesso apparato. Per
// controllarlo usa i ganci che gli passa chi lo ospita: cosi' funziona con il
// backend nativo, con Hamlib, con OmniRig o con TCI senza saperlo — e chi lo
// ospita non deve cambiare nulla qui quando cambia backend.
//
// Non apre e non configura il CAT da solo. La politica di connessione — quale
// profilo, quando riconnettere — appartiene all'applicazione, e aprire una
// seconda volta la stessa porta seriale e' il modo sicuro per non aprirla.
#pragma once

#include "DecoPortPacket.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <functional>

class QTimer;
class QUdpSocket;

class DecodiumDecoPortGateway : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString rigLabel READ rigLabel NOTIFY radioChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientsChanged)
    Q_PROPERTY(int sessionPort READ sessionPort NOTIFY runningChanged)
    // Su quali indirizzi ci si puo' collegare a questa radio. Serve a chi sta
    // davanti al PC di stazione per sapere cosa scrivere sull'altra macchina.
    Q_PROPERTY(QString primaryAddress READ primaryAddress NOTIFY runningChanged)
    Q_PROPERTY(QStringList addresses READ addresses NOTIFY runningChanged)
    // Quanti datagrammi arrivano e come vanno a finire. Serve a distinguere i
    // due guasti che da fuori si somigliano e non hanno nulla in comune: "non
    // arriva niente" (rete, firewall, porta) e "arriva ma lo rifiuto"
    // (password diversa, orologio sfasato). Senza questi numeri il gateway
    // tace in entrambi i casi, e chi pubblica non ha modo di sapere quale dei
    // due sta guardando.
    //
    // Sono contatori locali: il gateway continua a non rispondere NIENTE a chi
    // sbaglia la firma. Confermare la ricezione direbbe a un estraneo che il
    // pacchetto e' arrivato a destinazione, ed e' esattamente cio' che non si
    // vuole dire. Qui si scrive per il proprietario, non per la rete.
    Q_PROPERTY(QVariantMap traffico READ traffico NOTIFY trafficoChanged)

public:
    // I ganci verso la radio. Tutti facoltativi: quelli non forniti fanno
    // semplicemente si' che il gateway si dichiari senza controllo.
    struct RigHooks {
        std::function<bool()>                connected;
        std::function<double()>              frequencyHz;
        std::function<QString()>             modeName;     // nome del modo dell'applicazione
        std::function<bool()>                pttActive;
        std::function<bool()>                canTransmit;
        std::function<void(double)>          setFrequencyHz;
        std::function<void(const QString&)>  setModeName;
        std::function<void(bool)>            setPtt;

        // Gli strumenti. Hanno una firma diversa dai ganci qui sopra perche'
        // devono poter dire "non lo so": una radio senza misuratore di ROS non
        // deve far comparire un ROS, e nemmeno uno zero, che sul quadrante
        // significherebbe "adattamento perfetto". Restituiscono true solo se
        // la lettura c'e' stata davvero; il gancio non fornito vale come una
        // lettura mai avvenuta, e il campo non parte proprio.
        std::function<bool(double&)>         sMeterDbm;
        std::function<bool(double&)>         forwardPowerW;
        std::function<bool(double&)>         swr;
        std::function<bool(double&)>         alcPct;
        std::function<bool(double&)>         drainVoltage;
        std::function<bool(double&)>         drainCurrent;
        std::function<bool(double&)>         paTemperature;
        std::function<bool(double&)>         compressionDb;
        std::function<bool(double&)>         powerSettingPct;
    };

    explicit DecodiumDecoPortGateway(QObject* parent = nullptr);
    ~DecodiumDecoPortGateway() override;

    void setRigHooks(RigHooks hooks) { m_hooks = std::move(hooks); }

    // Senza chiave il gateway NON si accende. Pubblicare una radio in chiaro
    // vuol dire lasciare che chiunque sulla rete la sintonizzi e ascolti; una
    // porta senza serratura non e' una porta.
    void setAuthKey(const QByteArray& key) { m_authKey = key; }
    bool hasAuthKey() const { return !m_authKey.isEmpty(); }

    bool    running() const { return m_running; }
    QString rigLabel() const { return m_rigLabel; }
    QString status() const { return m_status; }
    int     clientCount() const { return m_clients.size(); }
    int     sessionPort() const { return m_sessionPort; }
    QStringList addresses() const;
    QString primaryAddress() const;

    // Cosa ha trovato il rilevamento: nome, porta, schede audio, quanto e'
    // sicuro e su cosa si basa. Solo lettura, nessun effetto sulla radio.
    Q_INVOKABLE QVariantMap detectedRadio() const { return m_detected; }
    Q_INVOKABLE void refreshDetection();

    Q_INVOKABLE bool start(int sessionPort = decoport::kSessionPort);
    Q_INVOKABLE void stop();

    // La frequenza di campionamento non si presume: la dichiara chi fornisce
    // l'audio, e il gateway la pubblica nel contesto. Decodium consegna 12 kHz
    // (il suo sink decima di 4 da 48), che per un modem e' tutto il passabanda
    // che serve a un quarto della banda di rete.
    void setAudioFormat(quint32 sampleRate, quint8 channels);

    // Audio RX in ingresso dal rubinetto che l'applicazione ha gia': niente
    // secondo QAudioSource sulla stessa scheda. I campioni arrivano a pezzi di
    // dimensione qualsiasi; qui vengono ricuciti in frame regolari da 10 ms.
    void pushRxAudio(const QVector<short>& samples, quint64 captureTsNs);

    // Il conto dei datagrammi, pronto per il QML. Vedi la proprieta' traffico.
    QVariantMap traffico() const;

signals:
    void runningChanged();
    void radioChanged();
    void statusChanged();
    void clientsChanged();
    // Emesso a cadenza dal timer del contesto, non a ogni pacchetto: sotto
    // carico i datagrammi sono migliaia al secondo e un segnale per ciascuno
    // costerebbe piu' del gateway stesso.
    void trafficoChanged();

    // Il gateway consegna i campioni TX quando e' arrivato il loro istante di
    // riproduzione, non quando sono arrivati dalla rete. E' tutta qui la
    // ragione dei timestamp: il jitter di rete finisce in un buffer invece che
    // nell'allineamento allo slot.
    void txAudioDue(const QVector<short>& samples);
    void txKeyRequested(bool on);

private slots:
    void onSessionDatagrams();
    void onAnnounceTick();
    void onContextTick();
    void onPlayoutTick();

private:
    struct Client {
        QHostAddress address;
        quint16      port {0};
        qint64       lastSeenMs {0};
        quint32      rxSequence {0};
    };
    struct PendingTx {
        quint64        dueNs {0};
        QVector<short> samples;
    };

    static QString clientKey(const QHostAddress& addr, quint16 port);

    void setStatus(const QString& s);
    decoport::Context buildContext() const;
    void sendTo(const Client& c, decoport::Type type, const QByteArray& payload, quint64 tsNs);
    void broadcastToClients(decoport::Type type, const QByteArray& payload, quint64 tsNs);
    void handleCommand(const decoport::Header& h, const QByteArray& payload,
                       const QHostAddress& from, quint16 fromPort);
    void handleAudioTx(const decoport::Header& h, const QByteArray& payload);
    void touchClient(const QHostAddress& addr, quint16 port);
    void reapClients(qint64 nowMs);
    // Vero se questo mittente e' in castigo per firme sbagliate.
    bool isBlocked(const QString& key, qint64 nowMs) const;
    void noteAuthFailure(const QString& key, const QHostAddress& addr, qint64 nowMs);

    RigHooks   m_hooks;
    QByteArray m_authKey;

    // Tentativi falliti per mittente. Non e' un firewall: serve a rendere
    // inutile provare le password a raffica su una rete che gia' raggiunge il
    // gateway, e a lasciarne traccia nel log.
    struct AuthFailures {
        int    count {0};
        qint64 windowStartMs {0};
        qint64 blockedUntilMs {0};
    };
    QHash<QString, AuthFailures> m_authFailures;

    // Il conto di cosa arriva sul socket di sessione. Tutti i datagrammi
    // passano da una sola di queste voci, cosi' la somma delle respinte piu'
    // le accettate torna sempre uguale ai ricevuti: se non torna, manca un
    // ramo in onSessionDatagrams().
    struct Traffico {
        quint64 ricevuti {0};      // datagrammi arrivati, qualunque esito
        quint64 accettati {0};     // firma buona e tempo buono: elaborati
        quint64 daBloccati {0};    // mittente in castigo: nemmeno guardati
        quint64 malformati {0};    // non e' roba nostra, o e' rovinata
        quint64 firmaErrata {0};   // password diversa da quella del gateway
        quint64 fuoriTempo {0};    // firma buona, timestamp fuori finestra
        QString ultimoMittente;    // chi ha bussato per ultimo, respinto o no
        qint64  ultimoMs {0};      // quando, per dire "3 minuti fa"
    };
    Traffico m_traffico;
    // Vero quando m_traffico e' cambiato dall'ultimo trafficoChanged().
    bool m_trafficoSporco {false};
    // Quando si e' scritta l'ultima riga di traffico nel log: una al minuto.
    qint64 m_traficoUltimoLogMs {0};
    // Vero finche' i numeri correnti non sono finiti nel log. Distinto da
    // m_trafficoSporco, che serve alla finestra e si azzera molto piu' spesso.
    bool m_traficoDaRegistrare {false};

    QUdpSocket* m_session {nullptr};
    QUdpSocket* m_announce {nullptr};
    QTimer*     m_announceTimer {nullptr};
    QTimer*     m_contextTimer {nullptr};
    QTimer*     m_playoutTimer {nullptr};

    QHash<QString, Client> m_clients;
    QVector<PendingTx>     m_txQueue;

    bool     m_running {false};
    int      m_sessionPort {decoport::kSessionPort};
    quint32  m_streamId {0};
    quint32  m_announceSeq {0};
    quint32  m_contextSeq {0};
    quint32  m_statusSeq {0};

    QString     m_rigLabel;
    QString     m_status;
    QVariantMap m_detected;
    QString     m_audioInputName;
    QString     m_audioOutputName;

    quint32 m_sampleRate {decoport::kDefaultSampleRate};
    quint8  m_channels {1};
    quint16 m_txAudioLeadMs {200};

    QVector<short> m_rxAccum;      // resto fra un frame e il successivo

    // Solo per la diagnosi: quanti frame TX sono arrivati dopo il loro momento.
    qint64 m_txLateFrames {0};
    qint64 m_txPlayedFrames {0};
    qint64 m_rxFramesSent {0};
};
