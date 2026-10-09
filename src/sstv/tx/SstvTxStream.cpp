// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTxStream.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace decodium::sstv {
namespace {

bool isKnownRole(SstvTxSegmentRole role) noexcept
{
    switch (role) {
    case SstvTxSegmentRole::Header:
    case SstvTxSegmentRole::Vis:
    case SstvTxSegmentRole::Image:
    case SstvTxSegmentRole::FskId:
    case SstvTxSegmentRole::Tail:
        return true;
    }
    return false;
}

} // namespace

SstvTxStream::SstvTxStream(std::uint32_t sampleRate,
                           std::vector<SstvToneSegment> plan,
                           double headroom)
    : m_generator(sampleRate, headroom)
{
    validateAndLoadPlan(std::move(plan));
    rebuildSchedule();
}

std::uint32_t SstvTxStream::sampleRate() const noexcept
{
    return m_generator.sampleRate();
}

std::size_t SstvTxStream::plannedSegmentCount() const noexcept
{
    return m_segments.size();
}

Picoseconds SstvTxStream::totalDuration() const noexcept
{
    return m_totalDuration;
}

std::uint64_t SstvTxStream::totalSamples() const noexcept
{
    return m_totalSamples;
}

std::uint64_t SstvTxStream::producedSamples() const noexcept
{
    return m_producedSamples;
}

std::uint64_t SstvTxStream::remainingSamples() const noexcept
{
    return m_totalSamples - m_producedSamples;
}

double SstvTxStream::progress() const noexcept
{
    if (m_totalSamples == 0U) {
        return 1.0;
    }
    return static_cast<double>(
        static_cast<long double>(m_producedSamples)
        / static_cast<long double>(m_totalSamples));
}

bool SstvTxStream::complete() const noexcept
{
    return m_segmentIndex >= m_segments.size();
}

bool SstvTxStream::cancelled() const noexcept
{
    return m_generator.cancelled();
}

std::optional<SstvTxSegmentCursor> SstvTxStream::currentSegment() const noexcept
{
    if (complete()) {
        return std::nullopt;
    }

    const RuntimeSegment& segment = m_segments[m_segmentIndex];
    return SstvTxSegmentCursor {
        m_segmentIndex,
        segment.definition.role,
        segment.definition.frequencyHz,
        segment.definition.duration,
        segment.sampleCount,
        m_segmentOffset
    };
}

std::size_t SstvTxStream::pullFloat(float* output, std::size_t capacity)
{
    return pull(output, capacity);
}

std::size_t SstvTxStream::pullPcm16(std::int16_t* output,
                                    std::size_t capacity)
{
    return pull(output, capacity);
}

const SstvToneMetrics& SstvTxStream::metrics() const noexcept
{
    return m_generator.metrics();
}

void SstvTxStream::cancel() noexcept
{
    m_generator.cancel();
}

void SstvTxStream::reset()
{
    rebuildSchedule();
}

void SstvTxStream::validateAndLoadPlan(std::vector<SstvToneSegment> plan)
{
    std::int64_t duration = 0;
    m_segments.reserve(plan.size());

    for (auto& segment : plan) {
        if (segment.duration.count < 0) {
            throw std::invalid_argument(
                "SSTV TX segment duration must not be negative");
        }
        if (!isKnownRole(segment.role)) {
            throw std::invalid_argument("unknown SSTV TX segment role");
        }
        m_generator.validateTone(segment.frequencyHz, segment.level);

        if (segment.duration.count
            > std::numeric_limits<std::int64_t>::max() - duration) {
            throw std::overflow_error("SSTV TX plan duration overflow");
        }
        duration += segment.duration.count;
        m_segments.push_back({std::move(segment), 0});
    }

    m_totalDuration = Picoseconds {duration};
}

void SstvTxStream::rebuildSchedule()
{
    m_generator.reset();
    std::uint64_t total = 0;

    for (RuntimeSegment& segment : m_segments) {
        const std::uint64_t sampleCount =
            m_generator.samplesForDuration(segment.definition.duration);
        if (sampleCount > std::numeric_limits<std::uint64_t>::max() - total) {
            throw std::overflow_error("SSTV TX plan sample count overflow");
        }
        total += sampleCount;
        segment.sampleCount = sampleCount;
    }

    m_totalSamples = total;
    m_producedSamples = 0;
    m_segmentIndex = 0;
    m_segmentOffset = 0;
    skipCompletedSegments();
}

void SstvTxStream::skipCompletedSegments() noexcept
{
    while (m_segmentIndex < m_segments.size()
           && m_segmentOffset >= m_segments[m_segmentIndex].sampleCount) {
        ++m_segmentIndex;
        m_segmentOffset = 0;
    }
}

template<typename OutputSample>
std::size_t SstvTxStream::pull(OutputSample* output, std::size_t capacity)
{
    static_assert(std::is_same_v<OutputSample, float>
                      || std::is_same_v<OutputSample, std::int16_t>,
                  "unsupported SSTV stream output type");

    if (capacity != 0U && output == nullptr) {
        throw std::invalid_argument("SSTV TX pull buffer must not be null");
    }
    if (capacity == 0U || complete() || cancelled()) {
        return 0;
    }

    std::size_t produced = 0;
    while (produced < capacity && !complete() && !cancelled()) {
        RuntimeSegment& segment = m_segments[m_segmentIndex];
        const std::uint64_t available =
            segment.sampleCount - m_segmentOffset;
        const std::size_t requested = static_cast<std::size_t>(
            std::min<std::uint64_t>(
                available,
                static_cast<std::uint64_t>(capacity - produced)));

        std::size_t rendered = 0;
        if constexpr (std::is_same_v<OutputSample, float>) {
            rendered = m_generator.generateFloat(
                segment.definition.frequencyHz,
                segment.definition.level,
                output + produced,
                requested);
        } else {
            rendered = m_generator.generatePcm16(
                segment.definition.frequencyHz,
                segment.definition.level,
                output + produced,
                requested);
        }

        produced += rendered;
        m_producedSamples += rendered;
        m_segmentOffset += rendered;

        if (m_segmentOffset == segment.sampleCount) {
            ++m_segmentIndex;
            m_segmentOffset = 0;
            skipCompletedSegments();
        }

        // A short render means cancellation arrived during this chunk.
        if (rendered != requested) {
            break;
        }
    }

    return produced;
}

} // namespace decodium::sstv
