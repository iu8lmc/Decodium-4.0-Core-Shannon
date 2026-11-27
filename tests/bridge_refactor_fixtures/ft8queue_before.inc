void DecodiumBridge::queueFt8DecodeRequest(const QVector<short>& audioSnapshot, quint64 serial,
                                           int nutc, qint64 slotIndexForUtc, int decodeDepth,
                                           int decodeQsoProgress, int cqHint, int nzhsym,
                                           bool ft8ApEnabled, bool suppressUiRows,
                                           bool listOnlyRows, int maxDecodeMsOverride,
                                           bool subpassRequested, int threadCountOverride)
{
    if (!m_ft8Worker) {
        return;
    }

    decodium::ft8::DecodeRequest req;
    req.serial = serial;
    req.audio = audioSnapshot;
    req.nutc = nutc;
    req.nqsoprogress = decodeQsoProgress;
    req.nfqso = qBound(m_nfa, m_rxFrequency, m_nfb);
    req.nftx = qBound(m_nfa, m_txFrequency, m_nfb);
    req.nfa = m_nfa;
    req.nfb = m_nfb;
    req.nzhsym = qBound(41, nzhsym, 50);
    bool const txAudioActive = m_transmitting || m_tuning;
    req.ndepth = txAudioActive ? qMin(decodeDepth, 2) : decodeDepth;
    int const normalThreadCount = effectiveFtThreadLimit();
    req.threadCount = threadCountOverride > 0
        ? qBound(1, qMin(threadCountOverride, normalThreadCount), kMaxFtDecodeThreads)
        : normalThreadCount;
    req.ncontest = m_ncontest;
    req.superFoxEnabled = getSetting(QStringLiteral("SuperFox"), false).toBool()
                          && (m_ncontest == 7);
    req.superFoxTolHz = getSetting(QStringLiteral("Ftol_SF"), 50).toInt();
    req.emedelay = 0.0f;
    req.nagain = 0;
    req.lft8apon = (ft8ApEnabled && !txAudioActive) ? 1 : 0;
    req.apMyCallEnabled = decodium::ft8::allowMyCallAp(
        false, m_transmitting && !m_tuning && m_mode == QStringLiteral("FT8"),
        m_lastFt8MessageTxMs, QDateTime::currentMSecsSinceEpoch());
    qint64 const monitoringAgeMs =
        (m_monitoring && m_monitoringSince.isValid())
            ? m_monitoringSince.msecsTo(QDateTime::currentDateTimeUtc())
            : std::numeric_limits<qint64>::max();
    bool const ft8StartupWindow =
        !txAudioActive
        && m_mode == QStringLiteral("FT8")
        && monitoringAgeMs >= 0
        && monitoringAgeMs < 45000;
    bool const startupLimitedHardware =
        ft8StartupWindow && effectiveFtThreadLimitForDecode() <= 2;
    bool const ft8StartupPressure =
        ft8StartupWindow
        && (m_lowCpuModeEnabled || cpuPressureActive() || startupLimitedHardware);
    bool const adaptiveFt8Pressure =
        !txAudioActive && (m_lowCpuModeEnabled || cpuPressureActive() || ft8StartupPressure);
    req.cpuPressureLimited = adaptiveFt8Pressure;
    if (adaptiveFt8Pressure) {
        req.ndepth = qMin(req.ndepth, 2);
        req.lft8apon = 0;
        bool const severePressure = cpuPressureSevereActive();
        bool const startupOrSeverePressure = ft8StartupPressure || severePressure;
        int const rxWindowHz = startupOrSeverePressure ? 700 : 1200;
        int rangeLo = qMax(m_nfa, req.nfqso - rxWindowHz);
        int rangeHi = qMin(m_nfb, req.nfqso + rxWindowHz);
        if (!startupOrSeverePressure && req.nftx >= m_nfa && req.nftx <= m_nfb) {
            int const txGuardHz = 350;
            rangeLo = qMin(rangeLo, qMax(m_nfa, req.nftx - txGuardHz));
            rangeHi = qMax(rangeHi, qMin(m_nfb, req.nftx + txGuardHz));
        }
        if (rangeHi - rangeLo >= 200 && rangeHi - rangeLo < req.nfb - req.nfa) {
            req.nfa = rangeLo;
            req.nfb = rangeHi;
        }
        if (startupOrSeverePressure) {
            req.ncandthin = qMin(req.ncandthin, 20);
        }
    }
    req.lmultift8 = 1;
    req.lapcqonly = cqHint;
    if (m_frequency < 30000000.0) {
        req.napwid = 5;
    } else if (m_frequency < 100000000.0) {
        req.napwid = 15;
    } else {
        req.napwid = 50;
    }
    req.ldiskdat = 0;
    req.mycall = m_callsign.toLocal8Bit();
    req.hiscall = m_dxCall.toLocal8Bit();
    req.hisgrid = m_dxGrid.toLocal8Bit();
    int const boundedDepth = qBound(1, req.ndepth, 4);
    int const unguardedThreadCount = req.threadCount;
    int microStallCleanPeriods = 0;
    int microStallRecentCount = 0;
    bool microStallGuardActive = false;
    {
        QMutexLocker locker(&m_ft8MicroStallGuardMutex);
        microStallGuardActive = m_ft8MicroStallGuard.active();
        microStallRecentCount = m_ft8MicroStallGuard.recentStallCount();
        req.threadCount = m_ft8MicroStallGuard.adjustedThreadCount(
            req.threadCount, boundedDepth);
        microStallCleanPeriods = m_ft8MicroStallGuard.cleanPeriods();
    }
    bool const microStallThreadReduction = req.threadCount < unguardedThreadCount;
    if (!microStallThreadReduction && microStallRecentCount > 0) {
        static qint64 s_lastFt8MicroStallGuardStateLogMs = 0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - s_lastFt8MicroStallGuardStateLogMs >= 5000) {
            s_lastFt8MicroStallGuardStateLogMs = nowMs;
            bridgeLog(QStringLiteral("FT8 micro-stall guard dispatch state: bridge=0x%1 serial=%2 depth=%3 threads=%4 recentStalls=%5 active=%6 cleanPeriods=%7")
                          .arg(reinterpret_cast<quintptr>(this), 0, 16)
                          .arg(serial)
                          .arg(boundedDepth)
                          .arg(req.threadCount)
                          .arg(microStallRecentCount)
                          .arg(microStallGuardActive ? 1 : 0)
                          .arg(microStallCleanPeriods));
        }
    }
    req.supplemental = !adaptiveFt8Pressure && !txAudioActive && boundedDepth >= 4
        && qEnvironmentVariableIsEmpty("DECODIUM_FT8_NO_SUPPLEMENTAL");
    req.subpass = !adaptiveFt8Pressure && subpassRequested && !txAudioActive && boundedDepth >= 4;  // F1: solo l'harvest chiede subpass
    req.coherentAvgEnabled = !adaptiveFt8Pressure && !txAudioActive && m_coherentAvgEnabled;
    req.neuralSyncEnabled = !adaptiveFt8Pressure && !txAudioActive && m_neuralSyncEnabled;
    req.turboFeedbackEnabled = !adaptiveFt8Pressure && !txAudioActive && m_turboFeedbackEnabled;
    if (req.nzhsym >= 50) {
        if (adaptiveFt8Pressure) {
            req.maxDecodeMs = cpuPressureSevereActive() ? 2200 : 2800;
        } else if (m_lowCpuModeEnabled) {
            req.maxDecodeMs = boundedDepth >= 4 ? 6500
                            : boundedDepth >= 3 ? 5500
                                                : 3500;
        } else {
            // Normal mode should favour decode yield over shaving a few
            // seconds.  On busy Windows systems the depth-4 follow-up was
            // repeatedly hitting the 6.5s live deadline and returning fewer
            // CQ decodes than JTDX.
            req.maxDecodeMs = boundedDepth >= 4 ? 10500
                            : boundedDepth >= 3 ? 7500
                                                : 4500;
        }
    } else {
        req.maxDecodeMs = boundedDepth >= 4 ? 3000 : 2200;
    }
    if (maxDecodeMsOverride > 0) {
        req.maxDecodeMs = qMin(req.maxDecodeMs, maxDecodeMsOverride);
    }
    if (!txAudioActive && boundedDepth >= 3) {
        qint64 const visualThrottleUntilMs = QDateTime::currentMSecsSinceEpoch()
            + qMax(500, req.maxDecodeMs) + 250;
        m_deepDecodeVisualSerial = serial;
        m_deepDecodeVisualThrottleUntilMs = qMax(
            m_deepDecodeVisualThrottleUntilMs, visualThrottleUntilMs);
    }
    if (microStallThreadReduction) {
        static qint64 s_lastFt8MicroStallGuardLogMs = 0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - s_lastFt8MicroStallGuardLogMs >= 5000) {
            s_lastFt8MicroStallGuardLogMs = nowMs;
            bridgeLog(QStringLiteral("FT8 micro-stall guard deep dispatch: bridge=0x%1 serial=%2 depth=%3 threads=%4->%5 cleanPeriods=%6 recentStalls=%7 active=%8")
                          .arg(reinterpret_cast<quintptr>(this), 0, 16)
                          .arg(serial)
                          .arg(boundedDepth)
                          .arg(unguardedThreadCount)
                          .arg(req.threadCount)
                          .arg(microStallCleanPeriods)
                          .arg(microStallRecentCount)
                          .arg(microStallGuardActive ? 1 : 0));
        }
    }
    if (adaptiveFt8Pressure) {
        static qint64 s_lastFt8AdaptiveRequestLogMs = 0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - s_lastFt8AdaptiveRequestLogMs >= 5000) {
            s_lastFt8AdaptiveRequestLogMs = nowMs;
            bridgeLog(QStringLiteral("FT8 adaptive pressure profile: serial=%1 depth=%2 threads=%3 maxMs=%4 range=%5-%6 candThin=%7 pressure=%8 severe=%9 startup=%10")
                          .arg(serial)
                          .arg(req.ndepth)
                          .arg(req.threadCount)
                          .arg(req.maxDecodeMs)
                          .arg(req.nfa)
                          .arg(req.nfb)
                          .arg(req.ncandthin)
                          .arg(cpuPressureActive() ? 1 : 0)
                          .arg(cpuPressureSevereActive() ? 1 : 0)
                          .arg(ft8StartupPressure ? 1 : 0));
        }
    }

    if (suppressUiRows) {
        m_ft8EarlyDecodeSerials.insert(serial);
        bridgeLog("FT8 early predecode: serial=" + QString::number(serial) +
                  " nzhsym=" + QString::number(req.nzhsym) +
                  " samples=" + QString::number(audioSnapshot.size()) +
                  " slot=" + QString::number(slotIndexForUtc) +
                  " depth=" + QString::number(req.ndepth) +
                  " ft8ap=" + QString::number(req.lft8apon));
    }
    if (listOnlyRows) {
        m_ft8DeepInTxSerials.insert(serial);
        bridgeLog("FT8 list-only follow-up: serial=" + QString::number(serial) +
                  " nzhsym=" + QString::number(req.nzhsym) +
                  " samples=" + QString::number(audioSnapshot.size()) +
                  " slot=" + QString::number(slotIndexForUtc) +
                  " depth=" + QString::number(req.ndepth) +
                  " maxMs=" + QString::number(req.maxDecodeMs) +
                  " ft8ap=" + QString::number(req.lft8apon));
    }

    auto* worker = m_ft8Worker;
    worker->markLatestDecodeSerial(serial);
    QMetaObject::invokeMethod(worker, [worker, req]() {
        worker->decode(req);
    }, Qt::QueuedConnection);
}

