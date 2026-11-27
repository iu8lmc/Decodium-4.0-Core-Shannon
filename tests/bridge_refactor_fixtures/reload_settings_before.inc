void DecodiumBridge::reloadBridgeSettingsFromPersistentStore()
{
    bridgeLog(QStringLiteral("reloadBridgeSettingsFromPersistentStore: applying legacy setup changes to QML bridge"));

    QString const previousCatBackend = m_catBackend;
    QString const previousAudioInputDevice = m_audioInputDevice;
    int const previousAudioInputChannel = m_audioInputChannel;

    loadSettings();
    applyAutoCqBurstCadenceToLegacyBackend();
    updatePeriodTicksMax();
    if (m_bandManager) {
        m_bandManager->setCurrentMode(m_mode);
        m_bandManager->updateFromFrequency(m_frequency);
    }

    scheduleAudioDeviceRefresh(250, false);

    if (m_cat4OmCat) {
        m_cat4OmCat->loadSettings();
    }
    if (m_omniRigCat) {
        m_omniRigCat->loadSettings();
    }
    if (m_hamlibCat) {
        m_hamlibCat->loadSettings();
    }

    if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend) && m_hamlibCat && m_hamlibCat->connected()) {
        m_hamlibCat->disconnectRig();
    }

    bool const audioInputConfigChanged =
        previousAudioInputDevice != m_audioInputDevice
        || previousAudioInputChannel != m_audioInputChannel;
    if (m_soundInput) {
        m_soundInput->setInputGain(rxInputGainFromLevel(m_rxInputLevel));
        if (m_monitoring
            && audioInputConfigChanged
            && !usingLegacyBackendForTx()
            && !useLegacyRigControlFallback(m_legacyBackend, m_catBackend)) {
            bridgeLog(QStringLiteral("reloadBridgeSettingsFromPersistentStore: restarting RX audio after input change"));
            stopAudioCapture();
            startAudioCapture();
        } else if (m_monitoring && !audioInputConfigChanged) {
            bridgeLog(QStringLiteral("reloadBridgeSettingsFromPersistentStore: keeping RX audio stream active"));
        }
    }
    if (m_soundOutput) {
        m_soundOutput->setAttenuation(txAttenuationFromSlider(m_txOutputLevel));
    }
    if (m_txAudioSink) {
        m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    }

    if (previousCatBackend != m_catBackend) {
        if (previousCatBackend == QStringLiteral("cat4om") && m_cat4OmCat->connected()) {
            m_cat4OmCat->disconnectRig();
        } else if (previousCatBackend == QStringLiteral("omnirig") && m_omniRigCat->connected()) {
            m_omniRigCat->disconnectRig();
        } else if (isHamlibFamilyBackend(previousCatBackend) && m_hamlibCat->connected()) {
            m_hamlibCat->disconnectRig();
        }
        m_catConnected = false;
        m_catRigName.clear();
        m_catMode.clear();
        emit catConnectedChanged();
        emit catRigNameChanged();
        emit catModeChanged();
        refreshPskReporterLocalStation();
        updateRigTelemetry(0.0, 0.0);
    } else {
        auto const activeConnected = [this]() -> bool {
            QObject* obj = catManagerObj();
            return obj ? obj->property("connected").toBool() : false;
        }();
        auto const activeRigName = [this, activeConnected]() -> QString {
            QObject* obj = catManagerObj();
            return (obj && activeConnected) ? obj->property("rigName").toString() : QString {};
        }();
        auto const activeMode = [this]() -> QString {
            QObject* obj = catManagerObj();
            return obj ? obj->property("mode").toString() : QString {};
        }();

        if (m_catConnected != activeConnected) {
            m_catConnected = activeConnected;
            emit catConnectedChanged();
        }
        if (m_catRigName != activeRigName) {
            m_catRigName = activeRigName;
            emit catRigNameChanged();
        }
        if (m_catMode != activeMode) {
            m_catMode = activeMode;
            emit catModeChanged();
        }
        refreshPskReporterLocalStation();
        if (isHamlibFamilyBackend(m_catBackend) && m_hamlibCat && m_hamlibCat->connected()) {
            updateRigTelemetry(m_hamlibCat->powerWatts(), m_hamlibCat->swr(),
                               m_hamlibCat->alc(), m_hamlibCat->alcValid());  // 1.0.323
        } else if (m_catBackend == QStringLiteral("cat4om")
                   && m_cat4OmCat && m_cat4OmCat->connected()) {
            updateRigTelemetry(m_cat4OmCat->powerWatts(), m_cat4OmCat->swr(),
                               m_cat4OmCat->alc(), m_cat4OmCat->alcValid());
        } else {
            updateRigTelemetry(0.0, 0.0, 0.0);
        }
    }

    emit callsignChanged();
    emit gridChanged();
    emit frequencyChanged();
    emit periodMillisecondsChanged();
    emit modeChanged();
    emit rxFrequencyChanged();
    emit txFrequencyChanged();
    emit holdTxFreqChanged();
    emit rxInputLevelChanged();
    emit autoRxInputLevelChanged();
    emit txOutputLevelChanged();
    emit audioInputDevicesChanged();
    emit audioOutputDevicesChanged();
    emit audioInputDeviceChanged();
    emit sstvRxSourceDeviceChanged();
    emit audioOutputDeviceChanged();
    emit audioInputChannelChanged();
    emit audioOutputChannelChanged();
    emit autoSeqChanged();
    emit waitPounceActiveChanged();
    emit asyncTxEnabledChanged();
    emit asyncDecodeEnabledChanged();
    emit txPeriodChanged();
    emit alt12EnabledChanged();
    emit quickQsoEnabledChanged();
    emit autoCqBurstCallsChanged();
    emit autoCqListenCyclesChanged();
    emit tx1Changed();
    emit tx2Changed();
    emit tx3Changed();
    emit tx4Changed();
    emit tx5Changed();
    emit tx6Changed();
    emit txMessagesChanged();
    emit currentTxChanged();
    emit currentTxMessageChanged();
    emit dxCallChanged();
    emit dxGridChanged();
    emit reportReceivedChanged();
    emit sendRR73Changed();
    emit fontScaleChanged();
    emit uiPaletteIndexChanged();
    emit catBackendChanged();
    emit catManagerChanged();
    emit alertOnCqChanged();
    emit alertOnMyCallChanged();
    emit recordRxEnabledChanged();
    emit recordTxEnabledChanged();
    emit stationNameChanged();
    emit stationQthChanged();
    emit stationRigInfoChanged();
    emit stationAntennaChanged();
    emit stationPowerWattsChanged();
    emit wsprPowerDbmChanged();
    emit autoStartMonitorOnStartupChanged();
    emit startFromTx2Changed();
    emit vhfUhfFeaturesChanged();
    emit directLogQsoChanged();
    emit confirm73Changed();
    emit contestExchangeChanged();
    emit contestNumberChanged();
    emit swlModeChanged();
    emit splitModeChanged();
    emit txWatchdogModeChanged();
    emit txWatchdogTimeChanged();
    emit txWatchdogCountChanged();
    emit filterCqOnlyChanged();
    emit cqFilterLevelChanged();
    emit activeReadyProfileChanged();
    emit filterMyCallOnlyChanged();
    emit contestTypeChanged();
    emit zapEnabledChanged();
    emit avgDecodeEnabledChanged();
    emit ft8ApEnabledChanged();
    emit deepSearchEnabledChanged();
    emit pskReporterEnabledChanged();
    emit pskReporterTimeSpanMinutesChanged();
    emit pskMapSpotWindowMinutesChanged();
    emit cloudlogEnabledChanged();
    emit cloudlogUrlChanged();
    emit cloudlogApiKeyChanged();
    emit qrzLogbookEnabledChanged();
    emit qrzLogbookApiKeyChanged();
    emit qrzLogbookReplaceDuplicatesChanged();
    emit lotwEnabledChanged();
    if (m_lotwUsers.isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            if (!m_shuttingDown && m_lotwUsers.isEmpty()) {
                updateLotwUsers();
            }
        });
    } else {
        refreshDecodeListDxcc();
    }
    emit showUsStateChanged();
    emit usStateDataChanged();
    emit wsprUploadEnabledChanged();
    emit nfaChanged();
    emit nfbChanged();
    emit ndepthChanged();
    emit singleDecodeChanged();
    emit ncontestChanged();
    emit specialOperationActivityChanged();
    emit foxModeChanged();
    emit houndModeChanged();
    emit colorCQChanged();
    emit colorMyCallChanged();
    emit colorDXEntityChanged();
    emit color73Changed();
    emit colorB4Changed();
    emit colorDecodeTextChanged();
    emit colorTxMessageChanged();
    emit colorNewDxccChanged();
    emit colorNewDxccBandChanged();
    emit colorNewContinentChanged();
    emit colorNewContinentBandChanged();
    emit colorNewCqZoneChanged();
    emit colorNewCqZoneBandChanged();
    emit colorNewItuZoneChanged();
    emit colorNewItuZoneBandChanged();
    emit colorNewGridChanged();
    emit colorNewGridBandChanged();
    emit colorNewCallChanged();
    emit colorNewCallBandChanged();
    emit colorLotwUserChanged();
    for (const QString& prop : decodeColorPropertyNames())
        emit decodeColorBoldChanged(prop, m_decodeColorBold.value(prop, decodeColorBoldDefault(prop)));
    emit b4StrikethroughChanged();
    emit alertSoundsEnabledChanged();
}
