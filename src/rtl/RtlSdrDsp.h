// SPDX-License-Identifier: GPL-3.0-or-later
//
// DSP stage shared by the RTL-SDR acquisition thread.  It deliberately keeps
// the RF IQ stream, the general-receiver demodulator and Decodium's 12 kHz
// weak-signal decoder input as three separate paths.

#pragma once

#include <QVector>

#include <vector>

class RtlSdrDsp final
{
public:
    static constexpr int kDecoderSampleRate = 12000;
    static constexpr int kReceiverAudioSampleRate = 48000;

    enum class Demodulator {
        WeakSignal,
        WideFm,
        NarrowFm,
        Am,
        Usb,
        Lsb,
        Cw
    };

    // These controls belong strictly to the post-demodulated RTL-SDR SSB
    // listening path. They are intentionally ignored for WeakSignal/FT8 and
    // every non-SSB demodulator.
    enum class SsbAgcMode {
        Off,
        Slow,
        Medium
    };

    enum class SsbNoiseReductionMode {
        Off,
        Light,
        Medium
    };

    struct Result {
        // Signed, interleaved I/Q at the tuner sample rate.  This is the RF
        // path used by the panadapter and waterfall; it is never decoder PCM.
        QVector<short> iq;
        // 12 kHz decoder PCM, present only in WeakSignal mode.
        QVector<short> decoderPcm;
        // 48 kHz audio for the RX listen path, present for radio demodulators.
        QVector<short> audioPcm;
    };

    RtlSdrDsp();

    // Compatibility overload for the weak-signal decoder path.
    bool configure(int inputSampleRate,
                   int outputSampleRate = kDecoderSampleRate,
                   double audioGain = 1.0);
    // General SDR receiver configuration. Non-weak demodulators output 48 kHz
    // mono PCM while preserving the full-rate IQ stream for RF display.
    bool configure(int inputSampleRate, Demodulator demodulator, double audioGain = 1.0,
                   int channelOffsetHz = 0, bool spectrumInverted = false,
                   double ssbVoiceBandwidthHz = 3500.0,
                   SsbAgcMode ssbAgcMode = SsbAgcMode::Slow,
                   int ssbNotchFrequencyHz = 0,
                   SsbNoiseReductionMode ssbNoiseReduction = SsbNoiseReductionMode::Off);
    void reset();

    bool isConfigured() const { return m_inputSampleRate > 0 && m_outputDecimation > 0; }
    int inputSampleRate() const { return m_inputSampleRate; }
    int outputSampleRate() const { return m_outputSampleRate; }
    int decimationFactor() const { return m_outputDecimation; }
    Demodulator demodulator() const { return m_demodulator; }

    // `bytes` must contain interleaved unsigned I/Q pairs from librtlsdr.
    // This legacy accessor returns only the decoder stream and remains useful
    // for focused unit tests and existing weak-signal consumers.
    QVector<short> process(const unsigned char *data, int bytes);
    Result processFrame(const unsigned char *data, int bytes, bool includeIq = true);

    static bool isSupportedSampleRate(int inputSampleRate,
                                      int outputSampleRate = kDecoderSampleRate);
    static bool isSupportedSampleRateForDemodulator(int inputSampleRate,
                                                    Demodulator demodulator);
    static QString demodulatorName(Demodulator demodulator);

private:
    bool enhancedSsbAudioEnabled() const;
    void rebuildFilters();
    static std::vector<double> buildLowpass(int tapCount, double cutoffHz, int sampleRate);
    static double filterSample(double value, const std::vector<double>& taps,
                               std::vector<double>& delay, int& writeIndex);
    double demodulate(double i, double q);
    short toPcm(double value) const;

    int m_inputSampleRate {0};
    int m_outputSampleRate {kDecoderSampleRate};
    int m_outputDecimation {0};
    int m_outputDecimationPhase {0};
    int m_outputDecimationSamples {0};
    int m_channelDecimation {1};
    int m_channelDecimationPhase {0};
    int m_demodSampleRate {0};
    int m_decoderDecimation {0};
    int m_decoderDecimationPhase {0};
    Demodulator m_demodulator {Demodulator::WeakSignal};
    bool m_spectrumInverted {false};
    double m_audioGain {1.0};

    double m_dcI {0.0};
    double m_dcQ {0.0};
    double m_previousI {0.0};
    double m_previousQ {0.0};
    double m_amDc {0.0};
    double m_deemphasis {0.0};
    double m_fmDc {0.0};
    double m_channelNcoStep {0.0};
    double m_channelNcoCos {1.0};
    double m_channelNcoSin {0.0};
    double m_channelNcoStepCos {1.0};
    double m_channelNcoStepSin {0.0};
    int m_channelNcoNormalizeCounter {0};
    double m_channelAlpha {1.0};
    double m_channelIState1 {0.0};
    double m_channelIState2 {0.0};
    double m_channelQState1 {0.0};
    double m_channelQState2 {0.0};
    double m_audioAlpha {1.0};
    double m_audioState1 {0.0};
    double m_audioState2 {0.0};
    double m_outputDecimationAccumulator {0.0};

    // The general receiver must remain inexpensive at the RTL-SDR input rate.
    // USB/LSB alone receive their quality pass after downsampling to 48 kHz:
    // a voice-band FIR, DC rejection and slow audio AGC.  Weak-signal decoder
    // PCM and the IQ stream for the panadapter never enter this path.
    double m_ssbDc {0.0};
    double m_ssbAgcEnvelope {0.05};
    double m_ssbVoiceBandwidthHz {3500.0};
    SsbAgcMode m_ssbAgcMode {SsbAgcMode::Slow};
    int m_ssbNotchFrequencyHz {0};
    SsbNoiseReductionMode m_ssbNoiseReduction {SsbNoiseReductionMode::Off};
    bool m_ssbNotchEnabled {false};
    double m_ssbNotchB0 {1.0};
    double m_ssbNotchB1 {0.0};
    double m_ssbNotchB2 {0.0};
    double m_ssbNotchA1 {0.0};
    double m_ssbNotchA2 {0.0};
    double m_ssbNotchZ1 {0.0};
    double m_ssbNotchZ2 {0.0};
    double m_ssbNoiseFloor {0.01};
    double m_ssbNoiseEnvelope {0.0};

    std::vector<double> m_channelTaps;
    std::vector<double> m_channelDelayI;
    std::vector<double> m_channelDelayQ;
    int m_channelDelayWrite {0};

    std::vector<double> m_ssbAudioTaps;
    std::vector<double> m_ssbAudioDelay;
    int m_ssbAudioDelayWrite {0};

    std::vector<double> m_decoderTaps;
    std::vector<double> m_decoderDelay;
    int m_decoderDelayWrite {0};
};
