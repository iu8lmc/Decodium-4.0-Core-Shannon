// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/digital/phy/HamDrmComplexTransform.h"
#include "../../src/sstv/digital/phy/HamDrmOfdmModem.h"
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
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::phy;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

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

void requireNear(double actual,
                 double expected,
                 double tolerance,
                 const std::string& message)
{
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        throw TestFailure(message + ": actual=" + std::to_string(actual)
                          + " expected=" + std::to_string(expected));
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
    bool threwExpected = false;
    try {
        std::forward<Callable>(callable)();
    } catch (const Exception&) {
        threwExpected = true;
    }
    require(threwExpected, message);
}

std::vector<std::uint8_t> patternBits(std::size_t count,
                                      std::uint32_t seed)
{
    std::vector<std::uint8_t> bits(count);
    std::uint32_t state = seed;
    for (auto& bit : bits) {
        state = state * 1'664'525U + 1'013'904'223U;
        bit = static_cast<std::uint8_t>((state >> 31U) & 1U);
    }
    return bits;
}

std::vector<HamDrmComplex> directDft(
    const std::vector<HamDrmComplex>& input,
    HamDrmTransformDirection direction)
{
    const double sign = direction == HamDrmTransformDirection::Forward
        ? -1.0 : 1.0;
    std::vector<HamDrmComplex> output(input.size(), HamDrmComplex {});
    for (std::size_t frequency = 0U; frequency < input.size(); ++frequency) {
        for (std::size_t time = 0U; time < input.size(); ++time) {
            const double phase = sign * 2.0 * kPi
                * static_cast<double>(frequency * time)
                / static_cast<double>(input.size());
            output[frequency] += input[time]
                * HamDrmComplex {std::cos(phase), std::sin(phase)};
        }
        if (direction == HamDrmTransformDirection::Inverse) {
            output[frequency] /= static_cast<double>(input.size());
        }
    }
    return output;
}

std::vector<HamDrmComplex> deterministicComplexInput(std::size_t count)
{
    std::vector<HamDrmComplex> values;
    values.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const double real = std::sin(static_cast<double>(index) * 0.37)
            + static_cast<double>(index % 7U) * 0.01;
        const double imaginary = std::cos(static_cast<double>(index) * 0.23)
            - static_cast<double>(index % 5U) * 0.02;
        values.emplace_back(real, imaginary);
    }
    return values;
}

class CountingTransform final : public HamDrmComplexTransform
{
public:
    std::size_t maximumSize() const noexcept override
    {
        return delegate_.maximumSize();
    }

    std::vector<HamDrmComplex> execute(
        const std::vector<HamDrmComplex>& input,
        HamDrmTransformDirection direction) const override
    {
        ++calls;
        return delegate_.execute(input, direction);
    }

    mutable std::size_t calls {0U};

private:
    HamDrmMixedRadixTransform delegate_;
};

void testParameterMatrix()
{
    struct Expected final
    {
        HamDrmRobustness robustness;
        HamDrmOccupiedBandwidth bandwidth;
        std::size_t fftSize;
        std::size_t guard;
        std::size_t symbols;
        int minimumCarrier;
        int maximumCarrier;
    };

    const std::array<Expected, 6U> expected {{
        {HamDrmRobustness::A, HamDrmOccupiedBandwidth::Hz2300,
         1'152U, 128U, 15U, 2, 54},
        {HamDrmRobustness::A, HamDrmOccupiedBandwidth::Hz2500,
         1'152U, 128U, 15U, 2, 58},
        {HamDrmRobustness::B, HamDrmOccupiedBandwidth::Hz2300,
         1'024U, 256U, 15U, 1, 45},
        {HamDrmRobustness::B, HamDrmOccupiedBandwidth::Hz2500,
         1'024U, 256U, 15U, 1, 51},
        {HamDrmRobustness::E, HamDrmOccupiedBandwidth::Hz2300,
         640U, 320U, 20U, 1, 29},
        {HamDrmRobustness::E, HamDrmOccupiedBandwidth::Hz2500,
         640U, 320U, 20U, 1, 31}
    }};

    for (const auto& row : expected) {
        const auto parameters = hamDrmOfdmParameters(row.robustness,
                                                      row.bandwidth);
        require(parameters.has_value(), "missing HAMDRM OFDM table row");
        require(parameters->isValid(), "HAMDRM OFDM table row is invalid");
        require(parameters->sampleRateHz == 48'000U,
                "unexpected HAMDRM sample rate");
        require(parameters->fftSize == row.fftSize,
                "unexpected HAMDRM FFT size");
        require(parameters->guardIntervalSamples == row.guard,
                "unexpected HAMDRM guard size");
        require(parameters->symbolsPerFrame == row.symbols,
                "unexpected HAMDRM frame symbol count");
        require(parameters->minimumCarrier == row.minimumCarrier,
                "unexpected HAMDRM minimum carrier");
        require(parameters->maximumCarrier == row.maximumCarrier,
                "unexpected HAMDRM maximum carrier");
        require(parameters->carrierCount()
                    == static_cast<std::size_t>(row.maximumCarrier
                                                - row.minimumCarrier + 1),
                "unexpected HAMDRM carrier count");
        require(parameters->symbolSamples() == row.fftSize + row.guard,
                "unexpected HAMDRM symbol size");
        require(parameters->nominalBandwidthHz()
                    == (row.bandwidth == HamDrmOccupiedBandwidth::Hz2300
                            ? 2'300U : 2'500U),
                "unexpected HAMDRM nominal bandwidth");
    }

    requireNear(hamDrmOfdmParameters(HamDrmRobustness::A,
                                     HamDrmOccupiedBandwidth::Hz2300)
                    ->carrierSpacingHz(),
                125.0 / 3.0, 1.0e-12,
                "mode A carrier spacing mismatch");
    requireNear(hamDrmOfdmParameters(HamDrmRobustness::B,
                                     HamDrmOccupiedBandwidth::Hz2300)
                    ->carrierSpacingHz(),
                46.875, 1.0e-12,
                "mode B carrier spacing mismatch");
    requireNear(hamDrmOfdmParameters(HamDrmRobustness::E,
                                     HamDrmOccupiedBandwidth::Hz2300)
                    ->carrierSpacingHz(),
                75.0, 1.0e-12,
                "mode E carrier spacing mismatch");

    require(!hamDrmOfdmParameters(static_cast<HamDrmRobustness>(255U),
                                  HamDrmOccupiedBandwidth::Hz2300)
                 .has_value(),
            "unknown robustness mode was accepted");
    require(!hamDrmOfdmParameters(HamDrmRobustness::A,
                                  static_cast<HamDrmOccupiedBandwidth>(255U))
                 .has_value(),
            "unknown occupied bandwidth was accepted");
}

void testQamMappings()
{
    const std::array<HamDrmConstellation, 3U> constellations {{
        HamDrmConstellation::Qam4,
        HamDrmConstellation::Qam16,
        HamDrmConstellation::Qam64
    }};
    const std::array<double, 3U> maximumLevels {{
        1.0 / std::sqrt(2.0),
        3.0 / std::sqrt(10.0),
        7.0 / std::sqrt(42.0)
    }};

    for (std::size_t constellationIndex = 0U;
         constellationIndex < constellations.size(); ++constellationIndex) {
        const auto constellation = constellations[constellationIndex];
        const std::size_t bitCount = hamDrmBitsPerCell(constellation);
        const std::size_t pointCount = 1U << bitCount;
        double averageEnergy = 0.0;
        std::vector<HamDrmComplex> points;
        points.reserve(pointCount);
        for (std::size_t point = 0U; point < pointCount; ++point) {
            std::array<std::uint8_t, 6U> bits {};
            for (std::size_t position = 0U; position < bitCount; ++position) {
                const std::size_t shift = bitCount - position - 1U;
                bits[position] = static_cast<std::uint8_t>(
                    (point >> shift) & 1U);
            }
            const auto mapped = hamDrmMapQamCell(bits.data(), bitCount,
                                                 constellation);
            averageEnergy += std::norm(mapped);
            points.push_back(mapped);
            const auto decision = hamDrmDemapQamCell(mapped, constellation);
            require(decision.bitCount == bitCount,
                    "QAM demapper bit count mismatch");
            require(std::equal(bits.begin(),
                               bits.begin()
                                   + static_cast<std::ptrdiff_t>(bitCount),
                               decision.bits.begin()),
                    "QAM hard decision failed on exact point");
            requireNear(decision.squaredError, 0.0, 1.0e-24,
                        "QAM exact-point error is nonzero");
        }
        averageEnergy /= static_cast<double>(pointCount);
        requireNear(averageEnergy, 1.0, 1.0e-12,
                    "QAM average energy is not normalized");
        std::sort(points.begin(), points.end(),
                  [](HamDrmComplex left, HamDrmComplex right) {
                      if (left.real() != right.real()) {
                          return left.real() < right.real();
                      }
                      return left.imag() < right.imag();
                  });
        require(std::adjacent_find(points.begin(), points.end()) == points.end(),
                "QAM constellation contains duplicate points");

        const std::array<std::uint8_t, 6U> zeroBits {};
        const auto zeroPoint = hamDrmMapQamCell(zeroBits.data(), bitCount,
                                                constellation);
        requireNear(zeroPoint.real(), maximumLevels[constellationIndex],
                    1.0e-12, "QAM zero-code in-phase level mismatch");
        requireNear(zeroPoint.imag(), maximumLevels[constellationIndex],
                    1.0e-12, "QAM zero-code quadrature level mismatch");

        const auto batchBits = patternBits(bitCount * 97U,
                                           0x51A7U
                                               + static_cast<std::uint32_t>(
                                                   constellationIndex));
        const auto batchCells = hamDrmMapQamBits(batchBits, constellation);
        require(hamDrmDemapQamCells(batchCells, constellation) == batchBits,
                "QAM batch round trip failed");
    }

    const std::vector<std::uint8_t> invalidBit {2U, 0U};
    requireThrows<std::invalid_argument>(
        [&invalidBit]() {
            static_cast<void>(hamDrmMapQamBits(
                invalidBit, HamDrmConstellation::Qam4));
        },
        "non-binary QAM input was accepted");
    requireThrows<std::invalid_argument>(
        []() {
            const std::vector<std::uint8_t> incomplete {0U, 1U, 0U};
            static_cast<void>(hamDrmMapQamBits(
                incomplete, HamDrmConstellation::Qam4));
        },
        "partial QAM cell was accepted");
    requireThrows<std::length_error>(
        []() {
            const std::vector<std::uint8_t> bits(6U, 0U);
            static_cast<void>(hamDrmMapQamBits(
                bits, HamDrmConstellation::Qam4, 2U));
        },
        "QAM cell bound was not enforced");
    requireThrows<std::invalid_argument>(
        []() {
            static_cast<void>(hamDrmDemapQamCell(
                {std::numeric_limits<double>::infinity(), 0.0},
                HamDrmConstellation::Qam4));
        },
        "non-finite QAM cell was accepted");
}

void testMixedRadixTransform()
{
    HamDrmMixedRadixTransform transform;
    for (const std::size_t size : {15U, 16U, 17U}) {
        const auto input = deterministicComplexInput(size);
        const auto expected = directDft(input,
                                        HamDrmTransformDirection::Forward);
        const auto actual = transform.execute(
            input, HamDrmTransformDirection::Forward);
        for (std::size_t index = 0U; index < size; ++index) {
            requireNear(actual[index], expected[index], 1.0e-10,
                        "mixed-radix FFT differs from direct DFT");
        }
    }

    for (const std::size_t size : {640U, 1'024U, 1'152U}) {
        const auto input = deterministicComplexInput(size);
        const auto spectrum = transform.execute(
            input, HamDrmTransformDirection::Forward);
        const auto recovered = transform.execute(
            spectrum, HamDrmTransformDirection::Inverse);
        for (std::size_t index = 0U; index < size; ++index) {
            requireNear(recovered[index], input[index], 2.0e-11,
                        "HAMDRM production-size FFT round trip failed");
        }
    }

    requireThrows<std::invalid_argument>(
        [&transform]() {
            static_cast<void>(transform.execute(
                {}, HamDrmTransformDirection::Forward));
        },
        "empty FFT input was accepted");
    const HamDrmMixedRadixTransform smallTransform(64U);
    requireThrows<std::length_error>(
        [&smallTransform]() {
            static_cast<void>(smallTransform.execute(
                std::vector<HamDrmComplex>(65U),
                HamDrmTransformDirection::Forward));
        },
        "FFT maximum size was not enforced");
}

void testOfdmCyclicPrefixAndInjection()
{
    const auto parameters = hamDrmOfdmParameters(
        HamDrmRobustness::B, HamDrmOccupiedBandwidth::Hz2500);
    require(parameters.has_value(), "missing OFDM test parameters");
    auto countingTransform = std::make_shared<CountingTransform>();
    HamDrmOfdmModem modem(*parameters, countingTransform);

    const auto bits = patternBits(parameters->carrierCount() * 4U,
                                  0x00C0FFEEU);
    const auto cells = hamDrmMapQamBits(bits,
                                       HamDrmConstellation::Qam16);
    const auto waveform = modem.modulateSymbol(cells);
    require(waveform.size() == parameters->symbolSamples(),
            "OFDM symbol size mismatch");
    for (std::size_t index = 0U;
         index < parameters->guardIntervalSamples; ++index) {
        requireNear(waveform[index],
                    waveform[parameters->fftSize + index],
                    1.0e-14,
                    "OFDM cyclic prefix mismatch");
    }

    const auto recovered = modem.demodulateSymbol(waveform, 0U);
    require(recovered.size() == cells.size(),
            "OFDM recovered carrier count mismatch");
    for (std::size_t index = 0U; index < cells.size(); ++index) {
        requireNear(recovered[index], cells[index], 2.0e-11,
                    "OFDM clean carrier round trip failed");
    }
    require(hamDrmDemapQamCells(recovered,
                                HamDrmConstellation::Qam16) == bits,
            "OFDM clean QAM bit round trip failed");
    require(countingTransform->calls == 2U,
            "injected OFDM FFT backend was not used");

    requireThrows<std::invalid_argument>(
        [&modem, &cells]() {
            auto shortCells = cells;
            shortCells.pop_back();
            static_cast<void>(modem.modulateSymbol(shortCells));
        },
        "OFDM carrier count mismatch was accepted");
    requireThrows<std::out_of_range>(
        [&modem, &waveform]() {
            static_cast<void>(modem.demodulateSymbol(waveform, 1U));
        },
        "truncated OFDM symbol was accepted");
}

void addDeterministicNoise(std::vector<HamDrmComplex>& waveform,
                           double standardDeviation,
                           std::uint32_t seed)
{
    std::mt19937 generator(seed);
    std::normal_distribution<double> distribution(0.0,
                                                   standardDeviation);
    for (auto& sample : waveform) {
        sample += HamDrmComplex {distribution(generator),
                                 distribution(generator)};
    }
}

void testFrequencyMixContinuity()
{
    const auto input = deterministicComplexInput(257U);
    const auto whole = hamDrmMixFrequency(input, 48'000.0, 13.25, 0.37);

    const std::size_t split = 113U;
    const std::vector<HamDrmComplex> firstInput(input.begin(),
                                                input.begin()
                                                    + static_cast<std::ptrdiff_t>(
                                                        split));
    const std::vector<HamDrmComplex> secondInput(
        input.begin() + static_cast<std::ptrdiff_t>(split), input.end());
    const auto first = hamDrmMixFrequency(firstInput, 48'000.0, 13.25, 0.37);
    const auto second = hamDrmMixFrequency(secondInput, 48'000.0, 13.25,
                                           first.finalPhaseRadians);
    for (std::size_t index = 0U; index < split; ++index) {
        requireNear(first.samples[index], whole.samples[index], 1.0e-13,
                    "frequency mixer first chunk mismatch");
    }
    for (std::size_t index = split; index < input.size(); ++index) {
        requireNear(second.samples[index - split], whole.samples[index],
                    1.0e-12,
                    "frequency mixer phase continuity failed");
    }
    requireNear(second.finalPhaseRadians, whole.finalPhaseRadians, 1.0e-12,
                "frequency mixer final phase mismatch");

    requireThrows<std::length_error>(
        [&input]() {
            static_cast<void>(hamDrmMixFrequency(input, 48'000.0, 0.0,
                                                 0.0, input.size() - 1U));
        },
        "frequency mixer sample bound was not enforced");
}

void testOfdmRoundTripWithOffsetAndNoise()
{
    const std::array<HamDrmRobustness, 3U> modes {{
        HamDrmRobustness::A,
        HamDrmRobustness::B,
        HamDrmRobustness::E
    }};
    const std::array<HamDrmOccupiedBandwidth, 2U> bandwidths {{
        HamDrmOccupiedBandwidth::Hz2300,
        HamDrmOccupiedBandwidth::Hz2500
    }};
    const std::array<HamDrmConstellation, 3U> constellations {{
        HamDrmConstellation::Qam4,
        HamDrmConstellation::Qam16,
        HamDrmConstellation::Qam64
    }};

    std::size_t testIndex = 0U;
    for (const auto mode : modes) {
        for (const auto bandwidth : bandwidths) {
            const auto parameters = hamDrmOfdmParameters(mode, bandwidth);
            require(parameters.has_value(), "missing noisy OFDM parameters");
            HamDrmOfdmModem modem(*parameters);
            const auto constellation = constellations[testIndex
                % constellations.size()];
            const std::size_t bitCount = parameters->carrierCount()
                * hamDrmBitsPerCell(constellation);
            const auto bits = patternBits(
                bitCount, 0xA17E0000U + static_cast<std::uint32_t>(testIndex));
            const auto cells = hamDrmMapQamBits(bits, constellation);
            const auto symbol = modem.modulateSymbol(cells);

            const std::size_t leadingSamples = 37U + testIndex;
            std::vector<HamDrmComplex> waveform(
                leadingSamples, HamDrmComplex {});
            waveform.insert(waveform.end(), symbol.begin(), symbol.end());

            const double frequencyOffsetHz = (testIndex & 1U) == 0U
                ? 7.25 : -9.5;
            waveform = hamDrmMixFrequency(
                waveform,
                static_cast<double>(parameters->sampleRateHz),
                frequencyOffsetHz).samples;
            addDeterministicNoise(
                waveform, 2.0e-5,
                0x600D0000U + static_cast<std::uint32_t>(testIndex));

            const auto sync = modem.synchronize(
                waveform, 0U, leadingSamples + 1U, 0.80);
            require(sync.locked, "OFDM synchronizer did not lock");
            require(sync.symbolStartSample == leadingSamples,
                    "OFDM synchronizer chose the wrong boundary");
            require(sync.normalizedCorrelation > 0.995,
                    "OFDM CP correlation is unexpectedly weak");
            requireNear(sync.coarseFrequencyOffsetHz, frequencyOffsetHz,
                        0.06,
                        "OFDM coarse frequency estimate is inaccurate");

            const auto recovered = modem.demodulateSymbol(
                waveform, sync.symbolStartSample,
                sync.coarseFrequencyOffsetHz);
            const auto recoveredBits = hamDrmDemapQamCells(recovered,
                                                           constellation);
            require(recoveredBits == bits,
                    "noisy/CFO OFDM QAM round trip failed");
            ++testIndex;
        }
    }
}

void testCorruptedCarrierIsolation()
{
    const auto parameters = hamDrmOfdmParameters(
        HamDrmRobustness::B, HamDrmOccupiedBandwidth::Hz2500);
    require(parameters.has_value(), "missing corruption test parameters");
    HamDrmOfdmModem modem(*parameters);
    const auto bits = patternBits(parameters->carrierCount() * 6U,
                                  0xBAD5EEDU);
    const auto cells = hamDrmMapQamBits(bits,
                                       HamDrmConstellation::Qam64);

    constexpr std::size_t corruptedCarrier = 17U;
    std::array<std::uint8_t, 6U> alternateBits {};
    const std::size_t bitOffset = corruptedCarrier * alternateBits.size();
    for (std::size_t bit = 0U; bit < alternateBits.size(); ++bit) {
        alternateBits[bit] = static_cast<std::uint8_t>(
            bits[bitOffset + bit] ^ 1U);
    }
    const auto alternateCell = hamDrmMapQamCell(
        alternateBits.data(), alternateBits.size(),
        HamDrmConstellation::Qam64);

    std::vector<HamDrmComplex> interferenceCells(cells.size(),
                                                  HamDrmComplex {});
    interferenceCells[corruptedCarrier] = alternateCell
        - cells[corruptedCarrier];
    auto waveform = modem.modulateSymbol(cells);
    const auto narrowbandInterference = modem.modulateSymbol(
        interferenceCells);
    for (std::size_t index = 0U; index < waveform.size(); ++index) {
        waveform[index] += narrowbandInterference[index];
    }

    const auto recovered = modem.demodulateSymbol(waveform, 0U);
    for (std::size_t carrier = 0U; carrier < cells.size(); ++carrier) {
        const auto expected = carrier == corruptedCarrier
            ? alternateCell : cells[carrier];
        requireNear(recovered[carrier], expected, 3.0e-11,
                    "narrowband corruption leaked between OFDM carriers");
    }

    const auto recoveredBits = hamDrmDemapQamCells(
        recovered, HamDrmConstellation::Qam64);
    for (std::size_t bit = 0U; bit < recoveredBits.size(); ++bit) {
        const bool insideCorruptedCell = bit >= bitOffset
            && bit < bitOffset + alternateBits.size();
        const auto expected = insideCorruptedCell
            ? alternateBits[bit - bitOffset] : bits[bit];
        require(recoveredBits[bit] == expected,
                "corrupted-carrier bit isolation failed");
    }
}

void testSynchronizationAndInputRejection()
{
    const auto parameters = hamDrmOfdmParameters(
        HamDrmRobustness::A, HamDrmOccupiedBandwidth::Hz2300);
    require(parameters.has_value(), "missing rejection test parameters");
    HamDrmOfdmModem modem(*parameters);

    const std::vector<HamDrmComplex> zeroWaveform(
        parameters->symbolSamples() + 8U, HamDrmComplex {});
    const auto weakSync = modem.synchronize(zeroWaveform, 0U, 9U, 0.50);
    require(!weakSync.locked, "synchronizer locked on a zero waveform");
    requireNear(weakSync.normalizedCorrelation, 0.0, 0.0,
                "zero-waveform synchronization metric is nonzero");

    requireThrows<std::invalid_argument>(
        [&modem, &zeroWaveform]() {
            static_cast<void>(modem.synchronize(zeroWaveform, 0U, 1U,
                                                1.01));
        },
        "invalid synchronization threshold was accepted");

    auto invalidParameters = *parameters;
    invalidParameters.sampleRateHz = 44'100U;
    requireThrows<std::invalid_argument>(
        [&invalidParameters]() {
            static_cast<void>(HamDrmOfdmModem(invalidParameters));
        },
        "non-native OFDM parameter set was accepted");

    auto undersizedTransform = std::make_shared<HamDrmMixedRadixTransform>(
        512U);
    requireThrows<std::invalid_argument>(
        [&parameters, &undersizedTransform]() {
            static_cast<void>(HamDrmOfdmModem(*parameters,
                                              undersizedTransform));
        },
        "undersized injected FFT backend was accepted");
}

struct NamedTest final
{
    const char* name;
    void (*function)();
};

} // namespace

int main()
{
    const std::array<NamedTest, 8U> tests {{
        {"parameter matrix", &testParameterMatrix},
        {"QAM mappings", &testQamMappings},
        {"mixed-radix transform", &testMixedRadixTransform},
        {"OFDM cyclic prefix and injection", &testOfdmCyclicPrefixAndInjection},
        {"frequency mixer continuity", &testFrequencyMixContinuity},
        {"OFDM offset/noise round trip", &testOfdmRoundTripWithOffsetAndNoise},
        {"corrupted carrier isolation", &testCorruptedCarrierIsolation},
        {"synchronization and rejection", &testSynchronizationAndInputRejection}
    }};

    std::size_t passed = 0U;
    for (const auto& test : tests) {
        try {
            test.function();
            ++passed;
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            std::cerr << "FAIL: " << test.name << ": " << error.what()
                      << '\n';
            return 1;
        }
    }
    std::cout << "HAMDRM PHY tests passed: " << passed << '/' << tests.size()
              << '\n';
    return 0;
}
