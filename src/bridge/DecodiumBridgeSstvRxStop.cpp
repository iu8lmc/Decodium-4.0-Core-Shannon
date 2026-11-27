#include "DecodiumBridge.h"

#if DECODIUM_HAS_SSTV
#include "DecodiumLegacyBackend.h"
#include "src/sstv/integration/SstvRxRuntime.h"
#include "src/sstv/integration/SstvWavReplayController.h"
#if DECODIUM_HAS_HAMDRM
#include "src/sstv/digital/waveform/HamDrmWaveformAdapters.h"
#endif
#endif

void DecodiumBridge::stopSstvRx()
{
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    m_hamDrmOwnsSstvRx = false;
#endif
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    const bool digitalContinues = m_hamDrmRxRequested
        && m_hamDrmRxBackend && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalContinues = false;
#endif
    const bool stopDedicatedCapture = m_sstvOwnsDedicatedAudioCapture;
    const bool stopOwnedMonitor = m_sstvOwnsMonitoring
        && !digitalContinues && (m_monitoring || m_monitorRequested);
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    if (m_sstvOwnsMonitoring && digitalContinues) {
        m_hamDrmOwnsMonitoring = true;
    }
#endif
    m_sstvRxRequested = false;
    m_sstvOwnsMonitoring = false;
    m_sstvOwnsDedicatedAudioCapture = false;
    const bool restoreLegacyMonitoring = m_sstvRestoresLegacyMonitoring;
    m_sstvRestoresLegacyMonitoring = false;
#if DECODIUM_HAS_SSTV
    if (sstvWavReplayActive()) {
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayController->cancel();
    }
    disableSstvProducerTaps();
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->stop();
    }
    if (digitalContinues) {
        refreshSstvProducerTaps();
    }
#endif
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();

    if (stopOwnedMonitor) {
        setMonitoring(false);
    } else if (stopDedicatedCapture) {
        // Return FT8/FT2 to its original single legacy PCM source after the
        // native SSTV session releases its dedicated capture.
        stopAudioCapture();
        if (usingLegacyBackendForRx() && m_legacyBackend) {
            rearmLegacyPcmSpectrumFeed(
                QStringLiteral("SSTV RX stopped"));
        }
    }
    if (restoreLegacyMonitoring && m_legacyBackend) {
        if (!digitalContinues) {
            // SSTV temporarily stopped the legacy monitor and took ownership
            // of the audio endpoint.  Merely setting the backend flag back to
            // true leaves the spectrum/period timers and PCM feed stopped,
            // which makes the dashboard panadapter appear frozen until the
            // user toggles Monitor OFF/ON manually.  Re-enter the normal RX
            // start path so audio capture, timers and visual feed are rebuilt
            // as one coherent generation.
            m_monitorRequested = false;
            if (m_monitoring) {
                m_monitoring = false;
                emit monitoringChanged();
            }
            startRx();
        } else {
            // HAMDRM may still own the shared monitor.  Do not tear down its
            // active generation; only restore the legacy backend mirror.
            m_monitorRequested = true;
            m_legacyBackend->setMonitoring(true);
            syncLegacyBackendState();
        }
    }
    emit statusMessage(tr("SSTV RX stopped"));
#endif
}

void DecodiumBridge::stopSstvRxForMonitorStop()
{
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    m_hamDrmOwnsSstvRx = false;
#endif
#if DECODIUM_HAS_SSTV && DECODIUM_HAS_HAMDRM
    const bool digitalActive = m_hamDrmRxRequested && m_hamDrmRxBackend
        && m_hamDrmRxBackend->active();
#else
    constexpr bool digitalActive = false;
#endif
    if (!m_sstvRxRequested && !digitalActive) {
        return;
    }
    m_sstvRxRequested = false;
    m_sstvOwnsMonitoring = false;
    m_sstvOwnsDedicatedAudioCapture = false;
    m_sstvRestoresLegacyMonitoring = false;
#if DECODIUM_HAS_SSTV
#if DECODIUM_HAS_HAMDRM
    if (digitalActive) {
        m_hamDrmRxRequested = false;
        m_hamDrmOwnsMonitoring = false;
        m_hamDrmRxBackend->failAudio(
            decodium::sstv::hamdrm::HamDrmStatus::failure(
                decodium::sstv::hamdrm::HamDrmErrorCode::IoFailure,
                "Decodium monitoring stopped during HAMDRM RX"));
    }
#endif
    if (sstvWavReplayActive()) {
        m_sstvWavReplayRestoreLive = false;
        m_sstvWavReplayController->cancel();
    }
    disableSstvProducerTaps();
    if (m_sstvRxRuntime) {
        m_sstvRxRuntime->stop();
    }
#endif
    emit sstvRxStateChanged();
    emit sstvRxSnapshotChanged();
#endif
}
