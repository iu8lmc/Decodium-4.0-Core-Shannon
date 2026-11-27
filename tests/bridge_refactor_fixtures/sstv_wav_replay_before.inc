bool DecodiumBridge::startSstvWavReplay(const QUrl& localFile)
{
#if DECODIUM_HAS_SSTV
    initialiseSstvRuntime();
    if (!m_sstvRxRuntime || !m_sstvWavReplayController
        || m_sstvWavReplayController->active()) {
        return false;
    }

    const auto previous = m_sstvRxRuntime->routeToken();
    m_sstvWavReplayRestoreLive = m_sstvRxRequested
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running
        && previous.valid()
        && previous.source.kind != SstvAudioSourceKind::Replay;
    m_sstvWavReplayRestoreKind = m_sstvWavReplayRestoreLive
        ? static_cast<std::uint8_t>(previous.source.kind) : 0U;
    m_sstvWavReplayRestoreStreamId = m_sstvWavReplayRestoreLive
        ? previous.source.streamId : 0U;

    // Close and drain every DirectConnection callback before the controller
    // creates the Replay generation. The file worker then becomes its only
    // producer and feeds the same SstvRxRuntime used by live reception.
    disableSstvProducerTaps();
    if (!m_sstvWavReplayController->startReplay(localFile)) {
        const QString detail = m_sstvWavReplayController->lastError();
        if (!restoreSstvLiveAfterReplay()) {
            emit errorMessage(tr(
                "SSTV WAV replay failed and the live RX route could not be restored"));
        }
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayRestoreKind = 0U;
        m_sstvWavReplayRestoreStreamId = 0U;
        emit sstvWavReplayChanged();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        if (!detail.isEmpty()) {
            emit errorMessage(tr("SSTV WAV replay: %1").arg(detail));
        }
        return false;
    }

    m_sstvWavReplaySession = m_sstvWavReplayController->sessionId();
    emit sstvWavReplayChanged();
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();
    emit statusMessage(tr("SSTV WAV replay started: %1")
                           .arg(m_sstvWavReplayController->fileName()));
    return true;
#else
    Q_UNUSED(localFile)
    emit errorMessage(tr(
        "This Decodium build has no native SSTV WAV replay support"));
    return false;
#endif
}

void DecodiumBridge::cancelSstvWavReplay()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWavReplayController) {
        m_sstvWavReplayController->cancel();
    }
#endif
}

void DecodiumBridge::finishSstvWavReplay(bool completed,
                                         bool cancelled,
                                         quint64 sessionId)
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvWavReplayController
        || sessionId == 0U || sessionId != m_sstvWavReplaySession) {
        return;
    }
    const bool wasRedecode = m_sstvRedecodeActive;
    m_sstvWavReplaySession = 0U;
    if (m_sstvRedecodeActive && m_sstvRxRuntime) {
        SstvRxControlSettings restore =
            m_sstvRxRuntime->rxControlSnapshot().settings;
        QString restoreError;
        if (!sstvRxSettingsFromMap(
                m_sstvRedecodeRestoreControls,
                restore,
                &restore,
                &restoreError)
            || !m_sstvRxRuntime->replaceRxControlSettings(
                std::move(restore))) {
            emit errorMessage(tr(
                "SSTV re-decode ended but the prior RX controls could not be restored: %1")
                                  .arg(restoreError));
        }
        m_sstvRedecodeActive = false;
        m_sstvRedecodeRestoreControls.clear();
    }
    if (wasRedecode) {
        const SstvRxRuntime::Snapshot snapshot = m_sstvRxRuntime
            ? m_sstvRxRuntime->snapshot() : SstvRxRuntime::Snapshot {};
        QVariantMap fields {
            {QStringLiteral("state"),
             completed ? QStringLiteral("completed")
                       : (cancelled ? QStringLiteral("cancelled")
                                    : QStringLiteral("failed"))},
            {QStringLiteral("success"), completed && !cancelled},
            {QStringLiteral("coveragePermille"),
             static_cast<qulonglong>(std::llround(std::clamp(
                 snapshot.image.coverage, 0.0, 1.0) * 1'000.0))},
        };
        if (!snapshot.image.mode.isEmpty()) {
            fields.insert(QStringLiteral("modeId"), snapshot.image.mode);
        }
        try {
            decodium::sstv::recordSstvDiagnosticEvent(
                sstvRxLog(), completed && !cancelled
                    ? QtInfoMsg : QtWarningMsg,
                QStringLiteral("rx.redecode-completed"), fields);
        } catch (...) {
            // Diagnostics are best-effort and never own replay completion.
        }
    }
    const bool restoreRequested = m_sstvWavReplayRestoreLive;
    bool restored = true;
    if (restoreRequested) {
        restored = restoreSstvLiveAfterReplay();
    } else if (!completed && !m_sstvRxRequested && m_sstvRxRuntime
               && m_sstvRxRuntime->state()
                      != SstvRxRuntime::State::Inactive
               && m_sstvRxRuntime->state()
                      != SstvRxRuntime::State::Shutdown) {
        // A cancelled/failed standalone import has no useful snapshot to
        // retain. Join the native RX worker instead of leaving it idle.
        m_sstvRxRuntime->stop();
    }
    m_sstvWavReplayRestoreLive = false;
    m_sstvWavReplayRestoreKind = 0U;
    m_sstvWavReplayRestoreStreamId = 0U;
    if (m_sstvRxAudioJobController) {
        m_sstvRxAudioJobController->discardPreparedRedecode();
        emit sstvRxAudioJobChanged();
    }

    if (!restored) {
        m_sstvRxRequested = false;
        m_sstvOwnsMonitoring = false;
        disableSstvProducerTaps();
        emit errorMessage(tr(
            "SSTV WAV replay ended but the live RX source could not be restored"));
    } else if (completed) {
        emit statusMessage(tr(
            "SSTV WAV replay completed and drained"));
    } else if (cancelled) {
        emit statusMessage(tr("SSTV WAV replay cancelled"));
    }
    emit sstvWavReplayChanged();
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();
#else
    Q_UNUSED(completed)
    Q_UNUSED(cancelled)
    Q_UNUSED(sessionId)
#endif
}

bool DecodiumBridge::restoreSstvLiveAfterReplay()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvWavReplayRestoreLive || !m_sstvRxRequested) {
        return true;
    }
    if (!m_sstvRxRuntime) {
        return false;
    }

    SstvAudioSourceKind kind = currentSstvAudioSourceKind();
    quint32 streamId = currentSstvAudioStreamId(kind);
    if (kind == SstvAudioSourceKind::Unknown || streamId == 0U) {
        kind = static_cast<SstvAudioSourceKind>(
            m_sstvWavReplayRestoreKind);
        streamId = m_sstvWavReplayRestoreStreamId;
    }
    if (kind == SstvAudioSourceKind::Unknown
        || kind == SstvAudioSourceKind::Replay || streamId == 0U) {
        return false;
    }

    bool selected = false;
    switch (m_sstvRxRuntime->state()) {
    case SstvRxRuntime::State::Running:
    case SstvRxRuntime::State::Cancelled:
        selected = m_sstvRxRuntime->switchSource(kind, streamId);
        break;
    case SstvRxRuntime::State::Inactive:
        selected = m_sstvRxRuntime->start(kind, streamId);
        break;
    case SstvRxRuntime::State::Error:
        selected = m_sstvRxRuntime->stop()
            && m_sstvRxRuntime->start(kind, streamId);
        break;
    case SstvRxRuntime::State::Stopping:
    case SstvRxRuntime::State::Shutdown:
        break;
    }
    if (!selected) {
        return false;
    }
    refreshSstvProducerTaps();
    return true;
#else
    return false;
#endif
}
