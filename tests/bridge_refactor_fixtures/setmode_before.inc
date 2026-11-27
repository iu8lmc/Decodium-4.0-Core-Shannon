void DecodiumBridge::setMode(const QString& v) {
    QString const requestedMode = v.trimmed();
    QString normalizedMode = canonicalApplicationDecodeMode(requestedMode);
    if (normalizedMode.isEmpty()) {
        if (isRadioOnlyModeLabel(requestedMode)) {
            if (m_catMode.compare(requestedMode, Qt::CaseInsensitive) != 0) {
                m_catMode = requestedMode;
                emit catModeChanged();
            }
            bridgeLog(QStringLiteral("setMode: ignored radio/CAT mode %1, keeping application mode %2")
                          .arg(requestedMode, m_mode));
        } else if (!requestedMode.isEmpty()) {
            bridgeLog(QStringLiteral("setMode: ignored unsupported application mode %1, keeping %2")
                          .arg(requestedMode, m_mode));
        }
        return;
    }

    // Keep compatibility with profiles created before the explicit Q65
    // selector existed.  The historic plain label always meant Q65-60A.
    if (normalizedMode == QStringLiteral("Q65")) {
        normalizedMode = QStringLiteral("Q65-60A");
    }

    if (isFt2LinkApplicationMode(normalizedMode) && !m_ft2LinkAccessUnlocked) {
        bridgeLog(QStringLiteral("FT2-Link access denied: mode change blocked until password is verified"));
        if (m_mode != QStringLiteral("FT2")) {
            setMode(QStringLiteral("FT2"));
        }
        return;
    }

    if (m_mode != normalizedMode) {
        if (m_q65Doppler) m_q65Doppler->setEnabled(false);
        quint64 const rttyRigGeneration = ++m_rttyRigModeGeneration;
        if (normalizedMode == QStringLiteral("RTTY")) {
            m_rttyRigModeState.enter(rttyCatContext(), m_catMode);
            bridgeLog(QStringLiteral("RTTY entry state: catMode=%1 rxLevel=%2 rxGain=%3 autoRx=%4 channel=%5")
                          .arg(m_catMode).arg(m_rxInputLevel).arg(rxInputGainFromLevel(m_rxInputLevel))
                          .arg(m_autoRxInputLevel ? 1 : 0).arg(m_audioInputChannel));
        } else if (m_mode == QStringLiteral("RTTY"))
            m_rttyRigModeState.leave(QDateTime::currentMSecsSinceEpoch());
        else
            m_rttyRigModeState.clear();
        if (m_rttyRxRecovery)
            m_rttyRxRecovery->cancel(QStringLiteral("application mode changed"));
        // 1.0.179 — Smooth Decode Flow: cambio mode invalida la coda
        // residua (per evitare merge cross-mode); reset + stop timer.
        if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
        m_pendingDecodeReleaseQueue.clear();
        QString const previousMode = m_mode;
        // A different application modem invalidates every deferred FT/JT
        // request. In particular, a callback created before entering JTTY
        // must never be allowed to reuse its old authorisation afterwards.
        invalidateDeferredTxRequests(QStringLiteral("application mode changed %1->%2")
                                         .arg(previousMode, normalizedMode));
        bool const enteringCw = normalizedMode == QStringLiteral("CW")
            && previousMode != QStringLiteral("CW");
        bool const leavingCw = previousMode == QStringLiteral("CW")
            && normalizedMode != QStringLiteral("CW");
        // The physical rig can already report CW at startup while Decodium
        // restores a saved digital application mode.  In that case there is
        // no application-side CW -> digital transition, but the next explicit
        // mode selection must still dislodge the rig from its startup CW
        // state and protect the request from a delayed CAT snapshot.
        QString const reportedCatModeBeforeChange = m_catMode.trimmed().toUpper();
        bool const leavingReportedRigCw = normalizedMode != QStringLiteral("CW")
            && (reportedCatModeBeforeChange == QStringLiteral("CW")
                || reportedCatModeBeforeChange == QStringLiteral("CW-R"));
        if (enteringCw) {
            QString const previousCatMode = m_catMode.trimmed().toUpper();
            m_catModeBeforeCw = (previousCatMode != QStringLiteral("CW")
                                  && previousCatMode != QStringLiteral("CW-R"))
                ? previousCatMode
                : QString();
            bridgeLog(QStringLiteral("CW entry CAT snapshot: reported=%1 restore=%2")
                          .arg(previousCatMode,
                               m_catModeBeforeCw.isEmpty()
                                   ? QStringLiteral("none") : m_catModeBeforeCw));
        }
        bool const monitorWasActive = m_monitoring;
        bool const monitorShouldStayActive = monitorWasActive || m_monitorRequested;
        bool const sourceUsesLegacyRx = usingLegacyBackendForRx();
        bool const enteringRtty = isStreamingKeyboardMode(normalizedMode);
        // Compute this before m_mode is changed.  A legacy FT8/JT monitor
        // needs a deliberate hand-off to the native RTTY PCM capture.
        // JTTY owns the native PCM capture while it is active.  Leaving it
        // for a legacy FT8/FT4 mode must perform the same explicit hand-off
        // as entering RTTY; otherwise the old SoundInput remains attached
        // while the legacy backend starts its period monitor, freezing the
        // panadapter/waterfall on the first slot after the mode change.
        bool const leavingJtty = isNativePcmKeyboardMode(previousMode);
        bool const rearmModernMonitor = monitorWasActive
            && (!sourceUsesLegacyRx || enteringRtty || leavingJtty);
        quint64 const monitorSessionId = monitorShouldStayActive ? ++m_periodTimerSessionId : m_periodTimerSessionId;
        if (rearmModernMonitor && m_periodTimer) {
            m_periodTimer->stop();
        }

        resetAutoCqBurstCadence(QStringLiteral("application mode changed %1->%2")
                                    .arg(previousMode, normalizedMode));
        m_mode = normalizedMode;
        if (!usesLegacyRxLevels() && m_sMeter != 0.0) {
            // The stopped legacy backend retains its last positive meter
            // value. Do not let it mask live native RMS on FT -> CW/JTTY/SSB.
            m_sMeter = 0.0;
            emit sMeterChanged();
        }
        decodium::audio::LegacyRxHandoffState legacyHandoffState;
        legacyHandoffState.monitoringActive = monitorWasActive;
        legacyHandoffState.leavingNativePcmMode = leavingJtty;
        legacyHandoffState.targetUsesLegacyBackend = usingLegacyBackendForRx();
        legacyHandoffState.targetKeepsDedicatedModernCapture =
            useDedicatedModernAudioCaptureWithLegacy();
        bool const nativePcmToLegacyRxHandoff =
            decodium::audio::requiresLegacyRxHandoff(legacyHandoffState);
        bool const bridgeOwnedExplicitQ65 = m_mode.startsWith(QStringLiteral("Q65-"), Qt::CaseInsensitive);
        decodium::audio::BridgeRxHandoffState bridgeHandoffState;
        bridgeHandoffState.monitoringActive = monitorWasActive;
        bridgeHandoffState.sourceUsesLegacyBackend = sourceUsesLegacyRx;
        bridgeHandoffState.targetUsesLegacyBackend = usingLegacyBackendForRx();
        bool const legacyToBridgeRxHandoff =
            decodium::audio::requiresBridgeRxHandoff(bridgeHandoffState);
        if (legacyToBridgeRxHandoff && m_periodTimer) {
            m_periodTimer->stop();
        }

        if (bridgeOwnedExplicitQ65) {
            // Explicit Q65 variants must never share RX ownership with the
            // old backend: it has no period/submode contract and can otherwise
            // leave the native audio buffer empty after the first window.
            if (m_legacyBackend && m_legacyBackend->monitoring()) {
                m_legacyBackend->setMonitoring(false);
            }
            m_legacyPcmSpectrumFeed = false;
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
            if (legacyToBridgeRxHandoff) {
                if (m_asyncDecodeTimer) {
                    m_asyncDecodeTimer->stop();
                }
                m_asyncDecodePending = false;
                bridgeLog(QStringLiteral("Q65 RX hand-off: legacy=%1 -> native=%2; rearming capture and UTC timer")
                              .arg(previousMode, m_mode));
            }
        }

        if (previousMode == QStringLiteral("RTTY")) {
            // Stop producers before releasing their shared USB output. The
            // mode has already changed, so late RTTY callbacks cannot re-key.
            emit rttyModeLeaving();
            setRttyInAscolto(false);
            decoPortKeyLocalRig(false);
            if (m_decoPortTxOut) {
                QPointer<RtlSdrAudioOutput> out(m_decoPortTxOut);
                QMetaObject::invokeMethod(m_decoPortTxOut, [out]() {
                    if (out) out->stop(QStringLiteral("leaving-rtty"));
                }, Qt::QueuedConnection);
            }
            m_decoPortTxOutRate = 0;
            bridgeLog(QStringLiteral("Leaving RTTY: producer stopped, PTT release requested, USB TX output closing"));
        }
        if (previousMode == QStringLiteral("JTTY")) {
            // Come per RTTY: prima si ferma chi produce audio, poi si
            // rilascia l'uscita condivisa.
            emit jttyModeLeaving();
            decoPortKeyLocalRig(false);
            if (m_decoPortTxOut) {
                QPointer<RtlSdrAudioOutput> out(m_decoPortTxOut);
                QMetaObject::invokeMethod(m_decoPortTxOut, [out]() {
                    if (out) out->stop(QStringLiteral("leaving-jtty"));
                }, Qt::QueuedConnection);
            }
            m_decoPortTxOutRate = 0;
            bridgeLog(QStringLiteral("Leaving JTTY: producer stopped, PTT release requested, USB TX output closing"));
        }
        if (previousMode == QStringLiteral("SSB")) ssbStop();
        if (previousMode == QStringLiteral("CW")) {
            // Il CW in corso si ferma: manipolatore, audio e PTT remoto.
            emit cwModeLeaving();
            bridgeLog(QStringLiteral("Leaving CW: transmission stopped"));
        }
        if (normalizedMode == QStringLiteral("JTTY") || normalizedMode == QStringLiteral("CW") || normalizedMode == QStringLiteral("SSB")) {
            // JTTY ha il suo ricevitore nativo sul PCM: il decodificatore
            // legacy si ferma come per RTTY. Lo stesso vale per il CW. La radio resta nel modo dati
            // configurato (e' la stessa strada di FT8) e la frequenza la
            // sceglie il piano di banda di JTTY.
            if (m_legacyBackend && m_legacyBackend->monitoring()) {
                m_legacyBackend->setMonitoring(false);
            }
            m_legacyPcmSpectrumFeed = false;
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
            if (m_decoding) {
                m_decoding = false;
                emit decodingChanged();
            }
            bridgeLog(QStringLiteral("JTTY exclusive workspace: legacy decoder stopped; native PCM keeps JTTY and panadapter active"));
        }

        // Entrando in RTTY si commuta la radio, perche' sceglierlo dal
        // selettore dei modi e' una richiesta di lavorare in RTTY, non di
        // guardarlo. La decodifica dei modi digitali si ferma da se': RTTY non
        // e' fra i modi a slot, e dispatchTimeSyncDecodeWhenReady esce subito.
        //
        // Il modo della radio e' quello che l'operatore ha configurato per i
        // dati (DATA-U di norma), non uno deciso qui: e' la stessa radio che
        // usa per FT8, e imporle un modo diverso da quello che ha scelto
        // sarebbe una sorpresa. La frequenza va sul segmento RTTY della banda
        // in cui si trova gia', non su una banda decisa da noi.
        if (normalizedMode == QStringLiteral("RTTY")) {
            // RTTY uses the native QML modem; the embedded legacy backend
            // cannot decode RTTY and otherwise keeps decoding the previously
            // selected digital mode in the background.
            if (m_legacyBackend && m_legacyBackend->monitoring()) {
                m_legacyBackend->setMonitoring(false);
            }
            // Do not leave the panadapter waiting for the legacy PCM tap we
            // have just stopped; the native capture below feeds it directly.
            m_legacyPcmSpectrumFeed = false;
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
            if (m_decoding) {
                m_decoding = false;
                emit decodingChanged();
            }
            bridgeLog(QStringLiteral("RTTY exclusive workspace: legacy decoder stopped; native PCM keeps RTTY and panadapter active"));
            applyRttyRigMode(QStringLiteral("rtty-mode"));
#if DECODIUM_HAS_RTTY
            int const banda = decortty::app::bandAt(m_frequency / 1e6);
            if (banda >= 0) {
                double const rttyHz = decortty::app::bandPlan()[static_cast<size_t>(banda)].rttyMhz * 1e6;
                bridgeLog(QStringLiteral("RTTY: QSY al segmento RTTY dei %1 -> %2 Hz")
                              .arg(decortty::app::bandPlan()[static_cast<size_t>(banda)].name)
                              .arg(rttyHz, 0, 'f', 0));
                requestRigFrequencyFromBridge(rttyHz, QStringLiteral("rtty-mode"));
            } else {
                bridgeLog(QStringLiteral("RTTY: la radio non e' in una banda HF nota, "
                                         "frequenza lasciata dov'e'"));
            }
#endif
        }

        if (m_autoCallEnabled && !autoCallModeSupported()) {
            bridgeLog(QStringLiteral("[AUTOCALL] disabled by mode change to %1").arg(m_mode));
            setAutoCallEnabled(false);
        }
        if ((m_mainQmlReady || m_startupServicesStarted) && !legacyBackendRequestedForRx()) {
            ensureDecodeWorkerForMode(m_mode);
        }
        if (m_ft2Worker) {
            m_ft2Worker->setDecodeEnabled(m_mode == QStringLiteral("FT2"));
        }
        if (m_legacyBackend) {
            m_legacyBackend->setFt2DecodeEnabled(m_mode == QStringLiteral("FT2"));
        }
        // 1.0.364+ MAM multi-stream nativo: un cambio modo invalida
        // ogni QSO multi-stream in corso.
        m_mamSlots.clear();
        m_mamLateReportRecoveries.clear();
        m_mamMessages.clear();
        m_mamF0sHz.clear();
        m_mamGains.clear();
        emit mamTxStreamsChanged();
        // A mode switch invalidates short-lived CAT/audio-frequency guards
        // from the previous mode. Leaving them active can make a later CAT
        // poll look stale and keep the displayed frequency stuck.
        m_localCatFrequencyGuardUntilMs = 0;
        m_localCatFrequencyTargetHz = 0.0;
        m_localCatFrequencyPreviousHz = 0.0;
        m_localRxFrequencyGuardUntilMs = 0;
        m_localRxFrequencyTargetHz = 0;
        m_localTxFrequencyGuardUntilMs = 0;
        m_localTxFrequencyTargetHz = 0;
        invalidateTxAudioCache();
        updatePeriodTicksMax();
        resetTimeSyncDecodeMetrics();
        if (previousMode == QStringLiteral("FT2") && m_mode != QStringLiteral("FT2")) {
            if (m_asyncDecodeTimer) {
                m_asyncDecodeTimer->stop();
            }
            m_asyncDecodePending = false;
            m_asyncAudioPos.store(0, std::memory_order_release);
            m_ft2AsyncLastDispatchAudioPos = 0;
            m_lastFt2AsyncDecodeDispatchMs = 0;
            if (m_ft2Worker) {
                QCoreApplication::removePostedEvents(m_ft2Worker, QEvent::MetaCall);
                m_ft2Worker->markLatestDecodeSerial(std::numeric_limits<quint64>::max());
                m_ft2Worker->cancelCurrentDecode();
            }
            bridgeLog(QStringLiteral("Leaving FT2 mode: legacy FT2 async/sync decode stopped (next=%1)")
                          .arg(m_mode));
        }
        configureNtpClientForMode(m_mode);
        if (m_ntpClient && m_ntpEnabled && isTimeSyncDecodeMode(m_mode)) {
            m_ntpClient->syncNow();
        }
        // FT2 rule (mainwindow: "Async L2 is mandatory and always ON"):
        // cbAsyncDecode forced=true, disabled when in FT2.  Mirror that here.
        if (m_mode == "FT2") {
            if (!m_autoSeq)            { m_autoSeq = true;            emit autoSeqChanged(); }
            if (!m_asyncTxEnabled)     { m_asyncTxEnabled = true;     emit asyncTxEnabledChanged(); }
            if (!m_asyncDecodeEnabled) { m_asyncDecodeEnabled = true;  emit asyncDecodeEnabledChanged(); }
            m_qsoCooldown.clear();
        } else {
            // Uscendo da FT2, forza async decode OFF (mainwindow: cbAsyncDecode->setChecked(false))
            if (m_asyncDecodeEnabled)  { m_asyncDecodeEnabled = false; emit asyncDecodeEnabledChanged(); }
        }
        QString bandMode = isFt2LinkApplicationMode(normalizedMode)
            ? QStringLiteral("FT2")
            : normalizedMode;
        // Q65 period/submode variants share the same band-plan frequency;
        // only decoder timing and TX tone spacing differ.
        if (bandMode.startsWith(QStringLiteral("Q65-"))) {
            bandMode = QStringLiteral("Q65");
        }
        m_bandManager->setCurrentMode(bandMode);
        // SSB and CW are operating modes, not single-frequency protocols.
        // Entering either workspace must therefore keep the operator's dial
        // exactly where it is.  The CW calling/QRP frequencies remain useful
        // when the operator explicitly changes band while already in CW, but
        // selecting CW itself must never QSY to one of them.
        bool const preserveOperatorDial = m_preserveFrequencyOnModeChange
            || normalizedMode == QStringLiteral("SSB")
            || normalizedMode == QStringLiteral("CW");
        if (preserveOperatorDial) {
            m_bandManager->updateFromFrequency(m_frequency);
        } else {
            // updateForMode emits bandFrequencyRequested(), whose sole bridge
            // handler owns the QSY.  Reissuing it here produced a second CAT
            // transaction with an already-overwritten previous frequency.
            m_bandManager->updateForMode(bandMode);
        }
        // Riavvia la cattura se attiva per adattare il periodo
        if (m_monitoring) {
            resetRxPeriodAccumulation(true);
            // Avvia/ferma async timer in base al modo
            if (m_mode == "FT2") {
                // store(release): il writer (callback audio) usa CAS sul commit
                // di m_asyncAudioPos, quindi questo reset annulla correttamente
                // eventuali increment in volo — il CAS del writer fallirà e il
                // campione verrà scartato.
                m_asyncAudioPos.store(0, std::memory_order_release);
                m_asyncDecodePending = false;
                m_asyncDecodeTimer->start();
            } else {
                m_asyncDecodeTimer->stop();
                m_asyncDecodePending = false;
            }
            if ((rearmModernMonitor || legacyToBridgeRxHandoff)
                && !nativePcmToLegacyRxHandoff) {
                restartAudioCaptureForModeChange(previousMode);
                armPeriodTimerForCurrentMode(monitorSessionId, QStringLiteral("setMode"));
            }
        }
        clearDecodeWindowsForModeChange(previousMode, m_mode);
        resetFtxDecodeWorkersForModeChange(previousMode, m_mode);

        emit periodMillisecondsChanged();
        emit modeChanged();
        if (legacyBackendAvailable()
            && !bridgeOwnedExplicitQ65
            && !isFt2LinkApplicationMode(normalizedMode)
            && !isStreamingKeyboardMode(normalizedMode)) {
            // setMode() causes the embedded widget backend to regenerate CQ
            // synchronously.  Refresh the profile identity immediately before
            // that call, even if an earlier state sync has not run yet.
            m_legacyBackend->setStationIdentity(m_callsign, m_grid);
            m_legacyBackend->setFt2DecodeEnabled(normalizedMode == QStringLiteral("FT2"));
            QString const legacyMode = normalizedMode;
            m_legacyBackend->setMode(legacyMode);
            applyAutoCqBurstCadenceToLegacyBackend();
            m_legacyStartupModeGuard = legacyMode.trimmed();
            m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 6000;
        } else if (legacyBackendAvailable()
                   && !bridgeOwnedExplicitQ65
                   && !isStreamingKeyboardMode(normalizedMode)) {
            m_legacyBackend->setStationIdentity(m_callsign, m_grid);
            m_legacyBackend->setFt2DecodeEnabled(false);
            m_legacyBackend->setMode(QStringLiteral("FT2"));
            applyAutoCqBurstCadenceToLegacyBackend();
            m_legacyStartupModeGuard = QStringLiteral("FT2");
            m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 6000;
        }
        if (nativePcmToLegacyRxHandoff) {
            completeNativePcmToLegacyRxHandoff(previousMode);
        }
        if (!m_callsign.trimmed().isEmpty()) {
            regenerateTxMessages();
        }
        QString const configuredRigMode = configuredCatRigMode();
        QString const rttyRestoreMode = m_rttyRigModeState.restoreTarget(
            rttyCatContext(), configuredRigMode, QDateTime::currentMSecsSinceEpoch());
        QString cwExitRestoreMode;
        QString cwExitVerifyMode;
        if (leavingCw || leavingReportedRigCw) {
            if (leavingCw)
                cwExitRestoreMode = m_catModeBeforeCw.trimmed().toUpper();
            if (cwExitRestoreMode.isEmpty()) {
                cwExitRestoreMode = normalizedMode == QStringLiteral("SSB")
                    ? m_ssbRadioMode.trimmed().toUpper()
                    : configuredRigMode;
            }
            // CW can have been adopted from the radio during CAT startup,
            // before there was an application-side CW entry from which to
            // take a CAT-mode snapshot.  Keep an explicit target for the
            // delayed verification in that case too: an old CW state reply
            // must not overwrite the first DATA/USB request.
            cwExitVerifyMode = cwExitRestoreMode;
            m_catModeBeforeCw.clear();
            bridgeLog(QStringLiteral("CW exit CAT restore: target=%1 applicationMode=%2 reportedBefore=%3")
                          .arg(cwExitRestoreMode.isEmpty()
                                   ? QStringLiteral("none") : cwExitRestoreMode,
                               normalizedMode,
                               reportedCatModeBeforeChange));
        }
        if (!cwExitRestoreMode.isEmpty()) {
            applyConfiguredCatRigMode(QStringLiteral("mode-change/cw-exit"),
                                       cwExitRestoreMode);
        } else if (normalizedMode == QStringLiteral("SSB")
            || normalizedMode == QStringLiteral("CW")
            || !configuredRigMode.isEmpty() || !rttyRestoreMode.isEmpty()) {
            applyConfiguredCatRigMode(QStringLiteral("mode-change"));
        } else if (m_catBackend == QStringLiteral("tci")
            && m_hamlibCat
            && m_hamlibCat->connected()
            && !isFt2LinkApplicationMode(normalizedMode)
            && !canonicalApplicationDecodeMode(normalizedMode).isEmpty()) {
            m_hamlibCat->setRigMode(normalizedMode);
        }
        if (!cwExitVerifyMode.isEmpty()) {
            // The first state report after opening a rig already left in CW
            // can arrive after the user's first mode selection. It restores
            // the manager's desired state to CW, even though the change to
            // DATA-U/USB was queued correctly. Reassert twice after the CAT
            // queue has drained; this is deliberately PTT-safe because
            // setRigMode() always writes desired PTT=false.
            static constexpr int kCwExitVerifyDelaysMs[] = {700, 1700};
            for (int const delayMs : kCwExitVerifyDelaysMs) {
                QTimer::singleShot(delayMs, this,
                                   [this, rttyRigGeneration, cwExitVerifyMode, delayMs]() {
                    if (rttyRigGeneration != m_rttyRigModeGeneration
                        || m_mode == QStringLiteral("CW")
                        || m_transmitting || m_tuning
                        || !m_catConnected) {
                        return;
                    }
                    if (m_catMode.compare(cwExitVerifyMode, Qt::CaseInsensitive) == 0) {
                        bridgeLog(QStringLiteral("CW exit CAT verify after %1 ms: mode confirmed %2")
                                      .arg(delayMs).arg(cwExitVerifyMode));
                        return;
                    }
                    bridgeLog(QStringLiteral("CW exit CAT verify after %1 ms: reported=%2, reasserting %3")
                                  .arg(delayMs).arg(m_catMode, cwExitVerifyMode));
                    applyConfiguredCatRigMode(QStringLiteral("mode-change/cw-exit-verify"),
                                               cwExitVerifyMode);
                });
            }
        }
        syncActiveCatTxSplitFrequency(QStringLiteral("mode-change"));
        quint64 const recoverySessionId = nativePcmToLegacyRxHandoff
            ? m_periodTimerSessionId
            : monitorSessionId;
        scheduleModeChangeMonitorRecovery(previousMode, m_mode, recoverySessionId, monitorShouldStayActive);
        // Schedule after the backend hand-off, even if its monitor flag is
        // temporarily false. UTC/NTP rearming must not cancel audio recovery.
        if (previousMode == QStringLiteral("RTTY") && monitorShouldStayActive)
            scheduleRttyExitRxRecovery();
        if (!rttyRestoreMode.isEmpty()) {
            bridgeLog(QStringLiteral("RTTY exit CAT restore: scheduled previous=%1 reported=%2")
                          .arg(rttyRestoreMode, m_catMode));
            // The immediate request can be deferred by a band QSY. Also
            // cover mode changes which do not emit a post-QSY callback.
            QTimer::singleShot(1200, this, [this, rttyRigGeneration]() {
                if (rttyRigGeneration != m_rttyRigModeGeneration) return;
                applyConfiguredCatRigMode(QStringLiteral("rtty-exit/settled"));
            });
            QTimer::singleShot(4500, this, [this, rttyRigGeneration, rttyRestoreMode]() {
                if (rttyRigGeneration != m_rttyRigModeGeneration) return;
                const auto target = m_rttyRigModeState.restoreTarget(
                    rttyCatContext(), configuredCatRigMode(), QDateTime::currentMSecsSinceEpoch());
                bridgeLog(QStringLiteral("RTTY exit CAT restore check: requested=%1 reported=%2 result=%3")
                              .arg(rttyRestoreMode, m_catMode,
                                   target.isEmpty() ? QStringLiteral("cancelled/context-or-policy-changed")
                                   : (m_catMode.compare(target, Qt::CaseInsensitive) == 0
                                      ? QStringLiteral("reported-match") : QStringLiteral("not-confirmed"))));
                m_rttyRigModeState.clear();
            });
        }
        scheduleTxAudioPrecompute();
    }
}

// === RX/TX STATE ===
