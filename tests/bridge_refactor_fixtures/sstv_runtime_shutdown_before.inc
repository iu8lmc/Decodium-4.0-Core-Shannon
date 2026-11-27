void DecodiumBridge::shutdownSstvRuntime()
{
    m_sstvRxRequested = false;
    m_sstvOwnsMonitoring = false;
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    m_hamDrmRxRequested = false;
    m_hamDrmOwnsMonitoring = false;
#endif
    if (m_sstvDiagnosticsController) {
        m_sstvDiagnosticsController->shutdown();
    }
    if (m_sstvRxAudioJobController) {
        m_sstvRxAudioJobController->shutdown();
    }
    if (m_sstvWavReplayController) {
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayController->shutdown();
    }
    if (m_sstvStudioController) {
        m_sstvStudioController->cancelWork();
    }
    if (m_sstvShareController) {
        m_sstvShareController->shutdown();
    }
    disableSstvProducerTaps();
#if DECODIUM_HAS_HAMDRM
    if (m_hamDrmRxBackend) {
        m_hamDrmRxBackend->shutdown();
    }
    if (m_hamDrmTxBackend) {
        m_hamDrmTxBackend->shutdown();
    }
#endif
    shutdownSstvTx();
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->shutdown();
    }
    shutdownSstvStorage();
#endif
}

