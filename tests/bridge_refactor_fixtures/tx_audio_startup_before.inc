    // TX audio: Modulator (QIODevice pull) + SoundOutput (QAudioSink Qt6)
    m_modulator  = new Modulator(48000, 15.0, this);
    m_soundOutput = new SoundOutput();

    // PTT giu' automatico quando il Modulator si ferma (fine TX/Tune).
    // Il device audio puo' consumare il QIODevice prima che il buffer hardware
    // sia realmente uscito: non abbassare PTT prima della durata della waveform.
    connect(m_modulator, &Modulator::stateChanged, this, [this](Modulator::ModulatorState st) {
        if (st == Modulator::Idle) {
            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            if (!m_tuning && m_transmitting && m_txPlaybackHoldUntilMs > nowMs) {
                qint64 const delayMs = qMax<qint64>(1, m_txPlaybackHoldUntilMs - nowMs);
                if (!m_txPlaybackReleasePending) {
                    m_txPlaybackReleasePending = true;
                    bridgeLog(QStringLiteral("modulator idle early: holding PTT for %1 ms").arg(delayMs));
                    QTimer::singleShot(delayMs, this, [this]() {
                        if (m_modulator && m_modulator->isActive()) {
                            m_txPlaybackReleasePending = false;
                            return;
                        }
                        finishModulatorIdlePlayback(QStringLiteral("held-idle"));
                    });
                }
                return;
            }
            finishModulatorIdlePlayback(QStringLiteral("idle"));
        }
    });
    connect(m_soundOutput, &SoundOutput::error, this, [this](const QString& msg) {
        QString text = msg.trimmed();
        if (!text.startsWith(QStringLiteral("Audio "), Qt::CaseInsensitive)) {
            text = QStringLiteral("Audio TX: ") + text;
        }
        bridgeLog("SoundOutput error: " + text);
        emit errorMessage(text);
        if ((m_bridgeAudioLegacyTxActive || m_ft2LinkTxActive) && m_transmitting) {
            m_txPlaybackHoldUntilMs = 0;
            m_txPlaybackHardDeadlineMs = 0;
            m_txPlaybackReleasePending = false;
            if (m_txPcmBuffer && m_txPcmBuffer->isOpen()) {
                m_txPcmBuffer->seek(m_txPcmBuffer->size());
            }
            QTimer::singleShot(0, this, [this]() {
                if ((m_bridgeAudioLegacyTxActive || m_ft2LinkTxActive) && m_transmitting) {
                    if (m_ft2LinkTxActive && !m_bridgeAudioLegacyTxActive && m_soundOutput) {
                        finishModulatorIdlePlayback(QStringLiteral("sound-output-error"));
                    } else {
                        completeTxPlayback(QStringLiteral("sound-output-error"), true);
                    }
                }
            });
        }
    });
    connect(m_soundOutput, &SoundOutput::status, this, [](const QString& msg) {
        bridgeLog("SoundOutput: " + msg);
    });

