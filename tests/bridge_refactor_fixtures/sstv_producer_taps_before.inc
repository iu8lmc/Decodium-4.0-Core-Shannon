void DecodiumBridge::disconnectSstvProducerTaps()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    auto disconnectTap = [](QMetaObject::Connection& connection) {
        if (connection) {
            QObject::disconnect(connection);
            connection = {};
        }
    };
    disconnectTap(m_sstvLocalAudioTap);
    disconnectTap(m_sstvLegacyAudioTap);
    disconnectTap(m_sstvTciAudioTap);
    disconnectTap(m_sstvDecoPortAudioTap);
    disconnectTap(m_sstvRtlPcmTap);
    disconnectTap(m_sstvRtlAudioTap);
#endif
}

void DecodiumBridge::disableSstvProducerTaps()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    disconnectSstvProducerTaps();
    if (m_sstvAudioRelay) {
        m_sstvAudioRelay->disableAndDrain();
    }
#endif
}

#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
decodium::sstv::hamdrm::HamDrmStatus
DecodiumBridge::activateHamDrmRxTap()
{
    Q_ASSERT(QThread::currentThread() == thread());
    using decodium::sstv::hamdrm::HamDrmErrorCode;
    using decodium::sstv::hamdrm::HamDrmStatus;
    if (!m_hamDrmRxBackend || !m_sstvAudioRelay
        || m_shuttingDown || QCoreApplication::closingDown()) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "Decodium shared RX audio is unavailable");
    }
    if (sstvWavReplayActive()) {
        return HamDrmStatus::failure(
            HamDrmErrorCode::UnsupportedFeature,
            "HAMDRM live RX cannot replace an active SSTV WAV replay");
    }
    if (m_hamDrmRxRequested) {
        return HamDrmStatus::success();
    }

    // In legacy FT modes the raw legacy tap can remain silent after TX. Reuse
    // the native SSTV capture arbitration, including its existing restoration
    // of legacy monitoring, when no analog SSTV receiver already owns it.
    const bool needsNativeLegacyCapture = !m_sstvRxRequested
        && usingLegacyBackendForRx() && !m_decoPortUseRemote
        && !rtlSdrEnabled() && !usingTciAudioInput();
    if (needsNativeLegacyCapture) {
        if (!startSstvRx()) {
            return HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                         "HAMDRM could not start the shared native SSTV capture");
        }
        m_hamDrmOwnsSstvRx = true;
    }
    const SstvAudioSourceKind kind = currentSstvAudioSourceKind();
    const quint32 streamId = currentSstvAudioStreamId(kind);
    if (kind == SstvAudioSourceKind::Unknown || streamId == 0U) {
        if (m_hamDrmOwnsSstvRx) {
            m_hamDrmOwnsSstvRx = false;
            stopSstvRx();
        }
        return HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "no Decodium RX audio source is available for HAMDRM");
    }
    const bool monitoringWasAlreadyRequested =
        m_monitoring || m_monitorRequested;
    m_hamDrmRxRequested = true;
    m_hamDrmOwnsMonitoring = !monitoringWasAlreadyRequested;
    m_hamDrmRxSourceKind = static_cast<std::uint8_t>(kind);
    m_hamDrmRxStreamId = streamId;
    refreshSstvProducerTaps();
    if (!m_monitoring) {
        setMonitoring(true);
    }
    if (!m_monitoring) {
        m_hamDrmRxRequested = false;
        m_hamDrmOwnsMonitoring = false;
        m_hamDrmRxSourceKind = static_cast<std::uint8_t>(
            SstvAudioSourceKind::Unknown);
        m_hamDrmRxStreamId = 0U;
        refreshSstvProducerTaps();
        if (m_hamDrmOwnsSstvRx) {
            m_hamDrmOwnsSstvRx = false;
            stopSstvRx();
        }
        return HamDrmStatus::failure(
            HamDrmErrorCode::IoFailure,
            "HAMDRM RX requires an active Decodium monitor");
    }
    const SstvAudioSourceKind selected = currentSstvAudioSourceKind();
    m_hamDrmRxSourceKind = static_cast<std::uint8_t>(selected);
    m_hamDrmRxStreamId = currentSstvAudioStreamId(selected);
    refreshSstvProducerTaps();
    emit statusMessage(tr("HAMDRM RX attached to the Decodium audio monitor"));
    return HamDrmStatus::success();
}

void DecodiumBridge::deactivateHamDrmRxTap()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_hamDrmRxRequested) {
        return;
    }
    const bool transferMonitoring = m_hamDrmOwnsMonitoring
        && m_sstvRxRequested;
    const bool stopOwnedMonitor = m_hamDrmOwnsMonitoring
        && !m_sstvRxRequested && (m_monitoring || m_monitorRequested);
    const bool stopOwnedSstvRx = m_hamDrmOwnsSstvRx && m_sstvRxRequested;
    m_hamDrmOwnsSstvRx = false;
    if (transferMonitoring) {
        m_sstvOwnsMonitoring = true;
    }
    m_hamDrmRxRequested = false;
    m_hamDrmOwnsMonitoring = false;
    m_hamDrmRxSourceKind = static_cast<std::uint8_t>(
        SstvAudioSourceKind::Unknown);
    m_hamDrmRxStreamId = 0U;
    disableSstvProducerTaps();
    refreshSstvProducerTaps();
    if (stopOwnedMonitor) {
        setMonitoring(false);
    }
    if (stopOwnedSstvRx) {
        stopSstvRx();
    }
    emit statusMessage(tr("HAMDRM RX detached from the Decodium audio monitor"));
}
#endif

void DecodiumBridge::refreshSstvProducerTaps()
{
#if DECODIUM_HAS_SSTV
    Q_ASSERT(QThread::currentThread() == thread());
    // A tap rebind is a real producer-generation boundary, not merely a Qt
    // signal-list update.  A DirectConnection callback may already be inside
    // DecodiumSstvAudioRelay when QObject::disconnect() returns.  Drain that
    // callback before publishing a new route/backend: analog ingress rejects
    // stale route tokens, but the separate HAMDRM PCM backend deliberately
    // receives raw samples and has no analog-route token to reject.
    disableSstvProducerTaps();
    const bool analogActive = m_sstvRxRequested && m_sstvRxRuntime
        && m_sstvRxRuntime->state() == SstvRxRuntime::State::Running;
#if DECODIUM_HAS_HAMDRM
    const bool digitalActive = m_hamDrmRxRequested && m_hamDrmRxBackend
        && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalActive = false;
#endif
    if ((!analogActive && !digitalActive) || !m_sstvAudioRelay) {
        return;
    }

    decodium::sstv::SstvRxRouteToken token;
    SstvAudioSourceKind selectedKind = SstvAudioSourceKind::Unknown;
    quint32 selectedStreamId = 0U;
    if (analogActive) {
        token = m_sstvRxRuntime->routeToken();
        if (!token.valid()) {
            return;
        }
        selectedKind = token.source.kind;
        selectedStreamId = token.source.streamId;
#if DECODIUM_HAS_HAMDRM
    } else {
        selectedKind = static_cast<SstvAudioSourceKind>(
            m_hamDrmRxSourceKind);
        selectedStreamId = m_hamDrmRxStreamId;
        if (selectedKind == SstvAudioSourceKind::Unknown
            || selectedStreamId == 0U) {
            return;
        }
#endif
    }
    if (selectedKind == SstvAudioSourceKind::Replay
        || sstvWavReplayActive()) {
        // Replay is the sole producer for this generation. Keep every live
        // DirectConnection detached and close the relay gate as an additional
        // guard against a callback already captured by a previous source.
        m_sstvAudioRelay->disableAndDrain();
        return;
    }
    const std::shared_ptr<DecodiumSstvAudioRelay> relay = m_sstvAudioRelay;
#if DECODIUM_HAS_HAMDRM
    relay->enable(analogActive ? m_sstvRxRuntime.get() : nullptr,
                  digitalActive ? m_hamDrmRxBackend : nullptr);
#else
    relay->enable(analogActive ? m_sstvRxRuntime.get() : nullptr);
#endif

    switch (selectedKind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr:
        if (m_audioSink) {
            m_sstvLocalAudioTap = connect(
                m_audioSink, &DecodiumAudioSink::audioSamplesProducedWithSourceTime,
                this, [relay, token](QVector<short> samples, qint64 sourceTimeNs) {
                    relay->submit(std::move(samples), SAMPLE_RATE, token, sourceTimeNs);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::LegacyBackend:
        if (m_legacyBackend) {
            m_sstvLegacyAudioTap = connect(
                m_legacyBackend, &DecodiumLegacyBackend::audioSamplesProduced,
                this, [relay, token](const QByteArray& pcmBytes) {
                    const int sampleCount = pcmBytes.size()
                        / static_cast<int>(sizeof(qint16));
                    if (sampleCount <= 0) {
                        return;
                    }
                    QVector<short> samples;
                    samples.reserve(sampleCount);
                    const auto* bytes = reinterpret_cast<const uchar*>(
                        pcmBytes.constData());
                    for (int index = 0; index < sampleCount; ++index) {
                        samples.append(qFromLittleEndian<qint16>(
                            bytes + index * static_cast<int>(sizeof(qint16))));
                    }
                    relay->submit(std::move(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::Tci:
        if (m_hamlibCat) {
            m_sstvTciAudioTap = connect(
                m_hamlibCat,
                &DecodiumTransceiverManager::tciPcmSamplesProduced,
                this, [relay, token](const QVector<short>& samples) {
                    relay->submit(QVector<short>(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::DecoPort:
        if (auto* remote = remoteLinkIfAny()) {
            m_sstvDecoPortAudioTap = connect(
                remote, &RemoteRadioLink::rxAudioProduced,
                this, [relay, token, selectedStreamId](const QVector<short>& samples,
                                      quint64, quint32 streamId) {
                    if (streamId != selectedStreamId) {
                        return;
                    }
                    relay->submit(QVector<short>(samples), SAMPLE_RATE, token);
                }, Qt::DirectConnection);
        }
        break;
    case SstvAudioSourceKind::RtlSdr:
        if (m_rtlSdrInput) {
            if (rtlSdrGeneralReceiverConfigured()) {
                m_sstvRtlAudioTap = connect(
                    m_rtlSdrInput, &RtlSdrInput::audioSamplesProduced,
                    this, [relay, token](QVector<short> samples,
                                          int sampleRate) {
                        relay->submit(std::move(samples), sampleRate, token);
                    }, Qt::DirectConnection);
            } else {
                m_sstvRtlPcmTap = connect(
                    m_rtlSdrInput, &RtlSdrInput::pcmSamplesProduced,
                    this, [relay, token](QVector<short> samples) {
                        relay->submit(std::move(samples), SAMPLE_RATE, token);
                    }, Qt::DirectConnection);
            }
        }
        break;
    case SstvAudioSourceKind::Replay:
    case SstvAudioSourceKind::Unknown:
        break;
    }
#endif
}
