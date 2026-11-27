void DecodiumBridge::scheduleSmartFt2AsyncTx(const QString& reason)
{
    static constexpr qint64 kSmartFt2AsyncTxMaxWaitMs = 2500;
    static constexpr int kSmartFt2AsyncTxMaxRetries = 25;

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    bool const retry = (reason == QStringLiteral("retry"));
    if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled
        || m_manualTxHold || !m_monitoring || m_transmitting || m_tuning || !m_txEnabled) {
        m_ft2AsyncSmartTxPending = false;
        ++m_ft2AsyncSmartTxSerial;
        m_ft2AsyncSmartTxDeadlineMs = 0;
        m_ft2AsyncSmartTxRetries = 0;
        bridgeLog(QStringLiteral("smartFt2Tx [%1]: cancelled mode=%2 async=%3 manualHold=%4 monitoring=%5 tx=%6 tune=%7 txEnabled=%8")
                      .arg(reason, m_mode)
                      .arg(m_asyncTxEnabled ? 1 : 0)
                      .arg(m_manualTxHold ? 1 : 0)
                      .arg(m_monitoring ? 1 : 0)
                      .arg(m_transmitting ? 1 : 0)
                      .arg(m_tuning ? 1 : 0)
                      .arg(m_txEnabled ? 1 : 0));
        return;
    }
    if (!retry && m_ft2AsyncSmartTxPending) {
        bridgeLog(QStringLiteral("smartFt2Tx [%1]: coalesced with pending timer").arg(reason));
        return;
    }
    if (!retry || m_ft2AsyncSmartTxDeadlineMs <= nowMs) {
        ++m_ft2AsyncSmartTxSerial;
        m_ft2AsyncSmartTxDeadlineMs = nowMs + kSmartFt2AsyncTxMaxWaitMs;
        m_ft2AsyncSmartTxRetries = 0;
    } else {
        ++m_ft2AsyncSmartTxRetries;
    }
    qint64 const remainingBudgetMs = m_ft2AsyncSmartTxDeadlineMs - nowMs;
    if (remainingBudgetMs <= 0 || m_ft2AsyncSmartTxRetries > kSmartFt2AsyncTxMaxRetries) {
        m_ft2AsyncSmartTxPending = false;
        bridgeLog(QStringLiteral("smartFt2Tx [%1]: stopped after budget/retry cap remaining=%2 retries=%3")
                      .arg(reason)
                      .arg(remainingBudgetMs)
                      .arg(m_ft2AsyncSmartTxRetries));
        return;
    }

    // Sistema integrato anti-collisione FT2 async. Combina 5 strategie
    // selezionate autonomamente in base allo stato del canale:
    //  S1 SLOT-EST   stima fine-frame del partner da timestamp primo decode
    //  S2 RMS-GATE   richiede audio sotto soglia per ~300ms (quiet)
    //  S3 DECODE-Q   richiede no-new-decode per >400ms
    //  S4 JITTER     anti-collisione multi-stazione (hash callsign 50-250ms)
    //  S5 HARD CAP   max 2500ms wait (safety)
    // FIX temporale (1.0.355): nowMs e' wall-clock e lo usano i gate RMS/decode-quiet
    // (che lavorano su timestamp wall-clock). Il branch slot-anchored invece ragiona
    // in correctedUtcEpochMs() (tempo-segnale, con offset NTP applicato). Confrontare
    // un end-time in scala corrected contro nowMs wall-clock introduce un errore pari
    // all'offset NTP (~97ms). refNowMs seleziona la scala corretta per ciascun branch.
    qint64 const nowCorrectedMs = correctedUtcEpochMs();
    qint64 refNowMs = nowMs;

    // S1: Slot estimation — partner ha iniziato TX ~3200ms prima del primo
    //     decode (DSP early decode arriva ad ~80-90% del frame). Frame dura
    //     ~3800ms. End = start + 3800. Aggiungi 200ms di safety.
    qint64 slotDelay = 0;
    qint64 estimatedPartnerStartMs = 0;
    qint64 estimatedPartnerEndMs   = 0;
    // FIX A: ancora la stima al TEMPO-SEGNALE (slot-boundary del frame del
    // partner ricavato dal clock UTC corretto + DT del decode), NON al
    // wall-clock di QUANDO la CPU ha finito il decode (m_ft2AsyncFirstDecodeMs,
    // che varia 200ms..2.6s -> frame-end sbagliato -> TX a caso).
    qint64 wallClockEndMs = 0;
    if (m_ft2AsyncFirstDecodeMs > 0) {
        estimatedPartnerStartMs = m_ft2AsyncFirstDecodeMs - 3200;
        wallClockEndMs          = estimatedPartnerStartMs + 3800;
    }
    if (m_ft2AsyncPartnerSlotMs > 0) {
        int const payloadMs = estimatedSyncPayloadMs(QStringLiteral("FT2"));
        estimatedPartnerStartMs = m_ft2AsyncPartnerSlotMs;
        estimatedPartnerEndMs   = m_ft2AsyncPartnerSlotMs + payloadMs;
        // estimatedPartnerEndMs e' in scala corrected-UTC -> confronta con nowCorrectedMs.
        refNowMs = nowCorrectedMs;
        // Log: signalEnd e wallEnd sono entrambi "ms alla fine", ciascuno nella PROPRIA
        // scala (signalEnd in corrected, wallEnd in wall-clock); diff = differenza tra le
        // due stime ms-alla-fine (non un mix di scale diverse come prima).
        bridgeLog(QStringLiteral("smartFt2Tx slot-anchor: signalEnd=%1ms wallEnd=%2ms diff=%3ms")
                  .arg(estimatedPartnerEndMs - nowCorrectedMs)
                  .arg(wallClockEndMs > 0 ? wallClockEndMs - nowMs : 0)
                  .arg(wallClockEndMs > 0
                           ? (estimatedPartnerEndMs - nowCorrectedMs) - (wallClockEndMs - nowMs)
                           : 0));
    } else {
        // Fallback wall-clock SOLO se il dato signal-time non e' disponibile.
        estimatedPartnerEndMs = wallClockEndMs;
        refNowMs = nowMs;
    }
    if (estimatedPartnerEndMs > refNowMs) {
        slotDelay = estimatedPartnerEndMs - refNowMs + 200;
    }

    // 1.0.447 Leva#6-A: requisiti gate adattivi (opt-in). OFF -> 3/400/50/200, byte-identico.
    int rmsRunsReq = 3, decodeQuietReq = 400, jitterBase = 50, jitterSpan = 200;
    ft2AsyncGateRequirements(nowMs, rmsRunsReq, decodeQuietReq, jitterBase, jitterSpan);
    // S2: RMS gate
    bool const rmsQuiet = (m_ft2AsyncAudioQuietRuns >= rmsRunsReq);

    // S3: Decode-quiet
    qint64 const decodeQuietMs = m_ft2AsyncLastDecodeMs > 0
        ? (nowMs - m_ft2AsyncLastDecodeMs) : 999999;
    bool const decodeQuiet = (decodeQuietMs > decodeQuietReq);

    // Decision tree:
    //  - se slotDelay molto grande (>500ms) usa quello (canale chiaramente occupato)
    //  - altrimenti aspetta che entrambi i gate (RMS + decode-quiet) siano OK
    //  - aggiungi jitter
    qint64 finalDelay = 0;
    QString strategy;
    if (slotDelay > 500) {
        finalDelay = slotDelay;
        strategy = QStringLiteral("S1-slot-est");
    } else if (!rmsQuiet || !decodeQuiet) {
        finalDelay = 100;  // poll
        strategy = QStringLiteral("S2-S3-poll");
    } else {
        // Tutti i gate OK → aggiungi jitter (S4) e go
        QString const myCall = m_callsign.trimmed();
        int hashAcc = 0;
        for (auto c : myCall) hashAcc = (hashAcc * 31 + c.unicode()) & 0xff;
        int const jitterMs = jitterBase + (hashAcc % jitterSpan);
        finalDelay = jitterMs;
        strategy = QStringLiteral("S4-jitter");
    }
    if (finalDelay > kSmartFt2AsyncTxMaxWaitMs) finalDelay = kSmartFt2AsyncTxMaxWaitMs;  // S5 per-delay cap
    QString safeSlotAdjustment;
    qint64 const safeFinalDelay = safeFt2AsyncTxDelay(finalDelay, &safeSlotAdjustment);
    if (safeFinalDelay != finalDelay) {
        strategy += QStringLiteral("+") + safeSlotAdjustment;
        finalDelay = safeFinalDelay;
    }
    finalDelay = qBound<qint64>(0, finalDelay, remainingBudgetMs);

    bridgeLog(QStringLiteral("smartFt2Tx [%1]: slotDelay=%2 rmsQuiet=%3 decodeQuiet=%4ms strategy=%5 final=%6ms retry=%7 budget=%8ms")
              .arg(reason)
              .arg(slotDelay)
              .arg(rmsQuiet ? 1 : 0)
              .arg(decodeQuietMs)
              .arg(strategy)
              .arg(finalDelay)
              .arg(m_ft2AsyncSmartTxRetries)
              .arg(remainingBudgetMs));

    if (finalDelay <= 0) {
        m_ft2AsyncSmartTxPending = false;
        if (!m_transmitting && !m_tuning && m_txEnabled) {
            checkAndStartPeriodicTx();
        }
        return;
    }
    quint64 const serial = m_ft2AsyncSmartTxSerial;
    quint64 const requestGeneration = m_txRequestGeneration;
    m_ft2AsyncSmartTxPending = true;
    QTimer::singleShot((int)finalDelay, this, [this, serial, requestGeneration]() {
        if (serial != m_ft2AsyncSmartTxSerial
            || requestGeneration != m_txRequestGeneration) return;
        m_ft2AsyncSmartTxPending = false;
        if (m_mode != QStringLiteral("FT2") || !m_asyncTxEnabled
            || m_manualTxHold || !m_monitoring || m_transmitting || m_tuning || !m_txEnabled) {
            return;
        }
        if (QDateTime::currentMSecsSinceEpoch() >= m_ft2AsyncSmartTxDeadlineMs
            || m_ft2AsyncSmartTxRetries >= kSmartFt2AsyncTxMaxRetries) {
            bridgeLog(QStringLiteral("smartFt2Tx: timer stopped at budget/retry cap retries=%1")
                          .arg(m_ft2AsyncSmartTxRetries));
            return;
        }
        // Re-check gates
        qint64 const nowMs2 = QDateTime::currentMSecsSinceEpoch();
        qint64 const decodeQuietMs2 = m_ft2AsyncLastDecodeMs > 0
            ? (nowMs2 - m_ft2AsyncLastDecodeMs) : 999999;
        int rmsRunsReq2 = 3, decodeQuietReq2 = 400, jitterBase2 = 50, jitterSpan2 = 200;
        ft2AsyncGateRequirements(nowMs2, rmsRunsReq2, decodeQuietReq2, jitterBase2, jitterSpan2);
        Q_UNUSED(jitterBase2); Q_UNUSED(jitterSpan2);
        bool const allClear = (m_ft2AsyncAudioQuietRuns >= rmsRunsReq2) && (decodeQuietMs2 > decodeQuietReq2);
        if (allClear) {
            bridgeLog(QStringLiteral("smartFt2Tx: gates clear after wait, TX now (decodeQuiet=%1ms quietRuns=%2)")
                      .arg(decodeQuietMs2).arg(m_ft2AsyncAudioQuietRuns));
            int elapsedMs = -1;
            int latestStartMs = 0;
            int delayMs = 0;
            if (isFt2AsyncTxStartTooLate(&elapsedMs, &latestStartMs, &delayMs)) {
                scheduleFt2AsyncTxAtNextSafeSlot(QStringLiteral("smartFt2Tx"), elapsedMs, latestStartMs, delayMs);
                return;
            }
            checkAndStartPeriodicTx();
        } else {
            // Gates ancora non puliti: bounded single-flight retry.
            scheduleSmartFt2AsyncTx(QStringLiteral("retry"));
        }
    });
}

