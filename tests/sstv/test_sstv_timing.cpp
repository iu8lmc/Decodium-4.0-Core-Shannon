// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/core/SstvTimingAccumulator.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace decodium::sstv;

class TestSstvTimingAccumulator final : public QObject
{
    Q_OBJECT

private slots:
    void exactSecondAtSupportedRates_data()
    {
        QTest::addColumn<quint32>("sampleRate");
        QTest::newRow("8-kHz") << quint32 {8000};
        QTest::newRow("11.025-kHz") << quint32 {11025};
        QTest::newRow("44.1-kHz") << quint32 {44100};
        QTest::newRow("48-kHz") << quint32 {48000};
        QTest::newRow("96-kHz") << quint32 {96000};
    }

    void exactSecondAtSupportedRates()
    {
        QFETCH(quint32, sampleRate);
        SstvTimingAccumulator accumulator(sampleRate);
        QCOMPARE(accumulator.sampleRate(), std::uint32_t {sampleRate});
        QCOMPARE(accumulator.samplesFor(Picoseconds {kPicosecondsPerSecond}),
                 std::uint64_t {sampleRate});
        QCOMPARE(accumulator.totalSamples(), std::uint64_t {sampleRate});
        QCOMPARE(accumulator.fractionalRemainder(), std::uint64_t {0});
    }

    void fractionalCarryHasNoLongTermLineDrift_data()
    {
        exactSecondAtSupportedRates_data();
    }

    void fractionalCarryHasNoLongTermLineDrift()
    {
        QFETCH(quint32, sampleRate);
        constexpr std::int64_t durationPs = 275'200'000; // 0.2752 ms
        constexpr std::uint64_t repetitions = 1000;

        SstvTimingAccumulator accumulator(sampleRate);
        std::uint64_t returnedTotal = 0;
        for (std::uint64_t i = 0; i < repetitions; ++i) {
            returnedTotal += accumulator.samplesFor(Picoseconds {durationPs});
        }

        // This product is deliberately bounded below uint64_t max, so it is an
        // independent direct integer oracle for the accumulator.
        const std::uint64_t exactNumerator = static_cast<std::uint64_t>(durationPs)
            * repetitions * sampleRate;
        const std::uint64_t expectedSamples = exactNumerator
            / static_cast<std::uint64_t>(kPicosecondsPerSecond);
        const std::uint64_t expectedRemainder = exactNumerator
            % static_cast<std::uint64_t>(kPicosecondsPerSecond);

        QCOMPARE(returnedTotal, expectedSamples);
        QCOMPARE(accumulator.totalSamples(), expectedSamples);
        QCOMPARE(accumulator.fractionalRemainder(), expectedRemainder);
    }

    void carryIsCommonAcrossDifferentSegments()
    {
        constexpr std::uint32_t sampleRate = 44'100;
        constexpr std::int64_t firstPs = 4'862'000'000;
        constexpr std::int64_t secondPs = 572'000'000;
        constexpr std::int64_t thirdPs = 275'200'000;

        SstvTimingAccumulator segmented(sampleRate);
        const auto first = segmented.samplesFor(Picoseconds {firstPs});
        const auto second = segmented.samplesFor(Picoseconds {secondPs});
        const auto third = segmented.samplesFor(Picoseconds {thirdPs});

        SstvTimingAccumulator combined(sampleRate);
        const auto all = combined.samplesFor(Picoseconds {firstPs + secondPs + thirdPs});

        QCOMPARE(first + second + third, all);
        QCOMPARE(segmented.totalSamples(), combined.totalSamples());
        QCOMPARE(segmented.fractionalRemainder(), combined.fractionalRemainder());
    }

    void resetClearsSamplesAndFractionalError()
    {
        SstvTimingAccumulator accumulator(48'000);
        accumulator.samplesFor(Picoseconds {275'200'000});
        QVERIFY(accumulator.fractionalRemainder() != 0U);

        accumulator.reset();
        QCOMPARE(accumulator.totalSamples(), std::uint64_t {0});
        QCOMPARE(accumulator.fractionalRemainder(), std::uint64_t {0});

        QCOMPARE(accumulator.samplesFor(Picoseconds {kPicosecondsPerSecond}),
                 std::uint64_t {48'000});
    }

    void zeroDurationDoesNotDisturbExistingCarry()
    {
        SstvTimingAccumulator accumulator(44'100);
        accumulator.samplesFor(Picoseconds {275'200'000});
        const auto totalBefore = accumulator.totalSamples();
        const auto remainderBefore = accumulator.fractionalRemainder();

        QCOMPARE(accumulator.samplesFor(Picoseconds {0}), std::uint64_t {0});
        QCOMPARE(accumulator.totalSamples(), totalBefore);
        QCOMPARE(accumulator.fractionalRemainder(), remainderBefore);
    }

    void invalidInputsAreRejectedWithoutMutation()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvTimingAccumulator(0));

        SstvTimingAccumulator accumulator(48'000);
        accumulator.samplesFor(Picoseconds {123'456'789});
        const auto totalBefore = accumulator.totalSamples();
        const auto remainderBefore = accumulator.fractionalRemainder();

        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 accumulator.samplesFor(Picoseconds {-1}));
        QCOMPARE(accumulator.totalSamples(), totalBefore);
        QCOMPARE(accumulator.fractionalRemainder(), remainderBefore);
    }

    void maximumPortableSampleRateAvoidsIntermediateOverflow()
    {
        constexpr std::uint32_t sampleRate = std::numeric_limits<std::uint32_t>::max();
        constexpr std::int64_t duration = kPicosecondsPerSecond - 1;
        SstvTimingAccumulator accumulator(sampleRate);

        // floor((P - 1) * rate / P) == rate - 1 because rate < P.
        QCOMPARE(accumulator.samplesFor(Picoseconds {duration}),
                 std::uint64_t {sampleRate} - 1U);
        QCOMPARE(accumulator.fractionalRemainder(),
                 static_cast<std::uint64_t>(kPicosecondsPerSecond) - sampleRate);
    }

    void accumulatedSampleOverflowIsDetectedTransactionally()
    {
        SstvTimingAccumulator accumulator(std::numeric_limits<std::uint32_t>::max());
        const Picoseconds huge {std::numeric_limits<std::int64_t>::max()};
        bool overflowObserved = false;

        for (int attempt = 0; attempt < 1000; ++attempt) {
            const auto totalBefore = accumulator.totalSamples();
            const auto remainderBefore = accumulator.fractionalRemainder();
            try {
                accumulator.samplesFor(huge);
            } catch (const std::overflow_error&) {
                overflowObserved = true;
                QCOMPARE(accumulator.totalSamples(), totalBefore);
                QCOMPARE(accumulator.fractionalRemainder(), remainderBefore);
                break;
            }
        }
        QVERIFY(overflowObserved);
    }
};

QTEST_APPLESS_MAIN(TestSstvTimingAccumulator)
#include "test_sstv_timing.moc"
