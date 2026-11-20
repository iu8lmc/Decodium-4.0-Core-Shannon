void DecodiumBridge::maybeDispatchBridgeOwnedJtDecodeFallback()
{
    QString const modeSnapshot = m_mode.trimmed().toUpper();
    bool const bridgeOwnedTimeSyncRx =
        bridgeOwnsLegacyJtRxWhenCatSuppressed(modeSnapshot)
        || bridgeOwnsFst4Audio(modeSnapshot);
    if (!bridgeOwnedTimeSyncRx) {
        return;
    }
    if (!m_monitoring || m_rxAudioSuspendedForTx || m_transmitting || m_tuning) {
        return;
    }
    if (m_pendingTimeSyncDecodeActive) {
        return;
    }

    int const periodMs = effectivePeriodMsForMode(modeSnapshot);
    int const targetSamples = targetDecodeSamplesForMode(modeSnapshot);
    int const minimumSamples = minimumDecodeSamplesForMode(modeSnapshot);
    if (periodMs <= 0 || targetSamples <= 0 || minimumSamples <= 0) {
        return;
    }

    qint64 const nowMs = correctedUtcEpochMs();
    qint64 const currentSlot = nowMs / periodMs;
    if (currentSlot <= 0) {
        return;
    }
    int const msInSlot = static_cast<int>(nowMs % static_cast<qint64>(periodMs));

    // Let the post-boundary audio callback settle, then decode the just-ended
    // bridge-owned long slot once. This is a defensive path for lab/no-CAT
    // macOS runs where the main period timer can be stale while audio capture
    // is healthy.
    if (msInSlot < 1500 || msInSlot > qMin(periodMs - 1000, 9000)) {
        return;
    }

    qint64 const completedSlot = currentSlot - 1;
    if (m_bridgeOwnedJtFallbackLastMode == modeSnapshot
        && m_bridgeOwnedJtFallbackLastSlot == completedSlot) {
        return;
    }
    if (nowMs - m_bridgeOwnedJtFallbackLastDispatchMs < qMax<qint64>(1500, periodMs / 4)) {
        return;
    }

    int bufferedSamples = 0;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        bufferedSamples = m_audioBuffer.size();
    }
    if (bufferedSamples < minimumSamples) {
        return;
    }

    m_bridgeOwnedJtFallbackLastMode = modeSnapshot;
    m_bridgeOwnedJtFallbackLastSlot = completedSlot;
    m_bridgeOwnedJtFallbackLastDispatchMs = nowMs;
    qInfo().noquote()
        << "[TIMESYNC][FALLBACK]"
        << "mode=" << modeSnapshot
        << "slot=" << completedSlot
        << "msInSlot=" << msInSlot
        << "buffered=" << bufferedSamples
        << "target=" << targetSamples
        << "min=" << minimumSamples
        << "session=" << m_periodTimerSessionId;
    dispatchTimeSyncDecodeWhenReady(completedSlot, modeSnapshot, m_periodTimerSessionId, nowMs + 1000);
}

