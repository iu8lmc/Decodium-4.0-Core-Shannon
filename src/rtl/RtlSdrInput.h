// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include "RtlSdrDsp.h"

class RtlSdrReader;

// Owns one asynchronous librtlsdr reader.  No device operation runs on the
// GUI thread: start/open/configure/read all happen in RtlSdrReader::run().
class RtlSdrInput final : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        SdrRadio,
        DirectSampling
    };
    Q_ENUM(Mode)

    struct Config {
        int deviceIndex {0};
        quint32 centerFrequencyHz {14074000U};
        // Selected RF channel relative to the hardware centre. A non-zero
        // offset lets zero-IF tuners avoid their DC spike while DSP recentres
        // the desired channel before decoding or demodulation.
        int channelOffsetHz {0};
        int sampleRate {240000};
        int ppmCorrection {0};
        // Negative means tuner AGC.  Manual values are tenths of a dB, as in
        // the librtlsdr API (e.g. 280 == 28.0 dB).
        int gainTenthsDb {-1};
        bool digitalAgc {false};
        Mode mode {Mode::SdrRadio};
        RtlSdrDsp::Demodulator demodulator {RtlSdrDsp::Demodulator::WeakSignal};
        bool spectrumInverted {false};
        bool biasTee {false};
        double audioGain {1.0};
        double ssbVoiceBandwidthHz {3500.0};
        RtlSdrDsp::SsbAgcMode ssbAgcMode {RtlSdrDsp::SsbAgcMode::Slow};
        int ssbNotchFrequencyHz {0};
        RtlSdrDsp::SsbNoiseReductionMode ssbNoiseReduction {
            RtlSdrDsp::SsbNoiseReductionMode::Off};
    };

    explicit RtlSdrInput(QObject *parent = nullptr);
    ~RtlSdrInput() override;

    static bool compiledIn();
    static QStringList enumerateDevices(QString *error = nullptr);
    static QString modeName(Mode mode);
    static QString demodulatorName(RtlSdrDsp::Demodulator demodulator);

    bool isRunning() const;
    bool isActive() const;
    Config activeConfig() const;
    bool retune(quint32 centerFrequencyHz, int channelOffsetHz);

public slots:
    void start(const Config& config);
    void stop();

signals:
    void started(QString deviceDescription);
    void stopped();
    void retuned(quint32 centerFrequencyHz, int channelOffsetHz);
    void configurationAdjusted(RtlSdrInput::Config config, QString reason);
    void statusChanged(QString message);
    void error(QString message);
    void runningChanged(bool running);
    // Full-rate signed interleaved I/Q for the RF panadapter/waterfall.  The
    // reader rate-limits this signal so display work never back-pressures USB.
    void iqSamplesReady(QVector<short> samples, int sampleRate, quint32 centerFrequencyHz);
    // First producer-boundary relays for bounded DirectConnection consumers.
    // Existing ready signals below retain their owner-thread queued semantics.
    void pcmSamplesProduced(QVector<short> samples);
    void audioSamplesProduced(QVector<short> samples, int sampleRate);
    void pcmSamplesReady(QVector<short> samples);
    void audioSamplesReady(QVector<short> samples, int sampleRate);

private slots:
    void onReaderStarted(const QString& deviceDescription);
    void onReaderStopped();
    void onReaderRetuned(quint32 centerFrequencyHz, int channelOffsetHz);
    void onReaderError(const QString& message);
    void onReaderConfigurationAdjusted(const RtlSdrInput::Config& config,
                                       const QString& reason);
    void startPendingReader();

private:
    RtlSdrReader *m_reader {nullptr};
    Config m_activeConfig;
    Config m_pendingConfig;
    bool m_startPending {false};
    bool m_running {false};
};

Q_DECLARE_METATYPE(RtlSdrInput::Config)
