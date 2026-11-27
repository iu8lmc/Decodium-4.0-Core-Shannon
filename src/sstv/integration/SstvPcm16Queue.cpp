// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvPcm16Queue.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

std::size_t checkedChunkCapacity(std::size_t capacity)
{
    if (capacity == 0U
        || capacity > SstvPcm16Queue::kHardMaximumChunks) {
        throw std::invalid_argument("invalid SSTV PCM queue chunk capacity");
    }
    return capacity;
}

std::size_t checkedSampleCapacity(std::size_t capacity)
{
    if (capacity == 0U
        || capacity > SstvPcm16Queue::kHardMaximumSamples) {
        throw std::invalid_argument("invalid SSTV PCM queue sample capacity");
    }
    return capacity;
}

bool sourceKindIsValid(SstvAudioSourceKind kind) noexcept
{
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
    case SstvAudioSourceKind::LegacyBackend:
    case SstvAudioSourceKind::DecoPort:
    case SstvAudioSourceKind::Tci:
    case SstvAudioSourceKind::WebSdr:
    case SstvAudioSourceKind::RtlSdr:
    case SstvAudioSourceKind::Replay:
        return true;
    case SstvAudioSourceKind::Unknown:
        return false;
    }
    return false;
}

} // namespace

SstvPcm16Queue::SstvPcm16Queue(std::size_t maximumChunks,
                               std::size_t maximumSamples)
    : m_maximumChunks(checkedChunkCapacity(maximumChunks))
    , m_maximumSamples(checkedSampleCapacity(maximumSamples))
    , m_slots(m_maximumChunks)
{
}

SstvPcm16Queue::~SstvPcm16Queue()
{
    cancel();
}

bool SstvPcm16Queue::push(SstvPcm16Chunk chunk)
{
    const auto signedCount = chunk.samples.size();
    if (!metadataIsValid(chunk) || signedCount <= 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        saturatingAdd(m_stats.rejectedChunks);
        return false;
    }

    const auto sampleCount = static_cast<std::size_t>(signedCount);
    if (sampleCount > m_maximumSamples) {
        std::lock_guard<std::mutex> lock(m_mutex);
        saturatingAdd(m_stats.rejectedChunks);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cancelled) {
            saturatingAdd(m_stats.rejectedChunks);
            return false;
        }

        while (m_size != 0U
               && (m_size >= m_maximumChunks
                   || sampleCount > m_maximumSamples - m_queuedSamples)) {
            dropOldestLocked(true);
        }

        if (m_size >= m_maximumChunks
            || sampleCount > m_maximumSamples - m_queuedSamples) {
            saturatingAdd(m_stats.rejectedChunks);
            return false;
        }

        const std::size_t tail = (m_head + m_size) % m_maximumChunks;
        m_slots[tail].emplace(std::move(chunk));
        ++m_size;
        m_queuedSamples += sampleCount;
        saturatingAdd(m_stats.enqueuedChunks);
        saturatingAdd(m_stats.enqueuedSamples,
                      static_cast<std::uint64_t>(sampleCount));
    }

    m_ready.notify_one();
    return true;
}

bool SstvPcm16Queue::tryPop(SstvPcm16Chunk& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled || m_size == 0U) {
        return false;
    }
    popLocked(chunk);
    return true;
}

SstvPcm16Queue::WaitResult SstvPcm16Queue::waitPop(
    SstvPcm16Chunk& chunk)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const std::uint64_t observedEpoch = m_wakeEpoch;
    ++m_waitingConsumers;
    m_ready.wait(lock, [this, observedEpoch] {
        return m_cancelled || m_wakeEpoch != observedEpoch || m_size != 0U;
    });
    --m_waitingConsumers;

    if (m_cancelled || m_wakeEpoch != observedEpoch) {
        return WaitResult::Cancelled;
    }

    popLocked(chunk);
    return WaitResult::Chunk;
}

void SstvPcm16Queue::cancel() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        clearLocked();
        m_cancelled = true;
        advanceWakeEpochLocked();
    }
    m_ready.notify_all();
}

void SstvPcm16Queue::restart() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        clearLocked();
        m_cancelled = false;
        advanceWakeEpochLocked();
    }
    m_ready.notify_all();
}

void SstvPcm16Queue::reset() noexcept
{
    restart();
}

bool SstvPcm16Queue::isCancelled() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cancelled;
}

std::size_t SstvPcm16Queue::maximumChunks() const noexcept
{
    return m_maximumChunks;
}

std::size_t SstvPcm16Queue::maximumSamples() const noexcept
{
    return m_maximumSamples;
}

SstvPcm16Queue::Stats SstvPcm16Queue::stats() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Stats result = m_stats;
    result.queuedChunks = m_size;
    result.queuedSamples = m_queuedSamples;
    result.waitingConsumers = m_waitingConsumers;
    result.cancelled = m_cancelled;
    return result;
}

bool SstvPcm16Queue::metadataIsValid(
    const SstvPcm16Chunk& chunk) noexcept
{
    return sourceKindIsValid(chunk.source.kind)
        && chunk.sampleRate != 0U
        && chunk.startTime.count() >= 0
        && chunk.generation != 0U;
}

void SstvPcm16Queue::saturatingAdd(std::uint64_t& value,
                                   std::uint64_t increment) noexcept
{
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    if (increment > maximum - value) {
        value = maximum;
    } else {
        value += increment;
    }
}

void SstvPcm16Queue::clearLocked() noexcept
{
    while (m_size != 0U) {
        dropOldestLocked(false);
    }
}

void SstvPcm16Queue::dropOldestLocked(bool backpressure) noexcept
{
    auto& slot = m_slots[m_head];
    const auto sampleCount = static_cast<std::size_t>(slot->samples.size());

    if (backpressure) {
        saturatingAdd(m_stats.droppedChunks);
        saturatingAdd(m_stats.droppedSamples,
                      static_cast<std::uint64_t>(sampleCount));
    } else {
        saturatingAdd(m_stats.clearedChunks);
        saturatingAdd(m_stats.clearedSamples,
                      static_cast<std::uint64_t>(sampleCount));
    }

    m_queuedSamples -= sampleCount;
    slot.reset();
    m_head = (m_head + 1U) % m_maximumChunks;
    --m_size;
    if (m_size == 0U) {
        m_head = 0U;
    }
}

void SstvPcm16Queue::popLocked(SstvPcm16Chunk& chunk) noexcept
{
    auto& slot = m_slots[m_head];
    const auto sampleCount = static_cast<std::size_t>(slot->samples.size());
    chunk = std::move(*slot);

    m_queuedSamples -= sampleCount;
    slot.reset();
    m_head = (m_head + 1U) % m_maximumChunks;
    --m_size;
    if (m_size == 0U) {
        m_head = 0U;
    }

    saturatingAdd(m_stats.dequeuedChunks);
    saturatingAdd(m_stats.dequeuedSamples,
                  static_cast<std::uint64_t>(sampleCount));
}

void SstvPcm16Queue::advanceWakeEpochLocked() noexcept
{
    // Unsigned wrap is defined.  A waiter would have to span 2^64 complete
    // lifecycle transitions for an ABA, while every transition also performs
    // notify_all(); this keeps the normal cancel/restart race lossless without
    // allocating a token in a real-time-ish path.
    ++m_wakeEpoch;
}

} // namespace decodium::sstv
