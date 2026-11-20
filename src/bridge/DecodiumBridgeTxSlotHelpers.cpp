#include "DecodeUtcTime.h"
#include <QString>

static bool bridgeTxPeriodIsEven(int txPeriod)
{
    // Bridge runtime convention:
    //   txPeriod == 1  -> legacy txFirst == true  -> first/even slot (:00/:30)
    //   txPeriod == 0  -> legacy txFirst == false -> second/odd slot (:15/:45)
    return txPeriod != 0;
}

static int replyTxPeriodForClickedDecode(QString const& mode,
                                         QString const& timeStr,
                                         int fallbackTxPeriod,
                                         int configuredPeriodMs = 0)
{
    int const msgSecond = secondsFromUtcDisplayToken(timeStr);
    if (msgSecond < 0) {
        return fallbackTxPeriod != 0 ? 1 : 0;
    }

    QString const normalizedMode = mode.trimmed().toUpper();
    if (normalizedMode == QStringLiteral("FT8")) {
        bool const dxIsFirstSequence = (msgSecond % 30) < 15;
        return dxIsFirstSequence ? 0 : 1;
    }
    if (normalizedMode == QStringLiteral("FT4")) {
        // FT4 second sequence starts at .5s (:07.5, :22.5, :37.5, :52.5)
        // but decoded rows are displayed as whole seconds (:07, :22, :37, :52).
        // Dividing the floored display second by 7500ms puts those rows in the
        // previous bucket and makes D4 transmit on top of the caller.
        bool const dxIsFirstSequence = (msgSecond % 15) < 7;
        return dxIsFirstSequence ? 0 : 1;
    }

    int pMs = 0;
    if (normalizedMode == QStringLiteral("FT2")) {
        pMs = 3750;
    } else if (normalizedMode == QStringLiteral("MSK144")) {
        pMs = configuredPeriodMs > 0 ? configuredPeriodMs : 15000;
    }
    if (pMs <= 0) {
        return fallbackTxPeriod != 0 ? 1 : 0;
    }
    // P0 fix (log ZL3DMH/ZL1BW 2026-06-11): la label e' troncata ai secondi
    // interi -> lo slot FT2 :18.75 diventa "18" e 18000/3750=4 (pari) invece
    // di 5 (dispari): parita' di mira sbagliata in 12 posizioni su 16 -> la
    // risposta atterra nello stesso sub-slot del CQer (sordita' reciproca,
    // 93% di overlap RF misurato). Round-to-nearest: identico al fix FT4
    // documentato sopra; corregge tutte le 16 posizioni e copre anche MSK144.
    int const dxPidx = (msgSecond * 1000 + pMs / 2) / pMs;
    return dxPidx % 2;
}
int decodiumBridgeReplyTxPeriodForClickedDecode(const QString& mode, const QString& time, int fallback, int period) { return replyTxPeriodForClickedDecode(mode, time, fallback, period); }


bool decodiumBridgeTxPeriodIsEven(int period) { return bridgeTxPeriodIsEven(period); }
