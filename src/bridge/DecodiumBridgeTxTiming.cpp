#include "DecodiumBridge.h"
#include <QTimer>
#include <QAudioFormat>

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeCatSuppressedForTx();
int decodiumBridgeLatestFt2AsyncTxStartMs(int, bool);
int decodiumBridgeTxSyncLeadInMsForMode(const QString&);

qint64 DecodiumBridge::syncTxPcmStartOffsetBytes(const QAudioFormat& format,
                                                 qint64 pcmSizeBytes,
                                                 int* elapsedMsOut) const
{
    if (elapsedMsOut) {
        *elapsedMsOut = -1;
    }
    if (!shouldAlignTxAudioToCurrentSyncSlot()
        || pcmSizeBytes <= 0
        || !format.isValid()
        || format.sampleRate() <= 0
        || format.bytesPerFrame() <= 0) {
        return 0;
    }

    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        return 0;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    if (elapsedMsOut) {
        *elapsedMsOut = elapsedMs;
    }
    if (elapsedMs <= 0) {
        return 0;
    }

    int const leadInMs = decodiumBridgeTxSyncLeadInMsForMode(m_mode);
    int const payloadElapsedMs = elapsedMs - leadInMs;
    if (payloadElapsedMs <= 0) {
        return 0;
    }
    // The bounded automatic reply sends the WHOLE frame. The final audio
    // start guard cancels an overrun instead of clipping the first Costas.
    if (m_mode == QStringLiteral("FT8") && m_activeFt8AutoTxFullFrame) return 0;

    // Fix QSO-completion: partenza tardiva (es. TX di risposta committato dopo il
    // decode dello slot precedente, ~+0.9-1.2s). Se l'INTERA forma d'onda entra ancora
    // nello slot e il DT resta decodificabile, SHIFTA (invia tutto da ora, nessun trim)
    // invece di TRONCARE il fronte: troncare distrugge la prima Costas e il partner non
    // decodifica -> QSO non si chiude. Trim solo se davvero troppo tardi (la coda
    // uscirebbe dallo slot) o DT oltre la tolleranza del decoder remoto.
    {
        qint64 const frameBytesShift = qMax<qint64>(1, format.bytesPerFrame());
        qint64 const payloadFrames = pcmSizeBytes / frameBytesShift;
        int const payloadDurationMs =
            static_cast<int>((payloadFrames * qint64 {1000}) / qMax(1, format.sampleRate()));
        int const slotTailMarginMs = 250;   // margine di coda prima della fine slot
        int maxShiftDtMs = 2000;            // DT max for modes that tolerate late shifted audio
        if (m_mode == QStringLiteral("FT8") || m_mode == QStringLiteral("FT4")) {
            maxShiftDtMs = decodiumBridgeTxSyncLeadInMsForMode(m_mode) + 150;
        }
        if (elapsedMs <= maxShiftDtMs
            && (elapsedMs + payloadDurationMs + slotTailMarginMs) <= periodMs) {
            return 0;  // shift: invia l'intera forma d'onda da ora, nessun trim del fronte
        }
    }

    qint64 const frameBytes = qMax<qint64>(1, format.bytesPerFrame());
    qint64 const offsetFrames =
        (static_cast<qint64>(payloadElapsedMs) * static_cast<qint64>(format.sampleRate())) / 1000;
    qint64 offsetBytes = offsetFrames * frameBytes;
    offsetBytes -= offsetBytes % frameBytes;
    if (offsetBytes >= pcmSizeBytes) {
        return qMax<qint64>(0, pcmSizeBytes - frameBytes);
    }
    return qMax<qint64>(0, offsetBytes);
}

bool DecodiumBridge::shouldAlignTxAudioToCurrentSyncSlot() const
{
    // CW-audio: durata variabile, nessun allineamento agli slot FT (evita anche
    // scheduleSyncTxBoundaryStop e i guard "late slot", che chiamano questa).
    if (m_cwTxActive) return false;
    if (m_ft2LinkTxActive) return false;
    // Elastic Fox cycle: the frame starts wherever the cycle says, not on the
    // slot grid, so nothing may align it to the slot (lead-in, PCM offset, late
    // start guard, boundary stop). Cleared when the transmission ends.
    if (m_foxElasticTxNow && foxElasticActive()) return false;
    QString const normalized = m_mode.trimmed().toUpper();
    // JT4 is a 60-second synchronous waveform.  Whenever this bridge owns
    // PCM playback, align the payload regardless of platform.  With CAT
    // enabled the legacy backend remains the owner and this path is not used.
    if (normalized == QStringLiteral("JT4")
        && (decodiumBridgeCatSuppressedForTx() || !usingLegacyBackendForTx())) {
        return true;
    }
    if (normalized == QStringLiteral("JT65") && decodiumBridgeCatSuppressedForTx()) {
        return true;
    }
    bool const ft2AutoCqCalling =
        normalized == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && m_autoCqRepeat
        && m_currentTx == 6;
    return normalized == QStringLiteral("FT8")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("Q65")
        || normalized.startsWith(QStringLiteral("Q65-"))
        || normalized == QStringLiteral("MSK144")
        || (normalized == QStringLiteral("FT2") && (!m_asyncTxEnabled || ft2AutoCqCalling));
}

bool DecodiumBridge::isSyncTxStartTooLate(int* elapsedMsOut, int* latestStartMsOut) const
{
    if (elapsedMsOut) {
        *elapsedMsOut = -1;
    }
    if (latestStartMsOut) {
        *latestStartMsOut = 0;
    }
    if (!shouldAlignTxAudioToCurrentSyncSlot()) {
        return false;
    }

    int const periodMs = effectivePeriodMsForMode(m_mode);
    if (periodMs <= 0) {
        return false;
    }

    int const latestStartMs = syncTxCommitDeadlineMs(m_ftxImmediateClickTx);
    if (latestStartMsOut) {
        *latestStartMsOut = latestStartMs;
    }
    if (latestStartMs <= 0) {
        return false;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    if (elapsedMsOut) {
        *elapsedMsOut = elapsedMs;
    }
    return elapsedMs >= latestStartMs;
}

bool DecodiumBridge::isFt2AsyncTxStartTooLate(int* elapsedMsOut,
                                              int* latestStartMsOut,
                                              int* delayToNextSlotMsOut) const
{
    if (elapsedMsOut) {
        *elapsedMsOut = -1;
    }
    if (latestStartMsOut) {
        *latestStartMsOut = 0;
    }
    if (delayToNextSlotMsOut) {
        *delayToNextSlotMsOut = 0;
    }

    if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled) {
        return false;
    }

    int const periodMs = periodMsForMode(m_mode);
    int const latestStartMs = decodiumBridgeLatestFt2AsyncTxStartMs(periodMs, m_ft2ConservativeTiming);
    if (latestStartMsOut) {
        *latestStartMsOut = latestStartMs;
    }
    if (periodMs <= 0 || latestStartMs <= 0) {
        return false;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    int delayMs = periodMs - elapsedMs + 40;
    if (delayMs <= 40 || delayMs > periodMs + 40) {
        delayMs = periodMs + 40;
    }

    if (elapsedMsOut) {
        *elapsedMsOut = elapsedMs;
    }
    if (delayToNextSlotMsOut) {
        *delayToNextSlotMsOut = delayMs;
    }
    return elapsedMs >= latestStartMs;
}

qint64 DecodiumBridge::safeFt2AsyncTxDelay(qint64 requestedDelayMs, QString* adjustmentOut) const
{
    if (adjustmentOut) {
        adjustmentOut->clear();
    }
    if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled) {
        return requestedDelayMs;
    }

    int const periodMs = periodMsForMode(m_mode);
    int const latestStartMs = decodiumBridgeLatestFt2AsyncTxStartMs(periodMs, m_ft2ConservativeTiming);
    if (periodMs <= 0 || latestStartMs <= 0) {
        return requestedDelayMs;
    }

    qint64 const safeRequestedDelay = qMax<qint64>(0, requestedDelayMs);
    qint64 const nowMs = correctedUtcEpochMs();
    int const elapsedMs = static_cast<int>(nowMs % static_cast<qint64>(periodMs));
    int const targetElapsedMs =
        static_cast<int>((nowMs + safeRequestedDelay) % static_cast<qint64>(periodMs));
    if (targetElapsedMs < latestStartMs) {
        return requestedDelayMs;
    }

    int delayMs = periodMs - elapsedMs + 40;
    if (delayMs <= 40 || delayMs > periodMs + 40) {
        delayMs = periodMs + 40;
    }
    if (adjustmentOut) {
        *adjustmentOut = QStringLiteral("FT2-safe-slot target=%1ms latest=%2ms")
                             .arg(targetElapsedMs)
                             .arg(latestStartMs);
    }
    return delayMs;
}

void DecodiumBridge::scheduleFt2AsyncTxAtNextSafeSlot(const QString& reason,
                                                      int elapsedMs,
                                                      int latestStartMs,
                                                      int delayMs)
{
    if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled || !m_txEnabled || m_manualTxHold) {
        return;
    }

    if (elapsedMs < 0 || latestStartMs <= 0 || delayMs <= 0) {
        int computedElapsedMs = -1;
        int computedLatestStartMs = 0;
        int computedDelayMs = 0;
        isFt2AsyncTxStartTooLate(&computedElapsedMs, &computedLatestStartMs, &computedDelayMs);
        if (elapsedMs < 0) {
            elapsedMs = computedElapsedMs;
        }
        if (latestStartMs <= 0) {
            latestStartMs = computedLatestStartMs;
        }
        if (delayMs <= 0) {
            delayMs = computedDelayMs;
        }
    }

    if (delayMs <= 0) {
        return;
    }

    // FIX 1.0.364 — rilevatore di fase-bloccata + sfasamento automatico (anti same-slot
    // lock FT2). In FT2 half-duplex due stazioni devono ALTERNARE la trasmissione: se
    // entrambe trasmettono nella stessa fase non si sentono (deadlock, confermato dai
    // log a 2 lati ZL1BW/ZL3DMH). Se stiamo ritrasmettendo lo stesso TX (m_txRetryCount
    // >= 3, QSO bloccato) e lo slot in cui trasmetteremmo ha la STESSA parita' dello
    // slot del partner (m_ft2AsyncPartnerSlotMs, vedi Fix A v2) -> aggiungo un periodo
    // cosi' ci spostiamo nella fase OPPOSTA (trasmettiamo quando il partner ascolta).
    // Auto-stabilizzante: una volta in fase opposta la parita' e' diversa e non sfasa
    // piu'. Rompe il lock anche se l'altra stazione gira una versione vecchia.
    if (m_ft2AsyncPartnerSlotMs > 0 && m_txRetryCount >= 3) {
        int const ft2LockPeriodMs = periodMsForMode(QStringLiteral("FT2"));
        if (ft2LockPeriodMs > 0) {
            qint64 const lockNowMs = correctedUtcEpochMs();
            qint64 const ourSlot = (lockNowMs + delayMs) / static_cast<qint64>(ft2LockPeriodMs);
            qint64 const partnerSlot = m_ft2AsyncPartnerSlotMs / static_cast<qint64>(ft2LockPeriodMs);
            qint64 const parity = (((ourSlot - partnerSlot) % 2) + 2) % 2;
            if (parity == 0) {
                delayMs += ft2LockPeriodMs;
                decodiumBridgeAudioLevelLog(QStringLiteral("FT2 phase-lock breaker: stessa fase TX del partner per %1 retry -> sfaso +1 periodo (ourSlot=%2 partnerSlot=%3 newDelay=%4ms)")
                              .arg(m_txRetryCount).arg(ourSlot).arg(partnerSlot).arg(delayMs));
            }
        }
    }

    decodiumBridgeAudioLevelLog(QStringLiteral("FT2 async late TX guard: defer TX%1 (%2) elapsed=%3ms latest=%4ms delay=%5ms")
                  .arg(m_currentTx)
                  .arg(reason)
                  .arg(elapsedMs)
                  .arg(latestStartMs)
                  .arg(delayMs));
    scheduleTxAudioPrecompute(qMin(delayMs, 250));

    if (m_periodicTxCheckScheduled) {
        return;
    }
    quint64 const scheduleSerial = m_ft2AsyncSmartTxSerial;
    quint64 const requestGeneration = m_txRequestGeneration;
    int const scheduledTx = m_currentTx;
    QString const scheduledPayload = buildCurrentTxMessage().trimmed();
    m_periodicTxCheckScheduled = true;
    QTimer::singleShot(delayMs, this, [this, reason, scheduleSerial, requestGeneration, scheduledTx, scheduledPayload]() {
        m_periodicTxCheckScheduled = false;
        if (scheduleSerial != m_ft2AsyncSmartTxSerial
            || requestGeneration != m_txRequestGeneration
            || scheduledTx != m_currentTx
            || (!scheduledPayload.isEmpty()
                && scheduledPayload != buildCurrentTxMessage().trimmed())) {
            decodiumBridgeAudioLevelLog(QStringLiteral("FT2 async late TX guard: stale retry skipped (%1) scheduledTX=%2 currentTX=%3")
                          .arg(reason)
                          .arg(scheduledTx)
                          .arg(m_currentTx));
            return;
        }
        if (m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && m_txEnabled
            && !m_manualTxHold
            && !m_transmitting
            && !m_tuning) {
            decodiumBridgeAudioLevelLog(QStringLiteral("FT2 async late TX guard: retry at safe slot (%1)")
                          .arg(reason));
            checkAndStartPeriodicTx();
        }
    });
}

