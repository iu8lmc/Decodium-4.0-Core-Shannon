// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../tx/SstvToneGenerator.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {
// These are distinct explicit wire conventions, not auto-detected aliases.
enum class SstvFax480Profile : std::uint8_t { Dayton480, Qsstv500, LibsstvVis480 };
enum class SstvFax480Region : std::uint8_t { Start, PhasingSync, PhasingWhite, Sync, Pixel, Complete };
struct SstvFax480Position final {
    SstvFax480Region region {SstvFax480Region::Complete};
    std::uint32_t line {0};
    std::uint32_t pixel {0};
    double frequencyHz {0};
};
class SstvFax480Mapper final {
public:
    explicit SstvFax480Mapper(SstvFax480Profile profile, std::uint32_t sampleRate);
    SstvFax480Position position(std::uint64_t sample) const noexcept;
    std::uint32_t height() const noexcept;
    std::uint64_t imageStart() const noexcept;
    std::uint64_t lineStart(std::uint32_t line) const;
    std::uint64_t totalSamples() const noexcept;
private:
    std::uint64_t sampleAtNs(std::uint64_t ns) const noexcept;
    SstvFax480Profile profile_;
    std::uint32_t rate_;
    std::uint64_t lineNs_;
    std::uint64_t headerNs_;
};
class SstvFax480Encoder final {
public:
    SstvFax480Encoder(SstvFax480Profile profile, std::uint32_t sampleRate,
                      std::vector<std::uint8_t> pixels);
    std::size_t pull(std::int16_t* output, std::size_t capacity);
    bool complete() const noexcept;
    bool cancelled() const noexcept { return generator_.cancelled(); }
    std::uint64_t totalSamples() const noexcept { return mapper_.totalSamples(); }
    std::uint64_t producedSamples() const noexcept { return cursor_; }
    std::size_t pullPcm16(std::int16_t* output, std::size_t capacity) { return pull(output, capacity); }
    void cancel() noexcept;
    void reset() noexcept;
    const SstvFax480Mapper& mapper() const noexcept { return mapper_; }
private:
    double frequency(std::uint64_t sample) const noexcept;
    SstvFax480Mapper mapper_;
    std::vector<std::uint8_t> pixels_;
    SstvToneGenerator generator_;
    std::uint64_t cursor_ {0};
};
} // namespace decodium::sstv
