void DecodiumBridge::selectSstvRxSource(
    decodium::sstv::SstvAudioSourceKind kind,
    quint32 streamId,
    bool resetExistingStream)
{
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    const bool digitalActive = m_hamDrmRxRequested && m_hamDrmRxBackend
        && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalActive = false;
#endif
    const bool analogActive = m_sstvRxRequested && m_sstvRxRuntime;
    if (!analogActive && !digitalActive) {
        return;
    }
    if (sstvWavReplayActive()) {
        // Hardware/source telemetry may continue while a file is decoded.
        // It must not steal or relabel the immutable Replay generation.
        return;
    }
    if (kind == SstvAudioSourceKind::Unknown || streamId == 0U) {
        emit errorMessage(tr("SSTV RX: invalid Decodium audio source"));
        return;
    }

    if (!analogActive) {
#if DECODIUM_HAS_HAMDRM
        const bool changed = m_hamDrmRxSourceKind
                != static_cast<std::uint8_t>(kind)
            || m_hamDrmRxStreamId != streamId;
        if (changed || resetExistingStream) {
            // Fence the old DirectConnection producer before resetting the
            // raw HAMDRM stream.  Otherwise a callback that started before
            // the source switch can enqueue old PCM into the new generation.
            disableSstvProducerTaps();
            m_hamDrmRxSourceKind = static_cast<std::uint8_t>(kind);
            m_hamDrmRxStreamId = streamId;
            m_hamDrmRxBackend->resetAudioStream();
            refreshSstvProducerTaps();
        }
#endif
        return;
    }

    const auto current = m_sstvRxRuntime->routeToken();
    const bool sourceChangeRequested = current.source.kind != kind
        || current.source.streamId != streamId;
    // Fence before either RX implementation changes generation.  The analog
    // runtime validates its route token, while HAMDRM intentionally accepts
    // raw PCM; draining here therefore prevents an already-running old
    // producer from crossing the shared source boundary.
    const bool producerFenceRequired = sourceChangeRequested
        || resetExistingStream;
    if (producerFenceRequired) {
        disableSstvProducerTaps();
    }
    bool changed = false;
    if (sourceChangeRequested) {
        changed = m_sstvRxRuntime->switchSource(kind, streamId);
    } else if (resetExistingStream) {
        // A queued chunk may exist before the worker has published either an
        // active rate or a processed-chunk counter.  A real capture restart is
        // still a generation boundary and must never reuse that pending PCM.
        changed = m_sstvRxRuntime->resetStream();
    }
    const auto selected = m_sstvRxRuntime->routeToken();
#if DECODIUM_HAS_HAMDRM
    bool digitalChanged = false;
    if (digitalActive && selected.valid()) {
        digitalChanged = m_hamDrmRxSourceKind
                != static_cast<std::uint8_t>(selected.source.kind)
            || m_hamDrmRxStreamId != selected.source.streamId
            || resetExistingStream;
        if (digitalChanged) {
            m_hamDrmRxSourceKind = static_cast<std::uint8_t>(
                selected.source.kind);
            m_hamDrmRxStreamId = selected.source.streamId;
            m_hamDrmRxBackend->resetAudioStream();
        }
    }
#else
    constexpr bool digitalChanged = false;
#endif
    bool selectedTapMissing = false;
    switch (selected.source.kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::WebSdr:
        selectedTapMissing = m_audioSink && !m_sstvLocalAudioTap;
        break;
    case SstvAudioSourceKind::LegacyBackend:
        selectedTapMissing = m_legacyBackend && !m_sstvLegacyAudioTap;
        break;
    case SstvAudioSourceKind::Tci:
        selectedTapMissing = m_hamlibCat && !m_sstvTciAudioTap;
        break;
    case SstvAudioSourceKind::DecoPort:
        selectedTapMissing = remoteLinkIfAny() && !m_sstvDecoPortAudioTap;
        break;
    case SstvAudioSourceKind::RtlSdr:
        selectedTapMissing = m_rtlSdrInput
            && !m_sstvRtlPcmTap && !m_sstvRtlAudioTap;
        break;
    case SstvAudioSourceKind::Replay:
    case SstvAudioSourceKind::Unknown:
        break;
    }
    // The callback closure, rather than the callback body, owns the route
    // token. Rebind at a generation boundary, or once when a selected producer
    // has just been constructed lazily. Repeated DecoPort state telemetry must
    // not churn an otherwise stable audio tap.
    if (changed || digitalChanged || producerFenceRequired
        || selectedTapMissing) {
        refreshSstvProducerTaps();
    }
    if (changed) {
        emit sstvRxStateChanged();
        emit sstvRxSourceDeviceChanged();
        emit sstvRxSnapshotChanged();
    }
#else
    Q_UNUSED(kind)
    Q_UNUSED(streamId)
    Q_UNUSED(resetExistingStream)
#endif
}
