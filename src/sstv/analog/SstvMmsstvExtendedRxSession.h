// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvMmsstvExtended.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

struct SstvMmsstvRxSessionConfig final
{
    SstvMmsstvMode mode {SstvMmsstvMode::Mp73};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {3U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvMmsstvRxSessionState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvMmsstvRxSessionUpdate final
{
    std::size_t inputObservations {0U};
    std::size_t decoderAcceptedObservations {0U};
    std::uint64_t observedScanSyncs {0U};
    std::uint64_t scansPublished {0U};
    std::uint64_t linesPublished {0U};
    bool imageChanged {false};
    SstvMmsstvRxSessionState state {SstvMmsstvRxSessionState::Receiving};
};

struct SstvMmsstvRxSessionMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t inputObservations {0U};
    std::uint64_t decoderAcceptedObservations {0U};
    std::uint64_t observedScanSyncs {0U};
    std::uint64_t scansPublished {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t discontinuities {0U};
    std::uint64_t finishCalls {0U};
    std::uint64_t cancelCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t rejectedRegressions {0U};
    std::size_t peakInputObservations {0U};
};

// One VIS/N-VIS-acquired image.  PCM stays in SstvRxRuntime; this session
// retains only bounded frequency observations and one scan's accumulators.
class SstvMmsstvRxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvMmsstvDecoder::MaximumObservationsPerConsume;

    explicit SstvMmsstvRxSession(SstvMmsstvRxSessionConfig config = {});

    SstvMmsstvRxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvMmsstvRxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvMmsstvRxSessionState notifyDiscontinuity(std::uint64_t nextSample);
    SstvMmsstvRxSessionState finish();
    void cancel() noexcept;

    SstvMmsstvMode mode() const noexcept;
    SstvMmsstvRxSessionState state() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvMmsstvDecoderMetrics decoderMetrics() const noexcept;
    SstvMmsstvRxSessionMetrics metrics() const noexcept;

private:
    static void validateConfig(const SstvMmsstvRxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    void updateState() noexcept;

    SstvMmsstvRxSessionConfig config_;
    SstvMmsstvDecoder decoder_;
    SstvMmsstvRxSessionMetrics metrics_;
    SstvMmsstvRxSessionState state_ {SstvMmsstvRxSessionState::Receiving};
    std::uint64_t lastInputSample_ {0U};
    std::uint64_t lastInputEndSample_ {0U};
    std::uint64_t lastInputSequence_ {0U};
    bool haveLastInputSample_ {false};
    bool haveLastInputSequence_ {false};
};

} // namespace decodium::sstv
