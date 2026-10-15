// Una radio remota, vista dall'applicazione: quello che serve al bridge per
// usarla al posto della scheda audio e del CAT locali, qualunque sia la strada.
//
// RadioLink e' il vocabolario minimo (frequenza, modo, PTT, audio); qui si
// aggiungono le cose che il bridge legge e chiama quando la radio remota prende
// il posto di quella locale: stato leggibile, sintonia, messaggi di stato, la
// latenza di trasmissione e i segnali dell'audio ricevuto. Le due strade che ci
// sono oggi, DecoPort in rete locale e Decolink tramite relay, stanno dietro
// questa stessa interfaccia: il bridge sceglie quale tenere accesa, non come
// parlarci.
#pragma once

#include "RadioLink.h"

#include <QString>
#include <QVector>

// Un istante del tasto CW: il tasto si apre o si chiude `deltaMs` millisecondi
// dopo l'evento precedente.
struct CwKeyEvent {
    quint16 deltaMs {0};
    bool down {false};
};

class RemoteRadioLink : public RadioLink {
    Q_OBJECT

public:
    explicit RemoteRadioLink(QObject* parent = nullptr) : RadioLink(parent) {}
    ~RemoteRadioLink() override = default;

    virtual double  frequencyHz() const = 0;
    virtual QString modeName() const = 0;
    virtual bool    ptt() const = 0;
    virtual double  sMeterDbm() const = 0;
    virtual QString status() const = 0;
    // Quanto prima dell'istante voluto va consegnato l'audio da trasmettere,
    // perche' la rete e il buffer del gateway non lo facciano arrivare tardi.
    virtual int     txAudioLeadMs() const = 0;
    virtual QString peerAddress() const = 0;
    // Identificatore del flusso audio, per chi deve distinguere piu' sorgenti
    // (SSTV). Zero se la strada non ne ha uno.
    virtual quint32 streamId() const { return 0; }

    // Comodita' che il bridge usa con tipi semplici.
    virtual void tune(double hz) = 0;
    virtual void setModeName(const QString& name) = 0;
    virtual void key(bool on) = 0;

    // False se questo accesso non puo' trasmettere (un ascoltatore): il bridge
    // lo chiede prima di alzare il PTT, cosi' non si manda in aria una richiesta
    // che il relay scarterebbe in silenzio.
    virtual bool canTransmit() const { return true; }

    // Non vuota se adesso non si puo' trasmettere per un motivo passeggero
    // (un altro operatore ha il PTT della stazione): il testo dice perche'.
    virtual QString txBlockedReason() const { return QString(); }

    // Il CW a tasto: invece dell'audio si mandano gli istanti del tasto e il
    // tono lo rigenera chi sta accanto alla radio. Solo Decolink lo sa fare.
    virtual bool supportsCwKey() const { return false; }
    virtual bool sendCwKey(const QVector<CwKeyEvent>& events, int toneHz)
    {
        Q_UNUSED(events)
        Q_UNUSED(toneHz)
        return false;
    }

signals:
    void statusChanged();
    void remoteStreamChanged(quint32 streamId);
    // Emesso in modo sincrono alla decodifica del pacchetto, prima della
    // consegna ordinaria di RadioLink::rxAudio. Solo consumatori limitati in
    // DirectConnection possono collegarsi qui.
    void rxAudioProduced(const QVector<short>& samples,
                         quint64 captureTsNs,
                         quint32 streamId);
};
