#include "Sequencer/SequencerTiming.hpp"

// Corpi estratti VERBATIM da DecodiumBridge.cpp (bridgeTxPeriodIsEven ~2295,
// autoSeqTxRank ~5617). Puri: nessuna dipendenza oltre gli argomenti.

namespace decodium
{
namespace seq
{

bool bridgeTxPeriodIsEven(int txPeriod)
{
    // Bridge runtime convention:
    //   txPeriod == 1  -> legacy txFirst == true  -> first/even slot (:00/:30)
    //   txPeriod == 0  -> legacy txFirst == false -> second/odd slot (:15/:45)
    return txPeriod != 0;
}

int autoSeqTxRank(int txNum)
{
    switch (txNum) {
    case 5: return 5;
    case 4: return 4;
    case 3: return 3;
    case 2: return 2;
    case 1: return 1;
    case 6: return 1;
    default: return 0;
    }
}

}
}
