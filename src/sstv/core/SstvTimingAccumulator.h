// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvTypes.h"

#include <cstdint>

namespace decodium::sstv {

// Converts exact fixed-point protocol durations into sample counts while
// carrying sub-sample error across every segment.  It intentionally does not
// use floating point or compiler-specific 128-bit integers.
class SstvTimingAccumulator final
{
public:
    explicit SstvTimingAccumulator(std::uint32_t sampleRate);

    std::uint32_t sampleRate() const noexcept;
    std::uint64_t samplesFor(Picoseconds duration);

    std::uint64_t totalSamples() const noexcept;
    std::uint64_t fractionalRemainder() const noexcept;
    void reset() noexcept;

private:
    std::uint32_t m_sampleRate {0};
    std::uint64_t m_totalSamples {0};
    // Numerator over kPicosecondsPerSecond, in picosecond-sample units.
    std::uint64_t m_fractionalRemainder {0};
};

} // namespace decodium::sstv
