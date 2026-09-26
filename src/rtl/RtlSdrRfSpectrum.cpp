// SPDX-License-Identifier: GPL-3.0-or-later

#include "RtlSdrRfSpectrum.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace {
constexpr float kPi = 3.14159265358979323846f;

void fftInPlace(std::vector<std::complex<float>>& values)
{
    const int size = static_cast<int>(values.size());
    for (int index = 1, bitReverse = 0; index < size; ++index) {
        int bit = size >> 1;
        for (; bitReverse & bit; bit >>= 1) bitReverse ^= bit;
        bitReverse ^= bit;
        if (index < bitReverse) std::swap(values[index], values[bitReverse]);
    }
    for (int length = 2; length <= size; length <<= 1) {
        const float angle = -2.0f * kPi / static_cast<float>(length);
        const std::complex<float> root(std::cos(angle), std::sin(angle));
        for (int offset = 0; offset < size; offset += length) {
            std::complex<float> twiddle(1.0f, 0.0f);
            for (int index = 0; index < length / 2; ++index) {
                const std::complex<float> even = values[offset + index];
                const std::complex<float> odd = values[offset + index + length / 2] * twiddle;
                values[offset + index] = even + odd;
                values[offset + index + length / 2] = even - odd;
                twiddle *= root;
            }
        }
    }
}

float percentile(QVector<float> values, float fraction)
{
    if (values.isEmpty()) return -120.0f;
    const int index = qBound(0, qRound((values.size() - 1) * fraction), values.size() - 1);
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values.at(index);
}
}

RtlSdrRfSpectrum::Frame RtlSdrRfSpectrum::compute(const QVector<short>& interleavedIq,
                                                   int sampleRate,
                                                   quint32 centerFrequencyHz)
{
    Frame result;
    if (sampleRate <= 0 || interleavedIq.size() < kFftSize * 2) {
        return result;
    }

    std::vector<std::complex<float>> fft(static_cast<size_t>(kFftSize));
    const int inputStart = interleavedIq.size() - kFftSize * 2;
    for (int sample = 0; sample < kFftSize; ++sample) {
        // Blackman-Harris controls leakage from strong broadcast carriers.
        const float phase = 2.0f * kPi * static_cast<float>(sample)
            / static_cast<float>(kFftSize - 1);
        const float window = 0.35875f - 0.48829f * std::cos(phase)
            + 0.14128f * std::cos(2.0f * phase) - 0.01168f * std::cos(3.0f * phase);
        const float i = static_cast<float>(interleavedIq.at(inputStart + sample * 2)) / 32768.0f;
        const float q = static_cast<float>(interleavedIq.at(inputStart + sample * 2 + 1)) / 32768.0f;
        fft[static_cast<size_t>(sample)] = std::complex<float>(i * window, q * window);
    }
    fftInPlace(fft);

    result.values.resize(kFftSize);
    for (int output = 0; output < kFftSize; ++output) {
        const int source = (output + kFftSize / 2) % kFftSize; // FFT shift
        const float magnitude = std::abs(fft[static_cast<size_t>(source)])
            / static_cast<float>(kFftSize);
        result.values[output] = 20.0f * std::log10(qMax(magnitude, 1.0e-8f));
    }

    const float noiseFloor = percentile(result.values, 0.20f) - 5.0f;
    const float signalPeak = percentile(result.values, 0.995f) + 5.0f;
    result.minDb = qBound(-130.0f, noiseFloor, -40.0f);
    result.maxDb = qBound(result.minDb + 40.0f,
                            qMax(result.minDb + 65.0f, signalPeak), 10.0f);
    result.frequencyMinHz = static_cast<float>(centerFrequencyHz)
        - static_cast<float>(sampleRate) * 0.5f;
    result.frequencyMaxHz = static_cast<float>(centerFrequencyHz)
        + static_cast<float>(sampleRate) * 0.5f;
    return result;
}
