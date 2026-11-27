void DecodiumBridge::onLegacyAudioSamples(QByteArray const& pcmSamples)
{
    if (pcmSamples.isEmpty()) {
        return;
    }

    int const sampleCount = pcmSamples.size() / static_cast<int>(sizeof(qint16));
    if (sampleCount <= 0) {
        return;
    }

    // The legacy waterfall and the decoder consume different signals.  Keep a
    // low-rate PCM probe so a visible waterfall cannot be mistaken for a live
    // decoder input when the two paths diverge.
    qint64 const audioDiagNowMs = QDateTime::currentMSecsSinceEpoch();
    if (audioDiagNowMs - m_lastLegacyAudioDiagLogMs >= 2000) {
        m_lastLegacyAudioDiagLogMs = audioDiagNowMs;
        qint64 sumSquares = 0;
        int peak = 0;
        int nonZero = 0;
        auto const *samples = reinterpret_cast<qint16 const *>(pcmSamples.constData());
        for (int i = 0; i < sampleCount; ++i) {
            int const value = samples[i];
            int const magnitude = qAbs(value);
            peak = qMax(peak, magnitude);
            sumSquares += static_cast<qint64>(value) * value;
            if (value != 0) {
                ++nonZero;
            }
        }
        double const rms = std::sqrt(static_cast<double>(sumSquares)
                                     / static_cast<double>(sampleCount)) / 32768.0;
        bridgeLog(QStringLiteral(
            "[LegacyAudio][PCM] samples=%1 rms=%2 peak=%3 nonzero=%4 mode=%5 monitor=%6")
                      .arg(sampleCount)
                      .arg(rms, 0, 'f', 5)
                      .arg(static_cast<double>(peak) / 32768.0, 0, 'f', 5)
                      .arg(nonZero)
                      .arg(m_mode)
                      .arg(m_monitoring ? 1 : 0));
    }

    bool const feedRecorder = m_wavManager && m_wavManager->recording();
    bool const feedSpectrum = m_legacyPcmSpectrumFeed;
    bool const ft2LinkMode = isFt2LinkApplicationMode(m_mode);
    bool const ft2LinkUsesDedicatedModernRx =
        ft2LinkMode
        && useModernSpectrumFeedWithLegacy()
        && useDedicatedModernAudioCaptureWithLegacy();
    bool const feedFt2Link = ft2LinkMode
        && !ft2LinkUsesDedicatedModernRx
        && m_monitoring
        && !m_transmitting
        && !m_tuning;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (ft2LinkMode && nowMs - m_lastFt2LinkLegacyRxTapLogMs >= 2000) {
        m_lastFt2LinkLegacyRxTapLogMs = nowMs;
        qint64 const lastEmitAgeMs = m_lastFt2LinkLegacyRxEmitMs > 0
            ? nowMs - m_lastFt2LinkLegacyRxEmitMs
            : -1;
        bridgeLog(QStringLiteral(
            "[Ft2Link][RXTAP] source=legacy samples=%1 emit=%2 spectrum=%3 recorder=%4 mode=%5 monitor=%6 tx=%7 tune=%8 pending=%9 scheduled=%10 lastEmitAgeMs=%11 dedicatedModern=%12")
            .arg(sampleCount)
            .arg(feedFt2Link ? 1 : 0)
            .arg(feedSpectrum ? 1 : 0)
            .arg(feedRecorder ? 1 : 0)
            .arg(m_mode)
            .arg(m_monitoring ? 1 : 0)
            .arg(m_transmitting ? 1 : 0)
            .arg(m_tuning ? 1 : 0)
            .arg(m_ft2LinkLegacyRxPending.size())
            .arg(m_ft2LinkLegacyRxDrainScheduled ? 1 : 0)
            .arg(lastEmitAgeMs)
            .arg(ft2LinkUsesDedicatedModernRx ? 1 : 0));
    }
    if (!feedFt2Link && !m_ft2LinkLegacyRxPending.isEmpty()) {
        m_ft2LinkLegacyRxPending.clear();
    }
    if (!feedRecorder && !feedSpectrum && !feedFt2Link) {
        return;
    }

    QVector<short> linearSamples;
    if (feedRecorder || feedFt2Link) {
        linearSamples.reserve(sampleCount);
    }

    auto const* bytes = reinterpret_cast<uchar const*>(pcmSamples.constData());
    for (int i = 0; i < sampleCount; ++i) {
        qint16 const sample = qFromLittleEndian<qint16>(bytes + i * static_cast<int>(sizeof(qint16)));
        if (feedRecorder || feedFt2Link) {
            linearSamples.append(sample);
        }
        if (feedSpectrum) {
            m_wfRing[m_wfRingPos % WF_RING_SIZE] = sample;
            ++m_wfRingPos;
        }
    }

    if (feedRecorder) {
        m_wavManager->feedSamples(linearSamples);
    }
    if (feedFt2Link) {
        if (!linearSamples.isEmpty()) {
            m_ft2LinkLegacyRxPending += linearSamples;
        }
        // Keep the legacy audio callback light: buffer samples here and let
        // the Qt event loop drain toward FT2-Link at a controlled cadence.
        static constexpr int kFt2LinkLegacyDrainMs = 200;
        static constexpr int kFt2LinkLegacyMaxPendingSamples = 24000;
        if (m_ft2LinkLegacyRxPending.size () > kFt2LinkLegacyMaxPendingSamples) {
            m_ft2LinkLegacyRxPending.erase (
                m_ft2LinkLegacyRxPending.begin (),
                m_ft2LinkLegacyRxPending.begin ()
                    + (m_ft2LinkLegacyRxPending.size ()
                       - kFt2LinkLegacyMaxPendingSamples));
        }
        if (!m_ft2LinkLegacyRxPending.isEmpty()
            && !m_ft2LinkLegacyRxDrainScheduled) {
            m_ft2LinkLegacyRxDrainScheduled = true;
            QTimer::singleShot(kFt2LinkLegacyDrainMs, this, [this]() {
                m_ft2LinkLegacyRxDrainScheduled = false;

                bool const canEmit = isFt2LinkApplicationMode(m_mode)
                    && m_monitoring
                    && !m_transmitting
                    && !m_tuning;
                if (!canEmit) {
                    m_ft2LinkLegacyRxPending.clear();
                    return;
                }
                if (m_ft2LinkLegacyRxPending.isEmpty()) {
                    return;
                }

                qint64 const drainNowMs = QDateTime::currentMSecsSinceEpoch();
                m_lastFt2LinkLegacyRxEmitMs = drainNowMs;
                QVector<short> batchedSamples;
                batchedSamples.swap(m_ft2LinkLegacyRxPending);
                if (drainNowMs - m_lastFt2LinkLegacyRxEmitLogMs >= 2000) {
                    m_lastFt2LinkLegacyRxEmitLogMs = drainNowMs;
                    Ft2LinkPcmStats const stats = ft2LinkPcmStats(batchedSamples);
                    bridgeLog(QStringLiteral(
                        "[Ft2Link][RXEMIT] source=legacy samples=%1 rms=%2 peak=%3 nonzero=%4 aboveNoise=%5")
                        .arg(batchedSamples.size())
                        .arg(stats.rms, 0, 'f', 4)
                        .arg(stats.peak, 0, 'f', 4)
                        .arg(stats.nonZero)
                        .arg(stats.aboveNoise));
                }
                emit ft2LinkRxSamplesReady(std::move(batchedSamples),
                                           static_cast<quint64>(drainNowMs));
            });
        }
    } else if (!m_ft2LinkLegacyRxPending.isEmpty()) {
        m_ft2LinkLegacyRxPending.clear();
    }
    if (feedSpectrum) {
        m_lastLegacyPcmSampleMs = nowMs;
    }
}
