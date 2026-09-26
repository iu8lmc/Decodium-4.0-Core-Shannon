// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvAvtRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

SstvAvtDecoderConfig decoderConfig(const SstvAvtRxSessionConfig& config)
{
    SstvAvtDecoderConfig result;
    result.mode = config.mode;
    result.sampleRate = config.sampleRate;
    result.clockErrorPpm = config.clockErrorPpm;
    result.imageStartSample = config.imageStartSample;
    result.observationSpanSamples = config.observationSpanSamples;
    result.frequencyOffsetHz = config.frequencyOffsetHz;
    result.minimumObservationConfidence =
        config.minimumObservationConfidence;
    result.maximumInterpolationGapPixels =
        config.maximumInterpolationGapPixels;
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

void SstvAvtRxSession::validateConfig(
    const SstvAvtRxSessionConfig& config)
{
    static_cast<void>(SstvAvtProtocol::spec(config.mode));
    if (config.observationSpanSamples == 0U
        || config.observationSpanSamples
            > SstvAvtDecoder::MaximumObservationSpanSamples) {
        throw std::invalid_argument("invalid AVT observation span");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvAvtDecoder::MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "AVT session frequency offset is out of range");
    }
}

SstvAvtRxSession::SstvAvtRxSession(SstvAvtRxSessionConfig config)
    : config_(config)
    , decoder_((validateConfig(config), decoderConfig(config)))
{
}

void SstvAvtRxSession::saturatingAdd(std::uint64_t& value,
                                     std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvAvtRxSessionUpdate SstvAvtRxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    saturatingAdd(metrics_.consumeCalls);
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "AVT session consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "AVT session observations must not be null");
    }
    SstvAvtRxSessionUpdate update;
    update.inputObservations = count;
    update.state = state_;
    saturatingAdd(metrics_.inputObservations, count);
    metrics_.peakInputObservations = std::max(
        metrics_.peakInputObservations, count);
    if (count == 0U || state_ != SstvAvtRxSessionState::Receiving) {
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

    const SstvAvtDecoderMetrics before = decoder_.metrics();
    update.decoderAcceptedObservations = decoder_.consume(observations,
                                                          count);
    if (reachedEnd && decoder_.state() == SstvAvtDecodeState::Receiving) {
        static_cast<void>(decoder_.finish());
    }
    updateState();
    const SstvAvtDecoderMetrics after = decoder_.metrics();

    update.linesPublished = after.linesPublished;
    update.imageChanged = after.componentsPublished
        != before.componentsPublished;
    update.state = state_;
    saturatingAdd(metrics_.decoderAcceptedObservations,
                  update.decoderAcceptedObservations);
    metrics_.linesPublished = after.linesPublished;
    return update;
}

SstvAvtRxSessionUpdate SstvAvtRxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvAvtRxSessionState SstvAvtRxSession::notifyDiscontinuity(
    std::uint64_t nextSample)
{
    saturatingAdd(metrics_.discontinuities);
    if (state_ != SstvAvtRxSessionState::Receiving) {
        return state_;
    }
    static_cast<void>(nextSample);
    static_cast<void>(decoder_.finish());
    updateState();
    return state_;
}

SstvAvtRxSessionState SstvAvtRxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ == SstvAvtRxSessionState::Receiving) {
        static_cast<void>(decoder_.finish());
        updateState();
    }
    return state_;
}

void SstvAvtRxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ == SstvAvtRxSessionState::Receiving) {
        decoder_.cancel();
        updateState();
    }
}

void SstvAvtRxSession::updateState() noexcept
{
    switch (decoder_.state()) {
    case SstvAvtDecodeState::Receiving:
        state_ = SstvAvtRxSessionState::Receiving;
        break;
    case SstvAvtDecodeState::Partial:
        state_ = SstvAvtRxSessionState::Partial;
        break;
    case SstvAvtDecodeState::Complete:
        state_ = SstvAvtRxSessionState::Complete;
        break;
    case SstvAvtDecodeState::Cancelled:
        state_ = SstvAvtRxSessionState::Cancelled;
        break;
    }
}

SstvAvtMode SstvAvtRxSession::mode() const noexcept
{
    return decoder_.mode();
}

SstvAvtRxSessionState SstvAvtRxSession::state() const noexcept
{
    return state_;
}

std::uint64_t SstvAvtRxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvAvtRxSession::imageEndSample() const noexcept
{
    return decoder_.imageEndSample();
}

const SstvImageFrame& SstvAvtRxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvAvtRxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvAvtRxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvAvtDecoderMetrics SstvAvtRxSession::decoderMetrics() const noexcept
{
    return decoder_.metrics();
}

SstvAvtRxSessionMetrics SstvAvtRxSession::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
