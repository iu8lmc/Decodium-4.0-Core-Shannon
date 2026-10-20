// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../rx/SstvAudioRingBuffer.h"

#include <QVector>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace decodium::sstv {

// Raw PCM ownership crosses the real-time-ish source callback boundary in
// this form.  Conversion to float, resampling, and every other DSP operation
// belong to the SSTV worker after it pops the chunk.
struct SstvPcm16Chunk final
{
    QVector<short> samples;
    SstvAudioSource source;
    std::uint32_t sampleRate {0U};
    std::chrono::nanoseconds startTime {0};
    std::uint64_t sequence {0U};
    std::uint64_t generation {0U};
};

// Multi-producer/single-or-multi-consumer bounded queue.  Chunk slots are
// allocated once by the constructor, so push() never grows the container.
// When either bound would be exceeded, complete oldest chunks are discarded;
// a block is never truncated because that would invalidate its timestamp.
class SstvPcm16Queue final
{
public:
    static constexpr std::size_t kHardMaximumChunks = 65'536U;
    static constexpr std::size_t kHardMaximumSamples = 12'000'000U;

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
        std::uint64_t clearedChunks {0U};
        std::uint64_t clearedSamples {0U};
        std::uint64_t rejectedChunks {0U};
        std::size_t queuedChunks {0U};
        std::size_t queuedSamples {0U};
        std::size_t waitingConsumers {0U};
        bool cancelled {false};
    };

    SstvPcm16Queue(std::size_t maximumChunks,
                   std::size_t maximumSamples);
    ~SstvPcm16Queue();

    SstvPcm16Queue(const SstvPcm16Queue&) = delete;
    SstvPcm16Queue& operator=(const SstvPcm16Queue&) = delete;
    SstvPcm16Queue(SstvPcm16Queue&&) = delete;
    SstvPcm16Queue& operator=(SstvPcm16Queue&&) = delete;

    bool push(SstvPcm16Chunk chunk);
    bool tryPop(SstvPcm16Chunk& chunk);
    WaitResult waitPop(SstvPcm16Chunk& chunk);

    // All lifecycle operations wake current waiters.  The wake epoch ensures
    // a cancel immediately followed by restart cannot be missed by a waiter
    // which has not reacquired the mutex yet.  cancel() and reset() clear all
    // old audio; restart() also starts with an empty queue.
    void cancel() noexcept;
    void restart() noexcept;
    void reset() noexcept;

    bool isCancelled() const noexcept;
    std::size_t maximumChunks() const noexcept;
    std::size_t maximumSamples() const noexcept;
    Stats stats() const noexcept;

private:
    static bool metadataIsValid(const SstvPcm16Chunk& chunk) noexcept;
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;

    void clearLocked() noexcept;
    void dropOldestLocked(bool backpressure) noexcept;
    void popLocked(SstvPcm16Chunk& chunk) noexcept;
    void advanceWakeEpochLocked() noexcept;

    const std::size_t m_maximumChunks;
    const std::size_t m_maximumSamples;

    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::vector<std::optional<SstvPcm16Chunk>> m_slots;
    std::size_t m_head {0U};
    std::size_t m_size {0U};
    std::size_t m_queuedSamples {0U};
    std::size_t m_waitingConsumers {0U};
    Stats m_stats;
    bool m_cancelled {false};
    std::uint64_t m_wakeEpoch {0U};
};

} // namespace decodium::sstv
