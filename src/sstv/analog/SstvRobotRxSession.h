// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvRobot.h"

#include "../dsp/SstvSyncTracker.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

struct SstvRobotRxSessionConfig final
{
    SstvRobotMode mode {SstvRobotMode::Colour36};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {6U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    // Keep the bounded Robot B/W 8 compatibility/tail guard in the live
    // audio runtime.  Tests and deterministic replay retain the canonical
    // image extent unless they explicitly opt in.
    bool preserveTerminalGuard {false};
    bool allowTerminalRowRecovery {false};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvRobotRxSessionState : std::uint8_t
{
    Receiving,
    Complete,
    Partial,
    Cancelled,
};

struct SstvRobotRxSessionUpdate final
{
    std::size_t inputObservations {0U};
    std::size_t decoderAcceptedObservations {0U};
    std::size_t observedLineSyncs {0U};
    std::size_t predictedLineSyncs {0U};
    std::uint64_t publishedLineRevision {0U};
    std::uint64_t linesPublished {0U};
    bool imageChanged {false};
    bool reachedImageEnd {false};
};

struct SstvRobotRxSessionMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t inputObservations {0U};
    std::uint64_t observationsBeforeImage {0U};
    std::uint64_t observationsAfterImage {0U};
    std::uint64_t syncObservations {0U};
    std::uint64_t syncEvents {0U};
    std::uint64_t observedLineSyncs {0U};
    std::uint64_t predictedLineSyncs {0U};
    std::uint64_t decoderAcceptedObservations {0U};
    std::uint64_t discontinuities {0U};
    std::uint64_t rejectedRegressions {0U};
    std::uint64_t finishCalls {0U};
    std::uint64_t cancelCalls {0U};
    std::size_t peakFilteredObservations {0U};
};

// Native Robot coordinator between the generic frequency demodulator,
// SstvSyncTracker and the bounded Robot image assembler.  Robot scanlines have
// a physical leading sync, so line zero is seeded at the VIS stop boundary.
class SstvRobotRxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvRobotDecoder::MaximumObservationsPerConsume;

    explicit SstvRobotRxSession(SstvRobotRxSessionConfig config);

    SstvRobotRxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvRobotRxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);

    SstvRobotRxSessionState notifyDiscontinuity(std::uint64_t nextSample);
    SstvRobotRxSessionState finish();
    void cancel() noexcept;

    SstvRobotRxSessionState state() const noexcept;
    SstvRobotMode mode() const noexcept;
    const SstvRobotRxSessionConfig& config() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvRobotRxSessionMetrics metrics() const noexcept;
    SstvSyncTrackerSnapshot syncSnapshot() const;
    SstvRobotDecoderMetrics decoderMetrics() const noexcept;

private:
    static void validateConfig(const SstvRobotRxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static SstvSyncTrackerConfig makeSyncConfig(
        const SstvRobotRxSessionConfig& config);

    void applySyncEvents(const std::vector<SstvSyncEvent>& events,
                         SstvRobotRxSessionUpdate* update);
    void refineBw8ImageEndFromObservedSync();
    void updateTerminalState(SstvRobotDecodeState decoderState) noexcept;

    SstvRobotRxSessionConfig config_;
    SstvRobotModeSpec spec_;
    SstvRobotMapper mapper_;
    SstvSyncTracker syncTracker_;
    SstvRobotDecoder decoder_;
    SstvRobotRxSessionMetrics metrics_;
    SstvRobotRxSessionState state_ {SstvRobotRxSessionState::Receiving};
    std::uint64_t canonicalImageEndSample_ {0U};
    std::uint64_t imageEndSample_ {0U};
    std::uint64_t initialSyncEndSample_ {0U};
    std::uint64_t lastSyncInputEndSample_ {0U};
    std::uint64_t lastInputSample_ {0U};
    std::uint64_t lastInputEndSample_ {0U};
    std::uint64_t lastInputSequence_ {0U};
    bool haveLastInputSample_ {false};
    bool haveLastInputSequence_ {false};
};

} // namespace decodium::sstv
