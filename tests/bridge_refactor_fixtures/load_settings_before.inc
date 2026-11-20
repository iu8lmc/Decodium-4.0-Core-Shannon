void DecodiumBridge::loadSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    QSettings* const stagedLegacySettings =
        new QSettings(legacyIniPath(), QSettings::IniFormat);
    QSettings& legacyIni = *stagedLegacySettings;
    auto const app = QCoreApplication::instance();
    auto const appPropertyString = [app](char const *name) {
        return app ? app->property(name).toString().trimmed() : QString {};
    };
    QString const labCallsign =
        appPropertyString("decodiumLabCallsign").toUpper();
    QString const labGrid =
        appPropertyString("decodiumLabGrid").toUpper();
    QString const activeProfileName = decodium::activeSettingsProfileName();
    bool const activeProfileRequested = !activeProfileName.isEmpty();
    QString const legacyCallsign = legacyIni.value(QStringLiteral("MyCall")).toString().trimmed().toUpper();
    QString const legacyGrid = legacyIni.value(QStringLiteral("MyGrid")).toString().trimmed().toUpper();
    QString const bridgeCallsign = s.value("callsign", QString()).toString().trimmed().toUpper();
    QString const bridgeGrid = s.value("grid", QString()).toString().trimmed().toUpper();
    if (!labCallsign.isEmpty()) {
        m_callsign = labCallsign;
    } else if (activeProfileRequested) {
        m_callsign = !bridgeCallsign.isEmpty() ? bridgeCallsign : legacyCallsign;
    } else {
        m_callsign = !legacyCallsign.isEmpty() ? legacyCallsign : bridgeCallsign;
    }
    if (!labGrid.isEmpty()) {
        m_grid = labGrid;
    } else if (activeProfileRequested) {
        m_grid = !bridgeGrid.isEmpty() ? bridgeGrid
                                       : (legacyGrid.isEmpty() ? QStringLiteral("AA00") : legacyGrid);
    } else {
        m_grid = !legacyGrid.isEmpty() ? legacyGrid
                                       : (bridgeGrid.isEmpty() ? QStringLiteral("AA00") : bridgeGrid);
    }
    bridgeLog(QStringLiteral("loadSettings: profile=[%1] labCall=%2 call=%3 labGrid=%4 grid=%5 legacyIni=[%6]")
                  .arg(activeProfileName.isEmpty() ? QStringLiteral("<root>") : activeProfileName,
                       labCallsign.isEmpty() ? QStringLiteral("<none>") : labCallsign,
                       m_callsign.isEmpty() ? QStringLiteral("<empty>") : m_callsign,
                       labGrid.isEmpty() ? QStringLiteral("<none>") : labGrid,
                       m_grid,
                       legacyIniPath()));
    loadLogbookSettings();
    m_frequency = s.value("frequency", 14074000.0).toDouble();
    {
        QString const savedMode = s.value("mode", "FT8").toString().trimmed();
        QString canonicalMode = canonicalApplicationDecodeMode(savedMode);
        // Older profiles saved the unqualified Q65 label, which was the
        // legacy/default path before the explicit period/submode selector.
        // Preserve the historical default while moving it onto the native
        // Q65 contract at startup.
        if (canonicalMode == QStringLiteral("Q65")) {
            canonicalMode = QStringLiteral("Q65-60A");
            bridgeLog(QStringLiteral("loadSettings: migrated saved Q65 mode to Q65-60A"));
        }
        if (!canonicalMode.isEmpty()) {
            m_mode = canonicalMode;
        } else {
            m_mode = QStringLiteral("FT8");
            if (!savedMode.isEmpty()) {
                bridgeLog(QStringLiteral("loadSettings: ignored invalid saved mode %1, fallback FT8")
                              .arg(savedMode));
            }
        }
    }
    m_audioInputDevice  = s.value("audioInputDevice", s.value("SoundInName", "")).toString();
    m_audioOutputDevice = s.value("audioOutputDevice", s.value("SoundOutName", "")).toString();
    m_audioInputDeviceId = s.value(QStringLiteral("audioInputDeviceId")).toString().trimmed();
    m_audioOutputDeviceId = s.value(QStringLiteral("audioOutputDeviceId")).toString().trimmed();
    bridgeLog(QStringLiteral("loadSettings: audio saved input=[%1] input_id=%2 output=[%3] output_id=%4")
                  .arg(m_audioInputDevice,
                       audioDeviceIdSettingForLog(m_audioInputDeviceId),
                       m_audioOutputDevice,
                       audioDeviceIdSettingForLog(m_audioOutputDeviceId)));
    m_audioInputChannel = s.contains("audioInputChannel")
        ? s.value("audioInputChannel", 0).toInt()
        : static_cast<int>(AudioDevice::fromString(s.value("AudioInputChannel", "Mono").toString()));
    m_audioOutputChannel = s.contains("audioOutputChannel")
        ? s.value("audioOutputChannel", 0).toInt()
        : static_cast<int>(AudioDevice::fromString(s.value("AudioOutputChannel", "Mono").toString()));
    m_audioInputChannel = qBound(0, m_audioInputChannel, 3);
    m_audioOutputChannel = qBound(0, m_audioOutputChannel, 3);
    m_rxInputLevel = qBound(0.0, s.value("rxInputLevel", 50.0).toDouble(), 100.0);
    m_autoRxInputLevel = s.value("autoRxInputLevel", true).toBool();
    m_txOutputLevel = qBound(0.0, s.value("txOutputLevel", 0.0).toDouble(), 450.0);
    m_txDisabledMask = s.value("txDisabledMask", 0).toInt() & 0x3F;  // 6 bit (TX1-TX6)
    m_hideGhostDecodes = s.value("hideGhostDecodes", true).toBool();  // 1.0.145 default ON
    m_decodeShowPeriodSeparator = s.value("decodeShowPeriodSeparator", true).toBool();
    // 1.0.174 — FT2 Weak-Signal Pack: master flag (default OFF, opt-in)
    m_ft2Conservative = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2Conservative"), false).toBool();
    // 1.0.311 — cap ripetizioni 73/RR73 FT2 (default 4; era hardcoded 8). Range 1-8.
    m_ft2SignoffRetryCap = qBound(1, s.value(QStringLiteral("Ft2SignoffRetryCap"), 4).toInt(), 8);
    // 1.0.314 — opt-in TX immediato al click (stile 1.0.283), default OFF = upstream sicuro.
    m_ftxImmediateClickTx = s.value(QStringLiteral("FtxImmediateClickTx"), false).toBool();
    // 1.0.371 - opt-in: logga RR73/TX4 anche se il partner sparisce (FT2 async). Default OFF.
    m_ft2LogRr73OnPartnerLeft = s.value(QStringLiteral("Ft2LogRr73OnPartnerLeft"), false).toBool();
    // 1.0.315 — cap ripetizioni 73/RR73 anche per FT4 (default 4) e FT8 (default 3). Range 1-8.
    m_ft4SignoffRetryCap = qBound(1, s.value(QStringLiteral("Ft4SignoffRetryCap"), 4).toInt(), 8);
    m_ft8SignoffRetryCap = qBound(1, s.value(QStringLiteral("Ft8SignoffRetryCap"), 3).toInt(), 8);
    m_maxQsoPerCall = qBound(0, s.value(QStringLiteral("MaxQsoPerCall"), 0).toInt(), 3);  // 1.0.731
    loadQsoCountFromLog();  // 1.0.731: a restart does not reset today's per-call count
    // 1.0.437 - opt-in: extra signoff retries per partner debole (FTX). Default OFF.
    m_ftxWeakSignoffBoost = s.value(QStringLiteral("FtxWeakSignoffBoost"), false).toBool();
    m_ftxWeakSnrThreshold = qBound(-30, s.value(QStringLiteral("FtxWeakSnrThreshold"), -15).toInt(), -5);
    m_ftxWeakSignoffBonus = qBound(1, s.value(QStringLiteral("FtxWeakSignoffBonus"), 3).toInt(), 6);
    // 1.0.446 - opt-in guard ri-aggancio RRR post-log. Default OFF, max 1 RRR di cortesia.
    m_ft2PostLogReengageGuard = s.value(QStringLiteral("Ft2PostLogReengageGuard"), false).toBool();
    m_ft2PostLogReengageMax = qBound(0, s.value(QStringLiteral("Ft2PostLogReengageMax"), 1).toInt(), 5);
    // 1.0.446 - P0-3 opt-in: watchdog logga QSO in chiusura (progress>=4). Default OFF.
    m_txWatchdogLogOnClose = s.value(QStringLiteral("TxWatchdogLogOnClose"), false).toBool();
    // 1.0.446 - P1-5: cap Caller-retries duro anche con watchdog ON.
    // 1.0.493 - default ON su fork: il watchdog (attivo di default) rendeva il cap utente
    // inefficace in ogni installazione standard ("Caller Retries non funziona").
    m_callerRetriesAlwaysHard = decodium::profiledSettingsValue(
        QString(), QStringLiteral("CallerRetriesAlwaysHard"), true).toBool();
    // 1.0.447 - Fondamenta Fase 1 opt-in: censimento transizioni stato FT2. Default OFF.
    m_ft2TransitionCensus = s.value(QStringLiteral("Ft2TransitionCensus"), false).toBool();
    // 1.0.447 - Leva#6-A opt-in: gate smart-TX adattivi. Default OFF.
    m_ft2AdaptiveTxGates = s.value(QStringLiteral("Ft2AdaptiveTxGates"), false).toBool();
    // 1.0.321 — opt-in FT2 manual one-shot disarm. Default OFF su fork (weak-signal friendly).
    m_ft2ManualOneShotEnabled = s.value(QStringLiteral("Ft2ManualOneShotEnabled"), false).toBool();
    m_ft2NarrowAsyncDecode = s.value(QStringLiteral("Ft2NarrowAsyncDecode"), false).toBool();
    m_autoCqRxHealthGate = s.value(QStringLiteral("AutoCqRxHealthGate"), true).toBool();
    m_ft2AsyncDeadlineMs = s.value(QStringLiteral("Ft2AsyncDecodeDeadlineMs"), 2500).toInt();
    // 1.0.326 — ALC calibration target (default 20, range 5-60; FT8/data tipicamente 15-25)
    m_alcTarget = qBound(5, s.value(QStringLiteral("AlcTarget"), 20).toInt(), 60);
    // 1.0.326 B2 — MaxCallerRetries persist (default 10, range 1-99)
    m_maxCallerRetries = qBound(1, decodium::profiledSettingsValue(
        QString(), QStringLiteral("MaxCallerRetries"), 10).toInt(), 99);
    // 1.0.388 — priorità processo (default 1 = Sopra il normale). Applicata sotto, dopo
    // il profilo interattivo di startup, così la scelta utente vince.
    m_processPriority  = qBound(0, s.value(QStringLiteral("ProcessPriority"), 1).toInt(), 3);
    applyProcessPriority();
    // 1.0.317 — opt-in FT8 fast sequence (grace 400ms + late-decode accept). Default OFF.
    m_ft8FastSequence = s.value(QStringLiteral("Ft8FastSequence"), false).toBool();
    // 1.0.367 — finestra TX FT2 async conservativa. Default ON = stabilità (no frame troncati).
    m_ft2ConservativeTiming = s.value(QStringLiteral("Ft2ConservativeTiming"), true).toBool();
    // 1.0.187 — FT2 Weak-Signal Pack F v2 / G
    m_ft2PartnerMemoryEnabled = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2PartnerMemoryEnabled"), false).toBool();
    m_ft2Tx2ResendOnStall     = s.value(QStringLiteral("Ft2Tx2ResendOnStall"),     true).toBool();
    // 1.0.289 — FT2 enhancement toggles (opt-in, default OFF = comportamento 1.0.288)
    m_ft2FullDecodeInAutoCq = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2FullDecodeInAutoCq"), false).toBool();
    m_callerScoring         = decodium::profiledSettingsValue(QString(), QStringLiteral("CallerScoring"), false).toBool();
    m_ft2QuickGiveUpStrong  = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2QuickGiveUpStrong"),  false).toBool();
    m_ft2AdaptiveDecode     = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2AdaptiveDecode"),     false).toBool();
    m_ft2ApHashCache        = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2ApHashCache"),        false).toBool();
    // 1.0.355 — skip decode sync di fine-slot quando l'async ha gia' coperto lo slot
    m_ft2AsyncSkipRedundantSyncDecode = s.value(QStringLiteral("Ft2AsyncSkipRedundantSyncDecode"), false).toBool();
    // 1.0.364+ - MAM multi-stream (MSHV) FASE 3 (default OFF; cap 2..5, default 3)
    m_mamMultiStream = decodium::profiledSettingsValue(QString(), QStringLiteral("MamMultiStream"), false).toBool();
    // 1.0.711 - budget di potenza del MAM (opt-in, default OFF; nessuna interfaccia ancora).
    m_mamPowerBudget = truthyEnvironmentFlag("DECODIUM_MAM_POWER_BUDGET")
        || decodium::profiledSettingsValue(QString(), QStringLiteral("MamPowerBudget"), false).toBool();
    // 1.0.711 - ciclo elastico della Fox FT2 (opt-in, default OFF; nessuna interfaccia ancora).
    m_ft2ElasticFox = truthyEnvironmentFlag("DECODIUM_FT2_ELASTIC_FOX")
        || decodium::profiledSettingsValue(QString(), QStringLiteral("Ft2ElasticFox"), false).toBool();
    m_mamMaxStreams  = qBound(1, decodium::profiledSettingsValue(QString(), QStringLiteral("MamMaxStreams"), 3).toInt(), 10);
    m_mamCqSlots     = decodium::profiledSettingsValue(QString(), QStringLiteral("MamCqSlots"), true).toBool();
    // 1.0.299 — Deep decode anche in TX (decode-list-only), opt-in default OFF
    m_ft8DeepDecodeInTx     = s.value(QStringLiteral("Ft8DeepDecodeInTx"),     false).toBool();
    m_ft8SubpassHarvest     = decodium::profiledSettingsValue(QString(), QStringLiteral("Ft8SubpassHarvest"),     false).toBool();
    // 1.0.304 (#9) — resume-on-reply, opt-in default OFF
    m_resumeQsoOnReply      = s.value(QStringLiteral("ResumeQsoOnReply"),      false).toBool();
    // 1.0.262 — CALL feature settings persistence (fork-only iu8lmc)
    m_targetCallSign          = s.value(QStringLiteral("CallFeature/TargetCallSign"), QString()).toString();
    m_targetCallMaxRetries    = qBound(0,  s.value(QStringLiteral("CallFeature/MaxRetries"), 10).toInt(),  999);
    m_targetCallTimeoutS      = qBound(10, s.value(QStringLiteral("CallFeature/TimeoutS"),   90).toInt(),  600);
    m_targetCallPeriod        = qBound(0,  s.value(QStringLiteral("CallFeature/Period"),     2).toInt(),    2);
    m_targetCallPauseS        = qBound(0,  s.value(QStringLiteral("CallFeature/PauseS"),     0).toInt(),  300);
    m_armedWatchEnabled       = s.value(QStringLiteral("CallFeature/Armed"), false).toBool();
    m_armedReArm              = s.value(QStringLiteral("CallFeature/ReArm"), false).toBool();
    // Persist configuration, but never restore the armed state or the session
    // counter: a previous run must not make the radio transmit on startup.
    m_autoCallEnabled         = false;
    m_autoCallMaxQsos         = qBound(0, s.value(QStringLiteral("CallFeature/AutoCallMaxQsos"), 5).toInt(), 999);
    m_autoCallPriority        = qBound(0, s.value(QStringLiteral("CallFeature/AutoCallPriority"), 0).toInt(), 2);
    m_autoCallQsoCount        = 0;
    m_autoCallCurrentCandidate.clear();
    bridgeLog(QStringLiteral("[FT2WS] Pack init Conservative=%1 PartnerMemory=%2 Tx2Resend=%3")
                  .arg(m_ft2Conservative ? "ON" : "OFF")
                  .arg(m_ft2PartnerMemoryEnabled ? "ON" : "OFF")
                  .arg(m_ft2Tx2ResendOnStall ? "ON" : "OFF"));
    // 1.0.168 — auto-start web server da settings
    bool const webServerEnabled = s.value("WebServerEnabled", false).toBool();
    int const webServerPort = s.value("WebServerPort", 8080).toInt();
    if (webServerEnabled) {
        QTimer::singleShot(2000, this, [this, webServerPort]() {
            startWebServer(webServerPort);
        });
    }
    m_nfa      = s.value("nfa", 200).toInt();
    m_nfb      = s.value("nfb", 4000).toInt();
    {
        // Do not resurrect Fox/Hound from an old D3/legacy INI at startup.
        // D4 starts the legacy backend only after the user explicitly enables
        // a special operation in the D4 settings, otherwise startup monitoring
        // can block or crash on systems where the legacy DB/thread init fails.
        int const savedActivity = qBound(kSpecialOpNone,
                                         s.value(QStringLiteral("SelectedActivity"), 0).toInt(),
                                         kSpecialOpMax);
        bool const specialEnabled = s.value(QStringLiteral("SpecialOpActivity"), false).toBool();
        m_specialOperationActivity = specialEnabled ? savedActivity : kSpecialOpNone;
        m_ncontest = m_specialOperationActivity;
        m_foxMode = m_specialOperationActivity == kSpecialOpFox;
        m_houndMode = m_specialOperationActivity == kSpecialOpHound;
        m_forceLegacyTxForSpecialOp = specialOperationRequiresLegacyTx();
    }
    {
        bool const hasBridgeLegacyNDepth = s.contains("NDepth");
        bool const hasStandaloneLegacyNDepth = legacyIni.contains(QStringLiteral("NDepth"));
        int const legacyNDepthBits = hasBridgeLegacyNDepth
            ? s.value("NDepth", 3).toInt()
            : legacyIni.value(QStringLiteral("NDepth"), 3).toInt();
        bool const hasLegacyNDepth = s.contains("NDepth");
        bool const hasModernNDepth = s.contains("ndepth");
        bool const legacyDepthMigrated = s.value("decodeDepthMigratedFromLegacy", false).toBool();
        int modernDepth = qBound(1, s.value("ndepth", 3).toInt(), 4);
        int const legacyDepth = legacyDepthBitsToModernDepth(legacyNDepthBits);

        if ((hasLegacyNDepth || hasStandaloneLegacyNDepth) && (!hasModernNDepth || !legacyDepthMigrated)) {
            modernDepth = qMax(modernDepth, legacyDepth);
        }

        m_ndepth = modernDepth;
        m_avgDecodeEnabled = s.value("avgDecodeEnabled",
                                     (legacyNDepthBits & 0x10) != 0).toBool();
        m_deepSearchEnabled = s.value("deepSearchEnabled",
                                      (legacyNDepthBits & 0x20) != 0).toBool();
        m_ft8ApEnabled = s.value("FT8AP", true).toBool();
        m_coherentAvgEnabled   = s.value("coherentAvgEnabled",   false).toBool();
        m_neuralSyncEnabled    = s.value("neuralSyncEnabled",    false).toBool();
        m_turboFeedbackEnabled = s.value("turboFeedbackEnabled", false).toBool();
        m_fastLdpcEnabled = s.value("fastLdpcEnabled", true).toBool();
        superldpc_set_enabled_c(m_fastLdpcEnabled ? 1 : 0);
        m_advAutoModeEnabled   = s.value("advAutoModeEnabled",   true).toBool();
    }
    m_singleDecode = getSetting(QStringLiteral("SingleDecode"), false).toBool();
    m_dxCall   = s.value("dxCall", "").toString();
    m_dxGrid   = s.value("dxGrid", "").toString();
    m_tx1 = s.value("tx1", "").toString();
    m_tx2 = s.value("tx2", "").toString();
    m_tx3 = s.value("tx3", "").toString();
    m_tx4 = s.value("tx4", "").toString();
    m_tx5 = s.value("tx5", "").toString();
    m_tx6 = s.value("tx6", "").toString();
    m_customCqMessage = s.value("customTx6Message", QString()).toString().trimmed().toUpper();
    // Profiles written before customTx6Message used TX6 alone. Preserve an
    // existing value only when it differs from the generated CQ.
    if (!s.contains("customTx6Message") && !m_tx6.trimmed().isEmpty()) {
        QString const generatedTx6 = standardFtxCqMessage(
            m_callsign, m_grid.left(4), m_mode.trimmed().toUpper());
        if (m_tx6.trimmed().compare(generatedTx6, Qt::CaseInsensitive) != 0) {
            m_customCqMessage = m_tx6.trimmed().toUpper();
        }
    }
    m_rxFrequency = s.value("rxFrequency", 1500).toInt();
    m_txFrequency = qBound(0, s.value("txFrequency", s.value("TXFrequency", 1500)).toInt(), 3000);
    m_holdTxFreq = s.value(QStringLiteral("HoldTxFreq"),
                           s.value(QStringLiteral("holdTxFreq"), false)).toBool();
    // Sanity: se la txFrequency salvata e' implausibile (<100Hz) — es. corrotta
    // da un click cluster spot che era troppo vicino alla dial — reset al
    // default 1500. Risolve "TUNE inaudibile" 1.0.108-1.0.112.
    if (m_txFrequency < 100) {
        m_txFrequency = 1500;
    }
    m_fontScale   = s.value("fontScale", 1.08).toDouble();
    // extra features
    m_alertOnCq        = s.value("alertOnCq",       s.value("alert_CQ", false)).toBool();
    m_alertOnMyCall    = s.value("alertOnMyCall",   s.value("alert_MyCall", false)).toBool();
    m_recordRxEnabled  = s.value("recordRxEnabled",  false).toBool();
    m_recordTxEnabled  = s.value("recordTxEnabled",  false).toBool();
    m_stationName      = s.value("StationName",     QString()).toString();
    m_stationQth       = s.value("StationQTH",      QString()).toString();
    m_stationRigInfo   = s.value("StationRigInfo",  QString()).toString();
    m_stationAntenna   = s.value("StationAntenna",  QString()).toString();
    m_stationPowerWatts= qBound(0, s.value("StationPowerW", 100).toInt(), 9999);
    m_wsprPowerDbm     = normalizeWsprPowerDbm(s.value("WsprPowerDbm",
                                                       s.value("dBm", 37)).toInt());
    bool const storedMonitorOffAtStartup = s.value("MonitorOFF", false).toBool();
    if (storedMonitorOffAtStartup) {
        s.setValue(QStringLiteral("MonitorOFF"), false);
        syncSettingToLegacyIni(QStringLiteral("MonitorOFF"), false);
    }
    m_autoStartMonitorOnStartup = true;
    bridgeLog(QStringLiteral("loadSettings: MonitorOFF=%1 forced to 0 autoStartMonitorOnStartup=1")
                  .arg(storedMonitorOffAtStartup ? 1 : 0));
    m_autoSeq          = s.value("autoSeq", s.value("AutoSeq", true)).toBool();
    m_waitPounceActive = s.value("WaitPounceActive", false).toBool();
    if (m_waitPounceActive && !m_autoSeq) {
        m_autoSeq = true;
    }
    m_startFromTx2     = s.value("startFromTx2",     false).toBool();
    m_vhfUhfFeatures   = s.value("vhfUhfFeatures",   s.value("VHFUHF", false)).toBool();
    m_directLogQso     = s.value("directLogQso",      false).toBool();
    m_autoSpotEnabled  = s.value("AutoSpotEnabled",   false).toBool();
    m_confirm73        = s.value("confirm73",          true).toBool();
    m_contestExchange  = s.value("contestExchange",   "").toString();
    m_contestNumber    = s.value("contestNumber",      1).toInt();
    m_swlMode          = s.value("swlMode",           false).toBool();
    m_splitMode        = s.value("splitMode",         false).toBool();
    QVariant legacyTxWatchdog = readSettingFromLegacyIni(QStringLiteral("TxWatchdog"));
    if (!legacyTxWatchdog.isValid()) {
        legacyTxWatchdog = readSettingFromLegacyIni(QStringLiteral("TXWatchdog"));
    }
    bool legacyTxWatchdogOk = false;
    int const legacyTxWatchdogMinutes = legacyTxWatchdog.toInt(&legacyTxWatchdogOk);
    bool const hasSavedTxWatchdogMode = s.contains(QStringLiteral("txWatchdogMode"));
    bool const hasSavedTxWatchdogTime = s.contains(QStringLiteral("txWatchdogTime"));

    m_txWatchdogTime = qBound(0,
                              hasSavedTxWatchdogTime
                                  ? s.value(QStringLiteral("txWatchdogTime")).toInt()
                                  : (legacyTxWatchdog.isValid() && legacyTxWatchdogOk
                                         ? legacyTxWatchdogMinutes
                                         : kDefaultTxWatchdogMinutes),
                              999);
    m_txWatchdogMode = qBound(0,
                              hasSavedTxWatchdogMode
                                  ? s.value(QStringLiteral("txWatchdogMode")).toInt()
                                  : (m_txWatchdogTime > 0 ? 1 : 0),
                              2);
    m_txWatchdogCount  = qBound(1,
                                s.value(QStringLiteral("txWatchdogCount"),
                                        kDefaultTxWatchdogCount).toInt(),
                                50);
    m_filterCqOnly     = s.value("filterCqOnly",      false).toBool();
    m_cqFilterLevel    = qBound(0, s.value("cqFilterLevel", 0).toInt(), 3);  // 1.0.383
    m_activeReadyProfile = s.value(QStringLiteral("ActiveReadyProfile"), QString()).toString();  // 1.0.384
    m_filterMyCallOnly = s.value("filterMyCallOnly",  false).toBool();
    m_filtersBypassed  = s.value(QStringLiteral("filtersBypassed"),
                                  s.value(QStringLiteral("FiltersBypassed"), false)).toBool();
    m_contestType      = s.value("contestType",        0).toInt();
    m_zapEnabled       = s.value("zapEnabled",        false).toBool();
    m_asyncDecodeEnabled=s.value("asyncDecodeEnabled",false).toBool();
    m_pskReporterEnabled=s.value("pskReporterEnabled",
                                  s.value("PSKReporter", false)).toBool();
    m_pskReporterTimeSpanMinutes = qBound(5,
                                           s.value("PskReporterTimeSpanMinutes", 60).toInt(),
                                           60);
    m_pskReporterTimeSpanMinutes = qBound(5,
                                           ((m_pskReporterTimeSpanMinutes + 2) / 5) * 5,
                                           60);
    // The Live Map uses a separate and much shorter PSK Reporter window than
    // callsign search / heard-by results. Migrate an existing map decay value
    // once so established profiles retain their chosen lifetime.
    int const legacyPskMapWindow = s.value(QStringLiteral("Decay/psk"), 15).toInt();
    m_pskMapSpotWindowMinutes = qBound(5,
        s.value(QStringLiteral("PskMapSpotWindowMinutes"), legacyPskMapWindow).toInt(), 60);
    m_pskMapSpotWindowMinutes = qBound(5,
        ((m_pskMapSpotWindowMinutes + 2) / 5) * 5, 60);
    syncPskMapSpotWindowToMap();
    m_worldMapDisplayed = s.value(QStringLiteral("WorldMapDisplayed"), true).toBool();
    m_directVisualAudioCaptureUnsafe = s.value(QStringLiteral("DirectVisualAudioCaptureUnsafe"), false).toBool();
    m_ftThreadsAuto = s.value(QStringLiteral("ftThreadsAuto"),
                              !s.contains(QStringLiteral("ftThreads"))).toBool();
    if (m_ftThreadsAuto) {
        int const cores = qMax(1, QThread::idealThreadCount());
        int const uiReserve = ftAutoUiReserveForCores(cores);
        m_ftThreads = autoFtThreadCountForCores(cores);
        s.setValue(QStringLiteral("ftThreads"), m_ftThreads);
        s.setValue(QStringLiteral("ftThreadsAuto"), true);
        s.setValue(QStringLiteral("ftThreadsAutoDetected"), true);
        bridgeLog(QStringLiteral("FT decoder threads AUTO: cores=%1 ui_reserve=%2 ft_threads=%3")
                      .arg(cores)
                      .arg(uiReserve)
                      .arg(m_ftThreads));
    } else {
        m_ftThreads = std::clamp(s.value("ftThreads", 3).toInt(), 1, kMaxFtDecodeThreads);
    }
    m_lowCpuModeEnabled = s.value(QStringLiteral("LowCpuMode"),
                                  s.value(QStringLiteral("lowCpuModeEnabled"), false)).toBool();
    // 1.0.497 Modalità PC lento: sempre da ROOT (per-macchina, come lo startup probe in main_qml)
    {
        QSettings rootS(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        m_lowEndMode = rootS.value(QStringLiteral("LowEndMode"), false).toBool();
    }
    // Default 'hamlib' per nuove installazioni: copre 400+ radio (incluso ICOM
    // CI-V completo) con molti meno problemi di compatibilita' rispetto al path
    // nativo che supporta solo comandi ASCII Kenwood/Yaesu. Gli utenti esistenti
    // mantengono il proprio backend salvato.
    m_catBackend        =s.value("catBackend",        "hamlib").toString().trimmed().toLower();
    if (m_catBackend != QStringLiteral("hamlib")
        && m_catBackend != QStringLiteral("tci")
        && m_catBackend != QStringLiteral("cat4om")
        && m_catBackend != QStringLiteral("omnirig")) {
        m_catBackend = QStringLiteral("hamlib");
    }
    m_lastSuccessfulCatConnected = s.value("lastSuccessfulCatConnected", false).toBool();
    m_lastSuccessfulCatBackend = s.value("lastSuccessfulCatBackend").toString();
    // TX QSO options
    m_reportReceived    =s.value("reportReceived",    "-10").toString();
    m_sendRR73          =s.value("sendRR73",           true).toBool();
    m_txPeriod          =s.value("txPeriod",            0).toInt() != 0 ? 1 : 0;
    m_alt12Enabled      =s.value("alt12Enabled",     false).toBool();
    m_quickQsoEnabled   =s.value("quickQsoEnabled",  false).toBool();
    m_autoCqBurstCalls  =qBound(0, s.value("AutoCqBurstCalls", 0).toInt(), 999);
    m_autoCqMaxCycles = qBound(0, s.value("AutoCqMaxCycles", 0).toInt(), 999);
    m_autoCqPauseSec = qBound(0, s.value("AutoCqPauseSec", 0).toInt(), 300);
    m_autoCqListenCycles=qBound(0, s.value("AutoCqListenCycles", 0).toInt(), 999);
    // B7 — Colors
    m_colorCQ       = s.value("colorCQ",        "#33FF33").toString();
    m_colorMyCall   = s.value("colorMyCall",     "#FF5555").toString();
    m_colorDXEntity = s.value("colorDXEntity",   "#FFAA33").toString();
    m_color73       = s.value("color73",         "#5599FF").toString();
    m_colorB4       = s.value("colorB4",         "#888888").toString();
    m_colorDecodeText = s.value("colorDecodeText", "#AFC4D8").toString();
    m_b4Strikethrough= s.value("b4Strikethrough",  true).toBool();
    // WSJT-X-style highlighting palette
    m_colorTxMessage        = s.value("colorTxMessage",        m_colorTxMessage       ).toString();
    m_colorNewDxcc          = s.value("colorNewDxcc",          m_colorNewDxcc         ).toString();
    m_colorNewDxccBand      = s.value("colorNewDxccBand",      m_colorNewDxccBand     ).toString();
    m_colorNewContinent     = s.value("colorNewContinent",     m_colorNewContinent    ).toString();
    m_colorNewContinentBand = s.value("colorNewContinentBand", m_colorNewContinentBand).toString();
    m_colorNewCqZone        = s.value("colorNewCqZone",        m_colorNewCqZone       ).toString();
    m_colorNewCqZoneBand    = s.value("colorNewCqZoneBand",    m_colorNewCqZoneBand   ).toString();
    m_colorNewItuZone       = s.value("colorNewItuZone",       m_colorNewItuZone      ).toString();
    m_colorNewItuZoneBand   = s.value("colorNewItuZoneBand",   m_colorNewItuZoneBand  ).toString();
    m_colorNewGrid          = s.value("colorNewGrid",          m_colorNewGrid         ).toString();
    m_colorNewGridBand      = s.value("colorNewGridBand",      m_colorNewGridBand     ).toString();
    m_colorNewCall          = s.value("colorNewCall",          m_colorNewCall         ).toString();
    m_colorNewCallBand      = s.value("colorNewCallBand",      m_colorNewCallBand     ).toString();
    m_colorLotwUser         = s.value("colorLotwUser",         m_colorLotwUser        ).toString();
    m_decodeColorEnabled.clear();
    m_decodeColorBold.clear();
    m_decodeColorBg.clear();
    m_decodeColorBgEnabled.clear();
    for (const QString& prop : decodeColorPropertyNames()) {
        m_decodeColorEnabled.insert(prop, s.value(decodeColorEnabledSettingKey(prop), true).toBool());
        m_decodeColorBold.insert(prop, s.value(decodeColorBoldSettingKey(prop), decodeColorBoldDefault(prop)).toBool());
        m_decodeColorBg.insert(prop, s.value(QStringLiteral("bg_%1").arg(prop), QString()).toString());
        m_decodeColorBgEnabled.insert(prop, s.value(QStringLiteral("bgEnabled_%1").arg(prop), false).toBool());
    }
    // B8 — Alerts
    m_alertSoundsEnabled = s.value("alertSoundsEnabled", s.value("alert_Enabled", false)).toBool();
    // Cloudlog
    auto migrateProfileAlias = [&s](QString const& canonicalKey,
                                    QString const& legacyKey,
                                    QVariant const& defaultValue) {
        QVariant value = s.value(canonicalKey);
        if (!value.isValid()) {
            value = s.value(legacyKey, defaultValue);
            if (value.isValid()) {
                s.setValue(canonicalKey, value);
            }
        }
        s.remove(legacyKey);
        return value.isValid() ? value : defaultValue;
    };
    m_cloudlogEnabled =
        migrateProfileAlias(QStringLiteral("CloudLog"),
                            QStringLiteral("cloudlogEnabled"),
                            false).toBool();
    m_cloudlogUrl =
        migrateProfileAlias(QStringLiteral("CloudLogApiUrl"),
                            QStringLiteral("cloudlogUrl"),
                            QString()).toString();
    QVariant const cloudlogKeyValue =
        migrateProfileAlias(QStringLiteral("CloudLogApiKey"),
                            QStringLiteral("cloudlogApiKey"),
                            QString());
    m_cloudlogApiKey =
        secure_settings::load_or_import(
            &s,
            secure_settings::service(m_callsign.trimmed().toUpper()),
            QStringLiteral("CloudLogApiKey"),
            cloudlogKeyValue.toString()).trimmed();
    s.sync();
    m_decoLogLinkEnabled = s.value(QStringLiteral("DecoLogLink"), true).toBool();
    m_decoLogLinkPort = qBound(1, s.value(QStringLiteral("DecoLogLinkPort"), 52237).toInt(), 65535);
    configureDecoLogLink();
    if (m_cloudlog) {
        m_cloudlog->setEnabled(m_cloudlogEnabled);
        m_cloudlog->setApiUrl(m_cloudlogUrl);
        m_cloudlog->setApiKey(m_cloudlogApiKey);
        m_cloudlog->setStationId(qBound(0, getSetting(QStringLiteral("CloudLogStationID"), 1).toInt(), 999));
    }
    // QRZ Logbook
    m_qrzLogbookEnabled = s.value("qrzLogbookEnabled", false).toBool();
    m_qrzLogbookApiKey = secure_settings::load_or_import(&s,
                                                         secure_settings::service(m_callsign.trimmed().toUpper()),
                                                         QStringLiteral("qrzLogbookApiKey"),
                                                         s.value("qrzLogbookApiKey", QString()).toString()).trimmed();
    m_qrzLogbookReplaceDuplicates = s.value("qrzLogbookReplaceDuplicates", false).toBool();
    if (m_qrzLogbook) {
        m_qrzLogbook->setEnabled(m_qrzLogbookEnabled);
        m_qrzLogbook->setApiKey(m_qrzLogbookApiKey);
        m_qrzLogbook->setReplaceDuplicates(m_qrzLogbookReplaceDuplicates);
    }
    if (m_callsignIntelligence) {
        m_callsignIntelligence->setQrzApiKey(m_qrzLogbookApiKey);
        m_callsignIntelligence->setLotwPassword(getSetting(QStringLiteral("Lotw_pwd"), QString()).toString());
    }
    // LotW
    m_lotwEnabled = s.value("lotwEnabled", false).toBool();
    m_showUsState = s.value("showUsState", true).toBool();
    // WSPR upload
    m_wsprUploadEnabled = s.value("wsprUploadEnabled", false).toBool();
    if (m_wsprUploader) m_wsprUploader->setEnabled(m_wsprUploadEnabled);
    // DX Cluster
    if (m_dxCluster) {
        m_dxCluster->loadSettings();
        if (!m_callsign.isEmpty()) m_dxCluster->setCallsign(m_callsign);
    }
    // UI state
    m_uiSpectrumHeight  = s.value("uiSpectrumHeight",  150).toInt();
    m_uiPaletteIndex    = qBound(0, s.value("uiPaletteIndex", 3).toInt(), 10);
    m_uiZoomFactor      = s.value("uiZoomFactor",      1.0).toDouble();
    m_uiWaterfallHeight = s.value("uiWaterfallHeight", 350).toInt();
    m_uiDecodeWinX      = s.value("uiDecodeWinX",      0).toInt();
    m_uiDecodeWinY      = s.value("uiDecodeWinY",      0).toInt();
    m_uiDecodeWinWidth  = s.value("uiDecodeWinWidth",  1100).toInt();
    m_uiDecodeWinHeight = s.value("uiDecodeWinHeight", 600).toInt();
    // PSK Reporter — aggiorna stazione locale con callsign/grid caricati
    if (m_pskReporter) {
        m_pskReporter->setUseTcpIp(getSetting(QStringLiteral("PSKReporterTCPIP"), false).toBool());
        refreshPskReporterLocalStation();
        if (m_mainQmlReady && m_startupServicesStarted) {
            m_pskReporter->setEnabled(m_pskReporterEnabled);
        } else {
            m_pskReporter->setEnabled(false);
            bridgeLog(QStringLiteral("PSK Reporter network startup deferred until Main.qml ready"));
        }
    }
    applyNtpSettings();
    // Garantisce che TX6 (CQ) sia sempre valorizzato dopo il caricamento settings
    regenerateTxMessages();

    // DecoPort: se questa postazione e' quella che pubblica la radio, si accende
    // da sola. Differito, perche' qui il CAT non e' ancora salito e il gateway
    // annuncerebbe una radio che risulta senza controllo per i primi secondi.
    if (decoPortAutoStart() && hasDecoPortPassword()) {
        int const port = decoPortConfiguredPort();
        QTimer::singleShot(4000, this, [this, port]() {
            if (m_shuttingDown)
                return;
            startDecoPortGateway(port);
        });
    }
}
