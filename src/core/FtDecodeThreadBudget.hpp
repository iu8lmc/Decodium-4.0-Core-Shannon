#pragma once

#include <algorithm>

namespace decodium {
namespace decode {

inline int adaptiveInteractiveThreadCount(int logicalCores,
                                          int normalLimit,
                                          int maximumThreads)
{
    int const cores = std::max(1, logicalCores);
    int const boundedNormal = std::max(1, std::min(normalLimit, maximumThreads));
    int const interactiveCap = cores <= 2 ? 1
                             : cores <= 4 ? cores - 1
                             : cores <= 7 ? cores - 2
                                          : std::max(2, (cores * 2) / 3);
    return std::max(1, std::min(boundedNormal, interactiveCap));
}

} // namespace decode
} // namespace decodium
