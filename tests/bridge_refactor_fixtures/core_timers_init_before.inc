    // Period timer: 250ms tick, mode determines how many ticks = 1 period
    m_periodTimer = new QTimer(this);
    m_periodTimer->setInterval(TIMER_MS);
    connect(m_periodTimer, &QTimer::timeout, this, &DecodiumBridge::onPeriodTimer);

    // UTC display timer
    m_utcTimer = new QTimer(this);
    m_utcTimer->setObjectName(QStringLiteral("decodiumUtcTimer"));
    m_utcTimer->setInterval(1000);
    connect(m_utcTimer, &QTimer::timeout, this, &DecodiumBridge::onUtcTimer);
    m_utcTimer->start();

    // Spectrum timer: emette dati FFT per la waterfall (intervallo configurabile)
    m_spectrumTimer = new QTimer(this);
    m_spectrumTimer->setObjectName(QStringLiteral("decodiumSpectrumTimer"));
    m_spectrumTimer->setTimerType(Qt::PreciseTimer);
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
        int interval = s.value("spectrumInterval", 20).toInt();
        m_spectrumTimer->setInterval(qBound(10, interval, 500));
    }
    connect(m_spectrumTimer, &QTimer::timeout, this, &DecodiumBridge::onSpectrumTimer);

    // Process CPU monitor: real Decodium process usage, normalized over logical cores.
    m_processCpuLogicalCores = qMax(1, QThread::idealThreadCount());
    m_lastProcessCpuUsec = decodiumCurrentProcessCpuUsec();
    m_processCpuSampleClock.start();
    m_processCpuSampleInitialized = true;
    m_processGpuSampleInitialized = decodiumCurrentProcessGpuTimeNs(&m_lastProcessGpuTimeNs);
    m_processGpuSampleClock.start();
    m_processCpuTimer = new QTimer(this);
    m_processCpuTimer->setObjectName(QStringLiteral("decodiumProcessUsageTimer"));
    m_processCpuTimer->setInterval(2000);
    connect(m_processCpuTimer, &QTimer::timeout, this, &DecodiumBridge::updateProcessCpuUsage);
    connect(m_processCpuTimer, &QTimer::timeout, this, &DecodiumBridge::updateProcessGpuUsage);
    m_processCpuTimer->start();

    // Main-thread stall probe: logs only visible event-loop delays, with FT slot phase.
    m_uiStallClock.start();
    m_uiStallTimer = new QTimer(this);
    m_uiStallTimer->setObjectName(QStringLiteral("decodiumUiStallTimer"));
    m_uiStallTimer->setInterval(100);
    connect(m_uiStallTimer, &QTimer::timeout, this, &DecodiumBridge::updateUiStallDiagnostics);
    m_uiStallTimer->start();

    // Async decode timer: per FT2 turbo async, decodifica ogni 100ms
    m_asyncDecodeTimer = new QTimer(this);
    m_asyncDecodeTimer->setObjectName(QStringLiteral("decodiumAsyncDecodeTimer"));
    m_asyncDecodeTimer->setInterval(100);
    connect(m_asyncDecodeTimer, &QTimer::timeout, this, &DecodiumBridge::onAsyncDecodeTimer);

    // 1.0.179 — Smooth Decode Flow scheduler: spalma il rilascio dei decode
    // FT8/FT4 dal batch (15-30 row insieme) in stream progressivo. Spread
    // target ~3s entro il periodo successivo. Default ON, auto-fallback su
    // UI stall (>500ms negli ultimi 5s).
    m_decodeReleaseTimer = new QTimer(this);
    m_decodeReleaseTimer->setTimerType(Qt::PreciseTimer);
    connect(m_decodeReleaseTimer, &QTimer::timeout,
            this, &DecodiumBridge::drainDecodeReleaseQueue);
    {
        QSettings s;
        m_smoothDecodeFlow = s.value(QStringLiteral("Decoder/SmoothDecodeFlow"),
                                     true).toBool();
    }

