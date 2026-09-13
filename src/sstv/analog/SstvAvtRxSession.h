// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvAvt.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

struct SstvAvtRxSessionConfig final
{
    SstvAvtMode mode {SstvAvtMode::Avt24};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {6U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::uint32_t maximumInterpolationGapPixels {4U};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvAvtRxSessionState : std::uint8_t
{
    Receiving,
    Partial,
    Complete,
    Cancelled,
};

struct SstvAvtRxSessionUpdate final
{
    std::size_t inputObservations {0U};
    std::size_t decoderAcceptedObservations {0U};
    std::uint64_t linesPublished {0U};
    bool imageChanged {false};
    SstvAvtRxSessionState state {SstvAvtRxSessionState::Receiving};
};

struct SstvAvtRxSessionMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t inputObservations {0U};
    std::uint64_t decoderAcceptedObservations {0U};
    std::uint64_t linesPublished {0U};
    std::uint64_t discontinuities {0U};
    std::uint64_t finishCalls {0U};
    std::uint64_t cancelCalls {0U};
    std::uint64_t rejectedInputCalls {0U};
    std::uint64_t rejectedOversizeCalls {0U};
    std::uint64_t rejectedRegressions {0U};
    std::size_t peakInputObservations {0U};
};

// One countdown-acquired AVT image. The session has no line-sync fallback:
// any discontinuity closes the current frame as complete/partial, because
// continuing at a fabricated phase would silently corrupt all later colours.
class SstvAvtRxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvAvtDecoder::MaximumObservationsPerConsume;

    explicit SstvAvtRxSession(SstvAvtRxSessionConfig config = {});

    SstvAvtRxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvAvtRxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);
    SstvAvtRxSessionState notifyDiscontinuity(std::uint64_t nextSample);
    SstvAvtRxSessionState finish();
    void cancel() noexcept;

    SstvAvtMode mode() const noexcept;
    SstvAvtRxSessionState state() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvAvtDecoderMetrics decoderMetrics() const noexcept;
    SstvAvtRxSessionMetrics metrics() const noexcept;

private:
    static void validateConfig(const SstvAvtRxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    void updateState() noexcept;

    SstvAvtRxSessionConfig config_;
    SstvAvtDecoder decoder_;
    SstvAvtRxSessionMetrics metrics_;
    SstvAvtRxSessionState state_ {SstvAvtRxSessionState::Receiving};
    std::uint64_t lastInputSample_ {0U};
    std::uint64_t lastInputEndSample_ {0U};
    std::uint64_t lastInputSequence_ {0U};
    bool haveLastInputSample_ {false};
    bool haveLastInputSequence_ {false};
};

} // namespace decodium::sstv
