// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmComplexTransform.h"
#include "HamDrmOfdmParameters.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace decodium::sstv::hamdrm::phy {

constexpr std::size_t kHamDrmMaximumWaveformSamples = 1U << 20U;
constexpr std::size_t kHamDrmMaximumSyncCandidates = 1U << 16U;

struct HamDrmFrequencyMixResult final
{
    std::vector<HamDrmComplex> samples;
    double finalPhaseRadians {0.0};
};

struct HamDrmOfdmSyncResult final
{
    bool locked {false};
    std::size_t symbolStartSample {0U};
    double normalizedCorrelation {0.0};
    double coarseFrequencyOffsetHz {0.0};
};

// The cyclic-prefix estimator is unambiguous only inside one half of the
// selected mode's carrier spacing.  Integer-carrier ambiguity resolution and
// pilot-aided common-phase/channel estimation belong to the later cell mapper.

HamDrmFrequencyMixResult hamDrmMixFrequency(
    const std::vector<HamDrmComplex>& input,
    double sampleRateHz,
    double frequencyOffsetHz,
    double initialPhaseRadians = 0.0,
    std::size_t maximumSamples = kHamDrmMaximumWaveformSamples);

class HamDrmOfdmModem final
{
public:
    explicit HamDrmOfdmModem(
        HamDrmOfdmParameters parameters,
        std::shared_ptr<const HamDrmComplexTransform> transform = {});

    const HamDrmOfdmParameters& parameters() const noexcept;

    std::vector<HamDrmComplex> modulateSymbol(
        const std::vector<HamDrmComplex>& carrierCells) const;

    std::vector<HamDrmComplex> demodulateSymbol(
        const std::vector<HamDrmComplex>& waveform,
        std::size_t symbolStartSample,
        double frequencyOffsetCorrectionHz = 0.0,
        double commonPhaseCorrectionRadians = 0.0) const;

    HamDrmOfdmSyncResult synchronize(
        const std::vector<HamDrmComplex>& waveform,
        std::size_t searchBeginSample,
        std::size_t searchEndSampleExclusive,
        double minimumNormalizedCorrelation = 0.70) const;

private:
    HamDrmOfdmParameters parameters_;
    std::shared_ptr<const HamDrmComplexTransform> transform_;
};

} // namespace decodium::sstv::hamdrm::phy
