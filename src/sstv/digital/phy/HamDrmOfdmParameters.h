// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace decodium::sstv::hamdrm::phy {

// Native 48 kHz parameters for the narrow-band amateur DRM waveform used by
// the pinned QSSTV compatibility reference.  This is deliberately distinct
// from the full ETSI broadcast-DRM profile space.
struct HamDrmOfdmParameters final
{
    HamDrmRobustness robustness {HamDrmRobustness::A};
    HamDrmOccupiedBandwidth occupiedBandwidth {
        HamDrmOccupiedBandwidth::Hz2300};
    std::uint32_t sampleRateHz {48'000U};
    std::size_t fftSize {0U};
    std::size_t guardIntervalSamples {0U};
    std::size_t symbolsPerFrame {0U};
    int minimumCarrier {0};
    int maximumCarrier {0};

    std::size_t carrierCount() const noexcept;
    std::size_t symbolSamples() const noexcept;
    double carrierSpacingHz() const noexcept;
    std::uint32_t nominalBandwidthHz() const noexcept;
    bool isValid() const noexcept;
};

std::optional<HamDrmOfdmParameters> hamDrmOfdmParameters(
    HamDrmRobustness robustness,
    HamDrmOccupiedBandwidth occupiedBandwidth) noexcept;

} // namespace decodium::sstv::hamdrm::phy
