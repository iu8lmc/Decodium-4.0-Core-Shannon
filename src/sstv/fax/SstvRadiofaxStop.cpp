// SPDX-License-Identifier: GPL-3.0-or-later
#include "SstvRadiofaxStop.h"
#include <cmath>
#include <stdexcept>

namespace decodium::sstv {
SstvRadiofaxStopDetector::SstvRadiofaxStopDetector(std::uint32_t sampleRate, double frequencyOffsetHz)
    : rate_(sampleRate), offset_(frequencyOffsetHz)
{
    if (rate_ < 8'000U || rate_ > 384'000U || !std::isfinite(offset_) || std::abs(offset_) > 400.0)
        throw std::invalid_argument("invalid radiofax stop detector configuration");
}
void SstvRadiofaxStopDetector::clearWindow(std::uint64_t start) noexcept
{
    start_ = start; total_ = valid_ = 0U; real_ = imag_ = absolute_ = mean_ = 0.0;
}
void SstvRadiofaxStopDetector::reset() noexcept
{
    clearWindow(0); last_ = firstMatch_ = 0; matches_ = 0; active_ = hasLast_ = false; detection_.reset();
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
            matches_ = 0U; clearWindow(obs.centreSample); active_ = true;
        }
        if (obs.centreSample - start_ >= rate_) {
            const bool match = total_ >= 100U && valid_ * 5U >= total_ * 3U
                && absolute_ >= 100.0 * static_cast<double>(valid_)
                && std::abs(mean_) <= 100.0 * static_cast<double>(valid_)
                && 2.0 * std::hypot(real_, imag_) >= 0.85 * absolute_;
            if (match) {
                if (matches_ == 0U) firstMatch_ = start_;
                if (++matches_ >= 2U) { detection_ = firstMatch_; return detection_; }
            } else matches_ = 0U;
            clearWindow(obs.centreSample);
        }
        ++total_;
        if (obs.valid() && std::isfinite(obs.correctedFrequencyHz)
            && std::isfinite(obs.confidence) && obs.confidence >= 0.2) {
            const double value = obs.correctedFrequencyHz - offset_ - 1900.0;
            const auto phaseIndex = (obs.centreSample % rate_) * 450U % rate_;
            const double phase = 2.0 * std::acos(-1.0) * static_cast<double>(phaseIndex) / rate_;
            real_ += value * std::cos(phase); imag_ += value * std::sin(phase);
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
    const float* samples, std::size_t count, bool enabled)
{
    if (count && !samples) throw std::invalid_argument("null radiofax PCM");
    if (detection_) return detection_;
    if (!enabled) {
        nextSample_ += count;
        count_ = 0U;
        matches_.fill(0U);
        real_.fill(0.0);
        imag_.fill(0.0);
        cosPhase_.fill(1.0);
        sinPhase_.fill(0.0);
        energy_ = 0.0;
        return std::nullopt;
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
                ++nextSample_;
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
