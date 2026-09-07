// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/dsp/SstvResampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

std::vector<float> tone(std::uint32_t sampleRate,
                        double frequency,
                        std::size_t sampleCount)
{
    std::vector<float> result(sampleCount);
    for (std::size_t index = 0U; index < sampleCount; ++index) {
        result[index] = static_cast<float>(std::sin(
            2.0 * kPi * frequency * static_cast<double>(index)
            / static_cast<double>(sampleRate)));
    }
    return result;
}

void append(std::vector<float>& destination, std::vector<float> source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}

std::vector<float> convert(std::uint32_t sampleRate,
                           const std::vector<float>& input,
                           const std::vector<std::size_t>& chunks)
{
    SstvResampler resampler(sampleRate);
    std::vector<float> output;
    std::size_t offset = 0U;
    std::size_t chunkIndex = 0U;
    while (offset < input.size()) {
        const std::size_t requested = chunks.empty()
            ? input.size()
            : chunks[chunkIndex++ % chunks.size()];
        const std::size_t count = std::min(requested, input.size() - offset);
        append(output, resampler.process(input.data() + offset, count));
        offset += count;
    }
    append(output, resampler.flush());
    return output;
}

double rms(const std::vector<float>& samples, std::size_t edgeToSkip)
{
    if (samples.size() <= edgeToSkip * 2U) {
        return 0.0;
    }
    double energy = 0.0;
    for (std::size_t index = edgeToSkip;
         index < samples.size() - edgeToSkip;
         ++index) {
        const double sample = samples[index];
        energy += sample * sample;
    }
    const std::size_t count = samples.size() - edgeToSkip * 2U;
    return std::sqrt(energy / static_cast<double>(count));
}

} // namespace

class TestSstvResampler final : public QObject
{
    Q_OBJECT

private slots:
    void supportedRatesAndExactFrameCount_data()
    {
        QTest::addColumn<quint32>("inputRate");
        for (const quint32 rate : {8'000U, 11'025U, 12'000U, 16'000U, 22'050U,
                                  24'000U, 32'000U, 44'100U, 48'000U, 96'000U}) {
            QTest::newRow(qPrintable(QString::number(rate))) << rate;
        }
    }

    void supportedRatesAndExactFrameCount()
    {
        QFETCH(quint32, inputRate);
        QVERIFY(SstvResampler::isSupportedInputRate(inputRate));
        SstvResampler resampler(inputRate);
        QCOMPARE(resampler.inputSampleRate(), std::uint32_t {inputRate});
        QCOMPARE(resampler.outputSampleRate(), std::uint32_t {12'000U});

        const std::size_t inputCount = static_cast<std::size_t>(inputRate / 5U + 137U);
        const std::vector<float> input(inputCount, 0.0F);
        std::vector<float> output;
        std::size_t offset = 0U;
        while (offset < input.size()) {
            const std::size_t count = std::min<std::size_t>(73U, input.size() - offset);
            append(output, resampler.process(input.data() + offset, count));
            offset += count;
            QVERIFY(resampler.bufferedInputSamples()
                    <= SstvResampler::kKernelHalfWidth * 2U + 73U);
        }
        append(output, resampler.flush());

        const std::uint64_t expected = static_cast<std::uint64_t>(inputCount)
            * SstvResampler::kOutputSampleRate / inputRate;
        QCOMPARE(output.size(), static_cast<std::size_t>(expected));
        QCOMPARE(resampler.totalInputSamples(), static_cast<std::uint64_t>(inputCount));
        QCOMPARE(resampler.totalOutputSamples(), expected);
        QCOMPARE(resampler.bufferedInputSamples(), std::size_t {0U});
        QVERIFY(resampler.isFlushed());
        QVERIFY(resampler.flush().empty());
    }

    void chunkBoundariesDoNotChangeOutput()
    {
        constexpr std::uint32_t inputRate = 44'100U;
        std::vector<float> input(static_cast<std::size_t>(inputRate / 3U));
        for (std::size_t index = 0U; index < input.size(); ++index) {
            const double time = static_cast<double>(index) / inputRate;
            input[index] = static_cast<float>(0.55 * std::sin(2.0 * kPi * 1'900.0 * time)
                                              + 0.2 * std::cos(2.0 * kPi * 1'100.0 * time));
        }

        const auto oneBlock = convert(inputRate, input, {});
        const auto manyBlocks = convert(inputRate, input, {1U, 7U, 64U, 3U, 511U, 29U});
        QCOMPARE(manyBlocks.size(), oneBlock.size());
        for (std::size_t index = 0U; index < oneBlock.size(); ++index) {
            QCOMPARE(manyBlocks[index], oneBlock[index]);
        }
    }

    void preservesSstvPassbandTone_data()
    {
        QTest::addColumn<quint32>("inputRate");
        QTest::newRow("upsample-8k") << quint32 {8'000U};
        QTest::newRow("fractional-44k1") << quint32 {44'100U};
        QTest::newRow("downsample-96k") << quint32 {96'000U};
    }

    void preservesSstvPassbandTone()
    {
        QFETCH(quint32, inputRate);
        const auto input = tone(inputRate, 1'900.0, inputRate / 2U);
        const auto output = convert(inputRate, input, {37U, 211U, 19U});
        const double level = rms(output, 300U);
        QVERIFY2(level > 0.65, qPrintable(QString("passband RMS too low: %1").arg(level)));
        QVERIFY2(level < 0.76, qPrintable(QString("passband RMS too high: %1").arg(level)));
    }

    void suppressesDownsamplingAliases_data()
    {
        QTest::addColumn<quint32>("inputRate");
        QTest::addColumn<double>("stopFrequency");
        QTest::newRow("48k-10k") << quint32 {48'000U} << 10'000.0;
        QTest::newRow("96k-20k") << quint32 {96'000U} << 20'000.0;
    }

    void suppressesDownsamplingAliases()
    {
        QFETCH(quint32, inputRate);
        QFETCH(double, stopFrequency);
        const std::size_t count = inputRate / 2U;
        const auto passband = convert(inputRate, tone(inputRate, 1'900.0, count), {257U});
        const auto stopband = convert(inputRate, tone(inputRate, stopFrequency, count), {257U});
        const double passLevel = rms(passband, 300U);
        const double stopLevel = rms(stopband, 300U);
        QVERIFY(passLevel > 0.65);
        QVERIFY2(stopLevel < passLevel * 0.02,
                 qPrintable(QString("alias level %1 relative to passband %2")
                                .arg(stopLevel)
                                .arg(passLevel)));
    }

    void resetRestoresInitialPhaseAndHistory()
    {
        const auto input = tone(48'000U, 2'300.0, 4'800U);
        SstvResampler resampler(48'000U);
        std::vector<float> first = resampler.process(input);
        append(first, resampler.flush());

        QVERIFY_THROWS_EXCEPTION(std::logic_error, resampler.process(input));
        resampler.reset();
        QVERIFY(!resampler.isFlushed());
        QCOMPARE(resampler.totalInputSamples(), std::uint64_t {0U});
        std::vector<float> second = resampler.process(input);
        append(second, resampler.flush());
        QCOMPARE(second, first);
    }

    void rejectsInvalidBlocksWithoutMutation()
    {
        QVERIFY(!SstvResampler::isSupportedInputRate(12'345U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvResampler(0U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvResampler(12'345U));

        SstvResampler resampler(48'000U);
        QCOMPARE(resampler.process(nullptr, 0U).size(), std::size_t {0U});
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, resampler.process(nullptr, 1U));

        float invalid[] = {0.0F, std::numeric_limits<float>::infinity(), 0.0F};
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, resampler.process(invalid, 3U));
        QCOMPARE(resampler.totalInputSamples(), std::uint64_t {0U});

        const float sample = 0.0F;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            resampler.process(&sample, SstvResampler::kMaxInputSamplesPerCall + 1U));
        QCOMPARE(resampler.totalInputSamples(), std::uint64_t {0U});
    }
};

QTEST_APPLESS_MAIN(TestSstvResampler)
#include "test_sstv_resampler.moc"
