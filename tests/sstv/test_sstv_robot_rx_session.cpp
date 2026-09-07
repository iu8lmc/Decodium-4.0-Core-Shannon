// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvRobotRxSession.h"
#include "../../src/sstv/image/SstvColourConverter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kRate = 12'000U;
constexpr std::uint64_t kImageStart = 80'000U;

const std::array<SstvRobotMode, 8U> kModes {{
    SstvRobotMode::Colour12,
    SstvRobotMode::Colour24,
    SstvRobotMode::Colour36,
    SstvRobotMode::Colour72,
    SstvRobotMode::Bw8,
    SstvRobotMode::Bw12,
    SstvRobotMode::Bw24,
    SstvRobotMode::Bw36,
}};

SstvRgbPixel sourcePixel(const SstvRobotModeSpec& spec,
                         std::uint32_t x,
                         std::uint32_t y)
{
    const std::uint32_t blockX = x / 2U;
    const std::uint32_t blockY =
        spec.chromaSubsampling == ChromaSubsampling::Cs420 ? y / 2U : y;
    return {
        static_cast<std::uint8_t>((blockX * 11U + blockY * 5U + 23U) & 0xffU),
        static_cast<std::uint8_t>((blockX * 7U + blockY * 13U + 67U) & 0xffU),
        static_cast<std::uint8_t>((blockX * 3U + blockY * 19U + 149U) & 0xffU),
    };
}

std::uint8_t chromaValue(const SstvRobotModeSpec& spec,
                         const SstvRobotPosition& position)
{
    const SstvYCbCrPixel converted = SstvColourConverter::rgbToYCbCr(
        sourcePixel(spec, position.pixel * 2U, position.line));
    return position.component == ColourComponent::ChrominanceRed
        ? converted.chrominanceRed : converted.chrominanceBlue;
}

double positionFrequency(const SstvRobotModeSpec& spec,
                         const SstvRobotPosition& position)
{
    switch (position.region) {
    case SstvRobotRegion::Sync:
        return SstvRobotProtocol::SyncFrequencyHz;
    case SstvRobotRegion::ChromaMarker:
        return position.component == ColourComponent::ChrominanceRed
            ? SstvRobotProtocol::CrMarkerFrequencyHz
            : SstvRobotProtocol::CbMarkerFrequencyHz;
    case SstvRobotRegion::MarkerPorch:
        return SstvRobotProtocol::MarkerPorchFrequencyHz;
    case SstvRobotRegion::Pixel:
        if (position.component == ColourComponent::Luminance
            || position.component == ColourComponent::Gray) {
            return SstvRobotProtocol::frequencyForValue(
                SstvColourConverter::rgbToGrayscale(
                    sourcePixel(spec, position.pixel, position.line)));
        }
        return SstvRobotProtocol::frequencyForValue(
            chromaValue(spec, position));
    case SstvRobotRegion::Outside:
    case SstvRobotRegion::Complete:
        break;
    }
    throw std::logic_error("invalid Robot session test position");
}

SstvFrequencyObservation makeObservation(std::uint64_t absolute,
                                         std::uint64_t sequence,
                                         double frequency)
{
    SstvFrequencyObservation observation;
    observation.status = SstvFrequencyStatus::Valid;
    observation.sequence = sequence;
    observation.centreSample = absolute;
    observation.rawFrequencyHz = frequency;
    observation.correctedFrequencyHz = frequency;
    observation.rms = 0.5;
    observation.snrDb = 35.0;
    observation.confidence = 0.99;
    observation.validSampleFraction = 1.0;
    return observation;
}

std::vector<SstvFrequencyObservation> observationsForLine(
    const SstvRobotModeSpec& spec,
    const SstvRobotMapper& mapper,
    std::uint32_t line,
    std::uint64_t imageStart,
    std::uint64_t& sequence)
{
    const std::uint64_t begin = mapper.lineStartSample(line);
    const std::uint64_t end = mapper.lineEndSample(line);
    std::vector<SstvFrequencyObservation> observations;
    observations.reserve(static_cast<std::size_t>(end - begin));
    for (std::uint64_t relative = begin; relative < end; ++relative) {
        const SstvRobotPosition position = mapper.positionAtSample(relative);
        observations.push_back(makeObservation(
            imageStart + relative,
            sequence++,
            positionFrequency(spec, position)));
    }
    return observations;
}

SstvRobotRxSessionConfig sessionConfig(SstvRobotMode mode,
                                       std::uint64_t imageStart = kImageStart,
                                       std::int32_t ppm = 0)
{
    SstvRobotRxSessionConfig config;
    config.mode = mode;
    config.sampleRate = kRate;
    config.imageStartSample = imageStart;
    config.observationSpanSamples = 1U;
    config.clockErrorPpm = ppm;
    config.minimumObservationConfidence = 0.20;
    config.maximumPendingDirtyEvents = 16U;
    return config;
}

void consumeLines(SstvRobotRxSession& session,
                  SstvRobotMode mode,
                  std::uint32_t firstLine,
                  std::uint32_t endLine,
                  std::uint64_t imageStart,
                  std::int32_t ppm,
                  std::uint64_t& sequence)
{
    const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
    const SstvRobotMapper mapper({mode, kRate, ppm});
    for (std::uint32_t line = firstLine; line < endLine; ++line) {
        const auto observations = observationsForLine(
            spec, mapper, line, imageStart, sequence);
        QVERIFY(observations.size()
                <= SstvRobotRxSession::MaximumObservationsPerConsume);
        const SstvRobotRxSessionUpdate update = session.consume(observations);
        QCOMPARE(update.inputObservations, observations.size());
    }
}

void compareRepresentativePixels(const SstvImageSnapshot& snapshot,
                                 const SstvRobotModeSpec& spec)
{
    for (const std::uint32_t y :
         {0U, 1U, spec.height / 2U, spec.height - 1U}) {
        for (const std::uint32_t x :
             {0U, 1U, spec.width / 2U, spec.width - 1U}) {
            const SstvRgbPixel source = sourcePixel(spec, x, y);
            const SstvRgbPixel expected = spec.colour
                ? SstvColourConverter::yCbCrToRgb(
                      SstvColourConverter::rgbToYCbCr(source))
                : SstvColourConverter::grayscaleToRgb(
                      SstvColourConverter::rgbToGrayscale(source));
            const SstvRgbPixel actual = snapshot.pixel(x, y);
            QVERIFY(std::abs(static_cast<int>(actual.red)
                             - static_cast<int>(expected.red)) <= 2);
            QVERIFY(std::abs(static_cast<int>(actual.green)
                             - static_cast<int>(expected.green)) <= 2);
            QVERIFY(std::abs(static_cast<int>(actual.blue)
                             - static_cast<int>(expected.blue)) <= 2);
        }
    }
}

} // namespace

class TestSstvRobotRxSession final : public QObject
{
    Q_OBJECT

private slots:
    void allModesCompleteThroughBoundedSyncSessions()
    {
        for (const SstvRobotMode mode : kModes) {
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
            const SstvRobotMapper mapper({mode, kRate, 0});
            SstvRobotRxSession session(sessionConfig(mode));
            QCOMPARE(session.mode(), mode);
            QCOMPARE(session.snapshot().width, spec.width);
            QCOMPARE(session.snapshot().height, spec.height);
            QCOMPARE(session.decoderMetrics().storedSyncAnchors,
                     std::size_t {1U});
            std::uint64_t sequence = 1U;
            consumeLines(session,
                         mode,
                         0U,
                         spec.height,
                         kImageStart,
                         0,
                         sequence);
            QCOMPARE(session.imageEndSample(),
                     kImageStart + mapper.imageSampleCount());
            QCOMPARE(session.state(), SstvRobotRxSessionState::Complete);
            QCOMPARE(session.finish(), SstvRobotRxSessionState::Complete);
            const SstvImageSnapshot snapshot = session.snapshot();
            QVERIFY(snapshot.isComplete());
            QCOMPARE(session.decoderMetrics().linesPublished,
                     std::uint64_t {spec.height});
            QCOMPARE(snapshot.completedPixels,
                     SstvRobotEncoder::pixelCount(mode));
            compareRepresentativePixels(snapshot, spec);
            QVERIFY(session.metrics().peakFilteredObservations
                    <= SstvRobotRxSession::MaximumObservationsPerConsume);
            QVERIFY(session.takeDirtyEvents().size() <= 16U);
        }
    }

    void clockErrorPartialAndDiscontinuityStayBounded()
    {
        constexpr std::int32_t ppm = 1'200;
        const SstvRobotMode mode = SstvRobotMode::Colour36;
        const SstvRobotMapper mapper({mode, kRate, ppm});
        SstvRobotRxSession session(sessionConfig(mode, kImageStart, ppm));
        std::uint64_t sequence = 1U;
        consumeLines(session, mode, 0U, 8U, kImageStart, ppm, sequence);
        QCOMPARE(session.notifyDiscontinuity(
                     kImageStart + mapper.lineStartSample(8U) + 1U),
                 SstvRobotRxSessionState::Partial);
        const SstvImageSnapshot snapshot = session.snapshot();
        QVERIFY(snapshot.coverage() > 0.0);
        QVERIFY(snapshot.coverage() < 1.0);
        QCOMPARE(session.metrics().discontinuities, std::uint64_t {1U});
        QCOMPARE(session.syncSnapshot().metrics.clockRegressions,
                 std::uint64_t {0U});
    }

    void cancellationAndRegressionFailClosed()
    {
        SstvRobotRxSession session(sessionConfig(SstvRobotMode::Bw12));
        SstvFrequencyObservation first = makeObservation(
            kImageStart, 2U, SstvRobotProtocol::SyncFrequencyHz);
        SstvFrequencyObservation regressed = first;
        regressed.sequence = 1U;
        regressed.centreSample = kImageStart - 1U;
        const std::array<SstvFrequencyObservation, 2U> input {{
            first, regressed}};
        const SstvRobotRxSessionUpdate update = session.consume(
            input.data(), input.size());
        QCOMPARE(update.decoderAcceptedObservations, std::size_t {0U});
        QCOMPARE(session.metrics().rejectedRegressions,
                 std::uint64_t {1U});
        session.cancel();
        QCOMPARE(session.state(), SstvRobotRxSessionState::Cancelled);
        QVERIFY(session.snapshot().cancelled);
        session.cancel();
        QCOMPARE(session.metrics().cancelCalls, std::uint64_t {2U});
    }

    void backToBackInstancesDoNotShareFrameState()
    {
        const SstvRobotMode mode = SstvRobotMode::Bw8;
        const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
        const SstvRobotMapper firstMapper({mode, kRate, 0});
        const std::uint64_t secondStart = kImageStart
            + firstMapper.imageSampleCount() + 10'920U;
        SstvRobotRxSession first(sessionConfig(mode, kImageStart));
        SstvRobotRxSession second(sessionConfig(mode, secondStart));
        std::uint64_t firstSequence = 1U;
        std::uint64_t secondSequence = 1U;
        consumeLines(first,
                     mode,
                     0U,
                     spec.height,
                     kImageStart,
                     0,
                     firstSequence);
        consumeLines(second,
                     mode,
                     0U,
                     spec.height,
                     secondStart,
                     0,
                     secondSequence);
        QCOMPARE(first.state(), SstvRobotRxSessionState::Complete);
        QCOMPARE(second.state(), SstvRobotRxSessionState::Complete);
        QCOMPARE(first.snapshot().pixels, second.snapshot().pixels);
        QCOMPARE(first.metrics().consumeCalls,
                 static_cast<std::uint64_t>(spec.height));
        QCOMPARE(second.metrics().consumeCalls,
                 static_cast<std::uint64_t>(spec.height));
    }

    void invalidConfigsAreRejected()
    {
        auto config = sessionConfig(SstvRobotMode::Colour36);
        config.sampleRate = 11'999U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 static_cast<void>(
                                     SstvRobotRxSession {config}));
        config = sessionConfig(SstvRobotMode::Colour36);
        config.clockErrorPpm = 100'001;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 static_cast<void>(
                                     SstvRobotRxSession {config}));
        config = sessionConfig(SstvRobotMode::Colour36);
        config.mode = static_cast<SstvRobotMode>(255U);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 static_cast<void>(
                                     SstvRobotRxSession {config}));
    }
};

QTEST_APPLESS_MAIN(TestSstvRobotRxSession)

#include "test_sstv_robot_rx_session.moc"
