#include "DecodiumBridge.h"
#include "src/rtl/RtlSdrInput.h"
#include "src/rtl/RtlSdrDsp.h"
#include "src/rtl/RtlSdrAudioOutput.h"
#include <QAudioDevice>
#include <QDateTime>
#include <QPointer>
#include <QStandardPaths>
#include <QThread>
#include <QDebug>
#include <atomic>

extern void decodiumBridgeLogbookLog(const QString&);
extern bool decodiumBridgeResolveAudioDeviceByIdentity(QList<QAudioDevice> const&, QString const&, QString const&, QAudioDevice*, QString*);

void DecodiumBridge::startRtlSdrAudioPlayback(int sampleRate)
{
    if (m_rtlSdrAudioStopping) {
        if (m_rtlSdrAudioThread && !m_rtlSdrAudioThread->wait(0)) return;
        delete m_rtlSdrAudioThread;
        m_rtlSdrAudioThread = nullptr;
        m_rtlSdrAudioOutput = nullptr;
        m_rtlSdrAudioStopping = false;
    }
    if (sampleRate != RtlSdrDsp::kReceiverAudioSampleRate
        || QDateTime::currentMSecsSinceEpoch() < m_rtlSdrAudioRetryAfterMs) {
        return;
    }

    const QString requestedName =
        getSetting(QStringLiteral("RtlSdrAudioOutputDevice"), QString()).toString().trimmed();
    QAudioDevice output;
    QString matchReason = QStringLiteral("system-default");
    if (requestedName.isEmpty()) {
        output = cachedDefaultAudioOutput(QStringLiteral("RTL-SDR receiver audio"), false);
    } else {
        bool resolved = decodiumBridgeResolveAudioDeviceByIdentity(
            cachedAudioOutputs(QStringLiteral("RTL-SDR receiver audio"), false),
            requestedName, QString(), &output, &matchReason);
        if (!resolved) {
            output = cachedDefaultAudioOutput(QStringLiteral("RTL-SDR receiver audio fallback"), false);
            matchReason = QStringLiteral("requested output not found; system-default fallback");
        }
    }
    if (output.id().isEmpty()) {
        const QString message = QStringLiteral("RTL-SDR audio output is unavailable: refresh audio devices in Settings > Audio.");
        m_rtlSdrStatus = message;
        emit rtlSdrStatusChanged();
        emit statusMessage(message);
        m_rtlSdrAudioRetryAfterMs = QDateTime::currentMSecsSinceEpoch() + 3000;
        return;
    }

    if (m_rtlSdrAudioPlaybackRequested
        && m_rtlSdrAudioPlaybackDeviceId == output.id()
        && m_rtlSdrAudioPlaybackSampleRate == sampleRate) {
        return;
    }

    if (!m_rtlSdrAudioThread) {
        m_rtlSdrAudioThread = new QThread(this);
        m_rtlSdrAudioThread->setObjectName(QStringLiteral("RtlSdrAudioOutput"));
        m_rtlSdrAudioOutput = new RtlSdrAudioOutput;
        m_rtlSdrAudioOutput->moveToThread(m_rtlSdrAudioThread);
        connect(m_rtlSdrAudioThread, &QThread::finished,
                m_rtlSdrAudioOutput, &QObject::deleteLater);
        connect(m_rtlSdrAudioOutput, &RtlSdrAudioOutput::statusChanged,
                this, [this, outputGuard = QPointer<RtlSdrAudioOutput>(m_rtlSdrAudioOutput)](const QString& message) {
            if (!outputGuard || outputGuard.data() != m_rtlSdrAudioOutput) return;
            if (m_rtlSdrStatus != message) {
                m_rtlSdrStatus = message;
                emit rtlSdrStatusChanged();
            }
            emit statusMessage(message);
        }, Qt::QueuedConnection);
        connect(m_rtlSdrAudioOutput, &RtlSdrAudioOutput::error,
                this, [this, outputGuard = QPointer<RtlSdrAudioOutput>(m_rtlSdrAudioOutput)](const QString& message) {
            if (!outputGuard || outputGuard.data() != m_rtlSdrAudioOutput) return;
            m_rtlSdrAudioPlaybackRequested = false;
            m_rtlSdrAudioPlaybackDeviceId.clear();
            m_rtlSdrAudioPlaybackSampleRate = 0;
            m_rtlSdrAudioRetryAfterMs = QDateTime::currentMSecsSinceEpoch() + 3000;
            m_rtlSdrStatus = message;
            emit rtlSdrStatusChanged();
            emit errorMessage(message);
        }, Qt::QueuedConnection);
        m_rtlSdrAudioThread->start(QThread::HighPriority);
    }
    if (!m_rtlSdrAudioThread->isRunning()) {
        return;
    }
    m_rtlSdrAudioPlaybackRequested = true;
    m_rtlSdrAudioPlaybackDeviceId = output.id();
    m_rtlSdrAudioPlaybackSampleRate = sampleRate;
    m_rtlSdrAudioRetryAfterMs = 0;
    decodiumBridgeLogbookLog(QStringLiteral("RTL-SDR receiver audio start queued output=[%1] match=%2 thread=dedicated")
                  .arg(output.description(), matchReason));
    QPointer<RtlSdrAudioOutput> audioOutput(m_rtlSdrAudioOutput);
    QMetaObject::invokeMethod(m_rtlSdrAudioOutput,
                              [audioOutput, output, sampleRate]() {
        if (audioOutput) {
            audioOutput->start(output, sampleRate);
        }
    }, Qt::QueuedConnection);
}

void DecodiumBridge::stopRtlSdrAudioPlayback(const QString& reason)
{
    m_rtlSdrAudioPlaybackRequested = false;
    m_rtlSdrAudioPlaybackDeviceId.clear();
    m_rtlSdrAudioPlaybackSampleRate = 0;
    if (!m_rtlSdrAudioThread) {
        return;
    }
    if (m_rtlSdrAudioThread->isRunning() && !m_rtlSdrAudioStopping) {
        m_rtlSdrAudioStopping = true;
        QPointer<RtlSdrAudioOutput> audioOutput(m_rtlSdrAudioOutput);
        QMetaObject::invokeMethod(m_rtlSdrAudioOutput, [audioOutput, reason]() {
            static std::atomic_bool labDelayUsed {false};
            int const labDelay = QStandardPaths::isTestModeEnabled()
                ? qBound(0, qEnvironmentVariableIntValue("DECODIUM_LAB_SDR_STOP_DELAY_MS"), 10000)
                : 0;
            if (labDelay && !labDelayUsed.exchange(true)) QThread::msleep(labDelay);
            if (audioOutput) audioOutput->stop(reason);
            QThread::currentThread()->quit();
        }, Qt::QueuedConnection);
    }
    if (!m_rtlSdrAudioThread->wait(3000)) {
        qWarning() << "RTL-SDR audio thread did not stop; retaining it for shutdown";
        return;
    }
    delete m_rtlSdrAudioThread;
    m_rtlSdrAudioThread = nullptr;
    m_rtlSdrAudioOutput = nullptr;
    m_rtlSdrAudioStopping = false;
}

void DecodiumBridge::onRtlSdrAudioSamplesReady(const QVector<short>& samples, int sampleRate)
{
    if (samples.isEmpty()
        || sampleRate != RtlSdrDsp::kReceiverAudioSampleRate
        || !m_monitoring
        || !rtlSdrEnabled()
        || !m_rtlSdrInput
        || !m_rtlSdrInput->isRunning()
        || m_transmitting
        || m_tuning) {
        return;
    }
    const RtlSdrInput::Config config = m_rtlSdrInput->activeConfig();
    // WeakSignal has its own 12 kHz callback above. Never feed this parallel
    // 48 kHz listening path as well, or two demodulators would contend for one
    // SSTV generation.
    if (config.demodulator == RtlSdrDsp::Demodulator::WeakSignal) {
        return;
    }
    const bool audioEnabled = getSetting(QStringLiteral("RtlSdrAudioEnabled"),
                                         config.demodulator != RtlSdrDsp::Demodulator::WeakSignal).toBool();
    if (!audioEnabled) {
        return;
    }

    startRtlSdrAudioPlayback(sampleRate);
    if (!m_rtlSdrAudioPlaybackRequested || !m_rtlSdrAudioOutput) {
        return;
    }
    QPointer<RtlSdrAudioOutput> audioOutput(m_rtlSdrAudioOutput);
    QMetaObject::invokeMethod(m_rtlSdrAudioOutput,
                              [audioOutput, samples, sampleRate]() {
        if (audioOutput) {
            audioOutput->enqueueSamples(samples, sampleRate);
        }
    }, Qt::QueuedConnection);
}

