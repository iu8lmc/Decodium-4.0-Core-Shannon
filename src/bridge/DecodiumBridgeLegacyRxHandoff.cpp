#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "PanadapterItem.hpp"
#include <QDateTime>
#include <QMutexLocker>
#include <utility>

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::migrateActiveMonitoringToLegacyBackend()
{
    if (!legacyTxBackendRequested() || !legacyBackendAvailable() || !m_monitoring) {
        return;
    }

    decodiumBridgeAudioLevelLog(QStringLiteral("Migrating active monitor session to legacy backend"));
    // Results still in flight belong to the preceding native RX session.
    invalidateDecodeSession();
    resetNativeDecodeDedupIndex();
    if (m_decoding) {
        m_decoding = false;
        emit decodingChanged();
    }
    ++m_periodTimerSessionId;

    if (m_periodTimer) {
        m_periodTimer->stop();
    }
    if (m_spectrumTimer) {
        m_spectrumTimer->stop();
    }
    if (m_asyncDecodeTimer) {
        m_asyncDecodeTimer->stop();
    }
    m_asyncDecodePending = false;

    stopAudioCapture();

    if (m_legacyBackend) {
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
    }

    if (useModernSpectrumFeedWithLegacy()) {
        bool const dedicatedModernCapture = useDedicatedModernAudioCaptureWithLegacy();
        m_legacyPcmSpectrumFeed = !dedicatedModernCapture;
        updatePeriodTicksMax();
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
        m_spectrumBuf.clear();
        m_periodTicks = 0;
        m_lastPeriodSlot = -1;   // re-anchor UTC slot at next tick
        m_wfRingPos = 0;
        m_lastPanadapterData.clear();
        m_driftFrameCount = 0;
        m_driftExpectedFrames = 0;
        m_driftClock.restart();
        if (dedicatedModernCapture) {
            startAudioCapture();
        } else {
            stopAudioCapture();
            m_lastLegacyPcmSampleMs = 0;
        }
        if (m_spectrumTimer) {
            m_spectrumTimer->start();
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("Legacy monitor migration keeps modern FFT panadapter feed active source=%1")
                      .arg(dedicatedModernCapture
                           ? QStringLiteral("dedicated_modern_qaudio")
                           : QStringLiteral("legacy_pcm_tap_single_capture")));
    } else {
        resetRxPeriodAccumulation(false);
        m_spectrumBuf.clear();
        m_wfRingPos = 0;
        m_lastPanadapterData.clear();
        m_legacyPcmSpectrumFeed = true;
        if (m_spectrumTimer) {
            m_spectrumTimer->start();
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("Legacy monitor migration uses legacy PCM tap for fast QML panadapter"));
    }

    // A CW/JTTY/SSB startup leaves the legacy state timer dormant. Starting
    // its audio monitor alone makes PCM/decodes live but leaves the QML meters
    // frozen on the preceding native level. Arm state refresh for both paths.
    scheduleLegacyStateRefreshBurst();
}

void DecodiumBridge::recoverGpuPanadapterAfterSourceHandoff(const QString& reason)
{
#if defined(DECODIUM_QML_PANADAPTER_DIRECT)
    m_panadapterItems.removeAll(QPointer<PanadapterItem>(nullptr));
    int targets = 0;
    for (const QPointer<PanadapterItem>& ref : std::as_const(m_panadapterItems)) {
        if (PanadapterItem* item = ref.data()) {
            item->recoverGpuDirectVisualAfterSourceHandoff();
            ++targets;
        }
    }
    decodiumBridgeAudioLevelLog(QStringLiteral("[PANDBG] Native PCM to legacy RX hand-off: GPU direct visual recovery queued "
                             "reason=%1 targets=%2 fallback=deferred")
                  .arg(reason)
                  .arg(targets));
#else
    Q_UNUSED(reason)
#endif
}

void DecodiumBridge::completeNativePcmToLegacyRxHandoff(const QString& previousMode)
{
    if (!m_monitoring || m_transmitting || m_tuning || !usingLegacyBackendForRx()
        || !legacyBackendAvailable()) {
        return;
    }

    QString const reason = QStringLiteral("mode hand-off %1 -> %2")
        .arg(previousMode, m_mode);
    bool const keepDedicatedModernCapture = useDedicatedModernAudioCaptureWithLegacy();

    if (keepDedicatedModernCapture) {
        // A deliberately configured modern capture must not be stopped and
        // reopened merely because the legacy decoder takes over FT8/FT4.  It
        // still needs the legacy monitor to start, and its visual source must
        // remain bridge-owned rather than the legacy PCM tap.
        if (m_periodTimer) {
            m_periodTimer->stop();
        }
        if (m_asyncDecodeTimer) {
            m_asyncDecodeTimer->stop();
        }
        m_asyncDecodePending = false;
        if (m_decoding) {
            m_decoding = false;
            emit decodingChanged();
        }
        syncLegacyBackendState();
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
        syncLegacyBackendState();
        m_legacyPcmSpectrumFeed = false;
        resetRxPeriodAccumulation(true);
        m_spectrumBuf.clear();
        m_wfRingPos = 0;
        m_lastPanadapterData.clear();
        m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch() - 1000;
        m_lastSpectrumRecoveryMs = 0;
        if (m_spectrumTimer && !m_spectrumTimer->isActive()) {
            m_spectrumTimer->start();
        }
        scheduleLegacyStateRefreshBurst();
        decodiumBridgeAudioLevelLog(QStringLiteral("Native PCM to legacy RX hand-off keeps dedicated modern capture: %1")
                      .arg(reason));
    } else {
        // This is the normal FT8/FT4 path.  The legacy backend becomes the
        // only owner of the sound device and its PCM tap becomes the source
        // for the GPU panadapter.  Calling only stopAudioCapture() leaves the
        // tap disabled from CW/JTTY/SSB and freezes the last texture forever.
        migrateActiveMonitoringToLegacyBackend();
        decodiumBridgeAudioLevelLog(QStringLiteral("Native PCM to legacy RX hand-off completed via legacy PCM tap: %1")
                      .arg(reason));
    }

    // The next PCM frame is deliberately rendered through fresh RHI textures.
    // This is a GPU recovery, not a request to fall back to FFTW/CPU.
    recoverGpuPanadapterAfterSourceHandoff(reason);
}

