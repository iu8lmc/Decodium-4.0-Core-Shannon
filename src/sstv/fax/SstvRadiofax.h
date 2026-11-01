// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "../tx/SstvToneGenerator.h"

namespace decodium::sstv {

// WMO 386, attachment III-5, analog facsimile control signals. IOC is a
// geometric ratio, not a prescribed number of digital pixels per line.
enum class SstvFaxIoc : std::uint16_t { Ioc288 = 288, Ioc576 = 576 };
enum class SstvFaxPhasePattern : std::uint8_t { Symmetric, WhiteFivePercent };
enum class SstvFaxRegion : std::uint8_t {
    Start, Phasing, Image, StopAlternating, StopBlack, Complete
};

struct SstvFaxConfig final {
    SstvFaxIoc ioc {SstvFaxIoc::Ioc576};
    std::uint16_t linesPerMinute {120U};
    std::uint32_t sampleRate {12'000U};
    std::uint32_t width {1'809U};
    std::uint32_t height {1U};
    SstvFaxPhasePattern phasing {SstvFaxPhasePattern::WhiteFivePercent};
    // Explicit TX/RX convention: a 5%-white stripe centred on the line
    // boundary followed by one all-white end-of-phasing line (FLdigi).
    bool centeredPhasingWithWhiteEndLine {false};
};

struct SstvFaxPosition final {
    SstvFaxRegion region {SstvFaxRegion::Complete};
    std::uint32_t line {0U};
    std::uint32_t pixel {0U};
    bool white {false}; // meaningful for control regions only
};

class SstvRadiofaxMapper final {
public:
    explicit SstvRadiofaxMapper(SstvFaxConfig config);
    SstvFaxPosition positionAtSample(std::uint64_t sample) const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t lineStartSample(std::uint32_t line) const;
    static double frequencyForValue(std::uint8_t value) noexcept;
    static std::uint8_t valueForFrequency(double frequency) noexcept;
private:
    SstvFaxConfig config_;
    std::uint64_t lineBoundary(std::uint64_t line) const noexcept;
    std::uint64_t lineForOffset(std::uint64_t offset) const noexcept;
    std::uint64_t imageStart_ {0U};
    std::uint64_t imageEnd_ {0U};
    std::uint64_t stopBlackStart_ {0U};
    std::uint64_t total_ {0U};
};

// Experimental bounded PCM source. The profile is explicit; automatic
// acquisition and application integration are separate work.
class SstvRadiofaxEncoder final {
public:
    SstvRadiofaxEncoder(SstvFaxConfig config, std::vector<std::uint8_t> pixels);
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity);
    bool complete() const noexcept;
    bool cancelled() const noexcept;
    void cancel() noexcept;
    void reset() noexcept;
    std::uint64_t totalSamples() const noexcept;
    std::uint64_t producedSamples() const noexcept;
    const SstvRadiofaxMapper& mapper() const noexcept;
private:
    double frequencyAt(std::uint64_t sample) const noexcept;
    SstvFaxConfig config_;
    SstvRadiofaxMapper mapper_;
    std::vector<std::uint8_t> pixels_;
    SstvToneGenerator generator_;
    std::uint64_t produced_ {0U};
};

} // namespace decodium::sstv
