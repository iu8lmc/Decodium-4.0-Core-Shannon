// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include "src/sstv/dsp/SstvPreprocessor.h"
#include "src/sstv/integration/SstvRxRuntime.h"

#include <QCoreApplication>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

namespace {

using decodium::sstv::SstvFrequencyDemodulator;
using decodium::sstv::SstvFrequencyDemodulatorConfig;
using decodium::sstv::SstvPreprocessor;
using decodium::sstv::SstvRxRuntime;

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::size_t kChunkSamples = 1'024U;
constexpr std::uint64_t kAudioSeconds = 15U;

double milliseconds(std::clock_t begin, std::clock_t end) noexcept
{
    if (begin == static_cast<std::clock_t>(-1)
        || end == static_cast<std::clock_t>(-1) || end < begin) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return static_cast<double>(end - begin) * 1'000.0
        / static_cast<double>(CLOCKS_PER_SEC);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    // An inactive runtime must not create its DSP worker.  Process CPU time is
    // sampled across a wall-clock quiet interval so the final report has a
    // reproducible upper bound instead of inferring inactivity from source
    // inspection alone.
    SstvRxRuntime inactive;
    const SstvRxRuntime::Snapshot before = inactive.snapshot();
    const std::clock_t idleCpuBegin = std::clock();
    const auto idleWallBegin = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    const auto idleWallEnd = std::chrono::steady_clock::now();
    const std::clock_t idleCpuEnd = std::clock();
    const SstvRxRuntime::Snapshot after = inactive.snapshot();

    SstvFrequencyDemodulatorConfig demodulatorConfig =
        SstvFrequencyDemodulatorConfig::sstvDefaults();
    // Match the production runtime's native fast-pixel settings.
    demodulatorConfig.averagingSamples = 3U;
    demodulatorConfig.hopSamples = 1U;
    SstvPreprocessor preprocessor;
    SstvFrequencyDemodulator demodulator(demodulatorConfig);

    const std::uint64_t totalSamples = kAudioSeconds * kSampleRate;
    std::vector<float> input(kChunkSamples, 0.0F);
    std::uint64_t generated = 0U;
    std::uint64_t observations = 0U;
    const auto dspBegin = std::chrono::steady_clock::now();
    while (generated < totalSamples) {
        const std::size_t count = static_cast<std::size_t>(std::min<
            std::uint64_t>(kChunkSamples, totalSamples - generated));
        for (std::size_t index = 0U; index < count; ++index) {
            const double sample = static_cast<double>(generated + index);
            const double phase = 2.0 * kPi * 1'900.0 * sample
                / static_cast<double>(kSampleRate);
            input[index] = static_cast<float>(0.35 * std::sin(phase));
        }
        std::vector<float> processed(input.cbegin(), input.cbegin()
            + static_cast<std::ptrdiff_t>(count));
        processed = preprocessor.process(processed);
        observations += demodulator.consume(processed).size();
        generated += count;
    }
    const auto dspEnd = std::chrono::steady_clock::now();

    const double idleWallMs = std::chrono::duration<double, std::milli>(
        idleWallEnd - idleWallBegin).count();
    const double idleCpuMs = milliseconds(idleCpuBegin, idleCpuEnd);
    const double dspWallSeconds = std::chrono::duration<double>(
        dspEnd - dspBegin).count();
    const double realtimeRatio = dspWallSeconds > 0.0
        ? static_cast<double>(kAudioSeconds) / dspWallSeconds : 0.0;
    const auto preMetrics = preprocessor.metricsSnapshot();
    const auto demodMetrics = demodulator.metricsSnapshot();

    const bool inactiveInvariant = !before.workerRunning
        && !after.workerRunning && before.chunksProcessed == 0U
        && after.chunksProcessed == 0U;
    const bool idleCpuBound = std::isfinite(idleCpuMs)
        && idleCpuMs <= 50.0;
    const bool realtimeBound = std::isfinite(realtimeRatio)
        && realtimeRatio >= 1.0;
    const bool streamInvariant = generated == totalSamples
        && preMetrics.samplesConsumed == totalSamples
        && preMetrics.samplesProduced == totalSamples
        && observations == demodMetrics.observationsProduced
        && observations != 0U;

    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"audio_seconds\": " << kAudioSeconds << ",\n"
              << "  \"dsp_wall_seconds\": " << dspWallSeconds << ",\n"
              << "  \"dsp_realtime_ratio\": " << realtimeRatio << ",\n"
              << "  \"frequency_observations\": " << observations << ",\n"
              << "  \"inactive_wall_ms\": " << idleWallMs << ",\n"
              << "  \"inactive_cpu_ms\": " << idleCpuMs << ",\n"
              << "  \"inactive_worker_running\": "
              << (after.workerRunning ? "true" : "false") << ",\n"
              << "  \"inactive_chunks_processed\": "
              << after.chunksProcessed << ",\n"
              << "  \"pass\": "
              << (inactiveInvariant && idleCpuBound && realtimeBound
                      && streamInvariant ? "true" : "false")
              << "\n}\n";

    inactive.shutdown();
    return inactiveInvariant && idleCpuBound && realtimeBound
            && streamInvariant ? 0 : 1;
}
