// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/digital/HamDrmMotCodec.h"
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
    const std::array<std::pair<const char*, void (*)()>, 10U> tests {{
        {"packet framing", testPacketFraming},
        {"capacity and scheduling", testCapacityAndExactScheduling},
        {"clean long loopback", testCleanLongInterleaverLoopback},
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
