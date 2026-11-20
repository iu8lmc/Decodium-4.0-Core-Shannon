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

// Un R+report puo' avanzare direttamente a TX4/RR73 solo quando appartiene
// a uno scambio gia' provato con lo stesso partner. In particolare un caller
// appena accettato da AutoCQ, oppure il suo primo decode utile, non deve mai
// ereditare un TX2/TX3 rimasto nello stato precedente.
bool mayAdvanceRogerReportToRr73 (bool freshAutoCqCaller,
                                  bool ownReportWasSentToSamePartner,
                                  bool priorReplyFromSamePartnerAccepted);

// A low-confidence decode rebuilt by the a-priori memory (marked '?') may keep
// a QSO moving, but it must never CLOSE one: a 73, RR73 or RRR (or a "TU" quick
// close) coming from it can be a phantom made from the tail of our own signal
// or from an old message, and would log a contact that never happened.
// `message` is the decoded text, trailing '?' included.
bool mayFeedRescuedDecodeToSequencer (const QString& message);

// The TX5/73 safety barrier asks whether the partner has received something
// from us that justifies closing: a numeric report, or a closing message
// (RR73, RRR, 73) of our own. A partner who answers our RR73 with RR73 must
// get its 73 and be logged, not be restarted at TX2 (LY5A, 27/10/2026).
// `message` is the last message we transmitted.
bool lastTxJustifiesSignoff (const QString& message);

// A partner whose QSO is already complete keeps calling because it did not hear our closing
// message. Decide whether a one-shot courtesy 73 is due for the directed message `message`
// (trailing '?' included): never for a low-confidence decode, never when the partner already
// sent its own 73, and never more than `maxPerWindow` times (`sentInWindow` already sent in
// the current window).
bool courtesySignoffDue (const QString& message, int sentInWindow, int maxPerWindow);

// MAM pile-up: a slot that has already sent RR73 to a partner who acknowledged our report
// goes on repeating RR73 until the retry limit when the partner's 73 is a low-confidence decode
// (LZ1DD, 28/10/2026: RR73 x4 in two minutes while two callers waited). Such a decode cannot
// close a QSO (see mayFeedRescuedDecodeToSequencer), but it is good evidence that the partner has
// stopped waiting for more RR73s. True when `message` is a low-confidence signoff and the slot is
// in exactly that state: partner report received, RR73 (TX4) sent at least once.
bool weakSignoffEndsRr73Repeats (const QString& message, int slotProgress, int slotCurrentTx,
                                 int slotLastNtx, int slotRetryCount, bool reportReceived);

// A Fox/Hound backend change must not interrupt the final TX5/73 or an open
// log confirmation. Both states still belong to the QSO being completed.
bool specialOperationTransitionRequiresDeferral (bool ownFinal73Pending,
                                                 bool logPromptOpen);

}
}

#endif
