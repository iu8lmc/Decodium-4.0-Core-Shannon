// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvAvtRxSession.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::uint64_t kImageStart = 75'000U;
constexpr std::array<SstvAvtMode, 3U> kModes {{
    SstvAvtMode::Avt24,
    SstvAvtMode::Avt90,
    SstvAvtMode::Avt94,
}};

std::uint8_t componentValue(std::uint32_t x,
                            std::uint32_t y,
                            ColourComponent component) noexcept
{
    const std::uint32_t salt = component == ColourComponent::Red
        ? 19U : (component == ColourComponent::Green ? 91U : 173U);
    return static_cast<std::uint8_t>((x * 7U + y * 13U + salt) & 0xffU);
}

std::vector<SstvFrequencyObservation> allObservations(
    SstvAvtMode mode, SstvAvtVariant variant = SstvAvtVariant::Normal)
{
    const SstvAvtModeSpec spec = SstvAvtProtocol::spec(mode);
    const SstvAvtMapper mapper({mode, kSampleRate, 0, variant});
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(spec.width) * spec.height * 3U);
    std::uint64_t sequence = 1U;
    std::uint64_t sample = 0U;
    while (sample < mapper.imageSampleCount()) {
        const SstvAvtPosition position = mapper.positionAtSample(sample);
        if (!position.valid()) {
            throw std::logic_error("invalid AVT session test position");
        }
        SstvFrequencyObservation observation;
        observation.status = SstvFrequencyStatus::Valid;
        observation.sequence = sequence++;
        observation.centreSample = kImageStart + position.segmentStartSample
            + (position.segmentEndSample - position.segmentStartSample) / 2U;
        observation.rawFrequencyHz = SstvAvtProtocol::frequencyForValue(
            componentValue(position.pixel,
                           position.line,
                           position.component), variant);
        observation.correctedFrequencyHz = observation.rawFrequencyHz;
        observation.rms = 0.5;
        observation.snrDb = 36.0;
        observation.confidence = 0.99;
        observation.validSampleFraction = 1.0;
        result.push_back(observation);
        if (position.segmentEndSample <= sample) {
            throw std::logic_error("AVT session test made no progress");
        }
        sample = position.segmentEndSample;
    }
    return result;
}

SstvAvtRxSessionConfig configFor(SstvAvtMode mode)
{
    SstvAvtRxSessionConfig config;
    config.mode = mode;
    config.sampleRate = kSampleRate;
    config.imageStartSample = kImageStart;
    config.observationSpanSamples = 1U;
    config.maximumInterpolationGapPixels = 0U;
    return config;
}

void consumeChunks(SstvAvtRxSession& session,
                   const std::vector<SstvFrequencyObservation>& observations,
                   std::size_t chunk)
{
    std::size_t offset = 0U;
    while (offset < observations.size()) {
        const std::size_t count = std::min(chunk,
                                           observations.size() - offset);
        static_cast<void>(session.consume(observations.data() + offset,
                                          count));
        offset += count;
    }
}

} // namespace

class TestSstvAvtRxSession final : public QObject
{
    Q_OBJECT

private slots:
    void pixelEdgeFallbackAndCentrePreferenceAB()
    {
        for (const auto variant : {SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
            auto config = configFor(SstvAvtMode::Avt24);
            config.variant = variant;
            const SstvAvtMapper mapper({config.mode, kSampleRate, 0, variant});
            const auto centre = allObservations(config.mode, variant);
            auto edgeOnly = centre;
            std::vector<SstvFrequencyObservation> mixed;
            mixed.reserve(centre.size() * 2U);
            std::uint64_t sequence = 1U;
            for (std::size_t i = 0; i < centre.size(); ++i) {
                const auto position = mapper.positionAtSample(centre[i].centreSample - kImageStart);
                edgeOnly[i].centreSample = kImageStart + position.segmentStartSample;
                auto wrongEdge = edgeOnly[i];
                wrongEdge.sequence = sequence++;
                wrongEdge.rawFrequencyHz = wrongEdge.correctedFrequencyHz =
                    SstvAvtProtocol::frequencyForValue(0U, variant);
                mixed.push_back(wrongEdge);
                auto middle = centre[i];
                middle.sequence = sequence++;
                mixed.push_back(middle);
            }
            SstvAvtRxSession normal(config), sparse(config), preferred(config);
            consumeChunks(normal, centre, 257U);
            consumeChunks(sparse, edgeOnly, 257U);
            consumeChunks(preferred, mixed, 257U);
            QCOMPARE(normal.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(sparse.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(preferred.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(sparse.snapshot().pixels, normal.snapshot().pixels);
            QCOMPARE(preferred.snapshot().pixels, normal.snapshot().pixels);
            QCOMPARE(sparse.decoderMetrics().interpolatedPixels, std::uint64_t {0U});
        }
    }

    void finalPixelDoesNotDependOnChunkBoundary()
    {
        for (const auto variant : {SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
            auto config = configFor(SstvAvtMode::Avt24);
            config.variant = variant;
            config.observationSpanSamples = 6U;
            auto observations = allObservations(config.mode, variant);
            const auto end = kImageStart + SstvAvtMapper({config.mode, kSampleRate, 0, variant}).imageSampleCount();
            auto first = observations.back();
            first.centreSample = end - 3U;
            first.rawFrequencyHz = first.correctedFrequencyHz = SstvAvtProtocol::frequencyForValue(40U, variant);
            auto last = first;
            last.sequence++;
            last.centreSample = end - 1U;
            last.rawFrequencyHz = last.correctedFrequencyHz = SstvAvtProtocol::frequencyForValue(200U, variant);
            observations.back() = first;
            observations.push_back(last);
            SstvAvtRxSession contiguous(config), split(config);
            consumeChunks(contiguous, observations, 257U);
            consumeChunks(split, {observations.begin(), observations.end() - 1}, 257U);
            static_cast<void>(split.consume(&observations.back(), 1U));
            QCOMPARE(contiguous.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(split.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(contiguous.snapshot().pixels, split.snapshot().pixels);
        }
    }

    void variantsMatchNormalRasterThroughSession()
    {
        const auto mode = SstvAvtMode::Avt24;
        SstvAvtRxSession normal(configFor(mode));
        consumeChunks(normal, allObservations(mode), 257U);
        QCOMPARE(normal.finish(), SstvAvtRxSessionState::Complete);
        const auto expected = normal.snapshot();
        for (const auto variant : {SstvAvtVariant::Narrow, SstvAvtVariant::Qrm,
                                   SstvAvtVariant::NarrowQrm}) {
            auto config = configFor(mode);
            config.variant = variant;
            SstvAvtRxSession session(config);
            consumeChunks(session, allObservations(mode, variant), 257U);
            QCOMPARE(session.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(session.snapshot().pixels, expected.pixels);
        }
    }

    void completeFramesAreChunkInvariantAndBounded()
    {
        for (const SstvAvtMode mode : kModes) {
            const std::vector<SstvFrequencyObservation> observations =
                allObservations(mode);
            SstvAvtRxSession contiguous(configFor(mode));
            SstvAvtRxSession fragmented(configFor(mode));
            consumeChunks(contiguous,
                          observations,
                          SstvAvtRxSession::MaximumObservationsPerConsume);
            consumeChunks(fragmented, observations, 257U);
            QCOMPARE(contiguous.finish(), SstvAvtRxSessionState::Complete);
            QCOMPARE(fragmented.finish(), SstvAvtRxSessionState::Complete);
            const SstvImageSnapshot first = contiguous.snapshot();
            const SstvImageSnapshot second = fragmented.snapshot();
            QCOMPARE(first.width, second.width);
            QCOMPARE(first.height, second.height);
            QCOMPARE(first.pixels, second.pixels);
            QCOMPARE(first.channelCoverage, second.channelCoverage);
            QCOMPARE(contiguous.decoderMetrics().linesPublished,
                     static_cast<std::uint64_t>(first.height));
            QVERIFY(contiguous.metrics().peakInputObservations
                    <= SstvAvtRxSession::MaximumObservationsPerConsume);
        }
    }

    void discontinuityClosesPartialAndCancellationIsTerminal()
    {
        const auto observations = allObservations(SstvAvtMode::Avt24);
        const SstvAvtModeSpec spec = SstvAvtProtocol::spec(
            SstvAvtMode::Avt24);
        const std::size_t threeLines = static_cast<std::size_t>(spec.width)
            * 3U * 3U;
        SstvAvtRxSession partial(configFor(SstvAvtMode::Avt24));
        static_cast<void>(partial.consume(observations.data(), threeLines));
        QCOMPARE(partial.notifyDiscontinuity(
                     observations[threeLines].centreSample),
                 SstvAvtRxSessionState::Partial);
        QVERIFY(partial.snapshot().coverage() > 0.0);
        QVERIFY(partial.snapshot().coverage() < 1.0);
        QCOMPARE(partial.metrics().discontinuities, std::uint64_t {1U});

        SstvAvtRxSession cancelled(configFor(SstvAvtMode::Avt24));
        cancelled.cancel();
        QCOMPARE(cancelled.state(), SstvAvtRxSessionState::Cancelled);
        QVERIFY(cancelled.snapshot().cancelled);
        QCOMPARE(cancelled.consume(observations.data(), 1U).state,
                 SstvAvtRxSessionState::Cancelled);
    }

    void chronologicalRegressionsAndOversizeCallsFailClosed()
    {
        const auto observations = allObservations(SstvAvtMode::Avt24);
        SstvAvtRxSession session(configFor(SstvAvtMode::Avt24));
        static_cast<void>(session.consume(observations.data(), 2U));
        const SstvAvtRxSessionUpdate rejected = session.consume(
            observations.data() + 1U, 1U);
        QCOMPARE(rejected.decoderAcceptedObservations, std::size_t {0U});
        QCOMPARE(session.state(), SstvAvtRxSessionState::Receiving);
        QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {1U});

        std::vector<SstvFrequencyObservation> oversized(
            SstvAvtRxSession::MaximumObservationsPerConsume + 1U);
        QVERIFY_THROWS_EXCEPTION(std::length_error,
                                 session.consume(oversized));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 session.consume(nullptr, 1U));
    }

    void configurationRejectsUnsupportedBounds()
    {
        SstvAvtRxSessionConfig invalid = configFor(SstvAvtMode::Avt90);
        invalid.observationSpanSamples = 0U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvAvtRxSession {invalid});
        invalid = configFor(SstvAvtMode::Avt90);
        invalid.maximumInterpolationGapPixels =
            SstvAvtDecoder::MaximumInterpolationGapPixels + 1U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvAvtRxSession {invalid});
    }
};

QTEST_APPLESS_MAIN(TestSstvAvtRxSession)
#include "test_sstv_avt_rx_session.moc"
