void DecodiumBridge::updateProcessCpuUsage()
{
    quint64 const currentCpuUsec = decodiumCurrentProcessCpuUsec();
    qint64 const wallUsec = m_processCpuSampleClock.isValid()
        ? m_processCpuSampleClock.nsecsElapsed() / 1000
        : 0;

    if (!m_processCpuSampleInitialized || wallUsec <= 0 || currentCpuUsec < m_lastProcessCpuUsec) {
        m_lastProcessCpuUsec = currentCpuUsec;
        m_processCpuSampleClock.restart();
        m_processCpuSampleInitialized = true;
        return;
    }

    quint64 const cpuDeltaUsec = currentCpuUsec - m_lastProcessCpuUsec;
    double usage = static_cast<double>(cpuDeltaUsec)
        / (static_cast<double>(wallUsec) * qMax(1, m_processCpuLogicalCores));
    if (!std::isfinite(usage)) {
        usage = 0.0;
    }
    usage = qBound(0.0, usage, 1.0);

    m_lastProcessCpuUsec = currentCpuUsec;
    m_processCpuSampleClock.restart();

    if ((m_monitoring || m_transmitting || m_tuning) && usage >= 0.45) {
        noteCpuPressure(QStringLiteral("process CPU %1%")
                            .arg(usage * 100.0, 0, 'f', 0),
                        10000,
                        usage >= 0.60);
    } else if (!cpuPressureActive()) {
        applyFtOpenMpThreadLimit(effectiveFtThreadLimit());
    }

    if (std::abs(m_processCpuUsage - usage) < 0.005) {
        return;
    }
    m_processCpuUsage = usage;
    emit processCpuUsageChanged();
}

void DecodiumBridge::updateProcessGpuUsage()
{
    double usage = -1.0;
    QString usageSource = QStringLiteral("unavailable");

#ifdef Q_OS_WIN
    bool available = false;
    usage = decodiumCurrentProcessGpuUsage(&available);
    if (!available || !std::isfinite(usage))
        usage = -1.0;
    else
        usageSource = QStringLiteral("process");
#elif defined(Q_OS_LINUX)
    quint64 currentGpuTimeNs = 0;
    decodium::gpu_usage::LinuxDrmCycleSample cycleSample;
    bool hasEngineTime = false;
    bool hasCycleSample = false;
    QString pciDevice;
    decodiumCurrentProcessLinuxGpuSamples(&currentGpuTimeNs,
                                          &cycleSample,
                                          &hasEngineTime,
                                          &hasCycleSample,
                                          &pciDevice);
    qint64 const wallNs = m_processGpuSampleClock.isValid()
        ? m_processGpuSampleClock.nsecsElapsed()
        : 0;
    double cycleUsage = -1.0;
    bool cycleUsageAvailable = false;
    if (hasCycleSample) {
        bool const resetCycleSample =
            !m_processGpuCycleSampleInitialized
            || cycleSample.busyCycles < m_lastProcessGpuBusyCycles
            || cycleSample.totalCycles <= m_lastProcessGpuTotalCycles;
        if (!resetCycleSample) {
            quint64 const busyDelta = cycleSample.busyCycles - m_lastProcessGpuBusyCycles;
            quint64 const totalDelta = cycleSample.totalCycles - m_lastProcessGpuTotalCycles;
            if (totalDelta > 0) {
                cycleUsage = static_cast<double>(busyDelta) / static_cast<double>(totalDelta);
                cycleUsageAvailable = true;
            }
        }
        m_lastProcessGpuBusyCycles = cycleSample.busyCycles;
        m_lastProcessGpuTotalCycles = cycleSample.totalCycles;
        m_processGpuCycleSampleInitialized = true;
    } else {
        m_processGpuCycleSampleInitialized = false;
    }
    if (hasEngineTime) {
        usageSource = QStringLiteral("drm-process");
        if (!m_processGpuSampleInitialized
            || wallNs <= 0
            || currentGpuTimeNs < m_lastProcessGpuTimeNs) {
            m_lastProcessGpuTimeNs = currentGpuTimeNs;
            m_processGpuZeroSampleCount = 0;
            m_processGpuSampleClock.restart();
            m_processGpuSampleInitialized = true;
            return;
        }

        quint64 const gpuDeltaNs = currentGpuTimeNs - m_lastProcessGpuTimeNs;
        m_processGpuZeroSampleCount = decodium::gpu_usage::nextLinuxDrmUnchangedSampleCount(
            m_lastProcessGpuTimeNs,
            currentGpuTimeNs,
            m_processGpuZeroSampleCount);
        if (decodium::gpu_usage::linuxDrmCounterIsStale(m_processGpuZeroSampleCount)) {
            if (cycleUsageAvailable) {
                usage = cycleUsage;
                usageSource = QStringLiteral("drm-process-cycles");
            } else {
                LinuxDrmDeviceUsageSample deviceSample;
                if (decodiumCurrentLinuxGpuDeviceUsage(pciDevice, &deviceSample)
                    && deviceSample.immediateUsage >= 0.0) {
                    usage = deviceSample.immediateUsage;
                    usageSource = QStringLiteral("drm-device");
                } else {
                    // Do not advertise a permanently frozen counter as a
                    // genuine 0% load. QML can now fall back to measured
                    // render activity.
                    usage = -1.0;
                    usageSource = QStringLiteral("drm-process-stale");
                }
            }
        } else {
            usage = static_cast<double>(gpuDeltaNs) / static_cast<double>(wallNs);
        }
        m_lastProcessGpuTimeNs = currentGpuTimeNs;
        m_processGpuSampleClock.restart();
    } else if (hasCycleSample) {
        m_processGpuSampleInitialized = false;
        m_processGpuZeroSampleCount = 0;
        usageSource = QStringLiteral("drm-process-cycles");
        if (!cycleUsageAvailable) {
            m_processGpuSampleClock.restart();
            return;
        }
        usage = cycleUsage;
        m_processGpuSampleClock.restart();
    } else {
        m_processGpuSampleInitialized = false;
        m_processGpuCycleSampleInitialized = false;
        m_processGpuZeroSampleCount = 0;
        LinuxDrmDeviceUsageSample deviceSample;
        if (decodiumCurrentLinuxGpuDeviceUsage(pciDevice, &deviceSample)
            && deviceSample.immediateUsage >= 0.0) {
            usage = deviceSample.immediateUsage;
            usageSource = QStringLiteral("drm-device");
            m_processGpuSampleClock.restart();
        } else {
            m_processGpuSampleClock.restart();
            usage = -1.0;
        }
    }

    if (usage >= 0.0) {
        usage = std::isfinite(usage) ? qBound(0.0, usage, 1.0) : -1.0;
    }
#elif defined(Q_OS_MAC)
    quint64 currentGpuTimeNs = 0;
    bool const available = decodiumCurrentProcessGpuTimeNs(&currentGpuTimeNs);
    qint64 const wallNs = m_processGpuSampleClock.isValid()
        ? m_processGpuSampleClock.nsecsElapsed()
        : 0;

    bool gpuCounterUnavailable = !available;
    if (available && currentGpuTimeNs == 0) {
        m_processGpuZeroSampleCount = qMin(m_processGpuZeroSampleCount + 1, 1000);
        if (m_processGpuZeroSampleCount >= 4) {
            gpuCounterUnavailable = true;
        }
    } else if (available) {
        m_processGpuZeroSampleCount = 0;
    }

    if (gpuCounterUnavailable) {
        m_processGpuSampleInitialized = false;
        m_processGpuSampleClock.restart();
        usage = -1.0;
    } else if (!m_processGpuSampleInitialized
               || wallNs <= 0
               || currentGpuTimeNs < m_lastProcessGpuTimeNs) {
        m_lastProcessGpuTimeNs = currentGpuTimeNs;
        m_processGpuSampleClock.restart();
        m_processGpuSampleInitialized = true;
        return;
    } else {
        quint64 const gpuDeltaNs = currentGpuTimeNs - m_lastProcessGpuTimeNs;
        usage = static_cast<double>(gpuDeltaNs) / static_cast<double>(wallNs);
        if (!std::isfinite(usage))
            usage = -1.0;
        else
            usage = qBound(0.0, usage, 1.0);
        m_lastProcessGpuTimeNs = currentGpuTimeNs;
        m_processGpuSampleClock.restart();
        usageSource = QStringLiteral("process");
    }
#else
    usage = -1.0;
#endif

    bool const sourceChanged = m_processGpuUsageSource != usageSource;
    if (std::abs(m_processGpuUsage - usage) < 0.0005 && !sourceChanged)
        return;
    m_processGpuUsage = usage;
    m_processGpuUsageSource = usageSource;
    emit processGpuUsageChanged();
    if (sourceChanged) {
        qInfo().noquote()
            << "[GPUDBG] GPU usage counter source"
            << "source=" << usageSource
            << "scope=" << (usageSource == QStringLiteral("drm-device")
                                  ? QStringLiteral("whole_device")
                                  : QStringLiteral("process"));
    }
}

