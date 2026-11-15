#pragma once

#include <algorithm>

namespace decodium {

// Retain both the write index modulo capacity and whether the ring has filled.
// Call after each bounded batch, before an ever-growing sample count can overflow.
inline void normalizeWaterfallRingPosition(int& position, int capacity)
{
    if (position >= 2 * capacity)
        position = capacity + position % capacity;
}

// Copy at most one ring of contiguous audio, preserving the full/empty
// information encoded by position. Source and ring must not overlap.
inline void appendWaterfallRing(short* ring, int capacity, int& position,
                                short const* source, int count)
{
    int const start = position % capacity;
    int const first = std::min(count, capacity - start);
    std::copy_n(source, first, ring + start);
    std::copy_n(source + first, count - first, ring);
    position += count;
    normalizeWaterfallRingPosition(position, capacity);
}

}
