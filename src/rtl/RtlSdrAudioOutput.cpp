// SPDX-License-Identifier: GPL-3.0-or-later

#include "RtlSdrAudioOutput.h"

#include <QAudioSink>
#include <QDateTime>
#include <QDebug>
#include <QIODevice>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstring>

RtlSdrAudioOutput::RtlSdrAudioOutput(QObject *parent)
    : QObject(parent)
    , m_pumpTimer(new QTimer(this))
{
    m_pumpTimer->setInterval(5);
    connect(m_pumpTimer, &QTimer::timeout, this, &RtlSdrAudioOutput::pump);
}

bool RtlSdrAudioOutput::chooseOutputFormat(const QAudioDevice& output,
                                           int sampleRate,
                                           QAudioFormat *format) const
{
    if (!format || output.id().isEmpty() || sampleRate <= 0) {
        return false;
    }

    for (QAudioFormat::SampleFormat sampleFormat :
         {QAudioFormat::Int16, QAudioFormat::Float}) {
        for (int channels : {1, 2}) {
            QAudioFormat candidate;
            candidate.setSampleRate(sampleRate);
            candidate.setChannelCount(channels);
            candidate.setSampleFormat(sampleFormat);
            if (output.isFormatSupported(candidate)) {
                *format = candidate;
                return true;
            }
        }
    }
    return false;
}

void RtlSdrAudioOutput::start(const QAudioDevice& output, int sampleRate)
{
    if (m_sink && m_outputId == output.id() && m_sourceSampleRate == sampleRate) {
        return;
    }
    stop(QStringLiteral("receiver audio reconfigured"));

    QAudioFormat format;
    if (!chooseOutputFormat(output, sampleRate, &format)) {
        emit error(QStringLiteral("RTL-SDR audio output %1 has no supported %2 Hz PCM format.")
                       .arg(output.description()).arg(sampleRate));
        return;
    }

    m_format = format;
    m_outputId = output.id();
    m_sourceSampleRate = sampleRate;
    m_sink = new QAudioSink(output, m_format, this);
    m_sink->setBufferSize(m_format.bytesForDuration(250000));
    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (!m_sink || state != QAudio::StoppedState || m_sink->error() == QAudio::NoError) {
            return;
        }
        const QString message = QStringLiteral("RTL-SDR audio output stopped (Qt error %1).")
                                    .arg(static_cast<int>(m_sink->error()));
        stop(QStringLiteral("Qt audio output error"));
        emit error(message);
    });

    m_device = m_sink->start();
    if (!m_device) {
        const QString message = QStringLiteral("RTL-SDR audio output could not start on %1.")
                                    .arg(output.description());
        stop(QStringLiteral("audio output start failed"));
        emit error(message);
        return;
    }

    m_telemetryStartMs = QDateTime::currentMSecsSinceEpoch();
    m_telemetrySamples = 0;
    m_droppedBytes = 0;
    m_telemetrySumSquares = 0.0;
    m_telemetryPeak = 0;
    emit runningChanged(true);
    emit statusChanged(QStringLiteral("RTL-SDR receiver audio: %1 Hz mono → %2")
                           .arg(sampleRate).arg(output.description()));
}

QByteArray RtlSdrAudioOutput::convertSamples(const QVector<short>& samples) const
{
    QByteArray converted;
    if (samples.isEmpty() || !m_format.isValid()) {
        return converted;
    }

    const int channels = m_format.channelCount();
    if (m_format.sampleFormat() == QAudioFormat::Int16) {
        converted.resize(samples.size() * channels * static_cast<int>(sizeof(short)));
        auto *destination = reinterpret_cast<short *>(converted.data());
        if (channels == 1) {
            std::memcpy(destination, samples.constData(), converted.size());
        } else {
            for (qsizetype index = 0; index < samples.size(); ++index) {
                destination[index * 2] = samples.at(index);
                destination[index * 2 + 1] = samples.at(index);
            }
        }
        return converted;
    }

    if (m_format.sampleFormat() == QAudioFormat::Float) {
        converted.resize(samples.size() * channels * static_cast<int>(sizeof(float)));
        auto *destination = reinterpret_cast<float *>(converted.data());
        for (qsizetype index = 0; index < samples.size(); ++index) {
            const float value = static_cast<float>(samples.at(index)) / 32768.0f;
            for (int channel = 0; channel < channels; ++channel) {
                destination[index * channels + channel] = value;
            }
        }
    }
    return converted;
}

void RtlSdrAudioOutput::enqueueSamples(const QVector<short>& samples, int sampleRate)
{
    if (!m_sink || !m_device || samples.isEmpty() || sampleRate != m_sourceSampleRate) {
        return;
    }

    logTelemetry(samples);
    m_pending.append(convertSamples(samples));
    const int maximumBytes = qMax(m_format.bytesForDuration(500000), 4096);
    if (m_pending.size() > maximumBytes) {
        const qsizetype excess = m_pending.size() - maximumBytes;
        m_pending.remove(0, excess);
        m_droppedBytes += excess;
    }
    pump();
    if (!m_pending.isEmpty() && !m_pumpTimer->isActive()) {
        m_pumpTimer->start();
    }
}

void RtlSdrAudioOutput::pump()
{
    if (!m_sink || !m_device || m_pending.isEmpty()) {
        if (m_pumpTimer->isActive()) {
            m_pumpTimer->stop();
        }
        return;
    }
    const qint64 writable = m_sink->bytesFree();
    if (writable <= 0) {
        return;
    }
    const qint64 toWrite = qMin<qint64>(writable, m_pending.size());
    const qint64 written = m_device->write(m_pending.constData(), toWrite);
    if (written > 0) {
        m_pending.remove(0, static_cast<qsizetype>(written));
    }
}

void RtlSdrAudioOutput::logTelemetry(const QVector<short>& samples)
{
    for (short sample : samples) {
        const int value = sample;
        m_telemetrySumSquares += static_cast<double>(value) * value;
        m_telemetryPeak = std::max(m_telemetryPeak, std::abs(value));
    }
    m_telemetrySamples += samples.size();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_telemetryStartMs <= 0) {
        m_telemetryStartMs = nowMs;
    }
    if (nowMs - m_telemetryStartMs < 5000 || m_telemetrySamples <= 0) {
        return;
    }

    const double rms = std::sqrt(m_telemetrySumSquares / m_telemetrySamples) / 32768.0;
    qInfo().noquote()
        << "[RTL-AUDIO]"
        << "samples=" << m_telemetrySamples
        << "rms=" << QString::number(rms, 'f', 5)
        << "peak=" << QString::number(static_cast<double>(m_telemetryPeak) / 32768.0, 'f', 5)
        << "queued_bytes=" << m_pending.size()
        << "dropped_bytes=" << m_droppedBytes
        << "format=" << QStringLiteral("%1Hz/%2ch/%3")
               .arg(m_format.sampleRate()).arg(m_format.channelCount())
               .arg(static_cast<int>(m_format.sampleFormat()));
    m_telemetryStartMs = nowMs;
    m_telemetrySamples = 0;
    m_droppedBytes = 0;
    m_telemetrySumSquares = 0.0;
    m_telemetryPeak = 0;
}

void RtlSdrAudioOutput::stop(const QString& reason)
{
    if (m_pumpTimer->isActive()) {
        m_pumpTimer->stop();
    }
    m_pending.clear();
    m_device = nullptr;
    if (!m_sink) {
        m_outputId.clear();
        m_sourceSampleRate = 0;
        return;
    }

    QAudioSink *sink = m_sink;
    m_sink = nullptr;
    sink->stop();
    delete sink;
    m_outputId.clear();
    m_sourceSampleRate = 0;
    emit runningChanged(false);
    if (!reason.trimmed().isEmpty()) {
        qInfo().noquote() << "[RTL-AUDIO] stopped reason=" << reason;
    }
}

void RtlSdrAudioOutput::shutdown()
{
    stop(QStringLiteral("shutdown"));
}
