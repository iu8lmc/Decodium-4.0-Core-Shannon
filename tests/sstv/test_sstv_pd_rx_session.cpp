// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvPdRxSession.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::uint64_t kImageStart = 90'000U;

std::vector<SstvFrequencyObservation> prefixObservations(
    SstvPdMode mode,
    std::uint32_t pairCount)
{
    SstvPdMapper mapper({mode, kSampleRate, 0});
    pairCount = std::min(pairCount, mapper.linePairCount());
    std::vector<SstvFrequencyObservation> result;
    std::uint64_t sequence = 1U;
    for (std::uint32_t pair = 0U; pair < pairCount; ++pair) {
        for (std::uint64_t relative = mapper.linePairStartSample(pair);
             relative < mapper.linePairEndSample(pair);
             ++relative) {
            const SstvPdPosition position = mapper.positionAtSample(relative);
            double frequency = SstvPdProtocol::PorchFrequencyHz;
            if (position.region == SstvPdRegion::Sync) {
                frequency = SstvPdProtocol::SyncFrequencyHz;
            } else if (position.region == SstvPdRegion::Pixel) {
                const std::uint8_t value = static_cast<std::uint8_t>(
                    (position.pixel + position.linePair * 7U
                     + static_cast<std::uint32_t>(position.scanIndex) * 41U)
                    & 0xffU);
                frequency = SstvPdProtocol::frequencyForValue(value);
            }
            SstvFrequencyObservation observation;
            observation.status = SstvFrequencyStatus::Valid;
            observation.sequence = sequence++;
            observation.centreSample = kImageStart + relative;
            observation.rawFrequencyHz = frequency;
            observation.correctedFrequencyHz = frequency;
            observation.rms = 0.5;
            observation.snrDb = 30.0;
            observation.confidence = 0.98;
            observation.validSampleFraction = 1.0;
            result.push_back(observation);
        }
    }
    return result;
}

SstvPdRxSessionConfig configFor(SstvPdMode mode)
{
    SstvPdRxSessionConfig config;
    config.mode = mode;
    config.sampleRate = kSampleRate;
    config.imageStartSample = kImageStart;
    config.observationSpanSamples = 3U;
    config.maximumPendingDirtyEvents = 16U;
    return config;
}

void consumeChunks(SstvPdRxSession& session,
                   const std::vector<SstvFrequencyObservation>& input)
{
    constexpr std::array<std::size_t, 5U> pattern {{
        1U, 31U, 509U, 4'093U, 8'192U}};
    std::size_t offset = 0U;
    std::size_t patternIndex = 0U;
    while (offset < input.size()) {
        const std::size_t count = std::min(
            pattern[patternIndex++ % pattern.size()], input.size() - offset);
        const SstvPdRxSessionUpdate update = session.consume(
            input.data() + offset, count);
        QCOMPARE(update.inputObservations, count);
        offset += count;
    }
}

} // namespace

class TestSstvPdRxSession final : public QObject
{
    Q_OBJECT

private slots:
    void arbitraryChunkingPublishesBoundedPartialPairs()
    {
        for (const SstvPdMode mode : {
                 SstvPdMode::Pd50,
                 SstvPdMode::Pd90,
                 SstvPdMode::Pd120,
                 SstvPdMode::Pd160,
                 SstvPdMode::Pd180,
                 SstvPdMode::Pd240,
                 SstvPdMode::Pd290}) {
            const auto input = prefixObservations(mode, 2U);
            QVERIFY(!input.empty());
            SstvPdRxSession session(configFor(mode));
            consumeChunks(session, input);
            QCOMPARE(session.notifyDiscontinuity(
                         input.back().centreSample + 10U),
                     SstvPdRxSessionState::Partial);
            const SstvImageSnapshot image = session.snapshot();
            QVERIFY(image.coverage() > 0.0);
            QCOMPARE(session.decoderMetrics().linePairsPublished,
                     std::uint64_t {2U});
            QCOMPARE(session.decoderMetrics().linesPublished,
                     std::uint64_t {4U});
            QVERIFY(session.metrics().peakInputObservations
                    <= SstvPdRxSession::MaximumObservationsPerConsume);
            QVERIFY(session.decoderMetrics().peakBufferedPixelAccumulators
                    <= static_cast<std::size_t>(image.width) * 4U);
            QVERIFY(session.takeDirtyEvents().size() <= 16U);
        }
    }

    void cancellationBoundsAndRegressionsFailClosed()
    {
        SstvPdRxSessionConfig invalid = configFor(SstvPdMode::Pd290);
        invalid.mode = static_cast<SstvPdMode>(255U);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvPdRxSession {invalid});
        invalid = configFor(SstvPdMode::Pd290);
        invalid.imageStartSample = std::numeric_limits<std::uint64_t>::max();
        QVERIFY_THROWS_EXCEPTION(std::overflow_error,
                                 SstvPdRxSession {invalid});

        const auto input = prefixObservations(SstvPdMode::Pd50, 1U);
        QVERIFY(input.size() > 10U);
        SstvPdRxSession regression(configFor(SstvPdMode::Pd50));
        static_cast<void>(regression.consume(input.data(), 10U));
        const SstvPdRxSessionUpdate rejected = regression.consume(
            input.data() + 9U, 1U);
        QCOMPARE(rejected.decoderAcceptedObservations, std::size_t {0U});
        QCOMPARE(regression.metrics().rejectedRegressions,
                 std::uint64_t {1U});

        SstvPdRxSession cancelled(configFor(SstvPdMode::Pd50));
        cancelled.cancel();
        cancelled.cancel();
        QCOMPARE(cancelled.state(), SstvPdRxSessionState::Cancelled);
        QCOMPARE(cancelled.consume(input.data(), 1U)
                     .decoderAcceptedObservations,
                 std::size_t {0U});
        QVERIFY(cancelled.snapshot().cancelled);
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            cancelled.consume(
                input.data(),
                SstvPdRxSession::MaximumObservationsPerConsume + 1U));
    }

    void observationSpanClosesAtCanonicalEndWithoutExtraPair()
    {
        const auto input = prefixObservations(SstvPdMode::Pd50, 1U);
        SstvPdRxSession session(configFor(SstvPdMode::Pd50));
        consumeChunks(session, input);
        SstvFrequencyObservation final = input.back();
        ++final.sequence;
        final.centreSample = session.imageEndSample() - 2U;
        const SstvPdRxSessionUpdate closed = session.consume(&final, 1U);
        QCOMPARE(closed.state, SstvPdRxSessionState::Partial);
        QCOMPARE(session.state(), SstvPdRxSessionState::Partial);
        QCOMPARE(session.imageEndSample(),
                 kImageStart
                     + SstvPdMapper({SstvPdMode::Pd50, kSampleRate, 0})
                           .imageSampleCount());
        QVERIFY(session.snapshot().coverage() > 0.0);
    }
};

QTEST_APPLESS_MAIN(TestSstvPdRxSession)

#include "test_sstv_pd_rx_session.moc"
