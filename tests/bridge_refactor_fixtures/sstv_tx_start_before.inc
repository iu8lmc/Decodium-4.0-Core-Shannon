bool DecodiumBridge::startSstvTx(const QString& fskId)
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
    if (!m_sstvTxCoordinator || !m_sstvStudioController) {
        return reject(tr("Native SSTV TX is unavailable"));
    }
    const std::shared_ptr<const QImage> prepared
        = m_sstvStudioController->preparedSnapshot();
    if (!prepared || prepared->isNull()) {
        return reject(tr("Prepare an SSTV image before transmitting"));
    }
    if (!sstvTxCanStart()) {
        return reject(sstvTxPreflightRejection());
    }
    if (!checkSwrAllowsTransmission(QStringLiteral("sstv-tx"))) {
        return reject(tr("SSTV TX was blocked by the Decodium SWR safety check"));
    }

    const std::string modeId
        = m_sstvStudioController->modeId().toStdString();
    const std::optional<decodium::sstv::SstvTxCoordinatorMode> mode
        = decodium::sstv::SstvTxSourceBuilder::modeFromId(modeId);
    if (!mode.has_value()) {
        return reject(tr("The selected SSTV mode is not executable"));
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

    decodium::sstv::SstvTxCoordinatorRequest request;
    request.mode = *mode;
    request.channelCount = channels;
    if (channels == 1U || m_audioOutputChannel == 0
        || m_audioOutputChannel == 3) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Both;
    } else if (m_audioOutputChannel == 1) {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Left;
    } else {
        request.channelRoute = decodium::sstv::SstvTxChannelRoute::Right;
    }
    try {
        request.pixels
            = decodium::sstv::SstvTxSourceBuilder::pixelsFromImage(
                *prepared, *mode);
    } catch (const std::exception& exception) {
        return reject(tr("Cannot prepare SSTV TX pixels: %1")
                          .arg(QString::fromUtf8(exception.what())));
    }

    const QString identifier = fskId.trimmed().toUpper();
    if (!identifier.isEmpty()) {
        decodium::sstv::SstvTxFskIdPlan plan;
        plan.text = identifier.toStdString();
        request.fskId = std::move(plan);
    }

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

    // Resolve exactly once, before any PTT ownership is acquired. The value
    // object and channel plan stay pinned until the coordinator releases this
    // session; later settings changes cannot redirect an in-flight TX.
    m_sstvTxOutputDevice = output;
    m_sstvTxOutputChannels = channels;
    m_sstvTxOutputPinned = true;
    m_sstvTxError.clear();
    m_sstvTxProgress = 0.0;
    const bool audioOnly = sstvTxAudioOnlyAllowed();
    m_sstvTxStartInProgress = true;
    const decodium::sstv::SstvTxCoordinatorResult result
        = m_sstvTxCoordinator->start(sstvTxNowMs(), request);
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
    queueSstvStudioTransmittedImage(prepared, identifier, m_sstvTxSessionId);
    emit sstvTxStateChanged();
    emit statusMessage(
        audioOnly
            ? tr("SSTV audio-only TX accepted: %1")
                  .arg(m_sstvStudioController->modeName())
            : tr("SSTV TX accepted: %1")
                  .arg(m_sstvStudioController->modeName()));
    return true;
#else
    Q_UNUSED(fskId)
    emit errorMessage(tr("This Decodium build has no native SSTV support"));
    return false;
#endif
}

