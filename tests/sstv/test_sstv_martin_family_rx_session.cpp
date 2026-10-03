// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvMartinM1RxSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::uint64_t kImageStart = 70'000U;
constexpr std::array<SstvMartinMode, 3U> kModes {{
    SstvMartinMode::M2,
    SstvMartinMode::M3,
    SstvMartinMode::M4,
}};

SstvRgbPixel patternPixel(std::uint32_t x, std::uint32_t y)
{
    const auto red = static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(x) * 255U + 159U) / 319U);
    const auto green = static_cast<std::uint8_t>((y * 2U) & 0xffU);
    const auto blue = static_cast<std::uint8_t>(
        (static_cast<unsigned>(red) + static_cast<unsigned>(green)) / 2U);
    return {red, green, blue};
}

double pixelFrequency(const SstvMartinM1Position& position)
{
    const SstvRgbPixel pixel = patternPixel(position.pixel, position.line);
    switch (position.component) {
    case ColourComponent::Green:
        return SstvMartinM1Protocol::frequencyForValue(pixel.green);
    case ColourComponent::Blue:
        return SstvMartinM1Protocol::frequencyForValue(pixel.blue);
    case ColourComponent::Red:
        return SstvMartinM1Protocol::frequencyForValue(pixel.red);
    default:
        throw std::logic_error("invalid Martin session test component");
    }
}

std::vector<SstvFrequencyObservation> syntheticObservations(
    SstvMartinMode mode,
    std::uint64_t endRelativeSample,
    std::uint32_t stride = 1U)
{
    SstvMartinM1Mapper mapper({kSampleRate, 0, mode});
    endRelativeSample = std::min(endRelativeSample,
                                 mapper.imageSampleCount());
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(
        endRelativeSample / stride + 1U));
    std::uint64_t sequence = 1U;
    for (std::uint64_t relative = 0U;
         relative < endRelativeSample;
         relative += stride) {
        const SstvMartinM1Position position =
            mapper.positionAtSample(relative);
        double frequency = SstvMartinM1Protocol::SeparatorFrequencyHz;
        if (position.region == SstvMartinM1Region::Sync) {
            frequency = SstvMartinM1Protocol::SyncFrequencyHz;
        } else if (position.region == SstvMartinM1Region::Pixel) {
            frequency = pixelFrequency(position);
        }
        SstvFrequencyObservation observation;
        observation.status = SstvFrequencyStatus::Valid;
        observation.sequence = sequence++;
        observation.centreSample = kImageStart + relative;
        observation.rawFrequencyHz = frequency;
        observation.correctedFrequencyHz = frequency;
        observation.rms = 0.5;
        observation.snrDb = 35.0;
        observation.confidence = 0.98;
        observation.validSampleFraction = 1.0;
        result.push_back(observation);
    }
    return result;
}

SstvMartinM1RxSessionConfig sessionConfig(SstvMartinMode mode)
{
    SstvMartinM1RxSessionConfig config;
    config.sampleRate = kSampleRate;
    config.imageStartSample = kImageStart;
    config.observationSpanSamples = 1U;
    config.minimumObservationConfidence = 0.20;
    config.maximumPendingDirtyEvents = 16U;
    config.mode = mode;
    return config;
}

void consumeChunks(SstvMartinM1RxSession& session,
                   const std::vector<SstvFrequencyObservation>& input,
                   const std::vector<std::size_t>& pattern)
{
    std::size_t offset = 0U;
    std::size_t patternIndex = 0U;
    while (offset < input.size()) {
        const std::size_t count = std::min(
            pattern[patternIndex++ % pattern.size()],
            input.size() - offset);
        const auto update = session.consume(input.data() + offset, count);
        QCOMPARE(update.inputObservations, count);
        offset += count;
    }
}

void compareRepresentativePixels(const SstvImageSnapshot& snapshot,
                                 const SstvMartinModeSpec& spec,
                                 std::uint8_t tolerance)
{
    const std::array<std::uint32_t, 4U> rows {{
        0U, 1U, spec.height / 2U, spec.height - 1U,
    }};
    for (const std::uint32_t y : rows) {
        for (const std::uint32_t x : {0U, 1U, 79U, 159U, 318U}) {
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

class TestSstvMartinFamilyRxSession final : public QObject
{
    Q_OBJECT

private slots:
    void modeGeometryIsSeededAndFullFramesComplete()
    {
        for (const SstvMartinMode mode : kModes) {
            const SstvMartinModeSpec spec =
                SstvMartinM1Protocol::spec(mode);
            SstvMartinM1Mapper mapper({kSampleRate, 0, mode});
            const auto observations = syntheticObservations(
                mode, mapper.imageSampleCount());
            SstvMartinM1RxSession session(sessionConfig(mode));
            QCOMPARE(session.mode(), mode);
            QCOMPARE(session.snapshot().width, spec.width);
            QCOMPARE(session.snapshot().height, spec.height);
            QCOMPARE(session.decoderMetrics().storedSyncAnchors,
                     std::size_t {1U});
            consumeChunks(session,
                          observations,
                          {1U, 31U, 977U, 4'093U, 8'192U});
            QCOMPARE(session.state(),
                     SstvMartinM1RxSessionState::Complete);
            QCOMPARE(session.finish(),
                     SstvMartinM1RxSessionState::Complete);
            const auto snapshot = session.snapshot();
            QVERIFY(snapshot.isComplete());
            QCOMPARE(snapshot.completedPixels,
                     SstvMartinM1Encoder::pixelCount(mode));
            compareRepresentativePixels(snapshot, spec, 2U);
            QCOMPARE(session.decoderMetrics().linesPublished,
                     std::uint64_t {spec.height});
            QVERIFY(session.metrics().observedLineSyncs
                    >= static_cast<std::uint64_t>(spec.height - 2U));
            QVERIFY(session.metrics().peakFilteredObservations
                    <= SstvMartinM1RxSession::MaximumObservationsPerConsume);
            QVERIFY(session.takeDirtyEvents().size() <= 16U);
        }
    }

    void arbitraryChunkingPreservesPartialFrames()
    {
        for (const SstvMartinMode mode : kModes) {
            const SstvMartinModeSpec spec =
                SstvMartinM1Protocol::spec(mode);
            SstvMartinM1Mapper mapper({kSampleRate, 0, mode});
            const std::uint64_t end = mapper.lineStartSample(12U);
            const auto observations = syntheticObservations(mode, end);
            SstvMartinM1RxSession contiguous(sessionConfig(mode));
            SstvMartinM1RxSession fragmented(sessionConfig(mode));
            consumeChunks(contiguous, observations, {8'192U});
            consumeChunks(fragmented,
                          observations,
                          {1U, 17U, 251U, 4'093U, 7'999U, 3U});
            QCOMPARE(contiguous.notifyDiscontinuity(
                         kImageStart + end + 1U),
                     SstvMartinM1RxSessionState::Partial);
            QCOMPARE(fragmented.notifyDiscontinuity(
                         kImageStart + end + 1U),
                     SstvMartinM1RxSessionState::Partial);
            const auto first = contiguous.snapshot();
            const auto second = fragmented.snapshot();
            QCOMPARE(first.width, spec.width);
            QCOMPARE(first.height, spec.height);
            QCOMPARE(second.pixels, first.pixels);
            QCOMPARE(second.channelCoverage, first.channelCoverage);
            QVERIFY(first.coverage() > 0.0);
            QCOMPARE(contiguous.syncSnapshot().metrics.clockRegressions,
                     std::uint64_t {0U});
            QCOMPARE(fragmented.syncSnapshot().metrics.clockRegressions,
                     std::uint64_t {0U});
        }
    }

    void modeSpecificBoundsAndCancellationFailClosed()
    {
        auto invalid = sessionConfig(SstvMartinMode::M4);
        invalid.mode = static_cast<SstvMartinMode>(255U);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMartinM1RxSession {invalid});
        invalid = sessionConfig(SstvMartinMode::M4);
        invalid.imageStartSample =
            std::numeric_limits<std::uint64_t>::max();
        QVERIFY_THROWS_EXCEPTION(std::overflow_error,
                                 SstvMartinM1RxSession {invalid});

        SstvMartinM1Mapper mapper({kSampleRate, 0, SstvMartinMode::M4});
        const auto observations = syntheticObservations(
            SstvMartinMode::M4, mapper.lineStartSample(2U));
        SstvMartinM1RxSession session(sessionConfig(SstvMartinMode::M4));
        consumeChunks(session, observations, {1'024U});
        session.cancel();
        session.cancel();
        QCOMPARE(session.state(),
                 SstvMartinM1RxSessionState::Cancelled);
        QCOMPARE(session.metrics().cancelCalls, std::uint64_t {2U});
        QVERIFY(session.snapshot().cancelled);
        const auto before = session.decoderMetrics().observationInputs;
        const auto ignored = session.consume(observations.data(), 1U);
        QCOMPARE(ignored.decoderAcceptedObservations, std::size_t {0U});
        QCOMPARE(session.decoderMetrics().observationInputs, before);
    }
};

QTEST_APPLESS_MAIN(TestSstvMartinFamilyRxSession)

#include "test_sstv_martin_family_rx_session.moc"
