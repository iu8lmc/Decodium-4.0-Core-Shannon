// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvReplayBuffer.h"

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

SstvReplayBuffer::SstvReplayBuffer(std::chrono::seconds retention,
                                   std::uint32_t sampleRate)
    : m_retention(retention)
    , m_sampleRate(sampleRate)
    , m_capacitySamples(checkedCapacity(retention, sampleRate))
{
}

bool SstvReplayBuffer::append(SstvAudioChunk chunk)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (chunk.samples.empty() || chunk.sampleRate != m_sampleRate) {
        ++m_stats.rejectedChunks;
        return false;
    }

    const std::size_t originalSamples = chunk.samples.size();
    const std::size_t leadingDrop = originalSamples > m_capacitySamples
        ? originalSamples - m_capacitySamples
        : 0U;
    if (leadingDrop != 0U) {
        trimChunkFront(chunk, leadingDrop);
    }

    if (!m_chunks.empty() && chunk.startTime < m_chunks.back().startTime) {
        ++m_stats.rejectedChunks;
        return false;
    }

    const std::size_t retainedSamples = chunk.samples.size();
    m_chunks.push_back(std::move(chunk));
    ++m_stats.appendedChunks;
    m_stats.appendedSamples += static_cast<std::uint64_t>(retainedSamples);
    m_stats.evictedSamples += static_cast<std::uint64_t>(leadingDrop);
    m_retainedSamples += retainedSamples;
    evictToCapacityLocked();
    return true;
}

std::vector<SstvAudioChunk> SstvReplayBuffer::snapshot() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<SstvAudioChunk>(m_chunks.begin(), m_chunks.end());
}

void SstvReplayBuffer::resize(std::chrono::seconds retention,
                              std::uint32_t sampleRate)
{
    const std::size_t capacity = checkedCapacity(retention, sampleRate);
    const std::lock_guard<std::mutex> lock(m_mutex);

    if (sampleRate != m_sampleRate) {
        m_stats.evictedChunks += static_cast<std::uint64_t>(m_chunks.size());
        m_stats.evictedSamples += static_cast<std::uint64_t>(m_retainedSamples);
        m_chunks.clear();
        m_retainedSamples = 0U;
    }
    m_retention = retention;
    m_sampleRate = sampleRate;
    m_capacitySamples = capacity;
    evictToCapacityLocked();
}

void SstvReplayBuffer::reset() noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_chunks.clear();
    m_retainedSamples = 0U;
    m_stats = {};
}

std::chrono::seconds SstvReplayBuffer::retention() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_retention;
}

std::uint32_t SstvReplayBuffer::sampleRate() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_sampleRate;
}

std::size_t SstvReplayBuffer::capacitySamples() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_capacitySamples;
}

std::size_t SstvReplayBuffer::retainedSamples() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_retainedSamples;
}

std::size_t SstvReplayBuffer::retainedChunks() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_chunks.size();
}

SstvReplayBuffer::Stats SstvReplayBuffer::stats() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

std::size_t SstvReplayBuffer::checkedCapacity(std::chrono::seconds retention,
                                              std::uint32_t sampleRate)
{
    if (retention.count() <= 0 || retention.count() > kMaximumRetentionSeconds) {
        throw std::invalid_argument("invalid SSTV replay retention");
    }
    if (sampleRate < kMinimumSampleRate || sampleRate > kMaximumSampleRate) {
        throw std::invalid_argument("invalid SSTV replay sample rate");
    }

    const auto seconds = static_cast<std::uint64_t>(retention.count());
    const auto rate = static_cast<std::uint64_t>(sampleRate);
    if (seconds > static_cast<std::uint64_t>(kMaximumRetainedSamples) / rate) {
        throw std::length_error("SSTV replay capacity exceeds its hard bound");
    }
    return static_cast<std::size_t>(seconds * rate);
}

void SstvReplayBuffer::trimChunkFront(SstvAudioChunk& chunk,
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

void SstvReplayBuffer::evictToCapacityLocked()
{
    while (m_retainedSamples > m_capacitySamples && !m_chunks.empty()) {
        const std::size_t excess = m_retainedSamples - m_capacitySamples;
        const std::size_t oldestSize = m_chunks.front().samples.size();
        if (oldestSize <= excess) {
            m_retainedSamples -= oldestSize;
            ++m_stats.evictedChunks;
            m_stats.evictedSamples += static_cast<std::uint64_t>(oldestSize);
            m_chunks.pop_front();
            continue;
        }

        trimChunkFront(m_chunks.front(), excess);
        m_retainedSamples -= excess;
        m_stats.evictedSamples += static_cast<std::uint64_t>(excess);
    }
}

} // namespace decodium::sstv
