// DecoRTTY — minimal radix-2 FFT (no external dependencies).
#pragma once

#include <complex>
#include <vector>

namespace decortty::dsp {

using Cpx = std::complex<float>;

// In-place iterative radix-2 FFT. `size` must be a power of two.
// Twiddles are precomputed once per size, so repeated transforms of the same
// length (the only case the demodulator has) cost no trig at all.
class Fft {
public:
    explicit Fft(int size);

    int size() const { return m_size; }

    // Forward transform, in place. `data` must hold exactly size() elements.
    void forward(Cpx* data) const;
    // Inverse transform, in place, scaled by 1/N.
    void inverse(Cpx* data) const;

    // Real input helper: copies `in` (size() reals) into a scratch buffer,
    // applies the window when one is supplied, and transforms.
    void forwardReal(const float* in, const float* window, Cpx* out) const;

    static bool isPowerOfTwo(int n) { return n > 0 && (n & (n - 1)) == 0; }

private:
    void transform(Cpx* data, bool inverseTransform) const;

    int                m_size{0};
    int                m_levels{0};
    std::vector<int>   m_reversed;   // bit-reversal permutation
    std::vector<Cpx>   m_twiddle;    // half-size twiddle table
};

// Hann window, normalised so coherent gain is 1.0.
std::vector<float> hannWindow(int n);
// Blackman-Harris (4-term), for spectrum display where sidelobes matter.
std::vector<float> blackmanHarrisWindow(int n);

} // namespace decortty::dsp
