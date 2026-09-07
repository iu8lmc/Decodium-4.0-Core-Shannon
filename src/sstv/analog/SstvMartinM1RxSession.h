// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvMartinM1.h"

#include "../dsp/SstvSyncTracker.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace decodium::sstv {

struct SstvMartinM1RxSessionConfig final
{
    std::uint32_t sampleRate {12'000U};
    std::uint64_t imageStartSample {0U};
    std::uint32_t observationSpanSamples {6U};
    std::int32_t clockErrorPpm {0};
    double frequencyOffsetHz {0.0};
    double minimumObservationConfidence {0.20};
    std::size_t maximumPendingDirtyEvents {
        SstvImageFrame::kDefaultMaximumDirtyEvents};
    SstvMartinMode mode {SstvMartinMode::M1};
};

enum class SstvMartinM1RxSessionState : std::uint8_t
{
    Receiving,
    Complete,
    Partial,
    Cancelled,
};

struct SstvMartinM1RxSessionUpdate final
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

struct SstvMartinM1RxSessionMetrics final
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

// Martin-family coordinator between the generic frequency demodulator,
// SstvSyncTracker and the bounded image assembler.  The caller owns
// VIS/manual selection and supplies the absolute sample at which image line 0
// begins.  This class owns no PCM and performs no Qt/UI work.
class SstvMartinM1RxSession final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume =
        SstvMartinM1Decoder::MaximumObservationsPerConsume;

    explicit SstvMartinM1RxSession(
        SstvMartinM1RxSessionConfig config);

    SstvMartinM1RxSessionUpdate consume(
        const SstvFrequencyObservation* observations,
        std::size_t count);
    SstvMartinM1RxSessionUpdate consume(
        const std::vector<SstvFrequencyObservation>& observations);

    // A discontinuity preserves already published pixels but ends this
    // continuously clocked session as partial.  A higher-level replay
    // controller may start a new decode attempt from buffered audio.
    SstvMartinM1RxSessionState notifyDiscontinuity(
        std::uint64_t nextSample);
    SstvMartinM1RxSessionState finish();
    void cancel() noexcept;

    SstvMartinM1RxSessionState state() const noexcept;
    SstvMartinMode mode() const noexcept;
    const SstvMartinM1RxSessionConfig& config() const noexcept;
    std::uint64_t imageStartSample() const noexcept;
    std::uint64_t imageEndSample() const noexcept;
    const SstvImageFrame& imageFrame() const noexcept;
    SstvImageSnapshot snapshot() const;
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    SstvMartinM1RxSessionMetrics metrics() const noexcept;
    SstvSyncTrackerSnapshot syncSnapshot() const;
    SstvMartinM1DecoderMetrics decoderMetrics() const noexcept;

private:
    static void validateConfig(const SstvMartinM1RxSessionConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;
    static SstvSyncTrackerConfig makeSyncConfig(
        const SstvMartinM1RxSessionConfig& config);

    void applySyncEvents(const std::vector<SstvSyncEvent>& events,
                         SstvMartinM1RxSessionUpdate* update);
    void updateTerminalState(SstvMartinM1DecodeState decoderState) noexcept;

    SstvMartinM1RxSessionConfig config_;
    SstvMartinModeSpec spec_;
    SstvMartinM1Mapper mapper_;
    SstvSyncTracker syncTracker_;
    SstvMartinM1Decoder decoder_;
    SstvMartinM1RxSessionMetrics metrics_;
    SstvMartinM1RxSessionState state_ {
        SstvMartinM1RxSessionState::Receiving};
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
