bool DecodiumBridge::preflightLegacyBridgeTxBeforePtt(const QString& reason)
{
#if defined(Q_OS_MAC)
    MainThreadTraceScope trace(QStringLiteral("legacy_bridge_tx_preflight_main"),
                               QStringLiteral("reason=%1 mode=%2 tx=%3")
                                   .arg(reason, m_mode)
                                   .arg(m_transmitting ? 1 : 0),
                               25);
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (!shouldUseBridgeAudioForLegacyDigitalTx()) {
        return true;
    }
    if (legacyBridgeAudioTxInFlight()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio preflight already in-flight (%1)").arg(reason));
        return true;
    }

    bool const forceImmediateLabTx = truthyEnvironmentFlag("DECODIUM_LAB_FORCE_TX_IMMEDIATE");
    if (m_monitoring && !forceImmediateLabTx) {
        int elapsedMs = -1;
        int latestStartMs = 0;
        if (isSyncTxStartTooLate(&elapsedMs, &latestStartMs)) {
            bridgeLog(QStringLiteral("legacyBridgeTxAudio preflight deferred late %1 TX%2 elapsed=%3ms latest=%4ms (%5)")
                          .arg(m_mode)
                          .arg(m_currentTx)
                          .arg(elapsedMs)
                          .arg(latestStartMs)
                          .arg(reason));
            if (usesDeferredManualSyncTx() && !m_autoCqRepeat) {
                m_deferredManualSyncTx = true;
            }
            scheduleSyncTxAtNextValidSlot(QStringLiteral("legacy-preflight:%1").arg(reason),
                                          elapsedMs,
                                          latestStartMs);
            emit statusMessage(QStringLiteral("TX%1 rimandato: slot %2 troppo avanzato")
                                   .arg(m_currentTx)
                                   .arg(m_mode));
            return false;
        }
    }

    QElapsedTimer phaseTimer;
    phaseTimer.start();
    syncLegacyBackendState();
    qint64 const syncMs = phaseTimer.elapsed();
    phaseTimer.restart();
    if (!checkSwrAllowsTransmission(QStringLiteral("legacyBridgeTxPreflight"))) {
        return false;
    }
    qint64 const swrMs = phaseTimer.elapsed();

    phaseTimer.restart();
    QString msg = currentBridgeTxRepresentativeMessage();
    forceRecentRogerReportSignoffIfNeeded(msg,
                                          QStringLiteral("legacyBridgeTxPreflight:%1").arg(reason));
    qint64 const msgMs = phaseTimer.elapsed();
    if (msg.trimmed().isEmpty()) {
        bridgeLog(QStringLiteral("legacyBridgeTxAudio preflight blocked empty TX message (%1)")
                      .arg(reason));
        emit errorMessage(QStringLiteral("Nessun messaggio TX selezionato"));
        return false;
    }
    phaseTimer.restart();
    if (!repairOrRejectStalePartnerTxMessage(msg,
                                             QStringLiteral("legacyBridgeTxPreflight:%1").arg(reason))) {
        emit statusMessage(QStringLiteral("TX bloccato: messaggio non coerente con il QSO attivo"));
        return false;
    }
    qint64 const repairMs = phaseTimer.elapsed();
    txTimelineLog(QStringLiteral("[TX-TL] legacy_preflight reason=%1 total_ms=%2 sync_ms=%3 swr_ms=%4 msg_ms=%5 repair_ms=%6 msg=[%7]")
                  .arg(reason)
                  .arg(totalTimer.elapsed())
                  .arg(syncMs)
                  .arg(swrMs)
                  .arg(msgMs)
                  .arg(repairMs)
                  .arg(msg.trimmed()));
    return true;
#else
    Q_UNUSED(reason);
    return true;
#endif
}

void DecodiumBridge::abortLegacyBridgeTxRequest(const QString& reason)
{
#if defined(Q_OS_MAC)
    cancelPendingLegacyBridgeAudioStart(QStringLiteral("abort:%1").arg(reason));
    bridgeLog(QStringLiteral("legacyBridgeTxAudio abort request: reason=%1 active=%2 transmitting=%3")
                  .arg(reason)
                  .arg(m_bridgeAudioLegacyTxActive ? 1 : 0)
                  .arg(m_transmitting ? 1 : 0));

    // Release the physical PTT before asking the legacy backend to stop.  The
    // latter emits rigPttRequested(false), so the per-transition guard below
    // makes both paths converge on one and only one CAT PTT-OFF command.
    requestLegacyPttOffOnce(QStringLiteral("abort:%1").arg(reason));
    if (m_legacyBackend && m_legacyBackend->transmitting()) {
        m_legacyBackend->stopTransmission();
    }

    if (m_bridgeAudioLegacyTxActive) {
        stopBridgeAudioForLegacyDigitalTx(reason);
        clearLegacyPttTransition(QStringLiteral("abort:%1").arg(reason));
        return;
    }

    m_txPlaybackHoldUntilMs = 0;
    m_txPlaybackHardDeadlineMs = 0;
    m_txPlaybackReleasePending = false;
    m_txAudioRestartPending = false;
    m_activeTxNumber = 0;
    m_activeTxMessage.clear();
    if (m_txPcmBuffer) {
        m_txPcmBuffer->close();
        delete m_txPcmBuffer;
        m_txPcmBuffer = nullptr;
    }
    m_txPcmData.clear();
    if (m_transmitting) {
        m_transmitting = false;
        emit transmittingChanged();
    }
    clearLegacyPttTransition(QStringLiteral("abort:%1").arg(reason));
    resumeRxAudioAfterTx(reason);
    resumeNonAudioTxWork(reason);
#else
    Q_UNUSED(reason);
#endif
}
