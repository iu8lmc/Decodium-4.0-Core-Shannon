#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "RttyRxRecovery.h"
#include "Audio/soundout.h"
#include <QAudioSink>
#include <QDateTime>

void decodiumBridgeAudioLevelLog(const QString&);
bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend*, const QString&);
qreal decodiumBridgeTxAttenuationFromSlider(double);
qreal decodiumBridgeTxGainFromSlider(double);
#define bridgeLog decodiumBridgeAudioLevelLog
#define useLegacyRigControlFallback decodiumBridgeLegacyRigControlFallback
#define txAttenuationFromSlider decodiumBridgeTxAttenuationFromSlider
#define txGainFromSlider decodiumBridgeTxGainFromSlider

void DecodiumBridge::applyAudioInputRuntimeChange(const QString& reason)
{
    if (m_rttyRxRecovery)
        m_rttyRxRecovery->cancel(QStringLiteral("audio input changed: ") + reason);
    bool nativeSstvCapture = false;
#if DECODIUM_HAS_SSTV
    nativeSstvCapture = nativeSstvRxForcesDedicatedAudioCapture();
#endif
    if (usingLegacyBackendForRx()
        && !useModernSpectrumFeedWithLegacy()
        && !nativeSstvCapture) {
        bridgeLog(QStringLiteral("audio input runtime change handled by legacy backend (%1)").arg(reason));
        return;
    }

    if (!m_monitoring) {
        return;
    }

    if (m_transmitting || m_tuning) {
        bridgeLog(QStringLiteral("audio input runtime change deferred while TX/Tune is active (%1)").arg(reason));
        scheduleMonitorRecovery(QStringLiteral("audio input change"), m_periodTimerSessionId, true);
        return;
    }

    if (useLegacyRigControlFallback(m_legacyBackend, m_catBackend)
        && !m_soundInput
        && !m_tciAudioCaptureActive
        && !usingTciAudioInput()) {
        bridgeLog(QStringLiteral("audio input runtime change skipped; legacy fallback owns RX audio (%1)").arg(reason));
        return;
    }

    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    bridgeLog(QStringLiteral("audio input runtime change: restarting RX capture (%1)").arg(reason));
    stopAudioCapture();
    m_audioWatchdogIgnoreUntilMs = now + 5000;
    m_audioUnhealthyStartMs = 0;
    resetRxPeriodAccumulation(true);
    resetTimeSyncDecodeMetrics();
    m_spectrumBuf.clear();
    m_wfRingPos = 0;
    m_lastPanadapterData.clear();
    m_lastPanadapterFrameMs = now;
    m_lastSpectrumRecoveryMs = now;
    m_driftFrameCount = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    m_asyncDecodePending = false;
    if (m_mode == QStringLiteral("FT2")) {
        m_asyncAudioPos.store(0, std::memory_order_release);
    }
    startAudioCapture();
    emit statusMessage(QStringLiteral("Audio RX aggiornato: %1").arg(m_audioInputDevice));
}

void DecodiumBridge::applyAudioOutputRuntimeChange(const QString& reason)
{
    bridgeLog(QStringLiteral("audio output runtime change: invalidating TX output (%1)").arg(reason));
    m_cachedTxOutputDeviceValid = false;
    invalidateTxAudioCache();

    if (m_soundOutput) {
        m_soundOutput->setAttenuation(txAttenuationFromSlider(m_txOutputLevel));
    }
    if (m_txAudioSink) {
        m_txAudioSink->setVolume(static_cast<float>(txGainFromSlider(m_txOutputLevel)));
    }

    if (m_transmitting || m_tuning) {
        bridgeLog(QStringLiteral("audio output device will be used from next TX/Tune (%1)").arg(reason));
        return;
    }

    scheduleTxAudioPrecompute();
}


#undef bridgeLog
#undef useLegacyRigControlFallback
#undef txAttenuationFromSlider
#undef txGainFromSlider
