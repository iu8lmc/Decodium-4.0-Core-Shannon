void DecodiumBridge::onDecoPortRemoteState()
{
    if (!m_decoPortUseRemote)
        return;
    auto* link = remoteLink();
    if (!link || !link->isLinked())
        return;

#if DECODIUM_HAS_SSTV
    // A reconnect to another gateway (or a restarted gateway on the same
    // address) carries a different protocol stream id. Select it before any
    // same-event audio is accepted by the SSTV bounded tap.
    selectSstvRxSource(decodium::sstv::SstvAudioSourceKind::DecoPort,
                       currentSstvAudioStreamId(
                           decodium::sstv::SstvAudioSourceKind::DecoPort));
#endif

    // Lo stato del CAT dell'applicazione e' quello della radio remota: se cade
    // il collegamento, si vede subito dove si guarda sempre.
    bool const linkedNow = link->isLinked();
    if (m_catConnected != linkedNow) {
        m_catConnected = linkedNow;
        emit catConnectedChanged();
    }
    QString const remoteMode = link->modeName();
    if (!remoteMode.isEmpty() && m_catMode != remoteMode) {
        m_catMode = remoteMode;
        emit catModeChanged();
    }
    QString const remoteName = link->rigLabel();
    if (!remoteName.isEmpty() && m_catRigName != remoteName) {
        m_catRigName = remoteName;
        emit catRigNameChanged();
    }

    const auto telemetry=link->state();
    updateRigTelemetry((telemetry.mask & decoport::FieldForwardPower) ? telemetry.forwardPowerW() : 0.,
                       (telemetry.mask & decoport::FieldSwr) ? telemetry.swr() : 0.,
                       telemetry.alcPct(), link->ptt() && (telemetry.mask & decoport::FieldAlc));
    double const f = link->frequencyHz();
    if (f <= 0.0 || std::abs(f - m_frequency) <= 1.0)
        return;

    m_decoPortApplyingRemote = true;
    setFrequency(f);
    m_decoPortApplyingRemote = false;
    if (m_bandManager)
        m_bandManager->updateFromFrequency(f);
}

