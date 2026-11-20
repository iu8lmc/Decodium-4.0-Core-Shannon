bool DecodiumBridge::startSstvCalibrationTone(const QString& toneId)
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_sstvTxCoordinator) {
        initialiseSstvTx();
    }
    auto reject = [this](const QString& detail) {
        m_sstvTxError = detail;
        emit sstvTxStateChanged();
        emit errorMessage(detail);
        return false;
    };

    const std::string requestedId = toneId.trimmed().toStdString();
    const std::optional<decodium::sstv::SstvCalibrationToneKind> kind
        = decodium::sstv::calibrationToneKindFromId(requestedId);
    if (!kind.has_value()) {
        return reject(tr("The selected SSTV calibration reference is invalid"));
    }
    if (!m_sstvTxCoordinator) {
        return reject(tr("Native SSTV TX is unavailable"));
    }
    if (!sstvTxGlobalPreflightReady()) {
        return reject(sstvTxPreflightRejection());
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("sstv-tx"))) {
        return reject(tr("SSTV TX was blocked by the Decodium SWR safety check"));
    }

    std::unique_ptr<decodium::sstv::SstvPcm16Source> source;
    try {
        source = decodium::sstv::makeCalibrationTonePcm16Source(
            *kind, 48'000U, 2'000U);
    } catch (const std::exception& exception) {
        return reject(tr("Cannot prepare the SSTV calibration reference: %1")
                          .arg(QString::fromUtf8(exception.what())));
    }
    if (!source || source->sampleRate() != 48'000U
        || source->totalSamples() == 0U) {
        return reject(tr("The SSTV calibration audio source is invalid"));
    }

    bool requestedDeviceFound = false;
    const QAudioDevice output = resolveTxOutputDevice(
        &requestedDeviceFound);
    if (!sstvOutputMatchesConfiguredSelection(
            m_audioOutputDevice, m_audioOutputDeviceId,
            requestedDeviceFound, output)) {
        return reject(tr("The configured Decodium TX audio output is unavailable; reconnect it or select an available output before SSTV TX"));
    }
    const unsigned channels
        = output.preferredFormat().channelCount() <= 1 ? 1U : 2U;
    const decodium::sstv::SstvCalibrationToneSpec& spec
        = decodium::sstv::calibrationToneSpec(*kind);

    decodium::sstv::SstvTxPreparedAudioRequest request;
    request.mode = std::string("calibration-") + spec.id;
    request.width = 1U;
    request.height = 1U;
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0
        || m_audioOutputChannel == 3) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    } else if (m_audioOutputChannel == 1) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    } else {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    }
    request.headerEndFrame = 0U;
    request.imageEndFrame = source->totalSamples();
    request.headroom = decodium::sstv::kDefaultSstvTxHeadroom;
    request.source = std::move(source);

    // 1.0.588: far scadere QUI la sessione precedente, prima di fissare il
    // dispositivo. start()/startPrepared() cominciano con un tick del
    // coordinatore: se la sessione di prima e' in uno stato terminale, quel
    // tick emette stateChanged, il nostro gancio chiama
    // releaseSstvTxBridgeOwnership() e cancella il pin appena messo. Il
    // preflight che segue trovava il dispositivo non fissato e rifiutava, cosi'
    // la prima trasmissione riusciva e ogni successiva no.
    if (m_sstvTxCoordinator) {
        static_cast<void>(m_sstvTxCoordinator->tick(sstvTxNowMs()));
    }

    // Resolve and pin exactly once before the coordinator's shared preflight,
    // just like an image TX.  The normal release barrier clears this route.
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    const bool audioOnly = sstvTxAudioOnlyAllowed();
    m_sstvTxStartInProgress = true;
    const decodium::sstv::SstvTxCoordinatorResult result
        = m_sstvTxCoordinator->startPrepared(
            sstvTxNowMs(), std::move(request));
    m_sstvTxStartInProgress = false;
    if (!result.accepted) {
        m_sstvTxOutputDevice = QAudioDevice {};
        m_sstvTxOutputChannels = 0U;
        m_sstvTxOutputPinned = false;
        const QString detail = result.detail.empty()
            ? tr("SSTV TX preflight failed")
            : QString::fromStdString(result.detail);
        return reject(detail);
    }
    m_sstvTxSessionId = static_cast<quint64>(result.sessionId);
    emit sstvTxStateChanged();
    emit statusMessage(
        audioOnly
            ? tr("SSTV audio-only calibration accepted: %1")
                  .arg(QString::fromLatin1(spec.id))
            : tr("SSTV calibration reference accepted: %1")
                  .arg(QString::fromLatin1(spec.id)));
    return true;
#else
    Q_UNUSED(toneId)
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

