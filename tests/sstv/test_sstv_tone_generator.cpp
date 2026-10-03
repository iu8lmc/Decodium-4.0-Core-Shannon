// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/tx/SstvToneGenerator.h"
#include "../../src/sstv/tx/SstvTxStream.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

std::vector<SstvToneSegment> mixedTonePlan()
{
    return {
        {1'900.0, Picoseconds {100'000'000'000LL}, 1.0,
         SstvTxSegmentRole::Header},
        {1'200.0, Picoseconds {30'000'000'000LL}, 0.85,
         SstvTxSegmentRole::Vis},
        {1'500.0, Picoseconds {107'123'456'789LL}, 1.0,
         SstvTxSegmentRole::Image},
        {2'100.0, Picoseconds {44'000'000'000LL}, 0.9,
         SstvTxSegmentRole::FskId},
        {1'500.0, Picoseconds {20'000'000'000LL}, 0.7,
         SstvTxSegmentRole::Tail}
    };
}

std::vector<float> renderFloat(SstvTxStream& stream, std::size_t chunkSize)
{
    if (chunkSize == 0U) {
        throw std::invalid_argument("test chunk size must be positive");
    }

    std::vector<float> result;
    result.reserve(static_cast<std::size_t>(stream.totalSamples()));
    std::vector<float> chunk(chunkSize);
    while (!stream.complete()) {
        const std::size_t produced = stream.pullFloat(chunk.data(), chunk.size());
        if (produced == 0U) {
            throw std::runtime_error("test stream made no progress");
        }
        result.insert(result.end(), chunk.begin(), chunk.begin() + produced);
    }
    return result;
}

std::vector<std::int16_t> renderPcm16(SstvTxStream& stream,
                                      std::size_t chunkSize)
{
    if (chunkSize == 0U) {
        throw std::invalid_argument("test chunk size must be positive");
    }

    std::vector<std::int16_t> result;
    result.reserve(static_cast<std::size_t>(stream.totalSamples()));
    std::vector<std::int16_t> chunk(chunkSize);
    while (!stream.complete()) {
        const std::size_t produced = stream.pullPcm16(chunk.data(), chunk.size());
        if (produced == 0U) {
            throw std::runtime_error("test stream made no progress");
        }
        result.insert(result.end(), chunk.begin(), chunk.begin() + produced);
    }
    return result;
}

} // namespace

class TestSstvToneGenerator final : public QObject
{
    Q_OBJECT

private slots:
    void fractionalSampleCountsCarryAcrossSegments()
    {
        constexpr std::uint32_t sampleRate = 44'100;
        constexpr std::int64_t segmentDuration = 275'200'000; // 275.2 us
        constexpr std::uint64_t repetitions = 1'000;

        std::vector<SstvToneSegment> plan;
        plan.reserve(repetitions);
        for (std::uint64_t i = 0; i < repetitions; ++i) {
            plan.push_back({
                (i & 1U) == 0U ? 1'500.0 : 2'300.0,
                Picoseconds {segmentDuration},
                1.0,
                SstvTxSegmentRole::Image
            });
        }

        const std::uint64_t numerator =
            static_cast<std::uint64_t>(segmentDuration)
            * repetitions * sampleRate;
        const std::uint64_t expected = numerator
            / static_cast<std::uint64_t>(kPicosecondsPerSecond);
        const std::uint64_t remainder = numerator
            % static_cast<std::uint64_t>(kPicosecondsPerSecond);

        SstvTxStream stream(sampleRate, plan);
        QCOMPARE(stream.totalSamples(), expected);
        QCOMPARE(stream.totalDuration().count,
                 segmentDuration * static_cast<std::int64_t>(repetitions));

        SstvToneGenerator generator(sampleRate);
        std::uint64_t generatedTotal = 0;
        for (std::uint64_t i = 0; i < repetitions; ++i) {
            generatedTotal +=
                generator.samplesForDuration(Picoseconds {segmentDuration});
        }
        QCOMPARE(generatedTotal, expected);
        QCOMPARE(generator.scheduledSamples(), expected);
        QCOMPARE(generator.timingRemainder(), remainder);

        generator.reset();
        QCOMPARE(generator.scheduledSamples(), std::uint64_t {0});
        QCOMPARE(generator.timingRemainder(), std::uint64_t {0});
    }

    void outputIsIndependentOfPullChunkSize()
    {
        const auto plan = mixedTonePlan();

        SstvTxStream fullFloat(44'100, plan);
        SstvTxStream chunkedFloat(44'100, plan);
        const auto fullFloatOutput = renderFloat(
            fullFloat,
            static_cast<std::size_t>(fullFloat.totalSamples()));
        const auto chunkedFloatOutput = renderFloat(chunkedFloat, 137);
        QVERIFY(fullFloatOutput == chunkedFloatOutput);
        QCOMPARE(fullFloat.metrics().samplesGenerated,
                 fullFloat.totalSamples());
        QCOMPARE(chunkedFloat.metrics().samplesGenerated,
                 chunkedFloat.totalSamples());

        SstvTxStream fullPcm(48'000, plan);
        SstvTxStream chunkedPcm(48'000, plan);
        const auto fullPcmOutput = renderPcm16(
            fullPcm,
            static_cast<std::size_t>(fullPcm.totalSamples()));
        const auto chunkedPcmOutput = renderPcm16(chunkedPcm, 251);
        QVERIFY(fullPcmOutput == chunkedPcmOutput);
    }

    void phaseIsContinuousAcrossSegmentAndFrequencyBoundaries()
    {
        // At 48 kHz, 250 us is exactly twelve samples.  A 1 kHz tone reaches
        // one quarter turn at that boundary; the first sample of the second
        // (2 kHz) segment must therefore be +1, not a reset-to-zero sample.
        const std::vector<SstvToneSegment> plan {
            {1'000.0, Picoseconds {250'000'000LL}, 1.0,
             SstvTxSegmentRole::Header},
            {2'000.0, Picoseconds {250'000'000LL}, 1.0,
             SstvTxSegmentRole::Vis}
        };
        SstvTxStream stream(48'000, plan, 1.0);

        const auto first = stream.currentSegment();
        QVERIFY(first.has_value());
        QCOMPARE(first->planIndex, std::size_t {0});
        QCOMPARE(first->role, SstvTxSegmentRole::Header);
        QCOMPARE(first->sampleCount, std::uint64_t {12});

        std::vector<float> samples(24);
        QCOMPARE(stream.pullFloat(samples.data(), 12), std::size_t {12});
        const auto second = stream.currentSegment();
        QVERIFY(second.has_value());
        QCOMPARE(second->planIndex, std::size_t {1});
        QCOMPARE(second->role, SstvTxSegmentRole::Vis);
        QCOMPARE(second->samplesProduced, std::uint64_t {0});

        QCOMPARE(stream.pullFloat(samples.data() + 12, 12), std::size_t {12});
        QVERIFY(stream.complete());
        QVERIFY(!stream.currentSegment().has_value());
        QVERIFY(std::abs(samples[12] - 1.0F) < 1.0e-6F);
    }

    void nominalToneHasExpectedFrequencyAndHeadroom()
    {
        constexpr std::uint32_t sampleRate = 48'000;
        constexpr double frequency = 1'900.0;
        SstvToneGenerator generator(sampleRate);
        const auto sampleCount = generator.samplesForDuration(
            Picoseconds {kPicosecondsPerSecond});
        QCOMPARE(sampleCount, std::uint64_t {sampleRate});

        std::vector<float> samples(sampleRate);
        QCOMPARE(generator.generateFloat(
                     frequency, 1.0, samples.data(), samples.size()),
                 samples.size());

        std::size_t positiveCrossings = 0;
        double squareSum = 0.0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            squareSum += static_cast<double>(samples[i]) * samples[i];
            if (i != 0U && samples[i - 1] <= 0.0F && samples[i] > 0.0F) {
                ++positiveCrossings;
            }
        }

        QVERIFY(positiveCrossings >= 1'899U);
        QVERIFY(positiveCrossings <= 1'901U);
        const double rms = std::sqrt(squareSum / samples.size());
        QVERIFY(std::abs(rms - kDefaultSstvTxHeadroom / std::sqrt(2.0))
                < 1.0e-5);

        const auto& metrics = generator.metrics();
        QCOMPARE(metrics.samplesGenerated, std::uint64_t {sampleRate});
        QCOMPARE(metrics.clippedSamples, std::uint64_t {0});
        QVERIFY(metrics.peakBeforeClamp <= kDefaultSstvTxHeadroom + 1.0e-12);
        QVERIFY(metrics.peakBeforeClamp > kDefaultSstvTxHeadroom * 0.999);
        QVERIFY(metrics.peakAfterClamp <= kDefaultSstvTxHeadroom + 1.0e-12);
    }

    void floatAndPcmOutputsClampAndReportPeaks()
    {
        SstvToneGenerator headroomGenerator(48'000, 0.8);
        std::vector<float> floatSamples(400);
        QCOMPARE(headroomGenerator.generateFloat(
                     1'200.0, 1.0, floatSamples.data(), floatSamples.size()),
                 floatSamples.size());
        const auto maximum = *std::max_element(
            floatSamples.begin(), floatSamples.end());
        const auto minimum = *std::min_element(
            floatSamples.begin(), floatSamples.end());
        QVERIFY(maximum <= 0.800001F);
        QVERIFY(minimum >= -0.800001F);
        QCOMPARE(headroomGenerator.metrics().clippedSamples, std::uint64_t {0});

        SstvToneGenerator clippingGenerator(48'000, 1.0);
        std::vector<std::int16_t> pcm(400);
        QCOMPARE(clippingGenerator.generatePcm16(
                     1'200.0, 2.0, pcm.data(), pcm.size()),
                 pcm.size());
        QCOMPARE(*std::max_element(pcm.begin(), pcm.end()),
                 std::numeric_limits<std::int16_t>::max());
        QCOMPARE(*std::min_element(pcm.begin(), pcm.end()),
                 std::numeric_limits<std::int16_t>::min());
        QVERIFY(clippingGenerator.metrics().clippedSamples > 0U);
        QVERIFY(clippingGenerator.metrics().peakBeforeClamp > 1.99);
        QCOMPARE(clippingGenerator.metrics().peakAfterClamp, 1.0);
    }

    void cancellationStopsAtAChunkBoundaryAndResetRestarts()
    {
        SstvToneGenerator generator(48'000);
        std::vector<float> untouched(32, 7.0F);
        generator.cancel();
        QCOMPARE(generator.generateFloat(
                     1'500.0, 1.0, untouched.data(), untouched.size()),
                 std::size_t {0});
        QVERIFY(std::all_of(untouched.begin(), untouched.end(),
                            [](float value) { return value == 7.0F; }));
        QVERIFY(generator.cancelled());

        generator.reset();
        QVERIFY(!generator.cancelled());
        QCOMPARE(generator.phaseTurns(), 0.0);
        QCOMPARE(generator.metrics().samplesGenerated, std::uint64_t {0});

        const auto plan = mixedTonePlan();
        SstvTxStream stream(48'000, plan);
        std::vector<float> chunk(257);
        QCOMPARE(stream.pullFloat(chunk.data(), 17), std::size_t {17});
        const auto position = stream.producedSamples();
        stream.cancel();
        QCOMPARE(stream.pullFloat(chunk.data(), chunk.size()), std::size_t {0});
        QCOMPARE(stream.producedSamples(), position);
        QVERIFY(!stream.complete());
        QVERIFY(stream.cancelled());

        const auto total = stream.totalSamples();
        stream.reset();
        QVERIFY(!stream.cancelled());
        QCOMPARE(stream.producedSamples(), std::uint64_t {0});
        QCOMPARE(stream.totalSamples(), total);
        QCOMPARE(stream.progress(), 0.0);
        QCOMPARE(stream.pullFloat(chunk.data(), chunk.size()), chunk.size());
    }

    void invalidInputsAreRejected()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvToneGenerator(0));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvToneGenerator(7'999));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvToneGenerator(384'001));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvToneGenerator(48'000, 0.0));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvToneGenerator(48'000, 1.01));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvToneGenerator(48'000,
                              std::numeric_limits<double>::quiet_NaN()));

        SstvToneGenerator generator(48'000);
        float output = 0.0F;
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            generator.generateFloat(0.0, 1.0, &output, 1));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            generator.generateFloat(24'000.0, 1.0, &output, 1));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            generator.generateFloat(
                std::numeric_limits<double>::quiet_NaN(), 1.0, &output, 1));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            generator.generateFloat(1'500.0, -0.1, &output, 1));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            generator.generateFloat(1'500.0, 1.0, nullptr, 1));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            generator.samplesForDuration(Picoseconds {-1}));
        QCOMPARE(generator.generateFloat(1'500.0, 1.0, nullptr, 0),
                 std::size_t {0});

        const auto invalidDuration = std::vector<SstvToneSegment> {
            {1'500.0, Picoseconds {-1}, 1.0, SstvTxSegmentRole::Image}
        };
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvTxStream(48'000, invalidDuration));

        const auto invalidFrequency = std::vector<SstvToneSegment> {
            {30'000.0, Picoseconds {1}, 1.0, SstvTxSegmentRole::Image}
        };
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvTxStream(48'000, invalidFrequency));

        const auto invalidRole = std::vector<SstvToneSegment> {
            {1'500.0, Picoseconds {1}, 1.0,
             static_cast<SstvTxSegmentRole>(255)}
        };
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvTxStream(48'000, invalidRole));

        const auto durationOverflow = std::vector<SstvToneSegment> {
            {1'500.0,
             Picoseconds {std::numeric_limits<std::int64_t>::max()},
             1.0,
             SstvTxSegmentRole::Header},
            {1'500.0, Picoseconds {1}, 1.0, SstvTxSegmentRole::Tail}
        };
        QVERIFY_THROWS_EXCEPTION(std::overflow_error,
                                 SstvTxStream(48'000, durationOverflow));

        SstvTxStream empty(48'000, {});
        QVERIFY(empty.complete());
        QCOMPARE(empty.progress(), 1.0);
        QCOMPARE(empty.pullFloat(nullptr, 0), std::size_t {0});
    }

    void veryLongTransmissionKeepsOnlyBoundedPullStorage()
    {
        constexpr std::int64_t seconds = 48 * 60 * 60;
        constexpr std::int64_t duration =
            seconds * kPicosecondsPerSecond;
        const std::vector<SstvToneSegment> plan {
            {1'500.0, Picoseconds {duration}, 1.0,
             SstvTxSegmentRole::Image}
        };

        SstvTxStream stream(48'000, plan);
        QCOMPARE(stream.plannedSegmentCount(), std::size_t {1});
        QCOMPARE(stream.totalSamples(),
                 static_cast<std::uint64_t>(seconds) * 48'000U);
        QVERIFY(stream.totalSamples()
                > std::numeric_limits<std::uint32_t>::max());

        std::vector<float> boundedChunk(257);
        QCOMPARE(stream.pullFloat(boundedChunk.data(), boundedChunk.size()),
                 boundedChunk.size());
        QCOMPARE(stream.producedSamples(), std::uint64_t {257});
        QCOMPARE(stream.remainingSamples(), stream.totalSamples() - 257U);
        QVERIFY(stream.progress() > 0.0);
        QVERIFY(stream.progress() < 1.0);
        QVERIFY(!stream.complete());
    }
};

QTEST_APPLESS_MAIN(TestSstvToneGenerator)
#include "test_sstv_tone_generator.moc"
