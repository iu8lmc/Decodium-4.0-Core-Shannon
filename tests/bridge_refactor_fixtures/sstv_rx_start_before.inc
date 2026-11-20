bool DecodiumBridge::startSstvRx()
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime) {
        emit errorMessage(tr("Native SSTV support is unavailable"));
        return false;
    }
    if (sstvWavReplayActive()) {
        emit errorMessage(tr(
            "SSTV RX monitor cannot replace an active WAV replay"));
        return false;
    }
    if (m_sstvRxRequested
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running) {
#if DECODIUM_HAS_HAMDRM
        // A caller explicitly starting an already shared receiver adopts it.
        m_hamDrmOwnsSstvRx = false;
#endif
        return true;
    }

    const bool retryingRequestedSession = m_sstvRxRequested;
    const bool previousMonitoringOwnership = m_sstvOwnsMonitoring;
    const bool monitoringWasAlreadyRequested =
        m_monitoring || m_monitorRequested;
    const bool legacyMonitorNeedsRestore = usingLegacyBackendForRx()
        && (m_monitoring || m_monitorRequested)
        && m_legacyBackend
        && (m_legacyBackend->monitoring() || m_legacyBackend->monitoringStartPending());
    auto rollbackStart = [this](bool releaseOwnedMonitoring) {
        const bool releaseDedicatedCapture =
            m_sstvOwnsDedicatedAudioCapture;
        const bool restoreLegacyMonitoring =
            m_sstvRestoresLegacyMonitoring;
        m_sstvRxRequested = false;
        disableSstvProducerTaps();
        if (m_sstvRxRuntime) {
            m_sstvRxRuntime->stop();
        }
        m_sstvOwnsMonitoring = false;
        m_sstvOwnsDedicatedAudioCapture = false;
        m_sstvRestoresLegacyMonitoring = false;
#if DECODIUM_HAS_HAMDRM
        if (m_hamDrmRxRequested && m_hamDrmRxBackend
            && m_hamDrmRxBackend->active()) {
            refreshSstvProducerTaps();
        }
#endif
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        if (releaseOwnedMonitoring && (m_monitoring || m_monitorRequested)) {
            setMonitoring(false);
        } else if (releaseDedicatedCapture) {
            stopAudioCapture();
            if (usingLegacyBackendForRx() && m_legacyBackend) {
                rearmLegacyPcmSpectrumFeed(
                    QStringLiteral("SSTV RX start rollback"));
            }
        }
        if (restoreLegacyMonitoring && m_legacyBackend) {
            // Rebuild the complete legacy RX generation after a failed SSTV
            // start as well. Restoring only the backend boolean would leave
            // the dashboard timers/audio feed stopped in the same way as a
            // normal SSTV close.
            m_monitorRequested = false;
            if (m_monitoring) {
                m_monitoring = false;
                emit monitoringChanged();
            }
            startRx();
        }
    };

    // Recover a previous cancelled/error session before creating a fresh
    // generation. Shutdown is terminal and only occurs during bridge teardown.
    const SstvRxRuntime::State previousState = m_sstvRxRuntime->state();
    if (previousState == SstvRxRuntime::State::Shutdown
        || previousState == SstvRxRuntime::State::Stopping) {
        emit errorMessage(tr("SSTV RX runtime is shutting down"));
        return false;
    }
    if (previousState != SstvRxRuntime::State::Inactive) {
        disableSstvProducerTaps();
        if (!m_sstvRxRuntime->stop()) {
            emit errorMessage(tr(
                "SSTV RX could not clear its previous session"));
            return false;
        }
    }

    // Also cancel a pending legacy startup: otherwise its delayed enable can
    // reopen the endpoint after the native SSTV capture has taken ownership.
    // stopSstvRx() restores the previous monitor request.
    if (legacyMonitorNeedsRestore) {
        m_sstvRestoresLegacyMonitoring = true;
        m_monitorRequested = false;
        m_legacyBackend->setMonitoring(false);
        // The legacy backend can leave its SoundInput object alive after the
        // backend state changes to stopped. Native SSTV must reopen that
        // endpoint on its own generation: reusing the old AudioQueue is
        // especially unreliable with BlackHole, where it may remain active
        // while delivering a permanent zero-RMS stream.
        stopAudioCapture();
        m_monitoring = false;
        emit monitoringChanged();
    }

    // Publish the request before selecting the initial source.  This is the
    // generation boundary that makes a manual/automatic SSTV session choose
    // the native capture even when the application mode is FT8/FT2.
    m_sstvRxRequested = true;
    m_sstvOwnsDedicatedAudioCapture =
        nativeSstvRxForcesDedicatedAudioCapture();

    const SstvAudioSourceKind initialKind = currentSstvAudioSourceKind();
    const quint32 initialStreamId = currentSstvAudioStreamId(initialKind);
    if (initialKind == SstvAudioSourceKind::Unknown || initialStreamId == 0U) {
        rollbackStart(retryingRequestedSession
                      && previousMonitoringOwnership);
        emit errorMessage(tr(
            "SSTV RX: no Decodium audio source is available"));
        return false;
    }

    if (!m_sstvRxRuntime->start(initialKind, initialStreamId)) {
        rollbackStart(retryingRequestedSession
                      && previousMonitoringOwnership);
        emit errorMessage(tr("SSTV RX worker could not be started"));
        return false;
    }

    m_sstvOwnsMonitoring = retryingRequestedSession
        ? previousMonitoringOwnership
        : !monitoringWasAlreadyRequested;
    if (legacyMonitorNeedsRestore) {
        m_sstvOwnsMonitoring = true;
    }
    refreshSstvProducerTaps();
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();

    if (!m_monitoring) {
        setMonitoring(true);
    }
    if (!m_monitoring) {
        const bool releaseOwnedMonitoring = m_sstvOwnsMonitoring;
        rollbackStart(releaseOwnedMonitoring);
        emit errorMessage(tr(
            "SSTV RX requires an active Decodium monitor"));
        return false;
    }

    // If the Decodium monitor was already running through the legacy backend,
    // startRx() was not involved in this session.  Ensure the native capture
    // exists before binding the local SSTV tap; startAudioCapture() is
    // idempotent for an already-active endpoint.
    if (nativeSstvRxForcesDedicatedAudioCapture()) {
        startAudioCapture();
    }

    const SstvAudioSourceKind selectedKind = currentSstvAudioSourceKind();
    selectSstvRxSource(selectedKind, currentSstvAudioStreamId(selectedKind));
    emit statusMessage(tr("SSTV RX started from %1").arg(sstvRxSource()));
    return true;
#else
    emit errorMessage(tr(
        "This Decodium build has no native SSTV support"));
    return false;
#endif
}
