void DecodiumBridge::onSpectrumTimer()
{
    auto const spectrumTimerStart = std::chrono::steady_clock::now();
    bool metricDelivered = false;
    bool metricHighRes = false;
    bool metricGpu = false;
    bool metricDirectVisual = false;
    bool metricThrottleSkip = false;
    int metricThrottleMinMs = -1;
    int metricUsable = 0;
    int metricWfAvail = 0;
    int metricIntervalMs = m_spectrumTimer ? m_spectrumTimer->interval() : -1;
    auto spectrumTimerMetricGuard = qScopeGuard([&]() {
        qint64 const elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - spectrumTimerStart).count();
        static qint64 lastLogMs = 0;
        static qint64 accumUs = 0;
        static int samples = 0;
        static int maxUs = 0;
        static int deliveredFrames = 0;
        static int highResFrames = 0;
        static int gpuFrames = 0;
        static int directVisualFrames = 0;
        static int throttleSkips = 0;
        static int lastUsable = 0;
        static int lastWfAvail = 0;
        static int lastIntervalMs = -1;
        static int lastThrottleMinMs = -1;
        accumUs += qMax<qint64>(0, elapsedUs);
        ++samples;
        maxUs = qMax(maxUs, static_cast<int>(qMin<qint64>(elapsedUs, std::numeric_limits<int>::max())));
        if (metricDelivered)
            ++deliveredFrames;
        if (metricHighRes)
            ++highResFrames;
        if (metricGpu)
            ++gpuFrames;
        if (metricDirectVisual)
            ++directVisualFrames;
        if (metricThrottleSkip)
            ++throttleSkips;
        lastUsable = metricUsable;
        lastWfAvail = metricWfAvail;
        lastIntervalMs = metricIntervalMs;
        lastThrottleMinMs = metricThrottleMinMs;
        qint64 const logNowMs = QDateTime::currentMSecsSinceEpoch();
        if (lastLogMs == 0)
            lastLogMs = logNowMs;
        if (logNowMs - lastLogMs < 60000 || samples <= 0)
            return;
        double const deliveryPct = samples > 0
            ? (100.0 * static_cast<double>(deliveredFrames) / static_cast<double>(samples))
            : 0.0;
        qInfo().noquote()
            << "[PANMETRIC] spectrum_timer"
            << "avg_us=" << (accumUs / samples)
            << "max_us=" << maxUs
            << "ticks=" << samples
            << "delivered_frames=" << deliveredFrames
            << "highres_frames=" << highResFrames
            << "gpu_frames=" << gpuFrames
            << "direct_visual_frames=" << directVisualFrames
            << "throttle_skips=" << throttleSkips
            << "delivery_pct=" << QString::number(deliveryPct, 'f', 1)
            << "last_usable=" << lastUsable
            << "last_wf_avail=" << lastWfAvail
            << "last_min_interval_ms=" << lastThrottleMinMs
            << "monitoring=" << (m_monitoring ? 1 : 0)
            << "interval_ms=" << lastIntervalMs;
        lastLogMs = logNowMs;
        accumUs = 0;
        samples = 0;
        maxUs = 0;
        deliveredFrames = 0;
        highResFrames = 0;
        gpuFrames = 0;
        directVisualFrames = 0;
        throttleSkips = 0;
        lastUsable = 0;
        lastWfAvail = 0;
        lastIntervalMs = -1;
        lastThrottleMinMs = -1;
    });

    // Native SSTV uses the dedicated DecodiumAudioSink while the embedded
    // legacy backend is intentionally quiescent.  Keep the visual spectrum
    // fed from m_audioBuffer instead of treating legacyPcmSpectrumFeed=false
    // as "no panadapter source".
    if (usingLegacyBackendForRx()
        && !useModernSpectrumFeedWithLegacy()
        && !m_legacyPcmSpectrumFeed
        && !nativeSstvRxForcesDedicatedAudioCapture()) {
        return;
    }
    if (!m_monitoring) return;
    if (m_transmitting || m_tuning) return;
    maybeDispatchBridgeOwnedJtDecodeFallback();
    bool const remoteWaterfallNeedsCpu =
        m_remoteServer
        && m_remoteServer->isRunning()
        && m_remoteServer->waterfallEnabled();
    bool spectrum3dNeedsCpuHistory = false;
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
    bool hasRegisteredPanadapterItem = false;
    bool hasLocalPanadapterConsumer = false;
    m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
    for (const QPointer<PanadapterItem>& ref : m_panadapterItems) {
        PanadapterItem* item = ref.data();
        if (!item) {
            continue;
        }
        hasRegisteredPanadapterItem = true;
        if (!item->isFrameConsumer()) {
            continue;
        }
        hasLocalPanadapterConsumer = true;
        if (item->requiresCpuSpectrumHistory()) {
            spectrum3dNeedsCpuHistory = true;
            break;
        }
    }
#endif
    // The remote waterfall still needs CPU-visible dB rows.  The local 3D
    // spectrum normally samples GPU history directly and reaches this branch
    // only when its shader path is unavailable or has failed.  Do not turn off
    // GPU FFT globally: select asynchronous FFTW only while a CPU consumer is
    // actually present.
    bool const panadapterNeedsCpuSpectrumHistory =
        remoteWaterfallNeedsCpu || spectrum3dNeedsCpuHistory;
    if (!m_spectrumVisible && !remoteWaterfallNeedsCpu) {
        if (m_spectrumTimer) {
            m_spectrumTimer->stop();
        }
        return;
    }
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
    if (hasRegisteredPanadapterItem
        && !hasLocalPanadapterConsumer
        && !remoteWaterfallNeedsCpu) {
        // The only local visual target is hidden/minimised. Do not copy audio
        // or run CPU/GPU FFT work that cannot be presented; a fresh ring
        // snapshot will be taken as soon as the target becomes visible.
        m_lastPanadapterFrameMs = 0;
        return;
    }
#endif

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPanadapterFrameMs <= 0) {
        m_lastPanadapterFrameMs = nowMs;
    }

    // An RTL-SDR is an RF source, not a sound card.  Its waterfall therefore
    // comes from complex IQ and spans centre ± sampleRate/2.  Do this before
    // the legacy 12 kHz PCM path so broadcast FM never appears as flat audio
    // noise in an FT8-sized display.
    if (rtlSdrRfView()) {
        processRtlSdrSpectrum();
        return;
    }

    // Copia i campioni recenti nel ring buffer waterfall (non viene consumato dal decoder).
    // Lock perché il callback del sink può appendere a m_audioBuffer in parallelo.
    if (!m_legacyPcmSpectrumFeed) {
        QMutexLocker locker(&m_audioBufferMutex);
        int avail = m_audioBuffer.size();
        if (avail < m_lastWaterfallAudioBufferSize) {
            m_lastWaterfallAudioBufferSize = 0;
        }
        int const newSamples = qMax(0, avail - m_lastWaterfallAudioBufferSize);
        if (newSamples > 0) {
            int const toCopy = qMin(newSamples, (int)WF_RING_SIZE);
            int const srcStart = avail - toCopy;
            for (int i = 0; i < toCopy; ++i) {
                m_wfRing[m_wfRingPos % WF_RING_SIZE] = m_audioBuffer[srcStart + i];
                ++m_wfRingPos;
            }
            m_lastWaterfallAudioBufferSize = avail;
            if (usingLegacyBackendForTx()
                && useModernSpectrumFeedWithLegacy()
                && avail > qMax((int)WF_RING_SIZE * 2,
                                 targetDecodeSamplesForMode(m_mode) + (int)WF_RING_SIZE)) {
                int const keepSamples = qMax((int)WF_RING_SIZE,
                                             targetDecodeSamplesForMode(m_mode));
                m_audioBuffer = m_audioBuffer.mid(avail - keepSamples);
                m_lastWaterfallAudioBufferSize = m_audioBuffer.size();
                m_audioBuffer.reserve(keepSamples + (int)WF_RING_SIZE);
            }
        }
    }

    // Usa il ring buffer waterfall per la FFT (sempre pieno, indipendente dal decoder)
    int wfAvail = qMin(m_wfRingPos, (int)WF_RING_SIZE);
    metricWfAvail = wfAvail;
    if (wfAvail < 512) {
        if (nowMs >= m_audioWatchdogIgnoreUntilMs
            && nowMs - m_lastPanadapterFrameMs > 6000
            && nowMs - m_lastSpectrumRecoveryMs > 12000) {
            m_lastSpectrumRecoveryMs = nowMs;
            if (m_legacyPcmSpectrumFeed && usingLegacyBackendForRx()) {
                rearmLegacyPcmSpectrumFeed(QStringLiteral("waterfall stalled: no legacy PCM samples"));
            } else {
                restartAudioCaptureFromWatchdog(QStringLiteral("waterfall stalled: no spectrum samples"));
            }
        }
        return;
    }

    // ── Alta risoluzione per PanadapterItem ──────────────────────────────────
    bool highResAvailable = false;
    {
        // Leggi dal ring buffer waterfall — sempre disponibile, mai interrotto
        int fftLen = PANADAPTER_FFT_SIZE;

        // In RTTY la finestra di analisi si accorcia. Non e' questione di
        // gusto: a 12 kHz i 4096 campioni della finestra normale sono 341 ms di
        // segnale, e a 45,45 baud un bit ne dura 22. Ogni riga del waterfall
        // conteneva percio' una quindicina di bit, mark e space finivano
        // insieme nella stessa riga, e le due portanti si vedevano come una
        // banda unica invece che come le due righe parallele che sono.
        //
        // Con 1024 campioni la riga copre 85 ms, tre o quattro bit: le due
        // portanti si separano e l'alternanza fra loro si vede. La FFT resta di
        // 4096 punti — il blocco viene gia' riempito di zeri poche righe piu'
        // sotto — quindi il panadapter non cambia di una virgola e la scala
        // delle frequenze resta quella.
        int const finestraRtty = 1024;
        bool const inRtty = m_mode.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) == 0;
        int usable = qMin(wfAvail, inRtty ? finestraRtty : fftLen);
        if (usable >= 512) {
            highResAvailable = true;
            // Visual-only workload: a dedicated PCM capture can hand frames
            // straight to the visual FFT without waiting for the legacy
            // decoder path. Native SSTV is such a capture while it owns RX;
            // do not apply the conservative legacy/start-up throttle there or
            // the panadapter visibly drops to roughly 10 fps for 30 seconds.
            bool const directVisualFastFeed =
                useModernSpectrumFeedWithLegacy()
                || nativeSstvRxForcesDedicatedAudioCapture();
            metricDirectVisual = directVisualFastFeed;
            // 1.0.186 — FPS cap configurabile (15/20/30 fps). Default 20 fps
            // (50ms interval) bilancia fluidita' UI con frame budget main thread.
            // Era hardcoded 33ms (~30 fps) → su PC modesti causava stall.
            int const fpsCap = qBound(15, m_spectrumFpsCap, 30);
            qint64 const cappedIntervalMs = 1000 / fpsCap;
            bool const deepDecodeVisualLoad = nowMs < m_deepDecodeVisualThrottleUntilMs;
            if (m_gpuPanadapterFftStallGuard.load()
                && m_gpuPanadapterRetryNotBeforeMs > 0
                && nowMs >= m_gpuPanadapterRetryNotBeforeMs
                && m_gpuPanadapterFftStallGuard.exchange(false)) {
                m_lastGpuPanadapterProbeMs = 0;
                qInfo().noquote()
                    << "[PANDBG] Panadapter GPU stall recovery probe eligible"
                    << "attempt=" << m_gpuPanadapterStallRetryCount
                    << "fallback_remains=FFTW_CPU_until_gpu_frame";
            }
            if (!m_gpuPanadapterFftStallGuard.load()
                && m_gpuPanadapterStableSinceMs > 0
                && m_gpuPanadapterFftAvailable.load()
                && m_panadapterGpuFftActive
                && nowMs - m_gpuPanadapterStableSinceMs >= 30000) {
                m_gpuPanadapterStallRetryCount = 0;
                m_gpuPanadapterRetryNotBeforeMs = 0;
                m_gpuPanadapterStableSinceMs = 0;
            }
            // Both the legacy PCM tap and the native SSTV capture can use
            // the same GPU-direct visual path.  Keep one predicate for the
            // source and the GPU readiness; using the legacy-only predicate
            // below made SSTV enter the 100–500 ms safety throttles even
            // though Metal was ready to consume its PCM frames.
            bool const acceleratedVisual =
                (directVisualFastFeed || m_legacyPcmSpectrumFeed)
                && !panadapterNeedsCpuSpectrumHistory
                && !m_lowCpuModeEnabled
                && !m_gpuPanadapterFftStallGuard.load()
                && m_forceGpuPanadapterFft.load()
                && m_gpuPanadapterFftAvailable.load();
            qint64 minPanadapterIntervalMs = directVisualFastFeed
                ? (m_legacyPcmSpectrumFeed ? qMax<qint64>(cappedIntervalMs, 66) : cappedIntervalMs)
                : decodium::decode::legacyPanadapterIntervalMs(
                      deepDecodeVisualLoad,
                      cpuPressureActive(),
                      cpuPressureSevereActive(),
                      acceleratedVisual,
                      static_cast<int>(cappedIntervalMs));
            qint64 const monitoringAgeMs =
                m_monitoringSince.isValid()
                    ? m_monitoringSince.msecsTo(QDateTime::currentDateTimeUtc())
                    : std::numeric_limits<qint64>::max();
            bool const visualStartupWarmup = monitoringAgeMs >= 0 && monitoringAgeMs < 30000;
            if (visualStartupWarmup && !acceleratedVisual)
                minPanadapterIntervalMs = qMax<qint64>(minPanadapterIntervalMs, 100);
            if ((cpuPressureActive() || cpuPressureSevereActive())
                && !acceleratedVisual) {
                minPanadapterIntervalMs = qMax<qint64>(
                    minPanadapterIntervalMs,
                    cpuPressureSevereActive() ? 500 : 250);
            }
            if (m_lowCpuModeEnabled) {
                minPanadapterIntervalMs = qMax<qint64>(
                    minPanadapterIntervalMs,
                    cpuPressureSevereActive() ? 750 : (cpuPressureActive() ? 600 : 500));
            }
            // On the GPU-direct macOS path, drive the timer at the effective
            // cadence instead of polling every 20 ms and discarding ticks.
            // With a 33 ms cap the old polling pattern delivered on alternating
            // 20/40 ms boundaries, limiting the visible result to ~25 fps.
            if (acceleratedVisual && m_spectrumTimer) {
                int const preciseInterval =
                    qBound(10, static_cast<int>(minPanadapterIntervalMs), 750);
                if (m_spectrumTimer->interval() != preciseInterval) {
                    m_spectrumTimer->setInterval(preciseInterval);
                }
            }
            if ((cpuPressureActive() || cpuPressureSevereActive())
                && nowMs - m_lastPanadapterPressureLogMs > 5000) {
                m_lastPanadapterPressureLogMs = nowMs;
                bridgeLog(QStringLiteral("panadapter pressure throttle: interval=%1ms direct_visual=%2 deep=%3 severe=%4 low_cpu=%5 spectrum_timer=%6")
                              .arg(minPanadapterIntervalMs)
                              .arg(directVisualFastFeed ? 1 : 0)
                              .arg(deepDecodeVisualLoad ? 1 : 0)
                              .arg(cpuPressureSevereActive() ? 1 : 0)
                              .arg(m_lowCpuModeEnabled ? 1 : 0)
                              .arg(m_spectrumTimer ? m_spectrumTimer->interval() : -1));
            }
            if (m_lastPanadapterFrameMs > 0
                && nowMs - m_lastPanadapterFrameMs < minPanadapterIntervalMs) {
                metricThrottleSkip = true;
                metricThrottleMinMs = static_cast<int>(minPanadapterIntervalMs);
                metricIntervalMs = static_cast<int>(minPanadapterIntervalMs);
                return;
            }
            metricIntervalMs = static_cast<int>(minPanadapterIntervalMs);
            metricThrottleMinMs = static_cast<int>(minPanadapterIntervalMs);
            int ringStart = (m_wfRingPos - usable) % WF_RING_SIZE;
            if (ringStart < 0)
                ringStart += WF_RING_SIZE;
            int const firstChunk = qMin(usable, WF_RING_SIZE - ringStart);

            int const nfaSnapshot = m_nfa;
            int const nfbSnapshot = m_nfb;
            float const freqPerBin = static_cast<float>(SAMPLE_RATE) / PANADAPTER_FFT_SIZE;
            float const freqMinHz = static_cast<int>(nfaSnapshot / freqPerBin) * freqPerBin;
            float const freqMaxHz = static_cast<int>(nfbSnapshot / freqPerBin) * freqPerBin;
            uint64_t const serial = ++m_panadapterComputeSerial;

            bool const gpuPanadapterFftAllowed =
                !m_lowCpuModeEnabled
                && !m_gpuPanadapterFftStallGuard.load();
            bool const forceGpuPanadapterFft =
                m_forceGpuPanadapterFft.load()
                && gpuPanadapterFftAllowed;
            bool gpuPanadapterFftAvailable =
                m_gpuPanadapterFftAvailable.load()
                && gpuPanadapterFftAllowed;
            if (!gpuPanadapterFftAvailable
                && forceGpuPanadapterFft
                && !panadapterNeedsCpuSpectrumHistory
                && nowMs - m_lastGpuPanadapterProbeMs > 30000) {
                bool const recoveringFromStall = m_gpuPanadapterRetryNotBeforeMs > 0;
                m_lastGpuPanadapterProbeMs = nowMs;
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
                m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
                for (const QPointer<PanadapterItem>& ref : std::as_const(m_panadapterItems)) {
                    if (PanadapterItem* item = ref.data())
                        item->prepareGpuSpectrumRetry();
                }
#endif
                gpuPanadapterFftAvailable = true;
                m_gpuPanadapterFftAvailable.store(true);
                if (recoveringFromStall) {
                    m_gpuPanadapterStableSinceMs = nowMs;
                    m_gpuPanadapterRetryNotBeforeMs = 0;
                }
                qInfo().noquote()
                    << "[PANDBG] Panadapter visual FFT GPU retry probe"
                    << "reason=" << (recoveringFromStall
                                             ? QStringLiteral("stall_recovery_reprobe")
                                             : QStringLiteral("force_gpu_first_reprobe"))
                    << "fallback=FFTW_CPU_on_failure";
            }
            bool const gpuPanadapterFft =
                gpuPanadapterFftAvailable
                && !panadapterNeedsCpuSpectrumHistory;
            if (gpuPanadapterFft) {
                static std::atomic_bool loggedGpuPath {false};
                bool expectedGpuLog = false;
                if (loggedGpuPath.compare_exchange_strong(expectedGpuLog, true)) {
                    qInfo().noquote()
                        << "[PANDBG] Panadapter visual FFT scheduling active"
                        << "thread=QSG_render_thread"
                        << "mode=RHI_compute_gpu"
                        << "force=" << (forceGpuPanadapterFft ? 1 : 0)
                        << "fallback=FFTW_CPU_on_failure";
                }

                m_lastPanadapterFrameMs = nowMs;
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
                bool anyAlive = false;
                // 1.0.569+ - un pannello NASCOSTO ritorna true da addPcmFrameI16
                // senza disegnare nulla. Con due panadapter registrati (DX-Pedition
                // ON: il classico nascosto + quello del workspace) quel true
                // mascherava il rifiuto dell'unico visibile: anyAccepted restava
                // vero, il GPU non veniva mai marcato indisponibile e il fallback
                // FFTW CPU non partiva -> panadapter fermo. Conto le accettazioni
                // solo dei pannelli che possono davvero consumare il frame.
                bool anyConsumer = false;
                bool anyAccepted = false;
                m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
                for (const QPointer<PanadapterItem>& ref : m_panadapterItems) {
                    PanadapterItem* it = ref.data();
                    if (!it)
                        continue;
                    anyAlive = true;
                    if (!it->isFrameConsumer())
                        continue;
                    anyConsumer = true;
                    bool const accepted = it->addPcmFrameI16(m_wfRing,
                                                             WF_RING_SIZE,
                                                             ringStart,
                                                             firstChunk,
                                                             usable,
                                                             nfaSnapshot,
                                                             nfbSnapshot,
                                                             freqMinHz,
                                                             freqMaxHz,
                                                             static_cast<qulonglong>(serial));
                    if (accepted)
                        anyAccepted = true;
                }
                if (anyAlive) {
                    metricDelivered = anyAccepted;
                    metricHighRes = true;
                    metricGpu = true;
                    metricUsable = usable;
                    if (anyConsumer && !anyAccepted) {
                        setGpuPanadapterFftAvailable(
                            false,
                            QStringLiteral("PanadapterItem rejected GPU FFT I16 frame"));
                    }
                    return;
                }
#endif

                QVector<float> pcmFrame(fftLen, 0.0f);
                for (int i = 0; i < firstChunk; ++i)
                    pcmFrame[i] = static_cast<float>(m_wfRing[ringStart + i]);
                if (firstChunk < usable) {
                    int const remaining = usable - firstChunk;
                    for (int i = 0; i < remaining; ++i)
                        pcmFrame[firstChunk + i] = static_cast<float>(m_wfRing[i]);
                }
                emit panadapterPcmFrameReady(std::move(pcmFrame),
                                             usable,
                                             nfaSnapshot,
                                             nfbSnapshot,
                                             freqMinHz,
                                             freqMaxHz,
                                             static_cast<qulonglong>(serial));
                metricDelivered = true;
                metricHighRes = true;
                metricGpu = true;
                metricUsable = usable;
                return;
            }

            // The next frame is being produced by the asynchronous FFTW
            // fallback (3D history, Low CPU, unsupported/failed compute, or a
            // stall guard). Keep the status bar aligned with the real worker.
            if (m_panadapterGpuFftActive)
                setGpuPanadapterFftActive(false, QStringLiteral("CPU FFTW"));

            QVector<short> panadapterSamples(fftLen);
            panadapterSamples.fill(0);
            std::memcpy(panadapterSamples.data(), m_wfRing + ringStart,
                        static_cast<size_t>(firstChunk) * sizeof(short));
            if (firstChunk < usable) {
                std::memcpy(panadapterSamples.data() + firstChunk, m_wfRing,
                            static_cast<size_t>(usable - firstChunk) * sizeof(short));
            }

            // La finestra corta dell'RTTY costa guadagno: con un quarto dei
            // campioni dentro la stessa trasformata, i picchi scendono di
            // dodici decibel e le portanti si vedono smorte. Si recupera
            // alzando i campioni prima della trasformata.
            //
            // Il fattore non e' fisso a quattro: si guarda prima quanto e' alto
            // il campione piu' forte e si alza solo fino a dove ci sta. Un
            // guadagno fisso, su un segnale gia' vicino al fondo scala, lo
            // taglierebbe — e un segnale tagliato nel waterfall si vede come
            // una riga larga e sbavata, cioe' l'opposto di quello che serve.
            if (inRtty && usable > 0 && usable < fftLen) {
                short piccoAssoluto = 1;
                for (int i = 0; i < usable; ++i)
                    piccoAssoluto = std::max<short>(piccoAssoluto,
                                                    static_cast<short>(std::abs(panadapterSamples[i])));
                float const massimoPossibile = 30000.0f / static_cast<float>(piccoAssoluto);
                float const voluto = static_cast<float>(fftLen) / static_cast<float>(usable);
                float const guadagno = std::min(voluto, massimoPossibile);
                if (guadagno > 1.05f) {
                    for (int i = 0; i < usable; ++i)
                        panadapterSamples[i] = static_cast<short>(
                            std::lround(panadapterSamples[i] * guadagno));
                }
            }

            static std::atomic_bool loggedRemoteCpu {false};
            static std::atomic_bool logged3dCpuHistory {false};
            bool expectedRemoteCpuLog = false;
            if (remoteWaterfallNeedsCpu
                && loggedRemoteCpu.compare_exchange_strong(expectedRemoteCpuLog, true)) {
                qInfo().noquote()
                    << "[PANDBG] Panadapter visual FFT GPU path bypassed"
                    << "reason=remote_waterfall_needs_CPU_db_values"
                    << "fallback=FFTW_CPU";
            }
            bool expected3dCpuLog = false;
            if (spectrum3dNeedsCpuHistory
                && logged3dCpuHistory.compare_exchange_strong(expected3dCpuLog, true)) {
                qInfo().noquote()
                    << "[PANDBG] Panadapter GPU direct path bypassed"
                    << "reason=stacked_3D_spectrum_needs_CPU_history"
                    << "fallback=async_FFTW_CPU";
            }

            bool expectedIdle = false;
            if (!m_panadapterComputeBusy.compare_exchange_strong(expectedIdle, true)) {
                return;
            }
            m_lastPanadapterFrameMs = nowMs;
            metricDelivered = true;
            metricHighRes = true;
            metricGpu = false;
            metricUsable = usable;

            static std::atomic_bool loggedAsyncPath {false};
            bool expectedLog = false;
            if (loggedAsyncPath.compare_exchange_strong(expectedLog, true)) {
                qInfo().noquote()
                    << "[PANDBG] Panadapter visual FFT scheduling active"
                    << "thread=QThreadPool"
                    << "mode=async"
                    << "fallback=skip_frame_when_busy";
            }

            QPointer<DecodiumBridge> guard(this);
            auto* task = QRunnable::create([
                guard,
                samples = std::move(panadapterSamples),
                nfaSnapshot,
                nfbSnapshot,
                usable,
                freqMinHz,
                freqMaxHz,
                serial
            ]() mutable {
                PanadapterFrameResult result = DecodiumBridge::computePanadapterFrame(samples,
                                                                                      nfaSnapshot,
                                                                                      nfbSnapshot);
                if (!guard)
                    return;

                QMetaObject::invokeMethod(guard, [
                    guard,
                    values = std::move(result.values),
                    minDb = result.minDb,
                    maxDb = result.maxDb,
                    usable,
                    freqMinHz,
                    freqMaxHz,
                    serial
                ]() mutable {
                    if (!guard)
                        return;
                    guard->finishPanadapterFrame(std::move(values),
                                                 minDb,
                                                 maxDb,
                                                 usable,
                                                 freqMinHz,
                                                 freqMaxHz,
                                                 serial);
                }, Qt::QueuedConnection);
            });
            QThreadPool::globalInstance()->start(task, -1);
        }
    }

    if (highResAvailable) {
        return;
    }

    // ── Legacy 512-bin fallback ──────────────────────────────────────────────
    {
        // Only used before the panadapter has enough samples for a high-res
        // frame. Do not interleave it with high-res frames: alternating 512-bin
        // and 4096-bin images makes the line graph flicker at fast intervals.
        m_spectrumBuf.resize(SPECTRUM_FFT_SIZE);
        for (int i = 0; i < SPECTRUM_FFT_SIZE; ++i) {
            int pos = (m_wfRingPos - SPECTRUM_FFT_SIZE + i + WF_RING_SIZE * 2) % WF_RING_SIZE;
            m_spectrumBuf[i] = m_wfRing[pos];
        }
        QVector<float> spectrum = computeSpectrum();
        if (!spectrum.isEmpty()) {
            m_lastPanadapterFrameMs = nowMs;
            emit spectrumDataReady(spectrum);
            metricDelivered = true;
            metricHighRes = false;
            metricGpu = false;
            metricUsable = SPECTRUM_FFT_SIZE;
        }
    }
}


