namespace {
RtlSdrDsp::Demodulator rtlSdrDemodulatorFromSetting(const QVariant& value)
{
    const QString name = value.toString().trimmed().toLower();
    if (name == QStringLiteral("wfm")) return RtlSdrDsp::Demodulator::WideFm;
    if (name == QStringLiteral("nfm")) return RtlSdrDsp::Demodulator::NarrowFm;
    if (name == QStringLiteral("am")) return RtlSdrDsp::Demodulator::Am;
    if (name == QStringLiteral("usb")) return RtlSdrDsp::Demodulator::Usb;
    if (name == QStringLiteral("lsb")) return RtlSdrDsp::Demodulator::Lsb;
    if (name == QStringLiteral("cw")) return RtlSdrDsp::Demodulator::Cw;
    return RtlSdrDsp::Demodulator::WeakSignal;
}

RtlSdrDsp::SsbAgcMode rtlSdrSsbAgcModeFromSetting(const QVariant& value)
{
    const QString name = value.toString().trimmed().toLower();
    if (name == QStringLiteral("off")) return RtlSdrDsp::SsbAgcMode::Off;
    if (name == QStringLiteral("medium")) return RtlSdrDsp::SsbAgcMode::Medium;
    return RtlSdrDsp::SsbAgcMode::Slow;
}

RtlSdrDsp::SsbNoiseReductionMode rtlSdrSsbNoiseReductionFromSetting(
    const QVariant& value)
{
    const QString name = value.toString().trimmed().toLower();
    if (name == QStringLiteral("light")) {
        return RtlSdrDsp::SsbNoiseReductionMode::Light;
    }
    if (name == QStringLiteral("medium")) {
        return RtlSdrDsp::SsbNoiseReductionMode::Medium;
    }
    return RtlSdrDsp::SsbNoiseReductionMode::Off;
}
}

void DecodiumBridge::startRtlSdrCapture()
{
    if (!m_rtlSdrInput) {
        return;
    }
    if (!RtlSdrInput::compiledIn()) {
        const QString message = QStringLiteral("RTL-SDR support is not included in this build.");
        if (m_rtlSdrStatus != message) {
            m_rtlSdrStatus = message;
            emit rtlSdrStatusChanged();
        }
        emit errorMessage(message);
        return;
    }

    RtlSdrInput::Config config;
    config.deviceIndex = qMax(0, getSetting(QStringLiteral("RtlSdrDeviceIndex"), 0).toInt());
    config.demodulator = rtlSdrDemodulatorFromSetting(
        getSetting(QStringLiteral("RtlSdrDemodulator"), QStringLiteral("weak")));
    config.sampleRate = getSetting(QStringLiteral("RtlSdrSampleRate"), 240000).toInt();
    if (!RtlSdrDsp::isSupportedSampleRateForDemodulator(config.sampleRate,
                                                         config.demodulator)) {
        config.sampleRate = config.demodulator == RtlSdrDsp::Demodulator::WideFm
            ? 960000 : 240000;
    }
    config.ppmCorrection = qBound(-500, getSetting(QStringLiteral("RtlSdrPpmCorrection"), 0).toInt(), 500);
    config.gainTenthsDb = qBound(-1, getSetting(QStringLiteral("RtlSdrGainTenthsDb"), -1).toInt(), 500);
    config.digitalAgc = getSetting(QStringLiteral("RtlSdrDigitalAgc"), false).toBool();
    config.biasTee = getSetting(QStringLiteral("RtlSdrBiasTee"), false).toBool();
    config.audioGain = qBound(0.05, getSetting(QStringLiteral("RtlSdrAudioGain"), 1.0).toDouble(), 50.0);
    config.ssbVoiceBandwidthHz = qBound(
        1800.0, getSetting(QStringLiteral("RtlSdrSsbVoiceBandwidthHz"), 3500.0).toDouble(),
        4000.0);
    config.ssbAgcMode = rtlSdrSsbAgcModeFromSetting(
        getSetting(QStringLiteral("RtlSdrSsbAgcMode"), QStringLiteral("slow")));
    config.ssbNotchFrequencyHz = qBound(
        0, getSetting(QStringLiteral("RtlSdrSsbNotchFrequencyHz"), 0).toInt(), 4800);
    config.ssbNoiseReduction = rtlSdrSsbNoiseReductionFromSetting(
        getSetting(QStringLiteral("RtlSdrSsbNoiseReduction"), QStringLiteral("off")));
    config.mode = getSetting(QStringLiteral("RtlSdrMode"), QStringLiteral("sdr"))
                          .toString().trimmed().compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0
        ? RtlSdrInput::Mode::DirectSampling
        : RtlSdrInput::Mode::SdrRadio;

    const bool followDial = getSetting(QStringLiteral("RtlSdrFollowDial"), true).toBool();
    const qint64 configuredFrequency = getSetting(QStringLiteral("RtlSdrFrequencyHz"),
                                                   static_cast<qint64>(m_frequency)).toLongLong();
    const qint64 selectedFrequency = followDial ? qRound64(m_frequency) : configuredFrequency;
    constexpr qint64 kMinimumRtlFrequencyHz = 100000;
    constexpr qint64 kMaximumRtlFrequencyHz = 1766000000;
    const qint64 selectedRfFrequency = qBound<qint64>(kMinimumRtlFrequencyHz,
                                                       selectedFrequency,
                                                       kMaximumRtlFrequencyHz);
    const bool ifEnabled = getSetting(QStringLiteral("RtlSdrIfEnabled"), false).toBool();
    const QString requestedIfSideband = getSetting(
        QStringLiteral("RtlSdrIfSideband"), QStringLiteral("auto"))
                                            .toString().trimmed().toLower();
    const bool useLsbIfShift = requestedIfSideband == QStringLiteral("lsb")
        || (requestedIfSideband == QStringLiteral("auto")
            && config.demodulator == RtlSdrDsp::Demodulator::Lsb);
    decodium::rtl_sdr::TuningRequest tuningRequest;
    tuningRequest.dialFrequencyHz = selectedRfFrequency;
    tuningRequest.sampleRate = config.sampleRate;
    tuningRequest.ifEnabled = ifEnabled;
    tuningRequest.ifFrequencyHz = getSetting(
        QStringLiteral("RtlSdrIfFrequencyHz"), 8830000).toLongLong();
    tuningRequest.usbShiftHz = getSetting(
        QStringLiteral("RtlSdrIfUsbShiftHz"), 1500).toLongLong();
    tuningRequest.lsbShiftHz = getSetting(
        QStringLiteral("RtlSdrIfLsbShiftHz"), -1500).toLongLong();
    tuningRequest.sideband = useLsbIfShift
        ? decodium::rtl_sdr::IfSideband::Lsb
        : decodium::rtl_sdr::IfSideband::Usb;
    tuningRequest.spectrumInverted = getSetting(
        QStringLiteral("RtlSdrIfSpectrumInverted"), false).toBool();
    const decodium::rtl_sdr::TuningPlan tuningPlan =
        decodium::rtl_sdr::makeTuningPlan(tuningRequest,
                                          kMinimumRtlFrequencyHz,
                                          kMaximumRtlFrequencyHz);
    QString modeAdjustment;
    if (config.mode == RtlSdrInput::Mode::DirectSampling) {
        const QString deviceIdentity = m_rtlSdrDevices.value(config.deviceIndex);
        const auto blockReason = decodium::rtl_sdr::directSamplingBlockReason(
            deviceIdentity, tuningPlan.selectedInputFrequencyHz);
        if (blockReason
            == decodium::rtl_sdr::DirectSamplingBlockReason::BlogV4UsesUpconverter) {
            modeAdjustment = QStringLiteral(
                "RTL-SDR Blog V4 uses its tuner and automatic HF upconverter; "
                "Direct Sampling is unavailable on this receiver.");
        } else if (blockReason
                   == decodium::rtl_sdr::DirectSamplingBlockReason::OutsideHfRange) {
            modeAdjustment = QStringLiteral(
                "RTL-SDR Direct Sampling is limited to 500 kHz–24 MHz; "
                "using SDR Radio at %1 Hz.").arg(tuningPlan.selectedInputFrequencyHz);
        }
        if (!modeAdjustment.isEmpty()) {
            config.mode = RtlSdrInput::Mode::SdrRadio;
            setSetting(QStringLiteral("RtlSdrMode"), QStringLiteral("sdr"));
            bridgeLog(QStringLiteral("RTL-SDR mode fallback: %1").arg(modeAdjustment));
            if (m_rtlSdrStatus != modeAdjustment) {
                m_rtlSdrStatus = modeAdjustment;
                emit rtlSdrStatusChanged();
            }
            emit statusMessage(modeAdjustment);
        }
    }
    config.centerFrequencyHz = tuningPlan.hardwareCenterFrequencyHz;
    config.channelOffsetHz = tuningPlan.channelOffsetHz;
    config.spectrumInverted = tuningPlan.spectrumInverted;

    const bool displayMappingChanged =
        m_rtlSdrRfCenterFrequencyHz != tuningPlan.displayCenterFrequencyHz
        || m_rtlSdrRfSelectedFrequencyHz != tuningPlan.displaySelectedFrequencyHz
        || m_rtlSdrRfSpectrumInverted != tuningPlan.spectrumInverted;
    if (displayMappingChanged) {
        m_rtlSdrRfCenterFrequencyHz = tuningPlan.displayCenterFrequencyHz;
        m_rtlSdrRfSelectedFrequencyHz = tuningPlan.displaySelectedFrequencyHz;
        m_rtlSdrRfSpectrumInverted = tuningPlan.spectrumInverted;
        m_rtlSdrSpectrumSerial.fetch_add(1);
        m_rtlSdrIqCount = 0;
        m_rtlSdrIqWritePos = 0;
        m_lastPanadapterData.clear();
        emit rtlSdrRfCenterFrequencyChanged();
        emit rtlSdrRfSelectedFrequencyChanged();
        emit rtlSdrRfViewChanged();
    }

    const RtlSdrInput::Config active = m_rtlSdrInput->activeConfig();
    const bool ssbAudioControlsActive = config.demodulator == RtlSdrDsp::Demodulator::Usb
        || config.demodulator == RtlSdrDsp::Demodulator::Lsb;
    const bool sameSsbAudioControls = !ssbAudioControlsActive
        || (qFuzzyCompare(active.ssbVoiceBandwidthHz + 1.0,
                          config.ssbVoiceBandwidthHz + 1.0)
            && active.ssbAgcMode == config.ssbAgcMode
            && active.ssbNotchFrequencyHz == config.ssbNotchFrequencyHz
            && active.ssbNoiseReduction == config.ssbNoiseReduction);
    const bool sameReceiverConfiguration = m_rtlSdrInput->isRunning()
        && active.deviceIndex == config.deviceIndex
        && active.sampleRate == config.sampleRate
        && active.ppmCorrection == config.ppmCorrection
        && active.gainTenthsDb == config.gainTenthsDb
        && active.digitalAgc == config.digitalAgc
        && active.mode == config.mode
        && active.demodulator == config.demodulator
        && active.spectrumInverted == config.spectrumInverted
        && active.biasTee == config.biasTee
        && qFuzzyCompare(active.audioGain + 1.0, config.audioGain + 1.0)
        && sameSsbAudioControls;
    const bool unchanged = sameReceiverConfiguration
        && active.centerFrequencyHz == config.centerFrequencyHz
        && active.channelOffsetHz == config.channelOffsetHz;
    if (unchanged) {
        return;
    }

    // Discard frames from the previous centre/rate before the reader replaces
    // its USB stream.  The outstanding FFT task observes the bumped serial.
    m_rtlSdrSpectrumSerial.fetch_add(1);
    m_rtlSdrIqCount = 0;
    m_rtlSdrIqWritePos = 0;
    m_lastPanadapterData.clear();
    if (sameReceiverConfiguration
        && m_rtlSdrInput->retune(config.centerFrequencyHz, config.channelOffsetHz)) {
#if DECODIUM_HAS_SSTV
        const auto source = decodium::sstv::SstvAudioSourceKind::RtlSdr;
        selectSstvRxSource(source, currentSstvAudioStreamId(source), true);
#endif
        m_audioWatchdogIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1500;
        bridgeLog(QStringLiteral(
            "RTL-SDR in-place retune requested dial=%1 input=%2 tuner=%3 offset=%4 if=%5 sideband=%6")
                      .arg(selectedRfFrequency)
                      .arg(tuningPlan.selectedInputFrequencyHz)
                      .arg(config.centerFrequencyHz)
                      .arg(config.channelOffsetHz)
                      .arg(ifEnabled ? 1 : 0)
                      .arg(useLsbIfShift ? QStringLiteral("LSB") : QStringLiteral("USB")));
        return;
    }
    if (config.demodulator == RtlSdrDsp::Demodulator::WeakSignal) {
        stopRtlSdrAudioPlayback(QStringLiteral("weak-signal demodulator selected"));
    }
    if (active.demodulator != config.demodulator) {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer.clear();
        m_lastWaterfallAudioBufferSize = 0;
    }

    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 5000;
    bridgeLog(QStringLiteral("RTL-SDR capture requested device=%1 mode=%2 demod=%3 dial=%4 input=%5 tuner=%6 offset=%7 rate=%8 ppm=%9 gain_tenths=%10 digital_agc=%11 bias=%12 if=%13 sideband=%14 inverted=%15")
                  .arg(config.deviceIndex)
                  .arg(RtlSdrInput::modeName(config.mode))
                  .arg(RtlSdrInput::demodulatorName(config.demodulator))
                  .arg(selectedRfFrequency)
                  .arg(tuningPlan.selectedInputFrequencyHz)
                  .arg(config.centerFrequencyHz)
                  .arg(config.channelOffsetHz)
                  .arg(config.sampleRate)
                  .arg(config.ppmCorrection)
                  .arg(config.gainTenthsDb)
                  .arg(config.digitalAgc ? 1 : 0)
                  .arg(config.biasTee ? 1 : 0)
                  .arg(ifEnabled ? 1 : 0)
                  .arg(useLsbIfShift ? QStringLiteral("LSB") : QStringLiteral("USB"))
                  .arg(tuningPlan.spectrumInverted ? 1 : 0));
#if DECODIUM_HAS_SSTV
    const auto sstvSource = decodium::sstv::SstvAudioSourceKind::RtlSdr;
    selectSstvRxSource(sstvSource,
                       currentSstvAudioStreamId(sstvSource),
                       true);
#endif
    m_rtlSdrInput->start(config);
}

void DecodiumBridge::stopRtlSdrCapture()
{
    if (m_rtlSdrRetuneTimer) {
        m_rtlSdrRetuneTimer->stop();
    }
    if (m_rtlSdrInput) {
        m_rtlSdrInput->stop();
    }
    m_rtlSdrSpectrumSerial.fetch_add(1);
    m_rtlSdrIqCount = 0;
    stopRtlSdrAudioPlayback(QStringLiteral("RTL-SDR capture stopped"));
}

void DecodiumBridge::scheduleRtlSdrRetune()
{
    if (!m_rtlSdrRetuneTimer) {
        return;
    }

    // QML can emit many intermediate dial values while the user edits the
    // frequency. Coalesce the burst before asking the reader for an in-place
    // asynchronous retune. In IF mode this only remaps the displayed RF scale;
    // the fixed IF stream remains untouched.
    constexpr int kRtlSdrRetuneDebounceMs = 250;
    m_rtlSdrRetuneTimer->start(kRtlSdrRetuneDebounceMs);
}

void DecodiumBridge::applyRtlSdrSettings(const QString& reason)
{
    const bool requestedGeneralReceiver = rtlSdrGeneralReceiverConfigured();
    const bool activeGeneralReceiver = m_rtlSdrInput && m_rtlSdrInput->isActive()
        && m_rtlSdrInput->activeConfig().demodulator
               != RtlSdrDsp::Demodulator::WeakSignal;
    const bool backendSelectionChanged =
        reason.contains(QStringLiteral("RtlSdrEnabled"))
        || (m_rtlSdrInput && m_rtlSdrInput->isActive()
            && activeGeneralReceiver != requestedGeneralReceiver);
    const bool monitorWasActive = m_monitoring || m_monitorRequested
        || (m_legacyBackend && m_legacyBackend->monitoring());

    if (backendSelectionChanged && monitorWasActive) {
        // Rebuild the complete RX ownership state. Merely replacing the USB
        // reader leaves the legacy backend monitoring and/or the FT decoder
        // timers alive, which is how startup previously continued to decode
        // the sound card while the user had selected WFM.
        bridgeLog(QStringLiteral("RTL-SDR RX ownership change: %1").arg(reason));
        ++m_periodTimerSessionId;
        m_monitorRequested = false;
        if (m_legacyBackend && m_legacyBackend->monitoring()) {
            m_legacyBackend->setMonitoring(false);
        }
        m_periodTimer->stop();
        m_asyncDecodeTimer->stop();
        m_spectrumTimer->stop();
        m_asyncDecodePending = false;
        stopAudioCapture();
        if (m_monitoring) {
            m_monitoring = false;
            emit monitoringChanged();
        }
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        resetRxPeriodAccumulation(false);
        QTimer::singleShot(0, this, [this]() {
            if (!m_shuttingDown) {
                startRx();
            }
        });
        return;
    }

    if (!rtlSdrEnabled()) {
        stopRtlSdrCapture();
        if (m_monitoring) {
            QTimer::singleShot(0, this, [this]() {
                if (m_monitoring && !rtlSdrEnabled()) {
                    startAudioCapture();
                }
            });
        }
        return;
    }
    if (!m_monitoring) {
        return;
    }
    const RtlSdrDsp::Demodulator demodulator = rtlSdrDemodulatorFromSetting(
        getSetting(QStringLiteral("RtlSdrDemodulator"), QStringLiteral("weak")));
    const bool audioEnabled = getSetting(QStringLiteral("RtlSdrAudioEnabled"),
                                         demodulator != RtlSdrDsp::Demodulator::WeakSignal).toBool();
    if (!audioEnabled || demodulator == RtlSdrDsp::Demodulator::WeakSignal
        || reason.contains(QStringLiteral("RtlSdrAudioOutputDevice"))) {
        stopRtlSdrAudioPlayback(QStringLiteral("RTL-SDR receiver audio disabled"));
    }
    if (m_transmitting || m_tuning) {
        emit statusMessage(QStringLiteral("RTL-SDR settings will apply after TX/Tune."));
        return;
    }
    bridgeLog(QStringLiteral("RTL-SDR settings changed: %1").arg(reason));
    if (m_soundInput || m_tciAudioCaptureActive) {
        stopAudioCapture();
    }
    startRtlSdrCapture();
}

