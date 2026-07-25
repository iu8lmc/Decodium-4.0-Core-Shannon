#ifndef DECODIUM_SEQUENCER_TIMING_HPP
#define DECODIUM_SEQUENCER_TIMING_HPP

// Fase 1 port mobile — step D2 (doc/mobile-port-plan.md): helper PURI di timing
// del sequencer, estratti verbatim da DecodiumBridge.cpp (erano static file-scope).
// Nessuno stato, nessun side effect. Condivisi tra desktop e core mobile: i corpi
// di checkAndStartPeriodicTx/autoSequenceStep, quando migrati in QsoSequencer, li
// chiameranno da qui.

namespace decodium
{
namespace seq
{

// Convenzione runtime del bridge: txPeriod==1 -> first/even slot (:00/:30);
// txPeriod==0 -> second/odd slot (:15/:45).
bool bridgeTxPeriodIsEven(int txPeriod);

// Rango di priorità del TX step per il defer async (TX5>TX4>…>TX1; TX6==TX1).
int  autoSeqTxRank(int txNum);

}
}

#endif
