bool DecodiumBridge::resetSstvRx()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRequested || !m_sstvRxRuntime
        || sstvWavReplayActive()) {
        return false;
    }
    const bool reset = m_sstvRxRuntime->resetStream();
    if (reset) {
        refreshSstvProducerTaps();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
    }
    return reset;
#else
    return false;
#endif
}

bool DecodiumBridge::abortSstvRxFrame()
{
#if DECODIUM_HAS_SSTV
    if (!m_sstvRxRequested || !m_sstvRxRuntime
        || sstvWavReplayActive()) {
        return false;
    }
    const bool aborted = m_sstvRxRuntime->resetStream();
    if (aborted) {
        refreshSstvProducerTaps();
        emit sstvRxStateChanged();
        emit sstvRxSnapshotChanged();
        emit statusMessage(tr(
            "Current SSTV frame aborted; receiver returned to search"));
    }
    return aborted;
#else
    return false;
#endif
}

