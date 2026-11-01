// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxPhasing.h"
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {

SstvRadiofaxClockTracker::SstvRadiofaxClockTracker(std::uint32_t sampleRate,
                                                   SstvRadiofaxPhaseLock lock,
                                                   double frequencyOffsetHz)
    : rate_(sampleRate), lock_(lock), offset_(frequencyOffsetHz),
      edge_(lock.latestWhiteEdgeSample), lastObservation_(lock.latestWhiteEdgeSample)
{
    if (sampleRate < 8000U || sampleRate > 384000U
        || (lock.linesPerMinute != 60U && lock.linesPerMinute != 90U
            && lock.linesPerMinute != 120U && lock.linesPerMinute != 240U)
        || (lock.pattern != SstvFaxPhasePattern::Symmetric
            && lock.pattern != SstvFaxPhasePattern::WhiteFivePercent)
        || !std::isfinite(offset_) || std::abs(offset_) > 400.0)
        throw std::invalid_argument("invalid radiofax clock tracker configuration");
}

void SstvRadiofaxClockTracker::consume(const SstvFrequencyObservation& obs) noexcept
{
    if (!valid_) return;
    if (obs.centreSample < lastObservation_
        || obs.centreSample - lock_.latestWhiteEdgeSample > std::uint64_t(rate_) * 3600U) {
        valid_ = false;
        return;
    }
    lastObservation_ = obs.centreSample;
    if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
        || !std::isfinite(obs.confidence) || obs.confidence < 0.2 || obs.confidence > 1.0) return;
    const double frequency = obs.correctedFrequencyHz - offset_;
    const int tone = frequency >= 1350.0 && frequency <= 1700.0 ? -1
        : (frequency >= 2100.0 && frequency <= 2450.0 ? 1 : 0);
    if (!tone || tone == tone_) return;
    if (tone == 1) {
        edge_ = obs.centreSample;
    } else {
        const double nominal = double(rate_) * 60.0 / lock_.linesPerMinute;
        const double width = double(obs.centreSample - edge_) / nominal;
        const double expected = lock_.pattern == SstvFaxPhasePattern::Symmetric ? 0.5 : 0.05;
        const double tolerance = lock_.pattern == SstvFaxPhasePattern::Symmetric ? 0.06 : 0.025;
        if (std::abs(width - expected) <= tolerance) {
            if (!haveFirst_) {
                first_ = last_ = edge_;
                haveFirst_ = true;
                pulses_ = 1U;
            } else if (edge_ > last_) {
                const double elapsed = double(edge_ - last_);
                const auto lines = static_cast<std::uint64_t>(std::llround(elapsed / nominal));
                // A long or irregular gap can be image content, not phasing.
                const double observed = lines ? elapsed / double(lines) : 0.0;
                const double tracked = lines_ ? double(last_ - first_) / lines_ : observed;
                if (lines == 0U || lines > 4U
                    || std::abs(observed - nominal) > nominal * kRadiofaxMaximumClockDeviation
                    || (lines_ && std::abs(observed - tracked) > nominal * 0.005)) {
                    valid_ = false;
                    return;
                }
                lines_ += lines;
                last_ = edge_;
                ++pulses_;
            }
        }
    }
    tone_ = tone;
}

std::optional<SstvRadiofaxClockEstimate> SstvRadiofaxClockTracker::estimate() const noexcept
{
    if (!valid_ || pulses_ < 6U || lines_ < 8U || last_ <= first_
        || last_ - first_ < std::uint64_t(rate_) * 2U) return std::nullopt;
    return SstvRadiofaxClockEstimate {last_ - first_, lines_};
}

SstvRadiofaxPhasingDetector::SstvRadiofaxPhasingDetector(std::uint32_t sampleRate,
                                                       double frequencyOffsetHz)
    : sampleRate_(sampleRate), offset_(frequencyOffsetHz)
{
    if (sampleRate < 8000U || sampleRate > 384000U || !std::isfinite(offset_)
        || std::abs(offset_) > 400.0) throw std::invalid_argument("invalid radiofax phasing configuration");
}

std::optional<SstvRadiofaxPhaseLock> SstvRadiofaxPhasingDetector::consume(
    const SstvFrequencyObservation* data, std::size_t count)
{
    if (count > MaximumObservationsPerConsume) throw std::length_error("radiofax phasing observation bound");
    if (count && !data) throw std::invalid_argument("null radiofax phasing observations");
    std::uint64_t previous = lastSample_;
    bool have = hasLast_;
    for (std::size_t i = 0; i < count; ++i) {
        if (have && data[i].centreSample < previous)
            throw std::invalid_argument("radiofax phasing timestamp regression");
        previous = data[i].centreSample;
        have = true;
    }
    for (std::size_t i = 0; i < count && !lock_; ++i) {
        const auto& obs = data[i];
        lastSample_ = obs.centreSample;
        hasLast_ = true;
        if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
            || !std::isfinite(obs.confidence) || obs.confidence < 0.20 || obs.confidence > 1.0) continue;
        const double f = obs.correctedFrequencyHz - offset_;
        const int tone = f >= 1350.0 && f <= 1700.0 ? -1
            : (f >= 2100.0 && f <= 2450.0 ? 1 : 0);
        if (!tone || tone == tone_) continue;
        if (tone == -1 && tone_ == 1 && hasWhiteEdge_) {
            whiteDuration_ = obs.centreSample - whiteEdge_;
            hasWhiteDuration_ = true;
        } else if (tone == 1 && tone_ == -1) {
            if (hasWhiteEdge_ && hasWhiteDuration_) {
                const auto period = obs.centreSample - whiteEdge_;
                std::uint16_t rpm = 0U;
                for (const std::uint16_t candidate : {60U, 90U, 120U, 240U}) {
                    const double expected = static_cast<double>(sampleRate_) * 60.0 / candidate;
                    if (std::abs(static_cast<double>(period) - expected) <= expected * kRadiofaxMaximumClockDeviation) {
                        rpm = candidate;
                        break;
                    }
                }
                const double whiteFraction = period ? static_cast<double>(whiteDuration_) / period : 0.0;
                const bool symmetric = std::abs(whiteFraction - 0.5) <= 0.06;
                const bool shortWhite = std::abs(whiteFraction - 0.05) <= 0.025;
                if (rpm && (symmetric || shortWhite)) {
                    const auto pattern = symmetric ? SstvFaxPhasePattern::Symmetric
                                                   : SstvFaxPhasePattern::WhiteFivePercent;
                    matches_ = candidateRpm_ == rpm && candidatePattern_ == pattern ? matches_ + 1U : 1U;
                    candidateRpm_ = rpm;
                    candidatePattern_ = pattern;
                    if (matches_ >= 3U) lock_ = SstvRadiofaxPhaseLock {rpm, pattern, obs.centreSample, period};
                } else {
                    matches_ = 0U;
                    candidateRpm_ = 0U;
                }
            }
            whiteEdge_ = obs.centreSample;
            hasWhiteEdge_ = true;
            hasWhiteDuration_ = false;
        }
        tone_ = tone;
    }
    return lock_;
}

std::optional<SstvRadiofaxPhaseLock> SstvRadiofaxPhasingDetector::consume(
    const std::vector<SstvFrequencyObservation>& data)
{
    return consume(data.data(), data.size());
}

void SstvRadiofaxPhasingDetector::reset() noexcept
{
    lastSample_ = whiteEdge_ = whiteDuration_ = 0U;
    hasLast_ = hasWhiteEdge_ = hasWhiteDuration_ = false;
    tone_ = 0;
    candidateRpm_ = 0U;
    matches_ = 0U;
    lock_.reset();
}

} // namespace decodium::sstv

namespace decodium::sstv {

SstvRadiofaxImageStartDetector::SstvRadiofaxImageStartDetector(
    std::uint32_t sampleRate, SstvRadiofaxPhaseLock lock, double frequencyOffsetHz)
    : sampleRate_(sampleRate), lock_(lock), offset_(frequencyOffsetHz),
      gapLimit_(sampleRate / 40U)
{
    if (sampleRate < 8000U || sampleRate > 384000U
        || (lock.linesPerMinute != 60U && lock.linesPerMinute != 90U
            && lock.linesPerMinute != 120U && lock.linesPerMinute != 240U)
        || (lock.pattern != SstvFaxPhasePattern::Symmetric
            && lock.pattern != SstvFaxPhasePattern::WhiteFivePercent)
        || !std::isfinite(offset_) || std::abs(offset_) > 400.0
        || lock.latestWhiteEdgeSample > UINT64_MAX - static_cast<std::uint64_t>(sampleRate) * 3601U) {
        throw std::invalid_argument("invalid radiofax image-start detector configuration");
    }
    const double nominal = double(sampleRate_) * 60.0 / lock_.linesPerMinute;
    // Retain the nominal anchor for ordinary small offsets: a single measured
    // period can include demodulator edge jitter. Only seed early tracking
    // when the offset would otherwise invalidate phasing before the longer
    // multi-pulse estimate becomes available.
    if (lock.detectedPeriodSamples
        && std::abs(double(lock.detectedPeriodSamples) - nominal) > nominal * 0.005)
        setClockEstimate(SstvRadiofaxClockEstimate {lock.detectedPeriodSamples, 1U});
}

std::uint64_t SstvRadiofaxImageStartDetector::lineStart(std::uint64_t line) const noexcept
{
    if (clock_) return lock_.latestWhiteEdgeSample + line * clock_->samples / clock_->lines;
    return lock_.latestWhiteEdgeSample + line * sampleRate_ * 60U / lock_.linesPerMinute;
}

void SstvRadiofaxImageStartDetector::setClockEstimate(
    std::optional<SstvRadiofaxClockEstimate> estimate) noexcept
{
    if (!estimate) { clock_.reset(); return; }
    if (estimate->lines == 0U || estimate->samples == 0U
        || estimate->samples > std::uint64_t(sampleRate_) * 3600U
        || estimate->lines > 14'400U) return;
    const double nominal = double(sampleRate_) * 60.0 / lock_.linesPerMinute;
    if (std::abs(double(estimate->samples) / double(estimate->lines) - nominal)
        > nominal * kRadiofaxMaximumClockDeviation) return;
    clock_ = estimate;
}

void SstvRadiofaxImageStartDetector::finishLine()
{
    if (expectedWhite_ >= 4U && expectedBlack_ >= 4U
        && (matchedWhite_ * 2U < expectedWhite_ || matchedBlack_ * 4U < expectedBlack_ * 3U)) {
        result_ = lineStart(line_);
    }
    expectedWhite_ = expectedBlack_ = matchedWhite_ = matchedBlack_ = 0U;
}

std::optional<std::uint64_t> SstvRadiofaxImageStartDetector::consume(
    const SstvFrequencyObservation* data, std::size_t count)
{
    if (count > SstvRadiofaxPhasingDetector::MaximumObservationsPerConsume)
        throw std::length_error("radiofax image-start observation bound");
    if (count && !data) throw std::invalid_argument("null radiofax image-start observations");
    if (!valid_) return std::nullopt;
    auto previous = lastSample_;
    bool have = hasLast_;
    for (std::size_t i = 0; i < count; ++i) {
        if (have && data[i].centreSample < previous)
            throw std::invalid_argument("radiofax image-start timestamp regression");
        previous = data[i].centreSample;
        have = true;
    }
    for (std::size_t i = 0; i < count && !result_; ++i) {
        const auto& obs = data[i];
        if (obs.centreSample < lock_.latestWhiteEdgeSample) continue;
        // Limit tracking to one hour to keep arithmetic and stale locks bounded.
        const auto elapsed = obs.centreSample - lock_.latestWhiteEdgeSample;
        if (elapsed > static_cast<std::uint64_t>(sampleRate_) * 3600U)
            throw std::length_error("radiofax phasing lock expired");
        const auto line = clock_
            ? ((elapsed + 1U) * clock_->lines - 1U) / clock_->samples
            : ((elapsed + 1U) * lock_.linesPerMinute - 1U)
                / (static_cast<std::uint64_t>(sampleRate_) * 60U);
        if ((hasLast_ && obs.centreSample - lastSample_ > gapLimit_)
            || line > line_ + 1U) {
            valid_ = false;
            return std::nullopt;
        }
        if (line != line_) {
            finishLine();
            line_ = line;
            if (result_) break;
        }
        lastSample_ = obs.centreSample;
        hasLast_ = true;
        if (!obs.valid() || !std::isfinite(obs.correctedFrequencyHz)
            || !std::isfinite(obs.confidence) || obs.confidence < 0.2 || obs.confidence > 1.0) continue;
        const auto start = lineStart(line_);
        const auto length = lineStart(line_ + 1U) - start;
        const double position = static_cast<double>(obs.centreSample - start) / static_cast<double>(length);
        const double whiteEnd = lock_.pattern == SstvFaxPhasePattern::Symmetric ? 0.5 : 0.05;
        const double guard = 0.008;
        const double frequency = obs.correctedFrequencyHz - offset_;
        if (position > guard && position < whiteEnd - guard) {
            ++expectedWhite_;
            if (frequency >= 2100.0 && frequency <= 2450.0) ++matchedWhite_;
        } else if (position > whiteEnd + guard && position < 1.0 - guard) {
            ++expectedBlack_;
            if (frequency >= 1350.0 && frequency <= 1700.0) ++matchedBlack_;
        }
    }
    return result_;
}

std::optional<std::uint64_t> SstvRadiofaxImageStartDetector::consume(
    const std::vector<SstvFrequencyObservation>& data)
{
    return consume(data.data(), data.size());
}

bool SstvRadiofaxImageStartDetector::valid() const noexcept { return valid_; }

} // namespace decodium::sstv
