// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace decodium::sstv {

// SSTV timings are represented as signed integer picoseconds.  The signed
// representation lets parsers reject negative input explicitly; valid values
// are always non-negative.
struct Picoseconds final
{
    std::int64_t count {0};

    constexpr Picoseconds() noexcept = default;
    explicit constexpr Picoseconds(std::int64_t value) noexcept
        : count(value)
    {
    }
};

constexpr std::int64_t kPicosecondsPerSecond = 1'000'000'000'000LL;
constexpr std::int64_t kPicosecondsPerMillisecond = 1'000'000'000LL;
constexpr std::int64_t kPicosecondsPerMicrosecond = 1'000'000LL;

constexpr bool operator==(Picoseconds lhs, Picoseconds rhs) noexcept
{
    return lhs.count == rhs.count;
}

constexpr bool operator!=(Picoseconds lhs, Picoseconds rhs) noexcept
{
    return !(lhs == rhs);
}

enum class ModeClassification : std::uint8_t {
    AnalogSstv,
    RelatedFax,
    DigitalSstv
};

// Catalogue state is deliberately separate from RX/TX implementation state.
// Merely listing a mode never makes it supported.
enum class CatalogStatus : std::uint8_t {
    Catalogued,
    Verified,
    Blocked
};

enum class CapabilityStatus : std::uint8_t {
    Unimplemented,
    Implemented,
    Verified,
    Blocked
};

enum class VisEncoding : std::uint8_t {
    Unknown,
    None,
    StandardSevenBit,
    Extended,
    // MMSSTV's four six-bit-group N-VIS protocol.  It is deliberately
    // distinct from the standard/wide-extended VIS bit framing.
    Narrow24Bit
};

enum class Parity : std::uint8_t {
    None,
    Even,
    Odd
};

enum class ColourSpace : std::uint8_t {
    Unknown,
    Rgb,
    YCbCr,
    Grayscale,
    Monochrome,
    ModeSpecific
};

enum class ColourComponent : std::uint8_t {
    Red,
    Green,
    Blue,
    Luminance,
    ChrominanceBlue,
    ChrominanceRed,
    Gray,
    ModeSpecific
};

enum class ChromaSubsampling : std::uint8_t {
    Unknown,
    NotApplicable,
    Cs444,
    Cs422,
    Cs420,
    ModeSpecific,
    // Full horizontal chroma resolution shared vertically by two rows.
    // Appended to preserve the numeric values of the existing catalogue ABI.
    Cs440
};

enum class EvidenceStatus : std::uint8_t {
    None,
    AuditedSources,
    DeterministicTests,
    IndependentVector
};

enum class InteroperabilityStatus : std::uint8_t {
    NotTested,
    UpstreamPathObserved,
    IndependentlyVerified,
    Blocked
};

enum class FixtureStatus : std::uint8_t {
    Missing,
    SelfGeneratedOnly,
    Independent,
    Blocked
};

} // namespace decodium::sstv
