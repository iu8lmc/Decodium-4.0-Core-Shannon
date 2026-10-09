// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvReplayBuffer.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace decodium::sstv {

struct SstvRxRetainedAcquisition final
{
    std::uint64_t acquisitionId {0U};
    SstvAudioSource source;
    std::chrono::nanoseconds startTime {0};
    std::chrono::nanoseconds endTime {0};
    bool closed {false};
    bool complete {false};
    std::string mode;
    std::string fskId;
    double frequencyCorrectionHz {0.0};
    double slantCorrectionPpm {0.0};
};

struct SstvRxRetainedAudioSnapshot final
{
    std::uint64_t acquisitionId {0U};
    SstvAudioSource source;
    std::uint32_t sampleRate {0U};
    std::chrono::nanoseconds requestedStartTime {0};
    std::chrono::nanoseconds requestedEndTime {0};
    std::chrono::nanoseconds retainedStartTime {0};
    std::chrono::nanoseconds retainedEndTime {0};
    bool acquisitionClosed {false};
    bool acquisitionComplete {false};
    bool truncatedAtStart {false};
    bool truncatedAtEnd {false};
    std::string mode;
    std::string fskId;
    double frequencyCorrectionHz {0.0};
    double slantCorrectionPpm {0.0};
    std::size_t sampleCount {0U};
    std::vector<SstvAudioChunk> chunks;
};

struct SstvRxRetainedAudioMetrics final
{
    std::uint64_t chunksAppended {0U};
    std::uint64_t chunksRejected {0U};
    std::uint64_t acquisitionsStarted {0U};
    std::uint64_t acquisitionsClosed {0U};
    std::uint64_t invalidAcquisitionUpdates {0U};
    std::uint64_t snapshotsCreated {0U};
    std::uint64_t emptySnapshots {0U};
    std::uint64_t samplesCopiedToSnapshots {0U};
    std::size_t peakAcquisitionDescriptors {0U};
    SstvReplayBuffer::Stats replay;
};

// Worker-side normalized 12 kHz retention.  PCM ownership is delegated to the
// existing hard-bounded SstvReplayBuffer; this class adds only a small bounded
// acquisition index and range snapshots used by async re-decode/raw-WAV jobs.
class SstvRxRetainedAudio final
{
public:
    struct Config final
    {
        std::uint32_t sampleRate {12'000U};
        std::uint32_t retentionSeconds {180U};
        std::size_t maximumAcquisitionDescriptors {32U};
    };

    SstvRxRetainedAudio();
    explicit SstvRxRetainedAudio(Config config);

    SstvRxRetainedAudio(const SstvRxRetainedAudio&) = delete;
    SstvRxRetainedAudio& operator=(const SstvRxRetainedAudio&) = delete;

    bool append(SstvAudioChunk chunk);
    bool beginAcquisition(std::uint64_t acquisitionId,
                          SstvAudioSource source,
                          std::chrono::nanoseconds startTime);
    bool closeAcquisition(std::uint64_t acquisitionId,
                          std::chrono::nanoseconds endTime,
                          bool complete,
                          std::string mode,
                          std::string fskId,
                          double frequencyCorrectionHz,
                          double slantCorrectionPpm);
    // FSK-ID normally follows the image.  Extend a closed descriptor to the
    // identifier boundary without reopening it, so raw export/re-decode keeps
    // the associated tail while the PCM remains owned by the same ring.
    bool associateFskId(std::uint64_t acquisitionId,
                        std::chrono::nanoseconds completedAt,
                        std::string fskId);

    std::optional<SstvRxRetainedAudioSnapshot> snapshotAcquisition(
        std::uint64_t acquisitionId);
    SstvRxRetainedAudioSnapshot snapshotRecent();

    // A retention change preserves newest PCM through SstvReplayBuffer.  A
    // sample-rate change is deliberately unsupported because this store is
    // the post-resampler native clock; construct a new pipeline instead.
    bool setRetentionSeconds(std::uint32_t seconds);
    void reset() noexcept;

    Config config() const noexcept;
    std::vector<SstvRxRetainedAcquisition> acquisitions() const;
    SstvRxRetainedAudioMetrics metrics() const noexcept;
    std::size_t retainedSamples() const noexcept;
    std::size_t capacitySamples() const noexcept;
    std::size_t acquisitionDescriptorCount() const noexcept;
    std::uint64_t mostRecentAcquisitionId() const noexcept;

private:
    static Config validate(Config config);
    static bool finiteCorrection(double value, double maximum) noexcept;
    static bool validBoundedToken(const std::string& value,
                                  std::size_t maximumCharacters) noexcept;
    static std::chrono::nanoseconds sampleDuration(
        std::size_t samples,
        std::uint32_t sampleRate) noexcept;
    static std::size_t firstSampleAtOrAfter(
        std::chrono::nanoseconds delta,
        std::uint32_t sampleRate) noexcept;
    static std::size_t samplesBefore(
        std::chrono::nanoseconds delta,
        std::uint32_t sampleRate) noexcept;
    static void increment(std::uint64_t& value,
                          std::uint64_t amount = 1U) noexcept;
    SstvRxRetainedAudioSnapshot snapshotRange(
        const SstvRxRetainedAcquisition* acquisition,
        std::chrono::nanoseconds start,
        std::chrono::nanoseconds end);
    void pruneDescriptorsLocked() noexcept;

    mutable std::mutex m_mutex;
    Config m_config;
    SstvReplayBuffer m_replay;
    std::deque<SstvRxRetainedAcquisition> m_acquisitions;
    SstvRxRetainedAudioMetrics m_metrics;
};

} // namespace decodium::sstv
