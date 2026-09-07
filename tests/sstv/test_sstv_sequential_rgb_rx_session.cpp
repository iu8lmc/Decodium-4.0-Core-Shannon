// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvSequentialRgbRxSession.h"

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
    SstvSequentialRgbMode mode,
    std::uint32_t lineCount)
{
    const SstvSequentialRgbModeSpec spec =
        SstvSequentialRgbProtocol::spec(mode);
    SstvSequentialRgbMapper mapper({mode, kSampleRate, 0});
    lineCount = std::min(lineCount, spec.height);
    std::vector<SstvFrequencyObservation> result;
    std::uint64_t sequence = 1U;
    for (std::uint32_t line = 0U; line < lineCount; ++line) {
        for (std::uint64_t relative = mapper.lineStartSample(line);
             relative < mapper.lineEndSample(line);
             ++relative) {
            const SstvSequentialRgbPosition position =
                mapper.positionAtSample(relative);
            double frequency = SstvSequentialRgbProtocol::GapFrequencyHz;
            if (position.region == SstvSequentialRgbRegion::Sync) {
                frequency = SstvSequentialRgbProtocol::SyncFrequencyHz;
            } else if (position.region == SstvSequentialRgbRegion::Pixel) {
                const std::uint8_t value = static_cast<std::uint8_t>(
                    (position.pixel + position.line * 3U
                     + static_cast<unsigned>(position.component) * 29U)
                    & 0xffU);
                frequency = SstvSequentialRgbProtocol::frequencyForValue(value);
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

SstvSequentialRgbRxSessionConfig configFor(SstvSequentialRgbMode mode)
{
    SstvSequentialRgbRxSessionConfig config;
    config.mode = mode;
    config.sampleRate = kSampleRate;
    config.imageStartSample = kImageStart;
    config.observationSpanSamples = 3U;
    config.maximumPendingDirtyEvents = 16U;
    return config;
}

void consumeChunks(SstvSequentialRgbRxSession& session,
                   const std::vector<SstvFrequencyObservation>& input)
{
    const std::array<std::size_t, 5U> pattern {{1U, 31U, 509U, 4'093U,
                                                8'192U}};
    std::size_t offset = 0U;
    std::size_t patternIndex = 0U;
    while (offset < input.size()) {
        const std::size_t count = std::min(
            pattern[patternIndex++ % pattern.size()], input.size() - offset);
        const auto update = session.consume(input.data() + offset, count);
        QCOMPARE(update.inputObservations, count);
        offset += count;
    }
}

} // namespace

class TestSstvSequentialRgbRxSession final : public QObject
{
    Q_OBJECT

private slots:
    void arbitraryChunkingPublishesBoundedPartialFrames()
    {
        for (const SstvSequentialRgbMode mode : {
                 SstvSequentialRgbMode::WraaseSc2_60,
                 SstvSequentialRgbMode::WraaseSc2_120,
                 SstvSequentialRgbMode::WraaseSc2_180,
                 SstvSequentialRgbMode::PasokonP3,
                 SstvSequentialRgbMode::PasokonP5,
                 SstvSequentialRgbMode::PasokonP7}) {
            const auto input = prefixObservations(mode, 3U);
            SstvSequentialRgbRxSession session(configFor(mode));
            consumeChunks(session, input);
            QCOMPARE(session.notifyDiscontinuity(
                         input.back().centreSample + 10U),
                     SstvSequentialRgbRxSessionState::Partial);
            const SstvImageSnapshot image = session.snapshot();
            QVERIFY(image.coverage() > 0.0);
            QCOMPARE(session.decoderMetrics().linesPublished,
                     std::uint64_t {3U});
            QVERIFY(session.metrics().peakInputObservations
                    <= SstvSequentialRgbRxSession::MaximumObservationsPerConsume);
            QVERIFY(session.takeDirtyEvents().size() <= 16U);
        }
    }

    void cancellationAndBoundsFailClosed()
    {
        auto invalid = configFor(SstvSequentialRgbMode::PasokonP7);
        invalid.mode = static_cast<SstvSequentialRgbMode>(255U);
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvSequentialRgbRxSession {invalid});
        invalid = configFor(SstvSequentialRgbMode::PasokonP7);
        invalid.imageStartSample = std::numeric_limits<std::uint64_t>::max();
        QVERIFY_THROWS_EXCEPTION(
            std::overflow_error,
            SstvSequentialRgbRxSession {invalid});

        const auto input = prefixObservations(
            SstvSequentialRgbMode::PasokonP3, 1U);
        SstvSequentialRgbRxSession session(
            configFor(SstvSequentialRgbMode::PasokonP3));
        session.cancel();
        session.cancel();
        QCOMPARE(session.state(),
                 SstvSequentialRgbRxSessionState::Cancelled);
        QCOMPARE(session.consume(input.data(), 1U)
                     .decoderAcceptedObservations,
                 std::size_t {0U});
        QVERIFY(session.snapshot().cancelled);
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            session.consume(
                input.data(),
                SstvSequentialRgbRxSession::MaximumObservationsPerConsume
                    + 1U));
    }

    void regressionsRejectWholeBatchAndObservationSpanClosesInput()
    {
        const auto input = prefixObservations(
            SstvSequentialRgbMode::WraaseSc2_60, 1U);
        QVERIFY(input.size() > 10U);
        SstvSequentialRgbRxSession regression(
            configFor(SstvSequentialRgbMode::WraaseSc2_60));
        const auto first = regression.consume(input.data(), 10U);
        QVERIFY(first.decoderAcceptedObservations <= 10U);
        const auto rejected = regression.consume(input.data() + 9U, 1U);
        QCOMPARE(rejected.decoderAcceptedObservations, std::size_t {0U});
        QCOMPARE(regression.metrics().rejectedRegressions,
                 std::uint64_t {1U});
        QCOMPARE(regression.metrics().rejectedInputCalls,
                 std::uint64_t {1U});

        SstvSequentialRgbRxSession span(
            configFor(SstvSequentialRgbMode::WraaseSc2_60));
        consumeChunks(span, input);
        SstvFrequencyObservation final = input.back();
        ++final.sequence;
        final.centreSample = span.imageEndSample() - 2U;
        const auto closed = span.consume(&final, 1U);
        QCOMPARE(closed.state, SstvSequentialRgbRxSessionState::Partial);
        QVERIFY(closed.imageChanged);
        QCOMPARE(span.state(), SstvSequentialRgbRxSessionState::Partial);
        QVERIFY(span.snapshot().coverage() > 0.0);
    }
};

QTEST_APPLESS_MAIN(TestSstvSequentialRgbRxSession)

#include "test_sstv_sequential_rgb_rx_session.moc"
