#include "DecodiumBridgeTxPolicy.h"
#include "DecodiumBridgeTxTimingMath.h"

int estimatedSyncPayloadMs(const QString& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT8"))
        return qRound((79.0 * 4.0 * 1920.0 * 1000.0) / 48000.0);
    if (normalized == QStringLiteral("FT4"))
        return qRound(((103.0 + 2.0) * 4.0 * 576.0 * 1000.0) / 48000.0);
    if (normalized == QStringLiteral("FT2"))
        return qRound(((103.0 + 2.0) * 4.0 * 288.0 * 1000.0) / 48000.0);
    if (normalized == QStringLiteral("WSPR"))
        return qRound((162.0 * 32768.0 * 1000.0) / 48000.0);
    if (normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))) {
        return qRound((162.0 * fst4SamplesPerSymbolAt48k(normalized, 0.0) * 1000.0) / 48000.0);
    }
    if (normalized == QStringLiteral("Q65") || normalized.startsWith(QStringLiteral("Q65-")))
        return qRound((85.0 * q65SamplesPerSymbolAt48k(normalized) * 1000.0) / 48000.0);
    return 0;
}

int syncTxStartBudgetMs(const QString& mode)
{
#if defined(Q_OS_MAC)
    if (mode == QStringLiteral("FT8"))
        return 700;   // 500 ms start delay + small safety margin
    if (mode == QStringLiteral("FT4") || mode == QStringLiteral("FT2"))
        return 500;   // 300 ms start delay + safety margin
    return 400;
#else
    if (mode == QStringLiteral("FT2"))
        return 300;   // ptt1 + sink spin-up safety
    return 350;       // conservative cross-platform margin for sink start/PTT
#endif
}

int minimumUsefulSyncPayloadMs(const QString& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT8"))
        return 2500;
    if (normalized == QStringLiteral("FT4"))
        return 1200;
    if (normalized == QStringLiteral("FT2"))
        return 700;
    if (normalized == QStringLiteral("Q65") || normalized.startsWith(QStringLiteral("Q65-")))
        return qMax(5000, estimatedSyncPayloadMs(normalized) - 2000);
    if (normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))) {
        return qMax(5000, estimatedSyncPayloadMs(normalized) - 2000);
    }
    return 0;
}

int ft2AsyncTxStartSafetyMs(bool conservative)
{
    // FIX B (1.0.353): 180ms basta a coprire PTT spin-up + sink start e libera
    // ~76% dello slot per il TX async (tollera il troncamento del payload).
    // 1.0.367 — con ft2ConservativeTiming (default ON) torna a 550ms: finestra
    // utile ~18%, niente frame troncati (comportamento calmo stile Decodium 3.0).
    return conservative ? 550 : 180;
}

int latestFt2AsyncTxStartMs(int periodMs, bool conservative)
{
    // 1.0.367 — gate SOFT: oltre questo punto del periodo si rinvia il TX al
    // prossimo slot invece di trasmettere ORA.
    //  - conservative (default ON): richiede spazio per il payload FT2 INTERO
    //    (~2520ms) -> finestra ~18%, mai un frame troncato (l'utente preferisce
    //    stabilità: feedback_ft2_stability_over_reactivity).
    //  - non-conservative (FIX B 1.0.353): basta il payload UTILE minimo
    //    (~700ms) -> finestra fino a ~76%, TX più reattivo ma può troncare.
    int const payloadMs = conservative
                              ? estimatedSyncPayloadMs(QStringLiteral("FT2"))
                              : minimumUsefulSyncPayloadMs(QStringLiteral("FT2"));
    if (periodMs <= 0 || payloadMs <= 0) {
        return 0;
    }
    return qMax(0, periodMs - payloadMs - ft2AsyncTxStartSafetyMs(conservative));
}

int decodiumBridgeLatestFt2AsyncTxStartMs(int periodMs, bool conservative) { return latestFt2AsyncTxStartMs(periodMs, conservative); }

int latestD3CompatibleSyncTxStartMs(const QString& mode, int periodMs, bool immediateClickTx)
{
    if (periodMs <= 0) {
        return 0;
    }

    // JT4 sync acquisition expects the 206-symbol payload to begin about one
    // second into its 60-second UTC window. Once that lead-in has passed, wait
    // for the next owned slot instead of shifting or trimming the preamble.
    if (mode.trimmed().toUpper() == QStringLiteral("JT4")) {
        return qMin((periodMs * 3) / 4,
                    decodiumBridgeTxSyncLeadInMsForMode(QStringLiteral("JT4")) + 150);
    }

    // Decodium3 accepted starts inside the first 75% of a sync slot, but the
    // bridge audio path trims PCM by elapsed slot time. Near the end of FT4/FT8
    // that can leave only a few milliseconds of payload. Keep the D3 upper cap
    // and also require a minimum useful payload window.
    if (mode == QStringLiteral("FT8")
        || mode == QStringLiteral("FT4")
        || mode == QStringLiteral("FT2")
        || mode == QStringLiteral("MSK144")
        || mode == QStringLiteral("Q65")
        || mode.startsWith(QStringLiteral("Q65-"))
        || mode == QStringLiteral("FST4")
        || mode == QStringLiteral("FST4W")
        || mode.startsWith(QStringLiteral("FST4-"))
        || mode.startsWith(QStringLiteral("FST4W-"))) {
        int const d3CapMs = (periodMs * 3) / 4;
        int const payloadMs = estimatedSyncPayloadMs(mode);
        int const minUsefulPayloadMs = minimumUsefulSyncPayloadMs(mode);
        if (payloadMs <= 0 || minUsefulPayloadMs <= 0) {
            return d3CapMs;
        }
        // FT8/FT4 must keep the Costas start close to the nominal 500 ms slot lead-in.
        // A shifted CQ at +1.4s is decodable only by luck and is visibly out of slot,
        // so defer late starts to the next valid TX period instead of sending now.
        // 1.0.314 — opt-in: con ftxImmediateClickTx ON il cap sale a 2000ms (D3/JTDX
        // accettano shift fino a ~2s; ripristina il "TX immediato al click" stile 1.0.283).
        if (mode == QStringLiteral("FT8")
            || mode == QStringLiteral("FT4")
            || mode == QStringLiteral("MSK144")
            || mode == QStringLiteral("Q65")
            || mode.startsWith(QStringLiteral("Q65-"))
            || mode == QStringLiteral("FST4")
            || mode == QStringLiteral("FST4W")
            || mode.startsWith(QStringLiteral("FST4-"))
            || mode.startsWith(QStringLiteral("FST4W-"))) {
            // 1.0.316 (hotfix Pasquale): 2000ms era troppo stretto — su FT8 (slot 15s)
            // l'utente clicca tipicamente dopo 2-5s e ricadeva nel defer. Ora con
            // immediateClickTx ON la finestra sale a d3CapMs (75% slot) = comportamento
            // 1.0.283 reale. Trade-off: click molto tardi (>50% slot) → TX shiftato e
            // il partner potrebbe non decodificare; il toggle è opt-in e l'utente sceglie.
            // MSK144 fills all but the final 500 ms of its configured T/R
            // period. A late start therefore crosses into the peer's slot even
            // though the repeated frame itself may decode. Never relax this
            // boundary for immediate-click operation.
            int const latestCleanStartMs = immediateClickTx
                && mode != QStringLiteral("MSK144")
                ? d3CapMs
                : (decodiumBridgeTxSyncLeadInMsForMode(mode) + 150);
            return qMax(0, qMin(d3CapMs, latestCleanStartMs));
        }
        int const payloadCapMs =
            decodiumBridgeTxSyncLeadInMsForMode(mode) + qMax(0, payloadMs - minUsefulPayloadMs);
        return qMax(0, qMin(d3CapMs, payloadCapMs));
    }

    int const payloadMs = estimatedSyncPayloadMs(mode);
    if (payloadMs <= 0) {
        return periodMs;
    }
    return qMax(0, periodMs - payloadMs - syncTxStartBudgetMs(mode));
}


int txSyncLeadInMsForMode(const QString& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    if (normalized == QStringLiteral("FT2LINK")) {
        return 300;
    }
    if (normalized == QStringLiteral("FT8") || normalized == QStringLiteral("FT4")) {
        return 500;
    }
    if (normalized == QStringLiteral("Q65") || normalized.startsWith(QStringLiteral("Q65-"))) {
        return 1000;
    }
    if (normalized == QStringLiteral("JT4")) {
        return 1000;
    }
    if (normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))) {
        return fst4PeriodSecondsForMode(normalized, 0.0) == 15 ? 500 : 1000;
    }
    if (normalized == QStringLiteral("FT2")) {
#if defined(Q_OS_LINUX)
        return 0;
#else
        return 300;
#endif
    }
    return 0;
}

int decodiumBridgeTxSyncLeadInMsForMode(const QString& mode)
{
    return txSyncLeadInMsForMode(mode);
}


int autoTxDecodeGraceMs(const QString& mode, bool ft8FastSequence)
{
    QString const normalized = mode.trimmed().toUpper();
    // Grace = quanto aspettare al boundary il decode dello slot appena finito prima
    // di committare il TX auto. Deve superare (settle 650ms + decode + advanceQsoState)
    // ~0.95-1.0s in FT8, altrimenti la fallback committa CQ con lo stato vecchio e la
    // risposta al partner viene persa (QSO non si chiude).
    // 1.0.317 — opt-in ft8FastSequence: grace FT8 ridotta a 400ms per reattività stile
    // WSJT-X/JTDX. Sotto pressione CPU il qMax(900,...) in effectiveAutoTxDecodeGraceMs
    // sovrascrive automaticamente (sicurezza > reattività su PC carichi).
    if (normalized == QStringLiteral("FT8"))
        return ft8FastSequence ? 400 : 1200;
    if (normalized == QStringLiteral("FT4"))
        return 700;
    if (normalized == QStringLiteral("FT2"))
        return 250;
    return 500;
}

