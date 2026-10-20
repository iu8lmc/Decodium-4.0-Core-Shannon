// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DecoPortCatFraming.h"
#include "DecoPortSerialLine.h"

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <optional>

class QIODevice;
class QTcpServer;
class QTimer;

// Una porta CAT, piu' programmi, nel protocollo della radio.
//
// rigctld condivide una radio traducendola nella lingua di Hamlib: chi non
// parla Hamlib resta fuori, e i programmi vanno riconfigurati. Il multiplexer
// fa l'opposto: tiene la porta fisica e la offre com'e', nel dialetto nativo,
// a quante prese servono. Un logger configurato per "FT-991A su COM12" resta
// configurato cosi': cambia solo che COM12 e' una porta virtuale dietro cui
// c'e' il multiplexer invece della radio. Anche Hamlib e' un cliente fra gli
// altri, attraverso una presa TCP locale.
//
// I comandi passano uno alla volta, nell'ordine in cui arrivano: e' cio' che
// permette di riconoscere a chi va ogni risposta. Quello che la radio dice da
// sola (AI, transceive) va a tutti quelli che lo vogliono sentire.
//
// Il PTT ha un padrone. Se il programma che ha mandato la radio in
// trasmissione chiude la porta o cade, il multiplexer la riporta in ricezione.
class DecoPortCatMux : public QObject
{
    Q_OBJECT

public:
    explicit DecoPortCatMux(QObject* parent = nullptr);
    ~DecoPortCatMux() override;

    void setDialect(decoport::cat::Dialect d) { m_dialect = d; }
    decoport::cat::Dialect dialect() const { return m_dialect; }

    // La porta della radio: "COM5", oppure "tcp:host:porta".
    bool open(const QString& physical, const DecoPortLineSettings& settings);
    void close();
    bool isOpen() const;
    QString physical() const { return m_line.spec(); }
    QString errorString() const { return m_error; }

    // Una presa TCP su 127.0.0.1 che parla il CAT grezzo, come ser2net.
    // port 0 ne sceglie una libera; restituisce quella usata, 0 se non si apre.
    // `unsolicited` dice se la presa riceve anche quello che la radio dice da
    // sola: un programma che ha acceso l'AI lo vuole, Hamlib no.
    // `trusted`: la presa di chi possiede la radio (l'applicazione che ospita
    // il multiplexer), a cui il divieto di PTT qui sotto non si applica.
    quint16 addTcpTap(quint16 port, const QString& name, bool unsolicited = true,
                      bool trusted = false);
    // L'estremo nascosto di una coppia virtuale: il programma apre l'altro.
    // `openByDcd`: il DCD segue l'apertura dell'altra parte (le coppie create
    // da DecoPort); altrimenti si guarda il DSR, cioe' il DTR del programma.
    bool addSerialTap(const QString& portName, const QString& name, bool openByDcd = false,
                      QString* error = nullptr);

    // Se le prese non fidate possono mandare la radio in trasmissione. Di
    // serie si': il multiplexer da solo non sa chi c'e' dall'altra parte. Chi
    // lo ospita puo' vietarlo, e allora i loro comandi di trasmissione non
    // arrivano alla radio (su CI-V ricevono "non capito", FA). Vale solo dove
    // il dialetto permette di riconoscerli: kenwood, newcat, civ.
    void setPttAllowed(bool allowed) { m_pttAllowed = allowed; }
    bool pttAllowed() const { return m_pttAllowed; }

    // Una mappa per presa: name, kind, endpoint, clients, keyed, error.
    QVariantList taps() const;

    qint64 commandsSent() const { return m_sent; }
    qint64 repliesRouted() const { return m_routed; }
    qint64 broadcasts() const { return m_broadcast; }
    qint64 timeouts() const { return m_timeouts; }
    qint64 pttRefusedCount() const { return m_pttRefused; }

signals:
    void lost(const QString& why);
    void tapsChanged();
    // Il PTT e' stato abbassato perche' chi l'aveva alzato non c'e' piu'.
    void pttReleased(const QString& tapName);
    // Una presa non fidata ha chiesto la trasmissione mentre e' vietata.
    void pttRefused(const QString& tapName);

private:
    struct Tap {
        int        id {0};
        QString    name;
        QString    kind;       // tcp, serial
        QString    endpoint;
        QIODevice* io {nullptr};
        DecoPortSerialLine* line {nullptr};
        QByteArray inbox;
        QTimer*    coalesce {nullptr};
        bool       unsolicited {true};
        // Solo per le porte virtuali: c'e' un programma dall'altra parte.
        bool       clientPresent {false};
        // L'ultimo stato letto della linea che dice "c'e' qualcuno". Conta
        // solo il passaggio da 1 a 0: un programma che non alza mai DTR non va
        // dato per sparito appena scrive.
        int        lastDsr {-1};
        bool       openByDcd {false};
        bool       trusted {false};
    };
    struct Server {
        QTcpServer* server {nullptr};
        QString     name;
        bool        unsolicited {true};
        bool        trusted {false};
        QString     error;
    };
    struct Pending {
        int tapId {-1};   // -1: un comando del multiplexer stesso
        decoport::cat::Command cmd;
    };

    int  addTap(Tap tap);
    void onPhysicalData();
    void onTapData(int tapId);
    void takeTapCommands(int tapId);
    void pump();
    void finish(bool timedOut);
    void deliver(int tapId, const QByteArray& bytes);
    void broadcast(const QByteArray& bytes);
    void clientGone(int tapId);
    void removeTap(int tapId);
    void pollPins();

    decoport::cat::Dialect m_dialect {decoport::cat::Dialect::Binary};
    DecoPortSerialLine m_line;
    QString    m_error;
    QByteArray m_rx;

    QHash<int, Tap>  m_taps;
    QVector<Server>  m_servers;
    int              m_nextTapId {1};

    QQueue<Pending>        m_queue;
    std::optional<Pending> m_inFlight;
    bool    m_gotReply {false};
    QTimer* m_timeout {nullptr};
    QTimer* m_silence {nullptr};
    QTimer* m_pins {nullptr};

    int    m_keyedTap {-1};
    quint8 m_keyedCiv {0};
    bool   m_pttAllowed {true};

    qint64 m_sent {0};
    qint64 m_routed {0};
    qint64 m_broadcast {0};
    qint64 m_timeouts {0};
    qint64 m_pttRefused {0};
};
