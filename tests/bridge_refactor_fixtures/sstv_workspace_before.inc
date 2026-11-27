bool DecodiumBridge::enterSstvWorkspace()
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        return true;
    }

    // Snapshot the user's normal RX state before changing anything. The
    // selected application mode and CAT frequency intentionally remain
    // untouched: SSTV owns the workspace/audio session, not the radio profile.
    m_sstvWorkspaceRestoreMonitoring = m_monitoring || m_monitorRequested;
    m_sstvWorkspaceActive = true;

    // Replace the previous monitor generation with the SSTV workspace's
    // visual-only capture. This invalidates delayed FT boundary callbacks and
    // quiesces a legacy decoder, but immediately reopens the shared PCM and
    // spectrum path so the dashboard panadapter keeps moving in the
    // background. If MONITOR was off, preserve that state.
    if (m_sstvWorkspaceRestoreMonitoring) {
        setMonitoring(false);
        setMonitoring(true);
    }
    if (m_decodeReleaseTimer) {
        m_decodeReleaseTimer->stop();
    }
    m_pendingDecodeReleaseQueue.clear();
    clearDecodeWindowsForModeChange(m_mode, QStringLiteral("SSTV"));
    resetFtxDecodeWorkersForModeChange(m_mode, QStringLiteral("SSTV"));
    resetRxPeriodAccumulation(false);

    qInfo().noquote()
        << QStringLiteral(
        "[SSTV] workspace entered: normal decoder suspended, panadapter preserved mode=%1 restoreMonitor=%2")
               .arg(m_mode)
               .arg(m_sstvWorkspaceRestoreMonitoring ? 1 : 0);
    return true;
#else
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

void DecodiumBridge::leaveSstvWorkspace()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvWorkspaceActive) {
        return;
    }

    const bool restoreMonitoring = m_sstvWorkspaceRestoreMonitoring;

    // A close is also a hard ownership boundary. QML normally stops analogue
    // and digital SSTV first, but stopping the shared monitor here makes the
    // C++ API safe if the window is closed by the window manager or shutdown.
    if (m_monitoring || m_monitorRequested) {
        setMonitoring(false);
    }

    // Consume any final legacy result that completed while the workspace was
    // open. It belongs to the suspended dashboard mode and must not appear as
    // a fresh decode immediately after the normal monitor is restored.
    if (m_legacyBackend) {
        if (!m_legacyBackend->bandActivityLines().isEmpty()) {
            m_legacyBackend->clearBandActivity();
        }
        if (!m_legacyBackend->rxFrequencyLines().isEmpty()) {
            m_legacyBackend->clearRxFrequency();
        }
        m_legacyBandActivityRevision =
            m_legacyBackend->bandActivityRevision();
        m_legacyRxFrequencyRevision =
            m_legacyBackend->rxFrequencyRevision();
    }
    primeLegacyAllTxtCursor();

    m_sstvWorkspaceActive = false;
    m_sstvWorkspaceRestoreMonitoring = false;

    if (restoreMonitoring && !m_shuttingDown && !QCoreApplication::closingDown()
        && !m_transmitting && !m_tuning) {
        setMonitoring(true);
    }

    qInfo().noquote()
        << QStringLiteral(
               "[SSTV] workspace left: normal decoder restore mode=%1 monitor=%2")
               .arg(m_mode)
               .arg(restoreMonitoring ? 1 : 0);
#endif
}

