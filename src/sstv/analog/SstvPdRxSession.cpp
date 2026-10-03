// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvPdRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

SstvPdDecoderConfig decoderConfig(const SstvPdRxSessionConfig& config)
{
    SstvPdDecoderConfig result;
    result.mode = config.mode;
    result.sampleRate = config.sampleRate;
    result.clockErrorPpm = config.clockErrorPpm;
    result.imageStartSample = config.imageStartSample;
    result.frequencyOffsetHz = config.frequencyOffsetHz;
    result.minimumObservationConfidence =
        config.minimumObservationConfidence;
    result.maximumPendingDirtyEvents = config.maximumPendingDirtyEvents;
    return result;
}

std::uint64_t observationIntervalEnd(std::uint64_t centreSample,
                                     std::uint32_t spanSamples) noexcept
{
    const std::uint64_t leftSpan = spanSamples / 2U;
    const std::uint64_t start = centreSample >= leftSpan
        ? centreSample - leftSpan : 0U;
    return spanSamples > std::numeric_limits<std::uint64_t>::max() - start
        ? std::numeric_limits<std::uint64_t>::max()
        : start + spanSamples;
}

} // namespace

void SstvPdRxSession::validateConfig(const SstvPdRxSessionConfig& config)
{
    static_cast<void>(SstvPdProtocol::spec(config.mode));
    if (config.observationSpanSamples == 0U
        || config.observationSpanSamples > 4'096U) {
        throw std::invalid_argument("invalid PD observation span");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvPdDecoder::MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "PD session frequency offset is out of range");
    }
}

SstvPdRxSession::SstvPdRxSession(SstvPdRxSessionConfig config)
    : config_(config)
    , decoder_((validateConfig(config), decoderConfig(config)))
{
}

void SstvPdRxSession::saturatingAdd(std::uint64_t& value,
                                    std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvPdRxSessionUpdate SstvPdRxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    saturatingAdd(metrics_.consumeCalls);
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error("PD session consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "PD session observations must not be null");
    }
    SstvPdRxSessionUpdate update;
    update.inputObservations = count;
    update.state = state_;
    saturatingAdd(metrics_.inputObservations, count);
    metrics_.peakInputObservations = std::max(
        metrics_.peakInputObservations, count);
    if (count == 0U || state_ != SstvPdRxSessionState::Receiving) {
        return update;
    }

    std::uint64_t previousSample = lastInputSample_;
    std::uint64_t previousSequence = lastInputSequence_;
    bool havePreviousSample = haveLastInputSample_;
    bool havePreviousSequence = haveLastInputSequence_;
    for (std::size_t index = 0U; index < count; ++index) {
        if ((havePreviousSample
             && observations[index].centreSample <= previousSample)
            || (havePreviousSequence
                && observations[index].sequence <= previousSequence)) {
            saturatingAdd(metrics_.rejectedInputCalls);
            saturatingAdd(metrics_.rejectedRegressions);
            return update;
        }
        previousSample = observations[index].centreSample;
        previousSequence = observations[index].sequence;
        havePreviousSample = true;
        havePreviousSequence = true;
    }

    bool reachedEnd = false;
    for (std::size_t index = 0U; index < count; ++index) {
        lastInputSample_ = observations[index].centreSample;
        lastInputEndSample_ = observationIntervalEnd(
            lastInputSample_, config_.observationSpanSamples);
        lastInputSequence_ = observations[index].sequence;
        haveLastInputSample_ = true;
        haveLastInputSequence_ = true;
        reachedEnd = reachedEnd
            || lastInputEndSample_ >= decoder_.imageEndSample();
    }

    const SstvPdDecoderMetrics before = decoder_.metrics();
    update.decoderAcceptedObservations = decoder_.consume(observations, count);
    if (reachedEnd && decoder_.state() == SstvPdDecodeState::Receiving) {
        static_cast<void>(decoder_.finish());
    }
    updateState();
    const SstvPdDecoderMetrics after = decoder_.metrics();
    update.observedPairSyncs = after.observedPairSyncs
        - before.observedPairSyncs;
    update.linePairsPublished = after.linePairsPublished;
    update.linesPublished = after.linesPublished;
    update.imageChanged = after.componentsPublished
        != before.componentsPublished;
    update.state = state_;

    saturatingAdd(metrics_.decoderAcceptedObservations,
                  update.decoderAcceptedObservations);
    saturatingAdd(metrics_.observedPairSyncs, update.observedPairSyncs);
    metrics_.linePairsPublished = after.linePairsPublished;
    metrics_.linesPublished = after.linesPublished;
    return update;
}

SstvPdRxSessionUpdate SstvPdRxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvPdRxSessionState SstvPdRxSession::notifyDiscontinuity(
    std::uint64_t nextSample)
{
    saturatingAdd(metrics_.discontinuities);
    if (state_ != SstvPdRxSessionState::Receiving) {
        return state_;
    }
    static_cast<void>(nextSample);
    static_cast<void>(decoder_.finish());
    updateState();
    return state_;
}

SstvPdRxSessionState SstvPdRxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ == SstvPdRxSessionState::Receiving) {
        static_cast<void>(decoder_.finish());
        updateState();
    }
    return state_;
}

void SstvPdRxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ == SstvPdRxSessionState::Receiving) {
        decoder_.cancel();
        updateState();
    }
}

void SstvPdRxSession::updateState() noexcept
{
    switch (decoder_.state()) {
    case SstvPdDecodeState::Receiving:
        state_ = SstvPdRxSessionState::Receiving;
        break;
    case SstvPdDecodeState::Partial:
        state_ = SstvPdRxSessionState::Partial;
        break;
    case SstvPdDecodeState::Complete:
        state_ = SstvPdRxSessionState::Complete;
        break;
    case SstvPdDecodeState::Cancelled:
        state_ = SstvPdRxSessionState::Cancelled;
        break;
    }
}

SstvPdMode SstvPdRxSession::mode() const noexcept
{
    return decoder_.mode();
}

SstvPdRxSessionState SstvPdRxSession::state() const noexcept
{
    return state_;
}

std::uint64_t SstvPdRxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvPdRxSession::imageEndSample() const noexcept
{
    return decoder_.imageEndSample();
}

const SstvImageFrame& SstvPdRxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvPdRxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvPdRxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvPdDecoderMetrics SstvPdRxSession::decoderMetrics() const noexcept
{
    return decoder_.metrics();
}

SstvPdRxSessionMetrics SstvPdRxSession::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
