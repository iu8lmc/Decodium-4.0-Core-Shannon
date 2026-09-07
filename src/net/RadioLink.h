// L'interfaccia neutra verso "una radio", qualunque strada ci sia sotto.
//
// Nasce per DecoPort ma non ne dipende come protocollo: dipende solo dal suo
// modello di stato, che e' gia' il vocabolario neutro che serve (frequenza,
// modo per quello che FA, PTT, S-meter, cosa e' in linea). Un secondo backend
// — un FlexRadio, che in rete c'e' gia' per conto suo — si affianca qui senza
// che il resto dell'applicazione se ne accorga.
//
// Il momento in cui una cosa deve accadere e' parte della richiesta, non un
// dettaglio: `setPtt` e `sendTxAudio` prendono un istante. Con zero significa
// "adesso"; valorizzato significa "a quell'ora", ed e' cosi' che un modem FT8
// puo' stare sul confine di slot attraverso una rete.
#pragma once

#include "DecoPortPacket.h"

#include <QObject>
#include <QString>
#include <QVector>

class RadioLink : public QObject {
    Q_OBJECT

public:
    explicit RadioLink(QObject* parent = nullptr) : QObject(parent) {}
    ~RadioLink() override = default;

    virtual bool    isLinked() const = 0;
    virtual QString rigLabel() const = 0;
    virtual decoport::Context state() const = 0;

    virtual void setFrequency(qint64 hz) = 0;
    virtual void setMode(decoport::Mode mode) = 0;
    // whenNs = 0 -> subito. Altrimenti l'istante Unix in nanosecondi.
    virtual void setPtt(bool on, quint64 whenNs = 0) = 0;
    // playAtNs e' quando il PRIMO campione deve raggiungere il modulatore.
    virtual void sendTxAudio(const QVector<short>& samples, quint64 playAtNs) = 0;

signals:
    void linkedChanged();
    void stateChanged();
    void rxAudio(const QVector<short>& samples, quint64 captureTsNs);
};
