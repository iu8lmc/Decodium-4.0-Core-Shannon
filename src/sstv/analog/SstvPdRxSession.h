// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvPd.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

struct SstvPdRxSessionConfig final
{
    SstvPdMode mode {SstvPdMode::Pd50};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {3U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvPdRxSessionState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvPdRxSessionUpdate final
{
    std::size_t inputObservations {0U};
    std::size_t decoderAcceptedObservations {0U};
    std::uint64_t observedPairSyncs {0U};
    std::uint64_t linePairsPublished {0U};
    std::uint64_t linesPublished {0U};
    bool imageChanged {false};
    SstvPdRxSessionState state {SstvPdRxSessionState::Receiving};
};

struct SstvPdRxSessionMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t inputObservations {0U};
    std::uint64_t decoderAcceptedObservations {0U};
    std::uint64_t observedPairSyncs {0U};
    std::uint64_t linePairsPublished {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t discontinuities {0U};
    std::uint64_t finishCalls {0U};
    std::uint64_t cancelCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t rejectedRegressions {0U};
    std::size_t peakInputObservations {0U};
};

// One VIS-acquired PD image session. The VIS stop boundary anchors the first
// 20 ms pair sync. PCM remains in the shared runtime; only bounded frequency
// observations and four scans for the current row pair are retained here.
class SstvPdRxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvPdDecoder::MaximumObservationsPerConsume;

    explicit SstvPdRxSession(SstvPdRxSessionConfig config = {});

    SstvPdRxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvPdRxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvPdRxSessionState notifyDiscontinuity(std::uint64_t nextSample);
    SstvPdRxSessionState finish();
    void cancel() noexcept;

    SstvPdMode mode() const noexcept;
    SstvPdRxSessionState state() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvPdDecoderMetrics decoderMetrics() const noexcept;
    SstvPdRxSessionMetrics metrics() const noexcept;

private:
    static void validateConfig(const SstvPdRxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    void updateState() noexcept;

    SstvPdRxSessionConfig config_;
    SstvPdDecoder decoder_;
    SstvPdRxSessionMetrics metrics_;
    SstvPdRxSessionState state_ {SstvPdRxSessionState::Receiving};
    std::uint64_t lastInputSample_ {0U};
    std::uint64_t lastInputEndSample_ {0U};
    std::uint64_t lastInputSequence_ {0U};
    bool haveLastInputSample_ {false};
    bool haveLastInputSequence_ {false};
};

} // namespace decodium::sstv
