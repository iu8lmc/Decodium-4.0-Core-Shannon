// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmCellPlan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace decodium::sstv::hamdrm::channel {

namespace {

constexpr std::size_t kFramesPerSuperframe = 3U;

struct PilotEntry final
{
    int carrier;
    int phase;
};

struct FacEntry final
{
    int symbol;
    int carrier;
};

constexpr std::array<FacEntry, 45U> kFacA {{
    {1,10},{1,22},{1,30},{1,50},{2,14},{2,26},{2,34},
    {3,18},{3,30},{3,38},{4,22},{4,34},{4,42},{5,18},
    {5,26},{5,38},{5,46},{6,22},{6,30},{6,42},{6,50},
    {7,26},{7,34},{7,46},{8,10},{8,30},{8,38},{8,50},
    {9,14},{9,34},{9,42},{10,18},{10,38},{10,46},{11,10},
    {11,22},{11,42},{11,50},{12,14},{12,26},{12,46},
    {13,18},{13,30},{14,22},{14,34}
}};

constexpr std::array<FacEntry, 45U> kFacB {{
    {0,21},{1,11},{1,23},{1,35},{2,13},{2,25},{2,37},
    {3,15},{3,27},{3,39},{4,5},{4,17},{4,29},{4,41},
    {5,7},{5,19},{5,31},{6,9},{6,21},{6,33},{7,11},
    {7,23},{7,35},{8,13},{8,25},{8,37},{9,15},{9,27},
    {9,39},{10,5},{10,17},{10,29},{10,41},{11,7},{11,19},
    {11,31},{12,9},{12,21},{12,33},{13,11},{13,23},
    {13,35},{14,13},{14,25},{14,37}
}};

constexpr std::array<FacEntry, 45U> kFacE {{
    {1,7},{1,23},{2,8},{2,16},{2,24},{3,9},{3,17},
    {4,10},{4,18},{5,11},{5,19},{6,4},{6,12},{7,13},
    {7,21},{8,6},{8,14},{8,22},{9,7},{9,23},{10,8},
    {10,16},{10,24},{11,9},{11,13},{11,17},{12,10},
    {12,18},{13,11},{13,19},{14,4},{14,12},{14,16},
    {15,13},{15,21},{16,6},{16,14},{16,22},{17,7},
    {17,23},{18,8},{18,16},{18,24},{19,9},{19,17}
}};

constexpr std::array<PilotEntry, 3U> kFrequencyA {{
    {9,205},{27,836},{36,215}
}};
constexpr std::array<PilotEntry, 3U> kFrequencyB {{
    {8,331},{24,651},{32,555}
}};
constexpr std::array<PilotEntry, 3U> kFrequencyE {{
    {5,788},{15,1014},{20,332}
}};

constexpr std::array<PilotEntry, 16U> kTimeA {{
    {6,973},{7,205},{11,717},{12,264},{15,357},{16,357},
    {23,952},{29,440},{30,856},{33,88},{34,88},{38,68},
    {39,836},{41,836},{45,836},{46,1008}
}};
constexpr std::array<PilotEntry, 15U> kTimeB {{
    {6,304},{10,331},{11,108},{14,620},{17,192},{18,704},
    {27,44},{28,432},{30,588},{33,844},{34,651},{38,651},
    {40,651},{41,460},{44,944}
}};
constexpr std::array<PilotEntry, 8U> kTimeE {{
    {7,432},{8,331},{13,108},{14,620},{21,192},{22,704},
    {26,44},{27,304}
}};

constexpr std::array<int, 15U> kWA {{
    228,341,455, 455,569,683, 683,796,910,
    910,0,114, 114,228,341
}};
constexpr std::array<int, 15U> kZA {{
    0,81,248, 18,106,106, 122,116,31,
    129,129,39, 33,32,111
}};
constexpr std::array<int, 15U> kWB {{
    512,0,512,0,512, 0,512,0,512,0, 512,0,512,0,512
}};
constexpr std::array<int, 15U> kZB {{
    0,57,164,64,12, 168,255,161,106,118,
    25,232,132,233,38
}};
constexpr std::array<int, 20U> kWE {{
    512,0,512,0,512, 0,512,0,512,0,
    512,0,512,0,512, 0,512,0,512,0
}};
constexpr std::array<int, 20U> kZE {{
    0,57,164,64,12, 168,255,161,106,118,
    25,232,132,233,38, 168,255,161,106,118
}};

struct TableView final
{
    const FacEntry* fac;
    std::size_t facSize;
    const PilotEntry* frequency;
    std::size_t frequencySize;
    const PilotEntry* time;
    std::size_t timeSize;
    const int* w;
    const int* z;
    std::size_t wzRows;
    std::size_t wzColumns;
    int x;
    int y;
    int k0;
    int frequencyInterval;
    int timeInterval;
    int q;
    std::array<int, 4U> boosted;
};

TableView tableFor(HamDrmRobustness robustness,
                   HamDrmOccupiedBandwidth bandwidth)
{
    const bool wide = bandwidth == HamDrmOccupiedBandwidth::Hz2500;
    switch (robustness) {
    case HamDrmRobustness::A:
        return {kFacA.data(), kFacA.size(), kFrequencyA.data(),
                kFrequencyA.size(), kTimeA.data(), kTimeA.size(),
                kWA.data(), kZA.data(), 5U, 3U, 4, 5, 2, 4, 5, 36,
                wide ? std::array<int, 4U> {{2,6,54,58}}
                     : std::array<int, 4U> {{2,4,50,54}}};
    case HamDrmRobustness::B:
        return {kFacB.data(), kFacB.size(), kFrequencyB.data(),
                kFrequencyB.size(), kTimeB.data(), kTimeB.size(),
                kWB.data(), kZB.data(), 3U, 5U, 2, 3, 1, 2, 3, 12,
                wide ? std::array<int, 4U> {{1,3,49,51}}
                     : std::array<int, 4U> {{1,3,43,45}}};
    case HamDrmRobustness::E:
        return {kFacE.data(), kFacE.size(), kFrequencyE.data(),
                kFrequencyE.size(), kTimeE.data(), kTimeE.size(),
                kWE.data(), kZE.data(), 4U, 5U, 1, 4, 1, 1, 4, 10,
                wide ? std::array<int, 4U> {{1,31,0,0}}
                     : std::array<int, 4U> {{1,29,0,0}}};
    }
    return {};
}

int positiveModulo(int value, int modulus) noexcept
{
    const int remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

phy::HamDrmComplex polarPilot(double amplitude, int phase)
{
    const double angle = 2.0 * std::acos(-1.0)
        * static_cast<double>(positiveModulo(phase, 1024)) / 1024.0;
    return {amplitude * std::cos(angle), amplitude * std::sin(angle)};
}

bool facAt(const TableView& table, int symbol, int carrier) noexcept
{
    for (std::size_t index = 0U; index < table.facSize; ++index) {
        if (table.fac[index].symbol == symbol
                && table.fac[index].carrier == carrier) {
            return true;
        }
    }
    return false;
}

const PilotEntry* pilotAt(const PilotEntry* entries,
                          std::size_t count,
                          int carrier) noexcept
{
    for (std::size_t index = 0U; index < count; ++index) {
        if (entries[index].carrier == carrier) {
            return entries + index;
        }
    }
    return nullptr;
}

bool isPilot(HamDrmCellKind kind) noexcept
{
    return hamDrmCellHas(kind, HamDrmCellKind::ScatteredPilot)
        || hamDrmCellHas(kind, HamDrmCellKind::TimePilot)
        || hamDrmCellHas(kind, HamDrmCellKind::FrequencyPilot);
}

bool finiteCells(const std::vector<phy::HamDrmComplex>& cells) noexcept
{
    return std::all_of(cells.begin(), cells.end(),
                       [](const phy::HamDrmComplex& cell) {
                           return std::isfinite(cell.real())
                               && std::isfinite(cell.imag());
                       });
}

HamDrmStatus invalid(const char* detail)
{
    return HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument, detail);
}

bool planShapeValid(const HamDrmCellPlan& plan) noexcept
{
    const std::size_t symbols = plan.symbolsPerSuperframe();
    const std::size_t carriers = plan.parameters.carrierCount();
    return plan.parameters.isValid() && symbols != 0U && carriers != 0U
        && plan.cells.size() == symbols * carriers
        && plan.usefulMscCellsPerSymbol.size() == symbols
        && plan.facCellsPerSymbol.size() == symbols;
}

} // namespace

HamDrmCellKind operator|(HamDrmCellKind left, HamDrmCellKind right) noexcept
{
    return static_cast<HamDrmCellKind>(static_cast<std::uint16_t>(left)
                                      | static_cast<std::uint16_t>(right));
}

HamDrmCellKind& operator|=(HamDrmCellKind& left,
                           HamDrmCellKind right) noexcept
{
    left = left | right;
    return left;
}

bool hamDrmCellHas(HamDrmCellKind value, HamDrmCellKind flag) noexcept
{
    return (static_cast<std::uint16_t>(value)
            & static_cast<std::uint16_t>(flag)) != 0U;
}

std::size_t HamDrmCellPlan::symbolsPerSuperframe() const noexcept
{
    return parameters.symbolsPerFrame * kFramesPerSuperframe;
}

const HamDrmCellDescriptor* HamDrmCellPlan::symbolCells(
    std::size_t absoluteSymbol) const noexcept
{
    const std::size_t carriers = parameters.carrierCount();
    if (absoluteSymbol >= symbolsPerSuperframe() || carriers == 0U
            || cells.size() < (absoluteSymbol + 1U) * carriers) {
        return nullptr;
    }
    return cells.data() + absoluteSymbol * carriers;
}

HamDrmValueResult<HamDrmCellPlan> hamDrmBuildCellPlan(
    const phy::HamDrmOfdmParameters& parameters)
{
    if (!parameters.isValid()) {
        return {std::nullopt, invalid("invalid HAMDRM OFDM parameters for cell plan")};
    }
    const TableView table = tableFor(parameters.robustness,
                                     parameters.occupiedBandwidth);
    if (table.fac == nullptr || table.w == nullptr || table.z == nullptr) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::UnsupportedProfile,
                                      "unsupported HAMDRM cell-plan profile")};
    }

    HamDrmCellPlan plan;
    plan.parameters = parameters;
    const std::size_t carriers = parameters.carrierCount();
    const std::size_t symbols = plan.symbolsPerSuperframe();
    plan.cells.resize(symbols * carriers);
    plan.usefulMscCellsPerSymbol.assign(symbols, 0U);
    plan.facCellsPerSymbol.assign(symbols, 0U);

    for (std::size_t absoluteSymbol = 0U; absoluteSymbol < symbols;
         ++absoluteSymbol) {
        const int frameSymbol = static_cast<int>(
            absoluteSymbol % parameters.symbolsPerFrame);
        for (std::size_t carrierIndex = 0U; carrierIndex < carriers;
             ++carrierIndex) {
            const int carrier = parameters.minimumCarrier
                + static_cast<int>(carrierIndex);
            auto& descriptor = plan.cells[absoluteSymbol * carriers
                                          + carrierIndex];
            descriptor.carrier = carrier;
            descriptor.kind = HamDrmCellKind::Msc;

            if (facAt(table, frameSymbol, carrier)) {
                descriptor.kind = HamDrmCellKind::Fac;
            }

            const int scatterBase = (table.frequencyInterval + 1) / 2
                + table.frequencyInterval
                    * positiveModulo(frameSymbol, table.timeInterval);
            const int scatterStride = table.frequencyInterval
                * table.timeInterval;
            const int delta = carrier - scatterBase;
            if ((delta % scatterStride) == 0) {
                const int n = positiveModulo(frameSymbol, table.y);
                const int m = frameSymbol / table.y;
                const int numerator = carrier - table.k0 - n * table.x;
                const int denominator = table.x * table.y;
                if ((numerator % denominator) != 0
                        || static_cast<std::size_t>(n) >= table.wzRows
                        || static_cast<std::size_t>(m) >= table.wzColumns) {
                    return {std::nullopt,
                            HamDrmStatus::failure(
                                HamDrmErrorCode::Malformed,
                                "HAMDRM scattered-pilot table is inconsistent")};
                }
                const int p = numerator / denominator;
                const std::size_t wzIndex = static_cast<std::size_t>(n)
                    * table.wzColumns + static_cast<std::size_t>(m);
                const int phase = positiveModulo(
                    4 * table.z[wzIndex] + p * table.w[wzIndex]
                    + p * p * (1 + frameSymbol) * table.q, 1024);
                descriptor.kind = HamDrmCellKind::ScatteredPilot;
                const bool boosted = std::find(table.boosted.begin(),
                                               table.boosted.end(), carrier)
                    != table.boosted.end();
                if (boosted) {
                    descriptor.kind |= HamDrmCellKind::BoostedPilot;
                }
                descriptor.pilotValue = polarPilot(boosted ? 2.0
                                                           : std::sqrt(2.0),
                                                   phase);
            }

            if (frameSymbol == 0) {
                if (const auto* time = pilotAt(table.time, table.timeSize,
                                               carrier)) {
                    if (hamDrmCellHas(descriptor.kind,
                                      HamDrmCellKind::ScatteredPilot)) {
                        descriptor.kind |= HamDrmCellKind::TimePilot;
                    } else {
                        descriptor.kind = HamDrmCellKind::TimePilot;
                    }
                    descriptor.pilotValue = polarPilot(std::sqrt(2.0),
                                                       time->phase);
                }
            }

            if (const auto* frequency = pilotAt(table.frequency,
                                                table.frequencySize,
                                                carrier)) {
                const bool overlaps = isPilot(descriptor.kind);
                descriptor.kind = overlaps
                    ? descriptor.kind | HamDrmCellKind::FrequencyPilot
                    : HamDrmCellKind::FrequencyPilot;
                int phase = frequency->phase;
                const std::size_t frequencyIndex = static_cast<std::size_t>(
                    frequency - table.frequency);
                if (parameters.robustness == HamDrmRobustness::E
                        && frequencyIndex < 2U && (frameSymbol & 1) != 0) {
                    phase += 512;
                }
                descriptor.pilotValue = polarPilot(std::sqrt(2.0), phase);
            }

            if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::Msc)) {
                ++plan.usefulMscCellsPerSymbol[absoluteSymbol];
            }
            if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::Fac)) {
                ++plan.facCellsPerSymbol[absoluteSymbol];
            }
        }
    }

    std::size_t totalMsc = 0U;
    for (const std::size_t count : plan.usefulMscCellsPerSymbol) {
        totalMsc += count;
    }
    plan.usefulMscCellsPerFrame = totalMsc / kFramesPerSuperframe;
    plan.dummyMscCellsPerSuperframe = totalMsc
        - kFramesPerSuperframe * plan.usefulMscCellsPerFrame;
    const std::size_t finalSymbol = symbols - 1U;
    if (plan.usefulMscCellsPerSymbol[finalSymbol]
            < plan.dummyMscCellsPerSuperframe) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "HAMDRM dummy-cell count is inconsistent")};
    }
    plan.usefulMscCellsPerSymbol[finalSymbol]
        -= plan.dummyMscCellsPerSuperframe;
    std::size_t usefulSeen = 0U;
    auto* finalCells = plan.cells.data() + finalSymbol * carriers;
    for (std::size_t index = 0U; index < carriers; ++index) {
        if (hamDrmCellHas(finalCells[index].kind, HamDrmCellKind::Msc)) {
            if (usefulSeen >= plan.usefulMscCellsPerSymbol[finalSymbol]) {
                finalCells[index].kind |= HamDrmCellKind::DummyMsc;
            } else {
                ++usefulSeen;
            }
        }
    }

    for (std::size_t frame = 0U; frame < kFramesPerSuperframe; ++frame) {
        std::size_t facCount = 0U;
        for (std::size_t symbol = 0U; symbol < parameters.symbolsPerFrame;
             ++symbol) {
            facCount += plan.facCellsPerSymbol[
                frame * parameters.symbolsPerFrame + symbol];
        }
        if (facCount != 45U) {
            return {std::nullopt,
                    HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                          "HAMDRM FAC cell plan does not contain 45 cells")};
        }
    }
    return {std::move(plan), HamDrmStatus::success()};
}

HamDrmValueResult<std::vector<phy::HamDrmComplex>> hamDrmMapCellSymbol(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& mscCells,
    const std::vector<phy::HamDrmComplex>& facCells,
    HamDrmConstellation mscConstellation)
{
    if (!planShapeValid(plan) || absoluteSymbol >= plan.symbolsPerSuperframe()
            || mscCells.size() != plan.usefulMscCellsPerSymbol[absoluteSymbol]
            || facCells.size() != plan.facCellsPerSymbol[absoluteSymbol]
            || !finiteCells(mscCells) || !finiteCells(facCells)) {
        return {std::nullopt, invalid("invalid HAMDRM cell-symbol mapping input")};
    }
    double dummyAmplitude = 0.0;
    switch (mscConstellation) {
    case HamDrmConstellation::Qam4:
    case HamDrmConstellation::Qam16:
        dummyAmplitude = 1.0 / std::sqrt(10.0);
        break;
    case HamDrmConstellation::Qam64:
        dummyAmplitude = 1.0 / std::sqrt(42.0);
        break;
    default:
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::UnsupportedProfile,
                                      "unsupported HAMDRM MSC constellation")};
    }

    const std::size_t carriers = plan.parameters.carrierCount();
    const auto* descriptors = plan.symbolCells(absoluteSymbol);
    std::vector<phy::HamDrmComplex> output(carriers);
    std::size_t mscIndex = 0U;
    std::size_t facIndex = 0U;
    std::size_t dummyIndex = 0U;
    for (std::size_t index = 0U; index < carriers; ++index) {
        const auto& descriptor = descriptors[index];
        if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::Msc)) {
            if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::DummyMsc)) {
                output[index] = {dummyAmplitude,
                                 (dummyIndex & 1U) == 0U ? dummyAmplitude
                                                        : -dummyAmplitude};
                ++dummyIndex;
            } else {
                output[index] = mscCells[mscIndex++];
            }
        }
        if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::Fac)) {
            output[index] = facCells[facIndex++];
        }
        if (isPilot(descriptor.kind)) {
            output[index] = descriptor.pilotValue;
        }
    }
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<HamDrmExtractedCellSymbol> hamDrmExtractCellSymbol(
    const HamDrmCellPlan& plan,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers,
    double corruptedPilotSquaredErrorThreshold)
{
    if (!planShapeValid(plan) || absoluteSymbol >= plan.symbolsPerSuperframe()
            || carriers.size() != plan.parameters.carrierCount()
            || !finiteCells(carriers)
            || !std::isfinite(corruptedPilotSquaredErrorThreshold)
            || corruptedPilotSquaredErrorThreshold < 0.0) {
        return {std::nullopt, invalid("invalid HAMDRM cell-symbol extraction input")};
    }
    HamDrmExtractedCellSymbol extracted;
    extracted.mscCells.reserve(plan.usefulMscCellsPerSymbol[absoluteSymbol]);
    extracted.facCells.reserve(plan.facCellsPerSymbol[absoluteSymbol]);
    const auto* descriptors = plan.symbolCells(absoluteSymbol);
    for (std::size_t index = 0U; index < carriers.size(); ++index) {
        const auto& descriptor = descriptors[index];
        if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::Msc)
                && !hamDrmCellHas(descriptor.kind,
                                  HamDrmCellKind::DummyMsc)) {
            extracted.mscCells.push_back(carriers[index]);
        }
        if (hamDrmCellHas(descriptor.kind, HamDrmCellKind::Fac)) {
            extracted.facCells.push_back(carriers[index]);
        }
        if (isPilot(descriptor.kind)) {
            const double squaredError = std::norm(carriers[index]
                                                   - descriptor.pilotValue);
            extracted.pilotSquaredError += squaredError;
            ++extracted.pilotCells;
            if (squaredError > corruptedPilotSquaredErrorThreshold) {
                ++extracted.corruptedPilotCells;
            }
        }
    }
    return {std::move(extracted), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm::channel
