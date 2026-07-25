#ifndef DECODIUM_SEQUENCER_CONFIG_HPP
#define DECODIUM_SEQUENCER_CONFIG_HPP

// Fase 1 port mobile — step D (doc/mobile-port-plan.md): i TOGGLE di
// configurazione che parametrizzano il sequencer QSO (retry/signoff cap,
// weak-signal pack, QuickQSO/async, watchdog, post-log re-engage). Su desktop
// il bridge li popola da QSettings; sul mobile dalla UI nativa. POD passato
// per const-ref ai metodi del sequencer (checkAndStartPeriodicTx,
// autoSequenceStep) allo step D → nessuna dipendenza da QSettings nel core.
// I default replicano quelli di DecodiumBridge.h (verificati riga per riga).

#include <QtGlobal>

namespace decodium
{
namespace seq
{

struct SequencerConfig
{
    // --- Retry / signoff cap (1.0.311/315/437/446/493) ---
    int  ft2SignoffRetryCap {4};       // DecodiumBridge.h m_ft2SignoffRetryCap
    int  ft4SignoffRetryCap {4};
    int  ft8SignoffRetryCap {3};
    int  maxCallerRetries {10};        // m_maxCallerRetries
    bool callerRetriesAlwaysHard {true}; // m_callerRetriesAlwaysHard (bypass watchdog-priority)

    // --- Weak-signal / conservative pack (FT2) ---
    bool ft2Conservative {false};
    bool ft2ConservativeTiming {true};
    bool ft2QuickGiveUpStrong {false};
    bool ftxWeakSignoffBoost {false};
    int  ftxWeakSnrThreshold {-15};
    int  ftxWeakSignoffBonus {3};

    // --- QuickQSO / async / click ---
    bool quickQsoEnabled {false};      // FT2 Quick QSO (Ultra2): salta TX1
    bool asyncTxEnabled {true};        // FT2 async TX (permanente, gated m_mode=="FT2")
    bool ftxImmediateClickTx {false};

    // --- Modalità QSO ---
    bool autoCqRepeat {false};
    bool multiAnswerMode {false};
    int  txDisabledMask {0};           // maschera Tx disabilitati (bit per Tx step)

    // --- Watchdog TX ---
    int  txWatchdogMode {1};           // 0=off 1=time 2=count …
    int  txWatchdogTime {6};           // minuti
    int  txWatchdogCount {3};          // periodi

    // --- Post-log re-engage guard (FT2) ---
    bool ft2PostLogReengageGuard {false};
    int  ft2PostLogReengageMax {1};
};

}
}

#endif
