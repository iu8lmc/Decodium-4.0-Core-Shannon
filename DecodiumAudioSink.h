#pragma once
#ifndef DECODIUM_AUDIO_SINK_H
#define DECODIUM_AUDIO_SINK_H

#include <cmath>
#include <functional>
#include <QObject>
#include <QVector>
#include "Audio/AudioDevice.hpp"

// DecodiumAudioSink: AudioDevice subclass that appends 16-bit PCM mono
// samples into DecodiumBridge's m_audioBuffer and emits RMS audio levels.
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
                               QObject* parent = nullptr)
        : AudioDevice(parent)
        , m_buffer(buffer)
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

        // Decimazione: tieni 1 campione ogni m_dsf
        const int prevSize = m_buffer.size();
        for (qint64 i = 0; i < numFrames; ++i) {
            if (m_dsfCounter == 0) {
                short s = tmp[static_cast<int>(i)];
                m_buffer.append(s);
                if (m_sampleCallback) m_sampleCallback(s);
            }
            m_dsfCounter = (m_dsfCounter + 1) % m_dsf;
        }

        // RMS sui campioni decimati per il meter
        const int newSamples = m_buffer.size() - prevSize;
        if (newSamples > 0) {
            double sumSq = 0.0;
            const short* s = m_buffer.constData() + prevSize;
            for (int i = 0; i < newSamples; ++i) { double v = s[i]; sumSq += v*v; }
            double rms = std::sqrt(sumSq / newSamples) / 32768.0;
            emit audioLevelChanged(rms);
        }

        return maxSize;
    }

signals:
    void audioLevelChanged(double level);

private:
    QVector<short>& m_buffer;
    unsigned        m_dsf;
    unsigned        m_dsfCounter;
    std::function<void(short)> m_sampleCallback;
};

#endif // DECODIUM_AUDIO_SINK_H
