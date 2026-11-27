void DecodiumBridge::dispatchTimeSyncDecodeWhenReady(qint64 completedUtcSlot,
                                                     const QString& modeSnapshot,
                                                     quint64 sessionId,
                                                     qint64 deadlineMs)
{
#if DECODIUM_HAS_SSTV
    if (m_sstvWorkspaceActive) {
        return;
    }
#endif
        if (!m_monitoring
        || sessionId != m_periodTimerSessionId
        || m_mode != modeSnapshot
        || !isTimeSyncDecodeMode(modeSnapshot)) {
        if (bridgeOwnsLegacyJtRxWhenCatSuppressed(modeSnapshot)
            || bridgeOwnsFst4Audio(modeSnapshot)) {
            qInfo().noquote()
                << "[TIMESYNC][DISPATCH_SKIP]"
                << "mode=" << modeSnapshot
                << "reason=state"
                << "monitoring=" << (m_monitoring ? 1 : 0)
                << "session=" << sessionId
                << "currentSession=" << m_periodTimerSessionId
                << "currentMode=" << m_mode
                << "timeSync=" << (isTimeSyncDecodeMode(modeSnapshot) ? 1 : 0);
        }
        return;
    }

    // The embedded legacy backend owns both capture and decode for these
    // modes on every supported platform. Avoid assembling an empty second
    // window in the bridge at the slot boundary; WAV/offline snapshots enter
    // feedAudioToDecoder() directly and are unaffected by this guard.
    if (usingLegacyBackendForRx()) {
        if (bridgeOwnsLegacyJtRxWhenCatSuppressed(modeSnapshot)
            || bridgeOwnsFst4Audio(modeSnapshot)) {
            qInfo().noquote()
                << "[TIMESYNC][DISPATCH_SKIP]"
                << "mode=" << modeSnapshot
                << "reason=legacy-backend-owns-rx";
        }
        m_pendingTimeSyncDecodeAudio.clear();
        m_pendingTimeSyncDecodeSlot = -1;
        m_pendingTimeSyncDecodeMode.clear();
        m_pendingTimeSyncDecodeActive = false;
        return;
    }

    if (m_rxAudioSuspendedForTx || m_transmitting || m_tuning) {
        int bufferedSamples = 0;
        {
            QMutexLocker locker(&m_audioBufferMutex);
            bufferedSamples = m_audioBuffer.size();
        }
        m_pendingTimeSyncDecodeAudio.clear();
        m_pendingTimeSyncDecodeSlot = -1;
        m_pendingTimeSyncDecodeMode.clear();
        m_pendingTimeSyncDecodeActive = false;

        static qint64 s_lastSuspendedDecodeSkipLogMs = 0;
        qint64 const logNowMs = QDateTime::currentMSecsSinceEpoch();
        if (logNowMs - s_lastSuspendedDecodeSkipLogMs >= 750) {
            s_lastSuspendedDecodeSkipLogMs = logNowMs;
            bridgeLog(QStringLiteral("time-sync decode skipped: mode=%1 slot=%2 tx=%3 tune=%4 rxSuspended=%5 buffered=%6 session=%7")
                          .arg(modeSnapshot)
                          .arg(completedUtcSlot)
                          .arg(m_transmitting ? 1 : 0)
                          .arg(m_tuning ? 1 : 0)
                          .arg(m_rxAudioSuspendedForTx ? 1 : 0)
                          .arg(bufferedSamples)
                          .arg(sessionId));
        }
        return;
    }

    int const targetSamples = targetDecodeSamplesForMode(modeSnapshot);
    if (!m_pendingTimeSyncDecodeActive
        || m_pendingTimeSyncDecodeSlot != completedUtcSlot
        || m_pendingTimeSyncDecodeMode != modeSnapshot) {
        m_pendingTimeSyncDecodeAudio.clear();
        m_pendingTimeSyncDecodeSlot = completedUtcSlot;
        m_pendingTimeSyncDecodeMode = modeSnapshot;
        m_pendingTimeSyncDecodeActive = true;

        QMutexLocker locker(&m_audioBufferMutex);
        if (targetSamples > 0 && m_audioBuffer.size() > targetSamples) {
            int const bufferSamples = m_audioBuffer.size();
            int const periodMs = periodMsForMode(modeSnapshot);
            qint64 const nowMs = correctedUtcEpochMs();
            int const msInSlot = (periodMs > 0)
                                     ? static_cast<int>(nowMs % static_cast<qint64>(periodMs))
                                     : 0;
            int const excessSamples = qMax(0, bufferSamples - targetSamples);
            int const estimatedCurrentSlotSamples =
                qBound(0, static_cast<int>((static_cast<qint64>(msInSlot) * SAMPLE_RATE + 999) / 1000),
                       targetSamples);
            int const currentSlotSamplesToKeep =
                qMin(excessSamples, estimatedCurrentSlotSamples);
            int const windowEnd = bufferSamples - currentSlotSamplesToKeep;
            int const windowStart = qMax(0, windowEnd - targetSamples);

            m_pendingTimeSyncDecodeAudio = m_audioBuffer.mid(windowStart, targetSamples);
            m_audioBuffer = m_audioBuffer.mid(windowEnd);
            m_audioBuffer.reserve(targetSamples);
            if (windowStart > 0 || currentSlotSamplesToKeep > 0) {
                bridgeLog(QStringLiteral("time-sync decode aligned buffered audio: mode=%1 buffer=%2 target=%3 droppedOld=%4 keptCurrent=%5 msInSlot=%6 slot=%7")
                              .arg(modeSnapshot)
                              .arg(bufferSamples)
                              .arg(targetSamples)
                              .arg(windowStart)
                              .arg(currentSlotSamplesToKeep)
                              .arg(msInSlot)
                              .arg(completedUtcSlot));
            }
        } else {
            m_pendingTimeSyncDecodeAudio = m_audioBuffer;
            m_audioBuffer.clear();
            m_audioBuffer.reserve(targetSamples > 0 ? targetSamples : m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
    }

    int currentSlotSamples = 0;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        currentSlotSamples = m_audioBuffer.size();
    }
    int const pendingSamples = m_pendingTimeSyncDecodeAudio.size();
    int const combinedSamples = pendingSamples + qMax(0, currentSlotSamples);
    qint64 const nowMs = correctedUtcEpochMs();
    if (targetSamples > 0 && combinedSamples < targetSamples && nowMs < deadlineMs) {
        int const missingSamples = targetSamples - combinedSamples;
        int const waitMs = qBound(20, (missingSamples * 1000 + SAMPLE_RATE - 1) / SAMPLE_RATE, 125);
        QTimer::singleShot(waitMs, this, [this, completedUtcSlot, modeSnapshot, sessionId, deadlineMs]() {
            dispatchTimeSyncDecodeWhenReady(completedUtcSlot, modeSnapshot, sessionId, deadlineMs);
        });
        return;
    }

    QVector<short> finalAudio = m_pendingTimeSyncDecodeAudio;
    int borrowedSamples = 0;
    int currentSamplesAfterBorrow = 0;
    if (targetSamples > 0 && finalAudio.size() < targetSamples) {
        int const needed = targetSamples - finalAudio.size();
        int const minimumSamples = minimumDecodeSamplesForMode(modeSnapshot);
        bool const consumeBorrowedSamples =
            isTimeSyncDecodeMode(modeSnapshot)
            && minimumSamples > 0
            && finalAudio.size() >= minimumSamples;
        QMutexLocker locker(&m_audioBufferMutex);
        borrowedSamples = qMin(needed, m_audioBuffer.size());
        if (borrowedSamples > 0) {
            // 1.0.166 — rollback al comportamento 1.0.157: borrow = PEEK,
            // non TAKE. Salvatore in 1.0.158 ha introdotto le 2 righe
            // sotto (mid+reserve) che drenavano i sample dal buffer dopo
            // averli copiati. Risultato: dopo TX (con m_audioBuffer
            // appena clearato e che si sta riempiendo), il primo slot
            // post-TX borrow="quasi tutto", strip → m_audioBuffer vuoto
            // → slot successivo trova ancora niente → decoder muto per
            // tutta la sessione, mentre il Panadapter usa m_asyncAudio
            // separato e mostra segnali. Diagnosticato via multi-agent.
            finalAudio += m_audioBuffer.mid(0, borrowedSamples);
            if (consumeBorrowedSamples) {
                // For normal time-synchronised windows the completed slot is
                // already usable and the borrow is only the short post-boundary
                // tail. Consuming it prevents the next FT8/FT4 window from
                // starting early with samples already used by the previous slot.
                m_audioBuffer = m_audioBuffer.mid(borrowedSamples);
                m_audioBuffer.reserve(targetSamples);
            }
        }
        currentSamplesAfterBorrow = m_audioBuffer.size();
    }
    if (targetSamples > 0 && finalAudio.size() > targetSamples) {
        finalAudio.resize(targetSamples);
    }

    int const minimumSamples = minimumDecodeSamplesForMode(modeSnapshot);
    bool const enoughForMode =
        minimumSamples <= 0 || finalAudio.size() >= minimumSamples;
    if (targetSamples > 0 && finalAudio.size() < targetSamples) {
        bridgeLog(QStringLiteral("time-sync decode %1: mode=%2 samples=%3 target=%4 min=%5 borrowed=%6 slot=%7")
                      .arg(enoughForMode ? QStringLiteral("partial dispatch")
                                         : QStringLiteral("settling timeout"))
                      .arg(modeSnapshot)
                      .arg(finalAudio.size())
                      .arg(targetSamples)
                      .arg(minimumSamples)
                      .arg(borrowedSamples)
                      .arg(completedUtcSlot));
    } else if (targetSamples > 0) {
        bridgeLog(QStringLiteral("time-sync decode dispatch: mode=%1 samples=%2 target=%3 borrowed=%4 remaining=%5 slot=%6")
                      .arg(modeSnapshot)
                      .arg(finalAudio.size())
                      .arg(targetSamples)
                      .arg(borrowedSamples)
                      .arg(currentSamplesAfterBorrow)
                      .arg(completedUtcSlot));
        if (bridgeOwnsLegacyJtRxWhenCatSuppressed(modeSnapshot)
            || bridgeOwnsFst4Audio(modeSnapshot)) {
            qInfo().noquote()
                << "[TIMESYNC][DISPATCH]"
                << "mode=" << modeSnapshot
                << "samples=" << finalAudio.size()
                << "target=" << targetSamples
                << "borrowed=" << borrowedSamples
                << "remaining=" << currentSamplesAfterBorrow
                << "slot=" << completedUtcSlot;
        }
    }

    m_pendingTimeSyncDecodeAudio.clear();
    m_pendingTimeSyncDecodeSlot = -1;
    m_pendingTimeSyncDecodeMode.clear();
    m_pendingTimeSyncDecodeActive = false;
    m_forcedDecodeAudioSnapshot = finalAudio;
    m_forcedDecodeAudioSnapshotActive = true;
    feedAudioToDecoder(completedUtcSlot);
}

