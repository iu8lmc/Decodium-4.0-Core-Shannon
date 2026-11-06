// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmOfdmParameters.h"

#include <limits>

namespace decodium::sstv::hamdrm::phy {

namespace {

constexpr std::uint32_t kNativeSampleRateHz = 48'000U;

} // namespace

std::size_t HamDrmOfdmParameters::carrierCount() const noexcept
{
    if (minimumCarrier < 0 || maximumCarrier < minimumCarrier) {
        return 0U;
    }
    return static_cast<std::size_t>(maximumCarrier - minimumCarrier + 1);
}

std::size_t HamDrmOfdmParameters::symbolSamples() const noexcept
{
    if (fftSize > std::numeric_limits<std::size_t>::max()
            - guardIntervalSamples) {
        return 0U;
    }
    return fftSize + guardIntervalSamples;
}

double HamDrmOfdmParameters::carrierSpacingHz() const noexcept
{
    if (fftSize == 0U) {
        return 0.0;
    }
    return static_cast<double>(sampleRateHz) / static_cast<double>(fftSize);
}

std::uint32_t HamDrmOfdmParameters::nominalBandwidthHz() const noexcept
{
    switch (occupiedBandwidth) {
    case HamDrmOccupiedBandwidth::Hz2300:
        return 2'300U;
    case HamDrmOccupiedBandwidth::Hz2500:
        return 2'500U;
    }
    return 0U;
}

bool HamDrmOfdmParameters::isValid() const noexcept
{
    if (sampleRateHz != kNativeSampleRateHz || fftSize == 0U
            || guardIntervalSamples == 0U || symbolsPerFrame == 0U
            || minimumCarrier <= 0 || maximumCarrier < minimumCarrier
            || static_cast<std::size_t>(maximumCarrier) >= fftSize
            || nominalBandwidthHz() == 0U || symbolSamples() == 0U) {
        return false;
    }

    const auto expected = hamDrmOfdmParameters(robustness,
                                                occupiedBandwidth);
    return expected.has_value()
        && expected->sampleRateHz == sampleRateHz
        && expected->fftSize == fftSize
        && expected->guardIntervalSamples == guardIntervalSamples
        && expected->symbolsPerFrame == symbolsPerFrame
        && expected->minimumCarrier == minimumCarrier
        && expected->maximumCarrier == maximumCarrier;
}

std::optional<HamDrmOfdmParameters> hamDrmOfdmParameters(
    HamDrmRobustness robustness,
    HamDrmOccupiedBandwidth occupiedBandwidth) noexcept
{
    int maximumCarrier = 0;
    switch (occupiedBandwidth) {
    case HamDrmOccupiedBandwidth::Hz2300:
        break;
    case HamDrmOccupiedBandwidth::Hz2500:
        break;
    default:
        return std::nullopt;
    }

    // A/B preserve the ETSI useful-symbol and guard ratios after exact 48 kHz
    // discretization.  The amateur "E" row below is the narrow-band QSSTV
    // compatibility variant (640/320, 20 symbols), not broadcast DRM mode E.
    switch (robustness) {
    case HamDrmRobustness::A:
        maximumCarrier = occupiedBandwidth
                == HamDrmOccupiedBandwidth::Hz2300 ? 54 : 58;
        return HamDrmOfdmParameters {
            robustness,
            occupiedBandwidth,
            kNativeSampleRateHz,
            1'152U,
            128U,
            15U,
            2,
            maximumCarrier
        };
    case HamDrmRobustness::B:
        maximumCarrier = occupiedBandwidth
                == HamDrmOccupiedBandwidth::Hz2300 ? 45 : 51;
        return HamDrmOfdmParameters {
            robustness,
            occupiedBandwidth,
            kNativeSampleRateHz,
            1'024U,
            256U,
            15U,
            1,
            maximumCarrier
        };
    case HamDrmRobustness::E:
        maximumCarrier = occupiedBandwidth
                == HamDrmOccupiedBandwidth::Hz2300 ? 29 : 31;
        return HamDrmOfdmParameters {
            robustness,
            occupiedBandwidth,
            kNativeSampleRateHz,
            640U,
            320U,
            20U,
            1,
            maximumCarrier
        };
    }
    return std::nullopt;
}

} // namespace decodium::sstv::hamdrm::phy
