// La radio di Decodium, vista come una radio qualsiasi.
//
// DecoRTTY nasce per andare a cercarsi una radio da solo: un FlexRadio in rete,
// o una FT-991A dietro il gateway, in entrambi i casi con VITA-49 sul filo.
// Dentro Decodium quel lavoro e' gia' fatto, e rifarlo era il modo sicuro per
// non farlo: la radio e' una sola, la porta seriale e' una sola, e il CAT
// dell'applicazione ce l'ha gia' in mano. Le due strade di rete non trovavano
// niente perche' non c'era niente da trovare — non era un guasto, era una
// domanda posta a chi non poteva rispondere.
//
// Questo collegamento chiude il cerchio: presenta al motore RTTY la radio che
// Decodium sta gia' governando. Sotto l'interfaccia RadioLink non cambia
// niente, quindi il decodificatore, i pannelli e il percorso di trasmissione
// restano quelli, senza sapere da dove arrivi la frequenza.
//
// Non conosce DecodiumBridge e non lo include: riceve dei ganci, come fa il
// gateway DecoPort. E' cio' che tiene src/rtty compilabile per conto suo e
// impedisce che l'applicazione e il sottosistema RTTY si tengano per il
// bavero a vicenda.
#pragma once

#include "link/RadioLink.h"

#include <QString>
#include <QTimer>
#include <QVector>

#include <functional>

namespace decortty::link {

class DecodiumLink final : public RadioLink {
    Q_OBJECT

public:
    // I ganci verso il CAT dell'applicazione. Quelli non forniti fanno
    // semplicemente si' che la radio si dichiari senza quella capacita': un
    // gancio assente e' "non lo so", mai un valore inventato.
    struct Ganci {
        std::function<bool()>               connesso;
        std::function<QString()>            nomeRadio;
        std::function<double()>             frequenzaHz;
        std::function<QString()>            modo;
        std::function<bool()>               inTrasmissione;
        std::function<bool()>               puoTrasmettere;
        std::function<void(double)>         impostaFrequenzaHz;
        std::function<void(const QString&)> impostaModo;
        std::function<void(bool)>           impostaPtt;
        // L'audio del modulatore RTTY verso la radio, mono a 12 kHz: e' la
        // stessa strada che DecoPort usa per l'audio che arriva dai suoi
        // client, con i suoi ritegni gia' collaudati.
        std::function<void(const QVector<short>&)> mandaAudioTx;
    };

    explicit DecodiumLink(Ganci ganci, QObject* parent = nullptr);
    ~DecodiumLink() override;

    bool    isConnected() const override;
    QString statusText() const override { return m_stato; }
    QString radioName() const override;
    double  frequencyMhz() const override;
    QString mode() const override;
    bool    isTransmitting() const override;
    bool    canTransmit() const override;
    int     signalStrengthDbm() const override { return 0; }
    bool    requiresFullScaleTransmitAudio() const override;

    void disconnectRadio() override;
    void setFrequencyMhz(double mhz) override;
    void setMode(const QString& mode) override;
    // Il passabanda lo governa Decodium con le sue impostazioni: qui non si
    // tocca. Stringere il filtro da RTTY vorrebbe dire cambiarlo anche per FT8,
    // che sta decodificando nella stessa finestra e con la stessa radio.
    void setFilter(int lowHz, int highHz) override;
    void applyRttyProfile(int markHz, int shiftHz) override;
    void setTransmit(bool on) override;

    // L'audio del modulatore RTTY verso la radio. Passa dalla stessa strada
    // che Decodium apre per l'audio dei client DecoPort — una sola uscita, con
    // i ritegni gia' scritti: non suona nulla mentre il sequencer dei modi
    // digitali sta trasmettendo o accordando.
    int sendTransmitAudio(const float* samples, int count) override;

    // L'audio ricevuto non lo produce questo collegamento: arriva dal percorso
    // audio dell'applicazione, che lo consegna qui. Vedi DecoRttyHost.
    void consegnaAudio(const std::vector<float>& campioni, int frames);

private:
    void sondaggio();

    Ganci   m_ganci;
    QTimer* m_sonda {nullptr};
    QString m_stato;

    // L'ultimo stato visto, per emettere i segnali solo quando cambia davvero:
    // il sondaggio gira di continuo, i binding del QML no.
    bool    m_ultimoConnesso {false};
    double  m_ultimaFrequenzaHz {0.0};
    QString m_ultimoModo;
    bool    m_ultimaTrasmissione {false};

    // Il buffer della conversione 24 -> 12 kHz, tenuto qui per non riallocarlo
    // a ogni pezzo di audio trasmesso.
    QVector<short> m_txBuffer;
};

} // namespace decortty::link
