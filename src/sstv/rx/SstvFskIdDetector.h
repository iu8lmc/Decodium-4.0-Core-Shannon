// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../core/SstvFskIdCodec.h"
#include "../dsp/SstvToneDetector.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace decodium::sstv {

enum class SstvFskIdDetectorState : std::uint8_t
{
    SearchingLeader,
    ReadingPreamble,
    ReadingStartBit,
    ReadingFrameBits,
};

enum class SstvFskIdCandidateEnd : std::uint8_t
{
    ChecksumReceived,
    MalformedHeader,
    FrameLimit,
    EndOfStream,
};

struct SstvFskIdDetectorConfig final
{
    std::uint32_t sampleRate {12'000U};

    // These spans are acquisition tolerances, not replacement protocol
    // constants.  The transmitted envelope remains the exact 300/100/22 ms
    // plan declared by SstvFskIdCodec.  Windowed tone observations do not
    // cover the first and last half-window, hence the deliberately shorter
    // minimum leader span.
    std::uint32_t minimumLeaderObservedMicroseconds {220'000U};
    std::uint32_t minimumPreambleMicroseconds {55'000U};
    std::uint32_t maximumPreambleMicroseconds {160'000U};
    std::uint32_t minimumStartBitMicroseconds {8'000U};
    std::uint32_t maximumStartBitMicroseconds {55'000U};
    std::uint32_t maximumObservationGapMicroseconds {66'000U};
    double maximumNominalToneErrorHz {25.0};
    SstvFskIdCodec::TextPolicy textPolicy {
        SstvFskIdCodec::TextPolicy::PermittedText};
};

struct SstvFskIdDetectorMetrics final
{
    std::uint64_t observationsConsumed {0U};
    std::uint64_t validToneObservations {0U};
    std::uint64_t invalidToneObservations {0U};
    std::uint64_t nonMonotonicObservations {0U};
    std::uint64_t gapResets {0U};
    std::uint64_t leaderCandidates {0U};
    std::uint64_t qualifiedLeaders {0U};
    std::uint64_t rejectedPreambles {0U};
    std::uint64_t rejectedStartBits {0U};
    std::uint64_t bitsCompleted {0U};
    std::uint64_t invalidBits {0U};
    std::uint64_t candidatesEmitted {0U};
    std::uint64_t validIdentifiers {0U};
    std::uint64_t malformedIdentifiers {0U};
    std::size_t peakBufferedBits {0U};
};

struct SstvFskIdCandidate final
{
    SstvFskIdCandidateEnd end {
        SstvFskIdCandidateEnd::FrameLimit};
    SstvFskIdCodec::DecodeResult decoded;

    // wireBits excludes the logical 0x20 search marker because the on-air
    // envelope represents it as a timed preamble/start marker.  decoded.rawBits
    // includes that reconstructed logical symbol for the framing codec.
    std::vector<SstvFskIdCodec::Bit> wireBits;
    std::vector<double> bitConfidences;
    double confidence {0.0};
    std::uint64_t leaderStartSample {0U};
    std::uint64_t frameDataStartSample {0U};
    std::uint64_t completedAtSample {0U};
    bool logicalStartSymbolReconstructed {true};

    bool valid() const noexcept
    {
        return decoded.valid();
    }
};

// Streaming FSK-ID envelope and bit detector.  It consumes the bounded tone
// observations already produced by SstvToneDetector; it neither retains PCM
// nor performs work on an audio callback.  Candidate storage is capped by the
// interoperable nine-character limit in SstvFskIdCodec.  Observations belong
// to one stream and both sequence and centreSample must increase strictly; a
// discontinuity resets acquisition and the current observation starts a new
// search rather than being discarded.
class SstvFskIdDetector final
{
public:
    static constexpr std::size_t MaximumObservationsPerConsume = 8'192U;
    static constexpr std::size_t MaximumBodySymbols =
        1U + SstvFskIdCodec::kMaximumTextLength + 1U + 1U;
    static constexpr std::size_t MaximumWireBits =
        MaximumBodySymbols * SstvFskIdCodec::kBitsPerSymbol;

    explicit SstvFskIdDetector(SstvFskIdDetectorConfig config = {});

    std::vector<SstvFskIdCandidate> consume(
        const SstvToneObservation* observations,
        std::size_t count);
    std::vector<SstvFskIdCandidate> consume(
        const std::vector<SstvToneObservation>& observations);

    // Finalises a bounded pending candidate for file/replay EOF.  Live
    // monitoring normally leaves the detector streaming and never calls it.
    std::optional<SstvFskIdCandidate> finishPending();

    void reset() noexcept;
    SstvFskIdDetectorState state() const noexcept;
    const SstvFskIdDetectorConfig& config() const noexcept;
    const SstvFskIdDetectorMetrics& metrics() const noexcept;
    std::size_t bufferedBitCount() const noexcept;

private:
    enum class ClassifiedTone : std::uint8_t
    {
        Leader,
        Zero,
        One,
        Other,
    };

    struct BitVote final
    {
        double zeroWeight {0.0};
        double oneWeight {0.0};
        double confidenceSum {0.0};
        std::uint32_t validCount {0U};
    };

    static void validateConfig(const SstvFskIdDetectorConfig& config);
    static void saturatingAdd(std::uint64_t& value,
                              std::uint64_t increment = 1U) noexcept;

    ClassifiedTone classify(const SstvToneObservation& observation) const
        noexcept;
    std::uint64_t samplesForMicroseconds(std::uint32_t microseconds) const
        noexcept;
    std::uint64_t transitionBoundary(std::uint64_t previous,
                                     std::uint64_t current) const noexcept;

    void processObservation(const SstvToneObservation& observation,
                            ClassifiedTone tone,
                            std::vector<SstvFskIdCandidate>& output);
    void processSearching(const SstvToneObservation& observation,
                          ClassifiedTone tone);
    void processPreamble(const SstvToneObservation& observation,
                         ClassifiedTone tone);
    void processStartBit(const SstvToneObservation& observation,
                         ClassifiedTone tone);
    void processFrameBit(const SstvToneObservation& observation,
                         ClassifiedTone tone,
                         std::vector<SstvFskIdCandidate>& output);

    void addVote(const SstvToneObservation& observation,
                 ClassifiedTone tone) noexcept;
    void completeCurrentBit(std::uint64_t completedAtSample,
                            std::vector<SstvFskIdCandidate>& output);
    bool inspectCompletedSymbol(std::uint64_t completedAtSample,
                                std::vector<SstvFskIdCandidate>& output);
    SstvFskIdCandidate makeCandidate(SstvFskIdCandidateEnd end,
                                     std::uint64_t completedAtSample) const;
    void emitCandidate(SstvFskIdCandidateEnd end,
                       std::uint64_t completedAtSample,
                       std::vector<SstvFskIdCandidate>& output);
    void returnToSearch() noexcept;
    void clearBitVote() noexcept;

    SstvFskIdDetectorConfig config_;
    SstvFskIdDetectorMetrics metrics_;
    SstvFskIdDetectorState state_ {
        SstvFskIdDetectorState::SearchingLeader};

    std::uint64_t bitSamples_ {0U};
    std::uint64_t minimumLeaderSpanSamples_ {0U};
    std::uint64_t minimumPreambleSamples_ {0U};
    std::uint64_t maximumPreambleSamples_ {0U};
    std::uint64_t minimumStartBitSamples_ {0U};
    std::uint64_t maximumStartBitSamples_ {0U};
    std::uint64_t maximumObservationGapSamples_ {0U};

    std::uint64_t lastObservationSample_ {0U};
    std::uint64_t lastObservationSequence_ {0U};
    bool haveLastObservation_ {false};
    std::uint64_t leaderStartSample_ {0U};
    std::uint64_t lastLeaderSample_ {0U};
    bool haveLeaderRun_ {false};
    bool leaderQualified_ {false};
    std::uint64_t preambleStartSample_ {0U};
    std::uint64_t lastPreambleSample_ {0U};
    std::uint64_t startBitStartSample_ {0U};
    std::uint64_t lastStartBitSample_ {0U};
    std::uint64_t frameDataStartSample_ {0U};
    std::size_t currentBitIndex_ {0U};

    BitVote currentVote_;
    std::vector<SstvFskIdCodec::Bit> wireBits_;
    std::vector<double> bitConfidences_;
    std::optional<std::size_t> terminatorSymbolIndex_;
};

} // namespace decodium::sstv
