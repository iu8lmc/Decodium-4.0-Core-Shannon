void DecodiumBridge::saveSettings()
{
    saveSettingsInternal(false);
}

void DecodiumBridge::saveSettingsAsync()
{
    bool expected = false;
    if (!m_asyncSettingsSavePending.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        m_asyncSettingsSaveAgain.store(true, std::memory_order_release);
        return;
    }
    saveSettingsInternal(true);
}

void DecodiumBridge::saveSettingsInternal(bool asynchronous)
{
    if (!asynchronous && m_asyncSettingsSavePending.load(std::memory_order_acquire)) {
        if (!m_settingsShutdownInProgress) {
            // Keep normal UI setters non-blocking if a previous snapshot is
            // still flushing; the worker completion will schedule the newest
            // snapshot after the current one.
            m_asyncSettingsSaveAgain.store(true, std::memory_order_release);
            return;
        }
        // The close path is the one place where waiting is intentional: it
        // must not let an older staged snapshot overwrite the final sync save.
        settingsWriteThreadPool().waitForDone(5000);
        m_asyncSettingsSavePending.store(false, std::memory_order_release);
        m_asyncSettingsSaveAgain.store(false, std::memory_order_release);
    }

    MainThreadTraceScope trace(QStringLiteral("save_settings"),
                               QStringLiteral("mode=%1 cat=%2 monitoring=%3 tx=%4")
                                   .arg(m_mode, m_catBackend)
                                   .arg(m_monitoring ? 1 : 0)
                                   .arg(m_transmitting ? 1 : 0),
                               25);
    // QSettings::setValue() stages changes in memory; its destructor/sync is
    // the expensive disk operation. Keep the staged objects alive and hand
    // them to a single worker for the debounced QML path.
    QSettings* const stagedSettings =
        new QSettings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    QSettings& s = *stagedSettings;
    decodium::beginActiveSettingsProfile(s);
    s.setValue("callsign", m_callsign);
    s.setValue("grid", m_grid);
    s.setValue("frequency", m_frequency);
    QString saveMode = canonicalApplicationDecodeMode(m_mode);
    if (saveMode.isEmpty()) {
        saveMode = QStringLiteral("FT8");
        bridgeLog(QStringLiteral("saveSettings: refused invalid application mode %1, saving FT8")
                      .arg(m_mode));
    }
    s.setValue("mode", saveMode);
    s.setValue("audioInputDevice", m_audioInputDevice);
    s.setValue("audioOutputDevice", m_audioOutputDevice);
    s.setValue("audioInputDeviceId", m_audioInputDeviceId);
    s.setValue("audioOutputDeviceId", m_audioOutputDeviceId);
    s.setValue("audioInputChannel", m_audioInputChannel);
    s.setValue("audioOutputChannel", m_audioOutputChannel);
    s.setValue("SoundInName", m_audioInputDevice);
    s.setValue("SoundOutName", m_audioOutputDevice);
    QString const audioInputChannelSetting = audioChannelSettingValue(m_audioInputChannel);
    QString const audioOutputChannelSetting = audioChannelSettingValue(m_audioOutputChannel);

    QSettings* const stagedLegacySettings =
        new QSettings(legacyIniPath(), QSettings::IniFormat);
    QSettings& legacyIni = *stagedLegacySettings;
    legacyIni.beginGroup(QStringLiteral("MultiSettings"));
    QString legacyGroupName = legacyIni.value(QStringLiteral("CurrentName")).toString();
    legacyIni.endGroup();
    if (legacyGroupName.isEmpty()) {
        legacyGroupName = QStringLiteral("Default");
    }
    QStringList legacyGroups {legacyGroupName, QStringLiteral("Configuration"), QStringLiteral("Default")};
    legacyGroups.removeDuplicates();
    auto setLegacyIniValue = [&legacyIni, &legacyGroups](const QString& key, const QVariant& value) {
        legacyIni.setValue(key, value);
        for (QString const& group : legacyGroups) {
            if (group.isEmpty()) {
                continue;
            }
            legacyIni.beginGroup(group);
            legacyIni.setValue(key, value);
            legacyIni.endGroup();
        }
    };
    legacyIni.setValue(QStringLiteral("MyCall"), m_callsign.trimmed().toUpper());
    legacyIni.setValue(QStringLiteral("MyGrid"), m_grid.trimmed().toUpper());
    legacyIni.setValue(QStringLiteral("SelectedActivity"), m_specialOperationActivity);
    legacyIni.setValue(QStringLiteral("SpecialOpActivity"), m_specialOperationActivity != kSpecialOpNone);
    s.setValue("AudioInputChannel", audioInputChannelSetting);
    s.setValue("AudioOutputChannel", audioOutputChannelSetting);
    setLegacyIniValue(QStringLiteral("SoundInName"), m_audioInputDevice);
    setLegacyIniValue(QStringLiteral("SoundOutName"), m_audioOutputDevice);
    setLegacyIniValue(QStringLiteral("AudioInputChannel"), audioInputChannelSetting);
    setLegacyIniValue(QStringLiteral("AudioOutputChannel"), audioOutputChannelSetting);
    s.setValue("rxInputLevel", m_rxInputLevel);
    s.setValue("autoRxInputLevel", m_autoRxInputLevel);
    s.setValue("txOutputLevel", m_txOutputLevel);
    s.setValue("txDisabledMask", m_txDisabledMask);
    s.setValue("hideGhostDecodes", m_hideGhostDecodes);
    s.setValue("decodeShowPeriodSeparator", m_decodeShowPeriodSeparator);
    s.setValue("nfa", m_nfa);
    s.setValue("nfb", m_nfb);
    s.setValue("ndepth", m_ndepth);
    s.setValue("SingleDecode", m_singleDecode);
    s.setValue("SelectedActivity", m_specialOperationActivity);
    s.setValue("SpecialOpActivity", m_specialOperationActivity != kSpecialOpNone);
    s.setValue("NDepth", legacyCompatibleDecodeDepthBits());
    s.setValue("decodeDepthMigratedFromLegacy", true);
    setLegacyIniValue(QStringLiteral("SingleDecode"), m_singleDecode);
    s.setValue("tx6", m_tx6);
    s.setValue("customTx6Message", m_customCqMessage);
    s.remove("dxCall");
    s.remove("dxGrid");
    s.remove("tx1");
    s.remove("tx2");
    s.remove("tx3");
    s.remove("tx4");
    s.remove("tx5");
    s.setValue("rxFrequency", m_rxFrequency);
    s.setValue("txFrequency", m_txFrequency);
    s.setValue("TXFrequency", m_txFrequency);
    s.setValue("HoldTxFreq", m_holdTxFreq);
    s.setValue("holdTxFreq", m_holdTxFreq);
    s.setValue("fontScale", m_fontScale);
    // extra features
    s.setValue("alertOnCq",       m_alertOnCq);
    s.setValue("alertOnMyCall",   m_alertOnMyCall);
    s.setValue("alert_CQ",        m_alertOnCq);
    s.setValue("alert_MyCall",    m_alertOnMyCall);
    s.setValue("recordRxEnabled", m_recordRxEnabled);
    s.setValue("recordTxEnabled", m_recordTxEnabled);
    s.setValue("StationName",     m_stationName);
    s.setValue("StationQTH",      m_stationQth);
    s.setValue("StationRigInfo",  m_stationRigInfo);
    s.setValue("StationAntenna",  m_stationAntenna);
    s.setValue("StationPowerW",   m_stationPowerWatts);
    s.setValue("WsprPowerDbm",    m_wsprPowerDbm);
    s.setValue("dBm",             m_wsprPowerDbm);
    s.setValue("MonitorOFF",      false);
    s.setValue("autoSeq",         m_autoSeq);
    s.setValue("AutoSeq",         m_autoSeq);
    s.setValue("WaitPounceActive", m_waitPounceActive);
    s.setValue("startFromTx2",    m_startFromTx2);
    s.setValue("vhfUhfFeatures",  m_vhfUhfFeatures);
    s.setValue("VHFUHF",          m_vhfUhfFeatures);
    s.setValue("directLogQso",    m_directLogQso);
    s.setValue("AutoSpotEnabled", m_autoSpotEnabled);
    s.setValue("confirm73",       m_confirm73);
    s.setValue("contestExchange", m_contestExchange);
    s.setValue("contestNumber",   m_contestNumber);
    s.setValue("swlMode",         m_swlMode);
    s.setValue("splitMode",       m_splitMode);
    s.setValue("txWatchdogMode",  m_txWatchdogMode);
    s.setValue("txWatchdogTime",  m_txWatchdogTime);
    s.setValue("txWatchdogCount", m_txWatchdogCount);
    int const legacyTxWatchdogMinutes = (m_txWatchdogMode == 1 && m_txWatchdogTime > 0)
        ? m_txWatchdogTime
        : 0;
    s.setValue("TxWatchdog",      legacyTxWatchdogMinutes);
    s.setValue("TXWatchdog",      legacyTxWatchdogMinutes);
    s.setValue("filterCqOnly",    m_filterCqOnly);
    s.setValue("cqFilterLevel",   m_cqFilterLevel);  // 1.0.383
    s.setValue("filterMyCallOnly",m_filterMyCallOnly);
    s.setValue("filtersBypassed", m_filtersBypassed);
    s.setValue("FiltersBypassed", m_filtersBypassed);
    s.setValue("contestType",     m_contestType);
    s.setValue("zapEnabled",      m_zapEnabled);
    s.setValue("deepSearchEnabled",m_deepSearchEnabled);
    s.setValue("avgDecodeEnabled", m_avgDecodeEnabled);
    s.setValue("coherentAvgEnabled",   m_coherentAvgEnabled);
    s.setValue("neuralSyncEnabled",    m_neuralSyncEnabled);
    s.setValue("turboFeedbackEnabled", m_turboFeedbackEnabled);
    s.setValue("fastLdpcEnabled", m_fastLdpcEnabled);
    s.setValue("advAutoModeEnabled",   m_advAutoModeEnabled);
    s.setValue("FT8AP",             m_ft8ApEnabled);
    s.setValue("asyncDecodeEnabled",m_asyncDecodeEnabled);
    s.setValue("pskReporterEnabled",m_pskReporterEnabled);
    s.setValue("PSKReporter",m_pskReporterEnabled);
    s.setValue("PSKReporterTCPIP", getSetting(QStringLiteral("PSKReporterTCPIP"), false).toBool());
    s.setValue("PskReporterTimeSpanMinutes", m_pskReporterTimeSpanMinutes);
    s.setValue("PskMapSpotWindowMinutes", m_pskMapSpotWindowMinutes);
    s.setValue("DirectVisualAudioCaptureUnsafe", m_directVisualAudioCaptureUnsafe);
    s.setValue("ftThreads",         m_ftThreads);
    s.setValue("ftThreadsAuto",     m_ftThreadsAuto);
    s.setValue("LowCpuMode",        m_lowCpuModeEnabled);
    s.setValue("lowCpuModeEnabled", m_lowCpuModeEnabled);
    s.setValue("catBackend",        m_catBackend);
    // TX QSO options
    s.remove("reportReceived");
    s.setValue("sendRR73",          m_sendRR73);
    s.setValue("txPeriod",          m_txPeriod);
    s.setValue("alt12Enabled",      m_alt12Enabled);
    s.setValue("quickQsoEnabled",   m_quickQsoEnabled);
    s.setValue("AutoCqBurstCalls",  m_autoCqBurstCalls);
    s.setValue("AutoCqMaxCycles", m_autoCqMaxCycles);
    s.setValue("AutoCqPauseSec", m_autoCqPauseSec);
    s.setValue("AutoCqListenCycles", m_autoCqListenCycles);
    // B7 — Colors
    s.setValue("colorCQ",        m_colorCQ);
    s.setValue("colorMyCall",    m_colorMyCall);
    s.setValue("colorDXEntity",  m_colorDXEntity);
    s.setValue("color73",        m_color73);
    s.setValue("colorB4",        m_colorB4);
    s.setValue("colorDecodeText",m_colorDecodeText);
    s.setValue("b4Strikethrough",m_b4Strikethrough);
    // WSJT-X-style highlighting palette
    s.setValue("colorTxMessage",        m_colorTxMessage);
    s.setValue("colorNewDxcc",          m_colorNewDxcc);
    s.setValue("colorNewDxccBand",      m_colorNewDxccBand);
    s.setValue("colorNewContinent",     m_colorNewContinent);
    s.setValue("colorNewContinentBand", m_colorNewContinentBand);
    s.setValue("colorNewCqZone",        m_colorNewCqZone);
    s.setValue("colorNewCqZoneBand",    m_colorNewCqZoneBand);
    s.setValue("colorNewItuZone",       m_colorNewItuZone);
    s.setValue("colorNewItuZoneBand",   m_colorNewItuZoneBand);
    s.setValue("colorNewGrid",          m_colorNewGrid);
    s.setValue("colorNewGridBand",      m_colorNewGridBand);
    s.setValue("colorNewCall",          m_colorNewCall);
    s.setValue("colorNewCallBand",      m_colorNewCallBand);
    s.setValue("colorLotwUser",         m_colorLotwUser);
    for (const QString& prop : decodeColorPropertyNames()) {
        s.setValue(decodeColorEnabledSettingKey(prop),
                   m_decodeColorEnabled.value(prop, true));
        s.setValue(decodeColorBoldSettingKey(prop),
                   m_decodeColorBold.value(prop, decodeColorBoldDefault(prop)));
        s.setValue(QStringLiteral("bg_%1").arg(prop), m_decodeColorBg.value(prop, QString()));
        s.setValue(QStringLiteral("bgEnabled_%1").arg(prop), m_decodeColorBgEnabled.value(prop, false));
    }
    // B8 — Alerts
    s.setValue("alertSoundsEnabled", m_alertSoundsEnabled);
    s.setValue("alert_Enabled",      m_alertSoundsEnabled);
    // Cloudlog
    s.setValue("CloudLog", m_cloudlogEnabled);
    s.setValue("DecoLogLink", m_decoLogLinkEnabled);
    s.setValue("DecoLogLinkPort", m_decoLogLinkPort);
    s.setValue("CloudLogApiUrl", m_cloudlogUrl);
    s.setValue("CloudLogApiKey",
               secure_settings::value_for_write(
                   secure_settings::service(m_callsign.trimmed().toUpper()),
                   QStringLiteral("CloudLogApiKey"),
                   m_cloudlogApiKey));
    s.remove(QStringLiteral("cloudlogEnabled"));
    s.remove(QStringLiteral("cloudlogUrl"));
    s.remove(QStringLiteral("cloudlogApiKey"));
    // QRZ Logbook
    s.setValue("qrzLogbookEnabled", m_qrzLogbookEnabled);
    s.setValue("qrzLogbookApiKey",
               secure_settings::value_for_write(secure_settings::service(m_callsign.trimmed().toUpper()),
                                                QStringLiteral("qrzLogbookApiKey"),
                                                m_qrzLogbookApiKey));
    s.setValue("qrzLogbookReplaceDuplicates", m_qrzLogbookReplaceDuplicates);
    // LotW / ADIF
    s.setValue("lotwEnabled",      m_lotwEnabled);
    s.setValue("showUsState",      m_showUsState);
    // WSPR upload
    s.setValue("wsprUploadEnabled", m_wsprUploadEnabled);
    // DX Cluster
    if (!asynchronous && m_dxCluster) m_dxCluster->saveSettings();
    // CAT managers — persist serial port, baud rate, rig name etc.
    if (!asynchronous && m_cat4OmCat)   m_cat4OmCat->saveSettings();
    if (!asynchronous && m_hamlibCat)   m_hamlibCat->saveSettings();
    if (!asynchronous && m_omniRigCat)  m_omniRigCat->saveSettings();
    // UI state (posizioni finestre, impostazioni panadapter)
    s.setValue("uiSpectrumHeight",  m_uiSpectrumHeight);
    s.setValue("uiPaletteIndex",    m_uiPaletteIndex);
    s.setValue("uiZoomFactor",      m_uiZoomFactor);
    s.setValue("uiWaterfallHeight", m_uiWaterfallHeight);
    s.setValue("uiDecodeWinX",      m_uiDecodeWinX);
    s.setValue("uiDecodeWinY",      m_uiDecodeWinY);
    s.setValue("uiDecodeWinWidth",  m_uiDecodeWinWidth);
    s.setValue("uiDecodeWinHeight", m_uiDecodeWinHeight);
    trace.addDetail(QStringLiteral("path=[%1] async=%2")
                        .arg(s.fileName())
                        .arg(asynchronous ? 1 : 0));

    if (!asynchronous) {
        delete stagedLegacySettings;
        delete stagedSettings;
        emit statusMessage(tr("Settings saved"));
        return;
    }

    // Both QSettings objects were constructed and mutated on this thread.
    // Detach their QObject affinity before handing them to the worker: a
    // queued QSettings update must never race the worker's sync/destruction.
    // A parentless object with no affinity may be pulled by the current
    // worker thread (including Qt versions where moveToThread returns void).
    decodium::detachSettingsForWorker(stagedSettings);
    decodium::detachSettingsForWorker(stagedLegacySettings);
    QPointer<DecodiumBridge> guard(this);
    settingsWriteThreadPool().start(QRunnable::create(
        [stagedSettings, stagedLegacySettings, guard]() {
            decodium::flushDetachedSettings(stagedSettings);
            decodium::flushDetachedSettings(stagedLegacySettings);

            if (guard) {
                QMetaObject::invokeMethod(guard.data(), [guard]() {
                    if (!guard) {
                        return;
                    }
                    const bool saveAgain = guard->m_asyncSettingsSaveAgain.exchange(
                        false, std::memory_order_acq_rel);
                    guard->m_asyncSettingsSavePending.store(
                        false, std::memory_order_release);
                    emit guard->statusMessage(QStringLiteral("Impostazioni salvate"));
                    if (saveAgain) {
                        QMetaObject::invokeMethod(guard.data(), [guard]() {
                            if (guard) {
                                guard->saveSettingsAsync();
                            }
                        }, Qt::QueuedConnection);
                    }
                }, Qt::QueuedConnection);
            }
        }));
}
