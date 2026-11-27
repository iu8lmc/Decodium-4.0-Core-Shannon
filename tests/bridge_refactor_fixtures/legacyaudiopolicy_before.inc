bool DecodiumBridge::shouldUseBridgeAudioForLegacyDigitalTx() const
{
#if defined(Q_OS_MAC)
    QString const mode = m_mode.trimmed().toUpper();
    // Keep this aligned with MainWindow::startTx2(): when the embedded legacy
    // backend mutes its own FTX audio, the bridge must provide the payload.
    return usingLegacyBackendForTx()
        && !usingTciAudioInput()
        && !specialOperationRequiresLegacyTx()
        && (mode == QStringLiteral("FT8")
            || mode == QStringLiteral("FT4")
            || mode == QStringLiteral("FT2")
            || mode == QStringLiteral("JT65")
            || mode == QStringLiteral("MSK144")
            || mode == QStringLiteral("Q65")
            || mode.startsWith(QStringLiteral("Q65-")));
#else
    return false;
#endif
}

QString DecodiumBridge::currentBridgeTxRepresentativeMessage() const
{
    if (!m_cwTxActive && multiStreamActive()) {
        return m_mamMessages.first().trimmed();
    }
    return buildCurrentTxMessage();
}

bool DecodiumBridge::legacyBridgeAudioTxInFlight() const
{
#if defined(Q_OS_MAC)
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    bool const playbackWindowOpen =
        (m_txPlaybackHoldUntilMs > nowMs) || (m_txPlaybackHardDeadlineMs > nowMs);
    return m_bridgeAudioLegacyTxActive
        || (m_txPcmBuffer && (m_transmitting || m_tuning || playbackWindowOpen))
        || (m_txAudioSink && (m_transmitting || m_tuning || playbackWindowOpen));
#else
    return false;
#endif
}

