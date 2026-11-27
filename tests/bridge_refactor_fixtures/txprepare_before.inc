bool DecodiumBridge::ensureTxAudioPrepared(const QString& msg, int txAudioFrequency, bool needPcm,
                                           QVector<float>* waveOut, QByteArray* pcmOut,
                                           QAudioFormat* formatOut, QAudioDevice* deviceOut,
                                           QString* errorOut)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    QString const mode = m_ft2LinkTxActive
        ? QStringLiteral("FT2LINK")
        : (m_cwTxActive ? QStringLiteral("CW") : m_mode.trimmed().toUpper());
    QString const message = msg.trimmed();
    bool const tciAudio = usingTciAudioInput();
    int const effectivePeriodMs = effectivePeriodMsForMode(mode);
    // La telemetria di stazione ha il suo messaggio: se lo slot MAM appena
    // chiuso non e' ancora stato potato, il generatore multi-stream rifarebbe
    // l'RR73 al posto della telemetria.
    bool const useMultiStream = multiStreamActive() && !m_telemetryTxActive;
    auto logResult = [&](bool cacheHit,
                         qint64 waveMs,
                         qint64 resolveMs,
                         qint64 formatMs,
                         qint64 pcmMs,
                         int waveSamples,
                         int pcmBytes,
                         const QAudioFormat& format,
                         const QAudioDevice& device) {
        txTimelineLog(QStringLiteral("[TX-TL] ensure_tx_audio mode=%1 msg=[%2] need_pcm=%3 cache=%4 total_ms=%5 wave_ms=%6 resolve_ms=%7 format_ms=%8 pcm_ms=%9 wave_samples=%10 pcm_bytes=%11 fmt=%12 dev=%13")
                      .arg(mode, message)
                      .arg(needPcm ? 1 : 0)
                      .arg(cacheHit ? 1 : 0)
                      .arg(totalTimer.elapsed())
                      .arg(waveMs)
                      .arg(resolveMs)
                      .arg(formatMs)
                      .arg(pcmMs)
                      .arg(waveSamples)
                      .arg(pcmBytes)
                      .arg(format.isValid() ? audioFormatToString(format) : QStringLiteral("not-required"),
                           device.description().isEmpty() ? QStringLiteral("not-required") : device.description()));
    };

    auto cacheMatchesBase = [&]() {
        return !m_txAudioCache.wave.isEmpty()
            && m_txAudioCache.mode == mode
            && m_txAudioCache.message == message
            && m_txAudioCache.txAudioFrequency == txAudioFrequency
            && m_txAudioCache.periodMs == effectivePeriodMs
            && m_txAudioCache.tciAudio == tciAudio
            && decodium::mamtx::cacheMatchesPayloadKind(m_txAudioCache.multiStream,
                                                          useMultiStream);
    };

    // 1.0.364+ — MAM multi-stream: bypassa la cache (force rebuild) cosi' il
    // multi-stream non serve mai PCM/wave mono stantio. Mono path invariato.
    if (!m_cwTxActive && !m_ft2LinkTxActive && !m_telemetryTxActive && !useMultiStream
        && cacheMatchesBase() && (!needPcm || !m_txAudioCache.pcm.isEmpty())) {
        if (needPcm) {
            if (m_cachedTxOutputDeviceValid
                && m_cachedTxOutputDeviceName == m_audioOutputDevice
                && m_cachedTxOutputDeviceId == m_audioOutputDeviceId
                && m_txAudioCache.outputDeviceName == m_audioOutputDevice
                && m_txAudioCache.outputDeviceDescription == m_cachedTxOutputDevice.description()
                && m_txAudioCache.outputChannel == m_audioOutputChannel
                && m_txAudioCache.outputFormat.isValid()) {
                if (waveOut) *waveOut = m_txAudioCache.wave;
                if (pcmOut) *pcmOut = m_txAudioCache.pcm;
                if (formatOut) *formatOut = m_txAudioCache.outputFormat;
                if (deviceOut) *deviceOut = m_cachedTxOutputDevice;
                logResult(true, 0, 0, 0, 0,
                          m_txAudioCache.wave.size(),
                          m_txAudioCache.pcm.size(),
                          m_txAudioCache.outputFormat,
                          m_cachedTxOutputDevice);
                scheduleIdleAudioBufferRelease(180000);
                return true;
            }
            QElapsedTimer phaseTimer;
            phaseTimer.start();
            bool found = false;
            QAudioDevice const device = resolveTxOutputDevice(&found);
            qint64 const resolveMs = phaseTimer.elapsed();
            phaseTimer.restart();
            QAudioFormat const format = chooseTxAudioFormat(device);
            qint64 const formatMs = phaseTimer.elapsed();
            if (m_txAudioCache.outputDeviceName == m_audioOutputDevice
                && m_txAudioCache.outputDeviceDescription == device.description()
                && m_txAudioCache.outputChannel == m_audioOutputChannel
                && sameAudioFormat(m_txAudioCache.outputFormat, format)) {
                if (waveOut) *waveOut = m_txAudioCache.wave;
                if (pcmOut) *pcmOut = m_txAudioCache.pcm;
                if (formatOut) *formatOut = m_txAudioCache.outputFormat;
                if (deviceOut) *deviceOut = device;
                Q_UNUSED(found)
                logResult(true, 0, resolveMs, formatMs, 0,
                          m_txAudioCache.wave.size(),
                          m_txAudioCache.pcm.size(),
                          m_txAudioCache.outputFormat,
                          device);
                scheduleIdleAudioBufferRelease(180000);
                return true;
            }
        } else {
            if (waveOut) *waveOut = m_txAudioCache.wave;
            if (pcmOut) pcmOut->clear();
            if (formatOut) *formatOut = QAudioFormat {};
            if (deviceOut) *deviceOut = QAudioDevice {};
            logResult(true, 0, 0, 0, 0,
                      m_txAudioCache.wave.size(),
                      0,
                      QAudioFormat {},
                      QAudioDevice {});
            scheduleIdleAudioBufferRelease(180000);
            return true;
        }
    }

    QString buildError;
    QElapsedTimer phaseTimer;
    phaseTimer.start();
    QVector<float> wave;
    if (m_ft2LinkTxActive) {
        wave = m_pendingFt2LinkWave;
        if (wave.isEmpty()) {
            buildError = QStringLiteral("Piano TX FT2-Link vuoto");
        }
    } else if (m_cwTxActive) {
        // CW-audio: tono sidetone (= txAudioFrequency) manipolato in Morse a WPM fisso.
        wave = decodium::txwave::generateCwWaveWpm(message, txAudioFrequency, m_cwWpm);
        if (wave.isEmpty()) {
            buildError = QStringLiteral("Generazione onda CW fallita");
        }
    } else if (useMultiStream) {
        QStringList streamDescription;
        streamDescription.reserve(m_mamMessages.size());
        for (int i = 0; i < m_mamMessages.size(); ++i) {
            streamDescription.append(QStringLiteral("%1Hz:[%2]")
                                         .arg(m_mamF0sHz.value(i))
                                         .arg(m_mamMessages.at(i)));
        }
        bridgeLog(QStringLiteral("MAM composite audio build: streams=%1 %2")
                      .arg(m_mamMessages.size())
                      .arg(streamDescription.join(QStringLiteral(" | "))));
        wave = generateMultiStreamFtxWave(mode, m_mamMessages, m_mamF0sHz, &buildError,
                                          m_mamPowerBudget ? m_mamGains : QVector<float> {});
    } else {
        wave = buildTxWaveformForMessage(mode, message, txAudioFrequency,
                                         effectivePeriodMsForMode(mode) / 1000.0,
                                         &buildError);
    }
    qint64 const waveMs = phaseTimer.elapsed();
    if (wave.isEmpty()) {
        if (errorOut) {
            *errorOut = buildError.isEmpty() ? QStringLiteral("Generazione onda TX fallita") : buildError;
        }
        invalidateTxAudioCache();
        return false;
    }

    QByteArray pcm;
    QAudioFormat format;
    QAudioDevice device;
    qint64 resolveMs = 0;
    qint64 formatMs = 0;
    qint64 pcmMs = 0;
    if (needPcm) {
        bool found = false;
        phaseTimer.restart();
        device = resolveTxOutputDevice(&found);
        resolveMs = phaseTimer.elapsed();
        if (!found && !m_audioOutputDevice.trimmed().isEmpty()) {
            bridgeLog("TX audio cache: requested output device not found, fallback to default: " +
                      device.description() +
                      " selected_id=" + audioDeviceIdSettingForLog(audioDeviceIdForSettings(device)) +
                      " requested=[" + m_audioOutputDevice + "] requested_id=" +
                      audioDeviceIdSettingForLog(m_audioOutputDeviceId));
        }
        phaseTimer.restart();
        format = chooseTxAudioFormat(device);
        formatMs = phaseTimer.elapsed();
        phaseTimer.restart();
        pcm = buildTxPcmBuffer(wave, format, boundedAudioChannel(m_audioOutputChannel));
        pcmMs = phaseTimer.elapsed();
        if (pcm.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral(
                    "Audio TX: impossibile costruire PCM per il device selezionato | "
                    "device=\"%1\" formato=%2 canale=%3 modo=%4 freqAudio=%5 Hz msg=\"%6\"")
                    .arg(device.description().isEmpty() ? QStringLiteral("<default output>")
                                                        : device.description(),
                         audioFormatToString(format),
                         QString::number(m_audioOutputChannel),
                         mode,
                         QString::number(txAudioFrequency),
                         message);
            }
            invalidateTxAudioCache();
            return false;
        }
    }

    m_txAudioCache.mode = mode;
    m_txAudioCache.message = message;
    m_txAudioCache.txAudioFrequency = txAudioFrequency;
    m_txAudioCache.periodMs = effectivePeriodMs;
    m_txAudioCache.tciAudio = tciAudio;
    m_txAudioCache.multiStream = useMultiStream;
    m_txAudioCache.outputDeviceName = needPcm ? m_audioOutputDevice : QString {};
    m_txAudioCache.outputDeviceDescription = needPcm ? device.description() : QString {};
    m_txAudioCache.outputChannel = needPcm ? m_audioOutputChannel : 0;
    m_txAudioCache.outputFormat = needPcm ? format : QAudioFormat {};
    m_txAudioCache.wave = wave;
    m_txAudioCache.pcm = pcm;

    if (waveOut) *waveOut = m_txAudioCache.wave;
    if (pcmOut) *pcmOut = m_txAudioCache.pcm;
    if (formatOut) *formatOut = m_txAudioCache.outputFormat;
    if (deviceOut) *deviceOut = device;
    logResult(false, waveMs, resolveMs, formatMs, pcmMs,
              m_txAudioCache.wave.size(),
              m_txAudioCache.pcm.size(),
              m_txAudioCache.outputFormat,
              device);
    scheduleIdleAudioBufferRelease(180000);
    return true;
}

