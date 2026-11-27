bool DecodiumBridge::startSstvRadiofaxTx(int ioc, int linesPerMinute)
{
    return startSstvRadiofaxTxAligned(ioc, linesPerMinute, false);
}

bool DecodiumBridge::startSstvRadiofaxTxAligned(int ioc, int linesPerMinute, bool centeredPhasing)
{
    return startSstvRadiofaxTxProfile(ioc, linesPerMinute, centeredPhasing, false);
}

bool DecodiumBridge::startSstvRadiofaxTxProfile(int ioc, int linesPerMinute, bool centeredPhasing, bool wmoDeadSector)
{
    return startSstvRadiofaxTxAdvanced(ioc, linesPerMinute, centeredPhasing, wmoDeadSector, false);
}

bool DecodiumBridge::startSstvRadiofaxTxAdvanced(int ioc, int linesPerMinute, bool centeredPhasing, bool wmoDeadSector, bool symmetricPhasing)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator) initialiseSstvTx();
    auto reject = [this](const QString& detail) {
        m_sstvTxError = detail;
        emit sstvTxStateChanged();
        emit errorMessage(detail);
        return false;
    };
    if (!m_sstvTxCoordinator || !m_sstvStudioController)
        return reject(tr("Native SSTV TX is unavailable"));
    if ((ioc != 288 && ioc != 576)
        || (linesPerMinute != 60 && linesPerMinute != 90
            && linesPerMinute != 120 && linesPerMinute != 240))
        return reject(tr("Invalid Radiofax IOC or line rate"));
    if (centeredPhasing && symmetricPhasing)
        return reject(tr("Centered and symmetric Radiofax phasing cannot be combined"));
    const auto prepared = m_sstvStudioController->preparedSnapshot();
    if (!prepared || prepared->isNull())
        return reject(tr("Prepare an image before transmitting Radiofax"));
    if (!sstvTxCanStart()) return reject(sstvTxPreflightRejection());
    if (!checkSwrAllowsTransmission(QStringLiteral("sstv-tx")))
        return reject(tr("Radiofax TX was blocked by the Decodium SWR safety check"));
    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(&requestedDeviceFound);
    if (!sstvOutputMatchesConfiguredSelection(m_audioOutputDevice, m_audioOutputDeviceId,
                                              requestedDeviceFound, output))
        return reject(tr("The configured Decodium TX audio output is unavailable"));
    const unsigned channels = output.preferredFormat().channelCount() <= 1 ? 1U : 2U;
    decodium::sstv::SstvFaxConfig config;
    config.ioc = ioc == 288 ? decodium::sstv::SstvFaxIoc::Ioc288 : decodium::sstv::SstvFaxIoc::Ioc576;
    config.linesPerMinute = static_cast<std::uint16_t>(linesPerMinute);
    config.phasing = symmetricPhasing ? decodium::sstv::SstvFaxPhasePattern::Symmetric
                                      : decodium::sstv::SstvFaxPhasePattern::WhiteFivePercent;
    config.centeredPhasingWithWhiteEndLine = centeredPhasing;
    config.wmoDeadSector = wmoDeadSector;
    config.sampleRate = 48'000U;
    config.width = static_cast<std::uint32_t>(prepared->width());
    config.height = static_cast<std::uint32_t>(prepared->height());
    decodium::sstv::SstvTxPreparedAudioRequest request;
    try {
        const QImage gray = prepared->convertToFormat(QImage::Format_Grayscale8);
        if (gray.isNull()) return reject(tr("Cannot convert the image to grayscale"));
        std::vector<std::uint8_t> pixels;
        pixels.reserve(static_cast<std::size_t>(config.width) * config.height);
        for (int y = 0; y < gray.height(); ++y) {
            const auto* line = gray.constScanLine(y);
            pixels.insert(pixels.end(), line, line + gray.width());
        }
        const decodium::sstv::SstvRadiofaxMapper mapper(config);
        request.source = decodium::sstv::makeRadiofaxPcm16Source(pixels, config);
        request.headerEndFrame = mapper.imageStartSample();
        // Radiofax stop and final black are part of the transmitted protocol;
        // keep PTT and the image-phase watchdog active until they finish.
        request.imageEndFrame = mapper.totalSamples();
    } catch (const std::exception& exception) {
        return reject(tr("Cannot prepare Radiofax audio: %1").arg(QString::fromUtf8(exception.what())));
    }
    request.mode = "hffax-experimental";
    request.width = config.width;
    request.height = config.height;
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0 || m_audioOutputChannel == 3)
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    else if (m_audioOutputChannel == 1)
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    else request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    m_sstvTxStartInProgress = true;
    const auto result = m_sstvTxCoordinator->startPrepared(sstvTxNowMs(), std::move(request));
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        return reject(result.detail.empty() ? tr("Radiofax TX preflight failed")
                                            : QString::fromStdString(result.detail));
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    emit sstvTxStateChanged();
    emit statusMessage(tr("Experimental Radiofax TX accepted"));
    return true;
#else
    Q_UNUSED(ioc) Q_UNUSED(linesPerMinute) Q_UNUSED(centeredPhasing) Q_UNUSED(wmoDeadSector) Q_UNUSED(symmetricPhasing)
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

