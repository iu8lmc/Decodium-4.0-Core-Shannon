// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace decodium::sstv::hamdrm::waveform {

// Fixed, bounded 4.5-sample delay for the laboratory timing candidate.
// State is retained across PCM chunks; reset it on every stream discontinuity.
class HamDrmFractionalDelay final {
public:
    HamDrmFractionalDelay()
    {
        const double pi = std::acos(-1.0);
        double sum = 0.0;
        for (std::size_t i = 0; i < taps_.size(); ++i) {
            const double x = double(i) - 4.5;
            taps_[i] = std::sin(pi * x) / (pi * x)
                * (0.54 - 0.46 * std::cos(2.0 * pi * double(i) / 9.0));
            sum += taps_[i];
        }
        for (auto& tap : taps_) tap /= sum;
    }

    std::vector<double> process(const std::vector<double>& samples)
    {
        std::vector<double> output(samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i) {
            std::move_backward(history_.begin(), history_.end() - 1, history_.end());
            history_[0] = samples[i];
            for (std::size_t tap = 0; tap < taps_.size(); ++tap)
                output[i] += taps_[tap] * history_[tap];
        }
        return output;
    }

    void reset() noexcept { history_.fill(0.0); }

private:
    std::array<double, 10> taps_ {};
    std::array<double, 10> history_ {};
};

} // namespace decodium::sstv::hamdrm::waveform
