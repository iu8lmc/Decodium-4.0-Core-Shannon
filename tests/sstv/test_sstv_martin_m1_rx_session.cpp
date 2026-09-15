// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvMartinM1RxSession.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::uint32_t kObservationSpan = 3U;
constexpr std::uint64_t kImageStart = 50'000U;

SstvRgbPixel patternPixel(std::uint32_t x, std::uint32_t y)
{
    const auto red = static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(x) * 255U + 159U) / 319U);
    const auto green = static_cast<std::uint8_t>(y);
    const auto blue = static_cast<std::uint8_t>(
        (static_cast<unsigned>(red) + static_cast<unsigned>(green)) / 2U);
    return {red, green, blue};
}

double pixelFrequency(const SstvMartinM1Position& position)
{
    const auto pixel = patternPixel(position.pixel, position.line);
    std::uint8_t value = 0U;
    switch (position.component) {
    case ColourComponent::Red:
        value = pixel.red;
        break;
    case ColourComponent::Green:
        value = pixel.green;
        break;
    case ColourComponent::Blue:
        value = pixel.blue;
        break;
    default:
        throw std::logic_error("test received a non-pixel component");
    }
    return SstvMartinM1Protocol::frequencyForValue(value);
}

std::vector<SstvFrequencyObservation> syntheticObservations(
    std::uint64_t firstRelativeSample,
    std::uint64_t endRelativeSample,
    double frequencyOffsetHz = 0.0)
{
    SstvMartinM1Mapper mapper({kSampleRate, 0});
    endRelativeSample = std::min(endRelativeSample, mapper.imageSampleCount());
    std::vector<SstvFrequencyObservation> observations;
    if (firstRelativeSample >= endRelativeSample) {
        return observations;
    }
    observations.reserve(static_cast<std::size_t>(
        (endRelativeSample - firstRelativeSample) / kObservationSpan + 1U));

    std::uint64_t relative = firstRelativeSample;
    const std::uint64_t remainder = relative % kObservationSpan;
    if (remainder != 1U) {
        relative += (kObservationSpan + 1U - remainder)
            % kObservationSpan;
    }
    std::uint64_t sequence = 0U;
    for (; relative < endRelativeSample; relative += kObservationSpan) {
        const auto position = mapper.positionAtSample(relative);
        double frequency = SstvMartinM1Protocol::SeparatorFrequencyHz;
        if (position.region == SstvMartinM1Region::Sync) {
            frequency = SstvMartinM1Protocol::SyncFrequencyHz;
        } else if (position.region == SstvMartinM1Region::Pixel) {
            frequency = pixelFrequency(position);
        }
        frequency += frequencyOffsetHz;
        SstvFrequencyObservation observation;
        observation.status = SstvFrequencyStatus::Valid;
        observation.sequence = sequence++;
        observation.centreSample = kImageStart + relative;
        observation.rawFrequencyHz = frequency;
        observation.correctedFrequencyHz = frequency;
        observation.afcCorrectionHz = 0.0;
        observation.rms = 0.5;
        observation.snrDb = 35.0;
        observation.confidence = 0.98;
        observation.validSampleFraction = 1.0;
        observations.push_back(observation);
    }
    return observations;
}

SstvMartinM1RxSessionConfig sessionConfig()
{
    SstvMartinM1RxSessionConfig config;
    config.sampleRate = kSampleRate;
    config.imageStartSample = kImageStart;
    config.observationSpanSamples = kObservationSpan;
    config.minimumObservationConfidence = 0.20;
    config.maximumPendingDirtyEvents = 32U;
    return config;
}

void consumeChunks(SstvMartinM1RxSession& session,
                   const std::vector<SstvFrequencyObservation>& observations,
                   const std::vector<std::size_t>& pattern)
{
    std::size_t offset = 0U;
    std::size_t patternIndex = 0U;
    while (offset < observations.size()) {
        const std::size_t count = std::min(
            pattern[patternIndex++ % pattern.size()],
            observations.size() - offset);
        const auto update = session.consume(observations.data() + offset, count);
        QVERIFY(update.inputObservations == count);
        QVERIFY(update.publishedLineRevision
                <= SstvMartinM1Protocol::Height);
        offset += count;
    }
}

void compareRepresentativePixels(const SstvImageSnapshot& snapshot,
                                 std::uint8_t tolerance)
{
    for (std::uint32_t y : {0U, 1U, 31U, 127U, 255U}) {
        // The terminal x=319 pixel shares its acquisition edge with the
        // trailing 1500 Hz separator and is intentionally covered by the
        // separate coverage assertion rather than a zero-smear value oracle.
        for (std::uint32_t x : {0U, 1U, 79U, 159U, 318U}) {
            const auto actual = snapshot.pixel(x, y);
            const auto expected = patternPixel(x, y);
            QVERIFY(std::abs(static_cast<int>(actual.red)
                             - static_cast<int>(expected.red))
                    <= tolerance);
            QVERIFY(std::abs(static_cast<int>(actual.green)
                             - static_cast<int>(expected.green))
                    <= tolerance);
            QVERIFY(std::abs(static_cast<int>(actual.blue)
                             - static_cast<int>(expected.blue))
                    <= tolerance);
        }
    }
}

} // namespace

class TestSstvMartinM1RxSession final : public QObject
{
    Q_OBJECT

private slots:
    void validatesConfigurationAndSeedsLineZero();
    void syntheticFullFrameIsProgressiveAndChunkInvariant();
    void frequencyOffsetAlsoCalibratesSyncTracking();
    void discontinuityPreservesAUsablePartialFrame();
    void rejectsClockRegressionTransactionally();
    void rejectsUnrepresentableDecoderSyncAnchor();
    void cancellationIsIdempotentAndBounded();
};

void TestSstvMartinM1RxSession::validatesConfigurationAndSeedsLineZero()
{
    auto invalid = sessionConfig();
    invalid.sampleRate = 7'999U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvMartinM1RxSession {invalid});
    invalid = sessionConfig();
    invalid.observationSpanSamples = 0U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvMartinM1RxSession {invalid});
    invalid = sessionConfig();
    invalid.frequencyOffsetHz =
        std::numeric_limits<double>::infinity();
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvMartinM1RxSession {invalid});
    invalid = sessionConfig();
    invalid.imageStartSample = std::numeric_limits<std::uint64_t>::max();
    QVERIFY_THROWS_EXCEPTION(std::overflow_error,
                             SstvMartinM1RxSession {invalid});

    SstvMartinM1RxSession session(sessionConfig());
    QCOMPARE(session.state(), SstvMartinM1RxSessionState::Receiving);
    QCOMPARE(session.imageStartSample(), kImageStart);
    QVERIFY(session.imageEndSample() > session.imageStartSample());
    QCOMPARE(session.decoderMetrics().storedSyncAnchors, std::size_t {1U});
    QCOMPARE(session.decoderMetrics().observedSyncs, std::uint64_t {1U});
    QCOMPARE(session.syncSnapshot().metrics.lockAcquisitions,
             std::uint64_t {1U});
    QCOMPARE(session.snapshot().coverage(), 0.0);

    QVERIFY(session.consume(nullptr, 0U).inputObservations == 0U);
    SstvFrequencyObservation observation;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             session.consume(nullptr, 1U));
    QVERIFY_THROWS_EXCEPTION(
        std::length_error,
        session.consume(
            &observation,
            SstvMartinM1RxSession::MaximumObservationsPerConsume + 1U));

    SstvMartinM1RxSession empty(sessionConfig());
    QCOMPARE(empty.finish(), SstvMartinM1RxSessionState::Partial);
    QCOMPARE(empty.syncSnapshot().metrics.clockRegressions,
             std::uint64_t {0U});
}

void TestSstvMartinM1RxSession::syntheticFullFrameIsProgressiveAndChunkInvariant()
{
    SstvMartinM1Mapper mapper({kSampleRate, 0});
    const auto observations = syntheticObservations(
        0U, mapper.imageSampleCount());
    QVERIFY(observations.size() > 400'000U);

    SstvMartinM1RxSession first(sessionConfig());
    consumeChunks(first, observations, {8'192U});
    QCOMPARE(observations.back().centreSample,
             first.imageEndSample() - 1U);
    // A complete final scanline is a protocol terminal condition.  It must
    // not require a fabricated observation centred beyond the image.
    QCOMPARE(first.state(), SstvMartinM1RxSessionState::Complete);
    QCOMPARE(first.finish(), SstvMartinM1RxSessionState::Complete);
    const auto firstSnapshot = first.snapshot();
    QVERIFY(firstSnapshot.isComplete());
    QCOMPARE(firstSnapshot.completedPixels,
             static_cast<std::size_t>(SstvMartinM1Protocol::Width)
                 * SstvMartinM1Protocol::Height);
    // Three-sample observations quantise the detected sync edge by at most a
    // sample.  Smooth ramps keep that acquisition tolerance measurable.
    compareRepresentativePixels(firstSnapshot, 2U);

    const auto dirty = first.takeDirtyEvents();
    QVERIFY(!dirty.empty());
    QVERIFY(dirty.size() <= sessionConfig().maximumPendingDirtyEvents);
    QVERIFY(std::any_of(dirty.cbegin(), dirty.cend(),
                        [](const SstvDirtyEvent& event) {
                            return event.coalesced;
                        }));

    SstvMartinM1RxSession fragmented(sessionConfig());
    consumeChunks(fragmented, observations,
                  {1U, 17U, 251U, 4'093U, 7'999U, 3U});
    QCOMPARE(fragmented.state(), SstvMartinM1RxSessionState::Complete);
    const auto fragmentedState = fragmented.finish();
    QCOMPARE(fragmentedState, SstvMartinM1RxSessionState::Complete);
    const auto fragmentedSnapshot = fragmented.snapshot();
    QCOMPARE(fragmentedSnapshot.pixels, firstSnapshot.pixels);
    QCOMPARE(fragmentedSnapshot.channelCoverage,
             firstSnapshot.channelCoverage);
    QCOMPARE(fragmented.decoderMetrics().linesPublished,
             std::uint64_t {SstvMartinM1Protocol::Height});
    QCOMPARE(fragmented.metrics().peakFilteredObservations,
             std::size_t {7'999U});
    QVERIFY(fragmented.metrics().observedLineSyncs >= 250U);
    QCOMPARE(fragmented.metrics().decoderAcceptedObservations,
             first.metrics().decoderAcceptedObservations);
    QCOMPARE(first.syncSnapshot().metrics.clockRegressions,
             std::uint64_t {0U});
    QCOMPARE(fragmented.syncSnapshot().metrics.clockRegressions,
             std::uint64_t {0U});

    // Exercise every possible late boundary across line 255.  In particular,
    // accepting its sync while line 254 is still the buffered full row must
    // remain Receiving rather than misclassifying that row as the terminal
    // one.
    const std::uint64_t finalLineStart = kImageStart
        + mapper.lineStartSample(SstvMartinM1Protocol::Height - 1U);
    const auto finalLine = std::lower_bound(
        observations.cbegin(), observations.cend(), finalLineStart,
        [](const SstvFrequencyObservation& observation,
           std::uint64_t sample) {
            return observation.centreSample < sample;
        });
    const std::size_t finalLineIndex = static_cast<std::size_t>(
        finalLine - observations.cbegin());
    SstvMartinM1RxSession boundary(sessionConfig());
    std::size_t boundaryOffset = 0U;
    while (boundaryOffset < finalLineIndex) {
        const std::size_t count = std::min<std::size_t>(
            SstvMartinM1RxSession::MaximumObservationsPerConsume,
            finalLineIndex - boundaryOffset);
        boundary.consume(observations.data() + boundaryOffset, count);
        boundaryOffset += count;
    }
    QCOMPARE(boundary.state(), SstvMartinM1RxSessionState::Receiving);
    bool sawFinalAnchorWhileReceiving = false;
    for (; boundaryOffset < observations.size(); ++boundaryOffset) {
        boundary.consume(observations.data() + boundaryOffset, 1U);
        QVERIFY(boundary.state() != SstvMartinM1RxSessionState::Partial);
        sawFinalAnchorWhileReceiving = sawFinalAnchorWhileReceiving
            || (boundary.state() == SstvMartinM1RxSessionState::Receiving
                && boundary.decoderMetrics().storedSyncAnchors
                    == SstvMartinM1Protocol::Height);
    }
    QVERIFY(sawFinalAnchorWhileReceiving);
    QCOMPARE(boundary.state(), SstvMartinM1RxSessionState::Complete);
    QCOMPARE(boundary.snapshot().pixels, firstSnapshot.pixels);
}

void TestSstvMartinM1RxSession::frequencyOffsetAlsoCalibratesSyncTracking()
{
    SstvMartinM1Mapper mapper({kSampleRate, 0});
    const std::uint64_t end = mapper.lineStartSample(20U);
    for (const double offsetHz : {100.0, -100.0}) {
        const auto observations = syntheticObservations(0U, end, offsetHz);
        auto config = sessionConfig();
        config.frequencyOffsetHz = offsetHz;
        SstvMartinM1RxSession session(config);
        consumeChunks(session, observations, {1U, 31U, 4'093U});
        QCOMPARE(session.notifyDiscontinuity(kImageStart + end + 1U),
                 SstvMartinM1RxSessionState::Partial);
        QVERIFY(session.metrics().observedLineSyncs >= 19U);
        QVERIFY(session.decoderMetrics().storedSyncAnchors >= 20U);
        QVERIFY(session.snapshot().coverage() > 0.06);
        QCOMPARE(session.syncSnapshot().metrics.clockRegressions,
                 std::uint64_t {0U});
    }
}

void TestSstvMartinM1RxSession::discontinuityPreservesAUsablePartialFrame()
{
    SstvMartinM1Mapper mapper({kSampleRate, 0});
    const std::uint64_t end = mapper.lineStartSample(12U);
    const auto observations = syntheticObservations(0U, end);
    SstvMartinM1RxSession session(sessionConfig());
    consumeChunks(session, observations, {3'001U, 19U});
    QCOMPARE(session.notifyDiscontinuity(kImageStart + end + 500U),
             SstvMartinM1RxSessionState::Partial);
    const auto snapshot = session.snapshot();
    QVERIFY(!snapshot.isComplete());
    QVERIFY(snapshot.coverage() > 0.03);
    QVERIFY(snapshot.coverage() < 0.10);
    QVERIFY(session.decoderMetrics().linesPublished >= 11U);
    QCOMPARE(session.metrics().discontinuities, std::uint64_t {1U});
    QCOMPARE(session.finish(), SstvMartinM1RxSessionState::Partial);
}

void TestSstvMartinM1RxSession::rejectsClockRegressionTransactionally()
{
    auto observations = syntheticObservations(0U, 30'000U);
    QVERIFY(observations.size() > 10U);
    SstvMartinM1RxSession session(sessionConfig());
    const auto first = session.consume(observations.data(), 10U);
    QCOMPARE(first.inputObservations, std::size_t {10U});
    const auto metricsBefore = session.decoderMetrics();

    SstvFrequencyObservation stale = observations[9];
    const auto rejected = session.consume(&stale, 1U);
    QCOMPARE(rejected.decoderAcceptedObservations, std::size_t {0U});
    QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {1U});
    QCOMPARE(session.decoderMetrics().observationInputs,
             metricsBefore.observationInputs);
    QCOMPARE(session.state(), SstvMartinM1RxSessionState::Receiving);

    SstvFrequencyObservation staleSequence = observations[10];
    staleSequence.sequence = observations[9].sequence;
    const auto sequenceRejected = session.consume(&staleSequence, 1U);
    QCOMPARE(sequenceRejected.decoderAcceptedObservations, std::size_t {0U});
    QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {2U});
    QCOMPARE(session.decoderMetrics().observationInputs,
             metricsBefore.observationInputs);

    const auto resumed = session.consume(observations.data() + 10U, 1U);
    QCOMPARE(resumed.inputObservations, std::size_t {1U});
    QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {2U});
    QCOMPARE(session.decoderMetrics().observationInputs,
             metricsBefore.observationInputs + 1U);
}

void TestSstvMartinM1RxSession::rejectsUnrepresentableDecoderSyncAnchor()
{
    SstvMartinM1Decoder decoder;
    const SstvMartinM1LineSync overflow {
        0U, std::numeric_limits<std::uint64_t>::max(), 1.0, false};
    QCOMPARE(decoder.consumeLineSyncs(&overflow, 1U), std::size_t {0U});
    QCOMPARE(decoder.metrics().rejectedSyncs, std::uint64_t {1U});
    QCOMPARE(decoder.metrics().storedSyncAnchors, std::size_t {0U});

    SstvMartinM1Mapper mapper({kSampleRate, 0});
    const std::uint64_t lineSpan = mapper.lineEndSample(0U)
        - mapper.lineStartSample(0U);
    SstvMartinM1Decoder boundaryDecoder;
    const SstvMartinM1LineSync boundary {
        0U,
        std::numeric_limits<std::uint64_t>::max() - lineSpan,
        1.0,
        false};
    QCOMPARE(boundaryDecoder.consumeLineSyncs(&boundary, 1U),
             std::size_t {1U});
}

void TestSstvMartinM1RxSession::cancellationIsIdempotentAndBounded()
{
    const auto observations = syntheticObservations(0U, 80'000U);
    SstvMartinM1RxSession session(sessionConfig());
    consumeChunks(session, observations, {1'024U});
    session.cancel();
    session.cancel();
    QCOMPARE(session.state(), SstvMartinM1RxSessionState::Cancelled);
    QCOMPARE(session.metrics().cancelCalls, std::uint64_t {2U});
    QVERIFY(session.snapshot().cancelled);
    const auto before = session.decoderMetrics();
    const auto ignored = session.consume(observations.data(), 1U);
    QCOMPARE(ignored.decoderAcceptedObservations, std::size_t {0U});
    QCOMPARE(session.decoderMetrics().observationInputs,
             before.observationInputs);
    QVERIFY(session.metrics().peakFilteredObservations
            <= SstvMartinM1RxSession::MaximumObservationsPerConsume);
}

QTEST_APPLESS_MAIN(TestSstvMartinM1RxSession)
#include "test_sstv_martin_m1_rx_session.moc"
