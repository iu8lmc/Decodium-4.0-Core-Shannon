// DecoRTTY — what the application knows about a radio.
//
// Due cose molto diverse stanno dietro questa interfaccia: la radio che
// Decodium governa dal suo CAT, e una scheda audio del PC su cui un altro
// programma mette il suo audio. Sopra la cucitura sono lo stesso oggetto — una
// frequenza, un modo, un PTT e un flusso di campioni — ed e' cio' che tiene il
// decodificatore, l'interfaccia e il percorso di trasmissione liberi dal sapere
// quale delle due sia collegata.
//
// Nel progetto originale le due cose erano un FlexRadio e una FT-991A dietro un
// gateway, entrambe raggiunte in rete con VITA-49. Quel trasporto qui non c'e'
// piu': la radio e' una sola e sta su una porta seriale.
#pragma once

#include <QObject>
#include <QString>

#include <vector>

namespace decortty::link {

class RadioLink : public QObject {
    Q_OBJECT

public:
    explicit RadioLink(QObject* parent = nullptr) : QObject(parent) {}
    ~RadioLink() override = default;

    virtual bool    isConnected() const = 0;
    virtual QString statusText() const = 0;
    // Il nome della stazione con cui si sta condividendo la radio, vuoto quando
    // si lavora da soli. Riguardava i FlexRadio, dove piu' programmi possono
    // usare lo stesso apparato insieme: senza quelli resta sempre vuoto, e chi
    // lo mostra sa gia' nasconderlo.
    virtual QString sharedWith() const { return {}; }
    virtual QString radioName() const = 0;
    virtual double  frequencyMhz() const = 0;
    virtual QString mode() const = 0;
    virtual bool    isTransmitting() const = 0;
    virtual bool    canTransmit() const = 0;
    // Vero quando i comandi di sintonia arrivano davvero all'apparato. Non e' la
    // stessa cosa di canTransmit(): con la radio di Decodium si comanda la
    // sintonia ma non si trasmette, perche' il modulatore e' quello
    // dell'applicazione. E da una scheda audio non si comanda niente: si
    // ascolta e basta.
    virtual bool    canControl() const { return isConnected(); }
    virtual int     signalStrengthDbm() const = 0;

    // QMX detects the incoming tone only above its configurable rise
    // threshold (80% by default), and its manual requires full-scale PC audio.
    // Conventional AFSK radios keep the operator-adjustable level so their ALC
    // is not driven.
    virtual bool    requiresFullScaleTransmitAudio() const { return false; }

    virtual void disconnectRadio() = 0;
    virtual void setFrequencyMhz(double mhz) = 0;
    virtual void setMode(const QString& mode) = 0;
    virtual void setFilter(int lowHz, int highHz) = 0;
    // Put the radio into the state RTTY wants: a data mode on the upper
    // sideband, and a filter around the two tones.
    virtual void applyRttyProfile(int markHz, int shiftHz) = 0;
    virtual void setTransmit(bool on) = 0;

    // Modulator audio, mono at 24 kHz. Returns how many packets went out, or 0
    // when the link cannot transmit.
    virtual int sendTransmitAudio(const float* samples, int count) = 0;

signals:
    void connectionChanged();
    void statusTextChanged();
    void sliceChanged();
    void transmittingChanged();
    void metersChanged();
    void errorOccurred(const QString& message);

    // Receive audio: interleaved stereo at 24 kHz.
    void audioReady(const std::vector<float>& samples, int frames);
};

} // namespace decortty::link
