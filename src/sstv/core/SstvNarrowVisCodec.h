// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace decodium::sstv {

enum class SstvNarrowVisMode : std::uint8_t
{
    Mp73,
    Mp110,
    Mp140,
    Mc110,
    Mc140,
    Mc180,
};

enum class SstvNarrowVisError : std::uint8_t
{
    None,
    InvalidHeader,
    InvalidSymbol,
    InvalidPreamble,
    ComplementMismatch,
    UnknownMode,
};

struct SstvNarrowVisTone final
{
    double frequencyHz {0.0};
    Picoseconds duration;
};

struct SstvNarrowVisEncodedFrame final
{
    SstvNarrowVisMode mode {SstvNarrowVisMode::Mp73};
    std::uint8_t payload {0U};
    std::array<std::uint8_t, 4U> groups {{0U, 0U, 0U, 0U}};
    std::array<bool, 24U> bitsLsbFirst {};
    std::array<SstvNarrowVisTone, 27U> tones;
    std::uint32_t packedWireValue {0U};
};

struct SstvNarrowVisDecodeResult final
{
    bool valid {false};
    std::optional<SstvNarrowVisMode> mode;
    std::uint8_t payload {0U};
    std::array<std::uint8_t, 4U> groups {{0U, 0U, 0U, 0U}};
    std::array<bool, 24U> bitsLsbFirst {};
    std::uint32_t packedWireValue {0U};
    SstvNarrowVisError error {SstvNarrowVisError::None};
    double confidence {0.0};
};

// MMSSTV narrow modes use a dedicated 24-bit N-VIS framing scheme, not the
// standard or wide extended VIS codec.  Four six-bit groups are transmitted
// least-significant bit first: 0x2d, 0x15, N-VIS, 0x15 xor N-VIS.  The full
// physical header is 1900/300 ms, 2100/100 ms, 1900/22 ms, then the 24 data
// symbols at 22 ms each (one=1900 Hz, zero=2100 Hz).
class SstvNarrowVisCodec final
{
public:
    static constexpr std::size_t GroupCount = 4U;
    static constexpr std::size_t BitsPerGroup = 6U;
    static constexpr std::size_t DataBitCount = 24U;
    static constexpr std::size_t ToneCount = 27U;
    static constexpr std::uint8_t FirstPreambleGroup = 0x2dU;
    static constexpr std::uint8_t SecondPreambleGroup = 0x15U;
    static constexpr double OneFrequencyHz = 1'900.0;
    static constexpr double ZeroFrequencyHz = 2'100.0;
    static constexpr Picoseconds LeaderDuration {300'000'000'000LL};
    static constexpr Picoseconds GuardDuration {100'000'000'000LL};
    static constexpr Picoseconds SymbolDuration {22'000'000'000LL};
    static constexpr Picoseconds FrameDuration {950'000'000'000LL};

    SstvNarrowVisCodec() = delete;

    static std::uint8_t payloadForMode(SstvNarrowVisMode mode);
    static std::optional<SstvNarrowVisMode> modeForPayload(
        std::uint8_t payload) noexcept;
    static SstvNarrowVisEncodedFrame encode(SstvNarrowVisMode mode);

    // frequencyHz must contain the complete three-tone physical header and
    // exactly 24 data symbols.  Protocol corruption is returned fail-closed;
    // malformed API bounds or an ambiguous tolerance throw invalid_argument.
    static SstvNarrowVisDecodeResult decode(
        const double* frequencyHz,
        std::size_t count,
        double toleranceHz = 90.0);
    static SstvNarrowVisDecodeResult decode(
        const std::array<double, ToneCount>& frequencyHz,
        double toleranceHz = 90.0);

    static const char* errorName(SstvNarrowVisError error) noexcept;
};

} // namespace decodium::sstv
