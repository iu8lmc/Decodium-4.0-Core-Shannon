#include "dsp/AutoTuner.h"

#include <algorithm>
#include <cmath>

namespace decortty::dsp {

void AutoTuner::setRange(float minHz, float maxHz)
{
    m_minHz = std::min(minHz, maxHz);
    m_maxHz = std::max(minHz, maxHz);
}

void AutoTuner::reset()
{
    m_idleSec        = 0.0;
    m_sinceActionSec = 0.0;
    m_flips          = 0;
    m_wasLocked      = false;
    m_sinceLockSec   = 1e9;
}

AutoTuner::Decision AutoTuner::update(const RttyDemodulator& demod,
                                      const RttyParams& params,
                                      double elapsedSec)
{
    Decision decision;
    decision.markHz  = params.markHz;
    decision.reverse = params.reverse;

    if (!m_enabled)
        return decision;

    m_sinceActionSec += elapsedSec;

    // Agganciati: si sta copiando, e non c'e' niente di piu' dannoso che
    // spostare la sintonia adesso. Il contatore dei tentativi si azzera, cosi'
    // la prossima caccia riparte pulita.
    if (demod.locked()) {
        m_idleSec      = 0.0;
        m_flips        = 0;
        m_wasLocked    = true;
        m_sinceLockSec = 0.0;
        return decision;
    }
    m_sinceLockSec += elapsedSec;

    // Si e' copiato da poco proprio qui: e' una pausa fra due passaggi, non una
    // frequenza sbagliata.
    if (m_sinceLockSec < kHoldAfterLock)
        return decision;

    // Appena persa la copia si concede il tempo del volano: una pausa fra due
    // righe non e' la fine della trasmissione.
    if (m_wasLocked) {
        m_wasLocked = false;
        m_idleSec   = 0.0;
    }

    m_idleSec += elapsedSec;
    if (m_idleSec < kIdleBeforeSearch)
        return decision;

    const RttyDemodulator::TonePair pair = demod.searchTonePair(m_minHz, m_maxHz);
    if (!pair.found || pair.marginDb < kMinMarginDb || pair.balanceDb > kMaxBalanceDb)
        return decision;   // non c'e' niente che somigli a una coppia di toni

    const float delta = std::abs(pair.markHz - params.markHz);
    if (delta >= kMinMoveHz) {
        // C'e' un segnale, ed e' altrove. Ci si sposta e si riparte da capo con
        // il conteggio dei versi: quello giusto per il vecchio segnale non dice
        // niente sul nuovo.
        decision.changed  = true;
        decision.markHz   = pair.markHz;
        decision.action   = Decision::Action::Retune;
        decision.marginDb = pair.marginDb;
        m_idleSec         = 0.0;
        m_sinceActionSec  = 0.0;
        m_flips           = 0;
        return decision;
    }

    // Siamo gia' centrati sulla coppia e non ne esce nulla. Restano due
    // spiegazioni: il verso e' sbagliato, oppure quello che si vede non e' RTTY
    // che possiamo leggere. La prima si prova; della seconda ci si accorge
    // perche' provare non serve a niente.
    if (m_sinceActionSec >= kSettleAfterMove && m_flips < 2) {
        decision.changed  = true;
        decision.reverse  = !params.reverse;
        decision.action   = Decision::Action::FlipReverse;
        decision.marginDb = pair.marginDb;
        ++m_flips;
        m_sinceActionSec = 0.0;
        m_idleSec        = 0.0;
    }
    return decision;
}

} // namespace decortty::dsp
