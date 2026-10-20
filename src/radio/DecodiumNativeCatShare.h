// Condivisione nel protocollo della radio (DecoPort).
//
// La CAT condivisa (DecodiumCatShare) offre la radio in rete come rigctld: la
// usa chi conosce Hamlib. Questa offre la stessa radio nel SUO protocollo, a
// chi la conosce per nome: un logger configurato per "FT-991A su COM12" resta
// configurato cosi', e COM12 diventa una porta virtuale dietro cui c'e'
// Decodium invece della radio.
//
// Come funziona. Quando Decodium apre il CAT, la seriale vera la prende il
// multiplexer di DecoPort (DecoPortCatMux), e Hamlib gli parla attraverso una
// presa su 127.0.0.1 invece che dalla COM: per Decodium cambia solo il
// percorso. Il multiplexer mette in fila i comandi di tutti, uno alla volta, e
// consegna ogni risposta a chi l'ha chiesta. Gli altri programmi arrivano da
// una porta COM virtuale (com0com) o da una presa TCP grezza.
//
// Il multiplexer ha un thread suo: nel thread dell'interfaccia ogni
// rallentamento della grafica ritarderebbe il CAT, PTT compreso.
//
// Stessa politica della CAT condivisa: a interruttore spento non cambia
// niente; la trasmissione agli altri programmi e' vietata finche' non la si
// abilita (Decodium trasmette sempre); se qualcosa non va, Decodium apre la
// COM come ha sempre fatto e la condivisione dice perche' non e' partita.
//
// Le porte virtuali le crea com0com, il "Null-modem emulator" di Vyacheslav
// Frolov (GPL 2 o successiva, https://com0com.sourceforge.net/), distribuito
// con Decodium invariato: vedi third_party/com0com/NOTICE.md.
//
// By IU8LMC

#ifndef DECODIUMNATIVECATSHARE_H
#define DECODIUMNATIVECATSHARE_H

#include "DecoPortSerialLine.h"

#include <QObject>
#include <QString>

class DecoPortCatMux;
class QThread;

class DecodiumNativeCatShare : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    enabled          READ enabled          NOTIFY configChanged)
    Q_PROPERTY(bool    allowPtt         READ allowPtt         NOTIFY configChanged)
    Q_PROPERTY(int     rawPort          READ rawPort          NOTIFY configChanged)
    Q_PROPERTY(bool    active           READ active           NOTIFY stateChanged)
    Q_PROPERTY(QString status           READ status           NOTIFY stateChanged)
    Q_PROPERTY(QString virtualPort      READ virtualPort      NOTIFY stateChanged)
    Q_PROPERTY(QString virtualStatus    READ virtualStatus    NOTIFY stateChanged)
    Q_PROPERTY(int     programs         READ programs         NOTIFY stateChanged)
    Q_PROPERTY(QString traffic          READ traffic          NOTIFY stateChanged)
    Q_PROPERTY(bool    busy             READ busy             NOTIFY stateChanged)
    Q_PROPERTY(bool    com0comInstalled READ com0comInstalled NOTIFY stateChanged)
    Q_PROPERTY(bool    com0comBundled   READ com0comBundled   CONSTANT)

public:
    explicit DecodiumNativeCatShare(QObject* parent = nullptr);
    ~DecodiumNativeCatShare() override;

    bool    enabled()  const { return m_enabled; }
    bool    allowPtt() const { return m_allowPtt; }
    int     rawPort()  const { return m_rawPort; }
    bool    active()   const { return m_active; }
    QString status()   const { return m_status; }
    QString virtualPort() const { return m_vcomOpen ? m_vcomVisible : QString(); }
    QString virtualStatus() const { return m_vcomStatus; }
    int     programs() const { return m_programs; }
    QString traffic()  const { return m_traffic; }
    bool    busy()     const { return m_busy; }
    bool    com0comInstalled() const;
    bool    com0comBundled() const;

    // La coppia virtuale ricordata ("COM6|CNCB0"): si riapre senza chiedere
    // niente a nessuno. Il bridge la salva quando cambia.
    void configure(bool enabled, bool allowPtt, int rawPort, const QString& rememberedPair);
    QString rememberedPair() const { return m_pair; }

    // Chiamato dal CAT di Decodium quando apre la radio. Restituisce l'indirizzo
    // da dare a Hamlib ("127.0.0.1:porta"), oppure vuoto: allora Decodium apre
    // la COM come sempre, e `why` (se non vuoto) dice perche'.
    // dataBits, stopBits 0 e handshake vuoto vogliono dire "come dichiara
    // Hamlib per quel modello". pttOnCatLines: il PTT usa RTS o DTR della
    // stessa porta CAT, che attraverso una presa di rete non si possono
    // muovere: in quel caso la condivisione non parte.
    QString redirect(const QString& port, const QString& rigName,
                     DecoPortLineSettings settings, bool pttOnCatLines, QString* why);
    // Il CAT di Decodium e' stato chiuso: la seriale torna libera.
    void release();

    // Installa com0com dalla copia inclusa (se manca) e crea la coppia di
    // porte. Chiede l'amministratore a Windows: si chiama solo da un'azione
    // esplicita dell'utente.
    Q_INVOKABLE void createVirtualPort();

signals:
    void configChanged();
    void stateChanged();
    // La coppia da ricordare e' cambiata: il bridge la salva nel profilo.
    void pairRemembered(const QString& pair);

private:
    void startMux(const QString& port, const QString& protocol, const DecoPortLineSettings& s,
                  QString* why);
    void stopMux();
    void openVirtualTap();
    void onSnapshot(int programs, const QString& traffic, bool open);
    void setStatus(const QString& s);

    bool    m_enabled {false};
    bool    m_allowPtt {false};
    int     m_rawPort {0};
    QString m_pair;            // "COM6|CNCB0"

    QThread*        m_thread {nullptr};
    QObject*        m_ctx {nullptr};    // vive nel thread del multiplexer
    DecoPortCatMux* m_mux {nullptr};    // toccato solo da quel thread

    QString m_key;             // porta e parametri del multiplexer in corso
    QString m_address;         // la presa per Hamlib
    bool    m_active {false};
    QString m_status;
    QString m_vcomVisible;
    bool    m_vcomOpen {false};
    QString m_vcomStatus;
    int     m_programs {0};
    QString m_traffic;
    bool    m_busy {false};
};

#endif // DECODIUMNATIVECATSHARE_H
