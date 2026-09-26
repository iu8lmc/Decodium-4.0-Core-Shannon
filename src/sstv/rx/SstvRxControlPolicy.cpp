// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvRxControlPolicy.h"

#include <cmath>
#include <limits>
#include <utility>

namespace decodium::sstv {
namespace {

bool finiteWithin(double value, double bound) noexcept
{
    return std::isfinite(value) && std::abs(value) <= bound;
}

} // namespace

SstvRxControlPolicy::SstvRxControlPolicy()
{
    m_snapshot.revision = 1U;
}

SstvRxControlPolicy::SstvRxControlPolicy(SstvRxControlSettings settings)
{
    if (!settingsAreValid(settings)) {
        settings = {};
    }
    m_snapshot.settings = std::move(settings);
    m_snapshot.revision = 1U;
}

SstvRxControlSnapshot SstvRxControlPolicy::snapshot() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

bool SstvRxControlPolicy::replace(SstvRxControlSettings settings)
{
    if (!settingsAreValid(settings)) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings = std::move(settings);
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setModeControl(SstvRxModeControl control,
                                         std::string manualMode)
{
    if (!modeControlIsValid(control)
        || (control == SstvRxModeControl::Manual
            && !modeNameIsValid(manualMode))
        || (control == SstvRxModeControl::Automatic
            && !manualMode.empty() && !modeNameIsValid(manualMode))) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.modeControl = control;
    m_snapshot.settings.manualMode = std::move(manualMode);
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setModeLock(bool enabled, std::string lockedMode)
{
    if ((enabled && !modeNameIsValid(lockedMode))
        || (!lockedMode.empty() && !modeNameIsValid(lockedMode))) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.modeLockEnabled = enabled;
    m_snapshot.settings.lockedMode = std::move(lockedMode);
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setReceiveWithoutVis(bool enabled)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.receiveWithoutVis = enabled;
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setTimingFallbackEnabled(bool enabled)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.timingFallbackEnabled = enabled;
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setAfc(SstvRxAfcMode mode,
                                 double manualCorrectionHz)
{
    if (!afcModeIsValid(mode)
        || !finiteWithin(manualCorrectionHz,
                         MaximumFrequencyCorrectionHz)) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.afcMode = mode;
    m_snapshot.settings.manualFrequencyCorrectionHz = manualCorrectionHz;
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setSlant(SstvRxSlantMode mode,
                                   double manualClockErrorPpm)
{
    if (!slantModeIsValid(mode)
        || !finiteWithin(manualClockErrorPpm,
                         MaximumAbsoluteClockErrorPpm)) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.slantMode = mode;
    m_snapshot.settings.manualClockErrorPpm = manualClockErrorPpm;
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setReplayRetentionSeconds(std::uint32_t seconds)
{
    if (seconds < MinimumReplayRetentionSeconds
        || seconds > MaximumReplayRetentionSeconds) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.replayRetentionSeconds = seconds;
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setRetainRawAudio(bool enabled)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.retainRawAudio = enabled;
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::setDiagnosticScopeEnabled(bool enabled)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.settings.diagnosticScopeEnabled = enabled;
    increment(m_snapshot.revision);
    return true;
}

void SstvRxControlPolicy::requestAfcReset() noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    increment(m_snapshot.afcResetSerial);
    increment(m_snapshot.revision);
}

void SstvRxControlPolicy::requestSlantReset() noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    increment(m_snapshot.slantResetSerial);
    increment(m_snapshot.revision);
}

bool SstvRxControlPolicy::requestRedecode(
    SstvRxRedecodeParameters parameters)
{
    if (!redecodeParametersAreValid(parameters)) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.redecode = std::move(parameters);
    increment(m_snapshot.redecodeSerial);
    increment(m_snapshot.revision);
    return true;
}

bool SstvRxControlPolicy::settingsAreValid(
    const SstvRxControlSettings& settings) noexcept
{
    if (!modeControlIsValid(settings.modeControl)
        || !afcModeIsValid(settings.afcMode)
        || !slantModeIsValid(settings.slantMode)
        || (settings.modeControl == SstvRxModeControl::Manual
            && !modeNameIsValid(settings.manualMode))
        || (!settings.manualMode.empty()
            && !modeNameIsValid(settings.manualMode))
        || (settings.modeLockEnabled
            && !modeNameIsValid(settings.lockedMode))
        || (!settings.lockedMode.empty()
            && !modeNameIsValid(settings.lockedMode))
        || !finiteWithin(settings.manualFrequencyCorrectionHz,
                         MaximumFrequencyCorrectionHz)
        || !finiteWithin(settings.manualClockErrorPpm,
                         MaximumAbsoluteClockErrorPpm)
        || settings.replayRetentionSeconds < MinimumReplayRetentionSeconds
        || settings.replayRetentionSeconds > MaximumReplayRetentionSeconds) {
        return false;
    }
    return true;
}

bool SstvRxControlPolicy::redecodeParametersAreValid(
    const SstvRxRedecodeParameters& parameters) noexcept
{
    return (parameters.mode.empty() || modeNameIsValid(parameters.mode))
        && afcModeIsValid(parameters.afcMode)
        && slantModeIsValid(parameters.slantMode)
        && finiteWithin(parameters.frequencyCorrectionHz,
                        MaximumFrequencyCorrectionHz)
        && finiteWithin(parameters.clockErrorPpm,
                        MaximumAbsoluteClockErrorPpm);
}

bool SstvRxControlPolicy::modeNameIsValid(const std::string& mode) noexcept
{
    if (mode.empty() || mode.size() > MaximumModeCharacters) {
        return false;
    }
    for (const char rawCharacter : mode) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        const bool alphaNumeric = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9');
        if (!alphaNumeric && character != '-' && character != '_'
            && character != '.') {
            return false;
        }
    }
    return true;
}

bool SstvRxControlPolicy::afcModeIsValid(SstvRxAfcMode mode) noexcept
{
    return mode == SstvRxAfcMode::Off
        || mode == SstvRxAfcMode::Automatic
        || mode == SstvRxAfcMode::Manual;
}

bool SstvRxControlPolicy::slantModeIsValid(SstvRxSlantMode mode) noexcept
{
    return mode == SstvRxSlantMode::Off
        || mode == SstvRxSlantMode::Automatic
        || mode == SstvRxSlantMode::Manual;
}

bool SstvRxControlPolicy::modeControlIsValid(
    SstvRxModeControl control) noexcept
{
    return control == SstvRxModeControl::Automatic
        || control == SstvRxModeControl::Manual;
}

void SstvRxControlPolicy::increment(std::uint64_t& serial) noexcept
{
    if (serial != std::numeric_limits<std::uint64_t>::max()) {
        ++serial;
    }
}

} // namespace decodium::sstv
