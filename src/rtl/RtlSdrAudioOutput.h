// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QByteArray>
#include <QObject>
#include <QVector>

class QAudioSink;
class QIODevice;
class QTimer;

// Receiver-audio playback lives on its own thread. Opening or stopping a
// CoreAudio/WASAPI/PipeWire stream can take hundreds of milliseconds and must
// never stall the QML/bridge event loop that also drives the RF display.
class RtlSdrAudioOutput final : public QObject
{
    Q_OBJECT

public:
    explicit RtlSdrAudioOutput(QObject *parent = nullptr);

public slots:
    void start(const QAudioDevice& output, int sampleRate);
    void enqueueSamples(const QVector<short>& samples, int sampleRate);
    void stop(const QString& reason = QString());
    void shutdown();

signals:
    void statusChanged(const QString& message);
    void error(const QString& message);
    void runningChanged(bool running);

private slots:
    void pump();

private:
    QByteArray convertSamples(const QVector<short>& samples) const;
    bool chooseOutputFormat(const QAudioDevice& output, int sampleRate,
                            QAudioFormat *format) const;
    void logTelemetry(const QVector<short>& samples);

    QAudioSink *m_sink {nullptr};
    QIODevice *m_device {nullptr};
    QTimer *m_pumpTimer {nullptr};
    QAudioFormat m_format;
    QByteArray m_outputId;
    QByteArray m_pending;
    int m_sourceSampleRate {0};
    qint64 m_telemetryStartMs {0};
    qint64 m_telemetrySamples {0};
    qint64 m_droppedBytes {0};
    double m_telemetrySumSquares {0.0};
    int m_telemetryPeak {0};
};
