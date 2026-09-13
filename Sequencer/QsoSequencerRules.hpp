#ifndef DECODIUM_QSO_SEQUENCER_RULES_HPP
#define DECODIUM_QSO_SEQUENCER_RULES_HPP

// Fase 1 port mobile — step A dello strangler (doc/mobile-port-plan.md):
// regole PURE del sequencer QSO estratte da DecodiumBridge.cpp, condivise
// tra desktop e decodium-core mobile. Nessuno stato, nessun side effect:
// ogni funzione dipende solo dai parametri.

#include <QString>

namespace decodium
{
namespace seq
{

// Cap ripetizioni 73/RR73 per modo (FT2/FT4/FT8: valore utente assoluto;
// altri modi: legacy modeCap + extra QSB/conservative). Estratta verbatim
// da DecodiumBridge.cpp (era static, 1.0.311/315/437).
int deferredSignoffRetryCapForMode (const QString& mode, int configuredMaxRetries,
                                    int partnerSnrDb = 127, bool conservative = false,
                                    bool quickGiveUpStrong = false,
                                    int ft2SignoffCap = 8,
                                    int ft4SignoffCap = 4,
                                    int ft8SignoffCap = 3, bool weakBoost = false,
                                    int weakSnrThreshold = -15, int weakBonus = 3);

// ---- Step A2: regole decisionali di advanceQsoState ------------------------

// Bit del TX step nella maschera utente "Tx disabled" (stessa semantica di
// DecodiumBridge::isTxDisabled).
bool isTxStepDisabledInMask (int txNum, int txDisabledMask);

enum class TxStepRemapReason
{
  None,
  QuickQsoSkipTx1,       // Quick QSO (Ultra2): TX1 -> TX2
  Tx1DisabledFallback,   // 1.0.379: Tx1 off utente -> risposta parte da TX2
};

struct TxStepRemap
{
  int txNum;
  TxStepRemapReason reason;
};

// Rimappa il TX step RICHIESTO nel TX step EFFETTIVO secondo le regole
// QuickQSO / Tx1-disabled (ordine e precedenza identici ad advanceQsoState).
TxStepRemap remapRequestedTxStep (int txNum, bool quickQsoEnabled, int txDisabledMask);

// Mappa TX step -> qsoProgress (2=REPLYING, 3=REPORT, 4=ROGER_REPORT,
// 5=SIGNOFF, 1=CALLING_CQ). Ritorna -1 per step non validi.
int qsoProgressForTxStep (int txNum);

}
}

#endif
