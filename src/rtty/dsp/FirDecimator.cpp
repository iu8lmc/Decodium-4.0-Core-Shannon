#include "dsp/FirDecimator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace decortty::dsp {

namespace {
constexpr float kPi    = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;
}

FirDecimator::FirDecimator(int factor, int inputRate, float cutoffHz, int taps)
{
    configure(factor, inputRate, cutoffHz, taps);
}

void FirDecimator::configure(int factor, int inputRate, float cutoffHz, int taps)
{
    m_factor = std::max(1, factor);
    taps     = std::max(8, taps | 1);   // odd length keeps the phase linear and symmetric

    const float outputRate = static_cast<float>(inputRate) / static_cast<float>(m_factor);
    if (cutoffHz <= 0.0f)
        cutoffHz = 0.8f * outputRate * 0.5f;
    const float fc = cutoffHz / static_cast<float>(inputRate);   // cycles/sample

    m_taps.assign(static_cast<size_t>(taps), 0.0f);
    const int   mid = taps / 2;
    float       sum = 0.0f;
    for (int i = 0; i < taps; ++i) {
        const int   n = i - mid;
        const float sinc = (n == 0) ? 2.0f * fc
                                    : std::sin(kTwoPi * fc * static_cast<float>(n))
                                          / (kPi * static_cast<float>(n));
        // Hamming window: 53 dB of stopband is more than the 12-bit-ish dynamic
        // range of the audio path needs, and it keeps the transition sharp.
        const float w = 0.54f - 0.46f * std::cos(kTwoPi * static_cast<float>(i)
                                                 / static_cast<float>(taps - 1));
        m_taps[static_cast<size_t>(i)] = sinc * w;
        sum += m_taps[static_cast<size_t>(i)];
    }
    if (sum != 0.0f)
        for (float& t : m_taps)
            t /= sum;   // unity DC gain

    reset();
}

void FirDecimator::reset()
{
    m_history.assign(m_taps.size(), 0.0f);
    m_pos   = 0;
    m_phase = 0;
}

int FirDecimator::process(const float* in, int count, float* out)
{
    const int taps = static_cast<int>(m_taps.size());
    int       written = 0;

    for (int i = 0; i < count; ++i) {
        m_history[static_cast<size_t>(m_pos)] = in[i];
        m_pos = (m_pos + 1) % taps;

        if (++m_phase < m_factor)
            continue;
        m_phase = 0;

        float acc = 0.0f;
        int   h   = m_pos;
        for (int t = 0; t < taps; ++t) {
            acc += m_taps[static_cast<size_t>(t)] * m_history[static_cast<size_t>(h)];
            h = (h + 1) % taps;
        }
        out[written++] = acc;
    }
    return written;
}

// ── FirInterpolator ─────────────────────────────────────────────────────────

FirInterpolator::FirInterpolator(int factor, int outputRate, float cutoffHz, int taps)
{
    configure(factor, outputRate, cutoffHz, taps);
}

void FirInterpolator::configure(int factor, int outputRate, float cutoffHz, int taps)
{
    m_factor = std::max(1, factor);
    taps     = std::max(8, taps | 1);

    const float inputRate = static_cast<float>(outputRate) / static_cast<float>(m_factor);
    if (cutoffHz <= 0.0f)
        cutoffHz = 0.8f * inputRate * 0.5f;
    const float fc = cutoffHz / static_cast<float>(outputRate);

    m_taps.assign(static_cast<size_t>(taps), 0.0f);
    const int mid = taps / 2;
    float     sum = 0.0f;
    for (int i = 0; i < taps; ++i) {
        const int   n = i - mid;
        const float sinc = (n == 0) ? 2.0f * fc
                                    : std::sin(kTwoPi * fc * static_cast<float>(n))
                                          / (kPi * static_cast<float>(n));
        const float w = 0.54f - 0.46f * std::cos(kTwoPi * static_cast<float>(i)
                                                 / static_cast<float>(taps - 1));
        m_taps[static_cast<size_t>(i)] = sinc * w;
        sum += m_taps[static_cast<size_t>(i)];
    }
    // Unity gain through the zero-stuffing: the filter sees only one non-zero
    // sample in every `factor`, so the passband would otherwise lose that much.
    if (sum != 0.0f)
        for (float& t : m_taps)
            t *= static_cast<float>(m_factor) / sum;

    reset();
}

void FirInterpolator::reset()
{
    m_history.assign(m_taps.size(), 0.0f);
    m_pos = 0;
}

int FirInterpolator::process(const float* in, int count, float* out)
{
    const int taps    = static_cast<int>(m_taps.size());
    int       written = 0;

    for (int i = 0; i < count; ++i) {
        for (int phase = 0; phase < m_factor; ++phase) {
            m_history[static_cast<size_t>(m_pos)] = (phase == 0) ? in[i] : 0.0f;
            m_pos = (m_pos + 1) % taps;

            float acc = 0.0f;
            int   h   = m_pos;
            for (int t = 0; t < taps; ++t) {
                acc += m_taps[static_cast<size_t>(t)] * m_history[static_cast<size_t>(h)];
                h = (h + 1) % taps;
            }
            out[written++] = acc;
        }
    }
    return written;
}

void stereoToMono(const float* interleaved, int frames, float* mono)
{
    for (int i = 0; i < frames; ++i)
        mono[i] = 0.5f * (interleaved[2 * i] + interleaved[2 * i + 1]);
}

} // namespace decortty::dsp
