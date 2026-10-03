// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvScottie.h"

#include "../dsp/SstvSyncTracker.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv {

struct SstvScottieRxSessionConfig final
{
    SstvScottieMode mode {SstvScottieMode::S1};
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {1U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
};

enum class SstvScottieRxSessionState : std::uint8_t
{
    Receiving,
    Complete,
    Partial,
    Cancelled,
};

struct SstvScottieRxSessionUpdate final
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

struct SstvScottieRxSessionMetrics final
{
    std::uint64_t consumeCalls {0U};
    std::uint64_t inputObservations {0U};
    std::uint64_t observationsBeforeImage {0U};
    std::uint64_t observationsAfterImage {0U};
    std::uint64_t syncObservations {0U};
    std::uint64_t syncEvents {0U};
    std::uint64_t observedLineSyncs {0U};
    std::uint64_t predictedLineSyncs {0U};
    std::uint64_t rejectedLineSyncs {0U};
    std::uint64_t decoderAcceptedObservations {0U};
    std::uint64_t pendingObservationsEvicted {0U};
    std::uint64_t discontinuities {0U};
    std::uint64_t rejectedRegressions {0U};
    std::uint64_t finishCalls {0U};
    std::uint64_t cancelCalls {0U};
    std::size_t pendingObservationCapacity {0U};
    std::size_t peakPendingObservations {0U};
    std::size_t peakFilteredObservations {0U};
};

// Streaming coordinator for Scottie S1/S2/S3/S4/DX.  Scottie sync is embedded
// after G and B, so this class retains one bounded row window until the
// physical sync supplies its anchor, then replays those pre-sync components.
// It never invents a line-leading pulse and never retains PCM or a full image
// waveform.
class SstvScottieRxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvScottieDecoder::MaximumObservationsPerConsume;
    static constexpr std::size_t MaximumPendingObservations = 32'768U;

    explicit SstvScottieRxSession(SstvScottieRxSessionConfig config);

    SstvScottieRxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvScottieRxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);

    SstvScottieRxSessionState notifyDiscontinuity(
        std::uint64_t nextSample);
    SstvScottieRxSessionState finish();
    void cancel() noexcept;

    SstvScottieRxSessionState state() const noexcept;
    const SstvScottieRxSessionConfig& config() const noexcept;
    SstvScottieMode mode() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvScottieRxSessionMetrics metrics() const noexcept;
    SstvSyncTrackerSnapshot syncSnapshot() const;
    SstvScottieDecoderMetrics decoderMetrics() const noexcept;

private:
    static void validateConfig(const SstvScottieRxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static SstvSyncTrackerConfig makeSyncConfig(
        const SstvScottieRxSessionConfig& config,
        const SstvScottieMapper& mapper);

    void retainPending(const std::vector<SstvFrequencyObservation>& input);
    std::size_t drainPending();
    void applySyncEvents(const std::vector<SstvSyncEvent>& events,
                         SstvScottieRxSessionUpdate* update);
    bool physicalLineForEvent(const SstvSyncEvent& event,
                              std::uint32_t& line);
    void updateTerminalState(SstvScottieDecodeState decoderState) noexcept;

    SstvScottieRxSessionConfig config_;
    SstvScottieModeSpec spec_;
    SstvScottieMapper mapper_;
    SstvSyncTracker syncTracker_;
    SstvScottieDecoder decoder_;
    SstvScottieRxSessionMetrics metrics_;
    std::vector<SstvFrequencyObservation> pending_;
    SstvScottieRxSessionState state_ {
        SstvScottieRxSessionState::Receiving};
    std::uint64_t imageEndSample_ {0U};
    std::uint64_t lastInputSample_ {0U};
    std::uint64_t lastInputSequence_ {0U};
    std::uint64_t lastSyncInputEndSample_ {0U};
    std::uint64_t decodeThroughSample_ {0U};
    std::uint64_t trackerLineOffset_ {0U};
    std::size_t pendingCapacity_ {0U};
    bool haveLastInputSample_ {false};
    bool haveLastInputSequence_ {false};
    bool haveDecodeThroughSample_ {false};
    bool haveTrackerLineOffset_ {false};
};

} // namespace decodium::sstv
