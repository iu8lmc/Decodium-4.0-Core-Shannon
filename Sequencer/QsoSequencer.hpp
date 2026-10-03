#ifndef DECODIUM_QSO_SEQUENCER_HPP
#define DECODIUM_QSO_SEQUENCER_HPP

// Fase 1 port mobile — step C strangler: scheletro della classe che, allo step
// D, ospiterà i corpi della logica di sequencing oggi in DecodiumBridge
// (checkAndStartPeriodicTx, autoSequenceStep, advanceQsoState, …). Opera SOLO
// su QsoSequencerState (lo stato, già raggruppato negli step B/B2/B3) e
// ISequencerSink (il seam, step C). Nessuna dipendenza da Qt-GUI, QSettings,
// audio o CAT → compilabile identico su desktop e mobile.
//
// Migrazione (step D): ogni funzione si sposta qui una alla volta; DecodiumBridge
// delega chiamando m_sequencer.<funzione>(), passando *this come sink. Gate per
// funzione: loopback ALPHA+BRAVO + diff del diagnostic log.

#include "Sequencer/ISequencerSink.hpp"
#include "Sequencer/QsoSequencerRules.hpp"
#include "Sequencer/QsoSequencerState.hpp"

namespace decodium
{
namespace seq
{

// Esito della decisione di avanzamento (step D): consolida in un unico punto
// portabile il remap (QuickQSO/Tx1-disabled), il ramo QuickQSO-TX3→SIGNOFF e la
// mappa txNum→progress, che prima erano chiamate separate dentro
// DecodiumBridge::advanceQsoState. Il bridge applica il piano (selectTx, emit,
// watchdog, partner-memory) mantenendo log e ordine identici; il mobile lo userà
// col proprio host. Decisione PURA: nessun side-effect.
struct AdvancePlan
{
    int  effectiveTxNum {0};                       // TX step post-remap
    TxStepRemapReason remapReason {TxStepRemapReason::None};
    bool quickQsoTx3Signoff {false};               // ramo QuickQSO TX3 → SIGNOFF diretto (progress=5)
    int  progress {-1};                            // qsoProgress target (-1 = invalido)
    bool valid {true};                             // false = step non valido → il chiamante rifiuta
};

class QsoSequencer
{
public:
    QsoSequencer(QsoSequencerState& state, ISequencerSink& sink)
        : m_state(state), m_sink(sink)
    {
    }

    // Non copiabile (tiene riferimenti).
    QsoSequencer(const QsoSequencer&) = delete;
    QsoSequencer& operator=(const QsoSequencer&) = delete;

    QsoSequencerState& state() { return m_state; }
    ISequencerSink&    sink()  { return m_sink; }

    // Decisione di avanzamento QSO (step D) — PURA, statica: replica esattamente
    // la logica decisionale di DecodiumBridge::advanceQsoState. Ordine identico:
    // (1) remap QuickQSO-Ultra2/Tx1-disabled; (2) se il TX effettivo è 3 e QuickQSO
    // è attivo → ramo SIGNOFF diretto (progress=5); (3) altrimenti mappa
    // txNum→progress (valid=false se lo step non è valido).
    static AdvancePlan planAdvance(int txNum, bool quickQsoEnabled, int txDisabledMask)
    {
        AdvancePlan p;
        TxStepRemap const remap = remapRequestedTxStep(txNum, quickQsoEnabled, txDisabledMask);
        p.effectiveTxNum = remap.txNum;
        p.remapReason = remap.reason;
        if (p.effectiveTxNum == 3 && quickQsoEnabled) {
            p.quickQsoTx3Signoff = true;
            p.progress = 5;   // SIGNOFF
            return p;
        }
        p.progress = qsoProgressForTxStep(p.effectiveTxNum);
        p.valid = (p.progress >= 0);
        return p;
    }

    // Gli altri metodi di logica (checkAndStartPeriodicTx, autoSequenceStep) — che
    // mutano stato e side-effect via QTimer/audio — arriveranno negli step D
    // successivi, con gate loopback ALPHA+BRAVO.

private:
    QsoSequencerState& m_state;
    ISequencerSink&    m_sink;
};

}
}

#endif
