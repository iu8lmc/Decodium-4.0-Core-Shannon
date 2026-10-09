// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvAudioRingBuffer.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

std::chrono::nanoseconds sampleDuration(std::size_t samples,
                                        std::uint32_t sampleRate) noexcept
{
    constexpr std::uint64_t nanosecondsPerSecond = 1'000'000'000ULL;
    const std::uint64_t count = static_cast<std::uint64_t>(samples);
    const std::uint64_t whole = count / sampleRate;
    const std::uint64_t remainder = count % sampleRate;

    const std::uint64_t maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (whole > maximum / nanosecondsPerSecond) {
        return std::chrono::nanoseconds::max();
    }
    const std::uint64_t duration = whole * nanosecondsPerSecond
        + remainder * nanosecondsPerSecond / sampleRate;
    if (duration > maximum) {
        return std::chrono::nanoseconds::max();
    }
    return std::chrono::nanoseconds {static_cast<std::int64_t>(duration)};
}

std::chrono::nanoseconds saturatingAdd(std::chrono::nanoseconds value,
                                       std::chrono::nanoseconds increment) noexcept
{
    if (increment.count() > 0
        && value.count() > std::chrono::nanoseconds::max().count() - increment.count()) {
        return std::chrono::nanoseconds::max();
    }
    return value + increment;
}

} // namespace

SstvAudioRingBuffer::SstvAudioRingBuffer(std::size_t maximumChunks,
                                         std::size_t maximumSamples)
    : m_maximumChunks(maximumChunks)
    , m_maximumSamples(maximumSamples)
{
    if (maximumChunks == 0U || maximumChunks > kMaximumChunkCapacity) {
        throw std::invalid_argument("invalid SSTV audio chunk capacity");
    }
    if (maximumSamples == 0U || maximumSamples > kMaximumSampleCapacity) {
        throw std::invalid_argument("invalid SSTV audio sample capacity");
    }
}

SstvAudioRingBuffer::~SstvAudioRingBuffer()
{
    cancel();
}

bool SstvAudioRingBuffer::push(SstvAudioChunk chunk)
{
    if (!metadataIsValid(chunk)) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        ++m_stats.rejectedChunks;
        return false;
    }

    const std::size_t originalSamples = chunk.samples.size();
    const std::size_t leadingDrop = originalSamples > m_maximumSamples
        ? originalSamples - m_maximumSamples
        : 0U;
    if (leadingDrop != 0U) {
        trimChunkFront(chunk, leadingDrop);
    }

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cancelled) {
            ++m_stats.rejectedChunks;
            return false;
        }

        while (m_chunks.size() >= m_maximumChunks) {
            dropOldestChunkLocked();
        }
        while (!m_chunks.empty()
               && m_queuedSamples > m_maximumSamples - chunk.samples.size()) {
            dropOldestChunkLocked();
        }

        const std::size_t retainedSamples = chunk.samples.size();
        m_chunks.push_back(std::move(chunk));
        m_stats.droppedSamples += static_cast<std::uint64_t>(leadingDrop);
        m_queuedSamples += retainedSamples;
        ++m_stats.enqueuedChunks;
        m_stats.enqueuedSamples += static_cast<std::uint64_t>(retainedSamples);
    }
    m_ready.notify_one();
    return true;
}

bool SstvAudioRingBuffer::tryPop(SstvAudioChunk& chunk)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_chunks.empty()) {
        return false;
    }
    popLocked(chunk);
    return true;
}

SstvAudioRingBuffer::WaitResult SstvAudioRingBuffer::waitPop(SstvAudioChunk& chunk)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const std::uint64_t observedGeneration = m_cancelGeneration;
    m_ready.wait(lock, [this, observedGeneration] {
        return m_cancelled || m_cancelGeneration != observedGeneration
            || !m_chunks.empty();
    });

    if (m_cancelled || m_cancelGeneration != observedGeneration) {
        return WaitResult::Cancelled;
    }
    popLocked(chunk);
    return WaitResult::Chunk;
}

void SstvAudioRingBuffer::cancel() noexcept
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_cancelled = true;
        ++m_cancelGeneration;
    }
    m_ready.notify_all();
}

void SstvAudioRingBuffer::restart() noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_cancelled = false;
}

bool SstvAudioRingBuffer::isCancelled() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_cancelled;
}

void SstvAudioRingBuffer::reset() noexcept
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_chunks.clear();
        m_queuedSamples = 0U;
        m_stats = {};
        m_cancelled = false;
        ++m_cancelGeneration;
    }
    m_ready.notify_all();
}

std::size_t SstvAudioRingBuffer::maximumChunks() const noexcept
{
    return m_maximumChunks;
}

std::size_t SstvAudioRingBuffer::maximumSamples() const noexcept
{
    return m_maximumSamples;
}

std::size_t SstvAudioRingBuffer::queuedChunks() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_chunks.size();
}

std::size_t SstvAudioRingBuffer::queuedSamples() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_queuedSamples;
}

SstvAudioRingBuffer::Stats SstvAudioRingBuffer::stats() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

bool SstvAudioRingBuffer::metadataIsValid(const SstvAudioChunk& chunk) noexcept
{
    return !chunk.samples.empty() && chunk.sampleRate >= kMinimumSampleRate
        && chunk.sampleRate <= kMaximumSampleRate;
}

void SstvAudioRingBuffer::trimChunkFront(SstvAudioChunk& chunk,
                                         std::size_t sampleCount)
{
    if (sampleCount == 0U) {
        return;
    }
    const auto firstRetained = chunk.samples.begin()
        + static_cast<std::ptrdiff_t>(sampleCount);
    std::vector<float> retained(firstRetained, chunk.samples.end());
    chunk.samples = std::move(retained);
    chunk.startTime = saturatingAdd(
        chunk.startTime, sampleDuration(sampleCount, chunk.sampleRate));
}

void SstvAudioRingBuffer::dropOldestChunkLocked() noexcept
{
    const std::size_t dropped = m_chunks.front().samples.size();
    m_queuedSamples -= dropped;
    ++m_stats.droppedChunks;
    m_stats.droppedSamples += static_cast<std::uint64_t>(dropped);
    m_chunks.pop_front();
}

void SstvAudioRingBuffer::popLocked(SstvAudioChunk& chunk) noexcept
{
    const std::size_t samples = m_chunks.front().samples.size();
    chunk = std::move(m_chunks.front());
    m_chunks.pop_front();
    m_queuedSamples -= samples;
    ++m_stats.dequeuedChunks;
    m_stats.dequeuedSamples += static_cast<std::uint64_t>(samples);
}

} // namespace decodium::sstv
