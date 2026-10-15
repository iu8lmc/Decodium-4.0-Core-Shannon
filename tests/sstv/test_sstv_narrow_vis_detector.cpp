// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/rx/SstvNarrowVisDetector.h"

#include <QTest>

#include <algorithm>
#include <vector>

using namespace decodium::sstv;

namespace {

std::vector<SstvNarrowVisToneEvent> eventsFor(SstvNarrowVisMode mode,
                                               std::uint64_t epoch = 0U)
{
    const SstvNarrowVisEncodedFrame frame = SstvNarrowVisCodec::encode(mode);
    std::vector<SstvNarrowVisToneEvent> events;
    for (const SstvNarrowVisTone& tone : frame.tones) {
        const std::uint64_t duration = static_cast<std::uint64_t>(
            tone.duration.count / kPicosecondsPerMicrosecond);
        if (!events.empty()
            && events.back().frequencyHz == tone.frequencyHz) {
            events.back().durationUs += duration;
        } else {
            events.push_back({epoch, duration, tone.frequencyHz, 1.0});
        }
        epoch += duration;
    }
    return events;
}

} // namespace

class TestSstvNarrowVisDetector final : public QObject
{
    Q_OBJECT

private slots:
    void allModesDecodeFromMergedRuns()
    {
        const SstvNarrowVisMode modes[] {
            SstvNarrowVisMode::Mp73,
            SstvNarrowVisMode::Mp110,
            SstvNarrowVisMode::Mp140,
            SstvNarrowVisMode::Mc110,
            SstvNarrowVisMode::Mc140,
            SstvNarrowVisMode::Mc180,
        };
        for (const SstvNarrowVisMode mode : modes) {
            SstvNarrowVisDetector detector;
            const auto events = eventsFor(mode, 1'000U);
            std::vector<SstvNarrowVisDetection> results;
            for (const auto& event : events) {
                const auto partial = detector.consume(&event, 1U);
                results.insert(results.end(), partial.begin(), partial.end());
            }
            QCOMPARE(results.size(), 1U);
            QVERIFY(results.front().valid());
            QVERIFY(results.front().codecResult.mode.has_value());
            QCOMPARE(*results.front().codecResult.mode, mode);
            QCOMPARE(results.front().frameStartedAtUs, 1'000U);
            QCOMPARE(results.front().frameEndedAtUs, 951'000U);
        }
    }

    void corruptionAndBoundsFailClosed()
    {
        auto events = eventsFor(SstvNarrowVisMode::Mp110);
        events[1U].durationUs = 30'000U;
        SstvNarrowVisDetector detector;
        const auto rejected = detector.consume(events);
        QVERIFY(!rejected.empty());
        QVERIFY(std::none_of(rejected.cbegin(), rejected.cend(),
                             [](const auto& item) { return item.valid(); }));

        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            detector.consume(nullptr,
                             SstvNarrowVisDetector::MaximumEventsPerConsume
                                 + 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvNarrowVisDetector(SstvNarrowVisDetectorConfig {
                100.0, 0.30, 0.30, 5'000U}));
    }

    void chunkingIsInvariantAndFinishIsBounded()
    {
        const auto events = eventsFor(SstvNarrowVisMode::Mc180, 42'000U);
        SstvNarrowVisDetector detector;
        std::vector<SstvNarrowVisDetection> results;
        for (std::size_t offset = 0U; offset < events.size();) {
            const std::size_t count = std::min<std::size_t>(
                2U, events.size() - offset);
            const auto part = detector.consume(events.data() + offset, count);
            results.insert(results.end(), part.begin(), part.end());
            offset += count;
        }
        QCOMPARE(results.size(), 1U);
        QVERIFY(results.front().valid());

        SstvNarrowVisDetector partial;
        const auto first = events.front();
        QCOMPARE(partial.consume(&first, 1U).size(), 0U);
        const auto finished = partial.finish(first.startTimeUs
                                             + first.durationUs);
        QVERIFY(finished.has_value());
        QCOMPARE(finished->status,
                 SstvNarrowVisDetectionStatus::Truncated);
    }
};

QTEST_APPLESS_MAIN(TestSstvNarrowVisDetector)
#include "test_sstv_narrow_vis_detector.moc"
