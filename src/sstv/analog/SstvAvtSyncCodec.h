// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvAvtMode : std::uint8_t
{
    Avt24,
    Avt90,
    Avt94,
};

enum class SstvAvtVariant : std::uint8_t
{
    Normal = 0U,
    Narrow = 1U,
    Qrm = 2U,
    NarrowQrm = 3U,
};

struct SstvAvtSyncTone final
{
    double frequencyHz {0.0};
    Picoseconds duration;
};

struct SstvAvtSyncFrame final
{
    SstvAvtMode mode {SstvAvtMode::Avt24};
    bool narrow {false};
    std::uint8_t counter {0U};
    std::uint8_t normalWord {0U};
    std::uint8_t invertedWord {0xffU};
    std::array<SstvAvtSyncTone, 17U> tones;
};

struct SstvAvtDecodedSyncFrame final
{
    bool valid {false};
    std::optional<SstvAvtMode> mode;
    bool narrow {false};
    std::uint8_t counter {0U};
    std::uint8_t normalWord {0U};
    std::uint8_t invertedWord {0U};
    std::uint8_t remainingFrames {0U};
    Picoseconds remainingDuration;
    double confidence {0.0};
};

// AVT has no per-line sync.  Its frame anchor is a 32-word digital countdown:
// a 1900 Hz start symbol followed by an 8-bit mode/counter word and its exact
// bitwise inverse.  Bits are sent MSB first.  This codec deliberately remains
// separate from standard VIS, which precedes the countdown three times.
class SstvAvtSyncCodec final
{
public:
    static constexpr std::size_t FrameCount = 32U;
    static constexpr std::size_t DataBitsPerFrame = 16U;
    static constexpr std::size_t TonesPerFrame = 17U;
    static constexpr std::size_t CountdownToneCount =
        FrameCount * TonesPerFrame;
    static constexpr double StartFrequencyHz = 1'900.0;
    static constexpr double NormalZeroFrequencyHz = 1'600.0;
    static constexpr double NormalOneFrequencyHz = 2'200.0;
    static constexpr double NarrowZeroFrequencyHz = 1'700.0;
    static constexpr double NarrowOneFrequencyHz = 2'100.0;
    // 102.4 baud exactly: 1 / 102.4 s = 9.765625 ms.
    static constexpr Picoseconds SymbolDuration {9'765'625'000LL};
    static constexpr Picoseconds FrameDuration {166'015'625'000LL};
    static constexpr Picoseconds CountdownDuration {5'312'500'000'000LL};

    SstvAvtSyncCodec() = delete;

    static std::uint8_t modePrefix(SstvAvtMode mode);
    static std::optional<SstvAvtMode> modeForPrefix(
        std::uint8_t prefix) noexcept;
    static std::uint8_t visPayload(SstvAvtMode mode,
                                   SstvAvtVariant variant);
    static std::optional<SstvAvtMode> modeForVis(
        std::uint8_t payload) noexcept;
    static SstvAvtVariant variantForVis(std::uint8_t payload);
    static bool isNarrow(SstvAvtVariant variant) noexcept;
    static bool isQrm(SstvAvtVariant variant) noexcept;

    static SstvAvtSyncFrame encodeFrame(SstvAvtMode mode,
                                        std::uint8_t counter,
                                        bool narrow = false);
    static std::vector<SstvAvtSyncTone> encodeCountdown(
        SstvAvtMode mode,
        bool narrow = false);

    // frequencyHz must contain exactly one start tone plus 16 data tones.
    // A frame is accepted only when the second byte is the exact inverse of
    // the first and its three-bit prefix names a supported AVT mode.
    static SstvAvtDecodedSyncFrame decodeFrame(
        const double* frequencyHz,
        std::size_t count,
        bool narrow = false,
        double toleranceHz = 120.0);
    static SstvAvtDecodedSyncFrame decodeFrame(
        const std::array<double, TonesPerFrame>& frequencyHz,
        bool narrow = false,
        double toleranceHz = 120.0);
};

} // namespace decodium::sstv
