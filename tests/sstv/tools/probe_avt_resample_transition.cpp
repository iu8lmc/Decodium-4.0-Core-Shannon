// SPDX-License-Identifier: GPL-3.0-or-later
// A/B synthetic boundary comparison using the production resampler and demodulator.
#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include "src/sstv/dsp/SstvResampler.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <limits>
#include <vector>

using namespace decodium::sstv;

int main()
{
    constexpr double pi = 3.141592653589793;
    constexpr int outputBoundary = 1200;
    for (int variant = 0; variant < 4; ++variant) {
      double maxDifference = 0.0;
      int incompleteWindows = 0;
      int incompleteDirect = 0;
      int incompleteResampled = 0;
      for (int phaseStep = 0; phaseStep < 8; ++phaseStep) {
        double fractions[2][3] = {};
        const double first = variant == 0 ? 1800.0 : variant == 1 ? 2000.0
            : variant == 2 ? 1500.0 : 2300.0;
        const double second = variant == 0 ? 2100.0 : variant == 1 ? 1700.0
            : variant == 2 ? 2300.0 : 1500.0;
        constexpr int rate = 48000;
        const int boundary = rate / 10;
        std::vector<float> input(static_cast<std::size_t>(rate / 5));
        double phase = phaseStep * pi / 4.0;
        for (int i = 0; i < static_cast<int>(input.size()); ++i) {
            phase += 2.0 * pi * (i < boundary ? first : second) / rate;
            input[static_cast<std::size_t>(i)] = static_cast<float>(0.7 * std::sin(phase));
        }
        for (bool resampled : {false, true}) {
            std::vector<float> samples;
            if (resampled) {
                SstvResampler converter(static_cast<std::uint32_t>(rate));
                samples = converter.process(input);
                const auto tail = converter.flush();
                samples.insert(samples.end(), tail.begin(), tail.end());
            } else {
                samples.reserve(input.size() / 4U);
                for (std::size_t i = 0; i < input.size(); i += 4U) {
                    samples.push_back(input[i]);
                }
            }
            auto config = SstvFrequencyDemodulatorConfig::sstvDefaults();
            config.averagingSamples = 3;
            config.hopSamples = 1;
            SstvFrequencyDemodulator demodulator(config);
            const auto observations = demodulator.consume(samples);
            for (int shift = 0; shift <= 2; ++shift) {
                double sum = 0.0;
                int count = 0;
                for (const auto& item : observations) {
                    const auto sample = static_cast<int>(item.centreSample);
                    if (item.valid() && sample >= outputBoundary - 3 + shift
                        && sample < outputBoundary - 1 + shift) {
                        sum += item.correctedFrequencyHz;
                        ++count;
                    }
                }
                fractions[resampled ? 1 : 0][shift] = count == 2
                    ? (sum / count - first) / (second - first)
                    : std::numeric_limits<double>::quiet_NaN();
            }
        }
        for (int shift = 0; shift <= 2; ++shift) {
            if (!std::isfinite(fractions[0][shift]) || !std::isfinite(fractions[1][shift])) {
                ++incompleteWindows;
                incompleteDirect += !std::isfinite(fractions[0][shift]);
                incompleteResampled += !std::isfinite(fractions[1][shift]);
                std::printf("%c phase=%d/4pi shift=%d incomplete-valid-observations\n",
                            'A' + variant, phaseStep, shift);
                continue;
            }
            const double difference = std::abs(fractions[1][shift] - fractions[0][shift]);
            maxDifference = std::max(maxDifference, difference);
            std::printf("%c phase=%d/4pi shift=%d direct=%.4f resampled=%.4f delta=%.4f\n",
                        'A' + variant, phaseStep, shift, fractions[0][shift],
                        fractions[1][shift], difference);
        }
      }
      std::printf("%c maximum difference across complete windows: %.4f; incomplete windows: %d/24 (direct %d, resampled %d)\n",
                  'A' + variant, maxDifference, incompleteWindows, incompleteDirect, incompleteResampled);
    }
}
