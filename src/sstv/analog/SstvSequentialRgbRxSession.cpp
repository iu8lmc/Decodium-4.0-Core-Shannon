// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvSequentialRgbRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

SstvSequentialRgbDecoderConfig decoderConfig(
    const SstvSequentialRgbRxSessionConfig& config)
{
    SstvSequentialRgbDecoderConfig result;
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

void SstvSequentialRgbRxSession::validateConfig(
    const SstvSequentialRgbRxSessionConfig& config)
{
    static_cast<void>(SstvSequentialRgbProtocol::spec(config.mode));
    if (config.observationSpanSamples == 0U
        || config.observationSpanSamples > 4'096U) {
        throw std::invalid_argument(
            "invalid sequential RGB observation span");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvSequentialRgbDecoder::MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "sequential RGB session frequency offset is out of range");
    }
}

SstvSequentialRgbRxSession::SstvSequentialRgbRxSession(
    SstvSequentialRgbRxSessionConfig config)
    : config_(config)
    , decoder_((validateConfig(config), decoderConfig(config)))
{
}

void SstvSequentialRgbRxSession::saturatingAdd(
    std::uint64_t& value,
    std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvSequentialRgbRxSessionUpdate SstvSequentialRgbRxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    saturatingAdd(metrics_.consumeCalls);
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "sequential RGB session consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "sequential RGB session observations must not be null");
    }
    SstvSequentialRgbRxSessionUpdate update;
    update.inputObservations = count;
    update.state = state_;
    saturatingAdd(metrics_.inputObservations, count);
    metrics_.peakInputObservations = std::max(
        metrics_.peakInputObservations, count);
    if (count == 0U || state_ != SstvSequentialRgbRxSessionState::Receiving) {
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

    const SstvSequentialRgbDecoderMetrics metricsBefore = decoder_.metrics();
    update.decoderAcceptedObservations = decoder_.consume(observations, count);
    if (reachedEnd
        && decoder_.state() == SstvSequentialRgbDecodeState::Receiving) {
        static_cast<void>(decoder_.finish());
    }
    updateState();
    const SstvSequentialRgbDecoderMetrics metricsAfter = decoder_.metrics();

    update.observedLineSyncs = metricsAfter.observedLineSyncs
        - metricsBefore.observedLineSyncs;
    update.linesPublished = metricsAfter.linesPublished;
    update.imageChanged = metricsAfter.componentsPublished
        != metricsBefore.componentsPublished;
    update.state = state_;

    saturatingAdd(metrics_.decoderAcceptedObservations,
                  update.decoderAcceptedObservations);
    saturatingAdd(metrics_.observedLineSyncs,
                  update.observedLineSyncs);
    metrics_.linesPublished = metricsAfter.linesPublished;
    return update;
}

SstvSequentialRgbRxSessionUpdate SstvSequentialRgbRxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvSequentialRgbRxSessionState
SstvSequentialRgbRxSession::notifyDiscontinuity(std::uint64_t nextSample)
{
    saturatingAdd(metrics_.discontinuities);
    if (state_ != SstvSequentialRgbRxSessionState::Receiving) {
        return state_;
    }
    // A gap at or beyond the exclusive image end may still close a frame whose
    // last accumulated scanline has not yet been published.
    static_cast<void>(nextSample);
    static_cast<void>(decoder_.finish());
    updateState();
    return state_;
}

SstvSequentialRgbRxSessionState SstvSequentialRgbRxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ == SstvSequentialRgbRxSessionState::Receiving) {
        static_cast<void>(decoder_.finish());
        updateState();
    }
    return state_;
}

void SstvSequentialRgbRxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ == SstvSequentialRgbRxSessionState::Receiving) {
        decoder_.cancel();
        updateState();
    }
}

void SstvSequentialRgbRxSession::updateState() noexcept
{
    switch (decoder_.state()) {
    case SstvSequentialRgbDecodeState::Receiving:
        state_ = SstvSequentialRgbRxSessionState::Receiving;
        break;
    case SstvSequentialRgbDecodeState::Partial:
        state_ = SstvSequentialRgbRxSessionState::Partial;
        break;
    case SstvSequentialRgbDecodeState::Complete:
        state_ = SstvSequentialRgbRxSessionState::Complete;
        break;
    case SstvSequentialRgbDecodeState::Cancelled:
        state_ = SstvSequentialRgbRxSessionState::Cancelled;
        break;
    }
}

SstvSequentialRgbMode SstvSequentialRgbRxSession::mode() const noexcept
{
    return decoder_.mode();
}

SstvSequentialRgbRxSessionState SstvSequentialRgbRxSession::state() const noexcept
{
    return state_;
}

std::uint64_t SstvSequentialRgbRxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvSequentialRgbRxSession::imageEndSample() const noexcept
{
    return decoder_.imageEndSample();
}

const SstvImageFrame& SstvSequentialRgbRxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvSequentialRgbRxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvSequentialRgbRxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvSequentialRgbDecoderMetrics
SstvSequentialRgbRxSession::decoderMetrics() const noexcept
{
    return decoder_.metrics();
}

SstvSequentialRgbRxSessionMetrics
SstvSequentialRgbRxSession::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
