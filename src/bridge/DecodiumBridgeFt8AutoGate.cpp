#include "DecodiumBridge.h"
#include <QTimer>

bool decodiumBridgeTruthyEnvironmentFlag(const char*);
bool decodiumBridgeTxPeriodIsEven(int);
int decodiumBridgeLatestD3SyncStartMs(const QString&, int, bool);
void decodiumBridgeFt8GateLog(const QString&);

bool DecodiumBridge::usesFt8AutoTxDecodeGate() const
{
    // The FT8 decode gate (1.0.714) keeps a stale TX1/TX3 from going out when the previous
    // slot's decode changes what must be sent. 1.0.722 stops it from losing TX cycles on a PC
    // whose decoder is about as slow as its deadline (IT9MRM, 721): it never skips a slot for a
    // late result once the decoder has been seen to be slow, and it remembers that across
    // clicks, HALTs and QSOs. DECODIUM_FT8_TX_GATE_OFF=1 switches it off altogether.
    static bool const gateDisabled = decodiumBridgeTruthyEnvironmentFlag("DECODIUM_FT8_TX_GATE_OFF");
    if (gateDisabled) {
        return false;
    }
    return m_mode == QStringLiteral("FT8")
        && !usingLegacyBackendForTx() && !usingTciAudioInput() && !m_decoPortUseRemote
        && !mamMultiStreamSequencerActive()
        && !m_cwTxActive && !m_telemetryTxActive
        && m_explicitManualTxRequest == 0
        && !m_callsign.trimmed().isEmpty()
        && ((m_autoSeq && m_txEnabled) || m_autoCqRepeat);
}

decodium::Ft8AutoTxDecodeGate::Context DecodiumBridge::ft8AutoTxDecodeContext() const
{
    return {correctedUtcEpochMs() / decodium::Ft8AutoTxDecodeGate::periodMs,
            m_periodTimerSessionId, m_txRequestGeneration};
}

int DecodiumBridge::syncTxCommitDeadlineMs(bool relaxed) const
{
    if (usesFt8AutoTxDecodeGate()) return m_ft8AutoTxDecodeGate.commitDeadline();
    return decodiumBridgeLatestD3SyncStartMs(m_mode, effectivePeriodMsForMode(m_mode), relaxed);
}

bool DecodiumBridge::validateFt8AutoTxAudioStart(qint64 payloadMs)
{
    if (!m_activeFt8AutoTxFullFrame) return true;
    using Gate = decodium::Ft8AutoTxDecodeGate;
    auto const context = ft8AutoTxDecodeContext();
    int const elapsedMs = int(correctedUtcEpochMs() % Gate::periodMs);
    if (!(context == m_activeFt8AutoTxContext) || !Gate::fullFrameFits(elapsedMs, payloadMs)) {
        decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: audio deadline missed, cancelling full frame elapsed=%1ms payload=%2ms")
                      .arg(elapsedMs).arg(payloadMs));
        completeTxPlayback(QStringLiteral("ft8-auto-audio-deadline"), true);
        return false;
    }
    m_ft8AutoTxDecodeGate.committed(context);
    return true;
}

bool DecodiumBridge::deferFt8AutoTxForDecode()
{
    if (!usesFt8AutoTxDecodeGate()) return false;
    using Gate = decodium::Ft8AutoTxDecodeGate;
    auto const context = ft8AutoTxDecodeContext();
    bool const isEven = context.slot % 2 == 0;
    if (decodiumBridgeTxPeriodIsEven(m_txPeriod) != isEven) return true;
    int const elapsedMs = int(correctedUtcEpochMs() % Gate::periodMs);
    // TX4 (RR73) and TX5 (73) were fixed by the previous exchange: no reply can change them.
    bool const finalMessage = m_currentTx == 4 || m_currentTx == 5;
    auto const decision = m_ft8AutoTxDecodeGate.decision(context, elapsedMs, finalMessage);
    if (decision == Gate::Decision::Ready) {
        if (m_ft8AutoTxDecodeGate.lateDecoderBypass(finalMessage)
            && m_ft8AutoTxDecodeGate.armTimer(context)) {
            decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: decoder late for %1 slot(s), sending TX%2 without waiting for the RX result elapsed=%3ms")
                          .arg(m_ft8AutoTxDecodeGate.lateStreak()).arg(m_currentTx).arg(elapsedMs));
        }
        return false;
    }

    // One timer per slot/lifecycle, shared by ALL automatic TX entry points.
    // A result callback releases this gate only after auto-seq has run. At
    // the deadline a missing result skips the slot, never sends stale TX1/3.
    if (m_ft8AutoTxDecodeGate.armTimer(context)) {
        if (decision == Gate::Decision::SkipSlot) {
            decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: skip slot, RX result not ready in time elapsed=%1ms").arg(elapsedMs));
        } else {
            int const deadlineMs = m_ft8AutoTxDecodeGate.decodeDeadline();
            int const delayMs = qMax(1, deadlineMs - elapsedMs);
            decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: waiting for previous RX slot, deadline=%1ms remaining=%2ms%3")
                          .arg(deadlineMs).arg(delayMs)
                          .arg(m_ft8AutoTxDecodeGate.slowDecoder() ? QStringLiteral(" (slow decoder)") : QString()));
            QTimer::singleShot(delayMs, Qt::PreciseTimer, this, [this, context]() {
                if (!(context == ft8AutoTxDecodeContext())
                    || !m_ft8AutoTxDecodeGate.matches(context)
                    || !usesFt8AutoTxDecodeGate() || m_manualTxHold
                    || !m_monitoring || m_transmitting || m_tuning) return;
                int const elapsed = int(correctedUtcEpochMs() % Gate::periodMs);
                bool const finalMessage = m_currentTx == 4 || m_currentTx == 5;
                if (m_ft8AutoTxDecodeGate.decision(context, elapsed, finalMessage) == Gate::Decision::Ready
                    && !m_ft8AutoTxDecodeGate.resultReady(context)) {
                    // Slow decoder: the extended wait ran out, send instead of losing the cycle.
                    decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: slow decoder, sending TX%1 without the RX result elapsed=%2ms")
                                  .arg(m_currentTx).arg(elapsed));
                } else if (m_ft8AutoTxDecodeGate.decision(context, elapsed, finalMessage) != Gate::Decision::Ready) {
                    // The RX result is late: count it, so a decoder that keeps missing the
                    // deadline stops starving the sequencer (eldiablo.log, 26/10/2026).
                    m_ft8AutoTxDecodeGate.deadlineExpired(context);
                    if (m_ft8AutoTxDecodeGate.decision(context, elapsed, finalMessage) != Gate::Decision::Ready) {
                        decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: decode deadline expired, skip stale TX%1 in this slot")
                                      .arg(m_currentTx));
                        return;
                    }
                    decodiumBridgeFt8GateLog(QStringLiteral("FT8 auto TX: decoder late for %1 slot(s), sending TX%2 without the RX result elapsed=%3ms")
                                  .arg(m_ft8AutoTxDecodeGate.lateStreak()).arg(m_currentTx).arg(elapsed));
                }
                checkAndStartPeriodicTx();
            });
        }
    }
    return true;
}

