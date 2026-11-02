// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxStop.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {
SstvRadiofaxStopDetector::SstvRadiofaxStopDetector(std::uint32_t sampleRate, double frequencyOffsetHz)
    : rate_(sampleRate), offset_(frequencyOffsetHz)
{
    if (rate_ < 8'000U || rate_ > 384'000U || !std::isfinite(offset_) || std::abs(offset_) > 400.0)
        throw std::invalid_argument("invalid radiofax stop detector configuration");
    cosTable_.resize(2U * rate_);
    sinTable_.resize(2U * rate_);
    for (std::uint32_t i = 0U; i < 2U * rate_; ++i) {
        const double phase = 2.0 * std::acos(-1.0) * static_cast<double>(i) / (2U * rate_);
        cosTable_[i] = std::cos(phase);
        sinTable_[i] = std::sin(phase);
    }
}
void SstvRadiofaxStopDetector::clearWindow(std::uint64_t start) noexcept
{
    start_ = start; total_ = valid_ = 0U; real_.fill(0.0); imag_.fill(0.0); absolute_ = mean_ = 0.0;
}
void SstvRadiofaxStopDetector::reset() noexcept
{
    clearWindow(0); last_ = 0; matches_.fill(0U); firstMatches_.fill(0U);
    active_ = hasLast_ = false; detection_.reset();
}
std::optional<std::uint64_t> SstvRadiofaxStopDetector::consume(
    const SstvFrequencyObservation* observations, std::size_t count)
{
    if (count > MaximumObservationsPerConsume) throw std::length_error("radiofax stop observation bound");
    if (count && !observations) throw std::invalid_argument("null radiofax stop observations");
    auto previous = last_; bool have = hasLast_;
    for (std::size_t i = 0; i < count; ++i) {
        if (have && observations[i].centreSample < previous)
            throw std::invalid_argument("radiofax stop timestamp regression");
        previous = observations[i].centreSample; have = true;
    }
    if (detection_) return detection_;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& obs = observations[i];
        if (hasLast_ && obs.centreSample == last_) continue;
        if (!active_ || (hasLast_ && obs.centreSample - last_ > rate_ / 100U)) {
            matches_.fill(0U); clearWindow(obs.centreSample); active_ = true;
        }
        if (obs.centreSample - start_ >= rate_) {
            const bool validWindow = total_ >= 100U && valid_ * 5U >= total_ * 3U
                && absolute_ >= 100.0 * static_cast<double>(valid_)
                && std::abs(mean_) <= 100.0 * static_cast<double>(valid_);
            for (std::size_t bin = 0U; bin < kModulationBins; ++bin) {
                const bool match = validWindow && 2.0 * std::hypot(real_[bin], imag_[bin]) >= 0.85 * absolute_;
                if (match) {
                    if (matches_[bin] == 0U) firstMatches_[bin] = start_;
                    if (++matches_[bin] >= 2U) { detection_ = firstMatches_[bin]; return detection_; }
                } else matches_[bin] = 0U;
            }
            clearWindow(obs.centreSample);
        }
        ++total_;
        if (obs.valid() && std::isfinite(obs.correctedFrequencyHz)
            && std::isfinite(obs.confidence) && obs.confidence >= 0.2) {
            const double value = obs.correctedFrequencyHz - offset_ - 1900.0;
            for (std::size_t bin = 0U; bin < kModulationBins; ++bin) {
                const auto phaseIndex = (obs.centreSample % (2U * rate_)) * (880U + bin) % (2U * rate_);
                real_[bin] += value * cosTable_[phaseIndex];
                imag_[bin] += value * sinTable_[phaseIndex];
            }
            absolute_ += std::abs(value); mean_ += value; ++valid_;
        }
        last_ = obs.centreSample; hasLast_ = true;
    }
    return std::nullopt;
}

SstvRadiofaxAudioStopDetector::SstvRadiofaxAudioStopDetector(std::uint32_t sampleRate)
    : rate_(sampleRate)
{
    if (rate_ < 8'000U || rate_ > 384'000U || rate_ % 10U != 0U)
        throw std::invalid_argument("invalid radiofax PCM stop sample rate");
    for (std::size_t bin = 0; bin < kToneBins; ++bin) {
        const double angle = 2.0 * std::acos(-1.0)
            * (430.0 + 5.0 * static_cast<double>(bin)) / rate_;
        cosStep_[bin] = std::cos(angle);
        sinStep_[bin] = std::sin(angle);
    }
    reset();
}
void SstvRadiofaxAudioStopDetector::reset() noexcept
{
    nextSample_ = 0U;
    minimumSample_ = 0U;
    clearAnalysis();
}
void SstvRadiofaxAudioStopDetector::clearAnalysis() noexcept
{
    count_ = 0U;
    matches_.fill(0U);
    real_.fill(0.0);
    imag_.fill(0.0);
    cosPhase_.fill(1.0);
    sinPhase_.fill(0.0);
    energy_ = 0.0;
    detection_.reset();
}
std::optional<std::uint64_t> SstvRadiofaxAudioStopDetector::consume(
    const float* samples, std::size_t count, bool enabled, std::uint64_t minimumSample)
{
    if (count && !samples) throw std::invalid_argument("null radiofax PCM");
    if (!enabled) {
        nextSample_ += count;
        clearAnalysis();
        return std::nullopt;
    }
    if (minimumSample != minimumSample_) {
        clearAnalysis();
        minimumSample_ = minimumSample;
    }
    if (detection_) {
        nextSample_ += count;
        return detection_;
    }
    if (nextSample_ < minimumSample) {
        const auto skip = static_cast<std::size_t>(std::min<std::uint64_t>(
            minimumSample - nextSample_, static_cast<std::uint64_t>(count)));
        nextSample_ += skip;
        if (skip != 0U) samples += skip;
        count -= skip;
        clearAnalysis();
    }
    const std::size_t window = rate_ / 10U;
    for (std::size_t i = 0; i < count; ++i, ++nextSample_) {
        const double value = std::isfinite(samples[i]) ? samples[i] : 0.0;
        for (std::size_t bin = 0; bin < kToneBins; ++bin) {
            real_[bin] += value * cosPhase_[bin];
            imag_[bin] += value * sinPhase_[bin];
            const double nextCos = cosPhase_[bin] * cosStep_[bin]
                - sinPhase_[bin] * sinStep_[bin];
            sinPhase_[bin] = sinPhase_[bin] * cosStep_[bin]
                + cosPhase_[bin] * sinStep_[bin];
            cosPhase_[bin] = nextCos;
        }
        energy_ += value * value;
        if (++count_ != window) continue;
        for (std::size_t bin = 0; bin < kToneBins; ++bin) {
            const bool match = energy_ >= 0.000025 * double(window)
                && 2.0 * (real_[bin] * real_[bin] + imag_[bin] * imag_[bin])
                    >= 0.75 * double(window) * energy_;
            matches_[bin] = match ? matches_[bin] + 1U : 0U;
            if (matches_[bin] >= 10U) {
                detection_ = nextSample_ + 1U - 10U * window;
                // The caller handed us the entire block, including samples
                // after the detection. Keep the absolute stream clock aligned
                // if the detector is later disabled and rearmed.
                nextSample_ += count - i;
                return detection_;
            }
        }
        count_ = 0U;
        real_.fill(0.0);
        imag_.fill(0.0);
        cosPhase_.fill(1.0);
        sinPhase_.fill(0.0);
        energy_ = 0.0;
    }
    return std::nullopt;
}

} // namespace decodium::sstv
