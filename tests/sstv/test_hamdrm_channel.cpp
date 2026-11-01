// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/digital/channel/HamDrmCellPlan.h"
#include "../../src/sstv/digital/channel/HamDrmChannelCoding.h"
#include "../../src/sstv/digital/channel/HamDrmChannelCrc.h"
#include "../../src/sstv/digital/channel/HamDrmFacCodec.h"
#include "../../src/sstv/digital/channel/HamDrmInterleaver.h"
#include "../../src/sstv/digital/channel/HamDrmMlcCodec.h"
#include "../../src/sstv/digital/phy/HamDrmOfdmParameters.h"
#include "../../src/sstv/digital/phy/HamDrmQam.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::channel;
using namespace decodium::sstv::hamdrm::phy;

namespace {

class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw TestFailure(message);
    }
}

void requireNear(HamDrmComplex actual,
                 HamDrmComplex expected,
                 double tolerance,
                 const std::string& message)
{
    if (!std::isfinite(actual.real()) || !std::isfinite(actual.imag())
            || std::abs(actual - expected) > tolerance) {
        throw TestFailure(message + ": error="
                          + std::to_string(std::abs(actual - expected)));
    }
}

template<typename Exception, typename Callable>
void requireThrows(Callable&& callable, const std::string& message)
{
    bool caught = false;
    try {
        std::forward<Callable>(callable)();
    } catch (const Exception&) {
        caught = true;
    }
    require(caught, message);
}

std::vector<std::uint8_t> patternBits(std::size_t count, std::uint32_t seed)
{
    std::vector<std::uint8_t> bits(count, 0U);
    std::uint32_t state = seed;
    for (auto& bit : bits) {
        state = state * 1'664'525U + 1'013'904'223U;
        bit = static_cast<std::uint8_t>((state >> 31U) & 1U);
    }
    return bits;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    for (const auto byte : bytes) {
        hash ^= byte;
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

const HamDrmCellDescriptor& descriptorAt(const HamDrmCellPlan& plan,
                                         std::size_t absoluteSymbol,
                                         int carrier)
{
    require(carrier >= plan.parameters.minimumCarrier
                && carrier <= plan.parameters.maximumCarrier,
            "cell-plan carrier lookup is out of range");
    const auto* row = plan.symbolCells(absoluteSymbol);
    require(row != nullptr, "cell-plan symbol lookup failed");
    return row[static_cast<std::size_t>(carrier
                                        - plan.parameters.minimumCarrier)];
}

void testCrcAndEnergyVectors()
{
    const std::string check = "123456789";
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(check.data());
    require(hamDrmChannelCrc8(bytes, check.size()) == 0x4BU,
            "CRC-8/DRM check vector mismatch");
    require(hamDrmChannelCrc16(bytes, check.size()) == 0xD64EU,
            "CRC-16/DRM check vector mismatch");

    const std::vector<std::uint8_t> payload {0x00U, 0x55U, 0xAAU, 0xFFU};
    const auto packet = hamDrmAppendChannelCrc16(payload);
    const auto verified = hamDrmVerifyAndStripChannelCrc16(packet);
    require(verified.ok() && *verified.value == payload,
            "channel CRC16 packet round trip failed");
    auto corrupted = packet;
    corrupted[1U] ^= 0x20U;
    require(hamDrmVerifyAndStripChannelCrc16(corrupted).status.code
                == HamDrmErrorCode::CrcMismatch,
            "channel CRC16 corruption was not detected");
    require(hamDrmVerifyAndStripChannelCrc16({0x00U}).status.code
                == HamDrmErrorCode::Truncated,
            "truncated channel CRC16 was accepted");

    const std::vector<std::uint8_t> zeros(64U, 0U);
    const auto dispersed = hamDrmEnergyDisperse(zeros);
    require(hamDrmEnergyDisperse(dispersed) == zeros,
            "energy dispersal is not self-inverse");
    require(fnv1a(dispersed) == 8'336'925'848'910'897'074ULL,
            "energy-dispersal deterministic vector changed: "
            + std::to_string(fnv1a(dispersed)));
    requireThrows<std::invalid_argument>(
        []() { static_cast<void>(hamDrmEnergyDisperse({0U, 2U})); },
        "non-binary energy-dispersal input was accepted");
}

void testPuncturingAndViterbi()
{
    const auto facSchedule = hamDrmPunctureSchedule(
        48U, 6U, 90U, HamDrmPunctureTailMode::Fac);
    require(facSchedule.size() == 54U,
            "FAC puncturing schedule step count mismatch");
    require(facSchedule[0U] == HamDrmPunctureMask::G0G1
                && facSchedule[1U] == HamDrmPunctureMask::G0
                && facSchedule[2U] == HamDrmPunctureMask::G0G1
                && facSchedule[51U] == HamDrmPunctureMask::G0G1
                && facSchedule[52U] == HamDrmPunctureMask::G0
                && facSchedule[53U] == HamDrmPunctureMask::G0G1,
            "FAC puncturing schedule vector mismatch");

    const auto mscSchedule = hamDrmPunctureSchedule(
        27U, 6U, 59U, HamDrmPunctureTailMode::Msc);
    require(mscSchedule.size() == 33U,
            "MSC puncturing schedule step count mismatch");
    require(mscSchedule[27U] == HamDrmPunctureMask::G0G1G2
                && mscSchedule[28U] == HamDrmPunctureMask::G0G1
                && mscSchedule[29U] == HamDrmPunctureMask::G0G1
                && mscSchedule[30U] == HamDrmPunctureMask::G0G1G2
                && mscSchedule[31U] == HamDrmPunctureMask::G0G1
                && mscSchedule[32U] == HamDrmPunctureMask::G0G1,
            "MSC tail puncturing vector mismatch");

    const auto input = patternBits(48U, 0xC0DEC0DEU);
    const auto encoded = hamDrmConvolutionEncode(
        input, 6U, 90U, HamDrmPunctureTailMode::Fac);
    require(encoded.ok(), "FAC convolutional vector did not encode");
    // Executed QSSTV CConvEncoder at 8c27d6d with the same 48-bit input,
    // FAC puncturing pattern 6 and 45 output cells.
    const std::string qsstvBits =
        "111011001010111011100101010101001100101011111010100101000100100111110100011010001110000111";
    require(qsstvBits.size() == encoded.value->size(), "QSSTV FAC vector size mismatch");
    std::vector<std::uint8_t> reference;
    for (const char bit : qsstvBits) reference.push_back(static_cast<std::uint8_t>(bit - '0'));
    require(*encoded.value == reference, "FAC convolutional output differs from QSSTV");
    const auto referenceDecoded = hamDrmViterbiDecode(reference, input.size(), 6U,
                                                       HamDrmPunctureTailMode::Fac);
    require(referenceDecoded.ok() && referenceDecoded.value->bits == input,
            "QSSTV FAC convolutional vector did not decode");
    require(fnv1a(*encoded.value) == 12'595'605'962'865'036'275ULL,
            "convolutional deterministic vector changed: "
            + std::to_string(fnv1a(*encoded.value)));
    const auto decoded = hamDrmViterbiDecode(
        *encoded.value, input.size(), 6U, HamDrmPunctureTailMode::Fac);
    require(decoded.ok() && decoded.value->bits == input
                && decoded.value->pathMetric == 0U,
            "hard Viterbi exact round trip failed");
    auto damaged = *encoded.value;
    damaged[17U] ^= 1U;
    const auto corrected = hamDrmViterbiDecode(
        damaged, input.size(), 6U, HamDrmPunctureTailMode::Fac);
    require(corrected.ok() && corrected.value->bits == input
                && corrected.value->pathMetric == 1U,
            "Viterbi failed to correct a deterministic bit error");
}

void testBlockAndSymbolInterleavers()
{
    const auto permutation = hamDrmBlockInterleaverPermutation(90U, 21U);
    const std::array<std::size_t, 8U> expected {{
        0U, 31U, 42U, 17U, 4U, 14U, 69U, 72U
    }};
    require(std::equal(expected.begin(), expected.end(), permutation.begin()),
            "block-interleaver prefix vector mismatch");
    auto sorted = permutation;
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t index = 0U; index < sorted.size(); ++index) {
        require(sorted[index] == index,
                "block interleaver is not a permutation");
    }
    const auto bits = patternBits(254U, 0x11223344U);
    const std::string qsstvInterleaved =
        "10110001011100100110010110000100001111100000000101010110110100010010101110011110010101100011011001110000010010111110101101000110111100101000110101100000100010000011111001001100001111011000001100000110010001100010101101101111110110100100110110100100110000";
    std::vector<std::uint8_t> independentBits;
    for (const char bit : qsstvInterleaved) independentBits.push_back(static_cast<std::uint8_t>(bit - '0'));
    require(hamDrmBitInterleave(bits, 13U) == independentBits,
            "bit interleaver differs from QSSTV reference");
    require(hamDrmBitDeinterleave(independentBits, 13U) == bits,
            "QSSTV bit-interleaver reference did not decode");
    require(hamDrmBitDeinterleave(hamDrmBitInterleave(bits, 13U), 13U)
                == bits,
            "bit-interleaver round trip failed");
    requireThrows<std::invalid_argument>(
        []() { static_cast<void>(hamDrmBlockInterleaverPermutation(90U, 4U)); },
        "even interleaver multiplier was accepted");
    requireThrows<std::invalid_argument>(
        []() { static_cast<void>(hamDrmBlockInterleaverPermutation(90U, 3U)); },
        "unsupported interleaver multiplier was accepted");

    constexpr std::size_t frameSize = 37U;
    for (const std::size_t depth : {1U, 5U}) {
        HamDrmSymbolInterleaverEncoder encoder(frameSize, depth);
        HamDrmSymbolInterleaverDecoder decoder(frameSize, depth);
        std::vector<std::vector<HamDrmComplex>> frames;
        for (std::size_t frame = 0U; frame < 12U; ++frame) {
            std::vector<HamDrmComplex> values(frameSize);
            for (std::size_t cell = 0U; cell < frameSize; ++cell) {
                values[cell] = {static_cast<double>(frame * 100U + cell),
                                -static_cast<double>(frame + cell)};
            }
            frames.push_back(values);
            const auto transmitted = encoder.push(values);
            const auto recovered = decoder.push(transmitted);
            if (frame + 1U < depth) {
                require(!recovered.has_value(),
                        "long symbol deinterleaver returned startup garbage");
            } else {
                require(recovered.has_value()
                            && *recovered == frames[frame + 1U - depth],
                        "symbol-interleaver round trip failed");
            }
        }
    }
}

void testQsstvReceiveInterleaverMap()
{
    std::ifstream fixture(DECODIUM_QSSTV_RX_MAP_FIXTURE);
    require(fixture.good(), "QSSTV receive mapping fixture missing");
    const std::array<HamDrmRobustness,3> modes {{HamDrmRobustness::A,HamDrmRobustness::B,HamDrmRobustness::E}};
    const std::array<HamDrmOccupiedBandwidth,2> widths {{HamDrmOccupiedBandwidth::Hz2300,HamDrmOccupiedBandwidth::Hz2500}};
    for(int m=0;m<3;++m) for(int b=0;b<2;++b) for(int d=0;d<2;++d) {
        int fm,fb,fd,count,modulo;
        require(bool(fixture >> fm >> fb >> fd >> count >> modulo),"QSSTV receive map header truncated");
        require(fm==m && fb==b && fd==d && modulo==128,"QSSTV receive map profile mismatch");
        std::vector<std::vector<int>> map(6,std::vector<int>(count));
        for(auto& row:map) for(auto& index:row) require(bool(fixture>>index),"QSSTV receive map truncated");
        const auto params=hamDrmOfdmParameters(modes[m],widths[b]);
        const auto plan=hamDrmBuildCellPlan(*params);
        require(plan.ok() && int(plan.value->usefulMscCellsPerFrame)==count,"QSSTV receive map count differs");
        const int depth=d==0 ? 5 : 1;
        const int symbols=int(params->symbolsPerFrame), frameStride=symbols*modulo;
        for (int variant=0;variant<2;++variant) {
        std::vector<HamDrmComplex> ring(6*frameStride);
        HamDrmSymbolInterleaverEncoder interleaver(count,depth);
        std::array<bool,6> candidate {{true,true,true,true,true,true}};
        for(int frame=0;frame<15;++frame) {
            std::vector<HamDrmComplex> source;
            for(int i=0;i<count;++i)
                source.emplace_back(frame < 9 ? double(frame+1) : 0.0,
                                    frame < 9 ? (variant==0 ? double(i+1) : -double(i+1)) : 0.0);
            const auto transmitted=interleaver.push(source);
            std::size_t offset=0;
            for(int symbol=0;symbol<symbols;++symbol) {
                const auto absolute=(frame%3)*symbols+symbol;
                const auto* cells=plan.value->symbolCells(absolute);
                for(std::size_t c=0;c<params->carrierCount();++c) {
                    if(hamDrmCellHas(cells[c].kind,HamDrmCellKind::Msc)) {
                        require(offset<transmitted.size(),"too many mapped MSC cells");
                        ring[(frame%6)*frameStride+symbol*modulo+params->minimumCarrier+int(c)]=transmitted[offset++];
                    }
                }
            }
            require(offset==transmitted.size(),"MSC frame mapping count differs");
            if(frame<depth+2) continue;
            for(int shift=0;shift<6;++shift) if(candidate[shift]) {
                const auto& row=map[(frame+shift)%6];
                for(int i=0;i<count;++i) {
                    const int sourceFrame=frame-depth+1;
                    const HamDrmComplex expected = sourceFrame < 9
                        ? HamDrmComplex(double(sourceFrame+1), variant==0 ? double(i+1) : -double(i+1))
                        : HamDrmComplex {};
                    if(row[i]<0 || row[i]>=int(ring.size()) || ring[row[i]] != expected) {
                        candidate[shift]=false; break;
                    }
                }
            }
        }
        // QSSTV's map row is one step ahead of the circular write slot.
        require(candidate[1] && std::count(candidate.begin(),candidate.end(),true)==1,
                "QSSTV receive map failed native interleaver profile " + std::to_string(m) + "/" + std::to_string(b) + "/" + std::to_string(d));
        }
    }
}

void testQsstvMappedSymbols()
{
    std::ifstream fixture(DECODIUM_QSSTV_MAPPER_FIXTURE);
    require(fixture.good(), "QSSTV mapper fixture missing");
    const std::array<HamDrmRobustness, 3> modes {{HamDrmRobustness::A, HamDrmRobustness::B, HamDrmRobustness::E}};
    const std::array<HamDrmOccupiedBandwidth, 2> widths {{HamDrmOccupiedBandwidth::Hz2300, HamDrmOccupiedBandwidth::Hz2500}};
    for (int m=0;m<3;++m) for (int b=0;b<2;++b) for (int v=0;v<2;++v) {
        int fm, fb, fv, symbols, carriers;
        require(bool(fixture >> fm >> fb >> fv >> symbols >> carriers), "mapper profile truncated");
        const auto params = hamDrmOfdmParameters(modes[m], widths[b]);
        require(params && fm == m && fb == b && fv == v && carriers == int(params->carrierCount()), "mapper profile differs");
        const auto built = hamDrmBuildCellPlan(*params);
        require(built.ok() && symbols == int(built.value->symbolsPerSuperframe()), "mapper plan differs");
        const auto& plan = *built.value;
        for (int symbol=0;symbol<symbols;++symbol) {
            int mscCount, facCount;
            require(bool(fixture >> mscCount >> facCount), "mapper symbol truncated");
            require(mscCount == int(plan.usefulMscCellsPerSymbol[symbol]) && facCount == int(plan.facCellsPerSymbol[symbol]), "mapper input counts differ");
            std::vector<HamDrmComplex> msc, fac;
            for (int i=0;i<mscCount;++i) msc.emplace_back(double(i+1)/100.0, double(symbol+v+1)/-100.0);
            for (int i=0;i<facCount;++i) fac.emplace_back(double(i+v+1)/-100.0, double(symbol+1)/100.0);
            const auto mapped = hamDrmMapCellSymbol(plan, symbol, msc, fac, HamDrmConstellation::Qam64);
            require(mapped.ok(), "native cell mapping failed");
            std::vector<HamDrmComplex> reference;
            for (int i=0;i<carriers;++i) {
                double real, imag; require(bool(fixture >> real >> imag), "mapper cells truncated");
                reference.emplace_back(real, imag);
                require(std::abs((*mapped.value)[i] - reference.back()) < 1e-6, "QSSTV mapped cell differs");
            }
            const auto extracted = hamDrmExtractCellSymbol(plan, symbol, reference, 1e-9);
            require(extracted.ok() && extracted.value->mscCells == msc && extracted.value->facCells == fac
                && extracted.value->corruptedPilotCells == 0, "QSSTV mapped cell extraction failed");
        }
    }
    std::string extra; require(!(fixture >> extra), "extra mapper fixture data");
}

void testQsstvCellMaps()
{
    std::ifstream fixture(DECODIUM_QSSTV_CELLMAP_FIXTURE);
    require(fixture.good(), "QSSTV cell map fixture not available");
    std::ifstream pilots(DECODIUM_QSSTV_PILOTS_FIXTURE);
    require(pilots.good(), "QSSTV pilot fixture not available");
    const std::array<HamDrmRobustness, 3> modes {{HamDrmRobustness::A, HamDrmRobustness::B, HamDrmRobustness::E}};
    const std::array<HamDrmOccupiedBandwidth, 2> widths {{HamDrmOccupiedBandwidth::Hz2300, HamDrmOccupiedBandwidth::Hz2500}};
    for (int m = 0; m < 3; ++m) for (int b = 0; b < 2; ++b) {
        int fm, fb, symbols, kmin, kmax, useful, carriers;
        require(bool(fixture >> fm >> fb >> symbols >> kmin >> kmax >> useful >> carriers), "invalid QSSTV cell map header");
        require(fm == m && fb == b, "unexpected QSSTV cell map profile");
        int pm, pb, ps, pmin, pmax, pu, pc;
        require(bool(pilots >> pm >> pb >> ps >> pmin >> pmax >> pu >> pc)
                    && pm == fm && pb == fb && ps == symbols && pmin == kmin
                    && pmax == kmax && pu == useful && pc == carriers,
                "invalid QSSTV pilot profile");
        const auto parameters = hamDrmOfdmParameters(modes[m], widths[b]);
        require(parameters.has_value(), "missing cell map profile");
        const auto built = hamDrmBuildCellPlan(*parameters);
        require(built.ok(), "cell map build failed");
        const auto& plan = *built.value;
        require(int(plan.symbolsPerSuperframe()) == symbols && parameters->minimumCarrier == kmin && parameters->maximumCarrier == kmax && int(parameters->carrierCount()) == carriers && int(plan.usefulMscCellsPerFrame) == useful, "QSSTV cell map dimensions differ");
        for (int symbol = 0; symbol < symbols; ++symbol) {
            std::string row;
            require(bool(fixture >> row) && row.size() == std::size_t(carriers * 2), "invalid QSSTV cell map row");
            for (int c = 0; c < carriers; ++c) {
                const auto ref = std::stoul(row.substr(c * 2, 2), nullptr, 16);
                const auto kind = plan.symbolCells(symbol)[c].kind;
                unsigned actual = 0;
                if (hamDrmCellHas(kind, HamDrmCellKind::Msc)) actual |= 2;
                if (hamDrmCellHas(kind, HamDrmCellKind::Fac)) actual |= 8;
                if (hamDrmCellHas(kind, HamDrmCellKind::TimePilot)) actual |= 16;
                if (hamDrmCellHas(kind, HamDrmCellKind::FrequencyPilot)) actual |= 32;
                if (hamDrmCellHas(kind, HamDrmCellKind::ScatteredPilot)) actual |= 64;
                if (hamDrmCellHas(kind, HamDrmCellKind::BoostedPilot)) actual |= 128;
                require(actual == ref, "QSSTV cell kind mismatch profile " + std::to_string(m) + "/" + std::to_string(b) + " symbol " + std::to_string(symbol) + " carrier " + std::to_string(kmin+c));
                if ((ref & (16U | 32U | 64U)) != 0U) {
                    double real, imag;
                    require(bool(pilots >> real >> imag), "truncated QSSTV pilot fixture");
                    const auto value = plan.symbolCells(symbol)[c].pilotValue;
                    require(std::abs(value - HamDrmComplex(real, imag)) < 1e-6,
                            "QSSTV pilot mismatch profile " + std::to_string(m) + "/" + std::to_string(b) + " symbol " + std::to_string(symbol) + " carrier " + std::to_string(kmin+c));
                }
            }
        }
    }
    std::string extra;
    require(!(fixture >> extra), "extra QSSTV cell map data");
    require(!(pilots >> extra), "extra QSSTV pilot data");
}

void testCellPlansAndPilots()
{
    const std::array<HamDrmRobustness, 3U> robustness {{
        HamDrmRobustness::A, HamDrmRobustness::B, HamDrmRobustness::E
    }};
    const std::array<HamDrmOccupiedBandwidth, 2U> bandwidths {{
        HamDrmOccupiedBandwidth::Hz2300,
        HamDrmOccupiedBandwidth::Hz2500
    }};
    for (const auto mode : robustness) {
        for (const auto bandwidth : bandwidths) {
            const auto parameters = hamDrmOfdmParameters(mode, bandwidth);
            require(parameters.has_value(), "missing OFDM parameters");
            const auto built = hamDrmBuildCellPlan(*parameters);
            require(built.ok(), "cell plan did not build: "
                                  + built.status.detail);
            const auto& plan = *built.value;
            std::size_t expectedUsefulCells = 0U;
            switch (mode) {
            case HamDrmRobustness::A:
                expectedUsefulCells = bandwidth
                        == HamDrmOccupiedBandwidth::Hz2300 ? 647U : 704U;
                break;
            case HamDrmRobustness::B:
                expectedUsefulCells = bandwidth
                        == HamDrmOccupiedBandwidth::Hz2300 ? 455U : 530U;
                break;
            case HamDrmRobustness::E:
                expectedUsefulCells = bandwidth
                        == HamDrmOccupiedBandwidth::Hz2300 ? 339U : 369U;
                break;
            }
            require(plan.usefulMscCellsPerFrame == expectedUsefulCells
                        && plan.dummyMscCellsPerSuperframe == 0U,
                    "cell-plan useful MSC count fixture mismatch");
            require(plan.cells.size() == plan.symbolsPerSuperframe()
                                           * parameters->carrierCount(),
                    "cell-plan dimensions mismatch");
            for (std::size_t frame = 0U; frame < 3U; ++frame) {
                std::size_t fac = 0U;
                for (std::size_t symbol = 0U;
                     symbol < parameters->symbolsPerFrame; ++symbol) {
                    fac += plan.facCellsPerSymbol[
                        frame * parameters->symbolsPerFrame + symbol];
                }
                require(fac == 45U, "FAC cell count per frame mismatch");
            }
            std::size_t useful = 0U;
            for (const auto count : plan.usefulMscCellsPerSymbol) {
                useful += count;
            }
            require(useful == 3U * plan.usefulMscCellsPerFrame,
                    "useful MSC cells do not fill three logical frames");

            for (std::size_t symbol = 0U;
                 symbol < plan.symbolsPerSuperframe(); ++symbol) {
                std::vector<HamDrmComplex> msc(
                    plan.usefulMscCellsPerSymbol[symbol]);
                std::vector<HamDrmComplex> fac(plan.facCellsPerSymbol[symbol]);
                for (std::size_t index = 0U; index < msc.size(); ++index) {
                    msc[index] = {static_cast<double>(index + 1U) / 100.0,
                                  -static_cast<double>(symbol + 1U) / 100.0};
                }
                for (std::size_t index = 0U; index < fac.size(); ++index) {
                    fac[index] = {-static_cast<double>(index + 1U) / 100.0,
                                  static_cast<double>(symbol + 1U) / 100.0};
                }
                const auto mapped = hamDrmMapCellSymbol(
                    plan, symbol, msc, fac, HamDrmConstellation::Qam64);
                require(mapped.ok(), "cell symbol did not map");
                const auto extracted = hamDrmExtractCellSymbol(
                    plan, symbol, *mapped.value, 1.0e-20);
                require(extracted.ok()
                            && extracted.value->mscCells == msc
                            && extracted.value->facCells == fac
                            && extracted.value->corruptedPilotCells == 0U,
                        "cell-symbol extraction round trip failed");
            }
        }
    }

    const auto a = hamDrmBuildCellPlan(*hamDrmOfdmParameters(
        HamDrmRobustness::A, HamDrmOccupiedBandwidth::Hz2300));
    const auto b = hamDrmBuildCellPlan(*hamDrmOfdmParameters(
        HamDrmRobustness::B, HamDrmOccupiedBandwidth::Hz2300));
    const auto e = hamDrmBuildCellPlan(*hamDrmOfdmParameters(
        HamDrmRobustness::E, HamDrmOccupiedBandwidth::Hz2300));
    require(a.ok() && b.ok() && e.ok(), "landmark cell plans did not build");
    require(hamDrmCellHas(descriptorAt(*a.value, 0U, 6).kind,
                          HamDrmCellKind::TimePilot)
                && hamDrmCellHas(descriptorAt(*a.value, 0U, 9).kind,
                                 HamDrmCellKind::FrequencyPilot),
            "mode A pilot landmarks mismatch");
    require(hamDrmCellHas(descriptorAt(*b.value, 0U, 21).kind,
                          HamDrmCellKind::Fac),
            "mode B FAC landmark mismatch");
    const auto& eOverlap = descriptorAt(*e.value, 0U, 5);
    require(hamDrmCellHas(eOverlap.kind, HamDrmCellKind::ScatteredPilot)
                && hamDrmCellHas(eOverlap.kind,
                                 HamDrmCellKind::FrequencyPilot),
            "mode E pilot precedence landmark mismatch");
    const auto& eOddFrequency = descriptorAt(*e.value, 1U, 5);
    requireNear(eOddFrequency.pilotValue,
                -descriptorAt(*e.value, 0U, 5).pilotValue,
                1.0e-12, "mode E odd-symbol frequency-pilot flip mismatch");

    const auto& plan = *b.value;
    const std::size_t symbol = 0U;
    std::vector<HamDrmComplex> msc(plan.usefulMscCellsPerSymbol[symbol]);
    std::vector<HamDrmComplex> fac(plan.facCellsPerSymbol[symbol]);
    const auto mapped = hamDrmMapCellSymbol(
        plan, symbol, msc, fac, HamDrmConstellation::Qam16);
    require(mapped.ok(), "pilot-corruption fixture did not map");
    auto corrupted = *mapped.value;
    const std::size_t pilotIndex = static_cast<std::size_t>(
        8 - plan.parameters.minimumCarrier);
    corrupted[pilotIndex] += HamDrmComplex {0.5, -0.25};
    const auto extracted = hamDrmExtractCellSymbol(
        plan, symbol, corrupted, 1.0e-6);
    require(extracted.ok() && extracted.value->corruptedPilotCells == 1U,
            "corrupted pilot carrier was not detected");

    auto invalidParameters = *hamDrmOfdmParameters(
        HamDrmRobustness::A, HamDrmOccupiedBandwidth::Hz2300);
    invalidParameters.maximumCarrier = 999;
    require(hamDrmBuildCellPlan(invalidParameters).status.code
                == HamDrmErrorCode::InvalidArgument,
            "invalid OFDM profile was accepted by cell planner");
}

void testFacCodec()
{
    std::ifstream fixture(DECODIUM_QSSTV_FAC_FIXTURE);
    require(fixture.good(), "QSSTV FAC fixture not available");
    std::set<std::array<int, 6U>> profiles;
    int frame, bandwidth, depth, constellation, protection, packet;
    std::string payloadReference, signReference;
    while (fixture >> frame >> bandwidth >> depth >> constellation >> protection >> packet
           >> payloadReference >> signReference) {
        require(frame >= 0 && frame < 3 && bandwidth >= 0 && bandwidth < 2
                    && depth >= 0 && depth < 2 && constellation >= 0 && constellation < 3
                    && protection >= 0 && protection < 2 && packet >= 0 && packet < 4,
                "invalid QSSTV FAC profile fixture");
        require(profiles.insert({frame, bandwidth, depth, constellation, protection, packet}).second,
                "duplicate QSSTV FAC profile fixture");
        require(payloadReference.size() == 48U && signReference.size() == 90U,
                "invalid QSSTV FAC vector length");
        HamDrmFacParameters referenceParameters;
        referenceParameters.frameIdentity = static_cast<std::size_t>(frame);
        referenceParameters.occupiedBandwidth = bandwidth ? HamDrmOccupiedBandwidth::Hz2500
                                                          : HamDrmOccupiedBandwidth::Hz2300;
        referenceParameters.interleaver = depth ? HamDrmInterleaver::Short : HamDrmInterleaver::Long;
        referenceParameters.mscConstellation = constellation == 0 ? HamDrmConstellation::Qam4
            : (constellation == 1 ? HamDrmConstellation::Qam16 : HamDrmConstellation::Qam64);
        referenceParameters.protection = protection ? HamDrmProtection::Normal : HamDrmProtection::High;
        referenceParameters.packetId = static_cast<std::uint8_t>(packet);
        referenceParameters.callsign = "TEST123";
        const auto localPayload = hamDrmEncodeFacPayloadBits(referenceParameters);
        const auto localCells = hamDrmEncodeFacCells(referenceParameters);
        require(localPayload.ok() && localCells.ok(), "FAC fixture profile did not encode");
        for (std::size_t i = 0; i < payloadReference.size(); ++i)
            require((*localPayload.value)[i] == static_cast<std::uint8_t>(payloadReference[i] - '0'),
                    "FAC payload differs from QSSTV fixture");
        const double magnitude = 1.0 / std::sqrt(2.0);
        std::vector<HamDrmComplex> referenceCells;
        for (std::size_t i = 0; i < signReference.size(); i += 2U) {
            referenceCells.emplace_back(signReference[i] == '1' ? -magnitude : magnitude,
                                        signReference[i + 1U] == '1' ? -magnitude : magnitude);
            requireNear(localCells.value->at(i / 2U), referenceCells.back(), 1e-12,
                        "FAC symbol differs from QSSTV fixture");
        }
        const auto decodedReference = hamDrmDecodeFacCells(referenceCells);
        require(decodedReference.ok() && decodedReference.value->parameters.frameIdentity
                    == referenceParameters.frameIdentity
                    && decodedReference.value->parameters.occupiedBandwidth == referenceParameters.occupiedBandwidth
                    && decodedReference.value->parameters.interleaver == referenceParameters.interleaver
                    && decodedReference.value->parameters.mscConstellation == referenceParameters.mscConstellation
                    && decodedReference.value->parameters.protection == referenceParameters.protection
                    && decodedReference.value->parameters.packetId == referenceParameters.packetId,
                "QSSTV FAC profile did not decode");
    }
    require(fixture.eof() && profiles.size() == 288U, "QSSTV FAC fixture incomplete");

    // Executed pinned QSSTV FACParam plus energy dispersal, convolution,
    // bit interleaving and 4-QAM mapping, with identical FAC parameters.
    const std::string qsstvPayload = "110010101010101001000101101001100000000000101011";
    const std::string qsstvCellSigns =
        "100100110101110110101101000011000000110011110000100000000110110100110011110101101011111100";
    HamDrmFacParameters qsstvParameters;
    qsstvParameters.frameIdentity = 0U;
    qsstvParameters.occupiedBandwidth = HamDrmOccupiedBandwidth::Hz2300;
    qsstvParameters.interleaver = HamDrmInterleaver::Long;
    qsstvParameters.mscConstellation = HamDrmConstellation::Qam16;
    qsstvParameters.protection = HamDrmProtection::High;
    qsstvParameters.packetId = 1U;
    qsstvParameters.callsign = "TEST123";
    const auto qsstvNativePayload = hamDrmEncodeFacPayloadBits(qsstvParameters);
    const auto qsstvNativeCells = hamDrmEncodeFacCells(qsstvParameters);
    require(qsstvNativePayload.ok() && qsstvNativePayload.value->size() == qsstvPayload.size(),
            "QSSTV FAC payload fixture invalid");
    for (std::size_t i = 0; i < qsstvPayload.size(); ++i)
        require((*qsstvNativePayload.value)[i] == static_cast<std::uint8_t>(qsstvPayload[i] - '0'),
                "FAC payload differs from QSSTV");
    require(qsstvNativeCells.ok() && qsstvCellSigns.size() == 2U * qsstvNativeCells.value->size(),
            "QSSTV FAC cell fixture invalid");
    std::vector<HamDrmComplex> independentCells;
    const double magnitude = 1.0 / std::sqrt(2.0);
    for (std::size_t i = 0; i < qsstvCellSigns.size(); i += 2U) {
        independentCells.emplace_back(qsstvCellSigns[i] == '1' ? -magnitude : magnitude,
                                      qsstvCellSigns[i + 1U] == '1' ? -magnitude : magnitude);
        requireNear(qsstvNativeCells.value->at(i / 2U), independentCells.back(), 1e-12,
                    "FAC QAM cell differs from QSSTV");
    }
    const auto decodedQsstv = hamDrmDecodeFacCells(independentCells);
    require(decodedQsstv.ok() && decodedQsstv.value->callsignFragment == "TES"
                && decodedQsstv.value->parameters.packetId == 1U,
            "QSSTV FAC cells failed to decode");

    HamDrmFacParameters parameters;
    parameters.frameIdentity = 0U;
    parameters.occupiedBandwidth = HamDrmOccupiedBandwidth::Hz2500;
    parameters.interleaver = HamDrmInterleaver::Short;
    parameters.mscConstellation = HamDrmConstellation::Qam16;
    parameters.protection = HamDrmProtection::Normal;
    parameters.packetId = 2U;
    parameters.callsign = "9H1ABCXYZ";

    const auto payload = hamDrmEncodeFacPayloadBits(parameters);
    require(payload.ok() && payload.value->size() == 48U,
            "FAC payload did not encode");
    require(fnv1a(*payload.value) == 9'631'577'500'391'121'850ULL,
            "FAC payload deterministic vector changed: "
            + std::to_string(fnv1a(*payload.value)));
    const auto decodedPayload = hamDrmDecodeFacPayloadBits(*payload.value);
    require(decodedPayload.ok()
                && decodedPayload.value->parameters.frameIdentity == 0U
                && decodedPayload.value->callsignFragment == "9H1",
            "FAC payload round trip failed");

    for (const std::uint8_t pattern : {std::uint8_t{0x55}, std::uint8_t{0xaa}}) {
        auto reserved=*payload.value;
        for (std::size_t bit=31U;bit<40U;++bit)
            reserved[bit]=static_cast<std::uint8_t>((pattern >> (bit%8U)) & 1U);
        std::array<std::uint8_t,5> bytes{};
        for (std::size_t bit=0;bit<40U;++bit)
            bytes[bit/8U] |= static_cast<std::uint8_t>(reserved[bit] << (7U-bit%8U));
        const auto crc=hamDrmChannelCrc8(bytes.data(),bytes.size());
        for (std::size_t bit=0;bit<8U;++bit)
            reserved[40U+bit]=static_cast<std::uint8_t>((crc >> (7U-bit)) & 1U);
        const auto parsed=hamDrmDecodeFacPayloadBits(reserved);
        require(parsed.ok() && parsed.value->callsignFragment=="9H1"
                    && parsed.value->parameters.packetId==parameters.packetId,
                "CRC-valid FAC with unused bits was rejected");
        reserved[35U]^=1U;
        require(hamDrmDecodeFacPayloadBits(reserved).status.code==HamDrmErrorCode::CrcMismatch,
                "FAC CRC did not protect unused bits");
    }

    const auto cells = hamDrmEncodeFacCells(parameters);
    require(cells.ok() && cells.value->size() == 45U,
            "FAC channel coding did not produce 45 cells");
    const auto decoded = hamDrmDecodeFacCells(*cells.value);
    require(decoded.ok()
                && decoded.value->parameters.occupiedBandwidth
                    == parameters.occupiedBandwidth
                && decoded.value->parameters.interleaver
                    == parameters.interleaver
                && decoded.value->parameters.mscConstellation
                    == parameters.mscConstellation
                && decoded.value->parameters.protection
                    == parameters.protection
                && decoded.value->parameters.packetId == parameters.packetId
                && decoded.value->callsignFragment == "9H1",
            "FAC cell round trip failed");

    auto oneCarrierError = *cells.value;
    oneCarrierError[19U] = -oneCarrierError[19U];
    const auto corrected = hamDrmDecodeFacCells(oneCarrierError);
    require(corrected.ok() && corrected.value->callsignFragment == "9H1"
                && corrected.value->correctedBitMetric > 0U,
            "FAC Viterbi did not survive a corrupted carrier");

    auto corruptedPayload = *payload.value;
    corruptedPayload[11U] ^= 1U;
    require(hamDrmDecodeFacPayloadBits(corruptedPayload).status.code
                == HamDrmErrorCode::CrcMismatch,
            "FAC CRC corruption was not detected");
    parameters.mscConstellation = static_cast<HamDrmConstellation>(255U);
    require(hamDrmEncodeFacCells(parameters).status.code
                == HamDrmErrorCode::UnsupportedProfile,
            "unsupported FAC profile was accepted");
}

void testMscMlcProfiles()
{
    std::ifstream fixture(DECODIUM_QSSTV_MSC_FIXTURE);
    require(fixture.good(), "QSSTV MSC fixture not available");
    std::set<std::array<int, 3U>> combinations;
    int count, mode, protection, inputCount;
    std::string quantized;
    while (fixture >> count >> mode >> protection >> inputCount >> quantized) {
        require((count == 339 || count == 369 || count == 455 || count == 530
                 || count == 647 || count == 704)
                    && mode >= 0 && mode < 3 && protection >= 0 && protection < 2,
                "invalid QSSTV MSC fixture parameters");
        require(combinations.insert({count, mode, protection}).second,
                "duplicate QSSTV MSC fixture row");
        require(quantized.size() == static_cast<std::size_t>(count) * 2U,
                "invalid QSSTV MSC fixture length");
        const auto modulation = mode == 0 ? HamDrmConstellation::Qam4
            : (mode == 1 ? HamDrmConstellation::Qam16 : HamDrmConstellation::Qam64);
        const auto protectionLevel = protection ? HamDrmProtection::Normal : HamDrmProtection::High;
        const auto profile = hamDrmMlcProfile(static_cast<std::size_t>(count), modulation, protectionLevel);
        require(profile.ok() && profile.value->inputBits == static_cast<std::size_t>(inputCount),
                "MSC input capacity differs from QSSTV");
        const auto input = patternBits(profile.value->inputBits, 0x44524D31U);
        const auto local = hamDrmEncodeMscCells(input, *profile.value);
        require(local.ok() && local.value->size() == static_cast<std::size_t>(count),
                "MSC fixture did not encode");
        const double scale = std::sqrt(mode == 0 ? 2.0 : (mode == 1 ? 10.0 : 42.0));
        std::vector<HamDrmComplex> reference;
        for (int i = 0; i < count; ++i) {
            const int real = quantized[static_cast<std::size_t>(2 * i)] - 'A' - 7;
            const int imag = quantized[static_cast<std::size_t>(2 * i + 1)] - 'A' - 7;
            require(real >= -7 && real <= 7 && imag >= -7 && imag <= 7,
                    "invalid QSSTV MSC quantized cell");
            reference.emplace_back(real / scale, imag / scale);
            requireNear(local.value->at(static_cast<std::size_t>(i)), reference.back(), 1e-12,
                        "MSC QAM cell differs from QSSTV");
        }
        const auto decoded = hamDrmDecodeMscCells(reference, *profile.value);
        require(decoded.ok() && decoded.value->bits == input,
                "QSSTV MSC cells failed to decode");
    }
    require(fixture.eof() && combinations.size() == 36U, "QSSTV MSC fixture incomplete");

    const std::array<HamDrmConstellation, 3U> constellations {{
        HamDrmConstellation::Qam4,
        HamDrmConstellation::Qam16,
        HamDrmConstellation::Qam64
    }};
    const std::array<HamDrmProtection, 2U> protections {{
        HamDrmProtection::High, HamDrmProtection::Normal
    }};

    std::uint32_t seed = 0x44524D31U;
    for (const auto constellation : constellations) {
        for (const auto protection : protections) {
            const auto profile = hamDrmMlcProfile(127U, constellation,
                                                  protection);
            require(profile.ok(), "MSC MLC profile was rejected");
            const auto bits = patternBits(profile.value->inputBits, seed++);
            const auto cells = hamDrmEncodeMscCells(bits, *profile.value);
            require(cells.ok() && cells.value->size() == 127U,
                    "MSC MLC encoder cell count mismatch");
            const auto decoded = hamDrmDecodeMscCells(*cells.value,
                                                      *profile.value);
            require(decoded.ok() && decoded.value->bits == bits,
                    "MSC MLC round trip failed");

            auto corrupted = *cells.value;
            corrupted[63U] = -corrupted[63U];
            const auto corrected = hamDrmDecodeMscCells(corrupted,
                                                        *profile.value);
            require(corrected.ok() && corrected.value->bits == bits,
                    "MSC MLC failed on one corrupted carrier");
        }
    }

    const auto qam4High = hamDrmMlcProfile(
        127U, HamDrmConstellation::Qam4, HamDrmProtection::High);
    const auto qam4Normal = hamDrmMlcProfile(
        127U, HamDrmConstellation::Qam4, HamDrmProtection::Normal);
    require(qam4High.ok() && qam4Normal.ok()
                && qam4High.value->inputBits == qam4Normal.value->inputBits
                && qam4High.value->puncturePatternIndex
                    == qam4Normal.value->puncturePatternIndex,
            "pinned QAM4 protection-equivalence boundary changed");

    require(!hamDrmSdcSupportStatus().ok()
                && hamDrmSdcSupportStatus().code
                    == HamDrmErrorCode::UnsupportedFeature,
            "SDC capability boundary was not explicit");
    constexpr auto capabilities = hamDrmChannelSubsetCapabilities();
    static_assert(capabilities.fac && capabilities.msc && !capabilities.sdc,
                  "HAMDRM channel capability declaration changed");
    require(hamDrmMlcProfile(127U, static_cast<HamDrmConstellation>(255U),
                             HamDrmProtection::High).status.code
                == HamDrmErrorCode::UnsupportedProfile,
            "unsupported MSC constellation was accepted");
    require(hamDrmMlcProfile(127U, HamDrmConstellation::Qam16,
                             static_cast<HamDrmProtection>(255U)).status.code
                == HamDrmErrorCode::UnsupportedProfile,
            "unsupported MSC protection was accepted");
}

using TestFunction = void (*)();

} // namespace

int main()
{
    const std::array<std::pair<const char*, TestFunction>, 10U> tests {{
        {"CRC and energy vectors", &testCrcAndEnergyVectors},
        {"puncturing and Viterbi", &testPuncturingAndViterbi},
        {"block and symbol interleavers", &testBlockAndSymbolInterleavers},
        {"cell plans and pilots", &testCellPlansAndPilots},
        {"QSSTV receive interleaver map", &testQsstvReceiveInterleaverMap},
        {"QSSTV mapped symbols", &testQsstvMappedSymbols},
        {"QSSTV cell maps", &testQsstvCellMaps},
        {"FAC codec", &testFacCodec},
        {"MSC MLC profiles", &testMscMlcProfiles},
        {"MSC MLC deterministic repeat", &testMscMlcProfiles},
    }};

    std::size_t passed = 0U;
    for (const auto& test : tests) {
        try {
            test.second();
            ++passed;
            std::cout << "[PASS] " << test.first << '\n';
        } catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.first << ": " << error.what()
                      << '\n';
            return 1;
        }
    }
    std::cout << passed << '/' << tests.size()
              << " HAMDRM channel tests passed\n";
    return 0;
}
