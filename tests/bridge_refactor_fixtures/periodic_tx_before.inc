void DecodiumBridge::checkAndStartPeriodicTx()
{
    // 1.0.256 fix BUG #1 audit agente-tx: guard reentry UNIVERSALE (RAII).
    // Pre-1.0.256 il guard era solo su 2 dei 11 call site, e quelli senza
    // (setTxEnabled QTimer singleShot, AutoCQ rearm, advanceQsoState post
    // double-click, deferred manual sync, smart Ft2 async retry, decode-
    // driven response, period UTC boundary, engageDxClusterSpot) potevano
    // far entrare 2 chiamate in startTx() prima che m_transmitting=true ->
    // doppia TX FT2 "singhiozzo".
    if (m_periodicTxInFlight) return;
    // The periodic FT/JT scheduler has no role in continuous/manual modes.
    // In particular, a persisted TX-enabled/AutoCQ state must not key a rig
    // merely because Decodium was restarted in CW.
    if (m_mode == QStringLiteral("CW")
        || m_mode == QStringLiteral("CW-R")
        || m_mode == QStringLiteral("JTTY")
        || m_mode == QStringLiteral("SSB")) {
        return;
    }
    // FT2-Link non usa il sequencer TX FTx standard (TX solo dal pannello).
    if (isFt2LinkApplicationMode(m_mode)) return;
    m_periodicTxInFlight = true;
    auto inFlightReset = qScopeGuard([this]() { m_periodicTxInFlight = false; });

    if (m_manualTxHold || !m_monitoring || m_transmitting || m_tuning) return;
    // 1.0.732: a CQ left in TX6 after AutoCQ was stopped is not retried every period
    if (m_currentTx == 6 && !m_autoCqRepeat && !m_multiAnswerMode && !m_txEnabled
        && m_explicitManualTxRequest == 0) {
        return;
    }
    // Telemetria di stazione rimandata dopo un QSO: prende questo slot, il CQ
    // o il messaggio successivo partono dal prossimo.
    if (m_stationTelemetryPending && !shouldDeferManualSyncTxStart()) {
        sendStationTelemetry();
        return;
    }
    // Common TX guard: every automatic/deferred scheduler reaches this
    // chokepoint before startTx().  In particular, a stale callback must not
    // turn a post-QSO currentTx=6 into a new CQ after AutoCQ was disabled.
    if (!txDispatchGuardAllows(QStringLiteral("checkAndStartPeriodicTx"))) {
        return;
    }
    if (deferFt8AutoTxForDecode()) return;
    // 1.0.364+ MAM multi-stream nativo (FASE 2): se il sequencer multi-QSO
    // e' attivo gestisce TUTTO il dispatch del periodo e ritorna. Gated da
    // mamMultiStreamSequencerActive() -> con MAM OFF questo if non scatta mai
    // e il path single-QSO sottostante resta byte-identico.
    if (mamMultiStreamSequencerActive()) {
        bool const elasticGo = m_foxElasticGo;
        m_foxElasticGo = false;
        mamDispatchPeriod(elasticGo);
        return;
    }
    // 1.0.446 - P1-4 (audit hardening, DIAGNOSTICO solo-logging, nessun blocco): rileva stati
    // (progress,currentTx) incoerenti al chokepoint TX. Logga solo l'anomalia (rara se la
    // macchina a stati e' sana). Eccezione QuickQSO (currentTx=3,progress=5) whitelistata.
    // Misura se il refactor canAdvance serve davvero prima di un eventuale enforcement.
    {
        bool const stateCoherent =
            (m_currentTx == 6 && m_qsoProgress == 1) ||
            (m_currentTx == 1 && m_qsoProgress == 2) ||
            (m_currentTx == 2 && m_qsoProgress == 3) ||
            (m_currentTx == 3 && (m_qsoProgress == 4 || (m_quickQsoEnabled && m_qsoProgress == 5))) ||
            (m_currentTx == 4 && m_qsoProgress == 5) ||
            (m_currentTx == 5 && m_qsoProgress == 5);
        if (!stateCoherent && m_qsoProgress >= 1 && m_qsoProgress <= 5) {
            bridgeLog(QStringLiteral("[STATE] incoerenza QSO @checkAndStartPeriodicTx: currentTx=%1 progress=%2 quick=%3")
                          .arg(m_currentTx).arg(m_qsoProgress).arg(m_quickQsoEnabled ? 1 : 0));
        }
    }
    // Sprint2-2: AutoCQ col RX morto = chiamare nel vuoto fino al TX watchdog
    // (caso reale 9 giu: 10+ min di CQ con rms=0). Se il watchdog audio segnala
    // RX unhealthy da >=15s, sospendi il REARM del CQ (solo TX6 in fase pura,
    // mai un QSO in corso ne' il TX manuale); riprende da solo al recovery.
    if (m_autoCqRxHealthGate && m_autoCqRepeat && m_currentTx == 6
        && m_qsoProgress <= 1
        && m_audioUnhealthyStartMs > 0) {
        qint64 const rxGateNowMs = QDateTime::currentMSecsSinceEpoch();
        qint64 const rxDeadMs = rxGateNowMs - m_audioUnhealthyStartMs;
        if (rxDeadMs >= 15000) {
            if (rxGateNowMs - m_autoCqRxHealthLogMs >= 60000) {
                m_autoCqRxHealthLogMs = rxGateNowMs;
                bridgeLog(QStringLiteral("AutoCQ sospeso: RX audio unhealthy da %1s (gate RX-health, riprende al recovery)")
                              .arg(rxDeadMs / 1000));
            }
            return;
        }
    }
    bool const awaitingFt2PartnerAtEntry = ft2AutoCqAwaitingPartnerDecode();
    if (awaitingFt2PartnerAtEntry && !m_ft2DeferredLogPending) {
        qint64 const waitAgeMs = m_ft2AutoCqAwaitingPartnerSinceMs > 0
            ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_ft2AutoCqAwaitingPartnerSinceMs)
            : 0;
        int const waitPeriodMs = periodMsForMode(m_mode);
        qint64 const waitLimitMs = waitPeriodMs > 0
            ? qMax<qint64>(static_cast<qint64>(waitPeriodMs) * 3, 22500)
            : 22500;
        bool const retryCapAlreadyExceeded =
            m_currentTx == m_lastNtx
            && m_txRetryCount > m_maxCallerRetries;
        // Sprint1 (1.0.393): se il phase-lock breaker ha appena corretto la
        // parita', concedi il tentativo in fase opposta + la risposta prima
        // di rilasciare il partner: in loopback (round 3, 01:59:15) il release
        // a 22.5s scattava 3.8s DOPO il firing del breaker, vanificando la
        // correzione. Grace = fino a defer + 2 periodi.
        bool const parityJustCorrected = m_ft2ParityDeferUntilMs > 0
            && waitPeriodMs > 0
            && correctedUtcEpochMs() < m_ft2ParityDeferUntilMs + 4 * static_cast<qint64>(waitPeriodMs);
        if (!parityJustCorrected
            && (retryCapAlreadyExceeded || waitAgeMs >= waitLimitMs || m_ft2AutoCqAwaitingPartnerSinceMs <= 0)) {
            bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: releasing stale partner wait TX%1 partner=%2 age=%3ms limit=%4ms retry=%5/%6")
                          .arg(m_ft2AutoCqAwaitingPartnerTx)
                          .arg(m_ft2AutoCqAwaitingPartnerBase.isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_ft2AutoCqAwaitingPartnerBase)
                          .arg(waitAgeMs)
                          .arg(waitLimitMs)
                          .arg(m_txRetryCount)
                          .arg(m_maxCallerRetries));
            clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("timeout/retry cap"));
        } else {
            return;
        }
    }
    if (!m_txEnabled) {
        if (awaitingFt2PartnerAtEntry && m_ft2DeferredLogPending) {
            // FT2 RR73 log-only wait: keep TX disarmed, but still allow the
            // deferred-log timeout below to mature.
        } else {
            if (!m_autoCqRepeat) return;
            if (m_currentTx >= 1 && m_currentTx <= 6 && isTxDisabled(m_currentTx)) {
                bridgeLog(QStringLiteral("checkAndStartPeriodicTx: AutoCQ rearm blocked because TX%1 is user-disabled")
                              .arg(m_currentTx));
                return;
            }
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: AutoCQ active with TX disabled -> rearm TX"));
            setTxEnabled(true);
            if (!m_txEnabled) return;
        }
    }
    if (m_autoCqRepeat
        && !m_autoCqLockedCall.trimmed().isEmpty()
        && (m_dxCall.isEmpty() || m_currentTx == 6 || m_qsoProgress <= 1)) {
        restoreAutoCqPartnerLock();
    }

    auto finishAutoSequenceQso = [&](QString const& reason, bool logNow) {
        if (releaseCourtesySignoffIfActive(reason)) {
            return;
        }
        // "log anyway" at a retry cap is the old policy of accepting a QSO whose last
        // acknowledgement never arrived. It is only honest when the exchange really
        // happened: if the partner never sent a report (M0PWL, 26/10) nothing is
        // logged, and nothing goes to QRZ.
        if (logNow && reason.contains(QLatin1String("log anyway")) && !m_reportReceivedReal) {
            bridgeLog(QStringLiteral("QSO NOT logged: retry cap reached but %1 never sent a report (%2)")
                          .arg(inferredPartnerForAutolog(), reason));
            logNow = false;
        }
        bridgeLog(reason);
        QString completedPartner = inferredPartnerForAutolog();
        if (logNow) {
            capturePendingAutoLogSnapshot();
            if (completedPartner.trimmed().isEmpty() && m_pendingAutoLogValid) {
                completedPartner = m_pendingAutoLogCall;
            }
            markWorldMapQsoClosed(completedPartner, reason);
            rememberCompletedAutoCqPartner(completedPartner, true, reason);
            logQso();
        } else {
            markWorldMapQsoClosed(completedPartner, reason);
            rememberCompletedAutoCqPartner(completedPartner, false, reason);
        }
        bool const continueQueuedCaller = (m_multiAnswerMode || m_autoCqRepeat) && !m_callerQueue.isEmpty();
        bool const continueAutoCq = autoCqCanRestartTx6();
        bool const preserveAutoTx = continueQueuedCaller || continueAutoCq;
        resetQsoStateForNextSequence(preserveAutoTx);
        if (applyDeferredSpecialOperationActivityIfReady()) {
            return;
        }
        if (continueQueuedCaller) {
            processNextInQueue();
        } else if (continueAutoCq) {
            if (!advanceQsoState(6)) {
                return;
            }
            if (!m_txEnabled) {
                setTxEnabled(true);
            }
            // 1.0.256 fix BUG #1 FT8/FT4 audit: usa checkAndStartPeriodicTx
            // invece di startTx() diretto. startTx() non valida la parita'
            // slot -> in FT8/FT4 puo' partire TX nello slot partner.
            checkAndStartPeriodicTx();
        }
    };

    auto completeFinishedSignoffIfReady = [&]() -> bool {
        QString const selectedPayload = buildCurrentTxMessage().trimmed();
        QString const activePartner = m_dxCall.trimmed();
        QString const activePartnerBase = normalizedBaseCall(activePartner);
        // TX5 is the local final 73.  Once it has actually been transmitted
        // for the active partner, logging must not depend on the current
        // payload/state still looking like TX5: CAT/status updates can move
        // that state before this check runs and previously lost valid QSOs.
        bool const lastTxWasFinal73ForActivePartner =
            m_lastNtx == 5
            && messageCarries73PayloadForCall(m_lastTransmittedMessage,
                                               activePartner,
                                               activePartnerBase);
        if (lastTxWasFinal73ForActivePartner
            && (m_mode == QStringLiteral("FT2")
                || m_mode == QStringLiteral("FT4")
                || m_mode == QStringLiteral("FT8"))) {
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: local TX5 73 confirmed by last TX payload -> QSO complete"), true);
            return true;
        }
        bool const selectedPayloadMentionsActivePartner =
            activePartnerBase.isEmpty()
            || messageContainsCallToken(selectedPayload, activePartner, activePartnerBase);
        bool const signoffPayload =
            ((m_currentTx == 5) && selectedPayloadMentionsActivePartner)
            || messageCarries73PayloadForCall(selectedPayload, activePartner, activePartnerBase);
        bool const lastTxSignoffForActivePartner =
            messageCarries73PayloadForCall(m_lastTransmittedMessage, activePartner, activePartnerBase);
        bool const completedCurrentSignoff =
            (m_lastNtx == m_currentTx && signoffPayload && lastTxSignoffForActivePartner);
        // Some TX backends can miss the explicit m_nTx73 increment, but
        // m_lastNtx + m_lastTransmittedMessage still prove TX5/RR73 was sent.
        bool const localSignoffAlreadyStarted =
            (m_nTx73 >= 1 || completedCurrentSignoff)
            && (lastTxSignoffForActivePartner || completedCurrentSignoff);
        bool const completedFinalTx5Once =
            m_currentTx == 5
            && (m_nTx73 >= 1
                || completedCurrentSignoff
                || (m_lastNtx == 5 && lastTxSignoffForActivePartner))
            && (m_mode == QStringLiteral("FT2")
                || m_mode == QStringLiteral("FT4")
                || m_mode == QStringLiteral("FT8"));
        bool const deferredRr73LogOnlyWait =
            m_ft2DeferredLogPending
            && m_ft2LogRr73OnPartnerLeft
            && m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && m_autoCqRepeat
            && m_currentTx == 4
            && ft2AutoCqAwaitingPartnerDecode();
        if (!((m_txEnabled || deferredRr73LogOnlyWait || completedFinalTx5Once)
              && signoffPayload
              && localSignoffAlreadyStarted)) {
            // 1.0.493 — visibilità edge-case: con deferred-log armato ma payload 73 non
            // riconosciuto per il partner attivo (free-text, hashed call, compound), il ramo
            // cap signoff sotto non viene MAI raggiunto e TX4/TX5 ripete fino al watchdog.
            if (m_ft2DeferredLogPending && !signoffPayload
                && (m_currentTx == 4 || m_currentTx == 5)) {
                bridgeLog(QStringLiteral("checkAndStartPeriodicTx: signoff cap gate REJECTED payload [%1] per partner %2 (token mismatch) — cap non applicabile su TX%3")
                              .arg(selectedPayload, activePartner)
                              .arg(m_currentTx));
            }
            return false;
        }

        bool const autoCqDeferredSignoffMode =
            m_mode == QStringLiteral("FT2")
            || m_mode == QStringLiteral("FT4")
            || m_mode == QStringLiteral("FT8");
        bool const needsExplicitFinal73Wait =
            autoCqDeferredSignoffMode
            && m_currentTx == 4
            && selectedPayloadMentionsActivePartner
            && !m_logAfterOwn73;
        if (needsExplicitFinal73Wait
            && !m_ft2DeferredLogPending) {
            m_ft2DeferredLogPending = true;
            capturePendingAutoLogSnapshot();
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: deferred TX4 signoff wait for final 73 mode=%1 count=%2")
                          .arg(m_mode)
                          .arg(m_nTx73));
        } else if (m_autoCqRepeat
            && autoCqDeferredSignoffMode
            && !m_logAfterOwn73
            && !m_ft2DeferredLogPending) {
            m_ft2DeferredLogPending = true;
            capturePendingAutoLogSnapshot();
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: repaired deferred signoff wait for AutoCQ TX%1 mode=%2 count=%3")
                          .arg(m_currentTx)
                          .arg(m_mode)
                          .arg(m_nTx73));
        }

        if (m_logAfterOwn73) {
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: completed own 73 → QSO complete"), true);
            return true;
        }

        if (completedFinalTx5Once) {
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: completed final TX5 73 once -> QSO complete"), true);
            return true;
        }

        if (m_ft2DeferredLogPending) {
            // Cap per-mode retry TX5/RR73 deferred by wall-clock intent:
            // FT8=3 (~45s), FT4=4 (~30s), FT2=8 (~30s).
            // 1.0.174 — passa partner SNR e Conservative flag per cap adattivo
            int const signoffCap = deferredSignoffRetryCapForMode(m_mode, m_maxCallerRetries,
                                                                   m_currentPartnerSnrDb, m_ft2Conservative,
                                                                   m_ft2QuickGiveUpStrong, m_ft2SignoffRetryCap,
                                                                   m_ft4SignoffRetryCap, m_ft8SignoffRetryCap, m_ftxWeakSignoffBoost, m_ftxWeakSnrThreshold, m_ftxWeakSignoffBonus);
            // 1.0.446 - P0-2: m_nTx73 sale SOLO a tx-playback-finished pulito; con errore
            // audio persistente (sink park) resta 0 e il cap signoff non avanzerebbe mai ->
            // ripetizione fino al solo watchdog. Contatore effettivo = max(m_nTx73,
            // m_txRetryCount) quando lo step non e' cambiato (anche invii falliti contano).
            // Caso normale (audio ok): m_txRetryCount==m_nTx73 -> identico.
            int const effSignoffCount = (m_currentTx == m_lastNtx)
                ? qMax(m_nTx73, m_txRetryCount) : m_nTx73;
            if (deferredRr73LogOnlyWait && effSignoffCount < signoffCap) {
                qint64 const periodMs = periodMsForMode(m_mode);
                qint64 const waitedMs = m_ft2AutoCqAwaitingPartnerSinceMs > 0
                    ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_ft2AutoCqAwaitingPartnerSinceMs)
                    : 0;
                int const sentCount = qMax(1, m_nTx73);
                int const remainingPeriods = qMax(1, signoffCap - sentCount);
                qint64 const requiredWaitMs = periodMs > 0
                    ? static_cast<qint64>(qMax(2, remainingPeriods)) * periodMs  // 1.0.732: at least 2 periods
                    : 0;
                if (requiredWaitMs > 0 && waitedMs < requiredWaitMs) {
                    return true;
                }
                finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: deferred FT2 RR73 log-only wait cap %1/%2 elapsed=%3ms -> log anyway, retry cap reached")
                                          .arg(m_nTx73)
                                          .arg(signoffCap)
                                          .arg(waitedMs), true);
                return true;
            }
            if (effSignoffCount < signoffCap) {
                bridgeLog(QStringLiteral("checkAndStartPeriodicTx: deferred signoff TX%1 mode=%2 count=%3/%4, waiting for final ack")
                              .arg(m_currentTx)
                              .arg(m_mode)
                              .arg(m_nTx73)
                              .arg(signoffCap));
                return false;
            }
            // 1.0.124: al timeout, logga COMUNQUE il QSO (logNow=true).
            // Il QSO ha completato lo scambio bidirezionale di report; il 73
            // finale del partner e' optional in FT8/FT2. Non loggare era un bug
            // che faceva perdere QSO validi con stazioni forti / DX rari.
            // Mantieni anche uno snapshot per il 73 tardivo: se il primo
            // tentativo di scrittura log non riesce (o resta in attesa della
            // conferma utente), il successivo 73 del partner deve poter
            // ripetere il solo log senza riaprire il QSO o fermare AutoCQ.
            if (m_autoCqRepeat) {
                armLateAutoLogSnapshot();
            }
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: deferred signoff TX%1 mode=%2 retry cap %3/%4 -> log anyway, retry cap reached")
                                      .arg(m_currentTx)
                                      .arg(m_mode)
                                      .arg(effSignoffCount)
                                      .arg(signoffCap), true);
            return true;
        }

        finishAutoSequenceQso("checkAndStartPeriodicTx: completed signoff count=" + QString::number(m_nTx73) +
                              " → QSO completo (TX" + QString::number(m_currentTx) + " sent)", true);
        return true;
    };

    if (completeFinishedSignoffIfReady()) {
        return;
    }

    auto const autoCqHasActivePartner = [&]() {
        return m_autoCqRepeat
            && ((m_qsoProgress > 1 && m_qsoProgress < 6)
                || !m_dxCall.trimmed().isEmpty()
                || !m_autoCqLockedCall.trimmed().isEmpty()
                || m_pendingAutoSeqTxAfterActiveTx > 0);
    };
    auto const autoCqHasPartnerIdentity = [&]() {
        return m_autoCqRepeat
            && (!m_dxCall.trimmed().isEmpty()
                || !m_autoCqLockedCall.trimmed().isEmpty()
                || m_pendingAutoSeqTxAfterActiveTx > 0);
    };

    if (m_currentTx == 6 && autoCqHasPartnerIdentity()) {
        if (!applyDeferredAutoSeqTxForRearm(QStringLiteral("AutoCQ partner-active CQ suppress"))) {
            restoreAutoCqPartnerLock();
        }
        if (m_currentTx == 6) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: suppress AutoCQ CQ while partner active dx=%1 lock=%2 pending=%3")
                          .arg(m_dxCall.trimmed().isEmpty() ? QStringLiteral("<none>") : m_dxCall.trimmed(),
                               m_autoCqLockedCall.trimmed().isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_autoCqLockedCall.trimmed())
                          .arg(m_pendingAutoSeqTxAfterActiveTx));
            return;
        }
    }

    if (ft2AutoCqAwaitingPartnerDecode()) {
        qint64 const waitAgeMs = m_ft2AutoCqAwaitingPartnerSinceMs > 0
            ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_ft2AutoCqAwaitingPartnerSinceMs)
            : 0;
        int const waitPeriodMs = periodMsForMode(m_mode);
        qint64 const waitLimitMs = waitPeriodMs > 0
            ? qMax<qint64>(static_cast<qint64>(waitPeriodMs) * 3, 22500)
            : 22500;
        if (m_ft2AutoCqAwaitingPartnerSinceMs > 0 && waitAgeMs >= waitLimitMs) {
            bridgeLog(QStringLiteral("FT2 AutoCQ one-shot: secondary stale release TX%1 partner=%2 age=%3ms limit=%4ms")
                          .arg(m_ft2AutoCqAwaitingPartnerTx)
                          .arg(m_ft2AutoCqAwaitingPartnerBase.isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_ft2AutoCqAwaitingPartnerBase)
                          .arg(waitAgeMs)
                          .arg(waitLimitMs));
            clearFt2AutoCqAwaitingPartnerDecode(QStringLiteral("secondary timeout"));
        } else {
            return;
        }
    }

    // FT2 async mode:
    // - Per risposte QSO gia' avviate (TX2..TX5): salta solo la parita'
    //   periodo, ma mantiene il guard "late start" separato sotto.
    //   TX1 da doppio-click resta invece period-gated: se lo facciamo
    //   entrare qui, il callback di fine audio FT2 riavvia TX1 a ogni slot
    //   senza lasciare uno slot RX reale.
    // - Per AutoCQ (nessuna stazione DX): usa il controllo di periodo normale
    //   → evita loop CQ continuo (CQ ogni 3.75s senza pausa RX)
    // 1.0.314 — opt-in ftxImmediateClickTx: rilassa TX1 (bypass period-gate)
    // = ripristina comportamento 1.0.283 (commit 06f3e8b di 1.0.300 aveva irrigidito a >=2
    // per fixare una race ora mitigata da ed7ffeb "FT2 manual double-click TX latch").
    bool const ft2ManualClickTx1Bypass =
        m_ft2ManualClickTx1BypassPeriodOnce
        && m_currentTx == 1
        && m_lastNtx != 1
        && !m_autoCqRepeat
        && !m_multiAnswerMode
        && !m_dxCall.trimmed().isEmpty();
    int const ft2MinTxForBypass = (m_ftxImmediateClickTx || ft2ManualClickTx1Bypass) ? 1 : 2;
    bool const ft2AutomatedExchange =
        m_mode == QStringLiteral("FT2")
        && m_asyncTxEnabled
        && (m_autoCqRepeat || m_multiAnswerMode || m_waitPounceActive);
    bool inQsoResponse = (m_mode == "FT2" && m_asyncTxEnabled &&
                          m_txEnabled && !m_dxCall.isEmpty() &&
                          m_currentTx >= ft2MinTxForBypass && m_currentTx <= 5
                          && !ft2AutomatedExchange);
    bool skipPeriodCheck = inQsoResponse;

    if (!skipPeriodCheck) {
        qint64 msNow = correctedUtcMsecsSinceStartOfDay();
        int pMs  = effectivePeriodMsForMode(m_mode);
        if (pMs <= 0) return;
        int pidx = (int)(msNow / (qint64)pMs);
        bool const isEven = (pidx % 2 == 0);
        bool const isOurPeriod = bridgeTxPeriodIsEven(m_txPeriod) ? isEven : !isEven;
        if (!isOurPeriod) return;

        // P0-2 (log ZL 2026-06-11): il phase-lock breaker viveva solo nel ramo
        // TX2-5; TX1 (risposta double-click period-gated) e TX6 (CQ) non
        // valutavano MAI la parita' vs l'anchor del partner (ZL1BW: 11 slot
        // TX1 in collisione, zero valutazioni). Qui il gate e' m_txPeriod:
        // la correzione giusta e' FLIPPARLO, non un defer.
        // P0-3: chi RISPONDE (TX1) cede SEMPRE - il CQer non decodifica nulla
        // durante la collisione, non ha anchor e non puo' mai cedere lui. Il
        // CQer (TX6) cede solo da call minore (caso simmetrico CQ-vs-CQ, dove
        // entrambi decodificano il CQ altrui e hanno l'anchor).
        if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled
            && m_ft2AsyncPartnerSlotMs > 0 && m_ft2AsyncPartnerSlotSetMs > 0
            && m_txRetryCount >= 2
            && (QDateTime::currentMSecsSinceEpoch() - m_ft2AsyncPartnerSlotSetMs) <= 60000) {
            qint64 const pgPartnerSlot = m_ft2AsyncPartnerSlotMs / static_cast<qint64>(pMs);
            qint64 const pgOurSlot = correctedUtcEpochMs() / static_cast<qint64>(pMs);
            bool const pgSameParity = ((((pgOurSlot - pgPartnerSlot) % 2) + 2) % 2) == 0;
            if (pgSameParity) {
                QString const pgMyCall = m_callsign.trimmed().toUpper();
                QString const pgDxCall = m_dxCall.trimmed().toUpper();
                bool const pgYield = (m_currentTx == 1)
                    || pgDxCall.isEmpty()
                    || pgMyCall < pgDxCall;
                if (pgYield) {
                    int const pgNewPeriod = (m_txPeriod != 0) ? 0 : 1;
                    bridgeLog(QStringLiteral("FT2 phase-lock breaker (period-gated): stessa fase del partner per %1 retry su TX%2 -> flip txPeriod %3->%4 (ourSlot=%5 partnerSlot=%6)")
                                  .arg(m_txRetryCount).arg(m_currentTx)
                                  .arg(m_txPeriod).arg(pgNewPeriod)
                                  .arg(pgOurSlot).arg(pgPartnerSlot));
                    setTxPeriod(pgNewPeriod);
                    return;
                }
                bridgeLog(QStringLiteral("FT2 phase-lock breaker (period-gated): stessa fase su TX%1 ma tengo la parita' (CQer call maggiore, retry=%2)")
                              .arg(m_currentTx).arg(m_txRetryCount));
            }
        }

        int const elapsedMs = static_cast<int>(msNow % static_cast<qint64>(pMs));
        // 1.0.318 — P2 fix: questo call site usa effectiveRelaxLatestCap() (helper 1.0.326 B4)
        int const latestStartMs = syncTxCommitDeadlineMs(effectiveRelaxLatestCap());
        if (latestStartMs > 0 && elapsedMs >= latestStartMs) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: too late in %1 slot, defer TX%2 elapsed=%3ms latest=%4ms")
                          .arg(m_mode)
                          .arg(m_currentTx)
                          .arg(elapsedMs)
                          .arg(latestStartMs));
            scheduleSyncTxAtNextValidSlot(QStringLiteral("checkAndStartPeriodicTx"),
                                          elapsedMs,
                                          latestStartMs);
            return;
        }

        if (m_mode == QStringLiteral("FT2")
            && m_asyncTxEnabled
            && !m_autoCqRepeat
            && !m_multiAnswerMode
            && !m_dxCall.trimmed().isEmpty()
            && m_currentTx == 1
            && m_lastNtx == 1
            && m_asyncLastTxEndMs > 0) {
            int const latestAsyncStartMs = latestFt2AsyncTxStartMs(pMs, m_ft2ConservativeTiming);
            int const replyGraceMs = latestAsyncStartMs > 250
                ? qBound(250, latestAsyncStartMs - 180, 650)
                : 250;
            qint64 const waitMs = static_cast<qint64>(pMs) + replyGraceMs;
            qint64 const elapsedSinceTxEndMs =
                QDateTime::currentMSecsSinceEpoch() - m_asyncLastTxEndMs;
            if (elapsedSinceTxEndMs >= 0 && elapsedSinceTxEndMs < waitMs) {
                int const retryDelayMs =
                    qBound(50, static_cast<int>(waitMs - elapsedSinceTxEndMs), replyGraceMs);
                if (QDateTime::currentMSecsSinceEpoch() - m_ft2RetryDeferLogMs >= pMs) {
                    m_ft2RetryDeferLogMs = QDateTime::currentMSecsSinceEpoch();
                    bridgeLog(QStringLiteral("FT2 manual TX1 reply grace: defer locator retry elapsed=%1ms wait=%2ms delay=%3ms")
                                  .arg(elapsedSinceTxEndMs)
                                  .arg(waitMs)
                                  .arg(retryDelayMs));
                }
                if (!m_periodicTxCheckScheduled) {
                    quint64 const graceSerial = m_ft2AsyncSmartTxSerial;
                    quint64 const requestGeneration = m_txRequestGeneration;
                    m_periodicTxCheckScheduled = true;
                    QTimer::singleShot(retryDelayMs, this, [this, graceSerial, requestGeneration]() {
                        m_periodicTxCheckScheduled = false;
                        if (graceSerial != m_ft2AsyncSmartTxSerial
                            || requestGeneration != m_txRequestGeneration
                            || m_mode != QStringLiteral("FT2")
                            || !m_asyncTxEnabled
                            || m_autoCqRepeat
                            || m_multiAnswerMode
                            || m_currentTx != 1
                            || m_lastNtx != 1
                            || !m_txEnabled
                            || m_transmitting
                            || m_tuning) {
                            return;
                        }
                        checkAndStartPeriodicTx();
                    });
                }
                return;
            }
        }

        // Guardia anti-CQ-consecutivi: dopo un CQ aspetta almeno il periodo RX
        // opposto. FT2 async decodifica in continuo, quindi non deve saltare
        // anche il prossimo slot TX valido: CQ, RX, CQ, RX.
        // FT8/FT4 sync mantengono lo stesso ritmo base: TX, RX, TX.
        bool isCqTx = (m_txEnabled && m_currentTx == 6);
        int cqGuardPeriods = 2;
        if (isCqTx && m_lastCqPidx >= 0 && (pidx - m_lastCqPidx) < cqGuardPeriods) {
            bridgeLog("checkAndStartPeriodicTx: CQ guard — pidx=" + QString::number(pidx) +
                      " lastCqPidx=" + QString::number(m_lastCqPidx) + " → pausa RX forzata");
            return;
        }
    } else {
        // FT2 async guard dopo TX:
        // - 200ms PTT debounce (sempre)
        // - Se è un RETRY dello stesso step: attendi 1 periodo per dare tempo al decode
        qint64 msNow = QDateTime::currentMSecsSinceEpoch();
        if (m_asyncLastTxEndMs > 0) {
            qint64 elapsed = msNow - m_asyncLastTxEndMs;
            if (elapsed < 200) return;  // PTT debounce
            // 1.0.174 — Retry guard SNR-adattivo:
            //   SNR > -8 dB  → 0.5 periodi (partner forte, retry rapido)
            //   SNR -8..-15  → 1.0 periodi (default)
            //   SNR < -15    → 1.5 periodi (QSB tolerant)
            //   Conservative ON → minimo 1.0 periodi, weak-signal scala a 1.5
            qint64 const periodMs = periodMsForMode(m_mode);
            double waitFactor = 1.0;
            bool const sameExchangeStepRetry =
                m_currentTx == m_lastNtx
                && m_currentTx >= 2
                && m_currentTx <= 5;
            // FIX E: il retry dello stesso step NON deve costare un periodo
            // intero (3750ms, "QSO sembra fermo"). Su FT2, se non Conservative
            // e il partner non e' debole (SNR > -15), mezzo periodo (1875ms)
            // basta a dare tempo al decode. 1.0/1.5 restano per deboli/Conservative.
            bool const partnerWeakRetry =
                m_currentPartnerSnrDb != 127 && m_currentPartnerSnrDb <= -15;
            bool const ft2FastRetry =
                m_mode == QStringLiteral("FT2")
                && !m_ft2Conservative
                && !partnerWeakRetry;
            if ((!sameExchangeStepRetry || ft2FastRetry)
                && !m_ft2Conservative
                && m_currentPartnerSnrDb != 127
                && m_currentPartnerSnrDb > -8) {
                waitFactor = 0.5;
            } else if (sameExchangeStepRetry && ft2FastRetry) {
                // FIX E: same-step retry FT2 non-debole con SNR intermedio
                // (-15..-8) -> mezzo periodo (non un periodo intero).
                waitFactor = 0.5;
            } else if (m_currentPartnerSnrDb != 127 && m_currentPartnerSnrDb < -15) {
                waitFactor = 1.5;
            }
            qint64 const waitMs = static_cast<qint64>(periodMs * waitFactor);
            if (m_currentTx == m_lastNtx && elapsed < waitMs) {
                // Sprint1-4 (1.0.393): il rinvio era un return MUTO -> impossibile
                // quantificare dai log gli slot persi in retry. 1 riga per periodo.
                if (msNow - m_ft2RetryDeferLogMs >= periodMs) {
                    m_ft2RetryDeferLogMs = msNow;
                    bridgeLog(QStringLiteral("FT2 async retry guard: defer TX%1 elapsed=%2ms wait=%3ms snr=%4 factor=%5")
                                  .arg(m_currentTx).arg(elapsed).arg(waitMs)
                                  .arg(m_currentPartnerSnrDb).arg(waitFactor));
                }
                return;
            }
        }
        // Sprint1-2 (1.0.393): il phase-lock breaker 1.0.364 vive solo in
        // scheduleFt2AsyncTxAtNextSafeSlot (path "TX too late"), MAI eseguito in
        // esercizio (0 firing in 46h di log: il TX async non e' mai late). Il
        // retry same-step attende periodi INTERI -> PRESERVA la parita': due
        // stazioni in fase restano in deadlock fino al TX watchdog (13 casi/46h,
        // caso reale R+49 ripetuto 5 volte). Replica del check QUI, nel path che
        // keya davvero: con retry>=3 e ancora fresca (<=60s, rinfrescata da
        // Sprint1-1), se lo slot in cui stiamo per trasmettere ha la STESSA
        // parita' dello slot del partner, rinvia al boundary successivo (fase
        // opposta). Auto-stabilizzante come l'originale; costa max 1 periodo.
        if (m_ft2ParityDeferUntilMs > 0
            && correctedUtcEpochMs() < m_ft2ParityDeferUntilMs) {
            return;
        }
        // NB: m_ft2ParityDeferUntilMs NON viene azzerato alla scadenza: resta
        // come timestamp "fase corretta a X" per la grace anti-release sotto
        // (releasing stale partner wait). Un valore vecchio e' inerte: il
        // check sopra e' false e la grace (defer+2 periodi) e' gia' passata.
        // Soglia >=2 (non >=3): il give-up same-step puo' scattare a 3 retry
        // e vincere la corsa col breaker (osservato in loopback 2026-06-11:
        // collisione organica, BRAVO ha mollato esattamente al 3o tentativo).
        // A >=2 il breaker scatta al 3o TX, PRIMA del give-up; falso positivo
        // = 1 periodo perso, accettabile.
        if (m_ft2AsyncPartnerSlotMs > 0 && m_txRetryCount >= 2
            && m_ft2AsyncPartnerSlotSetMs > 0
            && (msNow - m_ft2AsyncPartnerSlotSetMs) <= 60000) {
            int const ft2LockPeriodMs = periodMsForMode(QStringLiteral("FT2"));
            if (ft2LockPeriodMs > 0) {
                qint64 const lockNowMs = correctedUtcEpochMs();
                qint64 const ourSlot = lockNowMs / static_cast<qint64>(ft2LockPeriodMs);
                qint64 const partnerSlot = m_ft2AsyncPartnerSlotMs / static_cast<qint64>(ft2LockPeriodMs);
                qint64 const parity = (((ourSlot - partnerSlot) % 2) + 2) % 2;
                if (parity == 0) {
                    // Tie-break anti-danza (loopback 2026-06-11 round 2): se ENTRAMBE
                    // le stazioni montano il breaker, sfasano insieme e restano in
                    // collisione (shift simultaneo osservato: parita' identica al
                    // firing successivo). SOLO il call minore cede; il maggiore tiene
                    // SEMPRE (round 5: la clausola "maggiore cede ai retry dispari"
                    // faceva cedere ENTRAMBI sui dispari -> collisione ricreata,
                    // :16:45-48 BRAVO shiftato DENTRO lo slot di ALPHA). Contro
                    // versioni vecchie senza breaker: se siamo il maggiore il
                    // deadlock resta come pre-fix (non peggioriamo nulla).
                    QString const tieMyCall = m_callsign.trimmed().toUpper();
                    QString const tieDxCall = m_dxCall.trimmed().toUpper();
                    // P0-3 (log ZL 2026-06-11): a TX<=2 siamo ancora in aggancio -
                    // il partner non ha mai risposto a noi (se avesse risposto
                    // saremmo a TX3+). In collisione e' LUI a non sentirci: il
                    // CQer non ha anchor e non puo' cedere -> cediamo noi SEMPRE,
                    // anche da call maggiore (deadlock#1: ZL3DMH "teneva" 3 volte
                    // contro un CQer sordo). Call-order resta per TX3-5 simmetrico.
                    bool const iYield = tieDxCall.isEmpty()
                        || tieMyCall < tieDxCall
                        || m_currentTx <= 2;
                    if (!iYield) {
                        bridgeLog(QStringLiteral("FT2 phase-lock breaker: stessa fase ma tengo la parita' (tie-break call maggiore, retry=%1)")
                                      .arg(m_txRetryCount));
                    } else {
                    m_ft2ParityDeferUntilMs = (ourSlot + 1) * static_cast<qint64>(ft2LockPeriodMs);
                    bridgeLog(QStringLiteral("FT2 phase-lock breaker (path diretto): stessa fase del partner per %1 retry -> rinvio al prossimo slot (ourSlot=%2 partnerSlot=%3 deferUntilMs=%4)")
                                  .arg(m_txRetryCount).arg(ourSlot).arg(partnerSlot)
                                  .arg(m_ft2ParityDeferUntilMs));
                    return;
                    }
                }
            }
        }
        int elapsedMs = -1;
        int latestStartMs = 0;
        int delayMs = 0;
        if (isFt2AsyncTxStartTooLate(&elapsedMs, &latestStartMs, &delayMs)) {
            scheduleFt2AsyncTxAtNextSafeSlot(QStringLiteral("checkAndStartPeriodicTx"),
                                             elapsedMs,
                                             latestStartMs,
                                             delayMs);
            return;
        }
    }

    QString selectedTxPayload = buildCurrentTxMessage().trimmed();
    if (selectedTxPayload.isEmpty() && autoCqHasActivePartner()) {
        restoreAutoCqPartnerLock();
        regenerateTxMessages();
        selectedTxPayload = buildCurrentTxMessage().trimmed();
        if (selectedTxPayload.isEmpty()) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: suppress AutoCQ CQ fallback while partner active tx=%1 progress=%2 lock=%3")
                          .arg(m_currentTx)
                          .arg(m_qsoProgress)
                          .arg(m_autoCqLockedCall.trimmed().isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_autoCqLockedCall.trimmed()));
            return;
        }
    }
    forceRecentRogerReportSignoffIfNeeded(selectedTxPayload, QStringLiteral("checkAndStartPeriodicTx"));
    if (m_txEnabled && !selectedTxPayload.isEmpty()) {
        if (m_logAfterOwn73 && m_currentTx != 5 && !m_tx5.isEmpty()) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: own 73 pending -> force TX5 (current TX%1)")
                          .arg(m_currentTx));
            if (!advanceQsoState(5)) {
                return;
            }
            updateAutoCqPartnerLock();
        }

        if (m_currentTx == 4 && m_quickQsoEnabled && m_quickPeerSignaled) {
            finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: Quick QSO signoff complete after TX4"), true);
            return;
        }

        // 1.0.446 - bugfix: in FT2 il TX3 "R+report" (qsoProgress=4 ROGER_REPORT,
        // payload "R-NN" senza token 73/RR73/RRR) e il TX3 QuickQSO con "TU"
        // (qsoProgress=5) NON erano cappati da nessuno: completeFinishedSignoffIfReady
        // esce (payload non-73) e il Caller-retries cap e' disattivato perche'
        // inSignoffTxStep=true quando qsoProgress==4/5 (guard 1.0.433, voluto: non
        // azzerare lo stato a TX3 mentre arriva l'RR73 del partner -> evitare RR73
        // droppato come in 1.0.431). Risultato: il TX3 si ripeteva fino al TX watchdog
        // (~14x) senza loggare ("troppe chiamate"). Applichiamo qui lo STESSO signoff
        // cap del ramo TX4-RR73 (spinbox FT2, default 4) usando m_txRetryCount (m_nTx73
        // resta 0 perche' il payload non e' 73); raggiunto il cap logghiamo il QSO e
        // fermiamo. SICUREZZA anti-regressione 1.0.431: finishAutoSequenceQso(...,true)
        // LOGGA prima di azzerare e popola cooldown + recentWorked cache, quindi un
        // RR73 tardivo viene scartato dal gate cooldown in autoSequenceStep (no doppio
        // log, no riavvio TX1). Copre QuickQSO (progress5) e R+report normale (progress4).
        // 1.0.716: the cap was FT2 only. In FT8/FT4 a QuickQSO TX3 "R-16 TU" (or a TX3
        // R+report) to a partner who vanished was repeated until someone pressed HALT
        // (DD8HG, 26/10/2026: 41 identical transmissions in 20 minutes).
        if (m_currentTx == 3
            && (m_qsoProgress == 4 || m_qsoProgress == 5)
            && (m_mode == QStringLiteral("FT2") || m_mode == QStringLiteral("FT4")
                || m_mode == QStringLiteral("FT8"))) {
            int tx3SignoffCap = deferredSignoffRetryCapForMode(m_mode, m_maxCallerRetries,
                                                               m_currentPartnerSnrDb, m_ft2Conservative,
                                                               m_ft2QuickGiveUpStrong, m_ft2SignoffRetryCap,
                                                               m_ft4SignoffRetryCap, m_ft8SignoffRetryCap,
                                                               m_ftxWeakSignoffBoost, m_ftxWeakSnrThreshold, m_ftxWeakSignoffBonus);
            // 1.0.721: in FT8/FT4 the TX3 R-report is still waiting for the DX station's RR73, and
            // a DX in a pile-up can take many cycles to come back. The operator's own "Caller
            // retries" is the patience he asked for (IT9MRM, 20): the signoff cap of 3 closed
            // those QSOs after 90 s without a confirmation. FT2 keeps its own cap.
            if (m_mode != QStringLiteral("FT2")) {
                tx3SignoffCap = qMax(tx3SignoffCap, m_maxCallerRetries);
            }
            if (m_currentTx == m_lastNtx && m_txRetryCount >= tx3SignoffCap) {
                finishAutoSequenceQso(QStringLiteral("checkAndStartPeriodicTx: TX3 R+report mode=%1 progress=%2 retry cap %3/%4 -> log anyway, retry cap reached")
                                          .arg(m_mode)
                                          .arg(m_qsoProgress)
                                          .arg(m_txRetryCount)
                                          .arg(tx3SignoffCap), true);
                return;
            }
        }

        // Qualsiasi payload di signoff (RR73/RRR/73) completato >=1 volta chiude il QSO.
        if (completeFinishedSignoffIfReady()) {
            return;
        }

        // Retry limit: vale sia per AutoCQ sia per i QSO manuali avviati con doppio click.
        bool isCqAutoCq = (m_currentTx == 6 && m_autoCqRepeat);
        bool const manualPartnerQso =
            m_txEnabled
            && !m_autoCqRepeat
            && !m_dxCall.trimmed().isEmpty()
            && m_currentTx != 6;
        // "Caller retries" caps repeated CALLING steps (TX1/TX2) toward a partner
        // that hasn't engaged. It must NOT abort a QSO already in its CLOSE phase:
        // TX4/TX5 are the final signoff steps (per-mode signoff retry cap), and
        // TX3=R+report puts us at qsoProgress 4 (ROGER_REPORT) where the QSO is
        // established and only the partner's RR73/73 is missing. 1.0.433: extend
        // the upstream 1.0.432 guard (only TX4/TX5/SIGNOFF) to also cover
        // qsoProgress==4 - otherwise with a low cap (e.g. 1) in FT8/FT4 the halt
        // at TX3 cleared the QSO state right when the incoming RR73 arrived, so
        // the RR73 was dropped (user FT8 report; 1.0.431 fix overwritten in 1.0.432).
        // qsoProgress 4/5 can describe a closing QSO, but it must not
        // exempt a still-repeated TX2 report from Caller retries.  Otherwise
        // a partner that already received our report can leave TX2 looping
        // indefinitely (the visible symptom was 9+ identical report TXs
        // with Caller retries set to 3).  Only TX3-TX5 are sign-off steps.
        bool const inSignoffTxStep =
            (m_currentTx == 4 || m_currentTx == 5
             || (m_currentTx >= 3 && (m_qsoProgress == 4 || m_qsoProgress == 5)));
        bool applyRetryLimit =
            (m_autoCqRepeat || manualPartnerQso)
            && !isCqAutoCq
            && !inSignoffTxStep
            && !(m_currentTx == 5 && m_ft2DeferredLogPending);
        int effectiveMaxCallerRetries = m_maxCallerRetries;
        // 1.0.429: il valore "Caller retries" impostato dall'utente e' un limite
        // HARD per-step. Rimosso lo slow-CPU assist che alzava silenziosamente il
        // cap a 18 (FT2 async) / 14 (FT8/FT4) sotto pressione CPU/low-CPU mode:
        // scavalcava l'impostazione (utente metteva 4 e ne vedeva ~18). Chi vuole
        // piu' retry sotto carico alza direttamente lo SpinBox (opt-in, coerente con
        // "niente automazione silenziosa che scavalca i settaggi").
        // Sprint1 (1.0.393): se il phase-lock breaker ha appena corretto la
        // parita', il tentativo successivo e' il PRIMO in fase giusta: concedi
        // +1 retry oltre il cap. Senza questo (round 4 loopback, 02:09:22) il
        // cap contava il tentativo corretto come ennesimo e haltava il QSO
        // esattamente al boundary del defer del breaker.
        if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled
            && m_ft2ParityDeferUntilMs > 0) {
            int const parityGraceP = periodMsForMode(QStringLiteral("FT2"));
            if (parityGraceP > 0
                && correctedUtcEpochMs() < m_ft2ParityDeferUntilMs + 4 * static_cast<qint64>(parityGraceP)) {
                effectiveMaxCallerRetries += 1;
            }
        }
        // m_txRetryCount is set to 1 by the first successful send of a TX step.
        // The configured cap is the total number of sends, so equality already
        // means the operator's retry budget has been consumed.
        // 1.0.438: se il TX watchdog e' configurato, deve essere il limite
        // prioritario della sessione TX. Il cap "Caller retries" resta valido
        // quando il watchdog e' OFF; quando il watchdog e' ON non deve fermare
        // la chiamata prima del timeout impostato dall'utente (log reali: watchdog
        // 60min disarmato dopo 3-4min per retry limit TX1).
        bool const retryLimitOwnedByWatchdog =
            applyRetryLimit
            && !m_callerRetriesAlwaysHard   // 1.0.446 P1-5: opt-in -> cap duro anche con watchdog
            && txWatchdogLimitConfigured()
            && (m_txWatchdogMode == 1 || m_txWatchdogMode == 2);
        if (applyRetryLimit && retryLimitOwnedByWatchdog
            && m_currentTx == m_lastNtx
            && m_txRetryCount >= effectiveMaxCallerRetries) {
            if (m_txRetryCount == effectiveMaxCallerRetries
                || (effectiveMaxCallerRetries > 0
                    && (m_txRetryCount % effectiveMaxCallerRetries) == 0)) {
                bridgeLog(QStringLiteral("checkAndStartPeriodicTx: retry limit %1 su TX%2 ignorato: TX watchdog priority mode=%3 limit=%4")
                              .arg(effectiveMaxCallerRetries)
                              .arg(m_currentTx)
                              .arg(m_txWatchdogMode)
                              .arg(m_txWatchdogMode == 1
                                       ? QStringLiteral("%1min").arg(m_txWatchdogTime)
                                       : QStringLiteral("%1 periods").arg(m_txWatchdogCount)));
            }
        } else if (applyRetryLimit && m_currentTx == m_lastNtx
            && m_txRetryCount >= effectiveMaxCallerRetries) {
            QString const assistSuffix =
                (effectiveMaxCallerRetries != m_maxCallerRetries)
                    ? QStringLiteral(" (parity grace +1)")
                    : QString();
            QString const retryReason =
                "checkAndStartPeriodicTx: retry limit " +
                QString::number(effectiveMaxCallerRetries) + " su TX" +
                QString::number(m_currentTx) + assistSuffix + " → halt";
            bridgeLog(retryReason);
            QString const abandonedPartner = inferredPartnerForAutolog();
            if ((m_autoCqRepeat || m_multiAnswerMode) && m_qsoProgress > 1) {
                if (m_autoCqRepeat) {
                    armLateAutoLogSnapshot();
                }
                if (!abandonedPartner.trimmed().isEmpty()) {
                    rememberCompletedAutoCqPartner(abandonedPartner, false, retryReason);
                }
            }
            bool const continueQueuedCaller = (m_multiAnswerMode || m_autoCqRepeat) && !m_callerQueue.isEmpty();
            bool const continueAutoCq = autoCqCanRestartTx6();
            // IMPORTANT: do not pulse TX off while AutoCQ is meant to continue.
            // On the legacy backend, setTxEnabled(false) also clears AutoCQ/Auto.
            if (!continueQueuedCaller && !continueAutoCq) {
                setTxEnabled(false);
            }
            m_txRetryCount = 0;
            m_lastNtx = -1;
            m_nTx73    = 0;
            m_autoSeqRogerReportBase.clear();
            m_logAfterOwn73 = false;
            m_ft2DeferredLogPending = false;
            m_quickPeerSignaled = false;
            m_qsoLogged = false;
            m_lastCqPidx = -1;
            m_lastTransmittedMessage.clear();
            m_activeTxNumber = 0;
            m_activeTxMessage.clear();
            m_qsoStartedOn = QDateTime {};
            m_qsoFirstReplyOn = QDateTime {};
            clearPendingAutoSeqTx(retryReason);
            forgetPartnerMemoryForCall(abandonedPartner, retryReason);
            clearAutoCqPartnerLock();
            if (m_mode == QStringLiteral("FT2") && m_asyncTxEnabled) {
                clearFt2AsyncAbortQsoState(retryReason);
            }
            // clearDX: azzera dxCall (stazione non raggiungibile)
            setDxCall(QString());
            setDxGrid(QString());
            if (continueQueuedCaller) {
                processNextInQueue();
            } else if (continueAutoCq) {
                if (!advanceQsoState(6)) return;
                if (!m_txEnabled) setTxEnabled(true);
                checkAndStartPeriodicTx();
            } else {
                if (m_qsoProgress != 0) {
                    m_qsoProgress = 0;
                    emit qsoProgressChanged();
                }
                setCurrentTx(m_tx6.trimmed().isEmpty() ? 1 : 6);
            }
            return;
        }

        if (isAutoCqBurstPureCq(m_currentTx, selectedTxPayload)
            && autoCqBurstListening()) {
            bridgeLog(QStringLiteral("AutoCQ burst: suppressing CQ while listening"));
            return;
        }

        bridgeLog("checkAndStartPeriodicTx: TX" + QString::number(m_currentTx) +
                  " retry=" + QString::number(m_txRetryCount) +
                  " async=" + QString::number(m_asyncTxEnabled));
        // Aggiorna lastCqPidx se stiamo inviando CQ (TX6)
        if (m_currentTx == 6 && !skipPeriodCheck) {
            qint64 msNow2 = correctedUtcMsecsSinceStartOfDay();
            int pMs2 = periodMsForMode(m_mode);
            m_lastCqPidx = (int)(msNow2 / (qint64)pMs2);
        }
        startTx();
    } else if (autoCqCanRestartTx6()) {
        if (autoCqHasActivePartner()) {
            bridgeLog(QStringLiteral("checkAndStartPeriodicTx: suppress AutoCQ CQ fallback while partner active tx=%1 progress=%2 dx=%3 lock=%4")
                          .arg(m_currentTx)
                          .arg(m_qsoProgress)
                          .arg(m_dxCall.trimmed().isEmpty() ? QStringLiteral("<none>") : m_dxCall.trimmed(),
                               m_autoCqLockedCall.trimmed().isEmpty()
                                   ? QStringLiteral("<none>")
                                   : m_autoCqLockedCall.trimmed()));
            return;
        }
        if (autoCqBurstListening()) {
            bridgeLog(QStringLiteral("AutoCQ burst: suppressing CQ fallback while listening"));
            return;
        }
        bridgeLog("checkAndStartPeriodicTx: AutoCQ async=" + QString::number(m_asyncTxEnabled));
        if (!advanceQsoState(6)) {  // CQ -> CALLING (0)
            return;
        }
        // Aggiorna lastCqPidx per la guardia anti-consecutivi
        if (!skipPeriodCheck) {
            qint64 msNow2 = correctedUtcMsecsSinceStartOfDay();
            int pMs2 = periodMsForMode(m_mode);
            m_lastCqPidx = (int)(msNow2 / (qint64)pMs2);
        }
        startTx();
    }
}

