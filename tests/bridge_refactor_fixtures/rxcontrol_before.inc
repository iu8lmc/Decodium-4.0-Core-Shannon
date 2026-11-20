void DecodiumBridge::startRx()
{
    bridgeLog("startRx() called, m_monitoring=" + QString::number(m_monitoring) +
              " m_audioInputDevice=" + m_audioInputDevice +
              " m_audioInputDeviceId=" + audioDeviceIdSettingForLog(m_audioInputDeviceId));
    m_monitorRequested = true;
    // The embedded legacy monitor reports its state asynchronously. During
    // the first FT8 slot it may briefly say OFF while its worker attaches;
    // keep the dashboard visual pipeline alive through that transition.
    m_monitorStartupGraceUntilMs = QDateTime::currentMSecsSinceEpoch()
        + qMax<qint64>(effectivePeriodMsForMode(m_mode) * 2, 20000);
    if (m_monitoring) { bridgeLog("startRx: already monitoring, skip"); return; }
    ++m_decodeSessionId;
    resetNativeDecodeDedupIndex();
    resetFt2AsyncDecodeDedupIndex();
    m_ft8StartupPressureSlotDeferred = false;
    quint64 const monitorSessionId = ++m_periodTimerSessionId;

    if (!rtlSdrEnabled()
        && !isStreamingKeyboardMode(m_mode)
        && legacyTxBackendRequested()
        && !legacyBackendAvailable()
        && !ensureLegacyBackendAvailable()) {
        m_monitorRequested = false;
        emit errorMessage(QStringLiteral("Backend legacy non disponibile per Fox/Hound"));
        return;
    }

    if (usingLegacyBackendForRx()) {
        // Native SSTV owns the RX capture for the lifetime of its session.
        // Do not start the legacy backend first and then open a second
        // AudioQueue on the same BlackHole/USB endpoint: on macOS the second
        // queue can stay active while delivering no PCM at all.  SSTV still
        // keeps Decodium's monitor state coherent, but the legacy decoder is
        // deliberately quiesced until stopSstvRx() restores the normal path.
#if DECODIUM_HAS_SSTV
        if (m_sstvWorkspaceActive || nativeSstvRxForcesDedicatedAudioCapture()) {
            m_periodTimer->stop();
            m_asyncDecodeTimer->stop();
            m_asyncDecodePending = false;
            m_legacyPcmSpectrumFeed = false;
            if (m_legacyBackend && m_legacyBackend->monitoring()) {
                m_legacyBackend->setMonitoring(false);
            }
            m_monitoring = true;
            emit monitoringChanged();
            m_decoding = false;
            emit decodingChanged();
            resetRxPeriodAccumulation(true);
            m_spectrumBuf.clear();
            m_wfRingPos = 0;
            m_lastPanadapterData.clear();
            m_driftFrameCount = 0;
            m_driftExpectedFrames = 0;
            m_driftClock.restart();
            m_spectrumTimer->start();
            startAudioCapture();
            emit statusMessage(QStringLiteral("RX avviato via capture nativo SSTV - %1")
                                   .arg(m_mode));
            bridgeLog(QStringLiteral(
                "startRx: SSTV workspace exclusive capture active; legacy RX quiesced"));
            return;
        }
#endif
        bridgeLog("startRx: delegating monitoring to legacy backend");
        bool const bridgeOwnsMamTx = m_mamMultiStream
            && isMamMultiStreamMode()
            && (m_autoCqRepeat || m_multiAnswerMode)
            && shouldUseBridgeAudioForLegacyDigitalTx();
        if (!bridgeOwnsMamTx) {
            m_periodTimer->stop();
        }
        m_asyncDecodeTimer->stop();
        m_asyncDecodePending = false;
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
        syncLegacyBackendState();
        if (bridgeOwnsMamTx) {
            // Legacy owns RX/decode; keep the UTC scheduler alive solely for
            // the bridge-owned mixed MAM TX waveform.
            armPeriodTimerForCurrentMode(monitorSessionId,
                                         QStringLiteral("startRx legacy MAM bridge TX"));
        }
        bool const nativeSstvCapture =
#if DECODIUM_HAS_SSTV
            nativeSstvRxForcesDedicatedAudioCapture();
#else
            false;
#endif
        if (useModernSpectrumFeedWithLegacy() || nativeSstvCapture) {
            bool const dedicatedModernCapture = nativeSstvCapture
                || useDedicatedModernAudioCaptureWithLegacy();
            m_legacyPcmSpectrumFeed = !dedicatedModernCapture;
            updatePeriodTicksMax();
            resetRxPeriodAccumulation(true);
            m_spectrumBuf.clear();
            m_wfRingPos = 0;
            m_lastPanadapterData.clear();
            m_driftFrameCount = 0;
            m_driftExpectedFrames = 0;
            m_driftClock.restart();
            if (dedicatedModernCapture) {
                startAudioCapture();
            } else {
                stopAudioCapture();
                m_lastLegacyPcmSampleMs = 0;
            }
            m_spectrumTimer->start();
            bridgeLog(QStringLiteral("startRx: modern FFT/SSTV feed active source=%1 alongside legacy RX")
                          .arg(dedicatedModernCapture
                               ? QStringLiteral("dedicated_modern_qaudio")
                               : QStringLiteral("legacy_pcm_tap_single_capture")));
        } else {
            stopAudioCapture();
            resetRxPeriodAccumulation(false);
            m_spectrumBuf.clear();
            m_wfRingPos = 0;
            m_lastPanadapterData.clear();
            m_legacyPcmSpectrumFeed = !isFt2LinkApplicationMode(m_mode);
            if (m_spectrumTimer) {
                if (isFt2LinkApplicationMode(m_mode)) {
                    m_spectrumTimer->stop();
                } else {
                    m_spectrumTimer->start();
                }
            }
            bridgeLog(isFt2LinkApplicationMode(m_mode)
                          ? QStringLiteral("startRx: FT2-Link legacy RX uses backend waterfall rows for visual panadapter")
                          : QStringLiteral("startRx: legacy PCM tap feeds fast QML panadapter"));
        }
        emit statusMessage(tr("Reception started via legacy backend - %1").arg(m_mode));
        return;
    }

    const bool rtlGeneralReceiver = rtlSdrGeneralReceiverConfigured();
    const bool sstvWorkspaceReceiver =
#if DECODIUM_HAS_SSTV
        m_sstvWorkspaceActive;
#else
        false;
#endif
    const bool rttyWorkspaceReceiver = isStreamingKeyboardMode(m_mode);
    if (!rtlGeneralReceiver && !sstvWorkspaceReceiver && !rttyWorkspaceReceiver) {
        ensureDecodeWorkerForMode(m_mode);
    } else if (rtlGeneralReceiver) {
        bridgeLog(QStringLiteral("startRx: RTL-SDR general receiver selected; digital decoder dispatch disabled"));
    } else {
        bridgeLog(rttyWorkspaceReceiver
                      ? QStringLiteral("startRx: %1 workspace selected; normal digital decoder dispatch disabled").arg(m_mode)
                      : QStringLiteral("startRx: SSTV workspace selected; normal digital decoder dispatch disabled"));
    }

    updatePeriodTicksMax();
    m_legacyPcmSpectrumFeed = false;
    m_monitoring = true;
    emit monitoringChanged();
    resetRxPeriodAccumulation(true);
    m_spectrumBuf.clear();
    // A2 — Reset drift counters
    m_driftFrameCount    = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    resetTimeSyncDecodeMetrics();
    if (m_ntpClient && m_ntpEnabled && isTimeSyncDecodeMode(m_mode)) {
        configureNtpClientForMode(m_mode);
        m_ntpClient->syncNow();
    }
    // Sincronizza il period timer al prossimo boundary UTC (FT8=15s, FT4=7.5s, FT2=3.75s).
    // Il session id impedisce a una singleShot vecchia di riattivare il timer dopo stop/cambio modo.
    if (rtlGeneralReceiver || sstvWorkspaceReceiver || rttyWorkspaceReceiver) {
        m_periodTimer->stop();
        m_periodTicks = 0;
        m_lastPeriodSlot = -1;
        m_periodProgress = 0;
        emit periodProgressChanged();
    } else {
        armPeriodTimerForCurrentMode(monitorSessionId, QStringLiteral("startRx"));
    }
    m_spectrumTimer->start();
    if (!rtlGeneralReceiver && !sstvWorkspaceReceiver && !rttyWorkspaceReceiver && m_mode == "FT2") {
        // store(release): vedi commento nel reset di setMode — il writer usa
        // CAS sul commit e un increment in volo verrà scartato correttamente.
        m_asyncAudioPos.store(0, std::memory_order_release);
        m_asyncDecodePending = false;
        m_asyncDecodeTimer->start();
    }

    startAudioCapture();

    m_decoding = !rtlGeneralReceiver && !sstvWorkspaceReceiver && !rttyWorkspaceReceiver;
    emit decodingChanged();
    emit statusMessage(rtlGeneralReceiver
                           ? QStringLiteral("RTL-SDR general receiver started")
                           : (rttyWorkspaceReceiver
                                  ? QStringLiteral("RTTY audio monitor started")
                                  : (sstvWorkspaceReceiver
                                  ? QStringLiteral("SSTV audio monitor started")
                                  : QStringLiteral("RX avviato - %1").arg(m_mode))));
    bridgeLog("startRx() done");
}

void DecodiumBridge::stopRx()
{
    if (m_rttyRxRecovery)
        m_rttyRxRecovery->cancel(QStringLiteral("monitor stopped"));
    bridgeLog("stopRx() called");
    // The native SSTV engine cannot remain active without Decodium's single
    // source-of-truth monitor. stopSstvRx() clears the request before calling
    // setMonitoring(false), so this helper is recursion-safe.
    stopSstvRxForMonitorStop();
    // 1.0.179 — Smooth Decode Flow: stop RX invalida la coda residua.
    if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
    m_pendingDecodeReleaseQueue.clear();
    bool const monitorWasRequested = m_monitorRequested;
    m_monitorRequested = false;
    if (!m_monitoring) {
        if (monitorWasRequested) {
            ++m_periodTimerSessionId;
            if (legacyBackendAvailable()) {
                m_legacyBackend->setMonitoring(false);
            }
        }
        bridgeLog("stopRx: not monitoring, skip");
        scheduleIdleAudioBufferRelease(30000);
        return;
    }
    ++m_periodTimerSessionId;
    clearDeferredManualSyncTx(QStringLiteral("stop-rx"));

    if (usingLegacyBackendForRx()) {
        bridgeLog("stopRx: delegating monitoring stop to legacy backend");
        m_periodTimer->stop();
        m_spectrumTimer->stop();
        m_asyncDecodeTimer->stop();
        m_asyncDecodePending = false;
        m_legacyPcmSpectrumFeed = false;
        stopAudioCapture();
        m_spectrumBuf.clear();
        m_lastPanadapterData.clear();
        m_wfRingPos = 0;
        resetRxPeriodAccumulation(false);
        resetTimeSyncDecodeMetrics();
        m_legacyBackend->setMonitoring(false);
        syncLegacyBackendState();
        scheduleIdleAudioBufferRelease(30000);
        emit statusMessage(tr("Reception stopped"));
        return;
    }

    m_monitoring = false;
    emit monitoringChanged();
    m_periodTimer->stop();
    m_spectrumTimer->stop();
    m_asyncDecodeTimer->stop();
    m_asyncDecodePending = false;
    stopAudioCapture();
    resetRxPeriodAccumulation(false);
    resetTimeSyncDecodeMetrics();

    m_decoding = false;
    emit decodingChanged();
    scheduleIdleAudioBufferRelease(30000);
    emit statusMessage(tr("Reception stopped"));
}

// === TX helpers ============================================================

// Aggiorna SoundOutput con il dispositivo di uscita correntemente selezionato
