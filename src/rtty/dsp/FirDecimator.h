// DecoRTTY — windowed-sinc FIR decimator and stereo folding helpers.
#pragma once

#include <vector>

namespace decortty::dsp {

// Integer-factor decimator. The radio delivers 24 kHz audio and the decoder
// works at 8 kHz, so the only ratio in production is 3 — but the class takes an
// arbitrary factor because the file-replay path also has to handle 48 kHz WAVs.
//
// Only every Nth output of the convolution is ever computed, which is what makes
// decimation cheaper than filtering at the input rate.
class FirDecimator {
public:
    // `factor` is the decimation ratio, `cutoffHz` the passband edge relative to
    // the INPUT sample rate. Pass 0 for cutoff to get 0.8 × the output Nyquist,
    // which leaves the RTTY tones untouched while killing the aliases.
    FirDecimator(int factor = 3, int inputRate = 24000, float cutoffHz = 0.0f, int taps = 64);

    void configure(int factor, int inputRate, float cutoffHz = 0.0f, int taps = 64);
    void reset();

    int factor() const { return m_factor; }

    // Filter and decimate. Returns the number of samples written to `out`,
    // which must have room for count/factor + 1.
    int process(const float* in, int count, float* out);

private:
    std::vector<float> m_taps;
    std::vector<float> m_history;
    int                m_pos{0};
    int                m_factor{3};
    int                m_phase{0};
};

// Integer-factor interpolator — the decimator's mirror image, for the transmit
// direction. Zero-stuffing then filtering is what removes the spectral images
// an interpolator would otherwise put either side of the tones; a plain
// sample-repeat or linear interpolation leaves them there, and on the air they
// are splatter.
class FirInterpolator {
public:
    FirInterpolator(int factor = 2, int outputRate = 48000, float cutoffHz = 0.0f, int taps = 64);

    void configure(int factor, int outputRate, float cutoffHz = 0.0f, int taps = 64);
    void reset();

    int factor() const { return m_factor; }

    // Returns the number of samples written to `out`, which must have room for
    // count * factor.
    int process(const float* in, int count, float* out);

private:
    std::vector<float> m_taps;
    std::vector<float> m_history;
    int                m_pos{0};
    int                m_factor{2};
};

// Fold interleaved stereo into mono, in place-safe fashion. The Flex remote
// audio stream is stereo with both channels carrying the same slice audio
// unless the operator has split RX; averaging is the right default and costs
// nothing.
void stereoToMono(const float* interleaved, int frames, float* mono);

} // namespace decortty::dsp
