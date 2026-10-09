#pragma once
#ifndef DECODIUM_AUDIO_SINK_H
#define DECODIUM_AUDIO_SINK_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include "Audio/AudioDevice.hpp"

// DecodiumAudioSink: AudioDevice subclass that appends 16-bit PCM mono
// samples into DecodiumBridge's m_audioBuffer and emits audio health metrics.
//
// downSampleFactor: 4x path uses the same fil4 FIR low-pass shape as the
// legacy WSJT-X/Decodium3 detector before producing 12 kHz samples.
//
// sampleCallback: se impostata, viene chiamata per ogni campione decimato
// (utile per il ring buffer async FT2).
class DecodiumAudioSink : public AudioDevice
{
    Q_OBJECT

public:
    explicit DecodiumAudioSink(QVector<short>& buffer,
                               unsigned downSampleFactor = 1,
                               QObject* parent = nullptr,
                               QMutex* bufferMutex = nullptr)
        : AudioDevice(parent)
        , m_buffer(buffer)
        , m_bufferMutex(bufferMutex)
        , m_dsf(downSampleFactor < 1 ? 1 : downSampleFactor)
        , m_dsfCounter(0)
    {
    }

    void setSampleCallback(std::function<void(short)> cb) { m_sampleCallback = std::move(cb); }

    // Keep QAudioSource alive while deliberately ignoring its PCM stream.
    // This is used by the Windows TX path because suspend()/resume() can leave
    // some WASAPI USB inputs in StoppedState/IOError after PTT is released.
    // The flag is changed under the same mutex used by writeData(), so no TX
    // sample can slip into the decoder buffer after discarding has been armed.
    void setDiscardSamples(bool discard)
    {
        QMutexLocker locker(m_bufferMutex);
        if (m_discardSamples.load(std::memory_order_relaxed) == discard) {
            return;
        }
        m_discardSamples.store(discard, std::memory_order_release);
        m_dsfCounter = 0;
        m_filterDelay.fill(0.0f);
        m_pendingFullRate.clear();
    }

    void resetDecimationPhase()
    {
        if (m_bufferMutex) {
            QMutexLocker locker(m_bufferMutex);
            m_dsfCounter = 0;
            m_filterDelay.fill(0.0f);
            m_pendingFullRate.clear();
        } else {
            m_dsfCounter = 0;
            m_filterDelay.fill(0.0f);
            m_pendingFullRate.clear();
        }
    }

    qint64 readData(char* /*data*/, qint64 /*maxSize*/) override { return -1; }

    qint64 writeData(const char* data, qint64 maxSize) override
    {
        if (!data || maxSize <= 0) return 0;

        // Consume the backend data without performing conversion, buffering,
        // level metering or callbacks. QAudioSource therefore remains active,
        // while TX audio cannot reach the waterfall or any decoder path.
        if (m_discardSamples.load(std::memory_order_acquire)) return maxSize;

        const qint64 frameBytes = static_cast<qint64>(bytesPerFrame());
        const qint64 numFrames  = maxSize / frameBytes;
        if (numFrames <= 0) return maxSize;

        // De-interleave in buffer temporaneo a frequenza piena
        QVector<short> tmp(static_cast<int>(numFrames));
        store(data, static_cast<size_t>(numFrames), tmp.data());

        // Sezione critica: append su m_buffer condiviso con il main thread.
        // Il backend audio Qt6 (PulseAudio/ALSA) può invocare writeData da un
        // thread proprio anche se il sink ha parent nel main thread, e QVector
        // non è thread-safe per scritture concorrenti.
        int prevSize = 0;
        int newSamples = 0;
        double rms = 0.0;
        double peak = 0.0;
        int dynamicRange = 0;
        int clippedSamples = 0;
        QVector<short> emittedSamples;
        {
            QMutexLocker locker(m_bufferMutex);
            // setDiscardSamples() may have been waiting for this lock after
            // the fast-path check above. Recheck before appending anything.
            if (m_discardSamples.load(std::memory_order_acquire)) return maxSize;
            prevSize = m_buffer.size();
            if (m_dsf == kFil4Downsample) {
                appendFilteredFil4(tmp.constData(), static_cast<int>(numFrames));
            } else {
                for (qint64 i = 0; i < numFrames; ++i) {
                    if (m_dsfCounter == 0) {
                        short s = tmp[static_cast<int>(i)];
                        m_buffer.append(s);
                        if (m_sampleCallback) m_sampleCallback(s);
                    }
                    m_dsfCounter = (m_dsfCounter + 1) % m_dsf;
                }
            }
            newSamples = m_buffer.size() - prevSize;
            // Metriche calcolate dentro al lock perche leggono constData() del
            // buffer appena modificato; gli emit dei segnali sono fuori dal lock.
            if (newSamples > 0) {
                double sumSq = 0.0;
                const short* s = m_buffer.constData() + prevSize;
                int minSample = 32767;
                int maxSample = -32768;
                int peakAbs = 0;
                for (int i = 0; i < newSamples; ++i) {
                    const int sample = s[i];
                    const int absSample = sample < 0 ? -sample : sample;
                    const double v = sample;
                    sumSq += v * v;
                    if (sample < minSample) minSample = sample;
                    if (sample > maxSample) maxSample = sample;
                    if (absSample > peakAbs) peakAbs = absSample;
                    if (absSample >= 32700) ++clippedSamples;
                }
                rms = std::sqrt(sumSq / newSamples) / 32768.0;
                peak = static_cast<double>(peakAbs) / 32768.0;
                dynamicRange = maxSample - minSample;
                m_lastRms = rms;
                emittedSamples.reserve(newSamples);
                for (int i = 0; i < newSamples; ++i) {
                    emittedSamples.append(s[i]);
                }
            }
        }

        if (newSamples > 0) {
            emit audioLevelChanged(m_lastRms);
            emit audioHealthChanged(rms, peak, dynamicRange, clippedSamples, newSamples);
            emit audioSamplesProduced(emittedSamples);
            emit audioSamplesReady(emittedSamples);
        }

        return maxSize;
    }

signals:
    void audioLevelChanged(double level);
    void audioHealthChanged(double rms, double peak, int dynamicRange, int clippedSamples, int samples);
    // First producer-boundary signal for bounded DirectConnection consumers.
    // UI/network consumers keep using audioSamplesReady below.
    void audioSamplesProduced(QVector<short> samples);
    void audioSamplesReady(QVector<short> samples);

private:
    static constexpr int kFil4Taps {49};
    static constexpr int kFil4Downsample {4};

    short filterFil4Group(short const* input)
    {
        static constexpr std::array<float, kFil4Taps> weights {{
            0.000861074040f, 0.010051920210f, 0.010161983649f, 0.011363155076f,
            0.008706594219f, 0.002613872664f,-0.005202883094f,-0.011720748164f,
           -0.013752163325f,-0.009431602741f, 0.000539063909f, 0.012636767098f,
            0.021494659597f, 0.021951235065f, 0.011564169382f,-0.007656470131f,
           -0.028965787341f,-0.042637874109f,-0.039203309748f,-0.013153301537f,
            0.034320769178f, 0.094717832646f, 0.154224604789f, 0.197758325022f,
            0.213715139513f, 0.197758325022f, 0.154224604789f, 0.094717832646f,
            0.034320769178f,-0.013153301537f,-0.039203309748f,-0.042637874109f,
           -0.028965787341f,-0.007656470131f, 0.011564169382f, 0.021951235065f,
            0.021494659597f, 0.012636767098f, 0.000539063909f,-0.009431602741f,
           -0.013752163325f,-0.011720748164f,-0.005202883094f, 0.002613872664f,
            0.008706594219f, 0.011363155076f, 0.010161983649f, 0.010051920210f,
            0.000861074040f
        }};

        for (int i = 0; i < kFil4Taps - kFil4Downsample; ++i) {
            m_filterDelay[static_cast<size_t>(i)] =
                m_filterDelay[static_cast<size_t>(i + kFil4Downsample)];
        }
        for (int i = 0; i < kFil4Downsample; ++i) {
            m_filterDelay[static_cast<size_t>(kFil4Taps - kFil4Downsample + i)] =
                static_cast<float>(input[i]);
        }

        float sum = 0.0f;
        for (int i = 0; i < kFil4Taps; ++i) {
            sum += weights[static_cast<size_t>(i)] * m_filterDelay[static_cast<size_t>(i)];
        }
        return static_cast<short>(qBound(-32768, static_cast<int>(std::lround(sum)), 32767));
    }

    void appendOutputSample(short sample)
    {
        m_buffer.append(sample);
        if (m_sampleCallback) m_sampleCallback(sample);
    }

public:
    // Campioni da una sorgente che NON e' la scheda audio (oggi: la radio
    // remota di DecoPort), gia' alla frequenza di decodifica. Entrano nello
    // stesso buffer e fanno scattare gli stessi segnali, cosi' decoder,
    // cascata e S-meter non sanno da dove arrivi l'audio e non devono saperlo.
    //
    // Chi la usa deve avere fermato la cattura locale: due sorgenti che
    // riempiono lo stesso buffer si mescolerebbero in rumore.
    void injectExternalSamples(const QVector<short>& samples)
    {
        if (samples.isEmpty())
            return;

        double rms = 0.0;
        double peak = 0.0;
        int dynamicRange = 0;
        int clippedSamples = 0;
        {
            QMutexLocker locker(m_bufferMutex);
            if (m_discardSamples.load(std::memory_order_acquire))
                return;
            double sumSq = 0.0;
            int minSample = 32767;
            int maxSample = -32768;
            int peakAbs = 0;
            for (short sample : samples) {
                const int value = sample;
                const int absValue = value < 0 ? -value : value;
                sumSq += static_cast<double>(value) * static_cast<double>(value);
                if (value < minSample) minSample = value;
                if (value > maxSample) maxSample = value;
                if (absValue > peakAbs) peakAbs = absValue;
                if (absValue >= 32700) ++clippedSamples;
                appendOutputSample(sample);
            }
            rms = std::sqrt(sumSq / samples.size()) / 32768.0;
            peak = static_cast<double>(peakAbs) / 32768.0;
            dynamicRange = maxSample - minSample;
            m_lastRms = rms;
        }
        emit audioLevelChanged(m_lastRms);
        emit audioHealthChanged(rms, peak, dynamicRange, clippedSamples, samples.size());
        emit audioSamplesProduced(samples);
        emit audioSamplesReady(samples);
    }

private:

    void appendFilteredFil4(short const* input, int frames)
    {
        int index = 0;
        if (!m_pendingFullRate.isEmpty()) {
            while (m_pendingFullRate.size() < kFil4Downsample && index < frames) {
                m_pendingFullRate.append(input[index++]);
            }
            if (m_pendingFullRate.size() == kFil4Downsample) {
                appendOutputSample(filterFil4Group(m_pendingFullRate.constData()));
                m_pendingFullRate.clear();
            }
        }

        while (index + kFil4Downsample <= frames) {
            appendOutputSample(filterFil4Group(input + index));
            index += kFil4Downsample;
        }

        while (index < frames) {
            m_pendingFullRate.append(input[index++]);
        }
    }

    QVector<short>& m_buffer;
    QMutex*         m_bufferMutex;  // owned dal chiamante; nullptr = no locking
    unsigned        m_dsf;
    unsigned        m_dsfCounter;
    std::array<float, kFil4Taps> m_filterDelay {};
    QVector<short>  m_pendingFullRate;
    double          m_lastRms {0.0};
    std::function<void(short)> m_sampleCallback;
    std::atomic_bool m_discardSamples {false};
};

#endif // DECODIUM_AUDIO_SINK_H
