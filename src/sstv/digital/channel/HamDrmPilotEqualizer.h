// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "HamDrmCellPlan.h"

#include <cstddef>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

struct HamDrmPilotEqualizationResult final
{
    std::vector<phy::HamDrmComplex> carriers;
    phy::HamDrmComplex channelGain {};
    double normalizedPilotSquaredError {0.0};
    std::size_t pilotCells {0U};
    std::size_t corruptedPilotCells {0U};
};

// A bounded single-tap estimate for the flat audio/channel response represented
// by the pinned HAMDRM loop.  It intentionally does not claim multipath or
// frequency-selective equalization.
HamDrmValueResult<HamDrmPilotEqualizationResult>
hamDrmEqualizeFlatPilotChannel(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers,
    double corruptedPilotSquaredErrorThreshold,
    double minimumChannelMagnitude = 1.0e-9);

} // namespace decodium::sstv::hamdrm::channel
