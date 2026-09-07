// DecoRTTY — l'unico oggetto radio con cui parla l'interfaccia.
//
// Nel progetto originale cercava le radio in rete — FlexRadio con VITA-49, o
// una FT-991A dietro il gateway — e costruiva il collegamento giusto quando
// l'operatore ne sceglieva una. Dentro Decodium quella ricerca non trovava mai
// niente, e non per un guasto: la radio e' quella che l'applicazione governa
// gia' dal suo CAT, sull'unica porta seriale che c'e'. Cercarla in rete era
// chiedere a chi non poteva rispondere.
//
// Ne resta una sola: la radio che Decodium governa. Anche la scheda audio se
// n'e' andata — chi decide da dove arrivano audio e comandi e' Decodium, in un
// posto solo, e RTTY non ha piu' niente da connettere ne' da scegliere. Questa
// classe resta perche' l'interfaccia si lega a `radio.frequencyMhz` e ai suoi
// segnali, ma sotto ora c'e' un collegamento solo.
#pragma once

#include "link/DecodiumLink.h"

#include <QObject>
#include <QVariantList>

#include <memory>

namespace decortty::link {

class RadioHub : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString radioName READ radioName NOTIFY connectionChanged)
    Q_PROPERTY(double frequencyMhz READ frequencyMhz NOTIFY sliceChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY sliceChanged)
    Q_PROPERTY(bool transmitting READ transmitting NOTIFY transmittingChanged)
    Q_PROPERTY(bool canTransmit READ canTransmit NOTIFY connectionChanged)
    Q_PROPERTY(bool requiresFullScaleTransmitAudio READ requiresFullScaleTransmitAudio NOTIFY connectionChanged)
    Q_PROPERTY(int signalStrengthDbm READ signalStrengthDbm NOTIFY metersChanged)
    // La stazione con cui si condivide la radio. Resta sempre vuoto ora che i
    // FlexRadio non ci sono piu': lo tiene solo la barra di stato, che sa gia'
    // nasconderla quando e' vuota.
    Q_PROPERTY(QString sharedWith READ sharedWith NOTIFY connectionChanged)
    // Vero quando banda e modo si possono davvero cambiare: cioe' quando il CAT
    // di Decodium e' connesso alla radio. Se non lo e', i comandi non
    // arriverebbero da nessuna parte e l'interfaccia deve dirlo invece di
    // fingere.
    Q_PROPERTY(bool canControl READ canControl NOTIFY connectionChanged)
    // Il piano di banda, per la barra sopra il waterfall.
    Q_PROPERTY(QVariantList bands READ bands CONSTANT)
    // In quale banda siamo, -1 se fuori da tutte. Segue la radio anche quando a
    // spostarla e' la manopola o un altro programma.
    Q_PROPERTY(int currentBand READ currentBand NOTIFY sliceChanged)
    // I modi che hanno senso per un decodificatore AFSK. RTTY-U e RTTY-L non ci
    // sono apposta: la' l'apparato aspetta il tasto FSK e l'audio non uscirebbe.
    Q_PROPERTY(QStringList modes READ modes CONSTANT)

public:
    explicit RadioHub(QObject* parent = nullptr);
    ~RadioHub() override;

    bool    connected() const;
    QString statusText() const;
    QString radioName() const;
    QVariantList radioList() const;
    double  frequencyMhz() const;
    QString mode() const;
    bool    transmitting() const;
    bool    canTransmit() const;
    bool    requiresFullScaleTransmitAudio() const;
    int     signalStrengthDbm() const;
    // I dispositivi in uso, per poterli riproporre al prossimo avvio.
    QString sharedWith() const;
    bool    canControl() const;
    QVariantList bands() const;
    int     currentBand() const;
    QStringList  modes() const;

    // Si attacca alla radio che Decodium sta gia' governando. E' la strada
    // normale: la chiama l'host all'avvio, e non c'e' niente da scegliere.
    void collegaADecodium(DecodiumLink::Ganci ganci);

    // L'audio ricevuto dall'applicazione, quando la radio e' quella di
    // Decodium. Non fa nulla con una scheda audio, che il suo audio se lo
    // prende da sola.
    void consegnaAudioDecodium(const std::vector<float>& campioni, int frames);

    // Porta la radio sul segmento RTTY della banda scelta. Un secondo tocco
    // sulla banda in cui siamo gia' non fa niente: chi lo preme si aspetta di
    // essere gia' arrivato, non di essere riportato indietro dalla frequenza
    // che si e' cercato.
    Q_INVOKABLE void tuneToBand(int index);

    Q_INVOKABLE void setFrequencyMhz(double mhz);
    Q_INVOKABLE void setMode(const QString& mode);
    Q_INVOKABLE void setFilter(int lowHz, int highHz);
    Q_INVOKABLE void applyRttyProfile(int markHz, int shiftHz);
    Q_INVOKABLE void setTransmit(bool on);

    // Used by the engine for the transmit path.
    int sendTransmitAudio(const float* samples, int count);

signals:
    void connectionChanged();
    void statusTextChanged();
    void sliceChanged();
    void transmittingChanged();
    void metersChanged();
    void errorOccurred(const QString& message);
    void audioReady(const std::vector<float>& samples, int frames);

private:
    void adopt(RadioLink* link);
    void releaseLink();

    RadioLink*          m_link{nullptr};   // di proprieta'; uno alla volta
    // Lo stesso puntatore di m_link quando il collegamento e' quello di
    // Decodium, nullo altrimenti: serve a consegnargli l'audio senza dover
    // indovinare il tipo dietro l'interfaccia.
    DecodiumLink*       m_decodium{nullptr};
    QString             m_idleStatus;
};

} // namespace decortty::link
