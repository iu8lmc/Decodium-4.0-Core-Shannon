#include "DecodiumBridge.h"
#include "Audio/soundin.h"
#include "DecodiumAudioSink.h"
#include "src/rtl/RtlSdrInput.h"
#include <QDebug>
#include <QThread>
#include <cstdlib>
#if defined(Q_OS_MACOS)
#include <execinfo.h>
#endif

void decodiumBridgeAudioLevelLog(const QString&);

void DecodiumBridge::stopAudioCapture()
{
#if defined(Q_OS_MACOS)
    if (qEnvironmentVariableIntValue("DECODIUM_LAB_AUDIO_LIFECYCLE") == 1) {
        void* frames[20];
        const int count = backtrace(frames, 20);
        char** symbols = backtrace_symbols(frames, count);
        if (symbols) {
            for (int i = 0; i < count; ++i) qInfo() << "[LAB][AUDIO] stop caller" << symbols[i];
            std::free(symbols);
        }
    }
#endif
    // Cancel a pending watchdog fallback before any deliberate stop (mode or
    // device change) can replace the capture configuration.
    ++m_audioWatchdogRecoverySerial;
    // The same rule applies to a deferred mode-change reopen.  Once a caller
    // explicitly stops capture, an older CW/FT hand-off must never bring it
    // back behind that caller's back.
    ++m_modeChangeAudioRecoverySerial;
    m_lastAudioCaptureStartKey.clear();
    m_lastAudioCaptureStartMs = 0;
    if (m_rtlSdrInput && m_rtlSdrInput->isActive()) {
        stopRtlSdrCapture();
        m_rxAudioSuspendedForTx = false;
        m_audioUnhealthyStartMs = 0;
        m_audioWatchdogIgnoreUntilMs = 0;
        return;
    }
    if (m_tciAudioCaptureActive) {
        stopTciAudioCapture();
        return;
    }

    if (m_soundInput) {
        m_soundInput->stop();
    }
    if (m_audioSink) {
        m_audioSink->setDiscardSamples(false);
        m_audioSink->resetDecimationPhase();
    }
    m_rxAudioSuspendedForTx = false;
    m_rxAudioKeptOpenForTx = false;
    m_rxAudioIoRecoveryQueued = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;
}

void DecodiumBridge::teardownAudioCapture()
{
    m_lastAudioCaptureStartKey.clear();
    m_lastAudioCaptureStartMs = 0;
    stopRtlSdrCapture();
    if (m_soundInput) {
        decodiumBridgeAudioLevelLog(QStringLiteral("teardownAudioCapture: stopping and destroying SoundInput"));
        SoundInput *input = m_soundInput;
        m_soundInput = nullptr;
        QObject::disconnect(input, nullptr, this, nullptr);
        if (input->thread()
            && input->thread()->isRunning()
            && input->thread() != QThread::currentThread()) {
            QMetaObject::invokeMethod(input, &SoundInput::stop, Qt::BlockingQueuedConnection);
            input->deleteLater();
        } else {
            input->stop();
            delete input;
        }
    }
    m_rxAudioSuspendedForTx = false;
    m_rxAudioKeptOpenForTx = false;
    m_rxAudioIoRecoveryQueued = false;
    m_audioUnhealthyStartMs = 0;
    m_audioWatchdogIgnoreUntilMs = 0;

    if (m_audioSink) {
        decodiumBridgeAudioLevelLog(QStringLiteral("teardownAudioCapture: destroying audio sink"));
        QObject::disconnect(m_audioSink, nullptr, this, nullptr);
        m_audioSink->setSampleCallback({});
        if (m_audioSink->isOpen()) {
            m_audioSink->close();
        }
        delete m_audioSink;
        m_audioSink = nullptr;
    }
}

