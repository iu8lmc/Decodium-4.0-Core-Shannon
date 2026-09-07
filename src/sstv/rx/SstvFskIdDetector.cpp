// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvFskIdDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

constexpr std::uint64_t kMicrosecondsPerSecond = 1'000'000U;

bool isFiniteUnit(double value) noexcept
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

} // namespace

SstvFskIdDetector::SstvFskIdDetector(SstvFskIdDetectorConfig config)
    : config_(config)
{
    validateConfig(config_);
    bitSamples_ = samplesForMicroseconds(
        SstvFskIdCodec::kBitDurationMicroseconds);
    minimumLeaderSpanSamples_ = samplesForMicroseconds(
        config_.minimumLeaderObservedMicroseconds);
    minimumPreambleSamples_ = samplesForMicroseconds(
        config_.minimumPreambleMicroseconds);
    maximumPreambleSamples_ = samplesForMicroseconds(
        config_.maximumPreambleMicroseconds);
    minimumStartBitSamples_ = samplesForMicroseconds(
        config_.minimumStartBitMicroseconds);
    maximumStartBitSamples_ = samplesForMicroseconds(
        config_.maximumStartBitMicroseconds);
    maximumObservationGapSamples_ = samplesForMicroseconds(
        config_.maximumObservationGapMicroseconds);
    wireBits_.reserve(MaximumWireBits);
    bitConfidences_.reserve(MaximumWireBits);
}

std::vector<SstvFskIdCandidate> SstvFskIdDetector::consume(
    const SstvToneObservation* observations,
    std::size_t count)
{
    if (count != 0U && observations == nullptr) {
        throw std::invalid_argument(
            "null SSTV FSK-ID observation buffer");
    }
    if (count > MaximumObservationsPerConsume) {
        throw std::length_error(
            "SSTV FSK-ID observation chunk exceeds public limit");
    }

    std::vector<SstvFskIdCandidate> output;
    output.reserve(std::min<std::size_t>(count / 32U + 1U, 64U));

    for (std::size_t index = 0U; index < count; ++index) {
        const SstvToneObservation& observation = observations[index];
        saturatingAdd(metrics_.observationsConsumed);

        if (haveLastObservation_
            && (observation.centreSample <= lastObservationSample_
                || observation.sequence <= lastObservationSequence_)) {
            saturatingAdd(metrics_.nonMonotonicObservations);
            returnToSearch();
            haveLastObservation_ = false;
        }

        if (haveLastObservation_
            && observation.centreSample - lastObservationSample_
                > maximumObservationGapSamples_) {
            if (state_ != SstvFskIdDetectorState::SearchingLeader
                || haveLeaderRun_) {
                saturatingAdd(metrics_.gapResets);
            }
            returnToSearch();
        }

        const ClassifiedTone tone = classify(observation);
        if (tone == ClassifiedTone::Other) {
            saturatingAdd(metrics_.invalidToneObservations);
        } else {
            saturatingAdd(metrics_.validToneObservations);
        }

        processObservation(observation, tone, output);
        lastObservationSample_ = observation.centreSample;
        lastObservationSequence_ = observation.sequence;
        haveLastObservation_ = true;
    }
    return output;
}

std::vector<SstvFskIdCandidate> SstvFskIdDetector::consume(
    const std::vector<SstvToneObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

std::optional<SstvFskIdCandidate> SstvFskIdDetector::finishPending()
{
    if (state_ != SstvFskIdDetectorState::ReadingFrameBits
        || (wireBits_.empty() && currentVote_.validCount == 0U)) {
        returnToSearch();
        return std::nullopt;
    }

    // A current vote exists precisely because no observation has crossed the
    // next nominal bit boundary yet.  It is therefore partial evidence, even
    // when it contains a high-confidence window.  Do not fabricate a complete
    // bit at EOF: doing so could turn the first window of a truncated checksum
    // bit into a valid identifier.  Completed cells remain available in the
    // raw diagnostics and decode as truncated/missing framing as appropriate.
    std::vector<SstvFskIdCandidate> output;
    emitCandidate(SstvFskIdCandidateEnd::EndOfStream,
                  lastObservationSample_,
                  output);
    return std::move(output.front());
}

void SstvFskIdDetector::reset() noexcept
{
    metrics_ = {};
    haveLastObservation_ = false;
    lastObservationSample_ = 0U;
    lastObservationSequence_ = 0U;
    returnToSearch();
}

SstvFskIdDetectorState SstvFskIdDetector::state() const noexcept
{
    return state_;
}

const SstvFskIdDetectorConfig& SstvFskIdDetector::config() const noexcept
{
    return config_;
}

const SstvFskIdDetectorMetrics& SstvFskIdDetector::metrics() const noexcept
{
    return metrics_;
}

std::size_t SstvFskIdDetector::bufferedBitCount() const noexcept
{
    return wireBits_.size();
}

void SstvFskIdDetector::validateConfig(
    const SstvFskIdDetectorConfig& config)
{
    if (config.sampleRate < 8'000U || config.sampleRate > 384'000U) {
        throw std::invalid_argument(
            "unsupported SSTV FSK-ID detector sample rate");
    }
    if (config.minimumLeaderObservedMicroseconds == 0U
        || config.minimumPreambleMicroseconds == 0U
        || config.minimumPreambleMicroseconds
            >= config.maximumPreambleMicroseconds
        || config.minimumStartBitMicroseconds == 0U
        || config.minimumStartBitMicroseconds
            >= config.maximumStartBitMicroseconds
        || config.maximumObservationGapMicroseconds
            < config.maximumStartBitMicroseconds) {
        throw std::invalid_argument(
            "invalid SSTV FSK-ID acquisition timing bounds");
    }
    if (!std::isfinite(config.maximumNominalToneErrorHz)
        || config.maximumNominalToneErrorHz < 0.0
        || config.maximumNominalToneErrorHz >= 100.0) {
        throw std::invalid_argument(
            "invalid SSTV FSK-ID tone tolerance");
    }
    switch (config.textPolicy) {
    case SstvFskIdCodec::TextPolicy::Callsign:
    case SstvFskIdCodec::TextPolicy::PermittedText:
        break;
    default:
        throw std::invalid_argument(
            "invalid SSTV FSK-ID text policy");
    }
}

void SstvFskIdDetector::saturatingAdd(std::uint64_t& value,
                                      std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvFskIdDetector::ClassifiedTone SstvFskIdDetector::classify(
    const SstvToneObservation& observation) const noexcept
{
    if (!observation.valid()
        || !std::isfinite(observation.nominalFrequencyHz)
        || !isFiniteUnit(observation.confidence)) {
        return ClassifiedTone::Other;
    }

    const auto matches = [this, &observation](double target) noexcept {
        return std::abs(observation.nominalFrequencyHz - target)
            <= config_.maximumNominalToneErrorHz;
    };
    if (matches(SstvFskIdCodec::kLeaderFrequencyHz)) {
        return ClassifiedTone::Leader;
    }
    if (matches(SstvFskIdCodec::kZeroFrequencyHz)) {
        return ClassifiedTone::Zero;
    }
    if (matches(SstvFskIdCodec::kOneFrequencyHz)) {
        return ClassifiedTone::One;
    }
    return ClassifiedTone::Other;
}

std::uint64_t SstvFskIdDetector::samplesForMicroseconds(
    std::uint32_t microseconds) const noexcept
{
    const std::uint64_t numerator =
        static_cast<std::uint64_t>(config_.sampleRate) * microseconds;
    return (numerator + kMicrosecondsPerSecond - 1U)
        / kMicrosecondsPerSecond;
}

std::uint64_t SstvFskIdDetector::transitionBoundary(
    std::uint64_t previous,
    std::uint64_t current) const noexcept
{
    return previous + (current - previous) / 2U;
}

void SstvFskIdDetector::processObservation(
    const SstvToneObservation& observation,
    ClassifiedTone tone,
    std::vector<SstvFskIdCandidate>& output)
{
    switch (state_) {
    case SstvFskIdDetectorState::SearchingLeader:
        processSearching(observation, tone);
        break;
    case SstvFskIdDetectorState::ReadingPreamble:
        processPreamble(observation, tone);
        break;
    case SstvFskIdDetectorState::ReadingStartBit:
        processStartBit(observation, tone);
        break;
    case SstvFskIdDetectorState::ReadingFrameBits:
        processFrameBit(observation, tone, output);
        break;
    }
}

void SstvFskIdDetector::processSearching(
    const SstvToneObservation& observation,
    ClassifiedTone tone)
{
    if (tone == ClassifiedTone::Leader) {
        if (!haveLeaderRun_
            || observation.centreSample - lastLeaderSample_
                > maximumObservationGapSamples_) {
            leaderStartSample_ = observation.centreSample;
            haveLeaderRun_ = true;
            leaderQualified_ = false;
            saturatingAdd(metrics_.leaderCandidates);
        }
        lastLeaderSample_ = observation.centreSample;
        if (!leaderQualified_
            && lastLeaderSample_ - leaderStartSample_
                >= minimumLeaderSpanSamples_) {
            leaderQualified_ = true;
            saturatingAdd(metrics_.qualifiedLeaders);
        }
        return;
    }

    if (tone == ClassifiedTone::Zero && haveLeaderRun_
        && leaderQualified_) {
        preambleStartSample_ = transitionBoundary(
            lastLeaderSample_, observation.centreSample);
        lastPreambleSample_ = observation.centreSample;
        state_ = SstvFskIdDetectorState::ReadingPreamble;
        return;
    }

    // A Hann/Goertzel window centred on a tone transition is commonly
    // ambiguous.  Preserve a qualified leader across that bounded transition
    // window so the first clean 2100 Hz preamble observation can acquire it.
    if (tone == ClassifiedTone::Other && haveLeaderRun_
        && observation.centreSample - lastLeaderSample_
            <= maximumObservationGapSamples_) {
        return;
    }

    haveLeaderRun_ = false;
    leaderQualified_ = false;
}

void SstvFskIdDetector::processPreamble(
    const SstvToneObservation& observation,
    ClassifiedTone tone)
{
    if (tone == ClassifiedTone::Zero) {
        lastPreambleSample_ = observation.centreSample;
        if (lastPreambleSample_ - preambleStartSample_
            > maximumPreambleSamples_) {
            saturatingAdd(metrics_.rejectedPreambles);
            returnToSearch();
        }
        return;
    }

    if (tone == ClassifiedTone::One) {
        const std::uint64_t boundary = transitionBoundary(
            lastPreambleSample_, observation.centreSample);
        const std::uint64_t duration = boundary - preambleStartSample_;
        if (duration >= minimumPreambleSamples_
            && duration <= maximumPreambleSamples_) {
            startBitStartSample_ = boundary;
            lastStartBitSample_ = observation.centreSample;
            state_ = SstvFskIdDetectorState::ReadingStartBit;
            return;
        }
        saturatingAdd(metrics_.rejectedPreambles);
        returnToSearch();
        return;
    }

    if (tone == ClassifiedTone::Leader) {
        returnToSearch();
        processSearching(observation, tone);
        return;
    }

    if (observation.centreSample - preambleStartSample_
        > maximumPreambleSamples_) {
        saturatingAdd(metrics_.rejectedPreambles);
        returnToSearch();
    }
}

void SstvFskIdDetector::processStartBit(
    const SstvToneObservation& observation,
    ClassifiedTone tone)
{
    if (tone == ClassifiedTone::One) {
        lastStartBitSample_ = observation.centreSample;
        if (lastStartBitSample_ - startBitStartSample_
            > maximumStartBitSamples_) {
            saturatingAdd(metrics_.rejectedStartBits);
            returnToSearch();
        }
        return;
    }

    if (tone == ClassifiedTone::Zero) {
        const std::uint64_t boundary = transitionBoundary(
            lastStartBitSample_, observation.centreSample);
        const std::uint64_t duration = boundary - startBitStartSample_;
        if (duration < minimumStartBitSamples_
            || duration > maximumStartBitSamples_) {
            saturatingAdd(metrics_.rejectedStartBits);
            returnToSearch();
            return;
        }

        frameDataStartSample_ = boundary;
        currentBitIndex_ = 0U;
        wireBits_.clear();
        bitConfidences_.clear();
        terminatorSymbolIndex_.reset();
        clearBitVote();
        state_ = SstvFskIdDetectorState::ReadingFrameBits;
        std::vector<SstvFskIdCandidate> unused;
        processFrameBit(observation, tone, unused);
        return;
    }

    if (tone == ClassifiedTone::Leader) {
        returnToSearch();
        processSearching(observation, tone);
        return;
    }

    if (observation.centreSample - startBitStartSample_
        > maximumStartBitSamples_) {
        saturatingAdd(metrics_.rejectedStartBits);
        returnToSearch();
    }
}

void SstvFskIdDetector::processFrameBit(
    const SstvToneObservation& observation,
    ClassifiedTone tone,
    std::vector<SstvFskIdCandidate>& output)
{
    if (observation.centreSample < frameDataStartSample_) {
        return;
    }
    const std::uint64_t relative =
        observation.centreSample - frameDataStartSample_;
    const std::uint64_t observedBit64 = relative / bitSamples_;
    if (observedBit64 > MaximumWireBits) {
        emitCandidate(SstvFskIdCandidateEnd::FrameLimit,
                      observation.centreSample,
                      output);
        return;
    }
    const std::size_t observedBit =
        static_cast<std::size_t>(observedBit64);

    while (state_ == SstvFskIdDetectorState::ReadingFrameBits
           && currentBitIndex_ < observedBit) {
        completeCurrentBit(observation.centreSample, output);
        if (state_ != SstvFskIdDetectorState::ReadingFrameBits) {
            return;
        }
        ++currentBitIndex_;
        clearBitVote();
    }

    if (state_ != SstvFskIdDetectorState::ReadingFrameBits) {
        return;
    }
    if (currentBitIndex_ >= MaximumWireBits) {
        emitCandidate(SstvFskIdCandidateEnd::FrameLimit,
                      observation.centreSample,
                      output);
        return;
    }
    addVote(observation, tone);
}

void SstvFskIdDetector::addVote(
    const SstvToneObservation& observation,
    ClassifiedTone tone) noexcept
{
    if (tone != ClassifiedTone::Zero && tone != ClassifiedTone::One) {
        return;
    }
    const double weight = std::max(observation.confidence, 1.0e-9);
    if (tone == ClassifiedTone::Zero) {
        currentVote_.zeroWeight += weight;
    } else {
        currentVote_.oneWeight += weight;
    }
    currentVote_.confidenceSum += observation.confidence;
    if (currentVote_.validCount
        != std::numeric_limits<std::uint32_t>::max()) {
        ++currentVote_.validCount;
    }
}

void SstvFskIdDetector::completeCurrentBit(
    std::uint64_t completedAtSample,
    std::vector<SstvFskIdCandidate>& output)
{
    SstvFskIdCodec::Bit bit = 2U;
    double confidence = 0.0;
    const double totalWeight =
        currentVote_.zeroWeight + currentVote_.oneWeight;
    if (currentVote_.validCount != 0U && totalWeight > 0.0
        && currentVote_.zeroWeight != currentVote_.oneWeight) {
        const bool one = currentVote_.oneWeight > currentVote_.zeroWeight;
        bit = one ? 1U : 0U;
        const double winningWeight = one
            ? currentVote_.oneWeight
            : currentVote_.zeroWeight;
        const double meanObservationConfidence =
            currentVote_.confidenceSum
            / static_cast<double>(currentVote_.validCount);
        confidence = std::clamp(
            meanObservationConfidence * winningWeight / totalWeight,
            0.0,
            1.0);
    } else {
        saturatingAdd(metrics_.invalidBits);
    }

    wireBits_.push_back(bit);
    bitConfidences_.push_back(confidence);
    saturatingAdd(metrics_.bitsCompleted);
    metrics_.peakBufferedBits = std::max(
        metrics_.peakBufferedBits, wireBits_.size());
    inspectCompletedSymbol(completedAtSample, output);
}

bool SstvFskIdDetector::inspectCompletedSymbol(
    std::uint64_t completedAtSample,
    std::vector<SstvFskIdCandidate>& output)
{
    if (wireBits_.size() % SstvFskIdCodec::kBitsPerSymbol != 0U) {
        return false;
    }

    const std::size_t symbolIndex =
        wireBits_.size() / SstvFskIdCodec::kBitsPerSymbol - 1U;
    SstvFskIdCodec::Symbol symbol = 0U;
    bool symbolValid = true;
    const std::size_t firstBit =
        symbolIndex * SstvFskIdCodec::kBitsPerSymbol;
    for (unsigned bitIndex = 0U;
         bitIndex < SstvFskIdCodec::kBitsPerSymbol;
         ++bitIndex) {
        const SstvFskIdCodec::Bit bit = wireBits_[firstBit + bitIndex];
        if (bit > 1U) {
            symbolValid = false;
            continue;
        }
        symbol = static_cast<SstvFskIdCodec::Symbol>(
            symbol | static_cast<SstvFskIdCodec::Symbol>(bit << bitIndex));
    }

    if (symbolIndex == 0U
        && (!symbolValid || symbol != SstvFskIdCodec::kHeaderSymbol)) {
        emitCandidate(SstvFskIdCandidateEnd::MalformedHeader,
                      completedAtSample,
                      output);
        return true;
    }

    if (terminatorSymbolIndex_.has_value()
        && symbolIndex == *terminatorSymbolIndex_ + 1U) {
        emitCandidate(SstvFskIdCandidateEnd::ChecksumReceived,
                      completedAtSample,
                      output);
        return true;
    }

    if (!terminatorSymbolIndex_.has_value() && symbolValid
        && symbol == SstvFskIdCodec::kEndSymbol && symbolIndex >= 1U) {
        terminatorSymbolIndex_ = symbolIndex;
        return false;
    }

    const std::size_t lastAllowedTerminatorIndex =
        1U + SstvFskIdCodec::kMaximumTextLength;
    if (!terminatorSymbolIndex_.has_value()
        && symbolIndex >= lastAllowedTerminatorIndex) {
        emitCandidate(SstvFskIdCandidateEnd::FrameLimit,
                      completedAtSample,
                      output);
        return true;
    }
    return false;
}

SstvFskIdCandidate SstvFskIdDetector::makeCandidate(
    SstvFskIdCandidateEnd end,
    std::uint64_t completedAtSample) const
{
    SstvFskIdCandidate candidate;
    candidate.end = end;
    candidate.wireBits = wireBits_;
    candidate.bitConfidences = bitConfidences_;
    candidate.leaderStartSample = leaderStartSample_;
    candidate.frameDataStartSample = frameDataStartSample_;
    candidate.completedAtSample = completedAtSample;

    if (!candidate.bitConfidences.empty()) {
        long double sum = 0.0L;
        for (double value : candidate.bitConfidences) {
            sum += value;
        }
        candidate.confidence = static_cast<double>(
            sum / static_cast<long double>(candidate.bitConfidences.size()));
    }

    std::vector<SstvFskIdCodec::Bit> logicalBits =
        SstvFskIdCodec::symbolsToBits({SstvFskIdCodec::kStartSymbol});
    logicalBits.insert(logicalBits.end(), wireBits_.cbegin(), wireBits_.cend());
    SstvFskIdCodec::DecodeOptions options;
    options.textPolicy = config_.textPolicy;
    options.detectorConfidence = candidate.confidence;
    candidate.decoded = SstvFskIdCodec::decodeBits(logicalBits, options);
    return candidate;
}

void SstvFskIdDetector::emitCandidate(
    SstvFskIdCandidateEnd end,
    std::uint64_t completedAtSample,
    std::vector<SstvFskIdCandidate>& output)
{
    output.push_back(makeCandidate(end, completedAtSample));
    saturatingAdd(metrics_.candidatesEmitted);
    if (output.back().valid()) {
        saturatingAdd(metrics_.validIdentifiers);
    } else {
        saturatingAdd(metrics_.malformedIdentifiers);
    }
    returnToSearch();
}

void SstvFskIdDetector::returnToSearch() noexcept
{
    state_ = SstvFskIdDetectorState::SearchingLeader;
    haveLeaderRun_ = false;
    leaderQualified_ = false;
    leaderStartSample_ = 0U;
    lastLeaderSample_ = 0U;
    preambleStartSample_ = 0U;
    lastPreambleSample_ = 0U;
    startBitStartSample_ = 0U;
    lastStartBitSample_ = 0U;
    frameDataStartSample_ = 0U;
    currentBitIndex_ = 0U;
    wireBits_.clear();
    bitConfidences_.clear();
    terminatorSymbolIndex_.reset();
    clearBitVote();
}

void SstvFskIdDetector::clearBitVote() noexcept
{
    currentVote_ = {};
}

} // namespace decodium::sstv
