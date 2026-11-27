#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "AudioModeChangeCapturePolicy.h"
#include "RttyRxRecovery.h"
#include "DecodiumAudioSink.h"
#include "Audio/soundin.h"
#include "src/rtl/RtlSdrInput.h"
#include <QDateTime>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);
float decodiumBridgeRxInputGainFromLevel(double);
bool decodiumBridgeIsNativePcmKeyboardMode(QString const&);
bool decodiumBridgeIsFt2LinkApplicationMode(QString);

void DecodiumBridge::restartAudioCaptureForModeChange(const QString& previousMode)
{
    if (!m_monitoring || m_transmitting || m_tuning) {
        return;
    }

    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    bool const targetUsesLegacyBackend = usingLegacyBackendForRx();
    decodium::audio::ModeChangeCaptureState captureState;
    captureState.hasLocalQtCapture = m_soundInput != nullptr;
    captureState.tciCaptureActive = m_tciAudioCaptureActive;
    captureState.rtlSdrCaptureActive = m_rtlSdrInput && m_rtlSdrInput->isActive();
    captureState.targetUsesLegacyBackend = targetUsesLegacyBackend;
    captureState.targetKeepsDedicatedModernCapture =
        useDedicatedModernAudioCaptureWithLegacy()
        || nativeSstvRxForcesDedicatedAudioCapture();
    bool const keepExistingQtCapture =
        decodium::audio::preserveExistingCaptureForModeChange(captureState);

    if (keepExistingQtCapture) {
        // A prior mode change may have queued a real replacement.  Keeping the
        // current stream is itself a newer ownership decision, so invalidate
        // that old reopen before it can touch this healthy capture.
        ++m_modeChangeAudioRecoverySerial;
        decodiumBridgeAudioLevelLog(QStringLiteral("setMode: preserving active RX capture for mode change %1 -> %2 "
                                 "(same bridge-owned source; no QtMultimedia reopen)")
                      .arg(previousMode, m_mode));
    } else {
        decodiumBridgeAudioLevelLog(QStringLiteral("setMode: restarting RX capture for mode change %1 -> %2")
                      .arg(previousMode, m_mode));
        stopAudioCapture();
    }
    // A real native-PCM hand-off needs additional settling time before the
    // audio watchdog intervenes. Reusing the existing modern source does not.
    m_audioWatchdogIgnoreUntilMs = now
        + (!keepExistingQtCapture && decodiumBridgeIsNativePcmKeyboardMode(previousMode) ? 15000 : 5000);
    m_audioUnhealthyStartMs = 0;
    resetRxPeriodAccumulation(true);
    resetTimeSyncDecodeMetrics();
    m_spectrumBuf.clear();
    m_wfRingPos = 0;
    m_lastPanadapterData.clear();
    m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch();
    m_lastSpectrumRecoveryMs = 0;
    m_driftFrameCount = 0;
    m_driftExpectedFrames = 0;
    m_driftClock.restart();
    m_asyncDecodePending = false;
    if (m_mode == QStringLiteral("FT2")) {
        m_asyncAudioPos.store(0, std::memory_order_release);
    }

    if (keepExistingQtCapture) {
        if (m_soundInput) {
            m_soundInput->setInputGain(decodiumBridgeRxInputGainFromLevel(m_rxInputLevel));
        }
    } else {
        // An exclusive legacy target has to own the input device itself.  Do
        // not reopen the bridge stream after that deliberate hand-off.
        if (targetUsesLegacyBackend
            && !captureState.targetKeepsDedicatedModernCapture) {
            decodiumBridgeAudioLevelLog(QStringLiteral("setMode: local RX capture released to the legacy backend"));
            return;
        }

        // SoundInput lives on its own thread.  A real source replacement (for
        // example TCI -> local card) must wait for the old backend callbacks
        // to drain.  Ordinary CW -> FT mode changes never enter this branch:
        // they keep the same QAudioSource and avoid the Windows/WASAPI race.
        const quint64 recoverySerial = ++m_modeChangeAudioRecoverySerial;
        QTimer::singleShot(0, this, [this, recoverySerial]() {
            if (recoverySerial != m_modeChangeAudioRecoverySerial
                || !m_monitoring || m_transmitting || m_tuning
                || usingLegacyBackendForRx()) {
                decodiumBridgeAudioLevelLog(QStringLiteral("setMode: stale audio reopen skipped serial=%1 current=%2")
                              .arg(recoverySerial).arg(m_modeChangeAudioRecoverySerial));
                return;
            }
            startAudioCapture();
        });
    }
}

void DecodiumBridge::scheduleMonitorRecovery(const QString& reason,
                                             quint64 sessionId,
                                             bool monitorShouldStayActive)
{
    if (!monitorShouldStayActive) {
        return;
    }

    constexpr int kRetryDelaysMs[] = {350, 1000, 2500, 5000, 10000};
    for (int delayMs : kRetryDelaysMs) {
        QTimer::singleShot(delayMs, this, [this, reason, sessionId, delayMs]() {
            if (sessionId != m_periodTimerSessionId || !m_monitorRequested) {
                return;
            }
            if (m_transmitting || m_tuning) {
                if (delayMs == 10000) {
                    decodiumBridgeAudioLevelLog(QStringLiteral("%1: monitor recovery deferred while TX/Tune is active")
                                  .arg(reason));
                }
                return;
            }

            if (!m_monitoring) {
                decodiumBridgeAudioLevelLog(QStringLiteral("%1: monitor dropped, rearming RX").arg(reason));
                startRx();
                return;
            }

            if (usingLegacyBackendForRx()
                && legacyBackendAvailable()
                && !m_legacyBackend->monitoring()) {
                decodiumBridgeAudioLevelLog(QStringLiteral("%1: legacy monitor dropped, rearming backend").arg(reason));
                m_legacyBackend->setMonitoring(true);
                syncLegacyBackendState();
            }
        });
    }
}

void DecodiumBridge::scheduleModeChangeMonitorRecovery(const QString& previousMode,
                                                       const QString& requestedMode,
                                                       quint64 sessionId,
                                                       bool monitorWasActive)
{
    QString const reason = QStringLiteral("setMode %1 -> %2").arg(previousMode, requestedMode);
    scheduleMonitorRecovery(reason, sessionId, monitorWasActive);
}


void DecodiumBridge::restartAudioCaptureFromWatchdog(const QString& reason)
{
    if (!m_monitoring || m_transmitting || m_tuning) {
        return;
    }

    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kFt2LinkPostTxAckGuardMs = 15000;
    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode)
        && (m_ft2LinkTxActive
            || (m_soundInput
                && m_ft2LinkPostTxAckGuardUntilMs > 0
                && now < m_ft2LinkPostTxAckGuardUntilMs)
            || (m_lastTxEndMs > 0
                && now - m_lastTxEndMs < kFt2LinkPostTxAckGuardMs))) {
        decodiumBridgeAudioLevelLog(QStringLiteral(
            "Audio watchdog restart skipped: FT2-Link post-TX ACK guard reason=%1 sinceTxEnd=%2ms guardLeft=%3ms")
                      .arg(reason)
                      .arg(m_lastTxEndMs > 0 ? now - m_lastTxEndMs : -1)
                      .arg(qMax<qint64>(0, m_ft2LinkPostTxAckGuardUntilMs - now)));
        m_audioWatchdogIgnoreUntilMs = qMax(m_audioWatchdogIgnoreUntilMs,
                                            now + kFt2LinkPostTxAckGuardMs);
        return;
    }

    // 1.0.537 iu8lmc - l'audio TCI non e' una scheda audio: chiudere e riaprire
    // lo stream a pochi millisecondi di distanza non lo recupera, lo spegne.
    // Il registro mostrava audio_stop e audio_start a 4 ms, poi dodici secondi
    // di silenzio, poi di nuovo, all'infinito. Qui si limita la frequenza dei
    // riavvii senza disabilitare il recupero: si salta solo se i campioni sono
    // arrivati da poco o se si e' gia' riavviato da poco.
    if (m_tciAudioCaptureActive) {
        constexpr qint64 kTciAudioAliveMs = 12000;
        constexpr qint64 kTciRestartSettleMs = 15000;
        qint64 const sinceSamples = m_lastAudioHealthMs > 0 ? now - m_lastAudioHealthMs : -1;
        qint64 const sinceRestart = m_lastAudioWatchdogRestartMs > 0
            ? now - m_lastAudioWatchdogRestartMs : -1;
        bool const streamAlive = sinceSamples >= 0 && sinceSamples < kTciAudioAliveMs;
        bool const restartTooSoon = sinceRestart >= 0 && sinceRestart < kTciRestartSettleMs;
        if (streamAlive || restartTooSoon) {
            decodiumBridgeAudioLevelLog(QStringLiteral(
                "Audio watchdog: riavvio TCI saltato (%1) sinceSamples=%2ms sinceRestart=%3ms")
                          .arg(reason).arg(sinceSamples).arg(sinceRestart));
            m_audioWatchdogIgnoreUntilMs = qMax(m_audioWatchdogIgnoreUntilMs, now + 5000);
            m_audioUnhealthyStartMs = 0;
            return;
        }
    }

    if (rtlSdrEnabled()) {
        decodiumBridgeAudioLevelLog(QStringLiteral("RTL-SDR watchdog: restarting asynchronous receiver (%1)").arg(reason));
        stopRtlSdrCapture();
        startRtlSdrCapture();
        return;
    }

    if (usingLegacyBackendForRx()
        && !useModernSpectrumFeedWithLegacy()
        && !nativeSstvRxForcesDedicatedAudioCapture()) {
        rearmLegacyPcmSpectrumFeed(QStringLiteral("watchdog %1").arg(reason));
        return;
    }

    if (usingLegacyBackendForRx()
        && useModernSpectrumFeedWithLegacy()
        && !useDedicatedModernAudioCaptureWithLegacy()
        && !nativeSstvRxForcesDedicatedAudioCapture()) {
        decodiumBridgeAudioLevelLog(QStringLiteral("Audio watchdog: rearming legacy PCM tap for Direct Visual (%1)").arg(reason));
        m_legacyPcmSpectrumFeed = true;
        m_lastLegacyPcmSampleMs = 0;
        if (m_soundInput || m_tciAudioCaptureActive) {
            stopAudioCapture();
        }
        if (m_legacyBackend) {
            syncLegacyBackendTxState();
            m_legacyBackend->setMonitoring(true);
            m_legacyBackend->rearmMonitoring(QStringLiteral("direct visual watchdog %1").arg(reason));
        }
        m_audioUnhealthyStartMs = 0;
        m_audioWatchdogIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 5000;
        return;
    }

    m_lastAudioWatchdogRestartMs = now;
    m_lastAudioWatchdogLogMs = now;
    m_audioUnhealthyStartMs = 0;

    decodiumBridgeAudioLevelLog(QStringLiteral("Audio watchdog: restarting RX capture (%1)").arg(reason));
    emit statusMessage(QStringLiteral("Audio RX riavviato: watchdog audio"));
    m_audioWatchdogIgnoreUntilMs = now + 5000;
    resetRxPeriodAccumulation(true);
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

    // On Linux, SoundInput retires a terminal PipeWire source and reopens it
    // after its event queue has settled. Other platforms retain their
    // established watchdog escalation path unchanged.
    if (m_soundInput && !m_tciAudioCaptureActive) {
        quint64 const recoverySerial = ++m_audioWatchdogRecoverySerial;
        startAudioCapture(true);
        QTimer::singleShot(3500, this, [this, recoverySerial, now, reason]() {
            if (recoverySerial != m_audioWatchdogRecoverySerial
                || !m_monitoring
                || m_transmitting
                || m_tuning
                || !m_soundInput
                || m_tciAudioCaptureActive) {
                return;
            }
            if (m_lastAudioHealthMs >= now) {
                decodiumBridgeAudioLevelLog(QStringLiteral("Audio watchdog: non-destructive recovery received PCM callback (%1)")
                              .arg(reason));
                return;
            }

#if defined(Q_OS_LINUX)
            decodiumBridgeAudioLevelLog(QStringLiteral("Audio watchdog: recovery produced no PCM callback; awaiting the next safe retry (%1)")
                          .arg(reason));
#else
            decodiumBridgeAudioLevelLog(QStringLiteral("Audio watchdog: safe recovery produced no PCM callback; escalating to full reopen (%1)")
                          .arg(reason));
            stopAudioCapture();
            startAudioCapture();
#endif
        });
        return;
    }

    stopAudioCapture();
    startAudioCapture();
}


void DecodiumBridge::scheduleRttyExitRxRecovery()
{
    if (!m_rttyRxRecovery) {
        using Recovery = decodium::audio::RttyRxRecovery;
        m_rttyRxRecovery = new Recovery({
            [this]() {
                Recovery::State state;
                state.requested = m_monitorRequested;
                state.monitoring = m_monitoring;
                state.busy = m_transmitting || m_tuning || m_decoPortRemoteKeyed;
                state.pcmStamp = m_lastAudioHealthMs;
                state.details = QStringLiteral("rms=%1 peak=%2 catPtt=%3 mode=%4 catMode=%5 rxLevel=%6 rxGain=%7 autoRx=%8 channel=%9")
                    .arg(m_lastAudioHealthRms).arg(m_lastAudioHealthPeak)
                    .arg(activeCatReportsPttActive() ? 1 : 0).arg(m_mode).arg(m_catMode)
                    .arg(m_rxInputLevel).arg(decodiumBridgeRxInputGainFromLevel(m_rxInputLevel))
                    .arg(m_autoRxInputLevel ? 1 : 0).arg(m_audioInputChannel);
                if (m_mode == QStringLiteral("RTTY"))
                    state.cancelReason = QStringLiteral("returned to RTTY");
                else if (m_decoPortUseRemote || m_tciAudioCaptureActive || usingTciAudioInput() || rtlSdrEnabled())
                    state.cancelReason = QStringLiteral("RX source is not a local sound card");
                return state;
            },
            [this]() { startRx(); },
            [this](bool retry) {
                m_rxAudioSuspendedForTx = false;
                m_rxAudioKeptOpenForTx = false;
                if (m_audioSink) m_audioSink->setDiscardSamples(false);
                if (retry || (usingLegacyBackendForRx() && !useDedicatedModernAudioCaptureWithLegacy()))
                    restartAudioCaptureFromWatchdog(QStringLiteral("RTTY exit: verify RX PCM flow"));
                else
                    startAudioCapture(true);
            },
            [](const QString& message) { decodiumBridgeAudioLevelLog(message); }
        }, this);
    }
    m_rttyRxRecovery->start();
}

#define bridgeLog decodiumBridgeAudioLevelLog
void DecodiumBridge::refreshAudioDevices()
{
    refreshAudioDevicesInternal(QStringLiteral("manual refreshAudioDevices"),
                                false,
                                true);
}

bool DecodiumBridge::restoreAudioDefaults()
{
    if (m_transmitting || m_tuning) {
        const QString message = QStringLiteral("Audio recovery postponed while the radio is transmitting or tuning.");
        emit statusMessage(message);
        return false;
    }

    // Force a fresh enumeration: after sleep/wake or a USB/Pulse route change
    // the cached QAudioDevice objects can describe the old route.  Selecting
    // the current system defaults also clears stale device identities through
    // the normal setters, without deleting any other audio preferences.
    refreshAudioDeviceCache(QStringLiteral("safe audio recovery"), false, true);
    const QString input = m_cachedDefaultAudioInput.description().trimmed();
    const QString output = m_cachedDefaultAudioOutput.description().trimmed();
    if (input.isEmpty() && output.isEmpty()) {
        const QString message = QStringLiteral("Audio recovery could not find a system input or output device.");
        bridgeLog(message);
        emit errorMessage(message);
        return false;
    }

    if (!input.isEmpty()) {
        setAudioInputDevice(input);
    }
    if (!output.isEmpty()) {
        setAudioOutputDevice(output);
    }
    setAudioInputChannel(0);
    setAudioOutputChannel(0);
    refreshAudioDevicesInternal(QStringLiteral("safe audio recovery rebind"), false, false);

    const QString message = QStringLiteral("Audio restored to the current system defaults.");
    bridgeLog(message + QStringLiteral(" input=[%1] output=[%2]").arg(input, output));
    emit statusMessage(message);
    return true;
}

#undef bridgeLog
