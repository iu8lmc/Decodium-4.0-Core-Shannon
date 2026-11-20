void DecodiumBridge::feedAudioToDecoder(qint64 completedUtcSlot)
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        return;
    }
#endif
    QString const modeSnapshot = m_mode;
    // RTTY has a streaming native modem.  This is a final asynchronous
    // boundary for an FT8/FT4/JT dispatch queued before the RTTY switch.
    if (isStreamingKeyboardMode(modeSnapshot)) {
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
        }
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        return;
    }
    int const decodeDepth = effectiveDecodeDepth();

    // FT2-Link has its own streaming RX path (ft2LinkRxSamplesReady ->
    // FT2LinkQmlAdapter::ingestRxSamples).  Never dispatch the legacy FT2/FT8
    // end-of-period decoder here: it only produces confusing "FT2 decode
    // completed" diagnostics and wastes CPU while FT2-Link is active.
    if (isFt2LinkApplicationMode(modeSnapshot)) {
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        static qint64 s_lastFt2LinkSkipLogMs = 0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - s_lastFt2LinkSkipLogMs >= 10000) {
            s_lastFt2LinkSkipLogMs = nowMs;
            bridgeLog(QStringLiteral(
                "feedAudioToDecoder: skip legacy FTx decode in FT2-Link mode slot=%1")
                          .arg(completedUtcSlot));
        }
        return;
    }

    // The embedded backend already captures and decodes legacy-owned RX on
    // every supported platform. Do not create a second worker or report its
    // intentionally empty bridge buffer. Forced snapshots are used by the
    // WAV/offline path and must still reach the modern in-process decoder.
    if (usingLegacyBackendForRx() && !m_forcedDecodeAudioSnapshotActive) {
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        return;
    }

    // Defensive on-demand path for WAV/offline decode and very early manual
    // monitor actions that can precede the post-QML startup stage.
    ensureDecodeWorkerForMode(modeSnapshot);

    // Snapshot atomico del buffer audio sotto lock: il callback del sink
    // (DecodiumAudioSink::writeData) può eseguire append() in parallelo da
    // un thread del backend audio. Sfruttiamo il copy-on-write di QVector:
    // l'assegnazione `audioSnapshot = m_audioBuffer` incrementa solo il
    // refcount; `clear()` poi alloca un nuovo storage vuoto per m_audioBuffer
    // mentre `audioSnapshot` continua a possedere i dati del periodo appena
    // chiuso. Tutto il dispatch ai worker usa la snapshot locale, senza tenere
    // il lock per tutto il routing dei mode.
    QVector<short> audioSnapshot;
    QString emptyAudioReason;
    int carriedSamples = 0;
    bool usedForcedAudioSnapshot = false;
    int const targetSamples = isTimeSyncDecodeMode(modeSnapshot)
                                  ? targetDecodeSamplesForMode(modeSnapshot)
                                  : 0;
    if (m_forcedDecodeAudioSnapshotActive) {
        audioSnapshot = m_forcedDecodeAudioSnapshot;
        m_forcedDecodeAudioSnapshot.clear();
        m_forcedDecodeAudioSnapshotActive = false;
        usedForcedAudioSnapshot = true;
        if (audioSnapshot.isEmpty()) {
            QString const reason =
                QStringLiteral("Buffer audio vuoto (forced snapshot mode=%1 monitor=%2 sink=%3 rxSuspended=%4 driftExpected=%5)")
                    .arg(modeSnapshot)
                    .arg(m_monitoring ? "on" : "off")
                    .arg(m_audioSink ? "alive" : "null")
                    .arg(m_rxAudioSuspendedForTx ? "yes" : "no")
                    .arg(static_cast<qint64>(m_driftExpectedFrames.load(std::memory_order_relaxed)));
            emptyAudioReason = reason;
        }
    } else {
        QMutexLocker locker(&m_audioBufferMutex);
        if (m_audioBuffer.isEmpty()) {
            // Diagnostica dettagliata: questo path in passato era silenzioso
            // e mascherava i casi in cui il sink audio era fermo / scollegato.
            // Emettiamo mode, stato monitor, presenza del sink e se la cattura
            // era sospesa per TX — così il log basta a capire la causa.
            QString const reason =
                QStringLiteral("Buffer audio vuoto (mode=%1 monitor=%2 sink=%3 rxSuspended=%4 driftExpected=%5)")
                    .arg(modeSnapshot)
                    .arg(m_monitoring ? "on" : "off")
                    .arg(m_audioSink ? "alive" : "null")
                    .arg(m_rxAudioSuspendedForTx ? "yes" : "no")
                    .arg(static_cast<qint64>(m_driftExpectedFrames.load(std::memory_order_relaxed)));
            emptyAudioReason = reason;
        } else {
            if (targetSamples > 0 && m_audioBuffer.size() > targetSamples) {
                audioSnapshot = m_audioBuffer.mid(0, targetSamples);
                QVector<short> carry = m_audioBuffer.mid(targetSamples);
                carriedSamples = carry.size();
                m_audioBuffer = carry;
                m_audioBuffer.reserve(targetSamples);
            } else {
                audioSnapshot = m_audioBuffer;
                m_audioBuffer.clear();
                m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
            }
        }
    }

    if (!emptyAudioReason.isEmpty()) {
        bool const expectedTxGap =
            m_rxAudioSuspendedForTx || m_transmitting || m_tuning;
        if (expectedTxGap) {
            static qint64 s_lastExpectedEmptyDuringTxLogMs = 0;
            qint64 const logNowMs = QDateTime::currentMSecsSinceEpoch();
            if (logNowMs - s_lastExpectedEmptyDuringTxLogMs >= 750) {
                s_lastExpectedEmptyDuringTxLogMs = logNowMs;
                bridgeLog(QStringLiteral("feedAudioToDecoder: expected empty RX audio during TX/suspend: %1 slot=%2 forced=%3 tx=%4 tune=%5")
                              .arg(emptyAudioReason)
                              .arg(completedUtcSlot)
                              .arg(usedForcedAudioSnapshot ? 1 : 0)
                              .arg(m_transmitting ? 1 : 0)
                              .arg(m_tuning ? 1 : 0));
            }
        } else {
            bridgeLog("feedAudioToDecoder: " + emptyAudioReason);
            emit statusMessage(emptyAudioReason);
        }
        if (modeSnapshot == QStringLiteral("FT8")) {
            resetEarlyDecodeSchedule();
        }
        if (!expectedTxGap
            && m_monitoring
            && (m_soundInput || m_tciAudioCaptureActive || usingTciAudioInput())
            && !m_transmitting
            && !m_tuning
            && !m_rxAudioSuspendedForTx
            && QDateTime::currentMSecsSinceEpoch() >= m_audioWatchdogIgnoreUntilMs) {
            restartAudioCaptureFromWatchdog(emptyAudioReason);
        }
        return;
    }

    int const minSamples = minimumDecodeSamplesForMode(modeSnapshot);
    if (minSamples > 0 && audioSnapshot.size() < minSamples) {
        bridgeLog("feedAudioToDecoder: skip short " + modeSnapshot +
                  " window samples=" + QString::number(audioSnapshot.size()) +
                  " min=" + QString::number(minSamples));
        if (modeSnapshot == QStringLiteral("FT8")) {
            resetEarlyDecodeSchedule();
        }
        return;
    }

    // On a host that is already showing startup pressure, let the first GPU
    // scene-graph setup settle before Stage4 gets a complete FT8 slot. Audio
    // capture continues and only this one completed slot is discarded. Fast
    // hosts keep the normal first-slot decode path unchanged.
    if (modeSnapshot == QStringLiteral("FT8")
        && !m_ft8StartupPressureSlotDeferred
        && m_monitoring
        && m_monitoringSince.isValid()) {
        qint64 const monitoringAgeMs =
            m_monitoringSince.msecsTo(QDateTime::currentDateTimeUtc());
        int const startupThreadBudget = effectiveFtThreadLimitForDecode();
        bool const startupPressureDetected =
            m_lowCpuModeEnabled
            || cpuPressureActive()
            || startupThreadBudget <= 2;
        bool const activeExchange =
            m_transmitting
            || m_tuning
            || m_txEnabled
            || m_autoCqRepeat
            || legacyDecodeQsoProgress() > 0;
        if (monitoringAgeMs >= 0
            && monitoringAgeMs < 60000
            && startupPressureDetected
            && !activeExchange) {
            m_ft8StartupPressureSlotDeferred = true;
            resetEarlyDecodeSchedule();
            qInfo().noquote()
                << QStringLiteral("[FT8STARTUP] first final slot deferred for UI/GPU warmup ageMs=%1 threads=%2 lowCpu=%3 pressure=%4 slot=%5 samples=%6")
                       .arg(monitoringAgeMs)
                       .arg(startupThreadBudget)
                       .arg(m_lowCpuModeEnabled ? 1 : 0)
                       .arg(cpuPressureActive() ? 1 : 0)
                       .arg(completedUtcSlot)
                       .arg(audioSnapshot.size());
            return;
        }
    }

    m_decoding = true;
    emit decodingChanged();

    quint64 serial = ++m_decodeSerial;
    int const decodePeriodMs = effectivePeriodMsForMode(modeSnapshot);
    qint64 slotIndexForUtc = completedUtcSlot;
    if (slotIndexForUtc < 0 && decodePeriodMs > 0) {
        slotIndexForUtc = correctedUtcEpochMs() / decodePeriodMs;
    }
    if (modeSnapshot == QStringLiteral("FT8") && slotIndexForUtc >= 0) {
        noteFt8AdaptivePeriod(slotIndexForUtc);
    }

    int nutc = utcTokenForSlotStart(slotIndexForUtc, decodePeriodMs);
    QString utcToken = utcDisplayTokenForSlotStart(slotIndexForUtc, decodePeriodMs);
    if (nutc <= 0) {
        QString const utcFmt = (decodePeriodMs > 0 && decodePeriodMs < 60000)
                                   ? QStringLiteral("hhmmss")
                                   : QStringLiteral("hhmm");
        utcToken = QDateTime::currentDateTimeUtc().toString(utcFmt);
        nutc = utcToken.toInt();
    }
    int const nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    int const nftx = qBound(m_nfa, m_txFrequency, m_nfb);
    int const decodeQsoProgress = legacyDecodeQsoProgress();
    int const cqHint = legacyDecodeCqHint();
    bool const txAudioActive = m_transmitting || m_tuning;
    bool const txStartPending = !txAudioActive && (m_txEnabled || m_autoCqRepeat);
    bool const signoffWatchActive = signoffDecodeWatchActive();

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    m_decodeStartMsBySerial.insert(serial, nowMs);
    m_decodeModeBySerial.insert(serial, modeSnapshot);
    m_decodeUtcTokenBySerial.insert(serial, utcToken);
    m_decodeSessionBySerial.insert(serial, m_decodeSessionId);

    for (auto it = m_decodeStartMsBySerial.begin(); it != m_decodeStartMsBySerial.end(); ) {
        if (nowMs - it.value() > 60000) {
            m_decodeModeBySerial.remove(it.key());
            m_decodeUtcTokenBySerial.remove(it.key());
            m_decodeSessionBySerial.remove(it.key());
            m_ft8EarlyDecodeSerials.remove(it.key());
            m_ft8DeepInTxSerials.remove(it.key());  // 1.0.299: anti-leak se il deep e' stato cancellato
            m_ft8PendingDeepFollowups.remove(it.key());
            m_ft8PendingSubpassHarvest.remove(it.key());
            it = m_decodeStartMsBySerial.erase(it);
        } else {
            ++it;
        }
    }

    bridgeLog("feedAudioToDecoder: mode=" + modeSnapshot +
              " samples=" + QString::number(audioSnapshot.size()) +
              (usedForcedAudioSnapshot ? " forced=1" : QString()) +
              (carriedSamples > 0 ? " carried=" + QString::number(carriedSamples) : QString()) +
              " nutc=" + QString::number(nutc) +
              " slot=" + QString::number(slotIndexForUtc) +
              " nfqso=" + QString::number(nfqso) +
              " nftx=" + QString::number(nftx) +
              " depth=" + QString::number(decodeDepth) +
              " qso=" + QString::number(decodeQsoProgress) +
              " cqhint=" + QString::number(cqHint) +
              " ft8ap=" + QString::number(m_ft8ApEnabled ? 1 : 0));

    bool ft8RuntimeBacklog = false;
    QString ft8BacklogDetail;
    if (modeSnapshot == QStringLiteral("FT8")) {
        ft8RuntimeBacklog = ft8LiveDecodeBacklogActive(nowMs, 12000, serial, &ft8BacklogDetail);
        if (ft8RuntimeBacklog) {
            drainFt8LiveDecodeBacklog(serial, QStringLiteral("final-dispatch %1").arg(ft8BacklogDetail));
        }
    }

    if (modeSnapshot == "FT8") {
        m_ft8AutoTxFinalDecodeSerial = serial;
        if (m_ft8Worker) {
            m_ft8Worker->markLatestDecodeSerial(serial);
            m_ft8Worker->cancelCurrentDecode();
            bridgeLog(QStringLiteral("FT8 final decode: preempting in-flight early/deep decode before live final serial=%1")
                          .arg(serial));
        }
        int const fastDepth = qMin(decodeDepth, 2);
        // 1.0.299 — opt-in: il deep follow-up depth-4 gira anche durante l'operazione TX
        // (txStartPending) per recuperare le stazioni di terzi che il fast depth-2 perde.
        // SOLO come decode-list (il serial e' marcato list-only -> niente auto-seq, niente
        // doppio advanceQsoState/checkAndStartPeriodicTx). Mai sotto txAudioActive reale
        // (li' il clamp di queueFt8DecodeRequest forza depth 2 e l'audio TX gira).
        bool const deepFollowupInTx = txStartPending && !txAudioActive && m_ft8DeepDecodeInTx;
        bool const explicitDeepFollowup = m_deepSearchEnabled || m_avgDecodeEnabled;
        // P0 (1.0.403): guardie deep follow-up. Sui PC veloci resta il path
        // fast + deep/AP. Sui PC sotto soglia thread, pero', fast-only depth-2
        // lasciava l'FT8 quasi cieco: in quel caso usiamo un solo pass finale
        // pieno, cappato, invece di due pass accodati.
        static constexpr int kFt8DeepFollowupMinThreads = 3;
        static constexpr int kFt8ConservativeFullPassSafetyMs = 250;
        static constexpr int kFt8ConservativeFullPassMaxMs = 6500;
        static constexpr int kFt8ConservativeFullPassMinMs = 2400;
        bool const deepCooldownActive = m_ft8DeepFollowupCooldownSlots > 0;
        if (m_ft8DeepFollowupCooldownSlots > 0) {
            --m_ft8DeepFollowupCooldownSlots;
        }
        int const ftThreadLimit = effectiveFtThreadLimit();
        int const interactiveThreadLimit = adaptiveInteractiveFtThreadCount(ftThreadLimit);
        bool const deepThreadsOk = ftThreadLimit >= kFt8DeepFollowupMinThreads;
        qint64 deepFollowupLatestCompleteMs = 0;
        if (modeSnapshot == QStringLiteral("FT8") && decodePeriodMs > 0 && slotIndexForUtc >= 0) {
            static constexpr int kFt8DeepLatestOverrunMs = 6800;
            deepFollowupLatestCompleteMs =
                (slotIndexForUtc + 1) * static_cast<qint64>(decodePeriodMs)
                + kFt8DeepLatestOverrunMs;
        }
        qint64 const ft8DispatchNowMs = correctedUtcEpochMs();
        bool const ft8StartupDecodeGuard =
            QDateTime::currentMSecsSinceEpoch() < m_ft8StartupEarlyDecodeGuardUntilMs;
        bool const ft8CpuPressure = cpuPressureActive();
        bool const runDeepFollowup =
            !txAudioActive
            && (!txStartPending || deepFollowupInTx)
            && !signoffWatchActive
            && !m_lowCpuModeEnabled
            && !ft8StartupDecodeGuard
            && !ft8CpuPressure
            && !ft8RuntimeBacklog
            && deepThreadsOk
            && !deepCooldownActive
            && explicitDeepFollowup
            && deepFollowupLatestCompleteMs > ft8DispatchNowMs;
        int const conservativeFullPassBudgetMs =
            qBound(0,
                   static_cast<int>(deepFollowupLatestCompleteMs
                                    - ft8DispatchNowMs
                                    - kFt8ConservativeFullPassSafetyMs),
                   kFt8ConservativeFullPassMaxMs);
        bool const runConservativeFullPass =
            !runDeepFollowup
            && !signoffWatchActive
            && explicitDeepFollowup
            && !deepThreadsOk
            && !txAudioActive
            && !txStartPending
            && !ft8StartupDecodeGuard
            && !ft8CpuPressure
            && !ft8RuntimeBacklog
            && deepFollowupLatestCompleteMs > ft8DispatchNowMs
            && conservativeFullPassBudgetMs >= kFt8ConservativeFullPassMinMs;
        int const signoffFullPassBudgetMs =
            qBound(0,
                   static_cast<int>(deepFollowupLatestCompleteMs
                                    - ft8DispatchNowMs
                                    - kFt8ConservativeFullPassSafetyMs),
                   10500);
        bool const runSignoffFullPass =
            signoffWatchActive
            && !txAudioActive
            && !m_lowCpuModeEnabled
            && !ft8CpuPressure
            && !ft8RuntimeBacklog
            && deepFollowupLatestCompleteMs > ft8DispatchNowMs
            && signoffFullPassBudgetMs >= kFt8ConservativeFullPassMinMs;
        qInfo().noquote()
            << QStringLiteral("[FT8DISPATCH] serial=%1 effectiveDepth=%2 baseDepth=%3 deepSearch=%4 avg=%5 ft8ap=%6 txPending=%7 txAudio=%8 deepInTx=%9 runFollowup=%10 signoffWatch=%11 latestMs=%12 nowMs=%13 threads=%14/%15 cores=%16")
                   .arg(serial)
                   .arg(decodeDepth)
                   .arg(m_ndepth)
                   .arg(m_deepSearchEnabled ? 1 : 0)
                   .arg(m_avgDecodeEnabled ? 1 : 0)
                   .arg(m_ft8ApEnabled ? 1 : 0)
                   .arg(txStartPending ? 1 : 0)
                   .arg(txAudioActive ? 1 : 0)
                   .arg(m_ft8DeepDecodeInTx ? 1 : 0)
                   .arg(runDeepFollowup ? 1 : 0)
                   .arg(signoffWatchActive ? 1 : 0)
                   .arg(deepFollowupLatestCompleteMs)
                   .arg(ft8DispatchNowMs)
                   .arg(interactiveThreadLimit)
                   .arg(ftThreadLimit)
                   .arg(qMax(1, QThread::idealThreadCount()));
        if (runSignoffFullPass) {
            int const signoffDepth = qMax(decodeDepth, 4);
            bridgeLog("FT8 final signoff-watch full pass: serial=" + QString::number(serial) +
                      " depth=" + QString::number(signoffDepth) +
                      " ft8ap=1" +
                      " threads=" + QString::number(interactiveThreadLimit) +
                      " maxMs=" + QString::number(signoffFullPassBudgetMs) +
                      " deepLatestMs=" + QString::number(deepFollowupLatestCompleteMs));
            queueFt8DecodeRequest(audioSnapshot, serial, nutc, slotIndexForUtc, signoffDepth,
                                  decodeQsoProgress, cqHint, 50, true, false,
                                  false, signoffFullPassBudgetMs, false, interactiveThreadLimit);
        } else if (runConservativeFullPass) {
            bridgeLog("FT8 final conservative full pass: serial=" + QString::number(serial) +
                      " depth=" + QString::number(decodeDepth) +
                      " ft8ap=" + QString::number(m_ft8ApEnabled ? 1 : 0) +
                      " threads=" + QString::number(interactiveThreadLimit) +
                      " maxMs=" + QString::number(conservativeFullPassBudgetMs) +
                      " deepLatestMs=" + QString::number(deepFollowupLatestCompleteMs));
            queueFt8DecodeRequest(audioSnapshot, serial, nutc, slotIndexForUtc, decodeDepth,
                                  decodeQsoProgress, cqHint, 50, m_ft8ApEnabled, false,
                                  false, conservativeFullPassBudgetMs, false, interactiveThreadLimit);
        } else {
            bridgeLog("FT8 final fast pass: serial=" + QString::number(serial) +
                      " depth=" + QString::number(fastDepth) +
                      " ft8ap=0" +
                      (runDeepFollowup ? " followup=1" : " followup=0") +
                      (txStartPending ? " txPending=1" : QString()) +
                      " deepLatestMs=" + QString::number(deepFollowupLatestCompleteMs));
            queueFt8DecodeRequest(audioSnapshot, serial, nutc, slotIndexForUtc, fastDepth,
                                  decodeQsoProgress, cqHint, 50, false, false,
                                  false, 0, false, interactiveThreadLimit);
        }

        if (runDeepFollowup) {
            Ft8PendingDeepFollowup pending;
            pending.audio = audioSnapshot;
            pending.nutc = nutc;
            pending.slotIndex = slotIndexForUtc;
            pending.decodeDepth = decodeDepth;
            pending.decodeQsoProgress = decodeQsoProgress;
            pending.cqHint = cqHint;
            pending.ft8ApEnabled = m_ft8ApEnabled;
            pending.inTx = deepFollowupInTx;
            pending.sessionId = m_decodeSessionId;
            pending.utcToken = utcToken;
            pending.latestCompleteMs = deepFollowupLatestCompleteMs;
            m_ft8PendingDeepFollowups.insert(serial, std::move(pending));
            bridgeLog("FT8 final deep followup queued after fast serial=" +
                      QString::number(serial) +
                      " depth=" + QString::number(decodeDepth) +
                      " ft8ap=" + QString::number(m_ft8ApEnabled ? 1 : 0) +
                      " latestMs=" + QString::number(deepFollowupLatestCompleteMs) +
                      (deepFollowupInTx ? " inTx=1" : QString()));
        } else if (explicitDeepFollowup && !txAudioActive && !runConservativeFullPass) {
            bridgeLog("FT8 final deep followup skipped: latestMs=" +
                      QString::number(deepFollowupLatestCompleteMs) +
                      " txPending=" + QString::number(txStartPending ? 1 : 0) +
                      " startupGuard=" + QString::number(ft8StartupDecodeGuard ? 1 : 0) +
                      " cpuPressure=" + QString::number(ft8CpuPressure ? 1 : 0) +
                      " backlog=" + QString::number(ft8RuntimeBacklog ? 1 : 0) +
                      " threads=" + QString::number(ftThreadLimit) +
                      " threadsOk=" + QString::number(deepThreadsOk ? 1 : 0) +
                      " cooldownActive=" + QString::number(deepCooldownActive ? 1 : 0) +
                      " conservativeBudgetMs=" + QString::number(conservativeFullPassBudgetMs));
        }

    } else if (modeSnapshot == "FT2") {
        // FIX C (1.0.354): in FT2-async il decode sync di fine-slot — che la
        // 1.0.353 SOPPRIMEVA del tutto in async — qui viene RIPRISTINATO, ma a
        // profondita' piena FT2 (depth-4: la profondita' FT2 e' strutturalmente
        // clampata a 4 da effectiveDecodeDepth -> qBound(1,m_ndepth,4); il "20"
        // che a volte compare nei log e' in realta' 4|16, cioe' depth-4 con il
        // bit di weak-averaging, NON depth-20). L'async (depth-4 incrementale,
        // ogni 100ms) da' la reattivita'; questa passata sync gira a depth-4 +
        // weak-averaging (|16) sul buffer COMPLETO di slot e resta l'unica che
        // recupera le stazioni deboli/marginali che l'async parziale perde.
        // Modo FT2 NON-async invariato.
        // 1.0.355 — STEP 2 (opt-in, default OFF): se l'async ha GIA' prodotto
        // un decode accettato per QUESTO slot (m_ft2AsyncDecodeProducedSlotMs ==
        // slotIndexForUtc, entrambi indici corrected-UTC), salta la passata sync
        // di fine-slot: gira sullo STESSO worker/runtime_mutex dell'async e la
        // contesa (~1.8s post-TX) ritarda l'aggancio della risposta. La teniamo
        // SOLO quando l'async e' tornato vuoto per lo slot (recupero weak), come
        // faceva la 1.0.353. Log throttled 10s.
        if (m_asyncTxEnabled && m_ft2AsyncSkipRedundantSyncDecode
            && slotIndexForUtc >= 0
            && m_ft2AsyncDecodeProducedSlotMs == slotIndexForUtc) {
            static qint64 s_lastSkipLogMs = 0;
            if (nowMs - s_lastSkipLogMs >= 10000) {
                s_lastSkipLogMs = nowMs;
                bridgeLog(QStringLiteral("FT2 async: skip redundant end-slot sync decode (slot=%1 already covered by async)")
                              .arg(slotIndexForUtc));
            }
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        decodium::ft2::DecodeRequest req;
        req.serial = serial; req.audio = audioSnapshot;
        req.nutc = nutc; req.nqsoprogress = decodeQsoProgress;
        req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        // 1.0.289 — #1: con ft2FullDecodeInAutoCq ON, NON degradare la profondità durante
        // l'attesa AutoCQ (solo sotto CPU pressure) → OSD + 4ª passata + weak-averaging
        // restano attivi proprio mentre cerchi un risponditore debole. Default OFF = 1.0.288.
        bool const reduceForTx =
            txStartPending
            && !signoffWatchActive
            && (!m_ft2FullDecodeInAutoCq || cpuPressureActive());
        int ft2SyncDepth = reduceForTx ? qMin(decodeDepth, 2) : decodeDepth;
        // FIX C (1.0.354): in async la passata sync (che la 1.0.353 sopprimeva)
        // gira alla piena profondita' FT2 (clamp a 4: e' il massimo FT2). Il
        // weak-averaging |16 qui sotto resta attivo (req.ndepth>=4) -> recupero
        // weak-signal sul buffer completo di slot.
        if (m_asyncTxEnabled) ft2SyncDepth = qMin(ft2SyncDepth, 4);
        req.ndepth = ft2SyncDepth;
        if (!txAudioActive && !reduceForTx && req.ndepth >= 4) {
            req.ndepth |= 16;  // Enable FT2 weak-signal bitmetric averaging at deep decode.
        }
        req.threadCount = effectiveFtThreadLimit();
        req.ncontest = m_ncontest;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        // Sprint3-A: AP cache anche sul pass sync weak-recovery (prima solo async).
        if (m_ft2ApHashCache)
            req.apHashCache = m_hashedCallsignCache.snapshotValid(nowMs);
        // 1.0.291 — log diagnostico THROTTLED (1 riga su cambio-stato o ogni 10s):
        // chiarisce PERCHÉ il sync decode si riduce — flag #1 in memoria, pressione CPU,
        // txPending — e azzera lo spam "depth reduced" che annegava il diagnostic log.
        {
            static qint64 s_lastDepthLogMs = 0;
            static int s_lastReduced = -1;
            int const reducedNow = reduceForTx ? 1 : 0;
            if (reducedNow != s_lastReduced || nowMs - s_lastDepthLogMs >= 10000) {
                s_lastReduced = reducedNow;
                s_lastDepthLogMs = nowMs;
                bridgeLog(QStringLiteral("FT2 sync depth: %1->%2 reduce=%3 fullDecodeFlag=%4 cpuPressure=%5 txPending=%6")
                              .arg(decodeDepth).arg(req.ndepth)
                              .arg(reducedNow)
                              .arg(m_ft2FullDecodeInAutoCq ? 1 : 0)
                              .arg(cpuPressureActive() ? 1 : 0)
                              .arg(txStartPending ? 1 : 0)
                              + (signoffWatchActive ? QStringLiteral(" signoffWatch=1") : QString()));
            }
        }
        // markLatestDecodeSerial() DEVE essere chiamato prima di decode(),
        // altrimenti il controllo seriale in FT2DecodeWorker::decode() scarta sempre la richiesta.
        auto* worker = m_ft2Worker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        worker->markLatestDecodeSerial(serial);
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "FT4") {
        QString ft4BacklogDetail;
        bool const ft4RuntimeBacklog = ft4LiveDecodeBacklogActive(nowMs, 6500, serial, &ft4BacklogDetail);
        if (ft4RuntimeBacklog) {
            drainFt4LiveDecodeBacklog(serial, QStringLiteral("final-dispatch %1").arg(ft4BacklogDetail));
        }
        QString ft4LatencyGuardDetail;
        bool const ft4LatencyGuardPending =
            ft4LatencyGuardActive(nowMs)
            && ft4LiveDecodeBacklogActive(nowMs, 1000, serial, &ft4LatencyGuardDetail);
        if (ft4LatencyGuardPending) {
            m_decodeStartMsBySerial.remove(serial);
            m_decodeModeBySerial.remove(serial);
            m_decodeUtcTokenBySerial.remove(serial);
            m_decodeSessionBySerial.remove(serial);
            m_decoding = false;
            emit decodingChanged();
            bridgeLog(QStringLiteral("FT4 final decode skipped: latency guard pending early decode %1")
                          .arg(ft4LatencyGuardDetail));
            return;
        }

        decodium::ft4::DecodeRequest req;
        req.serial = serial; req.audio = audioSnapshot;
        req.nutc = nutc; req.nqsoprogress = decodeQsoProgress;
        req.nfqso = nfqso;
        req.nfa = m_nfa; req.nfb = m_nfb;
        int const requestedFt4Depth =
            signoffWatchActive && !m_lowCpuModeEnabled && !cpuPressureActive()
                ? qMax(decodeDepth, 4)
                : (txStartPending ? qMin(decodeDepth, 2) : decodeDepth);
        req.ndepth = effectiveFt4DecodeDepth(requestedFt4Depth);
        req.threadCount = effectiveFt4ThreadLimit();
        req.ncontest = m_ncontest;
        req.lapcqonly = cqHint;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        if (!txAudioActive && req.ndepth >= 3) {
            m_deepDecodeVisualSerial = serial;
            m_deepDecodeVisualThrottleUntilMs = qMax(
                m_deepDecodeVisualThrottleUntilMs,
                QDateTime::currentMSecsSinceEpoch() + qint64 {7000});
        }
        if (txStartPending && !signoffWatchActive && req.ndepth != decodeDepth) {
            bridgeLog(QStringLiteral("FT4 decode depth reduced before TX: %1 -> %2")
                          .arg(decodeDepth)
                          .arg(req.ndepth));
        } else if (signoffWatchActive) {
            bridgeLog(QStringLiteral("FT4 signoff-watch decode depth: requested=%1 effective=%2 txPending=%3")
                          .arg(requestedFt4Depth)
                          .arg(req.ndepth)
                          .arg(txStartPending ? 1 : 0));
        }
        if (ft4AdaptiveCpuLimitActive(nowMs) || ft4RuntimeBacklog || req.ndepth != requestedFt4Depth
            || req.threadCount != effectiveFtThreadLimit()) {
            bridgeLog(QStringLiteral("FT4 final adaptive dispatch: serial=%1 depth=%2 requestedDepth=%3 threads=%4 baseThreads=%5 backlog=%6 adaptive=%7")
                          .arg(serial)
                          .arg(req.ndepth)
                          .arg(requestedFt4Depth)
                          .arg(req.threadCount)
                          .arg(effectiveFtThreadLimit())
                          .arg(ft4RuntimeBacklog ? 1 : 0)
                          .arg(ft4AdaptiveCpuLimitActive(nowMs) ? 1 : 0));
        }
        auto* worker = m_ft4Worker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        worker->markLatestDecodeSerial(serial);
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "Q65" || modeSnapshot.startsWith(QStringLiteral("Q65-"))) {
        decodium::q65::DecodeRequest req;
        req.serial  = serial; req.audio = audioSnapshot;
        req.nutc    = nutc;   req.nfqso = nfqso;
        req.nfa     = m_nfa;  req.nfb   = m_nfb;
        req.ntol    = 20;
        req.ntrperiod = qBound(15, periodMsForMode(modeSnapshot) / 1000, 300);
        if (modeSnapshot.startsWith(QStringLiteral("Q65-"))) {
            int pos = 4;
            while (pos < modeSnapshot.size() && modeSnapshot.at(pos).isDigit()) {
                ++pos;
            }
            if (pos < modeSnapshot.size()) {
                QChar const submode = modeSnapshot.at(pos).toUpper();
                if (submode >= QLatin1Char('A') && submode <= QLatin1Char('E')) {
                    req.nsubmode = submode.unicode() - QLatin1Char('A').unicode();
                }
            }
        }
        // Q65 uses the low two bits as a depth class.  Global FT depth 4
        // therefore becomes zero in Q65 unless it is mapped back to Q65's
        // valid 1..3 range.
        int const q65DecodeDepth = qBound(1, decodeDepth, 3);
        bool const q65CoherentAverage = m_coherentAvgEnabled;
        req.ndepth = q65DecodeDepth | (q65CoherentAverage ? 16 : 0);
        req.coherentAvgEnabled = q65CoherentAverage;
        req.nclearave = q65CoherentAverage ? 0 : 1;
        if (!q65CoherentAverage) {
            notifyCoherentAveraging(0);
        }
        req.ncontest = m_ncontest;
        req.nqsoprogress = decodeQsoProgress;
        req.hisgrid = m_dxGrid.toLocal8Bit();
        req.emedelay = (req.ntrperiod == 60) ? 2.5f : 0.0f;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.zapEnabled = m_zapEnabled;
        auto* worker = m_q65Worker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        bridgeLog(QStringLiteral("[Q65DISPATCH] serial=%1 mode=%2 period=%3 submode=%4 samples=%5 target=%6 depth=%7 coherent=%8 clearAverage=%9")
                      .arg(serial)
                      .arg(modeSnapshot)
                      .arg(req.ntrperiod)
                      .arg(req.nsubmode)
                      .arg(audioSnapshot.size())
                      .arg(targetDecodeSamplesForMode(modeSnapshot))
                      .arg(req.ndepth)
                      .arg(req.coherentAvgEnabled ? 1 : 0)
                      .arg(req.nclearave));
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "MSK144") {
        decodium::msk144::DecodeRequest req;
        req.serial  = serial; req.audio = audioSnapshot;
        req.nutc    = nutc;
        req.kdone   = audioSnapshot.size();
        req.maxlines = 8;
        req.rxfreq  = m_rxFrequency;
        req.ftol    = 50;
        req.aggressive = qBound(1, decodeDepth & 7, 3);
        req.trperiod = effectivePeriodMsForMode(modeSnapshot) / 1000.0;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        auto* worker = m_mskWorker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "WSPR") {
        auto* worker = m_wsprWorker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        QString const wavPath = buildWsprDecodeWavPath(serial, utcToken);
        if (!writeMono16WavFile(wavPath, audioSnapshot, SAMPLE_RATE)) {
            bridgeLog(QStringLiteral("[WSPRDECODE] cannot write decode WAV serial=%1 path=%2 samples=%3")
                          .arg(serial)
                          .arg(wavPath)
                          .arg(audioSnapshot.size()));
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dataDir.isEmpty()) {
            dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        }
        if (dataDir.isEmpty()) {
            dataDir = QFileInfo(wavPath).absolutePath();
        }
        QDir().mkpath(dataDir);

        QStringList depthArgs;
        int const depthKind = decodeDepth & 7;
        if (depthKind <= 1) {
            depthArgs << QStringLiteral("-qB");
        } else {
            depthArgs << QStringLiteral("-C") << QStringLiteral("500")
                      << QStringLiteral("-o") << QStringLiteral("4");
            if (depthKind >= 3) {
                depthArgs << QStringLiteral("-d");
            }
        }
        decodium::wspr::DecodeRequest req;
        req.serial = serial;
        req.arguments = depthArgs
            << QStringLiteral("-a") << QDir::toNativeSeparators(dataDir)
            << QStringLiteral("-f") << QString::number(m_frequency / 1e6, 'f', 6)
            << QDir::toNativeSeparators(wavPath);
        m_wsprDecodeTempFiles.insert(serial, wavPath);
        bridgeLog(QStringLiteral("[WSPRDECODE] serial=%1 samples=%2 utc=%3 wav=%4 args=%5")
                      .arg(serial)
                      .arg(audioSnapshot.size())
                      .arg(utcToken)
                      .arg(wavPath)
                      .arg(req.arguments.join(QLatin1Char(' '))));
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot == "JT65" || modeSnapshot == "JT9" || modeSnapshot == "JT4") {
        decodium::legacyjt::DecodeRequest req;
        req.serial  = serial;
        req.mode    = modeSnapshot;
        req.audio   = audioSnapshot;
        req.nutc    = nutc;
        req.nfqso   = qBound(0, nfqso, 5000);
        req.nfa     = qBound(0, m_nfa, 4950);
        req.nfb     = qBound(req.nfa + 50, m_nfb, 5000);
        req.ndepth  = decodeDepth;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.hisgrid = m_dxGrid.toLocal8Bit();
        req.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit();
        if (modeSnapshot == QStringLiteral("JT9")) {
            // JT9-1 uses a 60 s, 12 kHz window and the decoder consumes one
            // eighth-sized count for its legacy npts8 contract.
            req.npts8 = audioSnapshot.size() / 8;
            req.nzhsym = 184;
            req.ntol = 20;
            req.newdat = 1;
            req.nagain = 0;
            req.nsubmode = 0;
            bridgeLog(QStringLiteral("JT9 dispatch: serial=%1 samples=%2 npts8=%3 nzhsym=%4 nfqso=%5 range=%6-%7 depth=%8")
                          .arg(serial)
                          .arg(audioSnapshot.size())
                          .arg(req.npts8)
                          .arg(req.nzhsym)
                          .arg(req.nfqso)
                          .arg(req.nfa)
                          .arg(req.nfb)
                          .arg(req.ndepth));
        } else if (modeSnapshot == QStringLiteral("JT4")) {
            // JT4A defaults used by the QML frontend. A non-zero tolerance is
            // essential on air; leaving DecodeRequest::ntol at zero restricted
            // acquisition to an exact audio-frequency match.
            req.ntol = 150;
            req.nsubmode = 0;
            req.nclearave = 0;
            req.minsync = 0;
            req.minw = 0;
            req.newdat = 1;
            bridgeLog(QStringLiteral("JT4A dispatch: serial=%1 samples=%2 nfqso=%3 tolerance=%4 depth=%5")
                          .arg(serial)
                          .arg(audioSnapshot.size())
                          .arg(req.nfqso)
                          .arg(req.ntol)
                          .arg(req.ndepth));
        }
        auto* worker = m_legacyJtWorker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else if (modeSnapshot.startsWith("FST4")) {
        decodium::fst4::DecodeRequest req;
        req.serial  = serial;
        req.mode    = modeSnapshot;
        req.audio   = audioSnapshot;
        req.nutc    = nutc;
        req.nfa     = qBound(0, m_nfa, 4950);
        req.nfb     = qBound(req.nfa + 50, m_nfb, 5000);
        req.nfqso   = qBound(0, nfqso, 5000);
        req.ndepth  = decodeDepth;
        req.lapcqonly = cqHint;
        req.mycall  = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit();
        req.ntrperiod = qBound(15, effectivePeriodMsForMode(modeSnapshot) / 1000, 1800);
        req.iwspr = modeSnapshot.startsWith(QStringLiteral("FST4W")) ? 1 : 0;
        if (req.ntol <= 0) {
            req.ntol = req.iwspr ? 250 : 150;
        }
        Ft2LinkPcmStats const windowStats = ft2LinkPcmStats(audioSnapshot);
        bridgeLog(QStringLiteral("FST4 dispatch: serial=%1 samples=%2 tr=%3 iwspr=%4 nfqso=%5 range=%6-%7 depth=%8 tol=%9")
                      .arg(serial)
                      .arg(audioSnapshot.size())
                      .arg(req.ntrperiod)
                      .arg(req.iwspr)
                      .arg(req.nfqso)
                      .arg(req.nfa)
                      .arg(req.nfb)
                      .arg(req.ndepth)
                      .arg(req.ntol));
        bridgeLog(QStringLiteral("[FST4RX] serial=%1 mode=%2 rms=%3 peak=%4 nonzero=%5 aboveNoise=%6")
                      .arg(serial)
                      .arg(modeSnapshot)
                      .arg(windowStats.rms, 0, 'f', 5)
                      .arg(windowStats.peak, 0, 'f', 5)
                      .arg(windowStats.nonZero)
                      .arg(windowStats.aboveNoise));
        auto* worker = m_fst4Worker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);

    } else {
        // 1.0.451 iu8lmc fix (freeze FT2-Link): il modo applicativo FT2-Link ha
        // il proprio decode point-to-point (ingestRxSamples: narrow/wide). Il
        // fallback FT8 deep+AP qui girava su audio non-FT8, impiegava 4-14s
        // trovando 0 righe e affamava il main thread -> freeze. FT2-Link non
        // deve usare il decoder FTx: esci senza decodificare (buffer gia' svuotato).
        if (isFt2LinkApplicationMode(modeSnapshot)) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        // Fallback a FT8
        decodium::ft8::DecodeRequest req;
        req.serial = serial; req.audio = audioSnapshot;
        req.nutc = nutc; req.nqsoprogress = decodeQsoProgress;
        req.nfqso = nfqso; req.nftx = nftx;
        req.nfa = m_nfa; req.nfb = m_nfb;
        req.nzhsym = 50;
        req.ndepth = decodeDepth;
        req.threadCount = effectiveFtThreadLimit();
        req.emedelay = 0.0f;
        req.ncontest = m_ncontest;
        req.nagain = 0;
        req.lft8apon = m_ft8ApEnabled ? 1 : 0;
        req.apMyCallEnabled = decodium::ft8::allowMyCallAp(
            false, m_transmitting && !m_tuning && m_mode == QStringLiteral("FT8"),
            m_lastFt8MessageTxMs, QDateTime::currentMSecsSinceEpoch());
        req.lmultift8 = 1;
        req.lapcqonly = cqHint;
        if (m_frequency < 30000000.0) {
            req.napwid = 5;
        } else if (m_frequency < 100000000.0) {
            req.napwid = 15;
        } else {
            req.napwid = 50;
        }
        req.ldiskdat = 0;
        req.mycall = m_callsign.toLocal8Bit();
        req.hiscall = m_dxCall.toLocal8Bit();
        req.hisgrid = m_dxGrid.toLocal8Bit();
        auto* worker = m_ft8Worker;
        if (!worker) {
            m_decoding = false;
            emit decodingChanged();
            return;
        }
        worker->markLatestDecodeSerial(serial);
        QMetaObject::invokeMethod(worker, [worker, req]() {
            worker->decode(req);
        }, Qt::QueuedConnection);
    }

    // m_audioBuffer è già stato clear()+reserve() sotto lock all'inizio della
    // funzione: i nuovi campioni del prossimo periodo si stanno già accumulando.
}

