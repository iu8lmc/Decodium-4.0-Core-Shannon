// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvMmsstvExtendedRxSession.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

SstvMmsstvDecoderConfig decoderConfig(
    const SstvMmsstvRxSessionConfig& config)
{
    SstvMmsstvDecoderConfig result;
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

void SstvMmsstvRxSession::validateConfig(
    const SstvMmsstvRxSessionConfig& config)
{
    static_cast<void>(SstvMmsstvProtocol::spec(config.mode));
    if (config.observationSpanSamples == 0U
        || config.observationSpanSamples > 4'096U) {
        throw std::invalid_argument("invalid MMSSTV observation span");
    }
    if (!std::isfinite(config.frequencyOffsetHz)
        || std::abs(config.frequencyOffsetHz)
            > SstvMmsstvDecoder::MaximumAbsoluteFrequencyOffsetHz) {
        throw std::invalid_argument(
            "MMSSTV session frequency offset is out of range");
    }
}

SstvMmsstvRxSession::SstvMmsstvRxSession(
    SstvMmsstvRxSessionConfig config)
    : config_(config)
    , decoder_((validateConfig(config), decoderConfig(config)))
{
}

void SstvMmsstvRxSession::saturatingAdd(
    std::uint64_t& value,
    std::uint64_t increment) noexcept
{
    value = increment > std::numeric_limits<std::uint64_t>::max() - value
        ? std::numeric_limits<std::uint64_t>::max()
        : value + increment;
}

SstvMmsstvRxSessionUpdate SstvMmsstvRxSession::consume(
    const SstvFrequencyObservation* observations,
    std::size_t count)
{
    saturatingAdd(metrics_.consumeCalls);
    if (count > MaximumObservationsPerConsume) {
        saturatingAdd(metrics_.rejectedInputCalls);
        saturatingAdd(metrics_.rejectedOversizeCalls);
        throw std::length_error(
            "MMSSTV session consume exceeds its work bound");
    }
    if (count != 0U && observations == nullptr) {
        saturatingAdd(metrics_.rejectedInputCalls);
        throw std::invalid_argument(
            "MMSSTV session observations must not be null");
    }
    SstvMmsstvRxSessionUpdate update;
    update.inputObservations = count;
    update.state = state_;
    saturatingAdd(metrics_.inputObservations, count);
    metrics_.peakInputObservations = std::max(
        metrics_.peakInputObservations, count);
    if (count == 0U || state_ != SstvMmsstvRxSessionState::Receiving) {
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

    const SstvMmsstvDecoderMetrics before = decoder_.metrics();
    update.decoderAcceptedObservations = decoder_.consume(observations, count);
    if (reachedEnd
        && decoder_.state() == SstvMmsstvDecodeState::Receiving) {
        static_cast<void>(decoder_.finish());
    }
    updateState();
    const SstvMmsstvDecoderMetrics after = decoder_.metrics();
    update.observedScanSyncs =
        after.observedScanSyncs - before.observedScanSyncs;
    update.scansPublished = after.scansPublished;
    update.linesPublished = after.linesPublished;
    update.imageChanged =
        after.componentsPublished != before.componentsPublished;
    update.state = state_;

    saturatingAdd(metrics_.decoderAcceptedObservations,
                  update.decoderAcceptedObservations);
    saturatingAdd(metrics_.observedScanSyncs, update.observedScanSyncs);
    metrics_.scansPublished = after.scansPublished;
    metrics_.linesPublished = after.linesPublished;
    return update;
}

SstvMmsstvRxSessionUpdate SstvMmsstvRxSession::consume(
    const std::vector<SstvFrequencyObservation>& observations)
{
    return consume(observations.data(), observations.size());
}

SstvMmsstvRxSessionState SstvMmsstvRxSession::notifyDiscontinuity(
    std::uint64_t nextSample)
{
    saturatingAdd(metrics_.discontinuities);
    if (state_ != SstvMmsstvRxSessionState::Receiving) {
        return state_;
    }
    static_cast<void>(nextSample);
    static_cast<void>(decoder_.finish());
    updateState();
    return state_;
}

SstvMmsstvRxSessionState SstvMmsstvRxSession::finish()
{
    saturatingAdd(metrics_.finishCalls);
    if (state_ == SstvMmsstvRxSessionState::Receiving) {
        static_cast<void>(decoder_.finish());
        updateState();
    }
    return state_;
}

void SstvMmsstvRxSession::cancel() noexcept
{
    saturatingAdd(metrics_.cancelCalls);
    if (state_ == SstvMmsstvRxSessionState::Receiving) {
        decoder_.cancel();
        updateState();
    }
}

void SstvMmsstvRxSession::updateState() noexcept
{
    switch (decoder_.state()) {
    case SstvMmsstvDecodeState::Receiving:
        state_ = SstvMmsstvRxSessionState::Receiving;
        break;
    case SstvMmsstvDecodeState::Partial:
        state_ = SstvMmsstvRxSessionState::Partial;
        break;
    case SstvMmsstvDecodeState::Complete:
        state_ = SstvMmsstvRxSessionState::Complete;
        break;
    case SstvMmsstvDecodeState::Cancelled:
        state_ = SstvMmsstvRxSessionState::Cancelled;
        break;
    }
}

SstvMmsstvMode SstvMmsstvRxSession::mode() const noexcept
{
    return decoder_.mode();
}

SstvMmsstvRxSessionState SstvMmsstvRxSession::state() const noexcept
{
    return state_;
}

std::uint64_t SstvMmsstvRxSession::imageStartSample() const noexcept
{
    return config_.imageStartSample;
}

std::uint64_t SstvMmsstvRxSession::imageEndSample() const noexcept
{
    return decoder_.imageEndSample();
}

const SstvImageFrame& SstvMmsstvRxSession::imageFrame() const noexcept
{
    return decoder_.imageFrame();
}

SstvImageSnapshot SstvMmsstvRxSession::snapshot() const
{
    return decoder_.snapshot();
}

std::vector<SstvDirtyEvent> SstvMmsstvRxSession::takeDirtyEvents()
{
    return decoder_.takeDirtyEvents();
}

SstvMmsstvDecoderMetrics SstvMmsstvRxSession::decoderMetrics() const noexcept
{
    return decoder_.metrics();
}

SstvMmsstvRxSessionMetrics SstvMmsstvRxSession::metrics() const noexcept
{
    return metrics_;
}

} // namespace decodium::sstv
