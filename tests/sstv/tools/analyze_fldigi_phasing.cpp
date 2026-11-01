// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic approximation of FLdigi's narrow-filter FM decisions.
// Input: mono 11025 Hz float32 PCM. Output: end time, period and fraction
// above the 128 threshold between high-to-low transitions. This does not run
// FLdigi's full state machine or sound-device path. Filter coefficients and
// discriminator quantization were checked against public upstream wefax.cxx.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    std::ifstream in(argv[1], std::ios::binary | std::ios::ate);
    if (!in || in.tellg() <= 0 || in.tellg() % 4 != 0) return 2;
    const auto count = static_cast<std::size_t>(in.tellg()) / sizeof(float);
    std::vector<float> samples(count);
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(samples.data()), count * sizeof(float))) return 3;
    constexpr double pi = 3.14159265358979323846;
    constexpr double coefficients[17] = {-7,-18,-15,11,56,116,177,223,240,
                                          223,177,116,56,11,-15,-18,-7};
    std::array<double,17> real {}, imaginary {};
    std::size_t index = 0, oscillator = 0;
    constexpr std::size_t increment = 8192 * 1900 / 11025;
    double previousI = 0, previousQ = 0;
    bool high = false;
    int length = 0, white = 0;
    for (std::size_t k = 0; k < samples.size(); ++k) {
        oscillator = (oscillator + increment) % 8192;
        const double phase = 2 * pi * oscillator / 8192;
        real[index] = samples[k] * std::cos(phase);
        imaginary[index] = samples[k] * std::sin(phase);
        index = (index + 1) % 17;
        double i = 0, q = 0;
        for (int j = 0; j < 17; ++j) {
            i += coefficients[j] * real[(index+j)%17];
            q += coefficients[j] * imaginary[(index+j)%17];
        }
        const double magnitude = std::hypot(i,q);
        if (magnitude < 1e-8) continue;
        i /= magnitude; q /= magnitude;
        const double sine = std::clamp(previousQ*i - previousI*q,-1.0,1.0);
        const int quantized = std::clamp(int((sine+1)*128),0,255);
        const double scaled = 127.5 + 127.5*11025/400
            * (std::asin(2.0*quantized/256-1)/(2*pi));
        const int pixel = std::clamp(int(scaled),0,255);
        previousI = i; previousQ = q;
        ++length;
        if (pixel > 128) ++white;
        if (pixel > 229 && !high) high = true;
        else if (pixel < 25 && high) {
            high = false;
            if (length > 1600 && length < 13000)
                std::printf("%.3f %.3f %.4f\n",double(k)/11025,
                            double(length)/11025,double(white)/length);
            length = white = 0;
        }
    }
}
