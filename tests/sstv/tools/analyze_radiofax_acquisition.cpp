// SPDX-License-Identifier: GPL-3.0-or-later
// Offline native frontend diagnostic for 48 kHz stereo float Radiofax audio.
#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include "src/sstv/dsp/SstvPreprocessor.h"
#include "src/sstv/dsp/SstvResampler.h"
#include "src/sstv/fax/SstvRadiofaxAutoRx.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <vector>

using namespace decodium::sstv;
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 8 || input.tellg() > 240LL * 48000 * 8 || input.tellg() % 8 != 0) return 2;
    input.seekg(0);
    SstvResampler resampler(48000U);
    SstvPreprocessor processor;
    auto cfg = SstvFrequencyDemodulatorConfig::sstvDefaults();
    cfg.averagingSamples = 3U; cfg.hopSamples = 1U;
    SstvFrequencyDemodulator demodulator(cfg);
    SstvFaxConfig fax;
    fax.width = 128U; fax.height = 150U;
    if (std::getenv("DECODIUM_RADIOFAX_IOC288")) fax.ioc = SstvFaxIoc::Ioc288;
    fax.centeredPhasingWithWhiteEndLine = !std::getenv("DECODIUM_RADIOFAX_NONCENTERED");
    fax.wmoDeadSector = std::getenv("DECODIUM_RADIOFAX_WMO_DEAD_SECTOR") != nullptr;
    fax.wmoTimedPhasing = std::getenv("DECODIUM_RADIOFAX_WMO_TIMED") != nullptr;
    SstvRadiofaxAutoRx receiver(fax);
    SstvRadiofaxAptStartDetector apt576(12000U, SstvFaxIoc::Ioc576);
    SstvRadiofaxAptStartDetector apt288(12000U, SstvFaxIoc::Ioc288);
    std::vector<float> stereo(8192U), mono(4096U);
    bool printedLock = false;
    int lastTone = 0;
    std::deque<SstvFrequencyObservation> history;
    while (input) {
        input.read(reinterpret_cast<char*>(stereo.data()), stereo.size() * sizeof(float));
        const auto count = static_cast<std::size_t>(input.gcount()) / (2U * sizeof(float));
        if (!count) break;
        for (std::size_t i = 0; i < count; ++i) mono[i] = (stereo[2*i] + stereo[2*i+1]) * 0.5f;
        const auto resampled = resampler.process(mono.data(), count);
        const auto filtered = processor.process(resampled);
        const auto observations = demodulator.consume(filtered);
        for (const auto& obs : observations) {
            apt576.consume(obs);
            apt288.consume(obs);
            if (std::getenv("DECODIUM_RADIOFAX_TRACE_LEVELS")) {
                history.push_back(obs);
                // Twelve lines at 60 LPM plus the confirmation window.
                while (!history.empty() && obs.centreSample - history.front().centreSample > 192000U)
                    history.pop_front();
            }
            if (!obs.valid() || obs.confidence < 0.2 || obs.confidence > 1.0) continue;
            const int tone = obs.correctedFrequencyHz >= 1350.0 && obs.correctedFrequencyHz <= 1700.0 ? -1
                : (obs.correctedFrequencyHz >= 2100.0 && obs.correctedFrequencyHz <= 2450.0 ? 1 : 0);
            if (std::getenv("DECODIUM_RADIOFAX_TRACE_EDGES") && tone == 1 && lastTone == -1)
                std::cout << "white_edge=" << obs.centreSample << "\n";
            if (tone) lastTone = tone;
        }
        receiver.consume(observations);
        if (!printedLock && receiver.phasing()) {
            const auto lock = *receiver.phasing();
            std::cout << "lock_edge=" << lock.latestWhiteEdgeSample << " period=" << lock.detectedPeriodSamples
                      << " lpm=" << lock.linesPerMinute
                      << " apt576=" << apt576.lastAptWhiteEdge().value_or(0U)
                      << " apt288=" << apt288.lastAptWhiteEdge().value_or(0U) << "\n";
            printedLock = true;
        }
        if (receiver.imageStartSample()) {
            std::cout << "image_start=" << *receiver.imageStartSample() << " ignored="
                      << receiver.ignoredIrregularPhasingPulses() << " valid="
                      << receiver.phasingClockTrackerValid();
            std::cout << " wmo_timed_phasing=" << receiver.usedWmoTimedPhasing();
            if (const auto clock = receiver.imageClock())
                std::cout << " clock_samples=" << clock->samples << " clock_lines=" << clock->lines
                          << " provisional=" << receiver.imageClockProvisional();
            std::cout << "\n";
            if (std::getenv("DECODIUM_RADIOFAX_TRACE_LEVELS") && receiver.phasing()) {
                const auto start = *receiver.imageStartSample();
                const auto period = std::uint64_t(12000U) * 60U / receiver.phasing()->linesPerMinute;
                const auto median = [&](std::uint64_t first, std::uint64_t last, double lo, double hi) {
                    std::vector<double> values;
                    for (const auto& item : history)
                        if (item.centreSample >= first && item.centreSample < last && item.valid()
                            && item.confidence >= 0.7 && item.confidence <= 1.0
                            && item.correctedFrequencyHz >= lo && item.correctedFrequencyHz <= hi)
                            values.push_back(item.correctedFrequencyHz);
                    if (values.size() < 64U) return -1.0;
                    std::sort(values.begin(), values.end());
                    return values[values.size() / 2U];
                };
                if (start > 12U * period) {
                    std::cout << "baseline_black=" << median(start - 12U * period, start - 7U * period, 1450, 1550)
                              << " baseline_white=" << median(start - 12U * period, start - 7U * period, 2250, 2350)
                              << " recent_black=" << median(start - 2U * period, start - period, 1450, 1550)
                              << " recent_white=" << median(start - period / 2U, start - period / 4U, 2250, 2350)
                              << "\n";
                }
            }
            return 0;
        }
    }
    std::cout << "no_image_start\n";
    return 1;
}
