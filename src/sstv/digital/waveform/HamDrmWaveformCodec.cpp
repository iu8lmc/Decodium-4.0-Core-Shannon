// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmWaveformCodec.h"

#include "HamDrmPacketCodec.h"
#include "../HamDrmMotCodec.h"
#include "../channel/HamDrmCellPlan.h"
#include "../channel/HamDrmFacCodec.h"
#include "../channel/HamDrmInterleaver.h"
#include "../channel/HamDrmMlcCodec.h"
#include "../channel/HamDrmPilotEqualizer.h"
#include "../phy/HamDrmComplexTransform.h"
#include "../phy/HamDrmOfdmParameters.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace decodium::sstv::hamdrm::waveform {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::size_t kFramesPerSuperframe = 3U;

HamDrmStatus failure(HamDrmErrorCode code, const std::string& detail)
{
    return HamDrmStatus::failure(code, detail);
}

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool limitsValid(const HamDrmWaveformLimits& limits,
                 std::size_t samplesPerFrame) noexcept
{
    return limits.maximumQueuedGroups != 0U
        && limits.maximumQueuedDataBytes != 0U
        && limits.maximumDataGroupBytes != 0U
        && samplesPerFrame != 0U
        && limits.maximumBufferedPcmSamples >= samplesPerFrame
        && limits.maximumFrames != 0U
        && limits.maximumSyncSearchSamples
            <= limits.maximumBufferedPcmSamples
                - samplesPerFrame
        && finite(limits.minimumCyclicPrefixMetric)
        && limits.minimumCyclicPrefixMetric >= 0.0
        && limits.minimumCyclicPrefixMetric <= 1.0
        && finite(limits.maximumFrequencyOffsetHz)
        && limits.maximumFrequencyOffsetHz >= 0.0
        && finite(limits.maximumNormalizedPilotSquaredError)
        && limits.maximumNormalizedPilotSquaredError >= 0.0
        && finite(limits.corruptedPilotSquaredErrorThreshold)
        && limits.corruptedPilotSquaredErrorThreshold >= 0.0
        && finite(limits.maximumCorruptedPilotFraction)
        && limits.maximumCorruptedPilotFraction >= 0.0
        && limits.maximumCorruptedPilotFraction <= 1.0;
}

struct WaveformProfile final
{
    phy::HamDrmOfdmParameters ofdm;
    channel::HamDrmCellPlan cellPlan;
    channel::HamDrmMlcProfile mlc;
    HamDrmWaveformCapacity capacity;
    std::uint32_t pcmSampleRateHz {kHamDrmPcmSampleRateHz};
    std::size_t usefulSamples {0U};
    std::size_t guardSamples {0U};
};

HamDrmValueResult<WaveformProfile> buildProfile(
    const HamDrmWaveformConfig& config,
    const HamDrmWaveformLimits& limits)
{
    const bool supportedPcmRate =
        config.pcmSampleRateHz == kHamDrmPcmSampleRateHz
        || config.pcmSampleRateHz == kHamDrmNativePcmSampleRateHz;
    const std::size_t samplesPerFrame = supportedPcmRate
        ? static_cast<std::size_t>(config.pcmSampleRateHz) * 2U / 5U
        : 0U;
    if (!supportedPcmRate || !limitsValid(limits, samplesPerFrame)
            || config.packetId > 3U
            || config.callsign.size() > 9U
            || !finite(config.carrierOffsetHz)
            || !finite(config.outputGain) || config.outputGain <= 0.0
            || config.outputGain > 1.0) {
        return {std::nullopt,
                failure(HamDrmErrorCode::InvalidArgument,
                        "invalid HAMDRM waveform configuration or limits")};
    }
    const auto ofdm = phy::hamDrmOfdmParameters(config.robustness,
                                                 config.occupiedBandwidth);
    if (!ofdm.has_value()) {
        return {std::nullopt,
                failure(HamDrmErrorCode::UnsupportedProfile,
                        "unsupported HAMDRM OFDM waveform profile")};
    }
    const double minimumTone = config.carrierOffsetHz
        + static_cast<double>(ofdm->minimumCarrier)
            * ofdm->carrierSpacingHz();
    const double maximumTone = config.carrierOffsetHz
        + static_cast<double>(ofdm->maximumCarrier)
            * ofdm->carrierSpacingHz();
    if (minimumTone <= 0.0
            || maximumTone
                >= static_cast<double>(config.pcmSampleRateHz) / 2.0) {
        return {std::nullopt,
                failure(HamDrmErrorCode::UnsupportedProfile,
                        "HAMDRM real PCM carrier range is outside Nyquist")};
    }
    const auto plan = channel::hamDrmBuildCellPlan(*ofdm);
    if (!plan.ok()) {
        return {std::nullopt, plan.status};
    }
    const auto mlc = channel::hamDrmMlcProfile(
        plan.value->usefulMscCellsPerFrame,
        config.constellation, config.protection);
    if (!mlc.ok()) {
        return {std::nullopt, mlc.status};
    }
    const std::size_t decodedBytes = mlc.value->inputBits / 8U;
    if (decodedBytes <= kHamDrmPacketOverheadBytes) {
        return {std::nullopt,
                failure(HamDrmErrorCode::UnsupportedProfile,
                        "HAMDRM MSC frame cannot hold a fixed packet")};
    }
    const std::size_t decimation = static_cast<std::size_t>(
        kHamDrmNativePcmSampleRateHz / config.pcmSampleRateHz);
    if (decimation == 0U
            || (ofdm->fftSize % decimation) != 0U
            || (ofdm->guardIntervalSamples % decimation) != 0U) {
        return {std::nullopt,
                failure(HamDrmErrorCode::UnsupportedProfile,
                        "HAMDRM OFDM sizes do not match the selected PCM rate")};
    }

    WaveformProfile profile;
    profile.ofdm = *ofdm;
    profile.cellPlan = *plan.value;
    profile.mlc = *mlc.value;
    profile.pcmSampleRateHz = config.pcmSampleRateHz;
    profile.usefulSamples = ofdm->fftSize / decimation;
    profile.guardSamples = ofdm->guardIntervalSamples / decimation;
    profile.capacity.usefulMscCellsPerFrame =
        profile.cellPlan.usefulMscCellsPerFrame;
    profile.capacity.decodedMscBitsPerFrame = profile.mlc.inputBits;
    profile.capacity.packetBodyBytes = decodedBytes
        - kHamDrmPacketOverheadBytes;
    profile.capacity.symbolsPerFrame = ofdm->symbolsPerFrame;
    profile.capacity.samplesPerSymbol = profile.usefulSamples
        + profile.guardSamples;
    profile.capacity.samplesPerFrame = profile.capacity.samplesPerSymbol
        * profile.capacity.symbolsPerFrame;
    profile.capacity.interleaverDepthFrames =
        config.interleaver == HamDrmInterleaver::Long ? 5U : 1U;
    if (profile.capacity.samplesPerFrame != samplesPerFrame) {
        return {std::nullopt,
                failure(HamDrmErrorCode::Malformed,
                        "HAMDRM frame is not exactly 400 ms")};
    }
    return {std::move(profile), HamDrmStatus::success()};
}

WaveformProfile requireProfile(const HamDrmWaveformConfig& config,
                               const HamDrmWaveformLimits& limits)
{
    auto result = buildProfile(config, limits);
    if (!result.ok()) {
        throw std::invalid_argument(result.status.detail);
    }
    return std::move(*result.value);
}

std::vector<std::uint8_t> bytesToBits(
    const std::vector<std::uint8_t>& bytes,
    std::size_t bitCount)
{
    if (bytes.size() > bitCount / 8U) {
        throw std::invalid_argument("HAMDRM byte vector exceeds bit capacity");
    }
    std::vector<std::uint8_t> bits(bitCount, 0U);
    for (std::size_t byte = 0U; byte < bytes.size(); ++byte) {
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            bits[byte * 8U + bit] = static_cast<std::uint8_t>(
                (bytes[byte] >> (7U - bit)) & 1U);
        }
    }
    return bits;
}

std::vector<std::uint8_t> bitsToBytes(
    const std::vector<std::uint8_t>& bits,
    std::size_t byteCount)
{
    if (byteCount > bits.size() / 8U) {
        throw std::invalid_argument("HAMDRM bit vector is too short");
    }
    std::vector<std::uint8_t> bytes(byteCount, 0U);
    for (std::size_t byte = 0U; byte < byteCount; ++byte) {
        for (std::size_t bit = 0U; bit < 8U; ++bit) {
            bytes[byte] = static_cast<std::uint8_t>(
                (bytes[byte] << 1U) | bits[byte * 8U + bit]);
        }
    }
    return bytes;
}

bool facMatchesConfig(const channel::HamDrmFacDecodeResult& decoded,
                      const HamDrmWaveformConfig& config) noexcept
{
    return decoded.parameters.occupiedBandwidth == config.occupiedBandwidth
        && decoded.parameters.interleaver == config.interleaver
        && decoded.parameters.mscConstellation == config.constellation
        && decoded.parameters.protection == config.protection
        && decoded.parameters.packetId == config.packetId;
}

bool isPilot(channel::HamDrmCellKind kind) noexcept
{
    return channel::hamDrmCellHas(
               kind, channel::HamDrmCellKind::ScatteredPilot)
        || channel::hamDrmCellHas(
               kind, channel::HamDrmCellKind::TimePilot)
        || channel::hamDrmCellHas(
               kind, channel::HamDrmCellKind::FrequencyPilot);
}

class RealToneLeastSquares final
{
public:
    RealToneLeastSquares(const WaveformProfile& profile,
                         double carrierOffsetHz)
        : usefulSamples_(profile.usefulSamples),
          carrierCount_(profile.ofdm.carrierCount()),
          columnCount_(2U * carrierCount_),
          matrix_(usefulSamples_ * columnCount_, 0.0),
          cholesky_(columnCount_ * columnCount_, 0.0)
    {
        if (columnCount_ > usefulSamples_) {
            throw std::invalid_argument(
                "HAMDRM real-tone system is underdetermined");
        }
        const double spacing = profile.ofdm.carrierSpacingHz();
        for (std::size_t sample = 0U; sample < usefulSamples_; ++sample) {
            for (std::size_t carrierIndex = 0U;
                 carrierIndex < carrierCount_; ++carrierIndex) {
                const int carrier = profile.ofdm.minimumCarrier
                    + static_cast<int>(carrierIndex);
                const double tone = carrierOffsetHz
                    + spacing * static_cast<double>(carrier);
                const double phase = 2.0 * kPi * tone
                    * static_cast<double>(sample)
                    / static_cast<double>(profile.pcmSampleRateHz);
                matrix_[sample * columnCount_ + 2U * carrierIndex] =
                    std::cos(phase);
                matrix_[sample * columnCount_ + 2U * carrierIndex + 1U] =
                    -std::sin(phase);
            }
        }
        for (std::size_t row = 0U; row < columnCount_; ++row) {
            for (std::size_t column = 0U; column <= row; ++column) {
                double value = 0.0;
                for (std::size_t sample = 0U; sample < usefulSamples_;
                     ++sample) {
                    value += matrix_[sample * columnCount_ + row]
                        * matrix_[sample * columnCount_ + column];
                }
                for (std::size_t inner = 0U; inner < column; ++inner) {
                    value -= cholesky_[row * columnCount_ + inner]
                        * cholesky_[column * columnCount_ + inner];
                }
                if (row == column) {
                    if (value <= 1.0e-10 || !finite(value)) {
                        throw std::invalid_argument(
                            "HAMDRM real-tone system is singular");
                    }
                    cholesky_[row * columnCount_ + column] =
                        std::sqrt(value);
                } else {
                    cholesky_[row * columnCount_ + column] = value
                        / cholesky_[column * columnCount_ + column];
                }
            }
        }
    }

    std::vector<phy::HamDrmComplex> demodulate(
        const double* samples) const
    {
        if (samples == nullptr) {
            throw std::invalid_argument("null HAMDRM real-tone samples");
        }
        std::vector<double> rhs(columnCount_, 0.0);
        for (std::size_t sample = 0U; sample < usefulSamples_; ++sample) {
            for (std::size_t column = 0U; column < columnCount_; ++column) {
                rhs[column] += matrix_[sample * columnCount_ + column]
                    * samples[sample];
            }
        }
        std::vector<double> intermediate(columnCount_, 0.0);
        for (std::size_t row = 0U; row < columnCount_; ++row) {
            double value = rhs[row];
            for (std::size_t column = 0U; column < row; ++column) {
                value -= cholesky_[row * columnCount_ + column]
                    * intermediate[column];
            }
            intermediate[row] = value
                / cholesky_[row * columnCount_ + row];
        }
        std::vector<double> solution(columnCount_, 0.0);
        for (std::size_t reverse = columnCount_; reverse > 0U; --reverse) {
            const std::size_t row = reverse - 1U;
            double value = intermediate[row];
            for (std::size_t column = row + 1U; column < columnCount_;
                 ++column) {
                value -= cholesky_[column * columnCount_ + row]
                    * solution[column];
            }
            solution[row] = value
                / cholesky_[row * columnCount_ + row];
        }
        std::vector<phy::HamDrmComplex> carriers(carrierCount_);
        for (std::size_t index = 0U; index < carrierCount_; ++index) {
            carriers[index] = {solution[2U * index],
                               solution[2U * index + 1U]};
        }
        return carriers;
    }

private:
    std::size_t usefulSamples_;
    std::size_t carrierCount_;
    std::size_t columnCount_;
    std::vector<double> matrix_;
    std::vector<double> cholesky_;
};

phy::HamDrmComplex pilotGain(
    const WaveformProfile& profile,
    std::size_t absoluteSymbol,
    const std::vector<phy::HamDrmComplex>& carriers)
{
    const auto* descriptors = profile.cellPlan.symbolCells(absoluteSymbol);
    if (descriptors == nullptr
            || carriers.size() != profile.ofdm.carrierCount()) {
        return {};
    }
    phy::HamDrmComplex cross {};
    double energy = 0.0;
    for (std::size_t index = 0U; index < carriers.size(); ++index) {
        if (isPilot(descriptors[index].kind)) {
            cross += carriers[index]
                * std::conj(descriptors[index].pilotValue);
            energy += std::norm(descriptors[index].pilotValue);
        }
    }
    return energy > std::numeric_limits<double>::epsilon()
        ? cross / energy : phy::HamDrmComplex {};
}

void recordFirstFailure(HamDrmStatus& destination,
                        const HamDrmStatus& candidate)
{
    if (destination.ok() && !candidate.ok()) {
        destination = candidate;
    }
}

} // namespace

HamDrmValueResult<HamDrmWaveformCapacity> hamDrmWaveformCapacity(
    const HamDrmWaveformConfig& config,
    const HamDrmWaveformLimits& limits)
{
    const auto profile = buildProfile(config, limits);
    if (!profile.ok()) {
        return {std::nullopt, profile.status};
    }
    return {profile.value->capacity, HamDrmStatus::success()};
}

class HamDrmWaveformTransmitter::Implementation final
{
public:
    Implementation(HamDrmWaveformConfig configuration,
                   HamDrmWaveformLimits resourceLimits)
        : config(std::move(configuration)),
          limits(resourceLimits),
          profile(requireProfile(config, limits)),
          interleaver(profile.capacity.usefulMscCellsPerFrame,
                      profile.capacity.interleaverDepthFrames),
          transform(2'048U)
    {
    }

    struct QueuedPacket final
    {
        std::vector<std::uint8_t> bytes;
        bool lastInGroup {false};
        std::size_t groupBytes {0U};
    };

    HamDrmStatus enqueue(const std::vector<std::uint8_t>& group)
    {
        if (cancelled || terminalError) {
            return setFailure(HamDrmErrorCode::Incomplete,
                              "HAMDRM transmitter is not active");
        }
        if (finishing) {
            return setFailure(HamDrmErrorCode::InvalidArgument,
                              "HAMDRM transmitter was already finished");
        }
        if (group.empty() || group.size() > limits.maximumDataGroupBytes) {
            ++statistics.resourceFailures;
            return setFailure(HamDrmErrorCode::LimitExceeded,
                              "HAMDRM MOT group exceeds waveform limit");
        }
        const auto parsed = parseHamDrmMotDataGroup(group.data(),
                                                     group.size());
        if (!parsed.ok()) {
            return setStatus(parsed.status);
        }
        if (queuedGroups >= limits.maximumQueuedGroups
                || group.size() > limits.maximumQueuedDataBytes
                || queuedBytes
                    > limits.maximumQueuedDataBytes - group.size()) {
            ++statistics.resourceFailures;
            return setFailure(HamDrmErrorCode::LimitExceeded,
                              "HAMDRM waveform transmit queue is full");
        }
        const HamDrmPacketParameters packetParameters {
            profile.capacity.packetBodyBytes,
            config.packetId,
            limits.maximumDataGroupBytes
        };
        const auto packetized = hamDrmPacketizeDataUnit(
            group, packetParameters, continuityIndex);
        if (!packetized.ok()) {
            return setStatus(packetized.status);
        }
        for (std::size_t index = 0U;
             index < packetized.value->packets.size(); ++index) {
            queue.push_back({packetized.value->packets[index],
                             index + 1U
                                == packetized.value->packets.size(),
                             group.size()});
        }
        continuityIndex = packetized.value->nextContinuityIndex;
        ++queuedGroups;
        queuedBytes += group.size();
        last = HamDrmStatus::success();
        return last;
    }

    HamDrmStatus finishStream()
    {
        if (cancelled || terminalError) {
            return setFailure(HamDrmErrorCode::Incomplete,
                              "HAMDRM transmitter is not active");
        }
        finishing = true;
        last = HamDrmStatus::success();
        return last;
    }

    std::vector<double> pull(std::size_t maximumSamples)
    {
        if (maximumSamples == 0U || cancelled || terminalError) {
            return {};
        }
        if (pendingOffset == pendingPcm.size()) {
            pendingPcm.clear();
            pendingOffset = 0U;
            if (!generateFrame()) {
                return {};
            }
        }
        const std::size_t available = pendingPcm.size() - pendingOffset;
        const std::size_t count = std::min(available, maximumSamples);
        std::vector<double> output(
            pendingPcm.begin() + static_cast<std::ptrdiff_t>(pendingOffset),
            pendingPcm.begin()
                + static_cast<std::ptrdiff_t>(pendingOffset + count));
        pendingOffset += count;
        return output;
    }

    void cancelStream() noexcept
    {
        queue.clear();
        pendingPcm.clear();
        pendingOffset = 0U;
        queuedGroups = 0U;
        queuedBytes = 0U;
        cancelled = true;
    }

    void resetStream()
    {
        queue.clear();
        pendingPcm.clear();
        pendingOffset = 0U;
        queuedGroups = 0U;
        queuedBytes = 0U;
        continuityIndex = 0U;
        physicalFrame = 0U;
        dataFrames = 0U;
        flushFrames = 0U;
        globalSample = 0U;
        finishing = false;
        cancelled = false;
        terminalError = false;
        interleaver.reset();
        statistics = {};
        last = HamDrmStatus::success();
    }

    bool isDone() const noexcept
    {
        const std::size_t requiredFlush = dataFrames == 0U ? 0U
            : profile.capacity.interleaverDepthFrames - 1U;
        return finishing && queue.empty() && flushFrames >= requiredFlush
            && pendingOffset == pendingPcm.size();
    }

    HamDrmStatus setStatus(const HamDrmStatus& status)
    {
        last = status;
        return last;
    }

    HamDrmStatus setFailure(HamDrmErrorCode code,
                            const std::string& detail)
    {
        return setStatus(failure(code, detail));
    }

    bool generateFrame()
    {
        const std::size_t requiredFlush = dataFrames == 0U ? 0U
            : profile.capacity.interleaverDepthFrames - 1U;
        const bool hasPacket = !queue.empty();
        const bool flushing = !hasPacket && finishing
            && flushFrames < requiredFlush;
        if (!hasPacket && !flushing) {
            return false;
        }
        if (statistics.transmittedFrames >= limits.maximumFrames) {
            ++statistics.resourceFailures;
            terminalError = true;
            setFailure(HamDrmErrorCode::LimitExceeded,
                       "HAMDRM transmit frame limit exceeded");
            return false;
        }

        std::vector<phy::HamDrmComplex> sourceCells(
            profile.capacity.usefulMscCellsPerFrame);
        if (hasPacket) {
            const auto bits = bytesToBits(queue.front().bytes,
                                          profile.mlc.inputBits);
            const auto encoded = channel::hamDrmEncodeMscCells(bits,
                                                                profile.mlc);
            if (!encoded.ok()) {
                terminalError = true;
                setStatus(encoded.status);
                return false;
            }
            sourceCells = *encoded.value;
        }
        std::vector<phy::HamDrmComplex> transmittedMsc;
        try {
            transmittedMsc = interleaver.push(sourceCells);
        } catch (const std::exception& error) {
            terminalError = true;
            setFailure(HamDrmErrorCode::Malformed, error.what());
            return false;
        }

        const std::size_t frameIdentity = physicalFrame
            % kFramesPerSuperframe;
        channel::HamDrmFacParameters facParameters;
        facParameters.frameIdentity = frameIdentity;
        facParameters.occupiedBandwidth = config.occupiedBandwidth;
        facParameters.interleaver = config.interleaver;
        facParameters.mscConstellation = config.constellation;
        facParameters.protection = config.protection;
        facParameters.packetId = config.packetId;
        facParameters.callsign = config.callsign;
        const auto fac = channel::hamDrmEncodeFacCells(facParameters);
        if (!fac.ok()) {
            terminalError = true;
            setStatus(fac.status);
            return false;
        }

        std::vector<double> frame;
        frame.reserve(profile.capacity.samplesPerFrame);
        std::size_t mscOffset = 0U;
        std::size_t facOffset = 0U;
        for (std::size_t symbol = 0U;
             symbol < profile.capacity.symbolsPerFrame; ++symbol) {
            const std::size_t absoluteSymbol = frameIdentity
                * profile.capacity.symbolsPerFrame + symbol;
            const std::size_t mscCount =
                profile.cellPlan.usefulMscCellsPerSymbol[absoluteSymbol];
            const std::size_t facCount =
                profile.cellPlan.facCellsPerSymbol[absoluteSymbol];
            const std::vector<phy::HamDrmComplex> symbolMsc(
                transmittedMsc.begin()
                    + static_cast<std::ptrdiff_t>(mscOffset),
                transmittedMsc.begin()
                    + static_cast<std::ptrdiff_t>(mscOffset + mscCount));
            const std::vector<phy::HamDrmComplex> symbolFac(
                fac.value->begin() + static_cast<std::ptrdiff_t>(facOffset),
                fac.value->begin()
                    + static_cast<std::ptrdiff_t>(facOffset + facCount));
            mscOffset += mscCount;
            facOffset += facCount;
            const auto mapped = channel::hamDrmMapCellSymbol(
                profile.cellPlan, absoluteSymbol, symbolMsc, symbolFac,
                config.constellation);
            if (!mapped.ok()) {
                terminalError = true;
                setStatus(mapped.status);
                return false;
            }
            appendOfdmSymbol(*mapped.value, frame);
        }
        if (mscOffset != transmittedMsc.size()
                || facOffset != fac.value->size()
                || frame.size() != profile.capacity.samplesPerFrame) {
            terminalError = true;
            setFailure(HamDrmErrorCode::Malformed,
                       "HAMDRM frame scheduler did not consume exact cells");
            return false;
        }
        pendingPcm = std::move(frame);
        pendingOffset = 0U;
        ++physicalFrame;
        ++statistics.transmittedFrames;
        if (hasPacket) {
            const QueuedPacket sent = std::move(queue.front());
            queue.pop_front();
            ++dataFrames;
            if (sent.lastInGroup) {
                --queuedGroups;
                queuedBytes -= sent.groupBytes;
            }
        } else {
            ++flushFrames;
        }
        last = HamDrmStatus::success();
        return true;
    }

    void appendOfdmSymbol(
        const std::vector<phy::HamDrmComplex>& carriers,
        std::vector<double>& frame)
    {
        std::vector<phy::HamDrmComplex> bins(
            profile.usefulSamples, phy::HamDrmComplex {});
        for (std::size_t index = 0U; index < carriers.size(); ++index) {
            const int carrier = profile.ofdm.minimumCarrier
                + static_cast<int>(index);
            bins[static_cast<std::size_t>(carrier)] = carriers[index];
        }
        auto useful = transform.execute(
            bins, phy::HamDrmTransformDirection::Inverse);
        for (auto& sample : useful) {
            sample *= static_cast<double>(profile.usefulSamples);
        }
        std::vector<phy::HamDrmComplex> withGuard;
        withGuard.reserve(profile.capacity.samplesPerSymbol);
        withGuard.insert(
            withGuard.end(),
            useful.end() - static_cast<std::ptrdiff_t>(profile.guardSamples),
            useful.end());
        withGuard.insert(withGuard.end(), useful.begin(), useful.end());
        const double increment = 2.0 * kPi * config.carrierOffsetHz
            / static_cast<double>(profile.pcmSampleRateHz);
        for (const auto& sample : withGuard) {
            const double phase = std::remainder(
                increment * static_cast<double>(globalSample), 2.0 * kPi);
            const phy::HamDrmComplex rotation {std::cos(phase),
                                               std::sin(phase)};
            const double value = (sample * rotation).real()
                * config.outputGain;
            frame.push_back(std::clamp(value, -1.0, 1.0));
            ++globalSample;
        }
    }

    HamDrmWaveformConfig config;
    HamDrmWaveformLimits limits;
    WaveformProfile profile;
    channel::HamDrmSymbolInterleaverEncoder interleaver;
    phy::HamDrmMixedRadixTransform transform;
    std::deque<QueuedPacket> queue;
    std::vector<double> pendingPcm;
    std::size_t pendingOffset {0U};
    std::size_t queuedGroups {0U};
    std::size_t queuedBytes {0U};
    std::uint8_t continuityIndex {0U};
    std::size_t physicalFrame {0U};
    std::size_t dataFrames {0U};
    std::size_t flushFrames {0U};
    std::size_t globalSample {0U};
    bool finishing {false};
    bool cancelled {false};
    bool terminalError {false};
    HamDrmStatus last;
    HamDrmWaveformStatistics statistics;
};

HamDrmWaveformTransmitter::HamDrmWaveformTransmitter(
    HamDrmWaveformConfig config,
    HamDrmWaveformLimits limits)
    : implementation_(std::make_unique<Implementation>(
          std::move(config), limits))
{
}

HamDrmWaveformTransmitter::~HamDrmWaveformTransmitter() = default;
HamDrmWaveformTransmitter::HamDrmWaveformTransmitter(
    HamDrmWaveformTransmitter&&) noexcept = default;
HamDrmWaveformTransmitter& HamDrmWaveformTransmitter::operator=(
    HamDrmWaveformTransmitter&&) noexcept = default;

HamDrmStatus HamDrmWaveformTransmitter::enqueueMotDataGroup(
    const std::vector<std::uint8_t>& encodedDataGroup)
{
    return implementation_->enqueue(encodedDataGroup);
}

HamDrmStatus HamDrmWaveformTransmitter::finish()
{
    return implementation_->finishStream();
}

std::vector<double> HamDrmWaveformTransmitter::pullPcm(
    std::size_t maximumSamples)
{
    return implementation_->pull(maximumSamples);
}

void HamDrmWaveformTransmitter::cancel() noexcept
{
    implementation_->cancelStream();
}

void HamDrmWaveformTransmitter::reset()
{
    implementation_->resetStream();
}

bool HamDrmWaveformTransmitter::done() const noexcept
{
    return implementation_->isDone();
}

bool HamDrmWaveformTransmitter::cancelled() const noexcept
{
    return implementation_->cancelled;
}

HamDrmStatus HamDrmWaveformTransmitter::lastStatus() const
{
    return implementation_->last;
}

HamDrmWaveformStatistics HamDrmWaveformTransmitter::statistics() const noexcept
{
    return implementation_->statistics;
}

class HamDrmWaveformReceiver::Implementation final
{
public:
    Implementation(HamDrmWaveformConfig configuration,
                   HamDrmWaveformLimits resourceLimits)
        : config(std::move(configuration)),
          limits(resourceLimits),
          profile(requireProfile(config, limits)),
          interleaver(profile.capacity.usefulMscCellsPerFrame,
                      profile.capacity.interleaverDepthFrames),
          packetReassembler({profile.capacity.packetBodyBytes,
                             config.packetId,
                             limits.maximumDataGroupBytes}),
          nominalDemodulator(profile, config.carrierOffsetHz)
    {
    }

    HamDrmWaveformReceiveBatch push(const double* samples,
                                    std::size_t sampleCount,
                                    bool end)
    {
        HamDrmWaveformReceiveBatch batch;
        batch.status = HamDrmStatus::success();
        batch.endOfStream = end;
        if (cancelled) {
            batch.status = failure(HamDrmErrorCode::Incomplete,
                                   "HAMDRM receiver is cancelled");
            batch.statistics = statistics;
            return batch;
        }
        if ((samples == nullptr && sampleCount != 0U)
                || (samples != nullptr
                    && !std::all_of(samples, samples + sampleCount, finite))) {
            batch.status = failure(HamDrmErrorCode::InvalidArgument,
                                   "invalid HAMDRM PCM input");
            batch.statistics = statistics;
            return batch;
        }
        if (sampleCount > limits.maximumBufferedPcmSamples
                || pcm.size() > limits.maximumBufferedPcmSamples
                    - sampleCount) {
            ++statistics.resourceFailures;
            batch.status = failure(HamDrmErrorCode::LimitExceeded,
                                   "HAMDRM PCM receive buffer limit exceeded");
            resetSignalPath();
            pcm.clear();
            batch.statistics = statistics;
            return batch;
        }
        if (sampleCount != 0U) {
            pcm.insert(pcm.end(), samples, samples + sampleCount);
        }

        bool progress = true;
        while (progress) {
            progress = false;
            if (!locked) {
                if (pcm.size() < profile.capacity.samplesPerFrame) {
                    break;
                }
                const std::size_t acquisitionSamples =
                    profile.capacity.samplesPerFrame
                    + limits.maximumSyncSearchSamples;
                if (!end && pcm.size() < acquisitionSamples) {
                    break;
                }
                const auto acquisition = acquire();
                if (!acquisition.ok()) {
                    if (acquisition.status.code
                            != HamDrmErrorCode::Incomplete) {
                        ++statistics.synchronizationFailures;
                        recordFirstFailure(batch.status,
                                           acquisition.status);
                    }
                    break;
                }
                frameStart = *acquisition.value;
                locked = true;
                progress = true;
            }
            if (locked && frameStart <= pcm.size()
                    && profile.capacity.samplesPerFrame
                        <= pcm.size() - frameStart) {
                if (statistics.receivedFrames >= limits.maximumFrames) {
                    ++statistics.resourceFailures;
                    recordFirstFailure(
                        batch.status,
                        failure(HamDrmErrorCode::LimitExceeded,
                                "HAMDRM receive frame limit exceeded"));
                    break;
                }
                const HamDrmStatus frameStatus = processFrame(
                    frameStart, batch.dataGroups);
                const std::size_t consumed = frameStart
                    + profile.capacity.samplesPerFrame;
                pcm.erase(pcm.begin(),
                          pcm.begin() + static_cast<std::ptrdiff_t>(consumed));
                frameStart = 0U;
                progress = true;
                if (!frameStatus.ok()) {
                    recordFirstFailure(batch.status, frameStatus);
                    locked = false;
                    resetDataPath();
                }
            }
        }
        if (end && !pcm.empty()) {
            recordFirstFailure(
                batch.status,
                failure(HamDrmErrorCode::Incomplete,
                        "HAMDRM PCM stream ended with a partial frame"));
        }
        batch.synchronized = locked;
        batch.statistics = statistics;
        return batch;
    }

    HamDrmValueResult<std::size_t> acquire()
    {
        if (pcm.size() < profile.capacity.samplesPerFrame) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::Incomplete,
                            "HAMDRM acquisition needs a complete frame")};
        }
        const std::size_t maximumCandidate = std::min(
            limits.maximumSyncSearchSamples,
            pcm.size() - profile.capacity.samplesPerFrame);
        std::vector<double> metrics(maximumCandidate + 1U, 0.0);
        double maximumMetric = 0.0;
        for (std::size_t candidate = 0U; candidate <= maximumCandidate;
             ++candidate) {
            metrics[candidate] = cyclicPrefixMetric(candidate);
            maximumMetric = std::max(maximumMetric, metrics[candidate]);
        }
        if (maximumMetric < limits.minimumCyclicPrefixMetric) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::Incomplete,
                            "HAMDRM cyclic-prefix synchronization not found")};
        }
        const double shortlistMetric = limits.minimumCyclicPrefixMetric;
        double bestObjective = std::numeric_limits<double>::infinity();
        std::size_t bestCandidate = 0U;
        bool found = false;
        for (std::size_t candidate = 0U; candidate <= maximumCandidate;
             ++candidate) {
            if (metrics[candidate] < shortlistMetric) {
                continue;
            }
            const double pilotScore = leastSquaresPilotScore(candidate);
            if (!finite(pilotScore)) {
                continue;
            }
            const double objective = pilotScore
                + 0.01 * (1.0 - metrics[candidate] / maximumMetric);
            if (objective < bestObjective) {
                bestObjective = objective;
                bestCandidate = candidate;
                found = true;
            }
        }
        if (!found) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::Incomplete,
                            "HAMDRM pilot-aided timing synchronization failed")};
        }
        statistics.lastCyclicPrefixMetric = metrics[bestCandidate];
        statistics.lastFrameStartSample = bestCandidate;
        const auto estimated = estimateFrequencyOffset(bestCandidate);
        if (!estimated.ok()) {
            return {std::nullopt, estimated.status};
        }
        statistics.estimatedFrequencyOffsetHz = *estimated.value;
        try {
            demodulator = std::make_unique<RealToneLeastSquares>(
                profile, config.carrierOffsetHz + *estimated.value);
        } catch (const std::exception& error) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::UnsupportedProfile,
                            error.what())};
        }
        return {bestCandidate, HamDrmStatus::success()};
    }

    double cyclicPrefixMetric(std::size_t candidate) const noexcept
    {
        double correlation = 0.0;
        double prefixEnergy = 0.0;
        double suffixEnergy = 0.0;
        for (std::size_t index = 0U; index < profile.guardSamples; ++index) {
            const double prefix = pcm[candidate + index];
            const double suffix = pcm[candidate + profile.usefulSamples
                                      + index];
            correlation += prefix * suffix;
            prefixEnergy += prefix * prefix;
            suffixEnergy += suffix * suffix;
        }
        const double denominator = prefixEnergy * suffixEnergy;
        return denominator > std::numeric_limits<double>::epsilon()
            ? correlation * correlation / denominator : 0.0;
    }

    double leastSquaresPilotScore(std::size_t candidate) const
    {
        const std::size_t usefulStart = candidate + profile.guardSamples;
        const auto carriers = nominalDemodulator.demodulate(
            pcm.data() + usefulStart);
        const auto* descriptors = profile.cellPlan.symbolCells(0U);
        if (descriptors == nullptr) {
            return std::numeric_limits<double>::infinity();
        }
        const auto gain = pilotGain(profile, 0U, carriers);
        double error = 0.0;
        double observedEnergy = 0.0;
        std::size_t count = 0U;
        for (std::size_t index = 0U; index < carriers.size(); ++index) {
            if (isPilot(descriptors[index].kind)) {
                error += std::norm(carriers[index]
                                   - gain * descriptors[index].pilotValue);
                observedEnergy += std::norm(carriers[index]);
                ++count;
            }
        }
        return count != 0U
                && observedEnergy > std::numeric_limits<double>::epsilon()
            ? error / observedEnergy
            : std::numeric_limits<double>::infinity();
    }

    HamDrmValueResult<double> estimateFrequencyOffset(
        std::size_t candidate) const
    {
        std::vector<double> times;
        std::vector<double> phases;
        times.reserve(profile.capacity.symbolsPerFrame);
        phases.reserve(profile.capacity.symbolsPerFrame);
        double previous = 0.0;
        bool havePrevious = false;
        for (std::size_t symbol = 0U;
             symbol < profile.capacity.symbolsPerFrame; ++symbol) {
            const std::size_t usefulStart = candidate
                + symbol * profile.capacity.samplesPerSymbol
                + profile.guardSamples;
            const auto carriers = nominalDemodulator.demodulate(
                pcm.data() + usefulStart);
            const auto gain = pilotGain(profile, symbol, carriers);
            if (std::abs(gain) <= 1.0e-12) {
                continue;
            }
            const double time = static_cast<double>(usefulStart)
                / static_cast<double>(profile.pcmSampleRateHz);
            const double knownPhase = 2.0 * kPi
                * config.carrierOffsetHz * time;
            double phase = std::arg(gain
                * phy::HamDrmComplex {std::cos(-knownPhase),
                                      std::sin(-knownPhase)});
            if (havePrevious) {
                while (phase - previous > kPi) {
                    phase -= 2.0 * kPi;
                }
                while (phase - previous < -kPi) {
                    phase += 2.0 * kPi;
                }
            }
            previous = phase;
            havePrevious = true;
            times.push_back(time);
            phases.push_back(phase);
        }
        if (times.size() < 3U) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::Incomplete,
                            "HAMDRM frequency estimate has too few pilots")};
        }
        double meanTime = 0.0;
        double meanPhase = 0.0;
        for (std::size_t index = 0U; index < times.size(); ++index) {
            meanTime += times[index];
            meanPhase += phases[index];
        }
        meanTime /= static_cast<double>(times.size());
        meanPhase /= static_cast<double>(phases.size());
        double numerator = 0.0;
        double denominator = 0.0;
        for (std::size_t index = 0U; index < times.size(); ++index) {
            numerator += (times[index] - meanTime)
                * (phases[index] - meanPhase);
            denominator += (times[index] - meanTime)
                * (times[index] - meanTime);
        }
        if (denominator <= std::numeric_limits<double>::epsilon()) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::Incomplete,
                            "HAMDRM frequency estimate is singular")};
        }
        const double offset = numerator / denominator / (2.0 * kPi);
        if (!finite(offset)
                || std::abs(offset) > limits.maximumFrequencyOffsetHz) {
            return {std::nullopt,
                    failure(HamDrmErrorCode::UnsupportedProfile,
                            "HAMDRM frequency offset is outside configured acquisition range")};
        }
        return {offset, HamDrmStatus::success()};
    }

    HamDrmStatus processFrame(
        std::size_t start,
        std::vector<std::vector<std::uint8_t>>& outputGroups)
    {
        if (!demodulator) {
            return failure(HamDrmErrorCode::Incomplete,
                           "HAMDRM demodulator has no frequency lock");
        }
        std::vector<phy::HamDrmComplex> mscCells;
        std::vector<phy::HamDrmComplex> facCells;
        mscCells.reserve(profile.capacity.usefulMscCellsPerFrame);
        facCells.reserve(channel::kHamDrmFacCells);
        double pilotErrorSum = 0.0;
        std::size_t pilotCount = 0U;
        std::size_t corruptedPilots = 0U;
        for (std::size_t symbol = 0U;
             symbol < profile.capacity.symbolsPerFrame; ++symbol) {
            const std::size_t usefulStart = start
                + symbol * profile.capacity.samplesPerSymbol
                + profile.guardSamples;
            const auto raw = demodulator->demodulate(pcm.data()
                                                      + usefulStart);
            const auto equalized = channel::hamDrmEqualizeFlatPilotChannel(
                profile.cellPlan, symbol, raw,
                limits.corruptedPilotSquaredErrorThreshold);
            if (!equalized.ok()) {
                ++statistics.pilotFailures;
                return equalized.status;
            }
            pilotErrorSum += equalized.value->normalizedPilotSquaredError
                * static_cast<double>(equalized.value->pilotCells);
            pilotCount += equalized.value->pilotCells;
            corruptedPilots += equalized.value->corruptedPilotCells;
            const auto extracted = channel::hamDrmExtractCellSymbol(
                profile.cellPlan, symbol, equalized.value->carriers,
                limits.corruptedPilotSquaredErrorThreshold);
            if (!extracted.ok()) {
                ++statistics.pilotFailures;
                return extracted.status;
            }
            mscCells.insert(mscCells.end(),
                            extracted.value->mscCells.begin(),
                            extracted.value->mscCells.end());
            facCells.insert(facCells.end(),
                            extracted.value->facCells.begin(),
                            extracted.value->facCells.end());
        }
        statistics.lastNormalizedPilotSquaredError = pilotCount == 0U
            ? std::numeric_limits<double>::infinity()
            : pilotErrorSum / static_cast<double>(pilotCount);
        const double corruptedFraction = pilotCount == 0U ? 1.0
            : static_cast<double>(corruptedPilots)
                / static_cast<double>(pilotCount);
        if (statistics.lastNormalizedPilotSquaredError
                > limits.maximumNormalizedPilotSquaredError
                || corruptedFraction > limits.maximumCorruptedPilotFraction) {
            ++statistics.pilotFailures;
            return failure(HamDrmErrorCode::Malformed,
                           "HAMDRM pilot validation rejected the frame");
        }
        if (mscCells.size() != profile.capacity.usefulMscCellsPerFrame
                || facCells.size() != channel::kHamDrmFacCells) {
            ++statistics.pilotFailures;
            return failure(HamDrmErrorCode::Malformed,
                           "HAMDRM extracted frame has the wrong cell count");
        }

        const auto fac = channel::hamDrmDecodeFacCells(facCells);
        if (!fac.ok() || !facMatchesConfig(*fac.value, config)) {
            ++statistics.facFailures;
            return fac.ok()
                ? failure(HamDrmErrorCode::UnsupportedProfile,
                          "HAMDRM FAC profile does not match configured receiver")
                : fac.status;
        }
        const std::size_t identity = fac.value->parameters.frameIdentity;
        if (haveFrameIdentity
                && identity != (lastFrameIdentity + 1U)
                                % kFramesPerSuperframe) {
            ++statistics.facFailures;
            return failure(HamDrmErrorCode::Incomplete,
                           "HAMDRM FAC frame identity sequence is broken");
        }
        haveFrameIdentity = true;
        lastFrameIdentity = identity;

        std::optional<std::vector<phy::HamDrmComplex>> logicalMsc;
        try {
            logicalMsc = interleaver.push(mscCells);
        } catch (const std::exception& error) {
            ++statistics.mscFailures;
            return failure(HamDrmErrorCode::Malformed, error.what());
        }
        ++statistics.receivedFrames;
        if (!logicalMsc.has_value()) {
            return HamDrmStatus::success();
        }
        const auto decoded = channel::hamDrmDecodeMscCells(*logicalMsc,
                                                            profile.mlc);
        if (!decoded.ok()) {
            ++statistics.mscFailures;
            return decoded.status;
        }
        const std::size_t packetBytes = profile.capacity.packetBodyBytes
            + kHamDrmPacketOverheadBytes;
        const std::size_t packetBits = packetBytes * 8U;
        if (decoded.value->bits.size() < packetBits
                || std::any_of(
                    decoded.value->bits.begin()
                        + static_cast<std::ptrdiff_t>(packetBits),
                    decoded.value->bits.end(),
                    [](std::uint8_t bit) { return bit != 0U; })) {
            ++statistics.mscFailures;
            return failure(HamDrmErrorCode::Malformed,
                           "HAMDRM MSC byte-alignment fill bits are non-zero");
        }
        const auto packet = bitsToBytes(decoded.value->bits, packetBytes);
        const auto reassembled = packetReassembler.push(packet);
        if (!reassembled.ok()) {
            ++statistics.packetCrcFailures;
            return reassembled.status;
        }
        if (!reassembled.value->dataUnit.has_value()) {
            return HamDrmStatus::success();
        }
        const auto& group = *reassembled.value->dataUnit;
        const auto parsed = parseHamDrmMotDataGroup(group.data(),
                                                     group.size());
        if (!parsed.ok()) {
            ++statistics.motCrcFailures;
            return parsed.status;
        }
        outputGroups.push_back(group);
        ++statistics.validatedDataGroups;
        return HamDrmStatus::success();
    }

    void resetDataPath() noexcept
    {
        interleaver.reset();
        packetReassembler.reset();
        haveFrameIdentity = false;
        lastFrameIdentity = 0U;
    }

    void resetSignalPath() noexcept
    {
        locked = false;
        frameStart = 0U;
        demodulator.reset();
        resetDataPath();
    }

    void cancelStream() noexcept
    {
        pcm.clear();
        resetSignalPath();
        cancelled = true;
    }

    void resetStream()
    {
        pcm.clear();
        resetSignalPath();
        cancelled = false;
        statistics = {};
    }

    HamDrmWaveformConfig config;
    HamDrmWaveformLimits limits;
    WaveformProfile profile;
    channel::HamDrmSymbolInterleaverDecoder interleaver;
    HamDrmPacketReassembler packetReassembler;
    RealToneLeastSquares nominalDemodulator;
    std::unique_ptr<RealToneLeastSquares> demodulator;
    std::vector<double> pcm;
    std::size_t frameStart {0U};
    std::size_t lastFrameIdentity {0U};
    bool haveFrameIdentity {false};
    bool locked {false};
    bool cancelled {false};
    HamDrmWaveformStatistics statistics;
};

HamDrmWaveformReceiver::HamDrmWaveformReceiver(
    HamDrmWaveformConfig config,
    HamDrmWaveformLimits limits)
    : implementation_(std::make_unique<Implementation>(
          std::move(config), limits))
{
}

HamDrmWaveformReceiver::~HamDrmWaveformReceiver() = default;
HamDrmWaveformReceiver::HamDrmWaveformReceiver(
    HamDrmWaveformReceiver&&) noexcept = default;
HamDrmWaveformReceiver& HamDrmWaveformReceiver::operator=(
    HamDrmWaveformReceiver&&) noexcept = default;

HamDrmWaveformReceiveBatch HamDrmWaveformReceiver::pushPcm(
    const double* samples,
    std::size_t sampleCount,
    bool endOfStream)
{
    return implementation_->push(samples, sampleCount, endOfStream);
}

HamDrmWaveformReceiveBatch HamDrmWaveformReceiver::pushPcm(
    const std::vector<double>& samples,
    bool endOfStream)
{
    return pushPcm(samples.data(), samples.size(), endOfStream);
}

void HamDrmWaveformReceiver::cancel() noexcept
{
    implementation_->cancelStream();
}

void HamDrmWaveformReceiver::reset()
{
    implementation_->resetStream();
}

bool HamDrmWaveformReceiver::cancelled() const noexcept
{
    return implementation_->cancelled;
}

HamDrmWaveformStatistics HamDrmWaveformReceiver::statistics() const noexcept
{
    return implementation_->statistics;
}

} // namespace decodium::sstv::hamdrm::waveform
