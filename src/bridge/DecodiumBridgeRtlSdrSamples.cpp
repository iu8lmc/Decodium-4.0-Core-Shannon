#include "DecodiumBridge.h"
#include "src/rtl/RtlSdrInput.h"
#include "src/rtl/RtlSdrDsp.h"
#include "src/rtl/RtlSdrRfSpectrum.h"
#include <QDateTime>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <QDebug>
#include <algorithm>
#include <cmath>

extern void decodiumBridgeLogbookLog(const QString&);
extern bool decodiumBridgeIsFt2LinkApplicationMode(QString);

void DecodiumBridge::onRtlSdrPcmSamplesReady(const QVector<short>& samples)
{
    if (samples.isEmpty()
        || !m_monitoring
        || !rtlSdrEnabled()
        || !m_rtlSdrInput
        || !m_rtlSdrInput->isRunning()
        || m_rtlSdrInput->activeConfig().demodulator != RtlSdrDsp::Demodulator::WeakSignal
        || m_transmitting
        || m_tuning) {
        return;
    }

    double sumSq = 0.0;
    int minSample = 32767;
    int maxSample = -32768;
    int peakAbs = 0;
    int clippedSamples = 0;
    for (short sample : samples) {
        const int value = sample;
        const int absSample = value < 0 ? -value : value;
        sumSq += static_cast<double>(value) * static_cast<double>(value);
        if (value < minSample) minSample = value;
        if (value > maxSample) maxSample = value;
        if (absSample > peakAbs) peakAbs = absSample;
        if (absSample >= 32700) ++clippedSamples;
    }

    {
        QMutexLocker locker(&m_audioBufferMutex);
        m_audioBuffer += samples;
    }

    for (short sample : samples) {
        uint64_t writePosition = m_asyncAudioPos.load(std::memory_order_acquire);
        m_asyncAudio[writePosition % ASYNC_BUF_SIZE] = sample;
        m_asyncAudioPos.compare_exchange_strong(writePosition, writePosition + 1,
                                                std::memory_order_release,
                                                std::memory_order_relaxed);
        if (m_wavManager) {
            m_wavManager->feedSample(sample);
        }
    }
    m_driftExpectedFrames.fetch_add(samples.size(), std::memory_order_relaxed);

    const double rms = std::sqrt(sumSq / samples.size()) / 32768.0;
    const double peak = static_cast<double>(peakAbs) / 32768.0;
    const int dynamicRange = maxSample - minSample;
    m_audioLevel = rms;
    emit audioLevelChanged();
    handleAudioHealth(rms, peak, dynamicRange, clippedSamples, samples.size());

    if (decodiumBridgeIsFt2LinkApplicationMode(m_mode) && m_monitoring
        && !m_transmitting && !m_tuning) {
        emit ft2LinkRxSamplesReady(samples,
                                   static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    }

    static qint64 lastLogMs = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - lastLogMs >= 5000) {
        lastLogMs = nowMs;
        int buffered = 0;
        {
            QMutexLocker locker(&m_audioBufferMutex);
            buffered = m_audioBuffer.size();
        }
        decodiumBridgeLogbookLog(QStringLiteral("RTL-SDR PCM: chunk=%1 buffered=%2 rms=%3 peak=%4 range=%5 clipped=%6")
                      .arg(samples.size())
                      .arg(buffered)
                      .arg(rms, 0, 'g', 4)
                      .arg(peak, 0, 'g', 4)
                      .arg(dynamicRange)
                      .arg(clippedSamples));
    }
}

void DecodiumBridge::onRtlSdrIqSamplesReady(const QVector<short>& samples,
                                            int sampleRate,
                                            quint32 centerFrequencyHz)
{
    if (samples.size() < RtlSdrRfSpectrum::kFftSize * 2
        || !m_monitoring
        || !rtlSdrEnabled()
        || !m_rtlSdrInput
        || !m_rtlSdrInput->isRunning()
        || m_transmitting
        || m_tuning
        || sampleRate <= 0
        || centerFrequencyHz == 0) {
        return;
    }

    if (m_rtlSdrRfSampleRate != sampleRate) {
        m_rtlSdrRfSampleRate = sampleRate;
        emit rtlSdrRfSampleRateChanged();
        emit rtlSdrRfViewChanged();
    }

    if (m_rtlSdrIqRing.size() != RTL_IQ_RING_SHORTS) {
        m_rtlSdrIqRing.fill(0, RTL_IQ_RING_SHORTS);
        m_rtlSdrIqWritePos = 0;
        m_rtlSdrIqCount = 0;
    }
    const int copyCount = qMin(samples.size(), RTL_IQ_RING_SHORTS);
    const short *source = samples.constData() + (samples.size() - copyCount);
    const int firstCount = qMin(copyCount, RTL_IQ_RING_SHORTS - m_rtlSdrIqWritePos);
    std::copy_n(source, firstCount, m_rtlSdrIqRing.begin() + m_rtlSdrIqWritePos);
    const int secondCount = copyCount - firstCount;
    if (secondCount > 0) {
        std::copy_n(source + firstCount, secondCount, m_rtlSdrIqRing.begin());
    }
    m_rtlSdrIqWritePos = (m_rtlSdrIqWritePos + copyCount) % RTL_IQ_RING_SHORTS;
    m_rtlSdrIqCount = qMin(RTL_IQ_RING_SHORTS, m_rtlSdrIqCount + copyCount);
}


void DecodiumBridge::processRtlSdrSpectrum()
{
    if (!rtlSdrRfView() || m_rtlSdrIqCount < RtlSdrRfSpectrum::kFftSize * 2
        || m_rtlSdrIqRing.size() != RTL_IQ_RING_SHORTS) {
        return;
    }
    bool expectedIdle = false;
    if (!m_rtlSdrSpectrumBusy.compare_exchange_strong(expectedIdle, true)) {
        return;
    }

    QVector<short> iq(RtlSdrRfSpectrum::kFftSize * 2);
    int source = (m_rtlSdrIqWritePos - iq.size() + RTL_IQ_RING_SHORTS) % RTL_IQ_RING_SHORTS;
    for (int index = 0; index < iq.size(); ++index) {
        iq[index] = m_rtlSdrIqRing[(source + index) % RTL_IQ_RING_SHORTS];
    }
    const int sampleRate = m_rtlSdrRfSampleRate;
    const quint32 centreFrequency = m_rtlSdrRfCenterFrequencyHz;
    const uint64_t serial = m_rtlSdrSpectrumSerial.fetch_add(1) + 1;
    m_lastPanadapterFrameMs = QDateTime::currentMSecsSinceEpoch();

    QPointer<DecodiumBridge> guard(this);
    auto *task = QRunnable::create([guard, iq = std::move(iq), sampleRate,
                                    centreFrequency, serial]() mutable {
        RtlSdrRfSpectrum::Frame frame = RtlSdrRfSpectrum::compute(iq, sampleRate, centreFrequency);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, frame = std::move(frame), serial]() mutable {
            if (!guard) {
                return;
            }
            guard->m_rtlSdrSpectrumBusy.store(false);
            if (serial != guard->m_rtlSdrSpectrumSerial.load()
                || !guard->rtlSdrRfView()
                || frame.values.isEmpty()) {
                return;
            }
            if (guard->m_lastPanadapterData.size() == frame.values.size()) {
                for (int index = 0; index < frame.values.size(); ++index) {
                    frame.values[index] = 0.85f * frame.values[index]
                        + 0.15f * guard->m_lastPanadapterData[index];
                }
            }
            guard->m_lastPanadapterData = frame.values;
            guard->m_lastPanMinDb = frame.minDb;
            guard->m_lastPanMaxDb = frame.maxDb;
            guard->m_lastPanFreqMin = frame.frequencyMinHz;
            guard->m_lastPanFreqMax = frame.frequencyMaxHz;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (nowMs - guard->m_lastRtlSdrRfLogMs >= 5000) {
                guard->m_lastRtlSdrRfLogMs = nowMs;
                const auto peakIt = std::max_element(frame.values.cbegin(),
                                                     frame.values.cend());
                const int peakIndex = static_cast<int>(std::distance(frame.values.cbegin(),
                                                                      peakIt));
                const float peakDb = peakIt != frame.values.cend() ? *peakIt : -160.0f;
                const double binWidth = frame.values.size() > 1
                    ? (frame.frequencyMaxHz - frame.frequencyMinHz)
                        / static_cast<double>(frame.values.size() - 1)
                    : 0.0;
                const double peakFrequency = frame.frequencyMinHz + peakIndex * binWidth;
                const int selectedIndex = binWidth > 0.0
                    ? qBound(0,
                             qRound((guard->m_rtlSdrRfSelectedFrequencyHz
                                     - frame.frequencyMinHz) / binWidth),
                             frame.values.size() - 1)
                    : 0;
                const float selectedDb = frame.values.value(selectedIndex, -160.0f);
                qInfo().noquote()
                    << "[RTL-RF]"
                    << "bins=" << frame.values.size()
                    << "rate=" << guard->m_rtlSdrRfSampleRate
                    << "span_hz=" << QString::number(frame.frequencyMinHz, 'f', 0)
                                     + QStringLiteral("..")
                                     + QString::number(frame.frequencyMaxHz, 'f', 0)
                    << "selected_hz=" << guard->m_rtlSdrRfSelectedFrequencyHz
                    << "selected_db=" << QString::number(selectedDb, 'f', 1)
                    << "peak_hz=" << QString::number(peakFrequency, 'f', 0)
                    << "peak_db=" << QString::number(peakDb, 'f', 1)
                    << "display_db=" << QString::number(frame.minDb, 'f', 1)
                                       + QStringLiteral("..")
                                       + QString::number(frame.maxDb, 'f', 1);
            }
            guard->publishRemoteWaterfallFrame(frame.values, frame.minDb, frame.maxDb,
                                                frame.frequencyMinHz, frame.frequencyMaxHz);
            emit guard->panadapterDataReady(frame.values, frame.minDb, frame.maxDb,
                                             frame.frequencyMinHz, frame.frequencyMaxHz);
        }, Qt::QueuedConnection);
    });
    QThreadPool::globalInstance()->start(task, -1);
}

