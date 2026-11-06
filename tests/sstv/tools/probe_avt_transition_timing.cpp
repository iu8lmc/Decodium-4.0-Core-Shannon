// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic A/B tone-boundary diagnostic; this does not change the decoder.
#include "src/sstv/dsp/SstvFrequencyDemodulator.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace decodium::sstv;

int main()
{
    constexpr double sampleRate = 12000.0;
    constexpr double pi = 3.141592653589793;
    constexpr int boundary = 1200;
    for (int variant = 0; variant < 2; ++variant) {
        const double first = variant ? 2000.0 : 1800.0;
        const double second = variant ? 1700.0 : 2100.0;
        std::vector<float> samples(2400);
        double phase = 0.0;
        for (int i = 0; i < static_cast<int>(samples.size()); ++i) {
            phase += 2.0 * pi * (i < boundary ? first : second) / sampleRate;
            samples[static_cast<std::size_t>(i)] = static_cast<float>(0.7 * std::sin(phase));
        }
        auto config = SstvFrequencyDemodulatorConfig::sstvDefaults();
        config.hilbertTaps = 31;
        config.averagingSamples = 3;
        config.hopSamples = 1;
        SstvFrequencyDemodulator demodulator(config);
        const auto observations = demodulator.consume(samples.data(), samples.size());
        for (int shift = -2; shift <= 2; ++shift) {
            double sum = 0.0;
            int count = 0;
            for (const auto& observation : observations) {
                const auto sample = static_cast<int>(observation.centreSample);
                if (observation.valid() && sample >= boundary - 3 + shift
                    && sample < boundary - 1 + shift) {
                    sum += observation.correctedFrequencyHz;
                    ++count;
                }
            }
            if (count != 2) return 1;
            const double mean = sum / count;
            std::printf("%c shift=%d mean=%.2f nominal=%.2f fraction-next=%.3f n=%d\n",
                        'A' + variant, shift, mean, first, (mean - first) / (second - first), count);
        }
    }
}
