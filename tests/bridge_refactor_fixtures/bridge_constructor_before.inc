DecodiumBridge::DecodiumBridge(QObject* parent)
    : QObject(parent)
{
    // The native SSTV facade is always present in SSTV-enabled builds, while
    // its worker and DSP pipeline remain stopped until the workspace starts
    // reception. A stable lifetime is required by the direct bounded PCM tap.
    initialiseSstvRuntime();

    // Istanzia i tre model nativi prima di tutto, così QML non vede mai un
    // puntatore nullo durante il caricamento asincrono dei pannelli.
    decodiumBridgeApplyFtOpenMpSchedulerProfile();
#ifdef Q_OS_WIN
    decodiumBridgeApplyWindowsInteractiveSchedulingProfile();
#endif

    initialiseDecodeModels();

    initialisePanadapterGpuPolicy();

    // FT2 async smart TX scheduler: ascolta il livello audio per mantenere
    // un counter di "quiet runs" (campioni consecutivi sotto soglia RMS).
    connect(this, &DecodiumBridge::audioLevelChanged,
            this, &DecodiumBridge::onAudioLevelForFt2Gate);

    initialiseDiagnosticConnections();

    m_mediaDevices = new QMediaDevices(this);
    initialiseRtlSdr();
    // Telemetria stazione+meteo: refresh periodico in background (non ad
    // ogni QSO, per non martellare l'API) — usato solo se WeatherApiEnable
    // e' attivo (il controllo e' dentro fetchWeatherForGrid()).
    m_weatherRefreshTimer = new QTimer(this);
    connect(m_weatherRefreshTimer, &QTimer::timeout, this, &DecodiumBridge::fetchWeatherForGrid);
    m_weatherRefreshTimer->start(20 * 60 * 1000);
    QTimer::singleShot(5000, this, &DecodiumBridge::fetchWeatherForGrid);

    initialiseAudioDeviceNotifications();

    m_themeManager    = new DecodiumThemeManager(this);
    // The effective quality follows the flat "Light" theme: tell the QML bindings when it changes.
    connect(m_themeManager, &DecodiumThemeManager::currentThemeChanged,
            this, &DecodiumBridge::uiQualityChanged);
    m_propagationManager = new DecodiumPropagationManager(this);
    initialiseSatelliteTracking();
    initialiseMapIntelligence();
    m_diagnostics     = new DecodiumDiagnostics(this);
    initialiseWavManager();
    m_macroManager    = new MacroManager(this);
    m_bandManager     = new BandManager(this);
    m_bandManager->setFrequencyResolver([this](const QString& bandLambda, const QString& mode) {
        return workingFrequencyForBandMode(bandLambda, mode);
    });
    m_cat4OmCat       = new DecodiumCat4OmManager(this);
    m_omniRigCat      = new DecodiumOmniRigManager(this);
    m_hamlibCat       = new DecodiumTransceiverManager(this);
    m_catShare        = new DecodiumCatShare(m_hamlibCat, this);
    // Condivisione nel protocollo della radio (DecoPort): prima di aprire la
    // radio il CAT chiede qui quale porta dare a Hamlib, e quando chiude
    // restituisce la seriale. A interruttore spento la risposta e' "la COM di
    // sempre", e niente cambia.
    m_nativeCatShare  = new DecodiumNativeCatShare(this);
    m_hamlibCat->setCatLineRedirect(
        [this](const QString& port, const QString& rigName, const DecoPortLineSettings& line,
               bool pttOnCatLines, QString* why) {
            return m_nativeCatShare
                ? m_nativeCatShare->redirect(port, rigName, line, pttOnCatLines, why)
                : QString();
        },
        [this]() {
            if (m_nativeCatShare)
                m_nativeCatShare->release();
        });
    connect(m_nativeCatShare, &DecodiumNativeCatShare::pairRemembered, this,
            [this](const QString& pair) {
                setSetting(QStringLiteral("NativeCatSharePair"), pair);
            });
    m_amplifier       = new DecodiumAmplifier(this);
    noteActiveCatProfileAtStartup();
    decodiumBridgeMigrateRemovedNativeCatBackend();
    m_cat4OmCat->loadSettings();
    m_omniRigCat->loadSettings();
    m_hamlibCat->loadSettings();
    // CAT condivisa: si apre solo se l'utente l'ha chiesto. A interruttore
    // spento non viene aperta alcuna porta e il comportamento resta quello
    // di sempre.
    // Lettura con ricaduta sulla radice: getSetting resta confinato al profilo
    // attivo e una chiave scritta a livello globale risulterebbe invisibile,
    // con l'interruttore acceso e nessun effetto.
    if (m_catShare && decodium::profiledSettingsValue({}, QStringLiteral("CatShareEnabled"), false).toBool()) {
        m_catShare->configure(
            true,
            decodium::profiledSettingsValue({}, QStringLiteral("CatSharePort"), 4533).toInt(),
            decodium::profiledSettingsValue({}, QStringLiteral("CatShareAllowControl"), false).toBool(),
            decodium::profiledSettingsValue({}, QStringLiteral("CatShareAllowPtt"), false).toBool());
    }
    // Condivisione nel protocollo della radio: stessa lettura con ricaduta
    // sulla radice. La coppia virtuale ricordata si riapre senza chiedere
    // niente a nessuno; crearne una nuova parte solo dal pulsante.
    // FT2: la ricerca della deriva (satelliti, QO-100) e' accesa di default; una
    // variabile d'ambiente impostata a mano ha la precedenza.
    if (qEnvironmentVariableIsEmpty("DECODIUM_FT2_DRIFT_SEARCH")
        && ft2DriftSearchEnabled()) {
        qputenv("DECODIUM_FT2_DRIFT_SEARCH", "1");
    }
    if (m_nativeCatShare) {
        m_nativeCatShare->configure(
            decodium::profiledSettingsValue({}, QStringLiteral("NativeCatShareEnabled"), false).toBool(),
            decodium::profiledSettingsValue({}, QStringLiteral("NativeCatShareAllowPtt"), false).toBool(),
            decodium::profiledSettingsValue({}, QStringLiteral("NativeCatShareRawPort"), 0).toInt(),
            decodium::profiledSettingsValue({}, QStringLiteral("NativeCatSharePair"), QString()).toString());
    }
    // Amplificatore: si apre solo se richiesto. Lettura con ricaduta sulla
    // radice, per la stessa ragione della CAT condivisa.
    QString const ampPort = decodium::profiledSettingsValue({}, QStringLiteral("AmpPort"), QString()).toString();
    if (m_amplifier) {
        m_amplifier->setManufacturer(
            decodium::profiledSettingsValue({}, QStringLiteral("AmpManufacturer"), QStringLiteral("SPE"))
                .toString());
    }
    if (m_amplifier && decodium::profiledSettingsValue({}, QStringLiteral("AmpEnabled"), false).toBool()
        && !decodiumBridgeAmplifierPortClashes(m_hamlibCat, ampPort)) {
        m_amplifier->configure(
            true,
            ampPort,
            decodium::profiledSettingsValue({}, QStringLiteral("AmpBaud"), 9600).toInt(),
            decodium::profiledSettingsValue({}, QStringLiteral("AmpPassive"), true).toBool(),
            decodium::profiledSettingsValue({}, QStringLiteral("AmpPollMs"), 500).toInt());
    }
    m_dxCluster       = new DecodiumDxCluster(this);
    // Ritrasmettitore degli spot in rete locale: si apre solo se l'utente
    // l'ha chiesto, come la CAT condivisa. Lettura con ricaduta sulla radice
    // per la stessa ragione — una chiave scritta a livello globale risulta
    // invisibile a getSetting, e l'interruttore sembrerebbe acceso senza fare
    // nulla.
    m_spotShare       = new DecodiumSpotShare(m_dxCluster, this);
    if (decodium::profiledSettingsValue({}, QStringLiteral("SpotShareEnabled"), false).toBool()) {
        m_spotShare->configure(
            true,
            decodium::profiledSettingsValue({}, QStringLiteral("SpotSharePort"), 4534).toInt());
    }
    if (m_mapIntelligenceService) {
        m_dxCluster->setOfflineMode(m_mapIntelligenceService->offlineMode());
    }
    QGuiApplication::setFont(fontSettingFont(QStringLiteral("Font"), QString(), 0));
    initialiseDxClusterNotifications();
    initialiseLegacyCqFilterConnections();
    initialiseReadyProfileConnections();
    initialiseLegacyMyCallFilterConnection();
    initialiseDecodeClusterConnections();
    m_activeStations  = new ActiveStationsModel(this);
    m_alertManager    = new DecodiumAlertManager(this);
    initialiseTimeSync();

    bridgeLog(QStringLiteral("DX Cluster auto-connect: waiting for Main.qml ready"));

    initialiseMacLegacyStartup();

    bridgeLog(QStringLiteral("UDP WSJT-X startup: waiting for Main.qml ready"));

    initialisePskAndCloudlog();

    // QRZ Logbook
    // DecoLink: DecoLog racconta il suo log. I QSO continuano ad andare a DecoLog
    // via UDP come a ogni logger; da qui arrivano la conferma di scrittura, chi e'
    // gia' stato lavorato fuori dal file ADIF di Decodium e l'FT2 Award.
    initialiseDecoLogLink();

    initialiseQrzAndWspr();

    initialiseRemoteWebConsole();

    initialiseCallsignIntelligence();
    initialiseUsStateAndWsprNotifications();

    // Start drift reference clock
    m_driftClock.start();

    initialiseCatConnections();

    initialiseCatTelemetry();

    // Decoder workers are created lazily after the saved mode is known.  This
    // avoids starting every DSP stack and allocating all of their FFT state
    // before the first frame; a mode switch creates only the newly selected
    // worker and keeps already-used workers available for later switches.
    bridgeLog(QStringLiteral("decoder workers: lazy selected-mode initialization enabled"));

    initialiseCoreTimers();

    initialiseUiStartupSettings();

    // 1.0.192 — Log Frequency Calibration al boot: l'utente vede subito se
    // la correzione e' attiva o no. Se entrambi 0, formula no-op (fast path).
    {
        double const slope = frequencyCalibrationSlopePpm();
        double const intercept = frequencyCalibrationInterceptHz();
        bool const active = !qFuzzyIsNull(slope) || !qFuzzyIsNull(intercept);
        bridgeLog(QStringLiteral("[CAT-CAL] Init slope=%1ppm intercept=%2Hz active=%3 "
                                 "(esempio su 14.074 MHz: correction=%4Hz)")
                      .arg(slope, 0, 'f', 5)
                      .arg(intercept, 0, 'f', 2)
                      .arg(active ? "YES" : "no")
                      .arg(14074000.0 * slope * 1e-6 + intercept, 0, 'f', 2));
    }

    m_legacyStateTimer = new QTimer(this);
    m_legacyStateTimer->setInterval(250);
    connect(m_legacyStateTimer, &QTimer::timeout, this, &DecodiumBridge::syncLegacyBackendState);

    initialiseTxAudioOutput();

    loadSettings();
    QTimer::singleShot(5000, this, [this]() {
        if (!m_shuttingDown) {
            m_startupTxGuard = false;
            bridgeLog(QStringLiteral("TX guard: startup RX-only window released"));
        }
    });
    initialiseSstvTx();
    if (m_mapIntelligenceService) {
        m_mapIntelligenceService->setRosterStationCall(m_callsign);
        m_mapIntelligenceService->reloadFromAdif(effectiveAdifLogPath());
    }
    refreshMapMoonOverlay();
    applyLowCpuRuntimeProfile(QStringLiteral("startup"));
    bridgeLog(QStringLiteral("Worked-before ADIF preload deferred until DXCC startup task"));
    resetStartupTransientQsoState();
    purgePersistentTransientQsoState();
    m_startupModeAutoPending = true;
    m_startupModeAutoUntilMs = QDateTime::currentMSecsSinceEpoch() + 10000;
    m_startupModeAutoAuthoritativeApplied = false;
    maybeApplyStartupModeFromRigFrequency(m_frequency);
    // 1.0.374: allinea il modo del BandManager al modo applicativo finale dell'avvio.
    // Avviando GIA' in FT2 (modo salvato == startup) l'early-return di
    // maybeApplyStartupModeFromRigFrequency salta setMode()->setCurrentMode(), cosi
    // m_currentMode resterebbe FT8 (default) e il cambio banda dalla band-bar darebbe
    // la freq FT8 invece di quella FT2. Ristabilisce l'invariante m_currentMode==m_mode.
    if (m_bandManager) {
        m_bandManager->setCurrentMode(m_mode);
    }
    m_legacyStartupModeGuard = m_mode.trimmed();
    m_legacyStartupModeGuardUntilMs = QDateTime::currentMSecsSinceEpoch() + 10000;
    bridgeLog(QStringLiteral("Audio device enumeration deferred until Main.qml ready"));

    bridgeLog("startup services: waiting for Main.qml ready signal");

    syncLegacyBackendState();
    if ((usingLegacyBackendForTx() || decodiumBridgeLegacyRigControlFallback(m_legacyBackend, m_catBackend)) && m_legacyStateTimer) {
        m_legacyStateTimer->start();
    }

    // 1.0.238 (Phase 5.2 perf roadmap): avvia il worker di persistenza decode
    // history su thread separato. Va fatto DOPO il setup di tutti i member
    // perche' il worker usa enrichDecodeEntry-style entry maps che dipendono
    // dal contesto del bridge gia' inizializzato.
    startPersistenceWorker();
    QElapsedTimer knownCqSeedTimer;
    knownCqSeedTimer.start();
    seedFt8KnownCqCacheFromAllTxt();
    qInfo() << "[STARTUP-TIMING] ft8_known_cq_seed_ms=" << knownCqSeedTimer.elapsed();
}

