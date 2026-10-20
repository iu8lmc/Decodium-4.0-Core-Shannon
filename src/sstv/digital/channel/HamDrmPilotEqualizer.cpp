// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmPilotEqualizer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace decodium::sstv::hamdrm::channel {
namespace {

bool isPilot(HamDrmCellKind kind) noexcept
{
    return hamDrmCellHas(kind, HamDrmCellKind::ScatteredPilot)
        || hamDrmCellHas(kind, HamDrmCellKind::TimePilot)
        || hamDrmCellHas(kind, HamDrmCellKind::FrequencyPilot);
}

bool finite(const phy::HamDrmComplex& value) noexcept
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

} // namespace

HamDrmValueResult<HamDrmPilotEqualizationResult>
hamDrmEqualizeFlatPilotChannel(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers,
    double corruptedPilotSquaredErrorThreshold,
    double minimumChannelMagnitude)
{
    if (!plan.parameters.isValid()
            || absoluteSymbol >= plan.symbolsPerSuperframe()
            || carriers.size() != plan.parameters.carrierCount()
            || !std::all_of(carriers.begin(), carriers.end(), finite)
            || !std::isfinite(corruptedPilotSquaredErrorThreshold)
            || corruptedPilotSquaredErrorThreshold < 0.0
            || !std::isfinite(minimumChannelMagnitude)
            || minimumChannelMagnitude <= 0.0) {
        return {std::nullopt,
                HamDrmStatus::failure(
                    HamDrmErrorCode::InvalidArgument,
                    "invalid HAMDRM pilot equalizer input")};
    }
    const auto* descriptors = plan.symbolCells(absoluteSymbol);
    if (descriptors == nullptr) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "HAMDRM cell plan has no symbol row")};
    }

    phy::HamDrmComplex cross {};
    double referenceEnergy = 0.0;
    std::size_t pilotCells = 0U;
    for (std::size_t index = 0U; index < carriers.size(); ++index) {
        if (isPilot(descriptors[index].kind)) {
            cross += carriers[index]
                * std::conj(descriptors[index].pilotValue);
            referenceEnergy += std::norm(descriptors[index].pilotValue);
            ++pilotCells;
        }
    }
    if (pilotCells == 0U
            || referenceEnergy <= std::numeric_limits<double>::epsilon()) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "HAMDRM symbol contains no usable pilots")};
    }
    const phy::HamDrmComplex gain = cross / referenceEnergy;
    if (!finite(gain) || std::abs(gain) < minimumChannelMagnitude) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                      "HAMDRM pilot channel estimate is singular")};
    }

    HamDrmPilotEqualizationResult result;
    result.channelGain = gain;
    result.pilotCells = pilotCells;
    result.carriers.reserve(carriers.size());
    for (const auto& carrier : carriers) {
        result.carriers.push_back(carrier / gain);
    }
    double error = 0.0;
    for (std::size_t index = 0U; index < carriers.size(); ++index) {
        if (isPilot(descriptors[index].kind)) {
            const double squared = std::norm(
                result.carriers[index] - descriptors[index].pilotValue);
            error += squared;
            if (squared > corruptedPilotSquaredErrorThreshold) {
                ++result.corruptedPilotCells;
            }
        }
    }
    result.normalizedPilotSquaredError = error / referenceEnergy;
    return {std::move(result), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm::channel
