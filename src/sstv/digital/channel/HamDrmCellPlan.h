// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"
#include "../phy/HamDrmOfdmParameters.h"
#include "../phy/HamDrmComplexTransform.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace decodium::sstv::hamdrm::channel {

enum class HamDrmCellKind : std::uint16_t {
    None = 0U,
    Msc = 1U << 0U,
    Fac = 1U << 1U,
    TimePilot = 1U << 2U,
    FrequencyPilot = 1U << 3U,
    ScatteredPilot = 1U << 4U,
    BoostedPilot = 1U << 5U,
    DummyMsc = 1U << 6U,
};

HamDrmCellKind operator|(HamDrmCellKind left, HamDrmCellKind right) noexcept;
HamDrmCellKind& operator|=(HamDrmCellKind& left,
                           HamDrmCellKind right) noexcept;
bool hamDrmCellHas(HamDrmCellKind value, HamDrmCellKind flag) noexcept;

struct HamDrmCellDescriptor final
{
    int carrier {0};
    HamDrmCellKind kind {HamDrmCellKind::None};
    phy::HamDrmComplex pilotValue {};
};

struct HamDrmCellPlan final
{
    phy::HamDrmOfdmParameters parameters;
    std::vector<HamDrmCellDescriptor> cells;
    std::vector<std::size_t> usefulMscCellsPerSymbol;
    std::vector<std::size_t> facCellsPerSymbol;
    std::size_t usefulMscCellsPerFrame {0U};
    std::size_t dummyMscCellsPerSuperframe {0U};

    std::size_t symbolsPerSuperframe() const noexcept;
    const HamDrmCellDescriptor* symbolCells(
        std::size_t absoluteSymbol) const noexcept;
};

struct HamDrmExtractedCellSymbol final
{
    std::vector<phy::HamDrmComplex> mscCells;
    std::vector<phy::HamDrmComplex> facCells;
    double pilotSquaredError {0.0};
    std::size_t pilotCells {0U};
    std::size_t corruptedPilotCells {0U};
};

HamDrmValueResult<HamDrmCellPlan> hamDrmBuildCellPlan(
    const phy::HamDrmOfdmParameters& parameters);

HamDrmValueResult<std::vector<phy::HamDrmComplex>> hamDrmMapCellSymbol(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& mscCells,
    const std::vector<phy::HamDrmComplex>& facCells,
    HamDrmConstellation mscConstellation);

HamDrmValueResult<HamDrmExtractedCellSymbol> hamDrmExtractCellSymbol(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers,
    double corruptedPilotSquaredErrorThreshold);

} // namespace decodium::sstv::hamdrm::channel
