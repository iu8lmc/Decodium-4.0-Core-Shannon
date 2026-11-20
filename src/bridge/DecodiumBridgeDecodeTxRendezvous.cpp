#include "DecodiumBridge.h"
#include "DecodiumBridgeTxPolicy.h"
#include <QTimer>

int decodiumBridgeLatestD3SyncStartMs(const QString&, int, bool);
void decodiumBridgeDecodeTxWaitLog(const QString&);

bool DecodiumBridge::shouldDeferAutoTxUntilTimeSyncDecode(const QString& modeSnapshot) const
{
    QString const normalizedMode = modeSnapshot.trimmed().toUpper();
    if (!isTimeSyncDecodeMode(normalizedMode)
        || usingLegacyBackendForTx()
        || m_manualTxHold
        || !m_monitoring
        || m_transmitting
        || m_tuning) {
        return false;
    }
    if (normalizedMode == QStringLiteral("FT2") && m_asyncTxEnabled) {
        return false;
    }

    QString const myCall = m_callsign.trimmed();
    bool const autoSeqActive =
        !myCall.isEmpty()
        && ((m_autoSeq && (m_txEnabled || !m_dxCall.trimmed().isEmpty()))
            || m_autoCqRepeat);

    return autoSeqActive && (m_txEnabled || m_autoCqRepeat);
}

bool DecodiumBridge::hasPendingTimeSyncDecodeForMode(const QString& modeSnapshot) const
{
    QString const normalizedMode = modeSnapshot.trimmed().toUpper();
    for (auto it = m_decodeModeBySerial.constBegin(); it != m_decodeModeBySerial.constEnd(); ++it) {
        if (it.value().trimmed().toUpper() != normalizedMode) {
            continue;
        }
        quint64 const serial = it.key();
        if (!m_decodeStartMsBySerial.contains(serial)) {
            continue;
        }
        if (m_ft8EarlyDecodeSerials.contains(serial)) {
            continue;
        }
        if (m_ft8DeepInTxSerials.contains(serial)) {
            continue;
        }
        return true;
    }
    return false;
}

int DecodiumBridge::effectiveAutoTxDecodeGraceMs(const QString& modeSnapshot) const
{
    QString const normalizedMode = modeSnapshot.trimmed().toUpper();
    int graceMs = autoTxDecodeGraceMs(normalizedMode, m_ft8FastSequence);
    if (cpuPressureActive() || m_lowCpuModeEnabled) {
        // FT2 async resta reattivo (80ms). FT8/FT4 NON vanno tagliati sotto pressione:
        // sulle macchine lente l'early-visible salta e il decode della risposta arriva
        // tardi -> dobbiamo aspettarlo (>=900ms), altrimenti committiamo prima della
        // risposta del partner e il QSO non si chiude.
        graceMs = (normalizedMode == QStringLiteral("FT2"))
            ? qMin(graceMs, 80)
            : qMax(900, qMin(graceMs, 1200));
    }
    return graceMs;
}

void DecodiumBridge::scheduleDeferredAutoTxAfterTimeSyncDecode(const QString& modeSnapshot,
                                                              quint64 sessionId)
{
    if (sessionId == m_periodTimerSessionId && modeSnapshot == m_mode
        && usesFt8AutoTxDecodeGate()) {
        if (!deferFt8AutoTxForDecode()) checkAndStartPeriodicTx();
        return;
    }
    QString const normalizedMode = modeSnapshot.trimmed().toUpper();
    int const periodMs = periodMsForMode(normalizedMode);
    // 1.0.326 B4: use effectiveRelaxLatestCap() (includes ft8FastSequence) — was using only m_ftxImmediateClickTx
    int const latestStartMs = decodiumBridgeLatestD3SyncStartMs(normalizedMode, periodMs, effectiveRelaxLatestCap());
    if (periodMs <= 0 || latestStartMs <= 0) {
        return;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    int const graceMs = effectiveAutoTxDecodeGraceMs(normalizedMode);
    int const maxDelayMs = latestStartMs - elapsedMs - 250;
    if (maxDelayMs <= 0) {
        if (sessionId != m_periodTimerSessionId
            || m_mode != modeSnapshot
            || m_manualTxHold
            || m_transmitting
            || m_tuning
            || !shouldDeferAutoTxUntilTimeSyncDecode(modeSnapshot)) {
            return;
        }
        if (elapsedMs < latestStartMs) {
            decodiumBridgeDecodeTxWaitLog(QStringLiteral("time-sync auto TX: no %1 decode-grace budget, checking TX now elapsed=%2ms latest=%3ms")
                          .arg(modeSnapshot)
                          .arg(elapsedMs)
                          .arg(latestStartMs));
            quint64 const requestGeneration = m_txRequestGeneration;
            QTimer::singleShot(0, this, [this, modeSnapshot, sessionId, requestGeneration]() {
                if (sessionId != m_periodTimerSessionId
                    || requestGeneration != m_txRequestGeneration
                    || m_mode != modeSnapshot
                    || m_manualTxHold
                    || m_transmitting
                    || m_tuning
                    || !shouldDeferAutoTxUntilTimeSyncDecode(modeSnapshot)) {
                    return;
                }
                checkAndStartPeriodicTx();
            });
        } else {
            decodiumBridgeDecodeTxWaitLog(QStringLiteral("time-sync auto TX: no safe %1 start budget, defer elapsed=%2ms latest=%3ms")
                          .arg(modeSnapshot)
                          .arg(elapsedMs)
                          .arg(latestStartMs));
            scheduleSyncTxAtNextValidSlot(QStringLiteral("time-sync-decode-budget"),
                                          elapsedMs,
                                          latestStartMs);
        }
        return;
    }
    int const delayMs = qMax(50, qMin(graceMs, maxDelayMs));

    decodiumBridgeDecodeTxWaitLog(QStringLiteral("time-sync auto TX: brief %1 decode grace before TX, fallback in %2ms")
                  .arg(modeSnapshot)
                  .arg(delayMs));

    quint64 const requestGeneration = m_txRequestGeneration;
    QTimer::singleShot(delayMs, this, [this, modeSnapshot, sessionId, requestGeneration]() {
        if (sessionId != m_periodTimerSessionId
            || requestGeneration != m_txRequestGeneration
            || m_mode != modeSnapshot
            || m_manualTxHold
            || m_transmitting
            || m_tuning) {
            return;
        }
        if (!shouldDeferAutoTxUntilTimeSyncDecode(modeSnapshot)) {
            return;
        }

        decodiumBridgeDecodeTxWaitLog(QStringLiteral("time-sync auto TX: fallback check after decode wait (%1)")
                      .arg(modeSnapshot));
        checkAndStartPeriodicTx();
    });
}

