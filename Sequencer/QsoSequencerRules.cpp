// Corpo spostato VERBATIM da DecodiumBridge.cpp (era static a file scope).
// Qualsiasi modifica alla logica va fatta QUI (desktop e mobile la condividono).
#include "Sequencer/QsoSequencerRules.hpp"
#include "Sequencer/MessageTokenRules.hpp"

#include <QStringList>
#include <QtGlobal>

namespace decodium
{
namespace seq
{

int deferredSignoffRetryCapForMode (const QString& mode, int configuredMaxRetries,
                                    int partnerSnrDb, bool conservative,
                                    bool quickGiveUpStrong,
                                    int ft2SignoffCap,
                                    int ft4SignoffCap,
                                    int ft8SignoffCap, bool weakBoost,
                                    int weakSnrThreshold, int weakBonus)
{
    Q_UNUSED(configuredMaxRetries)
    QString const normalized = mode.trimmed().toUpper();
    // 1.0.311/1.0.315 — cap ripetizioni 73/RR73 controllato dall'utente per modo
    // (FT2/FT4/FT8): valore ASSOLUTO e prevedibile (niente extra conservative/weak
    // che prima lo gonfiavano fino a 8-10). quickGiveUpStrong può solo ridurlo.
    // Default per modo: FT8=3, FT4=4, FT2=4 (= comportamento "tipico" pre-1.0.315
    // senza conservative/weak). Per partner deboli/QSB: alzare lo spinbox a 6-8.
    int userCap = -1;
    if (normalized == QStringLiteral("FT2"))      userCap = ft2SignoffCap;
    else if (normalized == QStringLiteral("FT4")) userCap = ft4SignoffCap;
    else if (normalized == QStringLiteral("FT8")) userCap = ft8SignoffCap;
    if (userCap >= 0) {
        // 1.0.727: FT2, FT4 and FT8 close with at least 2 RR73: with 1 the QSO was closed on the first RR73,
        // before the partner could send its 73 (paul log, 28/10)
        int const minCap = (normalized == QStringLiteral("FT4") || normalized == QStringLiteral("FT8")
                             || normalized == QStringLiteral("FT2")) ? 2 : 1;
        int cap = qBound(minCap, userCap, 8);
        // 1.0.437 - opt-in: extra signoff retries per partner debole (default OFF = byte-identico).
        if (weakBoost && partnerSnrDb != 127 && partnerSnrDb <= weakSnrThreshold)
            cap = qBound(1, cap + qBound(1, weakBonus, 6), 8);
        if (quickGiveUpStrong && partnerSnrDb != 127 && partnerSnrDb > 0)
            cap = qMin(cap, 4);
        return cap;
    }
    // Fallback per modi non-FTX (Q65, JT9, MSK144, ecc.): logica legacy modeCap + extras
    int modeCap = 3;
    if (normalized == QStringLiteral("FT4")) {
        modeCap = 4;
    }
    // 1.0.174 — extra retries per partner debole (QSB-aware).
    // SNR < -10 dB aggiunge fino a +4 retry, scalando linearmente:
    // -10→0, -12→1, -14→2, -16→3, -18+→4.
    // Conservative mode aggiunge +2 base.
    int extra = 0;
    if (partnerSnrDb != 127 && partnerSnrDb < -10) {
        extra = qBound(0, (-partnerSnrDb - 10) / 2, 4);
    }
    if (conservative) extra += 2;
    modeCap += extra;
    // 1.0.289 — #3: partner FORTE (SNR>0) che non chiude → cap ridotto (early give-up),
    // così non si ripete RR73 fino a 8/10 volte su una stazione già loggabile. Default OFF.
    if (quickGiveUpStrong && partnerSnrDb != 127 && partnerSnrDb > 0) {
        modeCap = qMin(modeCap, 4);
    }
    // The caller retry setting controls report-step retries, not the final
    // RR73/73 wait. A low value here made AutoCQ leave the QSO after the first
    // RR73, before the partner's final 73 could arrive.
    return modeCap;
}

bool isTxStepDisabledInMask (int txNum, int txDisabledMask)
{
    return txNum >= 1 && txNum <= 6 && (txDisabledMask & (1 << (txNum - 1)));
}

TxStepRemap remapRequestedTxStep (int txNum, bool quickQsoEnabled, int txDisabledMask)
{
    // Ordine identico ad advanceQsoState: QuickQSO ha precedenza sul fallback
    // Tx1-disabled (if / else-if originali).
    if (txNum == 1 && quickQsoEnabled) {
        return { 2, TxStepRemapReason::QuickQsoSkipTx1 };
    }
    if (txNum == 1 && isTxStepDisabledInMask(1, txDisabledMask)
        && !isTxStepDisabledInMask(2, txDisabledMask)) {
        return { 2, TxStepRemapReason::Tx1DisabledFallback };
    }
    return { txNum, TxStepRemapReason::None };
}

int qsoProgressForTxStep (int txNum)
{
    switch (txNum) {
        case 1: return 2; // TX1 (risposta CQ)  → REPLYING (in attesa risposta)
        case 2: return 3; // TX2 (report)       → REPORT
        case 3: return 4; // TX3 (R+report)     → ROGER_REPORT
        case 4: return 5; // TX4 (RR73/RRR)     → SIGNOFF
        case 5: return 5; // TX5 (73)           → SIGNOFF
        case 6: return 1; // TX6 (CQ)           → CALLING_CQ
        default: return -1;
    }
}

bool mayAdvanceRogerReportToRr73 (bool freshAutoCqCaller,
                                  bool ownReportWasSentToSamePartner,
                                  bool priorReplyFromSamePartnerAccepted)
{
    return !freshAutoCqCaller
        && ownReportWasSentToSamePartner
        && priorReplyFromSamePartnerAccepted;
}

bool lastTxJustifiesSignoff (const QString& message)
{
    if (!signalReportFromMessage (message).trimmed ().isEmpty ())
        return true;
    QStringList const tokens = message.simplified ().split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty ())
        return false;
    QString const last = tokens.last ();
    return last.compare (QLatin1String ("RR73"), Qt::CaseInsensitive) == 0
        || last.compare (QLatin1String ("RRR"), Qt::CaseInsensitive) == 0
        || last == QLatin1String ("73");
}

bool courtesySignoffDue (const QString& message, int sentInWindow, int maxPerWindow)
{
    if (sentInWindow >= maxPerWindow)
        return false;
    QStringList const tokens = message.simplified ().split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (tokens.size () < 3)
        return false;
    QString const last = tokens.last ();
    if (last.endsWith (QLatin1Char ('?')) || last == QLatin1String ("?"))
        return false;   // a low-confidence decode must not make us transmit
    if (last == QLatin1String ("73"))
        return false;   // the partner has already closed
    if (tokens.first ().compare (QLatin1String ("CQ"), Qt::CaseInsensitive) == 0)
        return false;   // not directed to us
    return true;
}

bool weakSignoffEndsRr73Repeats (const QString& message, int slotProgress, int slotCurrentTx,
                                 int slotLastNtx, int slotRetryCount, bool reportReceived)
{
    if (!reportReceived || slotProgress < 4 || slotCurrentTx != 4 || slotLastNtx != 4
        || slotRetryCount < 1)
        return false;
    // only a low-confidence signoff ('?'): a clean one is handled by the normal sequencer
    if (!message.trimmed ().endsWith (QLatin1Char ('?')))
        return false;
    QStringList tokens = message.simplified ().split (QLatin1Char (' '), Qt::SkipEmptyParts);
    while (!tokens.isEmpty () && tokens.last () == QLatin1String ("?"))
        tokens.removeLast ();
    if (tokens.isEmpty ())
        return false;
    QString last = tokens.last ();
    if (last.endsWith (QLatin1Char ('?')))
        last.chop (1);
    return last == QLatin1String ("73") || last.compare (QLatin1String ("RR73"), Qt::CaseInsensitive) == 0;
}

bool mayFeedRescuedDecodeToSequencer (const QString& message)
{
    QStringList tokens = message.simplified ().split (QLatin1Char (' '), Qt::SkipEmptyParts);
    while (!tokens.isEmpty () && tokens.last () == QLatin1String ("?"))
        tokens.removeLast ();
    if (tokens.isEmpty ())
        return false;
    QString last = tokens.last ();
    if (last.endsWith (QLatin1Char ('?')))
        last.chop (1);
    if (last.compare (QLatin1String ("73"), Qt::CaseInsensitive) == 0
        || last.compare (QLatin1String ("RR73"), Qt::CaseInsensitive) == 0
        || last.compare (QLatin1String ("RRR"), Qt::CaseInsensitive) == 0)
        return false;
    // "<call> <call> R+05 TU" / "+05 TU": the peer considers the QSO complete.
    if (last.compare (QLatin1String ("TU"), Qt::CaseInsensitive) == 0)
        return false;
    return true;
}

bool specialOperationTransitionRequiresDeferral (bool ownFinal73Pending,
                                                 bool logPromptOpen)
{
    return ownFinal73Pending || logPromptOpen;
}

}
}
