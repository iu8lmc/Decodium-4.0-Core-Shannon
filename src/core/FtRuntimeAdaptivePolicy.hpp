#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>

namespace decodium {
namespace decode {

class Ft8MicroStallGuard
{
public:
    static constexpr std::int64_t StallThresholdMs = 90;
    static constexpr std::int64_t ObservationWindowMs = 30000;
    static constexpr int ActivationStallCount = 3;
    static constexpr int CleanPeriodsToRestore = 16;

    enum class PeriodTransition
    {
        None,
        Restored
    };

    bool recordStall(std::int64_t nowMs, std::int64_t stallMs)
    {
        if (stallMs < StallThresholdMs) {
            return false;
        }
        if (!m_stalls.empty() && nowMs < m_stalls.back()) {
            m_stalls.clear();
        }
        while (!m_stalls.empty() && nowMs - m_stalls.front() > ObservationWindowMs) {
            m_stalls.pop_front();
        }
        m_stalls.push_back(nowMs);
        m_currentPeriodDirty = true;
        m_cleanPeriods = 0;
        if (!m_active && static_cast<int>(m_stalls.size()) >= ActivationStallCount) {
            m_active = true;
            return true;
        }
        return false;
    }

    PeriodTransition notePeriod(std::int64_t periodId)
    {
        if (periodId == m_lastPeriodId) {
            return PeriodTransition::None;
        }
        m_lastPeriodId = periodId;
        if (!m_active) {
            m_currentPeriodDirty = false;
            return PeriodTransition::None;
        }
        if (m_currentPeriodDirty) {
            m_cleanPeriods = 0;
        } else {
            ++m_cleanPeriods;
        }
        m_currentPeriodDirty = false;
        if (m_cleanPeriods < CleanPeriodsToRestore) {
            return PeriodTransition::None;
        }
        m_active = false;
        m_cleanPeriods = 0;
        m_stalls.clear();
        return PeriodTransition::Restored;
    }

    int adjustedThreadCount(int baseThreads, int decodeDepth) const
    {
        int const boundedBase = std::max(1, baseThreads);
        return m_active && decodeDepth >= 3
            ? std::max(1, boundedBase - 1)
            : boundedBase;
    }

    bool active() const { return m_active; }
    int cleanPeriods() const { return m_cleanPeriods; }
    int recentStallCount() const { return static_cast<int>(m_stalls.size()); }

private:
    std::deque<std::int64_t> m_stalls;
    std::int64_t m_lastPeriodId {std::numeric_limits<std::int64_t>::min()};
    int m_cleanPeriods {0};
    bool m_active {false};
    bool m_currentPeriodDirty {false};
};

inline int legacyPanadapterIntervalMs(bool deepDecodeActive,
                                      bool pressureActive,
                                      bool severePressureActive,
                                      bool gpuAccelerated = false,
                                      int requestedIntervalMs = 50)
{
    if (gpuAccelerated) {
        int const requested = std::clamp(requestedIntervalMs, 33, 66);
        if (severePressureActive) {
            return std::max(requested, 125);
        }
        if (pressureActive) {
            return std::max(requested, 66);
        }
        if (deepDecodeActive) {
            return std::max(requested, 50);
        }
        return requested;
    }

    if (severePressureActive) {
        return 250;
    }
    if (pressureActive) {
        return 180;
    }
    return deepDecodeActive ? 125 : 66;
}

} // namespace decode
} // namespace decodium
