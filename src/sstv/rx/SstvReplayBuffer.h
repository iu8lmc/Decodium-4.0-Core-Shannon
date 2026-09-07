// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvAudioRingBuffer.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace decodium::sstv {

// Bounded worker-side history used to replay the most recent normalized PCM
// after VIS/mode detection.  Retention is sample-duration based, not wall-clock
// based, so scheduler stalls and timestamp gaps cannot grow memory usage.
class SstvReplayBuffer final
{
public:
    static constexpr std::uint32_t kMinimumSampleRate = 1'000U;
    static constexpr std::uint32_t kMaximumSampleRate = 192'000U;
    static constexpr std::int64_t kMaximumRetentionSeconds = 600;
    static constexpr std::size_t kMaximumRetainedSamples = 12'000'000U;

    struct Stats final
    {
        std::uint64_t appendedChunks {0U};
        std::uint64_t appendedSamples {0U};
        std::uint64_t evictedChunks {0U};
        std::uint64_t evictedSamples {0U};
        std::uint64_t rejectedChunks {0U};
    };

    SstvReplayBuffer(std::chrono::seconds retention,
                     std::uint32_t sampleRate);

    SstvReplayBuffer(const SstvReplayBuffer&) = delete;
    SstvReplayBuffer& operator=(const SstvReplayBuffer&) = delete;
    SstvReplayBuffer(SstvReplayBuffer&&) = delete;
    SstvReplayBuffer& operator=(SstvReplayBuffer&&) = delete;

    // Accepts non-empty chunks at the configured rate and in non-decreasing
    // timestamp order.  An oversized chunk retains only its newest samples.
    bool append(SstvAudioChunk chunk);

    // Returns independent chunks in oldest-to-newest order.  Copy size is
    // bounded by capacitySamples(); callers should request snapshots off the
    // realtime audio callback.
    std::vector<SstvAudioChunk> snapshot() const;

    // A rate change clears history because existing sample durations cannot be
    // reinterpreted.  A retention-only change preserves the newest samples.
    void resize(std::chrono::seconds retention, std::uint32_t sampleRate);
    void reset() noexcept;

    std::chrono::seconds retention() const noexcept;
    std::uint32_t sampleRate() const noexcept;
    std::size_t capacitySamples() const noexcept;
    std::size_t retainedSamples() const noexcept;
    std::size_t retainedChunks() const noexcept;
    Stats stats() const noexcept;

private:
    static std::size_t checkedCapacity(std::chrono::seconds retention,
                                       std::uint32_t sampleRate);
    static void trimChunkFront(SstvAudioChunk& chunk,
                               std::size_t sampleCount);
    void evictToCapacityLocked();

    mutable std::mutex m_mutex;
    std::chrono::seconds m_retention;
    std::uint32_t m_sampleRate {0U};
    std::size_t m_capacitySamples {0U};
    std::size_t m_retainedSamples {0U};
    std::deque<SstvAudioChunk> m_chunks;
    Stats m_stats;
};

} // namespace decodium::sstv
