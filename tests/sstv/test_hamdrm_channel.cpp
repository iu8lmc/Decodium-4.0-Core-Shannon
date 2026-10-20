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
#include <iostream>
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
    const std::array<std::pair<const char*, TestFunction>, 7U> tests {{
        {"CRC and energy vectors", &testCrcAndEnergyVectors},
        {"puncturing and Viterbi", &testPuncturingAndViterbi},
        {"block and symbol interleavers", &testBlockAndSymbolInterleavers},
        {"cell plans and pilots", &testCellPlansAndPilots},
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
