void DecodiumBridge::cancelSstvTx()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_sstvTxCoordinator && m_sstvTxCoordinator->stateMachine().active()) {
        m_sstvTxCoordinator->cancel(sstvTxNowMs());
        emit statusMessage(tr("SSTV TX cancelled"));
    }
#endif
}

void DecodiumBridge::tickSstvTx()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvTxCoordinator || m_sstvTxShuttingDown) {
        return;
    }
    const std::uint64_t now = sstvTxNowMs();
    decodium::sstv::SstvTxCoordinatorSnapshot snapshot
        = m_sstvTxCoordinator->snapshot();
    const std::uint64_t sessionId
        = snapshot.stateMachine.currentSessionId;

    if (snapshot.stateMachine.state
            == decodium::sstv::SstvTxState::WaitingForPtt
        && (sstvTxUsesVoxPtt() || sstvTxPttActive())) {
        m_sstvTxCoordinator->notifyPttConfirmed(now, sessionId);
    }
    snapshot = m_sstvTxCoordinator->snapshot();
    if (snapshot.stateMachine.state
            == decodium::sstv::SstvTxState::ReleasingPtt
        && !sstvTxPttActive()) {
        m_sstvTxCoordinator->notifyPttReleased(now, sessionId);
    }
    snapshot = m_sstvTxCoordinator->snapshot();
    if (m_sstvTxCoordinator->stateMachine().active()
        && snapshot.stateMachine.pttRequestDispatched
        && snapshot.stateMachine.releaseRequired
        && !sstvTxCanControlPtt()
        && snapshot.stateMachine.state
            != decodium::sstv::SstvTxState::ReleasingPtt) {
        m_sstvTxCoordinator->notifyAudioError(
            now, sessionId,
            "Decodium CAT/PTT path disconnected during SSTV TX");
    }

    m_sstvTxCoordinator->tick(now);
    snapshot = m_sstvTxCoordinator->snapshot();
    if (snapshot.audioAttached && m_sstvTxAudioDevice && m_soundOutput) {
        const SoundOutputPlaybackStatus playback
            = m_soundOutput->trackedPlaybackStatus(
                static_cast<quint64>(sessionId));
        const std::uint64_t delivered
            = m_sstvTxAudioDevice->framesReadFromDevice();
        decodium::sstv::SstvTxPlaybackProgress progress;
        progress.playedFrames = playback.drained
            ? snapshot.audioPlan.totalFrames
            : std::min<std::uint64_t>(snapshot.audioPlan.totalFrames,
                                      playback.processedFrames);
        progress.playbackComplete = playback.drained
            && delivered >= snapshot.audioPlan.totalFrames;
        progress.failed = m_sstvTxAudioDevice->failed()
            || !playback.valid || playback.failed
            || playback.expectedFrames != snapshot.audioPlan.totalFrames;
        if (m_sstvTxAudioDevice->failed()) {
            progress.detail = "native SSTV audio source failed";
        } else if (!playback.valid) {
            progress.detail = "SSTV SoundOutput session tracking was lost";
        } else if (playback.failed) {
            progress.detail = playback.detail.toStdString();
        } else if (playback.expectedFrames
                   != snapshot.audioPlan.totalFrames) {
            progress.detail = "SSTV SoundOutput frame plan changed during TX";
        }
        m_sstvTxCoordinator->notifyPlayback(
            now, sessionId, progress);
    }
#endif
}

void DecodiumBridge::releaseSstvTxBridgeOwnership()
{
#if DECODIUM_HAS_SSTV
    m_sstvTxAudioDevice = nullptr;
    m_sstvTxOutputDevice = QAudioDevice {};
    m_sstvTxOutputChannels = 0U;
    m_sstvTxOutputPinned = false;
    m_sstvTxSessionId = 0U;
    if (!m_sstvOwnsBridgeTx) {
        return;
    }
    m_sstvOwnsBridgeTx = false;
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
    }
    restoreTxAudioSchedulingBoost(QStringLiteral("sstv-tx"));
    if (!m_sstvTxShuttingDown && !m_shuttingDown
        && !QCoreApplication::closingDown()) {
        resumeRxAudioAfterTx(QStringLiteral("sstv-tx"));
        resumeNonAudioTxWork(QStringLiteral("sstv-tx"));
    }
    emit sstvTxStateChanged();
#endif
}

void DecodiumBridge::shutdownSstvTx()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    m_sstvTxShuttingDown = true;
    if (m_sstvTxTimer) {
        m_sstvTxTimer->stop();
    }
    if (m_sstvTxCoordinator) {
        m_sstvTxCoordinator->shutdown(sstvTxNowMs());
        const auto snapshot = m_sstvTxCoordinator->snapshot();
        if (snapshot.stateMachine.state
                == decodium::sstv::SstvTxState::ReleasingPtt
            && !sstvTxPttActive()) {
            m_sstvTxCoordinator->notifyPttReleased(
                sstvTxNowMs(), snapshot.stateMachine.currentSessionId);
        }
    }
    if (m_sstvOwnsBridgeTx && !sstvTxUsesVoxPtt()) {
        setSstvTxPtt(false);
    }
    if (m_soundOutput && m_sstvTxAudioDevice) {
        if (!m_soundOutput->stopTrackedPlayback(m_sstvTxSessionId)) {
            m_soundOutput->stop();
        }
    }
    m_sstvTxAudioDevice = nullptr;
    m_sstvTxCoordinator.reset();
    releaseSstvTxBridgeOwnership();
    m_sstvTxState = QStringLiteral("Disabled");
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
#endif
}

