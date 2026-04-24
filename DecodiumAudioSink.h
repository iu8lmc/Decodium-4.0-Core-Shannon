#pragma once
#ifndef DECODIUM_AUDIO_SINK_H
#define DECODIUM_AUDIO_SINK_H

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
// downSampleFactor: campioni da scartare per ogni campione tenuto.
// Es. dsf=4: QAudioSource cattura a 48000 Hz, sink scrive a 12000 Hz.
// Decimazione semplice (1:dsf) — sufficiente per FT8/FT2.
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

    qint64 readData(char* /*data*/, qint64 /*maxSize*/) override { return -1; }

    qint64 writeData(const char* data, qint64 maxSize) override
    {
        if (!data || maxSize <= 0) return 0;

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
        {
            QMutexLocker locker(m_bufferMutex);
            prevSize = m_buffer.size();
            for (qint64 i = 0; i < numFrames; ++i) {
                if (m_dsfCounter == 0) {
                    short s = tmp[static_cast<int>(i)];
                    m_buffer.append(s);
                    if (m_sampleCallback) m_sampleCallback(s);
                }
                m_dsfCounter = (m_dsfCounter + 1) % m_dsf;
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
            }
        }

        if (newSamples > 0) {
            emit audioLevelChanged(m_lastRms);
            emit audioHealthChanged(rms, peak, dynamicRange, clippedSamples, newSamples);
        }

        return maxSize;
    }

signals:
    void audioLevelChanged(double level);
    void audioHealthChanged(double rms, double peak, int dynamicRange, int clippedSamples, int samples);

private:
    QVector<short>& m_buffer;
    QMutex*         m_bufferMutex;  // owned dal chiamante; nullptr = no locking
    unsigned        m_dsf;
    unsigned        m_dsfCounter;
    double          m_lastRms {0.0};
    std::function<void(short)> m_sampleCallback;
};

#endif // DECODIUM_AUDIO_SINK_H
