#include "dsp/Fft.h"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace decortty::dsp {

namespace {
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
}

Fft::Fft(int size)
    : m_size(size)
{
    if (!isPowerOfTwo(size))
        throw std::invalid_argument("Fft: size must be a power of two");

    while ((1 << m_levels) < m_size)
        ++m_levels;

    m_reversed.resize(m_size);
    for (int i = 0; i < m_size; ++i) {
        int r = 0;
        for (int b = 0; b < m_levels; ++b)
            r |= ((i >> b) & 1) << (m_levels - 1 - b);
        m_reversed[i] = r;
    }

    m_twiddle.resize(m_size / 2);
    for (int i = 0; i < m_size / 2; ++i) {
        const float ang = -kTwoPi * static_cast<float>(i) / static_cast<float>(m_size);
        m_twiddle[i] = Cpx(std::cos(ang), std::sin(ang));
    }
}

void Fft::transform(Cpx* data, bool inverseTransform) const
{
    for (int i = 0; i < m_size; ++i) {
        const int j = m_reversed[i];
        if (j > i)
            std::swap(data[i], data[j]);
    }

    for (int len = 2; len <= m_size; len <<= 1) {
        const int half = len / 2;
        const int step = m_size / len;
        for (int i = 0; i < m_size; i += len) {
            for (int k = 0; k < half; ++k) {
                Cpx w = m_twiddle[k * step];
                if (inverseTransform)
                    w = std::conj(w);
                const Cpx u = data[i + k];
                const Cpx v = data[i + k + half] * w;
                data[i + k]        = u + v;
                data[i + k + half] = u - v;
            }
        }
    }

    if (inverseTransform) {
        const float scale = 1.0f / static_cast<float>(m_size);
        for (int i = 0; i < m_size; ++i)
            data[i] *= scale;
    }
}

void Fft::forward(Cpx* data) const { transform(data, false); }
void Fft::inverse(Cpx* data) const { transform(data, true); }

void Fft::forwardReal(const float* in, const float* window, Cpx* out) const
{
    if (window) {
        for (int i = 0; i < m_size; ++i)
            out[i] = Cpx(in[i] * window[i], 0.0f);
    } else {
        for (int i = 0; i < m_size; ++i)
            out[i] = Cpx(in[i], 0.0f);
    }
    transform(out, false);
}

std::vector<float> hannWindow(int n)
{
    std::vector<float> w(static_cast<size_t>(n));
    if (n <= 1) {
        if (n == 1) w[0] = 1.0f;
        return w;
    }
    for (int i = 0; i < n; ++i)
        w[static_cast<size_t>(i)] =
            0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(n - 1)));
    return w;
}

std::vector<float> blackmanHarrisWindow(int n)
{
    std::vector<float> w(static_cast<size_t>(n));
    if (n <= 1) {
        if (n == 1) w[0] = 1.0f;
        return w;
    }
    constexpr float a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n - 1);
        w[static_cast<size_t>(i)] = a0 - a1 * std::cos(kTwoPi * t)
                                       + a2 * std::cos(2.0f * kTwoPi * t)
                                       - a3 * std::cos(3.0f * kTwoPi * t);
    }
    return w;
}

} // namespace decortty::dsp
