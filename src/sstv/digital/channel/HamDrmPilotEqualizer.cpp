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

HamDrmValueResult<HamDrmPilotEqualizationResult>
hamDrmEqualizeCrossValidatedFlatPilotChannel(
    const HamDrmCellPlan& plan, std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers,
    double corruptedPilotSquaredErrorThreshold, double minimumChannelMagnitude)
{
    auto result = hamDrmEqualizeFlatPilotChannel(plan, absoluteSymbol, carriers,
                                                  corruptedPilotSquaredErrorThreshold,
                                                  minimumChannelMagnitude);
    if (!result.ok()) return result;
    if (!std::all_of(result.value->carriers.begin(), result.value->carriers.end(), finite))
        return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                    "HAMDRM flat equalized carrier is not finite")};
    const auto* descriptors = plan.symbolCells(absoluteSymbol);
    phy::HamDrmComplex cross {};
    double referenceEnergy = 0.0;
    for (std::size_t i = 0; i < carriers.size(); ++i) {
        if (!isPilot(descriptors[i].kind)) continue;
        const auto reference = descriptors[i].pilotValue;
        cross += carriers[i] * std::conj(reference);
        referenceEnergy += std::norm(reference);
    }
    if (!finite(cross) || !std::isfinite(referenceEnergy))
        return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                    "HAMDRM flat pilot energy is not finite")};
    double totalError = 0.0;
    result.value->corruptedPilotCells = 0;
    for (std::size_t i = 0; i < carriers.size(); ++i) {
        if (!isPilot(descriptors[i].kind)) continue;
        const auto reference = descriptors[i].pilotValue;
        const double weight = std::norm(reference);
        const double remaining = referenceEnergy - weight;
        if (!std::isfinite(remaining) || remaining <= std::numeric_limits<double>::epsilon())
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                                        "HAMDRM flat cross-validation needs more pilots")};
        const auto predicted = (cross - carriers[i] * std::conj(reference)) / remaining;
        const auto expected = predicted * reference;
        const double energy = std::max(std::norm(carriers[i]), std::norm(expected));
        if (!finite(predicted) || !std::isfinite(energy)
            || energy <= std::numeric_limits<double>::epsilon())
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                                        "HAMDRM flat cross-validation is singular")};
        const double error = std::norm(carriers[i] - expected) / energy * weight;
        if (!std::isfinite(error) || !std::isfinite(totalError + error))
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                        "HAMDRM flat pilot error is not finite")};
        totalError += error;
        if (error > corruptedPilotSquaredErrorThreshold) ++result.value->corruptedPilotCells;
    }
    result.value->normalizedPilotSquaredError = totalError / referenceEnergy;
    return result;
}

HamDrmValueResult<HamDrmPilotEqualizationResult>
hamDrmEqualizeInterpolatedPilotChannel(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers,
    double corruptedPilotSquaredErrorThreshold,
    double minimumChannelMagnitude)
{
    if (!plan.parameters.isValid() || absoluteSymbol >= plan.symbolsPerSuperframe()
        || carriers.size() != plan.parameters.carrierCount()
        || !std::all_of(carriers.begin(), carriers.end(), finite)
        || !std::isfinite(corruptedPilotSquaredErrorThreshold)
        || corruptedPilotSquaredErrorThreshold < 0.0
        || !std::isfinite(minimumChannelMagnitude) || minimumChannelMagnitude <= 0.0)
        return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                                    "invalid HAMDRM interpolated pilot input")};
    const auto* descriptors = plan.symbolCells(absoluteSymbol);
    if (!descriptors)
        return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                    "HAMDRM cell plan has no symbol row")};
    std::vector<std::size_t> positions;
    std::vector<phy::HamDrmComplex> gains;
    double referenceEnergy = 0.0;
    for (std::size_t i = 0U; i < carriers.size(); ++i) {
        if (!isPilot(descriptors[i].kind)) continue;
        const auto known = descriptors[i].pilotValue;
        if (std::norm(known) <= std::numeric_limits<double>::epsilon())
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                        "HAMDRM pilot reference is zero")};
        positions.push_back(i);
        gains.push_back(carriers[i] / known);
        referenceEnergy += std::norm(known);
    }
    if (positions.size() < 3U)
        return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                                    "HAMDRM interpolation needs three pilots")};
    const auto interpolate = [&positions, &gains](std::size_t position,
                                                  std::size_t first,
                                                  std::size_t second) {
        const double ratio = (static_cast<double>(position) - static_cast<double>(positions[first]))
            / static_cast<double>(positions[second] - positions[first]);
        return gains[first] + ratio * (gains[second] - gains[first]);
    };
    HamDrmPilotEqualizationResult result;
    result.pilotCells = positions.size();
    result.channelGain = gains[gains.size() / 2U];
    result.carriers.resize(carriers.size());
    std::size_t right = 1U;
    for (std::size_t i = 0U; i < carriers.size(); ++i) {
        while (right + 1U < positions.size() && i > positions[right]) ++right;
        phy::HamDrmComplex gain;
        if (i <= positions.front()) gain = gains.front();
        else if (i >= positions.back()) gain = gains.back();
        else gain = interpolate(i, right - 1U, right);
        if (!finite(gain) || std::abs(gain) < minimumChannelMagnitude)
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                                        "HAMDRM interpolated channel estimate is singular")};
        result.carriers[i] = carriers[i] / gain;
        if (!finite(result.carriers[i]))
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                        "HAMDRM equalized carrier is not finite")};
    }
    double totalError = 0.0;
    for (std::size_t p = 0U; p < positions.size(); ++p) {
        const std::size_t first = p == 0U ? 1U : p - 1U;
        const std::size_t second = p == 0U ? 2U
            : p + 1U == positions.size() ? p - 2U : p + 1U;
        // Order of endpoints is immaterial for interpolation, but avoid an
        // unsigned subtraction when extrapolating at the final pilot.
        const auto predicted = first < second
            ? interpolate(positions[p], first, second)
            : interpolate(positions[p], second, first);
        if (!finite(predicted) || std::abs(predicted) < minimumChannelMagnitude)
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                                        "HAMDRM cross-validated pilot estimate is singular")};
        const auto reference = descriptors[positions[p]].pilotValue;
        const auto expected = predicted * reference;
        // A deep fade must not turn one leave-one-out residual into an
        // unbounded frame score. Normalise symmetrically by the larger of
        // observed and predicted energy, while retaining pilot weighting.
        const double energy = std::max(std::norm(carriers[positions[p]]),
                                       std::norm(expected));
        if (!std::isfinite(energy) || energy <= std::numeric_limits<double>::epsilon())
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Incomplete,
                                                        "HAMDRM pilot energy is too small")};
        const double error = std::norm(carriers[positions[p]] - expected)
            / energy * std::norm(reference);
        if (!std::isfinite(error))
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                        "HAMDRM interpolated pilot error is not finite")};
        totalError += error;
        if (!std::isfinite(totalError))
            return {std::nullopt, HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                                        "HAMDRM total pilot error is not finite")};
        if (error > corruptedPilotSquaredErrorThreshold) ++result.corruptedPilotCells;
    }
    result.normalizedPilotSquaredError = totalError / referenceEnergy;
    return {std::move(result), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm::channel
