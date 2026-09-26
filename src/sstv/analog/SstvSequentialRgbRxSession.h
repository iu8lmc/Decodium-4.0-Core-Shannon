// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvSequentialRgb.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

struct SstvSequentialRgbRxSessionConfig final
{
    SstvSequentialRgbMode mode {SstvSequentialRgbMode::WraaseSc2_60};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {3U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvSequentialRgbRxSessionState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvSequentialRgbRxSessionUpdate final
{
    std::size_t inputObservations {0U};
    std::size_t decoderAcceptedObservations {0U};
    std::uint64_t observedLineSyncs {0U};
    std::uint64_t linesPublished {0U};
    bool imageChanged {false};
    SstvSequentialRgbRxSessionState state {
        SstvSequentialRgbRxSessionState::Receiving};
};

struct SstvSequentialRgbRxSessionMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t inputObservations {0U};
    std::uint64_t decoderAcceptedObservations {0U};
    std::uint64_t observedLineSyncs {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t discontinuities {0U};
    std::uint64_t finishCalls {0U};
    std::uint64_t cancelCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t rejectedRegressions {0U};
    std::size_t peakInputObservations {0U};
};

// One VIS-acquired image session.  The first line-leading sync begins exactly
// at imageStartSample.  No PCM is retained and every consume call is bounded.
class SstvSequentialRgbRxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvSequentialRgbDecoder::MaximumObservationsPerConsume;

    explicit SstvSequentialRgbRxSession(
        SstvSequentialRgbRxSessionConfig config = {});

    SstvSequentialRgbRxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvSequentialRgbRxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvSequentialRgbRxSessionState notifyDiscontinuity(
        std::uint64_t nextSample);
    SstvSequentialRgbRxSessionState finish();
    void cancel() noexcept;

    SstvSequentialRgbMode mode() const noexcept;
    SstvSequentialRgbRxSessionState state() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvSequentialRgbDecoderMetrics decoderMetrics() const noexcept;
    SstvSequentialRgbRxSessionMetrics metrics() const noexcept;

private:
    static void validateConfig(const SstvSequentialRgbRxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    void updateState() noexcept;

    SstvSequentialRgbRxSessionConfig config_;
    SstvSequentialRgbDecoder decoder_;
    SstvSequentialRgbRxSessionMetrics metrics_;
    SstvSequentialRgbRxSessionState state_ {
        SstvSequentialRgbRxSessionState::Receiving};
    std::uint64_t lastInputSample_ {0U};
    std::uint64_t lastInputEndSample_ {0U};
    std::uint64_t lastInputSequence_ {0U};
    bool haveLastInputSample_ {false};
    bool haveLastInputSequence_ {false};
};

} // namespace decodium::sstv
