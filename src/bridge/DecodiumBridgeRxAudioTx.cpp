#include "DecodiumBridge.h"
#include "Audio/soundin.h"
#include "DecodiumAudioSink.h"
#include <QDateTime>
#include <QMutexLocker>
#include <QTimer>

void decodiumBridgeAudioLevelLog(const QString&);
#define bridgeLog decodiumBridgeAudioLevelLog

void DecodiumBridge::pauseRxAudioForTx(const QString& context)
{
    if (!m_soundInput) {
        return;
    }

#if defined(Q_OS_WIN)
    if (m_audioSink) {
        // Do not suspend a WASAPI USB input around every TX. Qt can fail to
        // resume otherwise healthy USB Audio CODEC endpoints with IOError.
        // Keep the endpoint active and discard its PCM before it reaches any
        // buffer, meter, waterfall, recording or decoder callback.
        m_audioSink->setDiscardSamples(true);
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
        m_rxAudioKeptOpenForTx = true;
        bridgeLog(QStringLiteral("SoundInput kept active with RX samples discarded during %1 (Windows WASAPI)")
                      .arg(context));
        return;
    }
#endif

    m_soundInput->suspend();
    m_rxAudioSuspendedForTx = true;
    bridgeLog(QStringLiteral("SoundInput suspended during %1").arg(context));
}

void DecodiumBridge::resumeRxAudioAfterTx(const QString& reason)
{
    const bool wasSuspended = m_rxAudioSuspendedForTx;
    const bool wasKeptOpen = m_rxAudioKeptOpenForTx;
    int bufferedBeforeResume = 0;
    {
        QMutexLocker locker(&m_audioBufferMutex);
        bufferedBeforeResume = m_audioBuffer.size();
    }
    m_rxAudioSuspendedForTx = false;
    m_rxAudioKeptOpenForTx = false;
    // 1.0.166 — marca timestamp fine TX per hold-off NTP rearm
    m_lastTxEndMs = QDateTime::currentMSecsSinceEpoch();
    const bool legacyPcmTapSingleCapture =
        usingLegacyBackendForRx()
        && useModernSpectrumFeedWithLegacy()
        && !useDedicatedModernAudioCaptureWithLegacy();
    bridgeLog(QStringLiteral("RX resume after TX: reason=%1 wasSuspended=%2 keptOpen=%3 monitor=%4 monitorRequested=%5 soundInput=%6 tci=%7 bufferedBefore=%8 legacyTap=%9")
                  .arg(reason)
                  .arg(wasSuspended ? 1 : 0)
                  .arg(wasKeptOpen ? 1 : 0)
                  .arg(m_monitoring ? 1 : 0)
                  .arg(m_monitorRequested ? 1 : 0)
                  .arg(m_soundInput ? QStringLiteral("alive") : QStringLiteral("null"))
                  .arg(m_tciAudioCaptureActive ? 1 : 0)
                  .arg(bufferedBeforeResume)
                  .arg(legacyPcmTapSingleCapture ? 1 : 0));

    if (wasKeptOpen) {
        // Keep discarding until the old RX/TX transition samples have been
        // removed, then reopen the sink path without touching QAudioSource.
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }
        if (m_audioSink) {
            m_audioSink->setDiscardSamples(false);
            bridgeLog(QStringLiteral("SoundInput remained active; RX sample flow restored after TX %1")
                          .arg(reason));
        } else {
            bridgeLog(QStringLiteral("Audio sink missing after kept-open TX %1; restarting capture")
                          .arg(reason));
            restartAudioCaptureFromWatchdog(QStringLiteral("post-TX audio sink missing (%1)").arg(reason));
            return;
        }
    }

    if (!m_monitoring
        && !(m_monitorRequested && usingLegacyBackendForRx())) {
        if (wasSuspended) {
            bridgeLog(QStringLiteral("SoundInput resume skipped after TX %1: monitor off").arg(reason));
        }
        return;
    }

    if (usingLegacyBackendForRx()
        && !useModernSpectrumFeedWithLegacy()
        && !nativeSstvRxForcesDedicatedAudioCapture()) {
        rearmLegacyPcmSpectrumFeed(QStringLiteral("post-TX %1").arg(reason));
        scheduleLegacyPcmSpectrumRearm(QStringLiteral("post-TX %1").arg(reason));
        return;
    }

    if (legacyPcmTapSingleCapture) {
        rearmLegacyPcmSpectrumFeed(QStringLiteral("post-TX direct visual %1").arg(reason));
    }

    if (wasSuspended) {
        {
            QMutexLocker locker(&m_audioBufferMutex);
            m_audioBuffer.clear();
            m_audioBuffer.reserve(m_periodTicksMax * TIMER_MS * SAMPLE_RATE / 1000);
        }

        if (m_soundInput) {
            m_soundInput->resume();
            bridgeLog("SoundInput resumed after TX " + reason);
        } else {
            bridgeLog("SoundInput missing after TX " + reason + ", restarting capture");
            restartAudioCaptureFromWatchdog(QStringLiteral("post-TX sound input missing (%1)").arg(reason));
            return;
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const quint64 resumeSerial = ++m_postTxRxResumeSerial;
    const qint64 resumeStartMs = nowMs;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = qMax(m_audioWatchdogIgnoreUntilMs,
#if defined(Q_OS_WIN)
                                        nowMs + 1200
#else
                                        nowMs + 2500
#endif
                                        );

    if (usingTciAudioInput()) {
        if (!m_tciAudioCaptureActive) {
            bridgeLog(QStringLiteral("post-TX TCI RX capture missing after %1, restarting capture")
                          .arg(reason));
            restartAudioCaptureFromWatchdog(QStringLiteral("post-TX TCI RX capture missing (%1)").arg(reason));
        }
        return;
    }

    auto scheduleResumeProbe = [this, reason, resumeSerial, resumeStartMs](int delayMs) {
        QTimer::singleShot(delayMs, this, [this, reason, resumeSerial, resumeStartMs, delayMs]() {
            if (resumeSerial != m_postTxRxResumeSerial
                || (!m_monitoring && !m_monitorRequested)
                || m_transmitting
                || m_tuning
                || m_rxAudioSuspendedForTx) {
                return;
            }

            const bool legacyPcmTapSingleCapture =
                usingLegacyBackendForRx()
                && useModernSpectrumFeedWithLegacy()
                && !useDedicatedModernAudioCaptureWithLegacy();
            if (legacyPcmTapSingleCapture) {
                if (m_lastLegacyPcmSampleMs >= resumeStartMs) {
                    return;
                }

                bridgeLog(QStringLiteral("post-TX RX legacy PCM watchdog: no samples after %1 delay=%2ms, rearming tap")
                              .arg(reason)
                              .arg(delayMs));
                rearmLegacyPcmSpectrumFeed(QStringLiteral("post-TX legacy PCM did not resume (%1)").arg(reason));
                return;
            }

            if (m_lastAudioHealthMs >= resumeStartMs) {
                return;
            }

            int bufferedSamples = 0;
            {
                QMutexLocker locker(&m_audioBufferMutex);
                bufferedSamples = m_audioBuffer.size();
            }

            if (bufferedSamples <= 0) {
                ++m_postTxRxResumeSerial;
                bridgeLog(QStringLiteral("post-TX RX audio watchdog: no samples after %1 delay=%2ms, restarting capture")
                              .arg(reason)
                              .arg(delayMs));
                restartAudioCaptureFromWatchdog(QStringLiteral("post-TX RX audio did not resume (%1)").arg(reason));
            }
        });
    };

#if defined(Q_OS_WIN)
    scheduleResumeProbe(450);
    scheduleResumeProbe(1200);
#else
    if (legacyPcmTapSingleCapture) {
        scheduleResumeProbe(350);
        scheduleResumeProbe(1200);
    } else {
        scheduleResumeProbe(1800);
    }
#endif
}

#undef bridgeLog
