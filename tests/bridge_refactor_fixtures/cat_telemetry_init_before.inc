    auto syncHamlibTelemetry = [this]() {
        if (!m_hamlibCat
            || !catSignalMatchesBackend(m_catBackend, QStringLiteral("hamlib"))
            || !m_hamlibCat->connected()) {
            updateRigTelemetry(0.0, 0.0, 0.0);
            return;
        }
        // 1.0.323 — passa ALC meter (0..100) e validità come telemetria separata.
        updateRigTelemetry(m_hamlibCat->powerWatts(), m_hamlibCat->swr(),
                           m_hamlibCat->alc(), m_hamlibCat->alcValid());
    };
    connect(m_hamlibCat, &DecodiumTransceiverManager::powerWattsChanged,
            this, syncHamlibTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::swrChanged,
            this, syncHamlibTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::alcChanged,   // 1.0.323
            this, syncHamlibTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::connectedChanged,
            this, syncHamlibTelemetry);
#if DECODIUM_HAS_SSTV
    connect(m_hamlibCat, &DecodiumTransceiverManager::connectedChanged,
            this, [this]() {
        if (!m_sstvRxRequested || !m_hamlibCat->connected()
            || !usingTciAudioInput()) {
            return;
        }
        const auto source = decodium::sstv::SstvAudioSourceKind::Tci;
        selectSstvRxSource(source, currentSstvAudioStreamId(source), true);
    });
#endif

    auto syncCat4OmTelemetry = [this]() {
        if (!m_cat4OmCat
            || m_catBackend != QStringLiteral("cat4om")
            || !m_cat4OmCat->connected()) {
            if (m_catBackend == QStringLiteral("cat4om")) {
                updateRigTelemetry(0.0, 0.0, 0.0);
            }
            return;
        }
        updateRigTelemetry(m_cat4OmCat->powerWatts(), m_cat4OmCat->swr(),
                           m_cat4OmCat->alc(), m_cat4OmCat->alcValid());
    };
    connect(m_cat4OmCat, &DecodiumCat4OmManager::powerWattsChanged,
            this, syncCat4OmTelemetry);
    connect(m_cat4OmCat, &DecodiumCat4OmManager::swrChanged,
            this, syncCat4OmTelemetry);
    connect(m_cat4OmCat, &DecodiumCat4OmManager::alcChanged,
            this, syncCat4OmTelemetry);
    connect(m_cat4OmCat, &DecodiumCat4OmManager::connectedChanged,
            this, syncCat4OmTelemetry);
    connect(m_hamlibCat, &DecodiumTransceiverManager::tciPcmSamplesReady,
            this, &DecodiumBridge::onTciPcmSamplesReady, Qt::QueuedConnection);
    connect(m_hamlibCat, &DecodiumTransceiverManager::tciModActiveChanged,
            this,
            [this](bool active) {
                if (active || !usingTciAudioInput() || !m_transmitting || m_tuning) {
                    return;
                }
                bridgeLog(QStringLiteral("TCI modulator inactive: finishing TX playback"));
                m_txPlaybackHoldUntilMs = 0;
                m_txPlaybackHardDeadlineMs = 0;
                m_txPlaybackReleasePending = false;
                m_txAudioRestartPending = false;
                QTimer::singleShot(0, this, [this]() {
                    if (m_transmitting && usingTciAudioInput() && !m_tuning) {
                        completeTxPlayback(QStringLiteral("tci-modulator-idle"));
                    }
                });
            },
            Qt::QueuedConnection);

