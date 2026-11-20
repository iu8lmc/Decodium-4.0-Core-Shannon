void DecodiumBridge::onDecoPortRxAudio(const QVector<short>& samples, quint64 captureTsNs)
{
    Q_UNUSED(captureTsNs)
    if (!m_decoPortUseRemote || samples.isEmpty() || !m_audioSink)
        return;
    if (m_transmitting || m_tuning || m_cwRemoteKeying)
        return;

    m_audioSink->injectExternalSamples(samples);
    m_decoPortRemoteSamples += samples.size();

    ssbMonitorAudio(samples);

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastDecoPortAudioLogMs >= 5000) {
        m_lastDecoPortAudioLogMs = nowMs;
        bridgeLog(QStringLiteral("DecoPort remote audio: %1 samples fed to the decoder (%2 s)")
                      .arg(m_decoPortRemoteSamples)
                      .arg(static_cast<double>(m_decoPortRemoteSamples) / SAMPLE_RATE, 0, 'f', 1));
    }
}

