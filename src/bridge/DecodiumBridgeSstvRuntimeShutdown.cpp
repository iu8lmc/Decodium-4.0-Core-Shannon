#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#include "src/sstv/integration/SstvRxAudioJobController.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvStudioController.h"
#include "src/sstv/integration/SstvWavReplayController.h"
#include "src/sstv/models/SstvShareController.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#endif
#endif

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

