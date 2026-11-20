// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>

namespace decodium {
// Preconditions: capacity > 0, count <= capacity, end >= count, no overlap.
inline void copyAudioRingSnapshot(short const* ring, int capacity,
                                  std::uint64_t end, short* output, int count)
{
    int const start = static_cast<int>((end - static_cast<std::uint64_t>(count))
                                       % static_cast<std::uint64_t>(capacity));
    int const first = std::min(count, capacity - start);
    std::copy_n(ring + start, first, output);
    std::copy_n(ring, count - first, output + first);
}
}
