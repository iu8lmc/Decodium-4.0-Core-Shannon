// DecoRTTY — auto-centratura: porta la sintonia sul segnale che c'è davvero.
//
// L'AFC risponde alla domanda «di quanto è derivato il segnale che sto già
// decodificando». Questa risponde a un'altra: «dove sta il segnale». Sono due
// mestieri diversi e vanno tenuti separati — allargare la finestra dell'AFC
// finché ci sta dentro tutta la banda vocale sembra la scorciatoia, ma
// significa che una portante di passaggio può rubare la sintonia a una
// trasmissione in corso.
//
// Perciò qui si cerca soltanto quando non si sta copiando nulla, e ci si ferma
// appena il decodificatore aggancia. Un operatore che sta leggendo un
// collegamento non viene mai spostato.
//
// La seconda cosa che fa è provare il verso. Mark e space sono due toni e basta:
// nessuna analisi dello spettro può dire quale sia quale, perché la differenza
// sta nel significato, non nel segnale. L'unico modo di scoprirlo è provare a
// decodificare in un verso e, se non ne esce niente, provare nell'altro.
#pragma once

#include "dsp/RttyDemodulator.h"

namespace decortty::dsp {

class AutoTuner {
public:
    struct Decision {
        bool  changed{false};
        float markHz{0.0f};
        bool  reverse{false};
        // Che cosa e' stato deciso, per poterlo dire all'operatore invece di
        // spostargli la sintonia in silenzio.
        enum class Action { None, Retune, FlipReverse } action{Action::None};
        float marginDb{0.0f};
    };

    // Estremi della ricerca. I toni RTTY stanno nel passa-banda della radio, e
    // cercare oltre significa soltanto trovarci il rumore dei bordi.
    void setRange(float minHz, float maxHz);
    void setEnabled(bool on) { m_enabled = on; }
    bool enabled() const { return m_enabled; }

    // Riparte da capo: da chiamare quando l'operatore cambia qualcosa a mano.
    void reset();

    // Da chiamare a intervalli regolari con il tempo trascorso dall'ultima
    // volta. Restituisce una decisione solo quando c'e' qualcosa da cambiare.
    Decision update(const RttyDemodulator& demod, const RttyParams& params,
                    double elapsedSec);

private:
    // Quanto il tono piu' debole deve superare il fondo perche' valga la pena
    // muoversi. Sotto questa soglia, quel che si vede e' rumore con una forma.
    static constexpr float kMinMarginDb = 6.0f;
    // Oltre questo squilibrio fra i due toni non e' una coppia FSK: e' una
    // portante con del rumore alla distanza giusta.
    static constexpr float kMaxBalanceDb = 10.0f;
    // Quanto lontana dev'essere la coppia trovata perche' spostarsi abbia senso.
    // Sotto, ci pensa l'AFC, e meglio: senza azzerare i filtri.
    static constexpr float kMinMoveHz = 12.0f;
    // Tempo di copia mancata prima di intervenire. Abbastanza da lasciar
    // agganciare una trasmissione che sta partendo.
    static constexpr double kIdleBeforeSearch = 2.0;
    // Tempo concesso a un tentativo prima di provare il verso opposto.
    static constexpr double kSettleAfterMove = 3.0;
    // Quanto si resta fermi dopo aver copiato qualcosa su questa frequenza.
    //
    // In un collegamento le pause fra un passaggio e l'altro durano decine di
    // secondi: l'altro sta scrivendo. Cercare in quel silenzio significa
    // spostare la sintonia e girare il verso proprio mentre l'operatore aspetta
    // la risposta — e quando la risposta arriva non si copia piu' niente. Se
    // qui si e' agganciato di recente, la frequenza e' quella giusta e si sta
    // fermi.
    static constexpr double kHoldAfterLock = 90.0;

    bool   m_enabled{false};
    float  m_minHz{400.0f};
    float  m_maxHz{2600.0f};
    double m_idleSec{0.0};
    double m_sinceActionSec{0.0};
    // Quante volte di fila si e' provato il verso senza cavarne nulla: dopo un
    // giro completo si smette di rigirare la manopola e si aspetta che cambi
    // qualcosa nell'aria.
    int    m_flips{0};
    bool   m_wasLocked{false};
    // Da quanto non si copia piu' nulla su questa frequenza. Grande quanto
    // basta a coprire il passaggio di chi sta scrivendo dall'altra parte.
    double m_sinceLockSec{1e9};
};

} // namespace decortty::dsp
