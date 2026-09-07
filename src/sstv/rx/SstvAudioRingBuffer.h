// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace decodium::sstv {

// Stable, allocation-free source identity carried from Decodium's common RX
// PCM fanout into the SSTV worker.  streamId distinguishes simultaneous
// instances of the same backend without putting strings on the audio callback.
enum class SstvAudioSourceKind : std::uint8_t
{
    Unknown = 0,
    LocalSoundCard,
    DecoPort,
    Tci,
    WebSdr,
    RtlSdr,
    LegacyBackend,
    Replay,
};

struct SstvAudioSource final
{
    SstvAudioSourceKind kind {SstvAudioSourceKind::Unknown};
    std::uint32_t streamId {0U};
};

constexpr bool operator==(const SstvAudioSource& left,
                          const SstvAudioSource& right) noexcept
{
    return left.kind == right.kind && left.streamId == right.streamId;
}

constexpr bool operator!=(const SstvAudioSource& left,
                          const SstvAudioSource& right) noexcept
{
    return !(left == right);
}

struct SstvAudioChunk final
{
    SstvAudioSource source;
    std::uint32_t sampleRate {0U};
    std::chrono::nanoseconds startTime {0};
    std::uint64_t sequence {0U};
    std::vector<float> samples;
};

class SstvAudioRingBuffer final
{
public:
    static constexpr std::size_t kMaximumChunkCapacity = 65'536U;
    static constexpr std::size_t kMaximumSampleCapacity = 12'000'000U;
    static constexpr std::uint32_t kMinimumSampleRate = 1'000U;
    static constexpr std::uint32_t kMaximumSampleRate = 384'000U;

    enum class WaitResult : std::uint8_t
    {
        Chunk,
        Cancelled,
    };

    struct Stats final
    {
        std::uint64_t enqueuedChunks {0U};
        std::uint64_t enqueuedSamples {0U};
        std::uint64_t dequeuedChunks {0U};
        std::uint64_t dequeuedSamples {0U};
        std::uint64_t droppedChunks {0U};
        std::uint64_t droppedSamples {0U};
        std::uint64_t rejectedChunks {0U};
    };

    SstvAudioRingBuffer(std::size_t maximumChunks,
                        std::size_t maximumSamples);
    ~SstvAudioRingBuffer();

    SstvAudioRingBuffer(const SstvAudioRingBuffer&) = delete;
    SstvAudioRingBuffer& operator=(const SstvAudioRingBuffer&) = delete;
    SstvAudioRingBuffer(SstvAudioRingBuffer&&) = delete;
    SstvAudioRingBuffer& operator=(SstvAudioRingBuffer&&) = delete;

    // Only validation, bounded chunk eviction, one move, and a wake-up happen
    // here.  DSP/resampling belongs to the consumer thread.  Empty or invalid
    // chunks and pushes made while cancelled return false.
    bool push(SstvAudioChunk chunk);
    bool tryPop(SstvAudioChunk& chunk);
    WaitResult waitPop(SstvAudioChunk& chunk);

    // cancel() wakes all current/future waiters and rejects pushes until
    // restart().  A generation counter makes a fast cancel/reset cycle visible
    // even if a waiter has not reacquired the mutex yet.
    void cancel() noexcept;
    void restart() noexcept;
    bool isCancelled() const noexcept;

    // Clears queued audio and statistics, reopens pushes, and cancels waiters
    // that were already blocked at the time of the reset.
    void reset() noexcept;

    std::size_t maximumChunks() const noexcept;
    std::size_t maximumSamples() const noexcept;
    std::size_t queuedChunks() const noexcept;
    std::size_t queuedSamples() const noexcept;
    Stats stats() const noexcept;

private:
    static bool metadataIsValid(const SstvAudioChunk& chunk) noexcept;
    static void trimChunkFront(SstvAudioChunk& chunk,
                               std::size_t sampleCount);
    void dropOldestChunkLocked() noexcept;
    void popLocked(SstvAudioChunk& chunk) noexcept;

    const std::size_t m_maximumChunks;
    const std::size_t m_maximumSamples;

    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::deque<SstvAudioChunk> m_chunks;
    std::size_t m_queuedSamples {0U};
    Stats m_stats;
    bool m_cancelled {false};
    std::uint64_t m_cancelGeneration {0U};
};

} // namespace decodium::sstv
