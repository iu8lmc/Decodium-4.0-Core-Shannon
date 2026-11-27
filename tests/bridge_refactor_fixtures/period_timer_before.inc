void DecodiumBridge::onPeriodTimer()
{
    // Progress bar e watchdog usano il counter di tick (cadenza UI invariata).
    // La decisione di alimentare il decoder è invece ancorata al boundary UTC
    // calcolato dall'UTC corretto NTP / periodMs, così il
    // drift del QTimer (±5–50ms/tick su Windows con event loop carico) non
    // sfasa più le finestre passate al decoder.
    m_periodTicks++;
#if DECODIUM_HAS_SSTV
    // A stale queued timeout from the mode that was active before opening the
    // SSTV page must never dispatch a decode or an automatic TX underneath it.
    if (m_sstvWorkspaceActive) {
        m_periodTicks = 0;
        if (m_periodProgress != 0) {
            m_periodProgress = 0;
            emit periodProgressChanged();
        }
        return;
    }
#endif
    qint64 const nowMs     = correctedUtcEpochMs();
    QString const modeSnapshot = m_mode;
    bool   const timeSyncMode = isTimeSyncDecodeMode(modeSnapshot);
    int    const periodMs  = effectivePeriodMsForMode(modeSnapshot);
    qint64 const utcSlot   = (periodMs > 0) ? (nowMs / periodMs) : 0;
    int    const msInSlot  = (periodMs > 0) ? (int)(nowMs % periodMs) : 0;
    // Progress ancorato all'UTC: l'utente vede la barra sincrona al vero
    // boundary, non al counter che può driftare.
    m_periodProgress = (periodMs > 0) ? (msInSlot * 100 / periodMs) : 0;
    emit periodProgressChanged();

    if (rtlSdrGeneralReceiverActive()) {
        // WFM/NFM/AM/SSB/CW are continuous receiver modes. They must not run
        // an FT/JT boundary decode or schedule digital TX from stale PCM.
        m_periodTicks = 0;
        if (m_periodProgress != 0) {
            m_periodProgress = 0;
            emit periodProgressChanged();
        }
        return;
    }

    // === TX watchdog configurabile (allineato a mainwindow m_txWatchdogMode) ===
    // m_txWatchdogMode: 0=off, 1=time-based (monotonic real elapsed), 2=count-based (periodi)
    bool const txWatchdogSessionActive =
        m_txEnabled || m_transmitting || m_autoCqRepeat || m_deferredManualSyncTx;
    bool const txWatchdogConfigured = txWatchdogLimitConfigured();
    if (txWatchdogSessionActive && txWatchdogConfigured) {
        if (!m_txWatchdogElapsedActive || !m_txWatchdogElapsed.isValid()) {
            m_txWatchdogElapsed.restart();
            m_txWatchdogElapsedActive = true;
            m_txWatchdogActiveSinceMs = QDateTime::currentMSecsSinceEpoch();
            m_txWatchdogLastProgressLogMs = 0;
            bridgeLog(QStringLiteral("TX watchdog armed: mode=%1 minutes=%2 count=%3 txEnabled=%4 transmitting=%5 autoCQ=%6 mono=1")
                          .arg(m_txWatchdogMode)
                          .arg(m_txWatchdogTime)
                          .arg(m_txWatchdogCount)
                          .arg(m_txEnabled ? 1 : 0)
                          .arg(m_transmitting ? 1 : 0)
                          .arg(m_autoCqRepeat ? 1 : 0));
        }
        qint64 const txWatchdogElapsedMs = m_txWatchdogElapsed.elapsed();
        ++m_txWatchdogTicks;
        // 1.0.187 — FT2 Pack G: prima del watchdog standard, se siamo in TX3
        // (R+report) stallato per 2+ periodi, forza un re-send TX2 invece di
        // far morire il QSO. Cap a 1 per QSO. Gated Conservative+FT2.
        // La funzione resetta m_txWatchdogTicks a 0; non deve pero' riarmare il
        // watchdog a minuti, che misura la sessione TX/AutoCQ wall-clock.
        if (maybeForceTx2ResendOnStall()) {
            bridgeLog(QStringLiteral("TX watchdog: FT2 TX2 resend reset period counter only"));
        }
        bool watchdogFired = false;
        if (m_txWatchdogMode == 1) {
            qint64 const maxMs = qint64(m_txWatchdogTime) * 60 * 1000;
            if (maxMs > 0 && txWatchdogElapsedMs >= maxMs) {
                watchdogFired = true;
            }
            // Not too chatty: once after 1 minute, then every 5 minutes.
            if (maxMs > 0
                && txWatchdogElapsedMs >= 60000
                && (m_txWatchdogLastProgressLogMs <= 0
                    || txWatchdogElapsedMs - m_txWatchdogLastProgressLogMs >= 300000)) {
                m_txWatchdogLastProgressLogMs = txWatchdogElapsedMs;
                qint64 const remainingMs = qMax<qint64>(0, maxMs - txWatchdogElapsedMs);
                bridgeLog(QStringLiteral("TX watchdog progress: elapsed=%1min remaining=%2min limit=%3min txEnabled=%4 transmitting=%5 autoCQ=%6")
                              .arg(QString::number(double(txWatchdogElapsedMs) / 60000.0, 'f', 1))
                              .arg(QString::number(double(remainingMs) / 60000.0, 'f', 1))
                              .arg(m_txWatchdogTime)
                              .arg(m_txEnabled ? 1 : 0)
                              .arg(m_transmitting ? 1 : 0)
                              .arg(m_autoCqRepeat ? 1 : 0));
            }
        } else if (m_txWatchdogMode == 2) {
            // Count-based: conta i periodi completi; non moltiplicare due volte
            // per m_txWatchdogCount, altrimenti Count=3 scatta dopo 9 periodi.
            int maxTicks = m_periodTicksMax;
            if (maxTicks > 0 && m_txWatchdogTicks >= maxTicks) {
                ++m_autoCQPeriodsMissed;
                m_txWatchdogTicks = 0;  // reset per contare il prossimo periodo
                if (m_autoCQPeriodsMissed >= m_txWatchdogCount) watchdogFired = true;
            }
        }
        if (watchdogFired) {
            qint64 const elapsedMs = qMax<qint64>(0, txWatchdogElapsedMs);
            QString const watchdogReason = QStringLiteral("TX watchdog: timeout mode=%1 elapsed=%2min limit=%3%4 -> halt TX")
                .arg(m_txWatchdogMode)
                .arg(QString::number(double(elapsedMs) / 60000.0, 'f', 1))
                .arg(m_txWatchdogMode == 1 ? QString::number(m_txWatchdogTime)
                                           : QString::number(m_txWatchdogCount))
                .arg(m_txWatchdogMode == 1 ? QStringLiteral("min") : QStringLiteral(" periods"));
            bridgeLog(watchdogReason);
            // 1.0.446 - P0-3 opt-in (default OFF): se il QSO ha gia' completato lo scambio
            // report bidirezionale (m_qsoProgress>=4) e il watchdog scatta, LOGGA il QSO
            // invece di abbandonarlo. 1.0.445 armava solo il late-snapshot (recuperato solo
            // se il partner re-inviava 73; in QSO manuale nemmeno quello). Replica la parte
            // log+chiusura di finishAutoSequenceQso(...,true) (lambda non chiamabile qui).
            if (m_txWatchdogLogOnClose && m_qsoProgress >= 4) {
                QString wdPartner = inferredPartnerForAutolog();
                capturePendingAutoLogSnapshot();
                if (wdPartner.trimmed().isEmpty() && m_pendingAutoLogValid) {
                    wdPartner = m_pendingAutoLogCall;
                }
                markWorldMapQsoClosed(wdPartner, watchdogReason);
                rememberCompletedAutoCqPartner(wdPartner, true, watchdogReason);
                logQso();
                bridgeLog(QStringLiteral("TX watchdog: QSO loggato in chiusura (progress=%1 partner=%2)")
                              .arg(m_qsoProgress).arg(wdPartner));
                if (m_qsoProgress != 6) { m_qsoProgress = 6; emit qsoProgressChanged(); }
                setDxCall(QString());
                setDxGrid(QString());
            } else if ((m_autoCqRepeat || m_multiAnswerMode) && m_qsoProgress > 1) {
                if (m_autoCqRepeat) {
                    armLateAutoLogSnapshot();
                }
                QString const abandonedPartner = inferredPartnerForAutolog();
                if (!abandonedPartner.trimmed().isEmpty()) {
                    rememberCompletedAutoCqPartner(abandonedPartner, false, watchdogReason);
                }
            }
            m_txWatchdogTicks = 0;
            m_txWatchdogActiveSinceMs = 0;
            m_txWatchdogLastProgressLogMs = 0;
            m_txWatchdogElapsedActive = false;
            m_txWatchdogElapsed.invalidate();
            m_autoCQPeriodsMissed = 0;
            m_txRetryCount = 0;
            m_lastNtx = -1;
            m_nTx73 = 0;
            m_autoSeqRogerReportBase.clear();
            clearAutoCqPartnerLock();
            halt();
            setTxEnabled(false);
            emit statusMessage(tr("Transmit watchdog: timed out, transmission stopped"));
        }
    } else {
        if (m_txWatchdogElapsedActive || m_txWatchdogActiveSinceMs > 0) {
            bridgeLog(QStringLiteral("TX watchdog disarmed: txEnabled=%1 transmitting=%2 autoCQ=%3 configured=%4 elapsed=%5min")
                          .arg(m_txEnabled ? 1 : 0)
                          .arg(m_transmitting ? 1 : 0)
                          .arg(m_autoCqRepeat ? 1 : 0)
                          .arg(txWatchdogConfigured ? 1 : 0)
                          .arg(m_txWatchdogElapsed.isValid()
                                   ? QString::number(double(qMax<qint64>(0, m_txWatchdogElapsed.elapsed())) / 60000.0, 'f', 1)
                                   : QStringLiteral("0.0")));
        }
        m_txWatchdogTicks = 0;      // reset quando TX non attivo
        m_txWatchdogActiveSinceMs = 0;
        m_txWatchdogLastProgressLogMs = 0;
        m_txWatchdogElapsedActive = false;
        m_txWatchdogElapsed.invalidate();
        m_autoCQPeriodsMissed = 0;
    }

    maybeDispatchFt8EarlyDecode(utcSlot, msInSlot, periodMs);
    maybeDispatchFt4EarlyDecode(utcSlot, msInSlot, periodMs);
    bool const pendingAutoDecodeBeforeBoundary =
        timeSyncMode && hasPendingTimeSyncDecodeForMode(modeSnapshot);

    // Boundary detection basata su UTC slot: FT8/FT4/FT2 devono alimentare il
    // decoder solo quando cambia davvero lo slot UTC. Su Windows il QTimer puo'
    // scattare pochi ms prima del boundary: usare anche m_periodTicksMax per
    // questi modi produceva un decode completo e subito dopo un frammento
    // da ~3000 campioni. Il counter resta fallback solo per modi non sync.
    qint64 const slotsAdvanced =
        (m_lastPeriodSlot >= 0) ? (utcSlot - m_lastPeriodSlot) : 0;
    bool const utcBoundaryCrossed =
        (m_lastPeriodSlot >= 0) && (utcSlot != m_lastPeriodSlot);
    bool const tickCounterExpired = !timeSyncMode && (m_periodTicks >= m_periodTicksMax);
    if (m_lastPeriodSlot < 0) {
        // Prima chiamata: inizializza lo slot e lascia che il prossimo
        // boundary sia quello di transizione vera.
        m_lastPeriodSlot = utcSlot;
    }
    if (utcBoundaryCrossed && timeSyncMode && slotsAdvanced < 0) {
        bridgeLog(QStringLiteral("period timer UTC slot moved backwards: mode=%1 last=%2 now=%3, resetting RX accumulation")
                      .arg(modeSnapshot)
                      .arg(m_lastPeriodSlot)
                      .arg(utcSlot));
        resetRxPeriodAccumulation(true);
        m_lastPeriodSlot = utcSlot;
        return;
    }
    if (utcBoundaryCrossed || tickCounterExpired) {
        qint64 const completedUtcSlot =
            (periodMs > 0)
                ? ((utcBoundaryCrossed && m_lastPeriodSlot >= 0)
                       ? ((timeSyncMode && slotsAdvanced > 1)
                              ? qMax<qint64>(0, utcSlot - 1)
                              : m_lastPeriodSlot)
                       : qMax<qint64>(0, utcSlot - 1))
                : -1;
        if (timeSyncMode && utcBoundaryCrossed && slotsAdvanced > 1) {
            bridgeLog(QStringLiteral("period timer lag: mode=%1 skippedSlots=%2 last=%3 now=%4 decodingLatestCompleteSlot=%5")
                          .arg(modeSnapshot)
                          .arg(slotsAdvanced - 1)
                          .arg(m_lastPeriodSlot)
                          .arg(utcSlot)
                          .arg(completedUtcSlot));
        }
        m_lastPeriodSlot = utcSlot;
        m_periodTicks = 0;
        m_periodProgress = 0;
        emit periodProgressChanged();
        if (timeSyncMode) {
            qint64 const kTimeSyncDecodeSettleMs =
                modeSnapshot == QStringLiteral("FT8") ? qint64 {650}
                : modeSnapshot == QStringLiteral("FT4") ? qint64 {350}
                : qint64 {1000};
            dispatchTimeSyncDecodeWhenReady(completedUtcSlot, modeSnapshot, m_periodTimerSessionId,
                                            nowMs + kTimeSyncDecodeSettleMs);
        } else {
            feedAudioToDecoder(completedUtcSlot);
        }
        // Fix QSO-completion FT8/FT4: in auto-seq / CQ-repeat dobbiamo aspettare il
        // decode dello slot APPENA finito (dispatchato a +650ms) prima di committare
        // il TX. Una risposta del partner — specie debole, mancata dall'early-visible —
        // viene decodificata ~+0.8-1.0s e deve reindirizzare il TX da CQ a risposta.
        // Senza questa attesa D4 committa CQ al boundary (+137ms) e rinvia la risposta
        // di un intero ciclo -> il partner non riceve risposta in tempo -> QSO non si
        // chiude (loop osservato dal vivo con EA3IXP). pendingAutoDecodeBeforeBoundary
        // da solo e' falso al boundary perche' il decode dello slot finito non e'
        // ancora partito; in time-sync mode aspettiamo sempre il decode imminente.
        bool const autoTxWaitsForDecode =
            shouldDeferAutoTxUntilTimeSyncDecode(modeSnapshot)
            && (pendingAutoDecodeBeforeBoundary || timeSyncMode);
        // Al boundary UTC: un click manuale tardivo in FT8/FT4/FT2 sync
        // viene armato qui invece di partire a meta' slot.
        bool const startedDeferredManualTx = tryStartDeferredManualSyncTx();
        // Al boundary del periodo: verifica se avviare TX automatico
        if (!startedDeferredManualTx) {
            if (autoTxWaitsForDecode) {
                scheduleDeferredAutoTxAfterTimeSyncDecode(modeSnapshot, m_periodTimerSessionId);
            } else {
                checkAndStartPeriodicTx();
            }
        }
        // B9 — Age active stations every period
        if (m_activeStations) m_activeStations->ageAllStations();

        // A2 — Update soundcard drift every ~5 periods
        static int driftCheckCount = 0;
        if (++driftCheckCount >= 5) {
            driftCheckCount = 0;
            qint64 elapsedMs = m_driftClock.elapsed();
            qint64 expectedMs = (qint64)m_driftExpectedFrames * 1000LL / SAMPLE_RATE;
            if (expectedMs > 0 && elapsedMs > 0) {
                double driftNew = (double)(elapsedMs - expectedMs) / (double)expectedMs * 1e6;
                if (qAbs(driftNew - m_soundcardDriftPpm) > 0.5) {
                    m_soundcardDriftPpm = driftNew;
                    emit soundcardDriftChanged();
                }
            }
        }
    }
}

