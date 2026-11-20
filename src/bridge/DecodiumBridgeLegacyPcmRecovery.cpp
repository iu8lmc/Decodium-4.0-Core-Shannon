#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include <QDateTime>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);

void DecodiumBridge::applyDirectVisualAudioCaptureMode(const QString& reason)
{
    bool const directVisual = useModernSpectrumFeedWithLegacy();
    applyLowCpuRuntimeProfile(QStringLiteral("direct-visual-%1").arg(reason));

    if (!usingLegacyBackendForTx()) {
        m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch() - 1000;
        if (m_spectrumTimer && m_monitoring && !m_spectrumTimer->isActive()) {
            m_spectrumTimer->start();
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("[PANDBG] Direct Visual %1 source=modern_audio interval_ms=%2 legacy_decoder=off reason=%3")
                      .arg(directVisual ? QStringLiteral("active") : QStringLiteral("inactive"))
                      .arg(m_spectrumTimer ? m_spectrumTimer->interval() : effectiveSpectrumTimerIntervalMs())
                      .arg(reason));
        emit statusMessage(directVisual
                           ? QStringLiteral("Direct Visual attivo: panadapter moderno veloce")
                           : QStringLiteral("Direct Visual disattivo: panadapter moderno prudente"));
        return;
    }

    if (!legacyBackendAvailable()) {
        decodiumBridgeAudioLevelLog(QStringLiteral("Direct Visual %1 (%2): legacy backend unavailable")
                      .arg(directVisual ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                           reason));
        return;
    }

    bool const monitorActive = m_monitoring
        || m_monitorRequested
        || (m_legacyBackend && m_legacyBackend->monitoring());
    if (!monitorActive) {
        m_legacyPcmSpectrumFeed = !directVisual;
        decodiumBridgeAudioLevelLog(QStringLiteral("Direct Visual %1 (%2): will apply on next RX start")
                      .arg(directVisual ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                           reason));
        return;
    }

    if (m_transmitting || m_tuning) {
        decodiumBridgeAudioLevelLog(QStringLiteral("Direct Visual %1 (%2): deferred while TX/Tune is active")
                      .arg(directVisual ? QStringLiteral("enabled") : QStringLiteral("disabled"),
                           reason));
        scheduleMonitorRecovery(QStringLiteral("direct visual setting"), m_periodTimerSessionId, true);
        return;
    }

    if (m_legacyBackend) {
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    updatePeriodTicksMax();
    resetRxPeriodAccumulation(directVisual);
    m_spectrumBuf.clear();
    m_wfRingPos = 0;
    m_lastPanadapterData.clear();
    m_lastPanadapterFrameMs = nowMs - 1000;
    m_lastSpectrumRecoveryMs = 0;
    m_driftFrameCount = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = nowMs + 5000;

    if (directVisual) {
        bool const dedicatedModernCapture = useDedicatedModernAudioCaptureWithLegacy();
        m_legacyPcmSpectrumFeed = !dedicatedModernCapture;
        if (dedicatedModernCapture) {
            startAudioCapture();
        } else {
            stopAudioCapture();
            m_lastLegacyPcmSampleMs = 0;
        }
        if (m_spectrumTimer && !m_spectrumTimer->isActive()) {
            m_spectrumTimer->start();
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("[PANDBG] Direct Visual active source=%1 interval_ms=%2 legacy_decoder=on")
                      .arg(dedicatedModernCapture
                           ? QStringLiteral("modern_qaudio_dual_capture")
                           : QStringLiteral("legacy_pcm_tap_single_capture"))
                      .arg(m_spectrumTimer ? m_spectrumTimer->interval() : effectiveSpectrumTimerIntervalMs()));
        emit statusMessage(dedicatedModernCapture
                           ? QStringLiteral("Direct Visual attivo: panadapter da cattura audio dedicata")
                           : QStringLiteral("Direct Visual attivo: panadapter dal tap PCM legacy"));
    } else {
        m_legacyPcmSpectrumFeed = true;
        rearmLegacyPcmSpectrumFeed(QStringLiteral("direct visual disabled: %1").arg(reason));
        if (m_spectrumTimer && !m_spectrumTimer->isActive()) {
            m_spectrumTimer->start();
        }
        decodiumBridgeAudioLevelLog(QStringLiteral("[PANDBG] Direct Visual inactive source=legacy_pcm_tap interval_ms=125 legacy_decoder=on"));
        emit statusMessage(QStringLiteral("Direct Visual disattivo: panadapter da tap PCM legacy"));
    }
}

void DecodiumBridge::rearmLegacyPcmSpectrumFeed(const QString& reason)
{
    if (!usingLegacyBackendForRx()) {
        return;
    }
    // Native SSTV/HAMDRM owns the dedicated local capture while active.  A
    // delayed legacy-spectrum retry must not stop that capture or reactivate
    // the legacy decoder behind the active receiver's back.
    if (nativeSstvRxForcesDedicatedAudioCapture()) {
        return;
    }
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode) && !useModernSpectrumFeedWithLegacy()) {
        m_legacyPcmSpectrumFeed = false;
        return;
    }
    if (useModernSpectrumFeedWithLegacy() && useDedicatedModernAudioCaptureWithLegacy()) {
        return;
    }

    decodiumBridgeAudioLevelLog(QStringLiteral("legacy PCM panadapter rearm: %1").arg(reason));
    m_legacyPcmSpectrumFeed = true;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 5000;

    if (m_soundInput || m_tciAudioCaptureActive) {
        decodiumBridgeAudioLevelLog(QStringLiteral("legacy PCM panadapter: closing stray modern audio capture"));
        stopAudioCapture();
    }

    if (m_legacyBackend) {
        syncLegacyBackendTxState();
        m_legacyBackend->setMonitoring(true);
        m_legacyBackend->rearmMonitoring(reason);
    }
    if (m_spectrumTimer && !m_spectrumTimer->isActive()) {
        m_spectrumTimer->start();
    }
    scheduleLegacyStateRefreshBurst();
}

void DecodiumBridge::scheduleLegacyPcmSpectrumRearm(const QString& reason)
{
    if (!usingLegacyBackendForRx() || useModernSpectrumFeedWithLegacy()) {
        return;
    }

    qint64 const baselineSampleMs = m_lastLegacyPcmSampleMs;
    static constexpr int kRetryDelaysMs[] = {250, 900, 1800, 3500};
    for (int const delayMs : kRetryDelaysMs) {
        QTimer::singleShot(delayMs, this, [this, reason, delayMs, baselineSampleMs]() {
            if (!m_monitorRequested || !usingLegacyBackendForRx() || useModernSpectrumFeedWithLegacy()) {
                return;
            }
            if (m_transmitting || m_tuning) {
                return;
            }

            qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_lastLegacyPcmSampleMs > baselineSampleMs
                && nowMs - m_lastLegacyPcmSampleMs < 1500) {
                return;
            }

            rearmLegacyPcmSpectrumFeed(
                QStringLiteral("%1 retry %2ms").arg(reason).arg(delayMs));
        });
    }
}

