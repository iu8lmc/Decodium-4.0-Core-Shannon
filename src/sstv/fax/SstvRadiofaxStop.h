// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../dsp/SstvFrequencyDemodulator.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace decodium::sstv {
// Experimental detector for a sustained 450 Hz black/white stop pattern.
// A matching raster pattern is indistinguishable from stop on signal alone.
class SstvRadiofaxStopDetector final {
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8192U;
    explicit SstvRadiofaxStopDetector(std::uint32_t sampleRate, double frequencyOffsetHz = 0.0);
    std::optional<std::uint64_t> consume(const SstvFrequencyObservation* observations, std::size_t count);
    void reset() noexcept;
private:
    void clearWindow(std::uint64_t start) noexcept;
    std::uint32_t rate_;
    double offset_;
    std::uint64_t start_ {0}, last_ {0}, firstMatch_ {0};
    std::size_t total_ {0}, valid_ {0};
    double real_ {0}, imag_ {0}, absolute_ {0}, mean_ {0};
    unsigned matches_ {0};
    bool active_ {false}, hasLast_ {false};
    std::optional<std::uint64_t> detection_;
};

// Some WEFAX transmitters send the APT stop as a continuous 450 Hz audio
// tone, outside the image FM passband. Examine raw resampled PCM separately.
class SstvRadiofaxAudioStopDetector final {
public:
    explicit SstvRadiofaxAudioStopDetector(std::uint32_t sampleRate);
    std::optional<std::uint64_t> consume(const float* samples, std::size_t count,
                                         bool enabled = true);
    void reset() noexcept;
private:
    std::uint32_t rate_;
    static constexpr std::size_t kToneBins = 9U;
    std::uint64_t nextSample_ {0U};
    std::size_t count_ {0U};
    std::array<double, kToneBins> real_ {}, imag_ {};
    std::array<double, kToneBins> cosStep_ {}, sinStep_ {}, cosPhase_ {}, sinPhase_ {};
    std::array<unsigned, kToneBins> matches_ {};
    double energy_ {0.0};
    std::optional<std::uint64_t> detection_;
};
} // namespace decodium::sstv
