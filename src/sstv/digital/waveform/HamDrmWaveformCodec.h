// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../HamDrmTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace decodium::sstv::hamdrm::waveform {

constexpr std::uint32_t kHamDrmPcmSampleRateHz = 12'000U;
constexpr std::size_t kHamDrmPcmSamplesPerFrame = 4'800U;
constexpr std::uint32_t kHamDrmNativePcmSampleRateHz = 48'000U;
constexpr std::size_t kHamDrmNativePcmSamplesPerFrame = 19'200U;

struct HamDrmWaveformSubsetCapabilities final
{
    bool motDataGroups {true};
    bool realPcm12kHz {true};
    bool realPcm48kHz {true};
    bool fac {true};
    bool mscPartB {true};
    bool sdc {false};
    bool mscPartA {false};
    bool vspp {false};
    bool blindProfileAcquisition {false};
    bool frequencySelectiveEqualization {false};
    bool independentlyCapturedInteropFixture {false};
};

constexpr HamDrmWaveformSubsetCapabilities
hamDrmWaveformSubsetCapabilities() noexcept
{
    return {};
}

struct HamDrmWaveformConfig final
{
    HamDrmRobustness robustness {HamDrmRobustness::E};
    HamDrmOccupiedBandwidth occupiedBandwidth {
        HamDrmOccupiedBandwidth::Hz2500};
    HamDrmConstellation constellation {HamDrmConstellation::Qam16};
    HamDrmProtection protection {HamDrmProtection::High};
    HamDrmInterleaver interleaver {HamDrmInterleaver::Long};
    std::uint8_t packetId {0U};
    std::string callsign {"DECODIUM"};
    double carrierOffsetHz {350.0};
    // 12 kHz is the native Decodium RX/DSP tap.  48 kHz uses the original
    // QSSTV/DRM OFDM FFT and guard sizes and is intended for SoundOutput TX.
    // No arbitrary-rate resampling is performed inside the waveform codec.
    std::uint32_t pcmSampleRateHz {kHamDrmPcmSampleRateHz};
    // QSSTV's pinned real-output path uses IFFT*N and a factor of 1000 for
    // signed 16-bit PCM.  Native double PCM expresses the same factor below.
    double outputGain {1'000.0 / 32'768.0};
};

struct HamDrmWaveformLimits final
{
    std::size_t maximumQueuedGroups {64U};
    std::size_t maximumQueuedDataBytes {1U << 20U};
    std::size_t maximumDataGroupBytes {16U * 1024U};
    std::size_t maximumBufferedPcmSamples {1U << 20U};
    std::size_t maximumFrames {4'096U};
    std::size_t maximumSyncSearchSamples {2'048U};
    double minimumCyclicPrefixMetric {0.015};
    double maximumFrequencyOffsetHz {15.0};
    double maximumNormalizedPilotSquaredError {0.12};
    double corruptedPilotSquaredErrorThreshold {0.25};
    double maximumCorruptedPilotFraction {0.30};
};

struct HamDrmWaveformCapacity final
{
    std::size_t usefulMscCellsPerFrame {0U};
    std::size_t decodedMscBitsPerFrame {0U};
    std::size_t packetBodyBytes {0U};
    std::size_t symbolsPerFrame {0U};
    std::size_t samplesPerSymbol {0U};
    std::size_t samplesPerFrame {0U};
    std::size_t interleaverDepthFrames {0U};
};

HamDrmValueResult<HamDrmWaveformCapacity> hamDrmWaveformCapacity(
    const HamDrmWaveformConfig& config,
    const HamDrmWaveformLimits& limits = {});

struct HamDrmWaveformStatistics final
{
    std::size_t transmittedFrames {0U};
    std::size_t receivedFrames {0U};
    std::size_t validatedDataGroups {0U};
    std::size_t synchronizationFailures {0U};
    std::size_t pilotFailures {0U};
    std::size_t facFailures {0U};
    std::size_t mscFailures {0U};
    std::size_t packetCrcFailures {0U};
    std::size_t motCrcFailures {0U};
    std::size_t resourceFailures {0U};
    double estimatedFrequencyOffsetHz {0.0};
    double lastCyclicPrefixMetric {0.0};
    double lastNormalizedPilotSquaredError {0.0};
    std::size_t lastFrameStartSample {0U};
};

class HamDrmWaveformTransmitter final
{
public:
    explicit HamDrmWaveformTransmitter(
        HamDrmWaveformConfig config,
        HamDrmWaveformLimits limits = {});
    ~HamDrmWaveformTransmitter();

    HamDrmWaveformTransmitter(HamDrmWaveformTransmitter&&) noexcept;
    HamDrmWaveformTransmitter& operator=(
        HamDrmWaveformTransmitter&&) noexcept;
    HamDrmWaveformTransmitter(const HamDrmWaveformTransmitter&) = delete;
    HamDrmWaveformTransmitter& operator=(
        const HamDrmWaveformTransmitter&) = delete;

    HamDrmStatus enqueueMotDataGroup(
        const std::vector<std::uint8_t>& encodedDataGroup);
    HamDrmStatus finish();
    std::vector<double> pullPcm(std::size_t maximumSamples);
    void cancel() noexcept;
    void reset();

    bool done() const noexcept;
    bool cancelled() const noexcept;
    HamDrmStatus lastStatus() const;
    HamDrmWaveformStatistics statistics() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

struct HamDrmWaveformReceiveBatch final
{
    HamDrmStatus status;
    std::vector<std::vector<std::uint8_t>> dataGroups;
    HamDrmWaveformStatistics statistics;
    bool synchronized {false};
    bool endOfStream {false};
};

class HamDrmWaveformReceiver final
{
public:
    explicit HamDrmWaveformReceiver(
        HamDrmWaveformConfig config,
        HamDrmWaveformLimits limits = {});
    ~HamDrmWaveformReceiver();

    HamDrmWaveformReceiver(HamDrmWaveformReceiver&&) noexcept;
    HamDrmWaveformReceiver& operator=(HamDrmWaveformReceiver&&) noexcept;
    HamDrmWaveformReceiver(const HamDrmWaveformReceiver&) = delete;
    HamDrmWaveformReceiver& operator=(const HamDrmWaveformReceiver&) = delete;

    HamDrmWaveformReceiveBatch pushPcm(const double* samples,
                                       std::size_t sampleCount,
                                       bool endOfStream = false);
    HamDrmWaveformReceiveBatch pushPcm(const std::vector<double>& samples,
                                       bool endOfStream = false);
    void cancel() noexcept;
    void reset();

    bool cancelled() const noexcept;
    HamDrmWaveformStatistics statistics() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace decodium::sstv::hamdrm::waveform
