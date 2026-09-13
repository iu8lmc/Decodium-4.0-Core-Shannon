// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTxStateMachine.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

bool isPrePttState(SstvTxState state) noexcept
{
    return state == SstvTxState::PreparingImage
        || state == SstvTxState::Encoding
        || state == SstvTxState::Ready
        || state == SstvTxState::RequestingPtt;
}

bool isOnAirState(SstvTxState state) noexcept
{
    return state == SstvTxState::WaitingForPtt
        || state == SstvTxState::TransmittingHeader
        || state == SstvTxState::TransmittingImage
        || state == SstvTxState::TransmittingFskId
        || state == SstvTxState::TailDelay
        || state == SstvTxState::ReleasingPtt;
}

} // namespace

SstvTxStateMachine::SstvTxStateMachine(SstvTxPolicy policy)
    : policy_(std::move(policy))
{
    validatePolicy(policy_);
}

SstvTxTransition SstvTxStateMachine::dispatch(
    std::uint64_t nowMs,
    const SstvTxEvent& event)
{
    const SstvTxState before = metrics_.state;

    if (hasClock_ && nowMs < metrics_.lastEventAtMs) {
        saturatingAdd(metrics_.rejectedEvents);
        if (metrics_.state == SstvTxState::Disabled) {
            metrics_.lastErrorCode = SstvTxErrorCode::ClockRegression;
            metrics_.lastErrorDetail = "non-monotonic SSTV TX timestamp";
            metrics_.lastCause = SstvTxCause::ClockRegression;
            return result(before, SstvTxCause::ClockRegression, false);
        }
        const std::uint64_t safeNow = metrics_.lastEventAtMs;
        requestTerminal(SstvTxState::Error,
                        SstvTxCause::ClockRegression,
                        SstvTxErrorCode::ClockRegression,
                        "non-monotonic SSTV TX timestamp",
                        safeNow);
        return result(before, SstvTxCause::ClockRegression, false);
    }
    if (!hasClock_) {
        hasClock_ = true;
        metrics_.stateEnteredAtMs = nowMs;
    }
    metrics_.lastEventAtMs = nowMs;

    // Release acknowledgement and fail-safe lifecycle events always outrank a
    // phase timeout.  In particular, a late PTT-off confirmation must still
    // be accepted after the nominal release deadline.
    if (std::holds_alternative<SstvTxPttReleased>(event)) {
        if (metrics_.state != SstvTxState::ReleasingPtt) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        metrics_.releaseRequired = false;
        metrics_.pttConfirmed = false;
        metrics_.pttRequestDispatched = false;
        metrics_.audioStarted = false;
        saturatingAdd(metrics_.pttReleases);
        DeferredTerminal terminal = std::move(deferred_);
        deferred_ = {};
        if (!terminal.valid) {
            terminal = {SstvTxState::Error,
                        SstvTxCause::Failure,
                        SstvTxErrorCode::InternalFailure,
                        "PTT release had no terminal outcome",
                        true};
        }
        enterTerminal(terminal.state,
                      terminal.cause,
                      terminal.error,
                      std::move(terminal.detail),
                      nowMs);
        return result(before, SstvTxCause::PttReleased, true);
    }

    if (const auto* failure = std::get_if<SstvTxFailure>(&event)) {
        if (!active()) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        const SstvTxErrorCode code = failure->code == SstvTxErrorCode::None
            ? SstvTxErrorCode::InternalFailure
            : failure->code;
        requestTerminal(SstvTxState::Error,
                        SstvTxCause::Failure,
                        code,
                        boundedDetail(failure->detail),
                        nowMs);
        return result(before, SstvTxCause::Failure, true);
    }

    if (std::holds_alternative<SstvTxCancel>(event)) {
        if (!active()) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        requestTerminal(SstvTxState::Cancelled,
                        SstvTxCause::Cancelled,
                        SstvTxErrorCode::None,
                        {},
                        nowMs);
        return result(before, SstvTxCause::Cancelled, true);
    }

    if (std::holds_alternative<SstvTxDisable>(event)) {
        if (!enabled_) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        enabled_ = false;
        if (metrics_.releaseRequired) {
            deferred_ = {SstvTxState::Disabled,
                         SstvTxCause::Disabled,
                         SstvTxErrorCode::None,
                         {},
                         true};
            if (metrics_.state != SstvTxState::ReleasingPtt) {
                saturatingAdd(metrics_.pttReleaseRequests);
                transitionTo(SstvTxState::ReleasingPtt,
                             SstvTxCause::Disabled,
                             nowMs);
            }
        } else {
            clearSession();
            transitionTo(SstvTxState::Disabled,
                         SstvTxCause::Disabled,
                         nowMs);
        }
        return result(before, SstvTxCause::Disabled, true);
    }

    if (std::holds_alternative<SstvTxReset>(event)) {
        if (!enabled_ || active() || metrics_.releaseRequired) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        resetToIdle(nowMs, SstvTxCause::Reset);
        return result(before, SstvTxCause::Reset, true);
    }

    if (expire(nowMs)) {
        return result(before, SstvTxCause::Timeout, true);
    }

    if (std::holds_alternative<SstvTxEnable>(event)) {
        if (enabled_ || metrics_.state != SstvTxState::Disabled) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        enabled_ = true;
        clearSession();
        transitionTo(SstvTxState::Idle, SstvTxCause::Enabled, nowMs);
        return result(before, SstvTxCause::Enabled, true);
    }

    if (std::holds_alternative<SstvTxTick>(event)) {
        return result(before, SstvTxCause::None, true);
    }

    if (const auto* prepare = std::get_if<SstvTxPrepare>(&event)) {
        bool automaticReset = false;
        if (isTerminal(metrics_.state)) {
            resetToIdle(nowMs, SstvTxCause::AutoReset);
            automaticReset = true;
        }
        if (!enabled_ || metrics_.state != SstvTxState::Idle) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        if (!validPrepare(*prepare)) {
            return reject(before, SstvTxCause::InvalidInput);
        }
        beginSession(*prepare, nowMs);
        return result(before,
                      SstvTxCause::PreparationStarted,
                      true,
                      automaticReset);
    }

    if (std::holds_alternative<SstvTxImagePrepared>(event)) {
        if (metrics_.state != SstvTxState::PreparingImage) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        transitionTo(SstvTxState::Encoding,
                     SstvTxCause::ImagePrepared,
                     nowMs);
        return result(before, SstvTxCause::ImagePrepared, true);
    }

    if (const auto* encoded =
            std::get_if<SstvTxEncodingComplete>(&event)) {
        if (metrics_.state != SstvTxState::Encoding) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        if (!validEncoding(*encoded)) {
            return reject(before, SstvTxCause::InvalidInput);
        }
        metrics_.encodedSamples = encoded->totalSamples;
        metrics_.sampleRate = encoded->sampleRate;
        metrics_.fskIdEnabled = encoded->fskIdEnabled;
        metrics_.encodedDurationMs =
            (encoded->totalSamples * 1'000U + encoded->sampleRate - 1U)
            / encoded->sampleRate;
        transitionTo(SstvTxState::Ready,
                     SstvTxCause::EncodingComplete,
                     nowMs);
        return result(before, SstvTxCause::EncodingComplete, true);
    }

    if (std::holds_alternative<SstvTxRequest>(event)) {
        if (metrics_.state != SstvTxState::Ready) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        transitionTo(SstvTxState::RequestingPtt,
                     SstvTxCause::TransmissionRequested,
                     nowMs);
        return result(before, SstvTxCause::TransmissionRequested, true);
    }

    if (const auto* dispatched =
            std::get_if<SstvTxPttRequestDispatched>(&event)) {
        if (metrics_.state != SstvTxState::RequestingPtt) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        metrics_.pttRequestDispatched = true;
        metrics_.releaseRequired = dispatched->releaseRequired;
        saturatingAdd(metrics_.pttRequestsDispatched);
        transitionTo(SstvTxState::WaitingForPtt,
                     SstvTxCause::PttRequestDispatched,
                     nowMs);
        return result(before, SstvTxCause::PttRequestDispatched, true);
    }

    if (std::holds_alternative<SstvTxPttConfirmed>(event)) {
        if (metrics_.state != SstvTxState::WaitingForPtt
            || !metrics_.pttRequestDispatched
            || metrics_.pttConfirmed) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        metrics_.pttConfirmed = true;
        saturatingAdd(metrics_.pttConfirmations);
        // The confirmation wait and configured PTT lead delay have separate
        // watchdog budgets even though both intentionally share the public
        // WaitingForPtt state.
        transitionTo(SstvTxState::WaitingForPtt,
                     SstvTxCause::PttConfirmed,
                     nowMs);
        return result(before, SstvTxCause::PttConfirmed, true);
    }

    if (std::holds_alternative<SstvTxLeadElapsed>(event)) {
        if (metrics_.state != SstvTxState::WaitingForPtt
            || !metrics_.pttConfirmed) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        metrics_.audioStarted = true;
        transitionTo(SstvTxState::TransmittingHeader,
                     SstvTxCause::LeadDelayElapsed,
                     nowMs);
        return result(before, SstvTxCause::LeadDelayElapsed, true);
    }

    if (std::holds_alternative<SstvTxHeaderComplete>(event)) {
        if (metrics_.state != SstvTxState::TransmittingHeader) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        transitionTo(SstvTxState::TransmittingImage,
                     SstvTxCause::HeaderComplete,
                     nowMs);
        return result(before, SstvTxCause::HeaderComplete, true);
    }

    if (std::holds_alternative<SstvTxImageComplete>(event)) {
        if (metrics_.state != SstvTxState::TransmittingImage) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        transitionTo(metrics_.fskIdEnabled
                         ? SstvTxState::TransmittingFskId
                         : SstvTxState::TailDelay,
                     SstvTxCause::ImageComplete,
                     nowMs);
        return result(before, SstvTxCause::ImageComplete, true);
    }

    if (std::holds_alternative<SstvTxFskIdComplete>(event)) {
        if (metrics_.state != SstvTxState::TransmittingFskId) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        transitionTo(SstvTxState::TailDelay,
                     SstvTxCause::FskIdComplete,
                     nowMs);
        return result(before, SstvTxCause::FskIdComplete, true);
    }

    if (std::holds_alternative<SstvTxTailElapsed>(event)) {
        if (metrics_.state != SstvTxState::TailDelay) {
            return reject(before, SstvTxCause::UnexpectedEvent);
        }
        deferred_ = {SstvTxState::Completed,
                     SstvTxCause::Completed,
                     SstvTxErrorCode::None,
                     {},
                     true};
        saturatingAdd(metrics_.pttReleaseRequests);
        transitionTo(SstvTxState::ReleasingPtt,
                     SstvTxCause::TailDelayElapsed,
                     nowMs);
        return result(before, SstvTxCause::TailDelayElapsed, true);
    }

    return reject(before, SstvTxCause::UnexpectedEvent);
}

SstvTxState SstvTxStateMachine::state() const noexcept
{
    return metrics_.state;
}

const SstvTxPolicy& SstvTxStateMachine::policy() const noexcept
{
    return policy_;
}

const SstvTxMetrics& SstvTxStateMachine::metrics() const noexcept
{
    return metrics_;
}

bool SstvTxStateMachine::enabled() const noexcept
{
    return enabled_;
}

bool SstvTxStateMachine::active() const noexcept
{
    return isPrePttState(metrics_.state) || isOnAirState(metrics_.state);
}

bool SstvTxStateMachine::releaseRequired() const noexcept
{
    return metrics_.releaseRequired;
}

bool SstvTxStateMachine::invariantsHold() const noexcept
{
    if (metrics_.state == SstvTxState::Disabled && enabled_) {
        return false;
    }
    if (metrics_.state != SstvTxState::Disabled && !enabled_
        && metrics_.state != SstvTxState::ReleasingPtt) {
        return false;
    }
    if (metrics_.pttConfirmed && !metrics_.pttRequestDispatched) {
        return false;
    }
    if (metrics_.audioStarted && !metrics_.pttConfirmed) {
        return false;
    }
    if (metrics_.releaseRequired && !metrics_.pttRequestDispatched) {
        return false;
    }
    if (metrics_.state == SstvTxState::ReleasingPtt
        && !deferred_.valid) {
        return false;
    }
    if (deferred_.valid
        && metrics_.state != SstvTxState::ReleasingPtt) {
        return false;
    }
    if (isTerminal(metrics_.state)
        && (metrics_.releaseRequired || metrics_.pttConfirmed
            || metrics_.audioStarted || deferred_.valid)) {
        return false;
    }
    return true;
}

bool SstvTxStateMachine::isTerminal(SstvTxState state) noexcept
{
    return state == SstvTxState::Completed
        || state == SstvTxState::Cancelled
        || state == SstvTxState::Error;
}

const char* SstvTxStateMachine::stateName(SstvTxState state) noexcept
{
    switch (state) {
    case SstvTxState::Disabled: return "Disabled";
    case SstvTxState::Idle: return "Idle";
    case SstvTxState::PreparingImage: return "PreparingImage";
    case SstvTxState::Encoding: return "Encoding";
    case SstvTxState::Ready: return "Ready";
    case SstvTxState::RequestingPtt: return "RequestingPtt";
    case SstvTxState::WaitingForPtt: return "WaitingForPtt";
    case SstvTxState::TransmittingHeader: return "TransmittingHeader";
    case SstvTxState::TransmittingImage: return "TransmittingImage";
    case SstvTxState::TransmittingFskId: return "TransmittingFskId";
    case SstvTxState::TailDelay: return "TailDelay";
    case SstvTxState::ReleasingPtt: return "ReleasingPtt";
    case SstvTxState::Completed: return "Completed";
    case SstvTxState::Cancelled: return "Cancelled";
    case SstvTxState::Error: return "Error";
    }
    return "Unknown";
}

const char* SstvTxStateMachine::causeName(SstvTxCause cause) noexcept
{
    switch (cause) {
    case SstvTxCause::None: return "None";
    case SstvTxCause::Enabled: return "Enabled";
    case SstvTxCause::Disabled: return "Disabled";
    case SstvTxCause::Reset: return "Reset";
    case SstvTxCause::AutoReset: return "AutoReset";
    case SstvTxCause::PreparationStarted: return "PreparationStarted";
    case SstvTxCause::ImagePrepared: return "ImagePrepared";
    case SstvTxCause::EncodingComplete: return "EncodingComplete";
    case SstvTxCause::TransmissionRequested: return "TransmissionRequested";
    case SstvTxCause::PttRequestDispatched: return "PttRequestDispatched";
    case SstvTxCause::PttConfirmed: return "PttConfirmed";
    case SstvTxCause::LeadDelayElapsed: return "LeadDelayElapsed";
    case SstvTxCause::HeaderComplete: return "HeaderComplete";
    case SstvTxCause::ImageComplete: return "ImageComplete";
    case SstvTxCause::FskIdComplete: return "FskIdComplete";
    case SstvTxCause::TailDelayElapsed: return "TailDelayElapsed";
    case SstvTxCause::PttReleased: return "PttReleased";
    case SstvTxCause::Completed: return "Completed";
    case SstvTxCause::Cancelled: return "Cancelled";
    case SstvTxCause::Failure: return "Failure";
    case SstvTxCause::Timeout: return "Timeout";
    case SstvTxCause::ClockRegression: return "ClockRegression";
    case SstvTxCause::UnexpectedEvent: return "UnexpectedEvent";
    case SstvTxCause::InvalidInput: return "InvalidInput";
    }
    return "Unknown";
}

SstvTxTransition SstvTxStateMachine::result(
    SstvTxState before,
    SstvTxCause cause,
    bool accepted,
    bool automaticReset) const noexcept
{
    return {before,
            metrics_.state,
            cause,
            accepted,
            before != metrics_.state,
            metrics_.releaseRequired,
            automaticReset,
            metrics_.currentSessionId};
}

SstvTxTransition SstvTxStateMachine::reject(SstvTxState before,
                                             SstvTxCause cause) noexcept
{
    saturatingAdd(metrics_.rejectedEvents);
    return result(before, cause, false);
}

void SstvTxStateMachine::transitionTo(SstvTxState state,
                                      SstvTxCause cause,
                                      std::uint64_t nowMs) noexcept
{
    metrics_.state = state;
    metrics_.lastCause = cause;
    metrics_.stateEnteredAtMs = nowMs;
}

void SstvTxStateMachine::clearSession() noexcept
{
    metrics_.mode.clear();
    metrics_.currentSessionId = 0U;
    metrics_.encodedSamples = 0U;
    metrics_.encodedDurationMs = 0U;
    metrics_.sampleRate = 0U;
    metrics_.imageWidth = 0U;
    metrics_.imageHeight = 0U;
    metrics_.fskIdEnabled = false;
    metrics_.pttRequestDispatched = false;
    metrics_.pttConfirmed = false;
    metrics_.releaseRequired = false;
    metrics_.audioStarted = false;
    deferred_ = {};
}

void SstvTxStateMachine::resetToIdle(std::uint64_t nowMs,
                                     SstvTxCause cause) noexcept
{
    clearSession();
    metrics_.lastErrorCode = SstvTxErrorCode::None;
    metrics_.lastErrorDetail.clear();
    transitionTo(SstvTxState::Idle, cause, nowMs);
}

void SstvTxStateMachine::beginSession(const SstvTxPrepare& prepare,
                                      std::uint64_t nowMs)
{
    clearSession();
    if (nextSessionId_ == std::numeric_limits<std::uint64_t>::max()) {
        nextSessionId_ = 1U;
    } else {
        ++nextSessionId_;
    }
    metrics_.currentSessionId = nextSessionId_;
    metrics_.mode = prepare.mode;
    metrics_.imageWidth = prepare.width;
    metrics_.imageHeight = prepare.height;
    metrics_.lastErrorCode = SstvTxErrorCode::None;
    metrics_.lastErrorDetail.clear();
    saturatingAdd(metrics_.sessionsStarted);
    transitionTo(SstvTxState::PreparingImage,
                 SstvTxCause::PreparationStarted,
                 nowMs);
}

void SstvTxStateMachine::enterTerminal(SstvTxState state,
                                       SstvTxCause cause,
                                       SstvTxErrorCode error,
                                       std::string detail,
                                       std::uint64_t nowMs)
{
    if (state == SstvTxState::Disabled) {
        clearSession();
    }
    metrics_.releaseRequired = false;
    metrics_.pttConfirmed = false;
    metrics_.pttRequestDispatched = false;
    metrics_.audioStarted = false;
    deferred_ = {};
    metrics_.lastErrorCode = error;
    metrics_.lastErrorDetail = boundedDetail(detail);
    switch (state) {
    case SstvTxState::Completed:
        saturatingAdd(metrics_.sessionsCompleted);
        break;
    case SstvTxState::Cancelled:
        saturatingAdd(metrics_.sessionsCancelled);
        break;
    case SstvTxState::Error:
        saturatingAdd(metrics_.sessionsFailed);
        break;
    default:
        break;
    }
    transitionTo(state, cause, nowMs);
}

void SstvTxStateMachine::requestTerminal(SstvTxState state,
                                         SstvTxCause cause,
                                         SstvTxErrorCode error,
                                         std::string detail,
                                         std::uint64_t nowMs)
{
    if (metrics_.state == SstvTxState::ReleasingPtt) {
        // A later, more severe failure may refine the outcome, but never
        // escapes the release barrier.
        if (deferred_.valid
            && deferred_.state == SstvTxState::Disabled) {
            transitionTo(SstvTxState::ReleasingPtt,
                         SstvTxCause::Disabled,
                         nowMs);
            return;
        }
        if (state == SstvTxState::Error || !deferred_.valid) {
            deferred_ = {state,
                         cause,
                         error,
                         boundedDetail(detail),
                         true};
        }
        transitionTo(SstvTxState::ReleasingPtt, cause, nowMs);
        return;
    }
    if (metrics_.releaseRequired) {
        deferred_ = {state,
                     cause,
                     error,
                     boundedDetail(detail),
                     true};
        saturatingAdd(metrics_.pttReleaseRequests);
        transitionTo(SstvTxState::ReleasingPtt, cause, nowMs);
        return;
    }
    enterTerminal(state, cause, error, std::move(detail), nowMs);
}

bool SstvTxStateMachine::expire(std::uint64_t nowMs)
{
    const std::uint64_t timeout = timeoutForState();
    if (timeout == 0U || nowMs < metrics_.stateEnteredAtMs
        || nowMs - metrics_.stateEnteredAtMs < timeout) {
        return false;
    }

    saturatingAdd(metrics_.watchdogExpiries);
    SstvTxErrorCode code = SstvTxErrorCode::WatchdogExpired;
    if (metrics_.state == SstvTxState::RequestingPtt
        || metrics_.state == SstvTxState::WaitingForPtt) {
        code = SstvTxErrorCode::PttTimeout;
    }
    requestTerminal(SstvTxState::Error,
                    SstvTxCause::Timeout,
                    code,
                    "SSTV TX phase watchdog expired",
                    nowMs);
    return true;
}

std::uint64_t SstvTxStateMachine::timeoutForState() const noexcept
{
    switch (metrics_.state) {
    case SstvTxState::PreparingImage:
        return policy_.timeouts.preparingImageMs;
    case SstvTxState::Encoding:
        return policy_.timeouts.encodingMs;
    case SstvTxState::RequestingPtt:
        return policy_.timeouts.requestingPttMs;
    case SstvTxState::WaitingForPtt:
        return policy_.timeouts.waitingForPttMs;
    case SstvTxState::TransmittingHeader:
        return policy_.timeouts.transmittingHeaderMs;
    case SstvTxState::TransmittingImage:
        if (metrics_.encodedDurationMs
            > std::numeric_limits<std::uint64_t>::max()
                - policy_.timeouts.transmittingImageSlackMs) {
            return std::numeric_limits<std::uint64_t>::max();
        }
        return metrics_.encodedDurationMs
            + policy_.timeouts.transmittingImageSlackMs;
    case SstvTxState::TransmittingFskId:
        return policy_.timeouts.transmittingFskIdMs;
    case SstvTxState::TailDelay:
        return policy_.timeouts.tailDelayMs;
    case SstvTxState::ReleasingPtt:
        return policy_.timeouts.releasingPttMs;
    default:
        return 0U;
    }
}

bool SstvTxStateMachine::validPrepare(
    const SstvTxPrepare& prepare) const noexcept
{
    return !prepare.mode.empty()
        && prepare.mode.size() <= policy_.maximumModeCharacters
        && prepare.mode.find('\0') == std::string::npos
        && prepare.width > 0U && prepare.height > 0U
        && prepare.width <= policy_.maximumImageDimension
        && prepare.height <= policy_.maximumImageDimension;
}

bool SstvTxStateMachine::validEncoding(
    const SstvTxEncodingComplete& encoded) const noexcept
{
    if (encoded.totalSamples == 0U
        || encoded.totalSamples > policy_.maximumEncodedSamples
        || encoded.sampleRate < policy_.minimumSampleRate
        || encoded.sampleRate > policy_.maximumSampleRate) {
        return false;
    }
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    return encoded.totalSamples <= maximum / 1'000U
        && encoded.totalSamples * 1'000U
            <= maximum - (encoded.sampleRate - 1U);
}

std::string SstvTxStateMachine::boundedDetail(
    const std::string& detail) const
{
    return detail.substr(0U, policy_.maximumErrorCharacters);
}

void SstvTxStateMachine::validatePolicy(const SstvTxPolicy& policy)
{
    const auto& timeouts = policy.timeouts;
    if (timeouts.preparingImageMs == 0U || timeouts.encodingMs == 0U
        || timeouts.requestingPttMs == 0U
        || timeouts.waitingForPttMs == 0U
        || timeouts.transmittingHeaderMs == 0U
        || timeouts.transmittingImageSlackMs == 0U
        || timeouts.transmittingFskIdMs == 0U
        || timeouts.tailDelayMs == 0U
        || timeouts.releasingPttMs == 0U
        || policy.maximumEncodedSamples == 0U
        || policy.minimumSampleRate == 0U
        || policy.minimumSampleRate > policy.maximumSampleRate
        || policy.maximumImageDimension == 0U
        || policy.maximumModeCharacters == 0U
        || policy.maximumErrorCharacters == 0U) {
        throw std::invalid_argument("invalid SSTV TX policy");
    }
}

void SstvTxStateMachine::saturatingAdd(std::uint64_t& value,
                                       std::uint64_t increment) noexcept
{
    if (increment > std::numeric_limits<std::uint64_t>::max() - value) {
        value = std::numeric_limits<std::uint64_t>::max();
    } else {
        value += increment;
    }
}

} // namespace decodium::sstv
