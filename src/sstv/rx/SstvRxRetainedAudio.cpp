// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRxRetainedAudio.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr double MaximumFrequencyCorrectionHz = 150.0;
constexpr double MaximumSlantCorrectionPpm = 5'000.0;
constexpr std::size_t MaximumModeCharacters = 64U;
constexpr std::size_t MaximumFskIdCharacters = 32U;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000ULL;

std::chrono::nanoseconds saturatingAdd(std::chrono::nanoseconds left,
                                       std::chrono::nanoseconds right)
    noexcept
{
    if (right.count() > 0
        && left.count() > std::chrono::nanoseconds::max().count()
                              - right.count()) {
        return std::chrono::nanoseconds::max();
    }
    return left + right;
}

} // namespace

SstvRxRetainedAudio::SstvRxRetainedAudio()
    : SstvRxRetainedAudio(Config {})
{
}

SstvRxRetainedAudio::SstvRxRetainedAudio(Config config)
    : m_config(validate(config))
    , m_replay(std::chrono::seconds {m_config.retentionSeconds},
               m_config.sampleRate)
{
}

bool SstvRxRetainedAudio::append(SstvAudioChunk chunk)
{
    if (chunk.sampleRate != m_config.sampleRate) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        increment(m_metrics.chunksRejected);
        return false;
    }
    const bool accepted = m_replay.append(std::move(chunk));
    const std::lock_guard<std::mutex> lock(m_mutex);
    increment(accepted ? m_metrics.chunksAppended
                       : m_metrics.chunksRejected);
    return accepted;
}

bool SstvRxRetainedAudio::beginAcquisition(
    std::uint64_t acquisitionId,
    SstvAudioSource source,
    std::chrono::nanoseconds startTime)
{
    if (acquisitionId == 0U
        || source.kind == SstvAudioSourceKind::Unknown
        || startTime.count() < 0) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        increment(m_metrics.invalidAcquisitionUpdates);
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto duplicate = std::find_if(
        m_acquisitions.begin(), m_acquisitions.end(),
        [acquisitionId](const auto& acquisition) {
            return acquisition.acquisitionId == acquisitionId;
        });
    if (duplicate != m_acquisitions.end()) {
        increment(m_metrics.invalidAcquisitionUpdates);
        return false;
    }
    while (m_acquisitions.size()
           >= m_config.maximumAcquisitionDescriptors) {
        const auto closed = std::find_if(
            m_acquisitions.begin(), m_acquisitions.end(),
            [](const auto& acquisition) { return acquisition.closed; });
        if (closed == m_acquisitions.end()) {
            increment(m_metrics.invalidAcquisitionUpdates);
            return false;
        }
        m_acquisitions.erase(closed);
    }
    SstvRxRetainedAcquisition acquisition;
    acquisition.acquisitionId = acquisitionId;
    acquisition.source = source;
    acquisition.startTime = startTime;
    acquisition.endTime = startTime;
    m_acquisitions.push_back(std::move(acquisition));
    increment(m_metrics.acquisitionsStarted);
    m_metrics.peakAcquisitionDescriptors = std::max(
        m_metrics.peakAcquisitionDescriptors, m_acquisitions.size());
    return true;
}

bool SstvRxRetainedAudio::closeAcquisition(
    std::uint64_t acquisitionId,
    std::chrono::nanoseconds endTime,
    bool complete,
    std::string mode,
    std::string fskId,
    double frequencyCorrectionHz,
    double slantCorrectionPpm)
{
    if (acquisitionId == 0U
        || !validBoundedToken(mode, MaximumModeCharacters)
        || !validBoundedToken(fskId, MaximumFskIdCharacters)
        || !finiteCorrection(frequencyCorrectionHz,
                             MaximumFrequencyCorrectionHz)
        || !finiteCorrection(slantCorrectionPpm,
                             MaximumSlantCorrectionPpm)) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        increment(m_metrics.invalidAcquisitionUpdates);
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = std::find_if(
        m_acquisitions.begin(), m_acquisitions.end(),
        [acquisitionId](const auto& acquisition) {
            return acquisition.acquisitionId == acquisitionId;
        });
    if (found == m_acquisitions.end() || found->closed
        || endTime <= found->startTime) {
        increment(m_metrics.invalidAcquisitionUpdates);
        return false;
    }
    found->endTime = endTime;
    found->closed = true;
    found->complete = complete;
    found->mode = std::move(mode);
    found->fskId = std::move(fskId);
    found->frequencyCorrectionHz = frequencyCorrectionHz;
    found->slantCorrectionPpm = slantCorrectionPpm;
    increment(m_metrics.acquisitionsClosed);
    pruneDescriptorsLocked();
    return true;
}

bool SstvRxRetainedAudio::associateFskId(
    std::uint64_t acquisitionId,
    std::chrono::nanoseconds completedAt,
    std::string fskId)
{
    if (acquisitionId == 0U
        || completedAt.count() < 0
        || !validBoundedToken(fskId, MaximumFskIdCharacters)
        || fskId.empty()) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        increment(m_metrics.invalidAcquisitionUpdates);
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto found = std::find_if(
        m_acquisitions.begin(), m_acquisitions.end(),
        [acquisitionId](const auto& acquisition) {
            return acquisition.acquisitionId == acquisitionId;
        });
    if (found == m_acquisitions.end()
        || completedAt <= found->startTime) {
        increment(m_metrics.invalidAcquisitionUpdates);
        return false;
    }
    found->fskId = std::move(fskId);
    found->endTime = std::max(found->endTime, completedAt);
    return true;
}

std::optional<SstvRxRetainedAudioSnapshot>
SstvRxRetainedAudio::snapshotAcquisition(std::uint64_t acquisitionId)
{
    SstvRxRetainedAcquisition acquisition;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        const auto found = std::find_if(
            m_acquisitions.begin(), m_acquisitions.end(),
            [acquisitionId](const auto& candidate) {
                return candidate.acquisitionId == acquisitionId;
            });
        if (found == m_acquisitions.end()) {
            return std::nullopt;
        }
        acquisition = *found;
    }
    const std::chrono::nanoseconds end = acquisition.closed
        ? acquisition.endTime : std::chrono::nanoseconds::max();
    return snapshotRange(&acquisition, acquisition.startTime, end);
}

SstvRxRetainedAudioSnapshot SstvRxRetainedAudio::snapshotRecent()
{
    return snapshotRange(nullptr,
                         std::chrono::nanoseconds {0},
                         std::chrono::nanoseconds::max());
}

bool SstvRxRetainedAudio::setRetentionSeconds(std::uint32_t seconds)
{
    if (seconds < 5U || seconds > 600U) {
        return false;
    }
    try {
        m_replay.resize(std::chrono::seconds {seconds},
                        m_config.sampleRate);
    } catch (const std::exception&) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_config.retentionSeconds = seconds;
    pruneDescriptorsLocked();
    return true;
}

void SstvRxRetainedAudio::reset() noexcept
{
    m_replay.reset();
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_acquisitions.clear();
    m_metrics = {};
}

SstvRxRetainedAudio::Config SstvRxRetainedAudio::config() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

std::vector<SstvRxRetainedAcquisition>
SstvRxRetainedAudio::acquisitions() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<SstvRxRetainedAcquisition>(m_acquisitions.begin(),
                                                  m_acquisitions.end());
}

SstvRxRetainedAudioMetrics SstvRxRetainedAudio::metrics() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    SstvRxRetainedAudioMetrics result = m_metrics;
    result.replay = m_replay.stats();
    return result;
}

std::size_t SstvRxRetainedAudio::retainedSamples() const noexcept
{
    return m_replay.retainedSamples();
}

std::size_t SstvRxRetainedAudio::capacitySamples() const noexcept
{
    return m_replay.capacitySamples();
}

std::size_t SstvRxRetainedAudio::acquisitionDescriptorCount() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_acquisitions.size();
}

std::uint64_t SstvRxRetainedAudio::mostRecentAcquisitionId() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_acquisitions.empty() ? 0U
                                  : m_acquisitions.back().acquisitionId;
}

SstvRxRetainedAudio::Config SstvRxRetainedAudio::validate(Config config)
{
    if (config.sampleRate != 12'000U
        || config.retentionSeconds < 5U
        || config.retentionSeconds > 600U
        || config.maximumAcquisitionDescriptors == 0U
        || config.maximumAcquisitionDescriptors > 256U) {
        throw std::invalid_argument("invalid SSTV retained-audio config");
    }
    return config;
}

bool SstvRxRetainedAudio::finiteCorrection(double value,
                                           double maximum) noexcept
{
    return std::isfinite(value) && std::abs(value) <= maximum;
}

bool SstvRxRetainedAudio::validBoundedToken(
    const std::string& value,
    std::size_t maximumCharacters) noexcept
{
    if (value.size() > maximumCharacters) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char rawCharacter) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        return character >= 0x20U && character <= 0x7eU;
    });
}

std::chrono::nanoseconds SstvRxRetainedAudio::sampleDuration(
    std::size_t samples,
    std::uint32_t sampleRate) noexcept
{
    const std::uint64_t count = static_cast<std::uint64_t>(samples);
    const std::uint64_t whole = count / sampleRate;
    const std::uint64_t remainder = count % sampleRate;
    const std::uint64_t maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (whole > maximum / NanosecondsPerSecond) {
        return std::chrono::nanoseconds::max();
    }
    const std::uint64_t duration = whole * NanosecondsPerSecond
        + remainder * NanosecondsPerSecond / sampleRate;
    return duration > maximum
        ? std::chrono::nanoseconds::max()
        : std::chrono::nanoseconds {static_cast<std::int64_t>(duration)};
}

std::size_t SstvRxRetainedAudio::firstSampleAtOrAfter(
    std::chrono::nanoseconds delta,
    std::uint32_t sampleRate) noexcept
{
    if (delta.count() <= 0) {
        return 0U;
    }
    const long double scaled = static_cast<long double>(delta.count())
        * static_cast<long double>(sampleRate)
        / static_cast<long double>(NanosecondsPerSecond);
    const long double rounded = std::ceil(scaled);
    return rounded >= static_cast<long double>(
                          std::numeric_limits<std::size_t>::max())
        ? std::numeric_limits<std::size_t>::max()
        : static_cast<std::size_t>(rounded);
}

std::size_t SstvRxRetainedAudio::samplesBefore(
    std::chrono::nanoseconds delta,
    std::uint32_t sampleRate) noexcept
{
    if (delta.count() <= 0) {
        return 0U;
    }
    const long double scaled = static_cast<long double>(delta.count())
        * static_cast<long double>(sampleRate)
        / static_cast<long double>(NanosecondsPerSecond);
    const long double rounded = std::ceil(scaled);
    return rounded >= static_cast<long double>(
                          std::numeric_limits<std::size_t>::max())
        ? std::numeric_limits<std::size_t>::max()
        : static_cast<std::size_t>(rounded);
}

void SstvRxRetainedAudio::increment(std::uint64_t& value,
                                    std::uint64_t amount) noexcept
{
    if (amount > std::numeric_limits<std::uint64_t>::max() - value) {
        value = std::numeric_limits<std::uint64_t>::max();
    } else {
        value += amount;
    }
}

SstvRxRetainedAudioSnapshot SstvRxRetainedAudio::snapshotRange(
    const SstvRxRetainedAcquisition* acquisition,
    std::chrono::nanoseconds start,
    std::chrono::nanoseconds end)
{
    SstvRxRetainedAudioSnapshot result;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        result.sampleRate = m_config.sampleRate;
    }
    result.requestedStartTime = start;
    result.requestedEndTime = end;
    if (acquisition) {
        result.acquisitionId = acquisition->acquisitionId;
        result.source = acquisition->source;
        result.acquisitionClosed = acquisition->closed;
        result.acquisitionComplete = acquisition->complete;
        result.mode = acquisition->mode;
        result.fskId = acquisition->fskId;
        result.frequencyCorrectionHz = acquisition->frequencyCorrectionHz;
        result.slantCorrectionPpm = acquisition->slantCorrectionPpm;
    }

    const std::vector<SstvAudioChunk> chunks = m_replay.snapshot();
    result.chunks.reserve(chunks.size());
    for (const SstvAudioChunk& sourceChunk : chunks) {
        const std::chrono::nanoseconds chunkEnd = saturatingAdd(
            sourceChunk.startTime,
            sampleDuration(sourceChunk.samples.size(), sourceChunk.sampleRate));
        if (chunkEnd <= start || sourceChunk.startTime >= end) {
            continue;
        }
        std::size_t first = sourceChunk.startTime < start
            ? firstSampleAtOrAfter(start - sourceChunk.startTime,
                                   sourceChunk.sampleRate)
            : 0U;
        std::size_t last = chunkEnd > end
            ? samplesBefore(end - sourceChunk.startTime,
                            sourceChunk.sampleRate)
            : sourceChunk.samples.size();
        first = std::min(first, sourceChunk.samples.size());
        last = std::min(last, sourceChunk.samples.size());
        if (last <= first) {
            continue;
        }
        SstvAudioChunk retained = sourceChunk;
        retained.startTime = saturatingAdd(
            sourceChunk.startTime,
            sampleDuration(first, sourceChunk.sampleRate));
        retained.samples.assign(
            sourceChunk.samples.begin() + static_cast<std::ptrdiff_t>(first),
            sourceChunk.samples.begin() + static_cast<std::ptrdiff_t>(last));
        if (result.chunks.empty()) {
            result.retainedStartTime = retained.startTime;
            if (acquisition) {
                result.source = retained.source;
            }
        }
        result.retainedEndTime = saturatingAdd(
            retained.startTime,
            sampleDuration(retained.samples.size(), retained.sampleRate));
        result.sampleCount += retained.samples.size();
        result.chunks.push_back(std::move(retained));
    }
    if (!result.chunks.empty()) {
        result.truncatedAtStart = result.retainedStartTime > start;
        result.truncatedAtEnd = end != std::chrono::nanoseconds::max()
            && result.retainedEndTime < end;
    } else {
        result.truncatedAtStart = true;
        result.truncatedAtEnd = end != std::chrono::nanoseconds::max();
    }

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        increment(m_metrics.snapshotsCreated);
        if (result.chunks.empty()) {
            increment(m_metrics.emptySnapshots);
        }
        increment(m_metrics.samplesCopiedToSnapshots,
                  static_cast<std::uint64_t>(result.sampleCount));
    }
    return result;
}

void SstvRxRetainedAudio::pruneDescriptorsLocked() noexcept
{
    while (m_acquisitions.size()
           > m_config.maximumAcquisitionDescriptors) {
        const auto closed = std::find_if(
            m_acquisitions.begin(), m_acquisitions.end(),
            [](const auto& acquisition) { return acquisition.closed; });
        if (closed == m_acquisitions.end()) {
            break;
        }
        m_acquisitions.erase(closed);
    }
}

} // namespace decodium::sstv
