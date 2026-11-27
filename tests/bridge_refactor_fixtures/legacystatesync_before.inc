void DecodiumBridge::syncLegacyBackendState()
{
    if (rtlSdrEnabled()) {
        return;
    }
    if (!usingLegacyBackendForRx() && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
        return;
    }
    if (m_syncingLegacyBackendState) {
        return;
    }
    QScopedValueRollback<bool> legacyStateGuard(m_syncingLegacyBackendState, true);
    bool const rttyApplicationMode = isStreamingKeyboardMode(m_mode);
    bool const bridgeOwnedExplicitQ65 = m_mode.trimmed().toUpper().startsWith(QStringLiteral("Q65-"));
#if DECODIUM_HAS_SSTV
    // The legacy state poll can otherwise reassert its saved MONITOR state a
    // few hundred milliseconds after the SSTV workspace switched to the
    // visual-only native capture. Keep CAT/telemetry synchronisation alive,
    // but make the hidden decoder unconditionally quiescent.
    if ((m_sstvWorkspaceActive || rttyApplicationMode || bridgeOwnedExplicitQ65)
        && m_legacyBackend->monitoring()) {
        m_legacyBackend->setMonitoring(false);
    }
#else
    if ((rttyApplicationMode || bridgeOwnedExplicitQ65) && m_legacyBackend->monitoring()) {
        m_legacyBackend->setMonitoring(false);
    }
#endif
    if (m_mode == QStringLiteral("FT8") && m_monitoring) {
        int const periodMs = effectivePeriodMsForMode(m_mode);
        if (periodMs > 0) {
            noteFt8AdaptivePeriod(correctedUtcEpochMs() / periodMs);
        }
    }
    MainThreadTraceScope trace(QStringLiteral("sync_legacy_backend_state"),
                               QStringLiteral("mode=%1 monitoring=%2 tx=%3 tune=%4 bridge_audio=%5")
                                   .arg(m_mode)
                                   .arg(m_monitoring ? 1 : 0)
                                   .arg(m_transmitting ? 1 : 0)
                                   .arg(m_tuning ? 1 : 0)
                                   .arg((m_bridgeAudioLegacyTxActive || m_bridgeAudioTuneActive) ? 1 : 0));

    auto updateBool = [] (bool& target, bool value, auto signal) {
        if (target != value) {
            target = value;
            signal();
        }
    };
    auto updateInt = [] (int& target, int value, auto signal) {
        if (target != value) {
            target = value;
            signal();
        }
    };
    auto updateString = [] (QString& target, const QString& value, auto signal) {
        if (target != value) {
            target = value;
            signal();
        }
    };
    auto updateDouble = [] (double& target, double value, auto signal) {
        if (!qFuzzyCompare(target + 1.0, value + 1.0)) {
            target = value;
            signal();
        }
    };
    auto updateTxMessage = [] (QString& target, QString const& value, auto changeSignal, auto extraSignals) {
        QString const normalized = value.trimmed().toUpper();
        if (target != normalized) {
            target = normalized;
            changeSignal();
            extraSignals();
        }
    };
    m_legacyBackend->setEmbeddedUiUpdatesEnabled(!isFt2LinkApplicationMode(m_mode));
#if defined(Q_OS_WIN)
    bool const allowAudioDeviceProbe =
        m_mainQmlReady || qEnvironmentVariableIsSet("DECODIUM_STARTUP_AUDIO_DIAGNOSTICS");
#else
    bool const allowAudioDeviceProbe = true;
#endif

    auto const app = QCoreApplication::instance();
    QString const labCallsign = app
        ? app->property("decodiumLabCallsign").toString().trimmed().toUpper()
        : QString {};
    QString const labGrid = app
        ? app->property("decodiumLabGrid").toString().trimmed().toUpper()
        : QString {};
    bool const activeProfileRequested =
        !decodium::activeSettingsProfileName().isEmpty();
    bool const labStationIdentityOverrideActive =
        !labCallsign.isEmpty() || !labGrid.isEmpty();
    bool labStationIdentityChanged = false;
    if (!labCallsign.isEmpty()) {
        if (m_callsign != labCallsign) {
            updateString(m_callsign, labCallsign, [this]() { emit callsignChanged(); });
            labStationIdentityChanged = true;
        }
    } else if (!activeProfileRequested && m_callsign.trimmed().isEmpty()) {
        QString const legacyCallsign = m_legacyBackend->callsign();
        if (!legacyCallsign.isEmpty()) {
            updateString(m_callsign, legacyCallsign, [this]() { emit callsignChanged(); });
        }
    }
    if (!labGrid.isEmpty()) {
        if (m_grid != labGrid) {
            updateString(m_grid, labGrid, [this]() { emit gridChanged(); });
            labStationIdentityChanged = true;
        }
    } else if (!activeProfileRequested && m_grid.trimmed().isEmpty()) {
        QString const legacyGrid = m_legacyBackend->grid();
        if (!legacyGrid.isEmpty()) {
            updateString(m_grid, legacyGrid, [this]() { emit gridChanged(); });
        }
    }
    if (labStationIdentityOverrideActive) {
        QString const normalizedMode = m_mode.trimmed().toUpper();
        QString const grid4 = m_grid.left(4).toUpper();
        bool const wsprLikeBeaconMode = normalizedMode == QStringLiteral("WSPR")
            || normalizedMode.startsWith(QStringLiteral("FST4W"));
        QString const expectedTx6 = wsprLikeBeaconMode
            ? (m_callsign.trimmed().toUpper()
               + (grid4.isEmpty() ? QString() : QStringLiteral(" ") + grid4)
               + QStringLiteral(" ")
               + QString::number(normalizeWsprPowerDbm(m_wsprPowerDbm)))
            : standardFtxCqMessage(m_callsign, grid4, normalizedMode);
        bool const staleLabTxMessages =
            !expectedTx6.isEmpty()
            && m_tx6.trimmed().compare(expectedTx6, Qt::CaseInsensitive) != 0;
        bool const staleLegacyLabTxMessages =
            !expectedTx6.isEmpty()
            && m_legacyBackend->txMessage(6).trimmed().compare(
                   expectedTx6, Qt::CaseInsensitive) != 0;
        if (labStationIdentityChanged || staleLabTxMessages || staleLegacyLabTxMessages) {
            refreshPskReporterLocalStation();
            if (m_dxCluster) {
                m_dxCluster->setCallsign(m_callsign);
            }
            regenerateTxMessages();
            syncLegacyBackendTxState();
            bridgeLog(QStringLiteral("lab station identity synced: call=%1 grid=%2 tx6=%3 bridgeStale=%4 legacyStale=%5")
                          .arg(m_callsign,
                               m_grid,
                               m_tx6,
                               staleLabTxMessages ? QStringLiteral("1") : QStringLiteral("0"),
                               staleLegacyLabTxMessages ? QStringLiteral("1") : QStringLiteral("0")));
        }
    }

    bool const monitorBeforeLegacySync = m_monitoring;
    bool const wasTransmittingBeforeLegacySync = m_transmitting;
    bool const legacyTransmittingNow = m_legacyBackend->transmitting();
    bool const legacyTxOrTune = legacyTransmittingNow
        || m_legacyBackend->tuning()
        || m_bridgeAudioLegacyTxActive
        || m_bridgeAudioTuneActive;
    bool const legacyMonitoringStartPending = m_legacyBackend->monitoringStartPending();
    // Native analog SSTV deliberately quiesces the embedded legacy backend
    // while keeping Decodium's monitor alive through the dedicated SoundInput.
    // Treat that ownership as active here; otherwise this synchronisation poll
    // mistakes the intentional legacy=false state for a dropped monitor and
    // schedules startRx() repeatedly, starving the panadapter.
    bool const nativeSstvMonitoring =
#if DECODIUM_HAS_SSTV
        m_sstvWorkspaceActive ||
#endif
        nativeSstvRxForcesDedicatedAudioCapture();
    // RTTY and explicit Q65 variants own the modern PCM capture. The legacy
    // monitor is intentionally false in these states.
    bool const nativeBridgeMonitoring = (rttyApplicationMode || bridgeOwnedExplicitQ65)
        && m_monitorRequested;
    bool const ft2LinkLegacyRxRequested =
        isFt2LinkApplicationMode(m_mode)
        && m_monitorRequested
        && usingLegacyBackendForRx();
    qint64 const syncNowMs = QDateTime::currentMSecsSinceEpoch();
    bool const startupMonitorGrace = m_monitorRequested
        && syncNowMs < m_monitorStartupGraceUntilMs
        && !legacyTxOrTune;
    bool const effectiveLegacyMonitoring = m_legacyBackend->monitoring()
        || nativeSstvMonitoring
        || nativeBridgeMonitoring
        || ft2LinkLegacyRxRequested
        || startupMonitorGrace
        || (m_monitorRequested
            && usingLegacyBackendForRx()
            && (legacyMonitoringStartPending || legacyTxOrTune));
    updateBool(m_monitoring, effectiveLegacyMonitoring, [this]() { emit monitoringChanged(); });
    if (m_monitorRequested && monitorBeforeLegacySync && !m_monitoring) {
        scheduleMonitorRecovery(QStringLiteral("legacy state sync"),
                                m_periodTimerSessionId,
                                true);
    }
    bool const dashboardDecoderActive = m_monitoring
#if DECODIUM_HAS_SSTV
        && !m_sstvWorkspaceActive
#endif
        && !rttyApplicationMode
        ;
    updateBool(m_decoding, dashboardDecoderActive,
               [this]() { emit decodingChanged(); });
    qint64 const txStateNowMs = QDateTime::currentMSecsSinceEpoch();
    bool const bridgeAudioPlaybackTxActive =
        m_transmitting
        && (m_txPlaybackHoldUntilMs > txStateNowMs
            || m_txPlaybackHardDeadlineMs > txStateNowMs
            || (m_txPcmBuffer && m_txPcmBuffer->isOpen()
                && m_txPcmBuffer->pos() < m_txPcmBuffer->size()));
    bool const bridgeOwnedCustomTxActive =
        m_bridgeAudioLegacyTxActive
        || m_ft2LinkTxActive
        || m_cwTxActive
        || m_telemetryTxActive;
    bool const bridgeManagedLegacyAudio = shouldUseBridgeAudioForLegacyDigitalTx();
    bool const authoritativeLegacyTransmitting =
        decodium::tx::legacyReportedTxIsAuthoritative(bridgeManagedLegacyAudio)
        && legacyTransmittingNow;
    bool const effectiveLegacyTransmitting =
        authoritativeLegacyTransmitting
        || bridgeOwnedCustomTxActive
        || bridgeAudioPlaybackTxActive;
    updateBool(m_transmitting, effectiveLegacyTransmitting, [this]() { emit transmittingChanged(); });
    bool const legacyTxStartedForMirror =
        usingLegacyBackendForTx()
        && legacyTransmittingNow
        && !wasTransmittingBeforeLegacySync
        && !shouldUseBridgeAudioForLegacyDigitalTx();
    if (!m_bridgeAudioTuneActive) {
        updateBool(m_tuning, m_legacyBackend->tuning(), [this]() { emit tuningChanged(); });
    }
    if (usingLegacyBackendForTx()) {
        bool legacyTxEnabled = m_legacyBackend->txEnabled();
        // In native MAM the bridge owns the TX state between UTC slots. The
        // embedded backend reports TX disabled while it is merely idle; do not
        // mirror that idle state back into m_txEnabled or MAM's dispatcher is
        // gated off and the legacy mono AutoCQ sequence starts again.
        bool const bridgeOwnsMamTx = m_mamMultiStream
            && isMamMultiStreamMode()
            && (m_autoCqRepeat || m_multiAnswerMode)
            && shouldUseBridgeAudioForLegacyDigitalTx();
        if (bridgeOwnsMamTx) {
            if (legacyTxEnabled) {
                m_legacyBackend->setTxEnabled(false);
                legacyTxEnabled = false;
                bridgeLog(QStringLiteral("syncLegacyBackendState: MAM bridge owns TX; legacy mono TX disarmed"));
            }
            if (!m_txEnabled && !m_manualTxHold) {
                m_txEnabled = true;
                emit txEnabledChanged();
                bridgeLog(QStringLiteral("syncLegacyBackendState: MAM retained bridge TX enable across legacy idle slot"));
            }
        } else if (m_autoCqRepeat && !m_manualTxHold && !legacyTxEnabled) {
            bridgeLog(QStringLiteral("syncLegacyBackendState: legacy TX enable off while AutoCQ active -> rearm TX"));
            m_legacyBackend->setTxEnabled(true);
            legacyTxEnabled = true;
        }
        if (m_mapRosterCallPending && m_txEnabled && !legacyTxEnabled) {
            // A roster CALL for a synchronized mode is armed in the bridge
            // before it is forwarded to the legacy backend at the next full
            // slot. Do not interpret the backend's expected idle state as a
            // user cancellation while that request is pending.
            legacyTxEnabled = true;
        }
        if (!bridgeOwnsMamTx && m_txEnabled != legacyTxEnabled) {
            bridgeLog(QStringLiteral("syncLegacyBackendState: legacy TX enable -> bridge %1")
                          .arg(legacyTxEnabled ? QStringLiteral("on") : QStringLiteral("off")));
            if (!legacyTxEnabled) {
                clearDeferredManualSyncTx(QStringLiteral("legacy-tx-enable-off"));
            }
            m_txEnabled = legacyTxEnabled;
            emit txEnabledChanged();
        }
    }
    if (m_legacyBackend->holdTxFreq() != m_holdTxFreq) {
        m_legacyBackend->setHoldTxFreq(m_holdTxFreq);
    }

    bool legacyRigFrequencyAuthoritative = false;
    if (m_legacyBackend->rigControlEnabled()) {
        QString const legacyRigName = m_legacyBackend->rigName().trimmed();
        bool legacyCatConnected = m_legacyBackend->catConnected();
        if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && !legacyCatConnected) {
            legacyCatConnected = !legacyRigName.isEmpty();
        }
        legacyRigFrequencyAuthoritative = legacyCatConnected;
        updateBool(m_catConnected, legacyCatConnected, [this]() { emit catConnectedChanged(); });
        updateString(m_catRigName, m_catConnected ? legacyRigName : QString {}, [this]() { emit catRigNameChanged(); });
        refreshPskReporterLocalStation();
    }
    double const legacySignalLevel = m_legacyBackend->signalLevel();
    bool const legacyOwnsRxLevels = usesLegacyRxLevels();
    updateDouble(m_sMeter, legacyOwnsRxLevels ? legacySignalLevel : 0.0,
                 [this]() { emit sMeterChanged(); });
    if (m_autoRxInputLevel
        && m_monitoring
        && legacyOwnsRxLevels
        && legacySignalLevel > 0.0) {
        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        if (now >= m_autoRxLevelManualHoldUntilMs) {
            constexpr double kLegacyHighDb = 78.0;
            constexpr double kLegacyTargetDb = 62.0;
            constexpr double kLegacyLowDb = 32.0;
            constexpr double kLegacyUsefulDb = 4.0;
            constexpr qint64 kLegacyDownCooldownMs = 1200;
            constexpr qint64 kLegacyUpCooldownMs = 3500;
            constexpr qint64 kLegacyLowHoldMs = 4000;
            constexpr double kMinGain = 0.05;
            double const currentGain = rxInputGainFromLevel(m_rxInputLevel);
            double const maxGain = rxInputGainFromLevel(kAutoRxInputMaxLevel);

            if (legacySignalLevel >= kLegacyHighDb) {
                m_autoRxLevelLowStartMs = 0;
                if (now - m_autoRxLevelLastAdjustMs >= kLegacyDownCooldownMs) {
                    double const targetGain = qBound(kMinGain,
                                                     currentGain * (kLegacyTargetDb / qMax(legacySignalLevel, 1.0)),
                                                     maxGain);
                    double const newLevel = rxInputLevelFromGain(targetGain);
                    if (newLevel < m_rxInputLevel - 0.5) {
                        applyRxInputLevel(newLevel, true);
                        m_autoRxLevelLastAdjustMs = now;
                        bridgeLog(QStringLiteral("Auto RX legacy level down: level=%1 sMeter=%2")
                                      .arg(m_rxInputLevel, 0, 'f', 1)
                                      .arg(legacySignalLevel, 0, 'f', 1));
                    }
                }
            } else if (legacySignalLevel >= kLegacyUsefulDb
                       && legacySignalLevel <= kLegacyLowDb
                       && m_rxInputLevel < kAutoRxInputMaxLevel - 0.5) {
                if (m_autoRxLevelLowStartMs == 0) {
                    m_autoRxLevelLowStartMs = now;
                } else if (now - m_autoRxLevelLowStartMs >= kLegacyLowHoldMs
                           && now - m_autoRxLevelLastAdjustMs >= kLegacyUpCooldownMs) {
                    double const targetGain = qBound(kMinGain, currentGain * 1.10, maxGain);
                    double const newLevel = qMin(kAutoRxInputMaxLevel, rxInputLevelFromGain(targetGain));
                    if (newLevel > m_rxInputLevel + 0.5) {
                        applyRxInputLevel(newLevel, true);
                        m_autoRxLevelLastAdjustMs = now;
                        bridgeLog(QStringLiteral("Auto RX legacy level up: level=%1 sMeter=%2")
                                      .arg(m_rxInputLevel, 0, 'f', 1)
                                      .arg(legacySignalLevel, 0, 'f', 1));
                    }
                }
            } else {
                m_autoRxLevelLowStartMs = 0;
            }
        }
    }
    if (legacyOwnsRxLevels) {
        // When the embedded legacy backend owns RX audio, the modern
        // capture is stopped but its SoundInput object may still exist.
        // Feed the QML LVL meter according to RX ownership, not that pointer,
        // from the legacy S-meter so the header indicator works on macOS and
        // other platforms that run in legacy audio mode.
        double const legacyAudioLevel = m_monitoring
            ? qBound(0.0, legacySignalLevel / 250.0, 1.0)
            : 0.0;
        updateDouble(m_audioLevel, legacyAudioLevel, [this]() { emit audioLevelChanged(); });
    }
    updateDouble(m_txOutputLevel, txLevelFromLegacyAttenuation(m_legacyBackend->txOutputAttenuation()),
                 [this]() { emit txOutputLevelChanged(); });

    QString const legacyModeRaw = m_legacyBackend->mode().trimmed();
    QString const legacyMode = canonicalApplicationDecodeMode(legacyModeRaw);
    if (!legacyModeRaw.isEmpty() && legacyMode.isEmpty()) {
        // The legacy/CAT side can expose the radio modulation mode (USB, DATA-U,
        // FSK-R, ...). That is not a Decodium decoder mode and must not replace
        // m_mode, otherwise FT8/FT4 can show a live waterfall but never decode.
        if (m_catMode.compare(legacyModeRaw, Qt::CaseInsensitive) != 0) {
            m_catMode = legacyModeRaw;
            emit catModeChanged();
        }
    }
    if (!legacyMode.isEmpty() && rttyApplicationMode) {
        // The legacy backend has retained its last digital decoder mode.  It
        // is valid for its own CAT implementation, but must never replace the
        // active RTTY application mode or re-enable dashboard decoding.
        static qint64 s_lastRttyLegacyModeIgnoreLogMs = 0;
        qint64 const logNowMs = QDateTime::currentMSecsSinceEpoch();
        if (logNowMs - s_lastRttyLegacyModeIgnoreLogMs >= 10000) {
            s_lastRttyLegacyModeIgnoreLogMs = logNowMs;
            bridgeLog(QStringLiteral("RTTY: ignoring retained legacy decoder mode %1; RTTY remains exclusive")
                          .arg(legacyModeRaw));
        }
    } else if (!legacyMode.isEmpty() && bridgeOwnedExplicitQ65) {
        // Do not let the retained legacy FT8/Q65 label replace Q65-15A ...
        // Q65-300E. Its unqualified "Q65" has no submode information and
        // would silently put the bridge back on the wrong ownership path.
        static qint64 s_lastQ65LegacyModeIgnoreLogMs = 0;
        qint64 const logNowMs = QDateTime::currentMSecsSinceEpoch();
        if (logNowMs - s_lastQ65LegacyModeIgnoreLogMs >= 10000) {
            s_lastQ65LegacyModeIgnoreLogMs = logNowMs;
            bridgeLog(QStringLiteral("Q65: ignoring retained legacy decoder mode %1; keeping application mode %2")
                          .arg(legacyModeRaw, m_mode));
        }
    } else if (!legacyMode.isEmpty() && isFt2LinkApplicationMode(m_mode)) {
        // FT2-Link intentionally keeps the legacy/radio side on the FT2 base
        // mode, but the application mode must remain FT2-Link.  The legacy
        // state poll can otherwise overwrite m_mode back to FT2 and re-enable
        // the legacy FT2 decoder while the FT2-Link UI/RX path is active.
        if (m_bandManager) {
            m_bandManager->setCurrentMode(QStringLiteral("FT2"));
        }
        if (m_ft2Worker) {
            m_ft2Worker->setDecodeEnabled(false);
        }
        if (m_legacyBackend) {
            m_legacyBackend->setFt2DecodeEnabled(false);
        }
        static qint64 s_lastFt2LinkLegacyModeIgnoreLogMs = 0;
        qint64 const logNowMs = QDateTime::currentMSecsSinceEpoch();
        if (logNowMs - s_lastFt2LinkLegacyModeIgnoreLogMs >= 10000) {
            s_lastFt2LinkLegacyModeIgnoreLogMs = logNowMs;
            bridgeLog(QStringLiteral("FT2-Link: ignoring legacy backend mode %1; keeping application mode %2")
                          .arg(legacyModeRaw, m_mode));
        }
    } else if (!legacyMode.isEmpty()) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_legacyStartupModeGuardUntilMs > 0 && nowMs > m_legacyStartupModeGuardUntilMs) {
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
        }
        bool const legacyStartupModeGuardActive =
            !m_legacyStartupModeGuard.isEmpty()
            && m_legacyStartupModeGuardUntilMs > 0
            && nowMs <= m_legacyStartupModeGuardUntilMs;
        if (legacyStartupModeGuardActive
            && legacyMode.compare(m_legacyStartupModeGuard, Qt::CaseInsensitive) != 0) {
            bridgeLog(QStringLiteral("legacy startup mode guard: ignoring backend mode %1, keeping %2")
                          .arg(legacyModeRaw, m_legacyStartupModeGuard));
            m_legacyBackend->setMode(m_legacyStartupModeGuard);
            return;
        }
        if (legacyStartupModeGuardActive
            && legacyMode.compare(m_legacyStartupModeGuard, Qt::CaseInsensitive) == 0) {
            m_legacyStartupModeGuard.clear();
            m_legacyStartupModeGuardUntilMs = 0;
        }
        if (m_bandManager) {
            m_bandManager->setCurrentMode(legacyMode);
        }
        QString const previousMode = m_mode;
        bool legacyModeChanged = false;
        if (m_mode != legacyMode) {
            m_mode = legacyMode;
            if (m_ft2Worker) {
                m_ft2Worker->setDecodeEnabled(m_mode == QStringLiteral("FT2"));
            }
            if (m_legacyBackend) {
                m_legacyBackend->setFt2DecodeEnabled(m_mode == QStringLiteral("FT2"));
            }
            legacyModeChanged = true;
        }
        updatePeriodTicksMax();
        if (legacyMode == "FT2") {
            if (!m_autoSeq)            { m_autoSeq = true;            emit autoSeqChanged(); }
            if (!m_asyncTxEnabled)     { m_asyncTxEnabled = true;     emit asyncTxEnabledChanged(); }
            if (!m_asyncDecodeEnabled) { m_asyncDecodeEnabled = true; emit asyncDecodeEnabledChanged(); }
            m_qsoCooldown.clear();
        } else {
            if (m_asyncDecodeEnabled) {
                m_asyncDecodeEnabled = false;
                emit asyncDecodeEnabledChanged();
            }
        }
        if (legacyModeChanged) {
            clearDecodeWindowsForModeChange(previousMode, legacyMode);
            emit periodMillisecondsChanged();
            emit modeChanged();
        }
    }

    int const publishedPeriodMs = effectivePeriodMsForMode(m_mode);
    if (publishedPeriodMs != m_lastPublishedPeriodMilliseconds) {
        int const previousPeriodMs = m_lastPublishedPeriodMilliseconds;
        m_lastPublishedPeriodMilliseconds = publishedPeriodMs;
        m_periodTicks = 0;
        m_lastPeriodSlot = -1;
        updatePeriodTicksMax();
        emit periodMillisecondsChanged();
        bridgeLog(QStringLiteral("Mode period updated: mode=%1 period=%2ms previous=%3ms")
                      .arg(m_mode)
                      .arg(publishedPeriodMs)
                      .arg(previousPeriodMs));
    }

    double const legacyDialFrequency = m_legacyBackend->dialFrequency();
    if (legacyDialFrequency > 0.0) {
        bool const catOwnsDialFrequency =
            m_catConnected && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend);
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        bool const localDialGuardActive =
            m_localCatFrequencyTargetHz > 0.0
            && m_localCatFrequencyGuardUntilMs > 0
            && nowMs <= m_localCatFrequencyGuardUntilMs;
        if (localDialGuardActive) {
            if (!qFuzzyCompare(m_frequency + 1.0, m_localCatFrequencyTargetHz + 1.0)) {
                m_frequency = m_localCatFrequencyTargetHz;
                emit frequencyChanged();
            }
            if (std::abs(legacyDialFrequency - m_localCatFrequencyTargetHz) > 1.0) {
                m_legacyBackend->setDialFrequency(m_localCatFrequencyTargetHz);
            }
            if (m_bandManager) {
                m_bandManager->updateFromFrequency(m_localCatFrequencyTargetHz);
            }
        } else if (catOwnsDialFrequency) {
            if (m_frequency > 0.0
                && !qFuzzyCompare(m_frequency + 1.0, legacyDialFrequency + 1.0)) {
                bridgeLog(QStringLiteral("legacy dial ignored while CAT authoritative: legacy=%1 bridge=%2")
                              .arg(QString::number(legacyDialFrequency, 'f', 0),
                                   QString::number(m_frequency, 'f', 0)));
                m_legacyBackend->setDialFrequency(m_frequency);
            }
        } else {
            if (!qFuzzyCompare(m_frequency + 1.0, legacyDialFrequency + 1.0)) {
                double const previousFrequency = m_frequency;
                m_frequency = legacyDialFrequency;
                emit frequencyChanged();
                clearDecodeWindowsForBandChange(previousFrequency,
                                                legacyDialFrequency,
                                                QStringLiteral("legacy-dial"));
            }
            if (m_bandManager) {
                m_bandManager->updateFromFrequency(legacyDialFrequency);
            }
            maybeApplyStartupModeFromRigFrequency(legacyDialFrequency, legacyRigFrequencyAuthoritative);
        }
    }

    int const legacyRxFrequency = qBound(0, m_legacyBackend->rxFrequency(), 5000);
    if (shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(legacyRxFrequency, false)) {
        m_legacyBackend->setRxFrequency(m_localRxFrequencyTargetHz);
    } else {
        updateInt(m_rxFrequency, legacyRxFrequency, [this]() { emit rxFrequencyChanged(); });
    }
    int const legacyTxFrequency = qBound(0, m_legacyBackend->txFrequency(), 5000);
    if (shouldIgnoreLegacyAudioFrequencyDuringLocalQsy(legacyTxFrequency, true)) {
        m_legacyBackend->setTxFrequency(m_localTxFrequencyTargetHz);
    } else {
        updateInt(m_txFrequency, legacyTxFrequency, [this]() { emit txFrequencyChanged(); });
    }
    updateInt(m_txPeriod, m_legacyBackend->txFirst() ? 1 : 0, [this]() { emit txPeriodChanged(); });
    updateBool(m_alt12Enabled, m_legacyBackend->alt12Enabled(), [this]() { emit alt12EnabledChanged(); });
    if (usingLegacyBackendForTx()) {
        updateSpecialOperationFromLegacy(m_legacyBackend->specialOperationActivity());
    }
    if (usingLegacyBackendForTx() && m_foxMode) {
        QStringList const legacyQueue = m_legacyBackend->foxCallerQueueLines();
        if (m_callerQueue != legacyQueue) {
            m_callerQueue = legacyQueue;
            emit callerQueueChanged();
        }
    } else if (usingLegacyBackendForTx() && !m_multiAnswerMode && !m_autoCqRepeat && !m_callerQueue.isEmpty()) {
        m_callerQueue.clear();
        emit callerQueueChanged();
    }
    // The bridge owns all six messages.  In particular, never pull a freshly
    // regenerated legacy CQ back into QML: the legacy backend has a separate
    // configuration store and formerly leaked its stale callsign here.
    bool const bridgeStationIdentityAuthoritative =
        labStationIdentityOverrideActive
        || !m_callsign.trimmed().isEmpty()
        || !m_grid.trimmed().isEmpty();
    if (!bridgeStationIdentityAuthoritative) {
        updateTxMessage(m_tx1, m_legacyBackend->txMessage(1),
                        [this]() { emit tx1Changed(); },
                        [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
        updateTxMessage(m_tx2, m_legacyBackend->txMessage(2),
                        [this]() { emit tx2Changed(); },
                        [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
        updateTxMessage(m_tx3, m_legacyBackend->txMessage(3),
                        [this]() { emit tx3Changed(); },
                        [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
        updateTxMessage(m_tx4, m_legacyBackend->txMessage(4),
                        [this]() { emit tx4Changed(); },
                        [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
        updateTxMessage(m_tx5, m_legacyBackend->txMessage(5),
                        [this]() { emit tx5Changed(); },
                        [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
        updateTxMessage(m_tx6, m_legacyBackend->txMessage(6),
                        [this]() { emit tx6Changed(); },
                        [this]() { emit txMessagesChanged(); emit currentTxMessageChanged(); });
        updateInt(m_currentTx, qBound(1, m_legacyBackend->currentTx(), 6),
                  [this]() { emit currentTxChanged(); emit currentTxMessageChanged(); });
    }
    if (legacyTxStartedForMirror) {
        appendTxDecodeEntry(buildCurrentTxMessage());
    }

    QString const legacyAudioInput = m_legacyBackend->audioInputDeviceName().trimmed();
    if (!legacyAudioInput.isEmpty()) {
        if (!allowAudioDeviceProbe) {
            if (!m_audioInputDevice.trimmed().isEmpty()
                && !audioDeviceNameExactMatches(legacyAudioInput, m_audioInputDevice)) {
                bridgeLog(QStringLiteral("syncLegacyBackendState: preserving bridge audio input [%1] over legacy [%2] without startup device probe")
                              .arg(m_audioInputDevice, legacyAudioInput));
                m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
                m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
            } else if (m_audioInputDevice.trimmed().isEmpty()) {
                m_audioInputDeviceId.clear();
                updateString(m_audioInputDevice, legacyAudioInput, [this]() {
                    emit audioInputDeviceChanged();
                    emit sstvRxSourceDeviceChanged();
                });
            }
        } else {
            QList<QAudioDevice> const inputs =
                cachedAudioInputs(QStringLiteral("syncLegacyBackendState input probe"), false);
            QAudioDevice bridgeInputDevice;
            QString bridgeInputReason;
            bool const bridgeInputAvailable =
                tryResolveAudioDeviceByIdentity(inputs,
                                                m_audioInputDevice,
                                                m_audioInputDeviceId,
                                                &bridgeInputDevice,
                                                &bridgeInputReason);
            bool const legacyInputAvailable =
                hasAudioDeviceNamed(inputs, legacyAudioInput);
            if (!m_audioInputDevice.trimmed().isEmpty()
                && bridgeInputAvailable
                && !audioDeviceNameExactMatches(legacyAudioInput, m_audioInputDevice)) {
                bridgeLog(QStringLiteral("syncLegacyBackendState: preserving bridge audio input [%1] over legacy [%2]")
                              .arg(m_audioInputDevice, legacyAudioInput));
                m_legacyBackend->setAudioInputDeviceName(m_audioInputDevice);
                m_legacyBackend->setAudioInputChannel(qBound(0, m_audioInputChannel, 3));
            } else if (legacyInputAvailable || m_audioInputDevice.trimmed().isEmpty()) {
                if (!audioDeviceNameExactMatches(m_audioInputDevice, legacyAudioInput)) {
                    m_audioInputDeviceId.clear();
                }
                updateString(m_audioInputDevice, legacyAudioInput, [this]() {
                    emit audioInputDeviceChanged();
                    emit sstvRxSourceDeviceChanged();
                });
            }
        }
    }
    QString const legacyAudioOutput = m_legacyBackend->audioOutputDeviceName().trimmed();
    if (!legacyAudioOutput.isEmpty()) {
        if (!allowAudioDeviceProbe) {
            if (!m_audioOutputDevice.trimmed().isEmpty()
                && !audioDeviceNameExactMatches(legacyAudioOutput, m_audioOutputDevice)) {
                bridgeLog(QStringLiteral("syncLegacyBackendState: preserving bridge audio output [%1] over legacy [%2] without startup device probe")
                              .arg(m_audioOutputDevice, legacyAudioOutput));
                m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
                m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
            } else if (m_audioOutputDevice.trimmed().isEmpty()) {
                m_audioOutputDeviceId.clear();
                updateString(m_audioOutputDevice, legacyAudioOutput, [this]() { emit audioOutputDeviceChanged(); });
            }
        } else {
            QList<QAudioDevice> const outputs =
                cachedAudioOutputs(QStringLiteral("syncLegacyBackendState output probe"), false);
            QAudioDevice bridgeOutputDevice;
            QString bridgeOutputReason;
            bool const bridgeOutputAvailable =
                tryResolveAudioDeviceByIdentity(outputs,
                                                m_audioOutputDevice,
                                                m_audioOutputDeviceId,
                                                &bridgeOutputDevice,
                                                &bridgeOutputReason);
            bool const legacyOutputAvailable =
                hasAudioDeviceNamed(outputs, legacyAudioOutput);
            if (!m_audioOutputDevice.trimmed().isEmpty()
                && bridgeOutputAvailable
                && !audioDeviceNameExactMatches(legacyAudioOutput, m_audioOutputDevice)) {
                bridgeLog(QStringLiteral("syncLegacyBackendState: preserving bridge audio output [%1] over legacy [%2]")
                              .arg(m_audioOutputDevice, legacyAudioOutput));
                m_legacyBackend->setAudioOutputDeviceName(m_audioOutputDevice);
                m_legacyBackend->setAudioOutputChannel(qBound(0, m_audioOutputChannel, 3));
            } else if (legacyOutputAvailable || m_audioOutputDevice.trimmed().isEmpty()) {
                if (!audioDeviceNameExactMatches(m_audioOutputDevice, legacyAudioOutput)) {
                    m_audioOutputDeviceId.clear();
                }
                updateString(m_audioOutputDevice, legacyAudioOutput, [this]() { emit audioOutputDeviceChanged(); });
            }
        }
    }
    updateInt(m_audioInputChannel, qBound(0, m_legacyBackend->audioInputChannel(), 3),
              [this]() { emit audioInputChannelChanged(); });
    updateInt(m_audioOutputChannel, qBound(0, m_legacyBackend->audioOutputChannel(), 3),
              [this]() { emit audioOutputChannelChanged(); });
    updateDouble(m_rxInputLevel, qBound(0.0, static_cast<double>(m_legacyBackend->rxInputLevel()), 100.0),
                 [this]() { emit rxInputLevelChanged(); });
    if (m_uiPaletteIndex <= 5) {
        m_legacyBackend->setWaterfallPalette(legacyPaletteNameForUiIndex(m_uiPaletteIndex));
    }

    trace.addDetail(QStringLiteral("after monitoring=%1 tx=%2 tune=%3")
                        .arg(m_monitoring ? 1 : 0)
                        .arg(m_transmitting ? 1 : 0)
                        .arg(m_tuning ? 1 : 0));
    syncLegacyBackendDecodeList();
}

