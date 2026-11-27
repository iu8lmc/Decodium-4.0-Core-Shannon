#include "DecodiumBridge.h"
#include <QTimer>

bool decodiumBridgeCatSuppressedForTx();
bool decodiumBridgeTxPeriodIsEven(int);
int decodiumBridgeLatestD3SyncStartMs(const QString&, int, bool);
void decodiumBridgeSyncScheduleLog(const QString&);

bool DecodiumBridge::usesDeferredManualSyncTx() const
{
    QString const normalized = m_mode.trimmed().toUpper();
    // JT4, JT65 and JT9 are normally scheduled by the embedded legacy backend.
    // When CAT is disabled, the bridge owns PCM playback and must preserve
    // the UTC slot parity; otherwise a payload can start at an arbitrary point
    // in the receive window and the synchronized decoders cannot find it.
    if ((normalized == QStringLiteral("JT4")
         || normalized == QStringLiteral("JT65")
         || normalized == QStringLiteral("JT9"))
        && (decodiumBridgeCatSuppressedForTx() || !usingLegacyBackendForTx())) {
        return true;
    }
    if (normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("FST4W")
        || normalized.startsWith(QStringLiteral("FST4-"))
        || normalized.startsWith(QStringLiteral("FST4W-"))) {
        return true;
    }
    // MSK144 uses the bridge PCM path on macOS even when sequencing remains
    // owned by the legacy backend. It must therefore be schedulable here too,
    // otherwise a direct legacy PTT request can bypass the late-slot guard.
    if (normalized == QStringLiteral("MSK144")) {
        return true;
    }
    return !usingLegacyBackendForTx()
        && (normalized == QStringLiteral("FT8")
            || normalized == QStringLiteral("FT4")
            || (normalized == QStringLiteral("FT2") && !m_asyncTxEnabled));
}

bool DecodiumBridge::shouldDeferManualSyncTxStart() const
{
    if (!usesDeferredManualSyncTx() || !m_monitoring) {
        return false;
    }

    if (m_transmitting) {
        return true;
    }

    if (m_tuning) {
        return false;
    }

    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        return false;
    }

    qint64 const msNow = correctedUtcMsecsSinceStartOfDay();
    qint64 const slotIndex = msNow / static_cast<qint64>(periodMs);
    bool const isEvenPeriod = ((slotIndex % 2) == 0);
    bool const isOurPeriod = decodiumBridgeTxPeriodIsEven(m_txPeriod) ? isEvenPeriod
                                                              : !isEvenPeriod;
    if (!isOurPeriod) {
        return true;
    }

    int const elapsedMs = static_cast<int>(msNow % static_cast<qint64>(periodMs));
    int const latestStartMs = decodiumBridgeLatestD3SyncStartMs(m_mode, periodMs, m_ftxImmediateClickTx);
    return latestStartMs > 0 && elapsedMs >= latestStartMs;
}

bool DecodiumBridge::tryStartDeferredManualSyncTx()
{
    if (!m_deferredManualSyncTx) {
        return false;
    }

    if (!usesDeferredManualSyncTx()) {
        clearDeferredManualSyncTx(QStringLiteral("unsupported-mode"));
        return false;
    }

    if (!m_monitoring || m_transmitting || m_tuning) {
        return false;
    }

    // setTxEnabled also queues automatic requests through this path. Only
    // an actual user click may bypass the RX rendezvous with manual timing.
    if (usesFt8AutoTxDecodeGate()) {
        clearDeferredManualSyncTx(QStringLiteral("auto-ft8-rx-rendezvous"));
        checkAndStartPeriodicTx();
        return m_transmitting;
    }

    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        clearDeferredManualSyncTx(QStringLiteral("invalid-period"));
        return false;
    }

    qint64 const msNow = correctedUtcMsecsSinceStartOfDay();
    qint64 const slotIndex = msNow / static_cast<qint64>(periodMs);
    bool const isEvenPeriod = ((slotIndex % 2) == 0);
    bool const isOurPeriod = decodiumBridgeTxPeriodIsEven(m_txPeriod) ? isEvenPeriod
                                                              : !isEvenPeriod;
    if (!isOurPeriod) {
        return false;
    }

    int const elapsedMs = static_cast<int>(msNow % static_cast<qint64>(periodMs));
    int const latestStartMs = decodiumBridgeLatestD3SyncStartMs(m_mode, periodMs, m_ftxImmediateClickTx);
    if (latestStartMs > 0 && elapsedMs >= latestStartMs) {
        decodiumBridgeSyncScheduleLog(QStringLiteral("manualSyncTx: slot too late for queued TX%1 elapsed=%2ms latest=%3ms")
                      .arg(m_currentTx)
                      .arg(elapsedMs)
                      .arg(latestStartMs));
        scheduleSyncTxAtNextValidSlot(QStringLiteral("manual-sync-tx"), elapsedMs, latestStartMs);
        return false;
    }

    if (!txDispatchGuardAllows(QStringLiteral("tryStartDeferredManualSyncTx"))) {
        clearDeferredManualSyncTx(QStringLiteral("TX guard"));
        return false;
    }
    clearDeferredManualSyncTx(QStringLiteral("slot-ready"));
    decodiumBridgeSyncScheduleLog(QStringLiteral("manualSyncTx: starting queued TX%1 at slot boundary")
                  .arg(m_currentTx));
    startTx();
    return true;
}

void DecodiumBridge::clearDeferredManualSyncTx(const QString& reason)
{
    if (!m_deferredManualSyncTx) {
        return;
    }
    m_deferredManualSyncTx = false;
    m_syncTxRetryScheduled = false;
    ++m_syncTxRetrySerial;
    decodiumBridgeSyncScheduleLog(QStringLiteral("manualSyncTx: cleared (%1)").arg(reason));
}

void DecodiumBridge::scheduleSyncTxAtNextValidSlot(const QString& reason,
                                                   int elapsedMs,
                                                   int latestStartMs)
{
    if (!usesDeferredManualSyncTx() || !m_monitoring || m_manualTxHold) {
        return;
    }

    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        return;
    }

    qint64 const msNow = correctedUtcMsecsSinceStartOfDay();
    qint64 const slotIndex = msNow / static_cast<qint64>(periodMs);
    int const currentElapsedMs = static_cast<int>(msNow % static_cast<qint64>(periodMs));
    if (elapsedMs < 0) {
        elapsedMs = currentElapsedMs;
    }
    if (latestStartMs <= 0) {
        latestStartMs = decodiumBridgeLatestD3SyncStartMs(m_mode, periodMs, m_ftxImmediateClickTx);
    }

    auto slotIsOurPeriod = [this](qint64 index) {
        bool const isEvenPeriod = ((index % 2) == 0);
        return decodiumBridgeTxPeriodIsEven(m_txPeriod) ? isEvenPeriod : !isEvenPeriod;
    };

    qint64 delayMs = -1;
    for (int slotsAhead = 0; slotsAhead <= 4; ++slotsAhead) {
        qint64 const candidateIndex = slotIndex + slotsAhead;
        if (!slotIsOurPeriod(candidateIndex)) {
            continue;
        }
        int const candidateElapsedMs = (slotsAhead == 0) ? currentElapsedMs : 0;
        if (latestStartMs > 0 && candidateElapsedMs >= latestStartMs) {
            continue;
        }
        delayMs = static_cast<qint64>(slotsAhead) * periodMs - currentElapsedMs + 40;
        break;
    }

    if (delayMs < 0) {
        delayMs = static_cast<qint64>(periodMs) * 2 - currentElapsedMs + 40;
    }
    if (delayMs < 40) {
        delayMs = 40;
    }

    scheduleTxAudioPrecompute(qMin<qint64>(delayMs, 250));
    quint64 const retrySerial = ++m_syncTxRetrySerial;
    quint64 const requestGeneration = m_txRequestGeneration;
    m_syncTxRetryScheduled = true;
    QString const modeSnapshot = m_mode;
    decodiumBridgeSyncScheduleLog(QStringLiteral("sync TX late guard: defer TX%1 (%2) mode=%3 elapsed=%4ms latest=%5ms delay=%6ms")
                  .arg(m_currentTx)
                  .arg(reason, modeSnapshot)
                  .arg(elapsedMs)
                  .arg(latestStartMs)
                  .arg(delayMs));
    QTimer::singleShot(delayMs, Qt::PreciseTimer, this, [this, reason, modeSnapshot, retrySerial, requestGeneration]() {
        if (retrySerial != m_syncTxRetrySerial
            || requestGeneration != m_txRequestGeneration) {
            return;
        }
        m_syncTxRetryScheduled = false;
        if (m_mode != modeSnapshot
            || !m_monitoring
            || m_manualTxHold
            || m_transmitting
            || m_tuning) {
            return;
        }

        decodiumBridgeSyncScheduleLog(QStringLiteral("sync TX late guard: retry at valid slot (%1) mode=%2")
                      .arg(reason, modeSnapshot));
        if (tryStartDeferredManualSyncTx()) {
            return;
        }
        if (m_txEnabled || m_autoCqRepeat) {
            checkAndStartPeriodicTx();
        }
    });
}

void DecodiumBridge::ensureSyncTxSchedulerActive(const QString& reason)
{
    if (!usesDeferredManualSyncTx() || !m_monitoring || !m_periodTimer) {
        return;
    }

    if (m_periodTimer->isActive()) {
        return;
    }

    quint64 const sessionId = ++m_periodTimerSessionId;
    decodiumBridgeSyncScheduleLog(QStringLiteral("%1: period timer inactive while sync TX is armed, rearming")
                  .arg(reason));
    armPeriodTimerForCurrentMode(sessionId, reason);
}


