// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/digital/HamDrmMotCodec.h"
#include "../../src/sstv/digital/HamDrmObjectAssembler.h"
#include "../../src/sstv/digital/HamDrmImageValidator.h"
#include <QImage>
#include "../../src/sstv/digital/channel/HamDrmChannelCrc.h"
#include "../../src/sstv/digital/waveform/HamDrmPacketCodec.h"
#include "../../src/sstv/digital/waveform/HamDrmWaveformCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::channel;
using namespace decodium::sstv::hamdrm::waveform;

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

std::vector<std::uint8_t> makeMotGroup(std::size_t payloadBytes,
                                       std::uint16_t transportId = 0x4242U)
{
    HamDrmMotDataGroup group;
    group.kind = HamDrmMotGroupKind::Body;
    group.continuityIndex = 3U;
    group.segmentNumber = 7U;
    group.lastSegment = true;
    group.transportId = transportId;
    group.payload.resize(payloadBytes);
    std::uint32_t state = 0x13579BDFU;
    for (auto& byte : group.payload) {
        state = state * 1'664'525U + 1'013'904'223U;
        byte = static_cast<std::uint8_t>(state >> 24U);
    }
    const auto encoded = encodeHamDrmMotDataGroup(group);
    require(encoded.ok(), "MOT test group did not encode");
    return *encoded.value;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    for (const std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

std::vector<double> transmit(const HamDrmWaveformConfig& config,
                             const std::vector<std::vector<std::uint8_t>>& groups,
                             std::size_t pullSize = 733U)
{
    HamDrmWaveformTransmitter transmitter(config);
    for (const auto& group : groups) {
        require(transmitter.enqueueMotDataGroup(group).ok(),
                "waveform transmitter rejected valid MOT group: "
                    + transmitter.lastStatus().detail);
    }
    require(transmitter.finish().ok(), "waveform transmitter did not finish");
    std::vector<double> pcm;
    std::size_t emptyPulls = 0U;
    while (!transmitter.done()) {
        auto chunk = transmitter.pullPcm(pullSize);
        if (chunk.empty()) {
            require(++emptyPulls < 3U,
                    "waveform transmitter stalled: "
                        + transmitter.lastStatus().detail);
        } else {
            emptyPulls = 0U;
            pcm.insert(pcm.end(), chunk.begin(), chunk.end());
        }
    }
    const auto capacity = hamDrmWaveformCapacity(config);
    require(capacity.ok()
                && transmitter.statistics().transmittedFrames
                    * capacity.value->samplesPerFrame == pcm.size(),
            "transmitter statistics/sample count mismatch");
    return pcm;
}

HamDrmWaveformReceiveBatch receive(
    const HamDrmWaveformConfig& config,
    const std::vector<double>& pcm,
    std::size_t chunkSize,
    std::vector<std::vector<std::uint8_t>>& groups)
{
    HamDrmWaveformReceiver receiver(config);
    HamDrmWaveformReceiveBatch last;
    std::size_t offset = 0U;
    while (offset < pcm.size()) {
        const std::size_t count = std::min(chunkSize, pcm.size() - offset);
        last = receiver.pushPcm(pcm.data() + offset, count,
                                offset + count == pcm.size());
        groups.insert(groups.end(), last.dataGroups.begin(),
                      last.dataGroups.end());
        offset += count;
    }
    return last;
}

void testPacketFraming()
{
    // Executed independently from QSSTV's CDataEncoder at 8c27d6d, with
    // packet ID 1, 16-byte body, 35-byte data unit and initial index zero.
    const std::array<std::string, 3U> qsstvHex {{
        "90000102030405060708090a0b0c0d0e0f8727",
        "11101112131415161718191a1b1c1d1e1f229c",
        "5a03202122000000000000000000000000cc9b"}};
    std::vector<std::uint8_t> qsstvData(35U);
    for (std::size_t i = 0; i < qsstvData.size(); ++i)
        qsstvData[i] = static_cast<std::uint8_t>(i);
    const HamDrmPacketParameters qsstvParameters {16U, 1U, 4096U};
    const auto qsstvLocal = hamDrmPacketizeDataUnit(qsstvData, qsstvParameters, 0U);
    require(qsstvLocal.ok() && qsstvLocal.value->packets.size() == qsstvHex.size(),
            "QSSTV packet fixture setup failed");
    HamDrmPacketReassembler qsstvRx(qsstvParameters);
    for (std::size_t i = 0; i < qsstvHex.size(); ++i) {
        std::vector<std::uint8_t> packet;
        for (std::size_t j = 0; j < qsstvHex[i].size(); j += 2U)
            packet.push_back(static_cast<std::uint8_t>(std::stoul(qsstvHex[i].substr(j, 2U), nullptr, 16)));
        require(packet == qsstvLocal.value->packets[i], "QSSTV packet bytes differ");
        const auto result = qsstvRx.push(packet);
        require(result.ok(), "QSSTV packet rejected");
        if (i + 1U == qsstvHex.size())
            require(result.value->dataUnit && *result.value->dataUnit == qsstvData,
                    "QSSTV packet data unit differs");
    }

    const auto group = makeMotGroup(173U);
    const HamDrmPacketParameters parameters {87U, 2U, 4'096U};
    const auto packetized = hamDrmPacketizeDataUnit(group, parameters, 6U);
    require(packetized.ok() && packetized.value->packets.size() == 3U,
            "packetizer did not fragment deterministic data unit");
    require(packetized.value->nextContinuityIndex == 1U,
            "packet continuity did not wrap modulo eight");
    require(packetized.value->packets[0U][0U] == 0xA6U
                && packetized.value->packets[1U][0U] == 0x27U
                && packetized.value->packets[2U][0U] == 0x68U,
            "packet header flags/CI fixture mismatch");
    require(fnv1a(packetized.value->packets[0U])
                == 16'881'968'009'189'787'369ULL,
            "packet framing deterministic vector changed: "
                + std::to_string(fnv1a(packetized.value->packets[0U])));

    HamDrmPacketReassembler reassembler(parameters);
    std::optional<std::vector<std::uint8_t>> recovered;
    for (const auto& packet : packetized.value->packets) {
        const auto pushed = reassembler.push(packet);
        require(pushed.ok(), "valid packet did not reassemble");
        if (pushed.value->dataUnit.has_value()) {
            recovered = pushed.value->dataUnit;
        }
    }
    require(recovered.has_value() && *recovered == group,
            "packet/data-unit round trip failed");

    auto damaged = packetized.value->packets[1U];
    damaged[19U] ^= 0x20U;
    reassembler.reset();
    require(reassembler.push(packetized.value->packets[0U]).ok(),
            "packet reassembler setup failed");
    require(reassembler.push(damaged).status.code
                == HamDrmErrorCode::CrcMismatch,
            "packet CRC corruption was accepted");
    require(reassembler.push(packetized.value->packets[2U]).status.code
                == HamDrmErrorCode::Incomplete,
            "packet after broken continuity was accepted");

    const HamDrmPacketParameters wide {311U, 0U, 4'096U};
    const std::vector<std::uint8_t> unrepresentable(300U, 0x55U);
    require(hamDrmPacketizeDataUnit(unrepresentable, wide, 0U).status.code
                == HamDrmErrorCode::UnsupportedProfile,
            "unrepresentable one-byte PPI length was accepted");
}

void testCapacityAndExactScheduling()
{
    const std::array<HamDrmRobustness, 3U> robustness {{
        HamDrmRobustness::A,
        HamDrmRobustness::B,
        HamDrmRobustness::E
    }};
    const std::array<HamDrmOccupiedBandwidth, 2U> bandwidths {{
        HamDrmOccupiedBandwidth::Hz2300,
        HamDrmOccupiedBandwidth::Hz2500
    }};
    const std::array<std::size_t, 6U> expectedBody {{
        157U, 171U, 109U, 127U, 80U, 87U
    }};
    std::size_t fixture = 0U;
    for (const auto mode : robustness) {
        for (const auto bandwidth : bandwidths) {
            HamDrmWaveformConfig config;
            config.robustness = mode;
            config.occupiedBandwidth = bandwidth;
            const auto capacity = hamDrmWaveformCapacity(config);
            require(capacity.ok(), "waveform capacity profile missing");
            require(capacity.value->packetBodyBytes == expectedBody[fixture++],
                    "waveform packet capacity fixture mismatch");
            require(capacity.value->samplesPerFrame
                        == kHamDrmPcmSamplesPerFrame,
                    "HAMDRM profile is not exactly 400 ms at 12 kHz");
            require(capacity.value->samplesPerSymbol
                        * capacity.value->symbolsPerFrame
                            == kHamDrmPcmSamplesPerFrame,
                    "HAMDRM symbol/frame scheduler mismatch");
        }
    }
}

void testIndependentQsstvSuperframesFailClosed()
{
    HamDrmWaveformConfig config;
    config.pcmSampleRateHz = 48000;
    const std::array<const char*, 2> files {{DECODIUM_QSSTV_SUPERFRAME_A, DECODIUM_QSSTV_SUPERFRAME_B}};
    for (const auto* path : files) {
        std::ifstream input(path, std::ios::binary);
        require(input.good(), "QSSTV superframe fixture missing");
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
        require(bytes.size() == 3U * kHamDrmNativePcmSamplesPerFrame * 2U, "QSSTV fixture length incorrect");
        std::vector<double> pcm;
        for (std::size_t i=0;i<bytes.size();i+=2) {
            const auto raw = static_cast<std::uint16_t>(bytes[i] | (std::uint16_t(bytes[i+1]) << 8U));
            const int sample = raw < 32768U ? int(raw) : int(raw) - 65536;
            pcm.push_back(double(sample)/32768.0);
        }
        std::vector<std::vector<std::uint8_t>> a, b;
        const auto batchA = receive(config, pcm, 977U, a);
        const auto batchB = receive(config, pcm, 8192U, b);
        require(a.empty() && b.empty(), "synthetic FAC emitted a false MOT group");
        require(batchA.statistics.facFailures > 0 && batchB.statistics.facFailures > 0,
                "QSSTV superframe did not reach FAC validation: pilots="
                  + std::to_string(batchA.statistics.pilotFailures) + " sync="
                  + std::to_string(batchA.statistics.synchronizationFailures));
        require(batchA.statistics.pilotFailures == 0 && batchB.statistics.pilotFailures == 0,
                "QSSTV superframe pilots rejected");
        require(batchA.statistics.facFailures == batchB.statistics.facFailures,
                "QSSTV superframe outcome changed with PCM chunking");
    }
}

void testIndependentQsstvValidFac()
{
    HamDrmWaveformConfig config;
    config.pcmSampleRateHz = 48000;
    config.interleaver = HamDrmInterleaver::Short;
    const std::array<const char*, 2> files {{DECODIUM_QSSTV_VALID_FAC_A, DECODIUM_QSSTV_VALID_FAC_B}};
    for (const auto* path : files) {
        std::ifstream input(path, std::ios::binary);
        require(input.good(), "QSSTV valid FAC fixture missing");
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
        require(bytes.size() == 3U * kHamDrmNativePcmSamplesPerFrame * 2U, "QSSTV valid FAC fixture length incorrect");
        std::vector<double> pcm;
        for (std::size_t i=0;i<bytes.size();i+=2) {
            const auto raw = static_cast<std::uint16_t>(bytes[i] | (std::uint16_t(bytes[i+1]) << 8U));
            pcm.push_back(double(raw < 32768U ? int(raw) : int(raw) - 65536)/32768.0);
        }
        std::vector<std::vector<std::uint8_t>> a, b;
        const auto batchA = receive(config, pcm, 977U, a);
        const auto batchB = receive(config, pcm, 8192U, b);
        require(a.empty() && b.empty(), "random MSC emitted MOT data");
        require(batchA.statistics.receivedFrames > 0 && batchB.statistics.receivedFrames > 0,
                "QSSTV valid FAC did not decode: fac=" + std::to_string(batchA.statistics.facFailures)
                + " pilots=" + std::to_string(batchA.statistics.pilotFailures));
        require(batchA.statistics.receivedFrames == batchB.statistics.receivedFrames,
                "QSSTV FAC results differ between chunk sizes");
        require(batchA.statistics.packetCrcFailures + batchA.statistics.mscFailures > 0
                && batchB.statistics.packetCrcFailures + batchB.statistics.mscFailures > 0,
                "QSSTV random MSC did not reach a data integrity check");
    }
}

void testIndependentQsstvMotGroups()
{
    HamDrmWaveformConfig config;
    config.pcmSampleRateHz = 48000;
    config.interleaver = HamDrmInterleaver::Short;
    const std::array<const char*, 2> files {{DECODIUM_QSSTV_MOT_A, DECODIUM_QSSTV_MOT_B}};
    const std::array<std::string, 2> expectedHex {{
        "7300800012ee55001b000000400d840185000000004600cc0b0073616d706c652e6a706775b5",
        "7400800012ee55000412345678ef53"}};
    for (std::size_t variant=0;variant<files.size();++variant) {
        std::ifstream input(files[variant], std::ios::binary);
        require(input.good(), "QSSTV MOT PCM fixture missing");
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
        require(bytes.size() == 3U * kHamDrmNativePcmSamplesPerFrame * 2U, "QSSTV MOT PCM length incorrect");
        std::vector<double> pcm;
        for (std::size_t i=0;i<bytes.size();i+=2) {
            const auto raw=static_cast<std::uint16_t>(bytes[i] | (std::uint16_t(bytes[i+1])<<8U));
            pcm.push_back(double(raw<32768U ? int(raw) : int(raw)-65536)/32768.0);
        }
        std::vector<std::uint8_t> expected;
        for (std::size_t i=0;i<expectedHex[variant].size();i+=2)
            expected.push_back(static_cast<std::uint8_t>(std::stoul(expectedHex[variant].substr(i,2), nullptr, 16)));
        const auto nativePcm = transmit(config, {expected}, 977U);
        require(nativePcm.size() == kHamDrmNativePcmSamplesPerFrame,
                "native transmit frame has unexpected size");
        double maximumError = 0.0;
        for (std::size_t i=0; i<nativePcm.size(); ++i)
            maximumError = std::max(maximumError, std::abs(nativePcm[i] - pcm[i]));
        require(maximumError < 1.1/32768.0,
                "native/QSSTV PCM differ: max=" + std::to_string(maximumError));
        std::vector<std::vector<std::uint8_t>> a,b;
        const auto batchA=receive(config,pcm,977U,a);
        const auto batchB=receive(config,pcm,8192U,b);
        require(!a.empty() && a==b && std::all_of(a.begin(),a.end(),[&](const auto& item) { return item==expected; }),
            "QSSTV MOT waveform failed: groups=" + std::to_string(a.size())
            + " received=" + std::to_string(batchA.statistics.receivedFrames)
            + " FAC=" + std::to_string(batchA.statistics.facFailures)
            + " packet=" + std::to_string(batchA.statistics.packetCrcFailures)
            + " MOT=" + std::to_string(batchA.statistics.motCrcFailures)
            + " detail=" + batchA.status.detail);
        require(batchA.statistics.validatedDataGroups == batchB.statistics.validatedDataGroups,
                "QSSTV MOT group count changed with PCM chunking");
    }
}

void testIndependentQsstvObjectAssembly()
{
    HamDrmWaveformConfig config;
    config.pcmSampleRateHz=48000; config.interleaver=HamDrmInterleaver::Short;
    for (const auto* path : {DECODIUM_QSSTV_OBJECT_A, DECODIUM_QSSTV_OBJECT_B}) {
        std::ifstream input(path,std::ios::binary);
        require(input.good(),"QSSTV object PCM missing");
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
        require(bytes.size()==3U*kHamDrmNativePcmSamplesPerFrame*2U,"QSSTV object PCM length incorrect");
        std::vector<double> pcm;
        for(std::size_t i=0;i<bytes.size();i+=2) {
            const auto raw=static_cast<std::uint16_t>(bytes[i] | (std::uint16_t(bytes[i+1])<<8U));
            pcm.push_back(double(raw<32768U ? int(raw) : int(raw)-65536)/32768.0);
        }
        std::vector<std::vector<std::uint8_t>> a,b;
        receive(config,pcm,977U,a); receive(config,pcm,8192U,b);
        require(a==b && a.size()>=2,"QSSTV object groups did not decode");
        HamDrmObjectAssembler assembler(0xee55U);
        for(const auto& group:a) require(assembler.ingest(group.data(),group.size()).ok(),"QSSTV object group rejected");
        const auto object=assembler.assembledObject();
        require(object.ok() && object.value->originalBytes == std::vector<std::uint8_t>({0x12,0x34,0x56,0x78}),
                "QSSTV MOT object did not assemble");
    }
}

void testSelectiveRepairWaveform()
{
    const std::array<const char*, 2> images {{DECODIUM_QSSTV_IMAGE_A, DECODIUM_QSSTV_IMAGE_B}};
    for (std::size_t variant = 0; variant < images.size(); ++variant) {
        std::ifstream input(images[variant], std::ios::binary);
        require(input.good(), "repair JPEG fixture missing");
        const std::vector<std::uint8_t> expected((std::istreambuf_iterator<char>(input)), {});
        HamDrmMotObjectMetadata metadata;
        metadata.filename = variant == 0 ? "repair-a.jpg" : "repair-b.jpg";
        metadata.transportId = qsstvCompatibleTransportId(metadata.filename);
        const auto encoded = encodeHamDrmObject(metadata, expected, 50);
        require(encoded.ok() && encoded.value->bodyGroups.size() > 4,
                "repair fixture needs at least five segments");
        const std::size_t firstMissing = variant == 0 ? 1 : 2;
        const std::size_t secondMissing = variant == 0 ? 3 : encoded.value->bodyGroups.size() - 1;

        HamDrmObjectAssembler assembler(metadata.transportId);
        for (const auto& group : encoded.value->headerGroups)
            require(assembler.ingest(group.data(), group.size()).ok(), "partial header rejected");
        for (std::size_t index = 0; index < encoded.value->bodyGroups.size(); ++index) {
            if (index == firstMissing || index == secondMissing) continue;
            const auto& group = encoded.value->bodyGroups[index];
            require(assembler.ingest(group.data(), group.size()).ok(), "partial body rejected");
        }
        require(!assembler.assembledObject().ok(), "partial image unexpectedly complete");

        auto repair = encoded.value->headerGroups;
        repair.push_back(encoded.value->bodyGroups[firstMissing]);
        repair.push_back(encoded.value->bodyGroups[secondMissing]);
        HamDrmWaveformConfig config;
        config.interleaver = HamDrmInterleaver::Short;
        const auto pcm = transmit(config, repair, variant == 0 ? 733 : 997);
        std::vector<std::vector<std::uint8_t>> decoded;
        const auto batch = receive(config, pcm, variant == 0 ? 977 : 8192, decoded);
        require(batch.status.ok() && decoded == repair, "repair waveform groups differ");
        for (const auto& group : decoded)
            require(assembler.ingest(group.data(), group.size()).ok(), "repair group rejected");
        const auto result = assembler.assembledObject();
        require(result.ok() && result.value->originalBytes == expected,
                "waveform repair did not restore the exact JPEG");
    }
}

void testIndependentQsstvImageAssembly()
{
    HamDrmWaveformConfig config;
    config.pcmSampleRateHz=48000; config.interleaver=HamDrmInterleaver::Short;
    const std::array<const char*,2> files {{DECODIUM_QSSTV_IMAGE_PCM_A, DECODIUM_QSSTV_IMAGE_PCM_B}};
    const std::array<const char*,2> images {{DECODIUM_QSSTV_IMAGE_A, DECODIUM_QSSTV_IMAGE_B}};
    for (std::size_t variant=0;variant<files.size();++variant) {
        std::ifstream input(files[variant],std::ios::binary), imageInput(images[variant],std::ios::binary);
        require(input.good() && imageInput.good(), "QSSTV image fixture missing");
        const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
        const std::vector<std::uint8_t> expected((std::istreambuf_iterator<char>(imageInput)),{});
        require(bytes.size()==9U*kHamDrmNativePcmSamplesPerFrame*2U,"QSSTV image PCM length incorrect");
        std::vector<double> pcm;
        for(std::size_t i=0;i<bytes.size();i+=2) {
            const auto raw=static_cast<std::uint16_t>(bytes[i] | (std::uint16_t(bytes[i+1])<<8U));
            pcm.push_back(double(raw<32768U ? int(raw) : int(raw)-65536)/32768.0);
        }
        std::vector<std::vector<std::uint8_t>> a,b;
        const auto batchA=receive(config,pcm,977U,a); const auto batchB=receive(config,pcm,8192U,b);
        require(a==b && a.size()>=5,"QSSTV image groups failed: groups=" + std::to_string(a.size())
            + " FAC=" + std::to_string(batchA.statistics.facFailures)
            + " packet=" + std::to_string(batchA.statistics.packetCrcFailures)
            + " chunkB=" + std::to_string(batchB.statistics.validatedDataGroups));
        const auto first=parseHamDrmMotDataGroup(a.front().data(),a.front().size());
        require(first.ok(), "QSSTV image first group malformed");
        HamDrmObjectAssembler assembler(first.value->transportId);
        for(const auto& group:a) {
            const auto result=assembler.ingest(group.data(),group.size());
            const auto parsed=parseHamDrmMotDataGroup(group.data(),group.size());
            require(result.ok(),"QSSTV image group rejected: " + result.status.detail
                + " firstId=" + std::to_string(first.value->transportId)
                + " id=" + std::to_string(parsed.ok() ? parsed.value->transportId : 0));
        }
        const auto object=assembler.assembledObject();
        require(object.ok() && object.value->originalBytes==expected,"QSSTV image bytes did not assemble exactly");
        require(a.size()==9U, "QSSTV image did not provide all nine frames: " + std::to_string(a.size()));
        const auto nativePcm = transmit(config, a, 977U);
        require(nativePcm.size()==pcm.size(), "native image waveform has wrong duration");
        double maximumError=0.0;
        for(std::size_t i=0;i<pcm.size();++i)
            maximumError=std::max(maximumError,std::abs(nativePcm[i]-pcm[i]));
        require(maximumError < 1.1/32768.0, "native/QSSTV image waveform differs: max=" + std::to_string(maximumError));
        const auto info=validateHamDrmImage(object.value->metadata, object.value->originalBytes.data(), object.value->originalBytes.size());
        require(info.ok() && info.value->width==16 && info.value->height==16, "QSSTV image validation failed");
        const QImage decoded=QImage::fromData(object.value->originalBytes.data(), int(object.value->originalBytes.size()));
        require(!decoded.isNull() && decoded.width()==16 && decoded.height()==16,"QSSTV JPEG could not decode");
        const QColor pixel=decoded.pixelColor(8,8);
        require(variant==0 ? pixel.red()>200 && pixel.blue()<40 : pixel.blue()>200 && pixel.red()<40,
                "QSSTV JPEG pixels changed");
    }
}

void testCleanLongInterleaverLoopback()
{
    HamDrmWaveformConfig config;
    const auto group = makeMotGroup(41U);
    const auto pcm = transmit(config, {group}, 509U);
    require(pcm.size() == 5U * kHamDrmPcmSamplesPerFrame,
            "long interleaver did not schedule one data plus four flush frames");
    require(std::all_of(pcm.begin(), pcm.end(), [](double sample) {
                return std::isfinite(sample) && std::abs(sample) <= 1.0;
            }),
            "waveform transmitter emitted invalid normalized PCM");
    std::vector<std::vector<std::uint8_t>> recovered;
    const auto batch = receive(config, pcm, 911U, recovered);
    require(batch.status.ok(), "clean waveform loopback failed: "
                                   + batch.status.detail);
    require(recovered.size() == 1U && recovered[0U] == group,
            "clean long-interleaver waveform did not recover MOT group");
    require(batch.statistics.receivedFrames == 5U
                && batch.statistics.validatedDataGroups == 1U,
            "clean loopback statistics mismatch");
}

void testLiveAcquisitionAfterSilence()
{
    HamDrmWaveformConfig config;
    for (const std::size_t prefix : {12'000U, 19'137U}) {
        const auto group = makeMotGroup(prefix == 12'000U ? 41U : 42U);
        auto signal = transmit(config, {group});
        signal.insert(signal.begin(), prefix, 0.0);
        std::vector<std::vector<std::uint8_t>> recovered;
        receive(config, signal, 240U, recovered);
        require(recovered.size() == 1U && recovered.front() == group,
                "live acquisition failed after silence");
    }
}

void testReacquireAfterPartialFrame()
{
    HamDrmWaveformConfig config;
    for (const std::size_t fragmentLength : {1'000U, 2'000U}) {
        const auto group = makeMotGroup(fragmentLength == 1'000U ? 41U : 42U);
        const auto signal = transmit(config, {group});
        std::vector<double> interrupted(signal.begin(), signal.begin() + fragmentLength);
        interrupted.insert(interrupted.end(), 100U, 0.0);
        interrupted.insert(interrupted.end(), signal.begin(), signal.end());
        std::vector<std::vector<std::uint8_t>> recovered;
        const auto batch = receive(config, interrupted, 240U, recovered);
        require(batch.statistics.pilotFailures > 0U,
                "partial frame did not exercise a rejected pilot candidate");
        require(recovered.size() == 1U && recovered.front() == group,
                "receiver did not reacquire after a partial frame");
    }
}

void testLiveAcquisitionAfterNoise()
{
    HamDrmWaveformConfig config;
    for (const std::size_t prefix : {4'800U, 7'200U}) {
        const auto group = makeMotGroup(prefix == 4'800U ? 41U : 42U);
        const auto signal = transmit(config, {group});
        std::vector<double> pcm(prefix);
        std::uint32_t state = 0x12345678U;
        for (auto& sample : pcm) {
            state = state * 1'664'525U + 1'013'904'223U;
            sample = (double(state >> 8U) / double(1U << 24U) - 0.5) * 0.02;
        }
        pcm.insert(pcm.end(), signal.begin(), signal.end());
        std::vector<std::vector<std::uint8_t>> recovered;
        receive(config, pcm, 240U, recovered);
        require(recovered.size() == 1U && recovered.front() == group,
                "receiver failed after initial broadband noise");
    }
}

void testNative48kTransmitPath()
{
    HamDrmWaveformConfig decimated;
    decimated.robustness = HamDrmRobustness::A;
    decimated.interleaver = HamDrmInterleaver::Short;
    HamDrmWaveformConfig native = decimated;
    native.pcmSampleRateHz = kHamDrmNativePcmSampleRateHz;
    const auto nativeCapacity = hamDrmWaveformCapacity(native);
    require(nativeCapacity.ok()
                && nativeCapacity.value->samplesPerFrame
                    == kHamDrmNativePcmSamplesPerFrame,
            "native 48 kHz frame capacity mismatch");

    const auto group = makeMotGroup(23U);
    const auto pcm12 = transmit(decimated, {group}, 701U);
    const auto pcm48 = transmit(native, {group}, 2'803U);
    require(pcm48.size() == pcm12.size() * 4U,
            "native 48 kHz waveform duration mismatch");
    std::vector<double> downsampled;
    downsampled.reserve(pcm12.size());
    for (std::size_t index = 0U; index < pcm12.size(); ++index) {
        downsampled.push_back(pcm48[index * 4U]);
        require(std::abs(downsampled.back() - pcm12[index]) < 1.0e-10,
                "native 48 kHz OFDM is not an exact 4x representation");
    }
    std::vector<std::vector<std::uint8_t>> recovered;
    const auto batch = receive(decimated, downsampled, 977U, recovered);
    require(batch.status.ok() && recovered.size() == 1U
                && recovered.front() == group,
            "native 48 kHz waveform did not decode after exact decimation");
}

void testNativeProfileMatrixLoopback()
{
    const std::array<HamDrmRobustness, 6U> robustness {{
        HamDrmRobustness::A, HamDrmRobustness::A,
        HamDrmRobustness::B, HamDrmRobustness::B,
        HamDrmRobustness::E, HamDrmRobustness::E
    }};
    const std::array<HamDrmOccupiedBandwidth, 6U> bandwidth {{
        HamDrmOccupiedBandwidth::Hz2300,
        HamDrmOccupiedBandwidth::Hz2500,
        HamDrmOccupiedBandwidth::Hz2300,
        HamDrmOccupiedBandwidth::Hz2500,
        HamDrmOccupiedBandwidth::Hz2300,
        HamDrmOccupiedBandwidth::Hz2500
    }};
    const std::array<HamDrmConstellation, 3U> constellations {{
        HamDrmConstellation::Qam4,
        HamDrmConstellation::Qam16,
        HamDrmConstellation::Qam64
    }};
    const std::array<HamDrmProtection, 2U> protections {{
        HamDrmProtection::High,
        HamDrmProtection::Normal
    }};
    for (std::size_t index = 0U; index < robustness.size(); ++index) {
        for (const auto constellation : constellations) {
            for (const auto protection : protections) {
                HamDrmWaveformConfig config;
                config.robustness = robustness[index];
                config.occupiedBandwidth = bandwidth[index];
                config.constellation = constellation;
                config.protection = protection;
                config.interleaver = HamDrmInterleaver::Short;
                const auto group = makeMotGroup(17U,
                    static_cast<std::uint16_t>(0x7000U + index));
                const auto pcm = transmit(config, {group}, 4'800U);
                std::vector<std::vector<std::uint8_t>> recovered;
                const auto batch = receive(config, pcm, 4'800U, recovered);
                require(batch.status.ok() && recovered.size() == 1U
                            && recovered[0U] == group,
                        "native A/B/E bandwidth/QAM waveform matrix failed at row "
                            + std::to_string(index) + ": "
                            + batch.status.detail);
            }
        }
    }
}

void testMultiFrameMotObjectGroups()
{
    HamDrmMotObjectMetadata metadata;
    metadata.transportId = 0x3131U;
    metadata.filename = "native.jp2";
    std::vector<std::uint8_t> body(211U, 0U);
    for (std::size_t index = 0U; index < body.size(); ++index) {
        body[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
    }
    const auto object = encodeHamDrmObject(metadata, body, 53U);
    require(object.ok(), "test MOT object did not encode");
    std::vector<std::vector<std::uint8_t>> groups =
        object.value->headerGroups;
    groups.insert(groups.end(), object.value->bodyGroups.begin(),
                  object.value->bodyGroups.end());
    require(groups.size() > 3U, "MOT object fixture did not span groups");

    HamDrmWaveformConfig config;
    config.interleaver = HamDrmInterleaver::Short;
    const auto pcm = transmit(config, groups, 1'337U);
    std::vector<std::vector<std::uint8_t>> recovered;
    const auto batch = receive(config, pcm, 743U, recovered);
    require(batch.status.ok(), "multi-frame MOT object waveform failed: "
                                   + batch.status.detail);
    require(recovered == groups,
            "multi-frame waveform changed MOT object-group order/content");
}

void testCfoOffsetAndAwgnLoopback()
{
    HamDrmWaveformConfig transmitConfig;
    transmitConfig.carrierOffsetHz = 354.0;
    HamDrmWaveformConfig receiveConfig;
    receiveConfig.carrierOffsetHz = 350.0;
    const auto group = makeMotGroup(29U, 0x5050U);
    auto pcm = transmit(transmitConfig, {group}, 2'003U);
    std::mt19937 generator(0xC0DEC0DEU);
    std::normal_distribution<double> noise(0.0, 0.00035);
    std::vector<double> impaired(137U, 0.0);
    impaired.reserve(137U + pcm.size());
    for (double sample : pcm) {
        impaired.push_back(sample * 0.82 + noise(generator));
    }
    for (std::size_t index = 0U; index < 137U; ++index) {
        impaired[index] = noise(generator);
    }

    std::vector<std::vector<std::uint8_t>> recovered;
    const auto batch = receive(receiveConfig, impaired, 4'801U, recovered);
    require(batch.status.ok(), "impaired waveform loopback failed: "
                                   + batch.status.detail
                                   + " groups=" + std::to_string(recovered.size())
                                   + " frames=" + std::to_string(
                                       batch.statistics.receivedFrames)
                                   + " cfo=" + std::to_string(
                                       batch.statistics.estimatedFrequencyOffsetHz)
                                   + " cp=" + std::to_string(
                                       batch.statistics.lastCyclicPrefixMetric)
                                   + " start=" + std::to_string(
                                       batch.statistics.lastFrameStartSample)
                                   + " pilot=" + std::to_string(
                                       batch.statistics.lastNormalizedPilotSquaredError));
    require(recovered.size() == 1U && recovered[0U] == group,
            "CFO/timing/AWGN waveform did not recover MOT group");
    require(std::abs(batch.statistics.estimatedFrequencyOffsetHz - 4.0)
                < 0.45,
            "pilot-aided CFO estimate is outside deterministic tolerance: "
                + std::to_string(
                    batch.statistics.estimatedFrequencyOffsetHz));
}

void testCorruptionDropAndFailClosed()
{
    HamDrmWaveformConfig config;
    config.interleaver = HamDrmInterleaver::Short;
    const auto group = makeMotGroup(37U, 0x6161U);
    const auto clean = transmit(config, {group});
    require(clean.size() == kHamDrmPcmSamplesPerFrame,
            "short interleaver scheduled unexpected flush frames");

    auto corrupted = clean;
    for (std::size_t index = 1'400U; index < 1'720U; ++index) {
        corrupted[index] = (index & 1U) == 0U ? 0.95 : -0.95;
    }
    std::vector<std::vector<std::uint8_t>> recovered;
    const auto corruptBatch = receive(config, corrupted, 997U, recovered);
    require(recovered.empty(), "corrupted carrier emitted an MOT group");
    require(!corruptBatch.status.ok()
                || corruptBatch.statistics.packetCrcFailures != 0U
                || corruptBatch.statistics.pilotFailures != 0U
                || corruptBatch.statistics.facFailures != 0U,
            "corrupted carrier was not reported");

    auto dropped = clean;
    dropped.erase(dropped.begin() + 1'111);
    recovered.clear();
    const auto dropBatch = receive(config, dropped, 877U, recovered);
    require(recovered.empty(), "sample drop emitted an MOT group");
    require(!dropBatch.status.ok(), "sample drop was not reported fail-closed");
}

void testCancelResetAndBounds()
{
    HamDrmWaveformConfig config;
    config.interleaver = HamDrmInterleaver::Short;
    const auto group = makeMotGroup(18U);
    HamDrmWaveformTransmitter transmitter(config);
    require(transmitter.enqueueMotDataGroup(group).ok(),
            "transmitter setup failed");
    transmitter.cancel();
    require(transmitter.cancelled() && transmitter.pullPcm(100U).empty(),
            "transmitter cancellation did not stop PCM");
    transmitter.reset();
    require(!transmitter.cancelled()
                && transmitter.enqueueMotDataGroup(group).ok()
                && transmitter.finish().ok(),
            "transmitter reset did not restore operation");

    HamDrmWaveformReceiver receiver(config);
    receiver.cancel();
    require(receiver.cancelled()
                && !receiver.pushPcm(std::vector<double>(64U, 0.0)).status.ok(),
            "receiver cancellation did not stop input");
    receiver.reset();
    require(!receiver.cancelled(), "receiver reset did not clear cancellation");

    HamDrmWaveformLimits tight;
    tight.maximumQueuedGroups = 1U;
    tight.maximumQueuedDataBytes = group.size();
    tight.maximumDataGroupBytes = group.size();
    HamDrmWaveformTransmitter bounded(config, tight);
    require(bounded.enqueueMotDataGroup(group).ok(),
            "bounded transmitter rejected first group");
    require(bounded.enqueueMotDataGroup(group).code
                == HamDrmErrorCode::LimitExceeded,
            "bounded transmitter accepted queue overflow");

    HamDrmWaveformLimits receiveLimits;
    receiveLimits.maximumBufferedPcmSamples = kHamDrmPcmSamplesPerFrame;
    receiveLimits.maximumSyncSearchSamples = 0U;
    HamDrmWaveformReceiver boundedReceiver(config, receiveLimits);
    const std::vector<double> oversized(kHamDrmPcmSamplesPerFrame + 1U, 0.0);
    require(boundedReceiver.pushPcm(oversized).status.code
                == HamDrmErrorCode::LimitExceeded,
            "receiver accepted PCM beyond configured bound");

    HamDrmWaveformConfig unsupported = config;
    unsupported.robustness = static_cast<HamDrmRobustness>(255U);
    requireThrows<std::invalid_argument>(
        [&unsupported]() {
            HamDrmWaveformTransmitter invalid(unsupported);
        },
        "unsupported waveform profile was accepted");
    HamDrmWaveformConfig nonFinite = config;
    nonFinite.carrierOffsetHz = std::numeric_limits<double>::quiet_NaN();
    requireThrows<std::invalid_argument>(
        [&nonFinite]() {
            HamDrmWaveformReceiver invalid(nonFinite);
        },
        "non-finite carrier offset was accepted");
    HamDrmWaveformConfig arbitraryRate = config;
    arbitraryRate.pcmSampleRateHz = 44'100U;
    requireThrows<std::invalid_argument>(
        [&arbitraryRate]() {
            HamDrmWaveformTransmitter invalid(arbitraryRate);
        },
        "arbitrary HAMDRM PCM sample rate was accepted");
}

} // namespace

int main()
{
    const std::array<std::pair<const char*, void (*)()>, 19U> tests {{
        {"packet framing", testPacketFraming},
        {"capacity and scheduling", testCapacityAndExactScheduling},
        {"QSSTV independent JPEG images", testIndependentQsstvImageAssembly},
        {"selective repair waveform A/B", testSelectiveRepairWaveform},
        {"QSSTV independent MOT object", testIndependentQsstvObjectAssembly},
        {"QSSTV independent MOT groups", testIndependentQsstvMotGroups},
        {"QSSTV valid FAC superframes", testIndependentQsstvValidFac},
        {"QSSTV synthetic superframes", testIndependentQsstvSuperframesFailClosed},
        {"clean long loopback", testCleanLongInterleaverLoopback},
        {"live acquisition after silence A/B", testLiveAcquisitionAfterSilence},
        {"partial frame reacquisition A/B", testReacquireAfterPartialFrame},
        {"live acquisition after noise A/B", testLiveAcquisitionAfterNoise},
        {"native 48 kHz TX", testNative48kTransmitPath},
        {"native profile matrix", testNativeProfileMatrixLoopback},
        {"multi-frame MOT object", testMultiFrameMotObjectGroups},
        {"CFO offset AWGN", testCfoOffsetAndAwgnLoopback},
        {"corruption/drop fail closed", testCorruptionDropAndFailClosed},
        {"cancel/reset/bounds", testCancelResetAndBounds},
        {"repeat clean loopback", testCleanLongInterleaverLoopback}
    }};
    try {
        for (const auto& test : tests) {
            test.second();
            std::cout << "PASS: " << test.first << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
