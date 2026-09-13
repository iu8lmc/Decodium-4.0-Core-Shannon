// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmOfdmModem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace decodium::sstv::hamdrm::phy {

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

void validateFinite(double value, const char* message)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument(message);
    }
}

} // namespace

HamDrmFrequencyMixResult hamDrmMixFrequency(
    const std::vector<HamDrmComplex>& input,
    double sampleRateHz,
    double frequencyOffsetHz,
    double initialPhaseRadians,
    std::size_t maximumSamples)
{
    validateFinite(sampleRateHz, "non-finite HAMDRM sample rate");
    validateFinite(frequencyOffsetHz, "non-finite HAMDRM frequency offset");
    validateFinite(initialPhaseRadians, "non-finite HAMDRM initial phase");
    if (sampleRateHz <= 0.0) {
        throw std::invalid_argument("HAMDRM sample rate must be positive");
    }
    const std::size_t effectiveMaximum = std::min(
        maximumSamples, kHamDrmMaximumWaveformSamples);
    if (input.size() > effectiveMaximum) {
        throw std::length_error("HAMDRM waveform exceeds mixing bound");
    }

    HamDrmFrequencyMixResult result;
    result.samples.resize(input.size());
    const double phaseIncrement = 2.0 * kPi * frequencyOffsetHz
        / sampleRateHz;
    for (std::size_t index = 0U; index < input.size(); ++index) {
        const double phase = std::remainder(
            initialPhaseRadians
                + phaseIncrement * static_cast<double>(index),
            2.0 * kPi);
        result.samples[index] = input[index]
            * HamDrmComplex {std::cos(phase), std::sin(phase)};
    }
    result.finalPhaseRadians = std::remainder(
        initialPhaseRadians
            + phaseIncrement * static_cast<double>(input.size()),
        2.0 * kPi);
    return result;
}

HamDrmOfdmModem::HamDrmOfdmModem(
    HamDrmOfdmParameters parameters,
    std::shared_ptr<const HamDrmComplexTransform> transform)
    : parameters_(parameters),
      transform_(std::move(transform))
{
    if (!parameters_.isValid()) {
        throw std::invalid_argument("invalid HAMDRM OFDM parameter set");
    }
    if (!transform_) {
        transform_ = std::make_shared<HamDrmMixedRadixTransform>();
    }
    if (parameters_.fftSize > transform_->maximumSize()) {
        throw std::invalid_argument("HAMDRM FFT backend is too small");
    }
}

const HamDrmOfdmParameters& HamDrmOfdmModem::parameters() const noexcept
{
    return parameters_;
}

std::vector<HamDrmComplex> HamDrmOfdmModem::modulateSymbol(
    const std::vector<HamDrmComplex>& carrierCells) const
{
    if (carrierCells.size() != parameters_.carrierCount()) {
        throw std::invalid_argument("HAMDRM carrier-cell count mismatch");
    }
    for (const auto& cell : carrierCells) {
        if (!std::isfinite(cell.real()) || !std::isfinite(cell.imag())) {
            throw std::invalid_argument("non-finite HAMDRM carrier cell");
        }
    }

    std::vector<HamDrmComplex> bins(parameters_.fftSize,
                                    HamDrmComplex {});
    for (std::size_t index = 0U; index < carrierCells.size(); ++index) {
        const int carrier = parameters_.minimumCarrier
            + static_cast<int>(index);
        bins[static_cast<std::size_t>(carrier)] = carrierCells[index];
    }
    const auto useful = transform_->execute(
        bins, HamDrmTransformDirection::Inverse);
    if (useful.size() != parameters_.fftSize) {
        throw std::runtime_error("HAMDRM FFT backend returned wrong size");
    }

    std::vector<HamDrmComplex> waveform;
    waveform.reserve(parameters_.symbolSamples());
    waveform.insert(waveform.end(),
                    useful.end()
                        - static_cast<std::ptrdiff_t>(
                            parameters_.guardIntervalSamples),
                    useful.end());
    waveform.insert(waveform.end(), useful.begin(), useful.end());
    return waveform;
}

std::vector<HamDrmComplex> HamDrmOfdmModem::demodulateSymbol(
    const std::vector<HamDrmComplex>& waveform,
    std::size_t symbolStartSample,
    double frequencyOffsetCorrectionHz,
    double commonPhaseCorrectionRadians) const
{
    validateFinite(frequencyOffsetCorrectionHz,
                   "non-finite HAMDRM frequency correction");
    validateFinite(commonPhaseCorrectionRadians,
                   "non-finite HAMDRM phase correction");
    if (waveform.size() > kHamDrmMaximumWaveformSamples) {
        throw std::length_error("HAMDRM waveform exceeds demodulator bound");
    }
    if (symbolStartSample > waveform.size()
            || parameters_.symbolSamples()
                > waveform.size() - symbolStartSample) {
        throw std::out_of_range("truncated HAMDRM OFDM symbol");
    }

    std::vector<HamDrmComplex> useful(parameters_.fftSize);
    const std::size_t usefulStart = symbolStartSample
        + parameters_.guardIntervalSamples;
    const double correctionIncrement = -2.0 * kPi
        * frequencyOffsetCorrectionHz
        / static_cast<double>(parameters_.sampleRateHz);
    for (std::size_t index = 0U; index < parameters_.fftSize; ++index) {
        const std::size_t absoluteIndex = usefulStart + index;
        const double phase = std::remainder(
            correctionIncrement * static_cast<double>(absoluteIndex)
                - commonPhaseCorrectionRadians,
            2.0 * kPi);
        useful[index] = waveform[absoluteIndex]
            * HamDrmComplex {std::cos(phase), std::sin(phase)};
    }

    const auto bins = transform_->execute(
        useful, HamDrmTransformDirection::Forward);
    if (bins.size() != parameters_.fftSize) {
        throw std::runtime_error("HAMDRM FFT backend returned wrong size");
    }

    std::vector<HamDrmComplex> carrierCells;
    carrierCells.reserve(parameters_.carrierCount());
    for (int carrier = parameters_.minimumCarrier;
         carrier <= parameters_.maximumCarrier; ++carrier) {
        carrierCells.push_back(bins[static_cast<std::size_t>(carrier)]);
    }
    return carrierCells;
}

HamDrmOfdmSyncResult HamDrmOfdmModem::synchronize(
    const std::vector<HamDrmComplex>& waveform,
    std::size_t searchBeginSample,
    std::size_t searchEndSampleExclusive,
    double minimumNormalizedCorrelation) const
{
    if (waveform.size() > kHamDrmMaximumWaveformSamples) {
        throw std::length_error("HAMDRM waveform exceeds synchronizer bound");
    }
    validateFinite(minimumNormalizedCorrelation,
                   "non-finite HAMDRM synchronization threshold");
    if (minimumNormalizedCorrelation < 0.0
            || minimumNormalizedCorrelation > 1.0) {
        throw std::invalid_argument("invalid HAMDRM synchronization threshold");
    }
    if (waveform.size() < parameters_.symbolSamples()) {
        return {};
    }

    const std::size_t finalCandidateExclusive = waveform.size()
        - parameters_.symbolSamples() + 1U;
    const std::size_t end = std::min(searchEndSampleExclusive,
                                     finalCandidateExclusive);
    if (searchBeginSample >= end) {
        return {};
    }
    if (end - searchBeginSample > kHamDrmMaximumSyncCandidates) {
        throw std::length_error("HAMDRM synchronization search exceeds bound");
    }

    double bestMetric = -1.0;
    std::size_t bestStart = searchBeginSample;
    HamDrmComplex bestCorrelation {};
    for (std::size_t candidate = searchBeginSample; candidate < end;
         ++candidate) {
        HamDrmComplex correlation {};
        double prefixEnergy = 0.0;
        double suffixEnergy = 0.0;
        for (std::size_t index = 0U;
             index < parameters_.guardIntervalSamples; ++index) {
            const HamDrmComplex prefix = waveform[candidate + index];
            const HamDrmComplex suffix = waveform[
                candidate + parameters_.fftSize + index];
            correlation += std::conj(prefix) * suffix;
            prefixEnergy += std::norm(prefix);
            suffixEnergy += std::norm(suffix);
        }
        const double denominator = prefixEnergy * suffixEnergy;
        const double metric = denominator
                > std::numeric_limits<double>::epsilon()
            ? std::norm(correlation) / denominator : 0.0;
        if (metric > bestMetric) {
            bestMetric = metric;
            bestStart = candidate;
            bestCorrelation = correlation;
        }
    }

    HamDrmOfdmSyncResult result;
    result.normalizedCorrelation = std::clamp(bestMetric, 0.0, 1.0);
    if (bestMetric < minimumNormalizedCorrelation) {
        return result;
    }
    result.locked = true;
    result.symbolStartSample = bestStart;
    result.coarseFrequencyOffsetHz = std::arg(bestCorrelation)
        * static_cast<double>(parameters_.sampleRateHz)
        / (2.0 * kPi * static_cast<double>(parameters_.fftSize));
    return result;
}

} // namespace decodium::sstv::hamdrm::phy
