// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvScottieRxSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kRate = 12'000U;
constexpr std::uint64_t kImageStart = 75'000U;

const std::array<SstvScottieMode, 5U> kModes {{
    SstvScottieMode::S1,
    SstvScottieMode::S2,
    SstvScottieMode::S3,
    SstvScottieMode::S4,
    SstvScottieMode::DX,
}};

std::uint32_t observationStep(SstvScottieMode mode)
{
    switch (mode) {
    case SstvScottieMode::S1:
        return 3U;
    case SstvScottieMode::S2:
        return 2U;
    case SstvScottieMode::S3:
        return 3U;
    case SstvScottieMode::S4:
        return 2U;
    case SstvScottieMode::DX:
        return 6U;
    }
    throw std::logic_error("invalid Scottie test mode");
}

SstvRgbPixel patternPixel(std::uint32_t x, std::uint32_t y)
{
    const auto red = static_cast<std::uint8_t>(
        (static_cast<std::uint64_t>(x) * 255U + 159U) / 319U);
    const auto green = static_cast<std::uint8_t>(y);
    const auto blue = static_cast<std::uint8_t>(
        (static_cast<unsigned>(red) + static_cast<unsigned>(green)) / 2U);
    return {red, green, blue};
}

double pixelFrequency(const SstvScottiePosition& position)
{
    const SstvRgbPixel pixel = patternPixel(position.pixel, position.line);
    std::uint8_t value = 0U;
    switch (position.component) {
    case ColourComponent::Green:
        value = pixel.green;
        break;
    case ColourComponent::Blue:
        value = pixel.blue;
        break;
    case ColourComponent::Red:
        value = pixel.red;
        break;
    default:
        throw std::logic_error("non-pixel Scottie test component");
    }
    return SstvScottieProtocol::frequencyForValue(value);
}

SstvFrequencyObservation makeObservation(std::uint64_t sequence,
                                         std::uint64_t centre,
                                         double frequency)
{
    SstvFrequencyObservation observation;
    observation.status = SstvFrequencyStatus::Valid;
    observation.sequence = sequence;
    observation.centreSample = centre;
    observation.rawFrequencyHz = frequency;
    observation.correctedFrequencyHz = frequency;
    observation.afcCorrectionHz = 0.0;
    observation.rms = 0.5;
    observation.snrDb = 35.0;
    observation.confidence = 0.98;
    observation.validSampleFraction = 1.0;
    return observation;
}

std::vector<SstvFrequencyObservation> syntheticObservations(
    SstvScottieMode mode,
    std::int32_t clockErrorPpm,
    double frequencyOffsetHz,
    std::uint32_t step,
    std::uint64_t endRelativeSample = std::numeric_limits<std::uint64_t>::max(),
    bool suppressFirstSync = false)
{
    SstvScottieMapper mapper({mode, kRate, clockErrorPpm});
    const bool fullFrame = endRelativeSample ==
        std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t end = fullFrame
        ? mapper.imageSampleCount()
        : std::min(endRelativeSample, mapper.imageSampleCount());
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(end / step + 2U));
    std::uint64_t sequence = 0U;
    for (std::uint64_t relative = 0U; relative < end; relative += step) {
        const auto position = mapper.positionAtSample(relative);
        double frequency = SstvScottieProtocol::PorchFrequencyHz;
        if (position.region == SstvScottieRegion::Sync
            && !(suppressFirstSync && position.line == 0U)) {
            frequency = SstvScottieProtocol::SyncFrequencyHz;
        } else if (position.region == SstvScottieRegion::Pixel) {
            frequency = pixelFrequency(position);
        }
        result.push_back(makeObservation(
            sequence++, kImageStart + relative,
            frequency + frequencyOffsetHz));
    }
    if (fullFrame && !result.empty()
        && result.back().centreSample != kImageStart + end - 1U) {
        const std::uint64_t relative = end - 1U;
        const auto position = mapper.positionAtSample(relative);
        double frequency = SstvScottieProtocol::PorchFrequencyHz;
        if (position.region == SstvScottieRegion::Sync) {
            frequency = SstvScottieProtocol::SyncFrequencyHz;
        } else if (position.region == SstvScottieRegion::Pixel) {
            frequency = pixelFrequency(position);
        }
        result.push_back(makeObservation(
            sequence, kImageStart + relative,
            frequency + frequencyOffsetHz));
    }
    return result;
}

SstvScottieRxSessionConfig sessionConfig(SstvScottieMode mode,
                                         std::uint32_t step)
{
    SstvScottieRxSessionConfig config;
    config.mode = mode;
    config.sampleRate = kRate;
    config.imageStartSample = kImageStart;
    config.observationSpanSamples = step;
    config.maximumPendingDirtyEvents = 32U;
    return config;
}

void consumeChunks(SstvScottieRxSession& session,
                   const std::vector<SstvFrequencyObservation>& observations,
                   const std::vector<std::size_t>& pattern)
{
    const SstvScottieModeSpec spec = SstvScottieProtocol::spec(
        session.mode());
    std::size_t offset = 0U;
    std::size_t patternIndex = 0U;
    while (offset < observations.size()) {
        const std::size_t count = std::min(
            pattern[patternIndex++ % pattern.size()],
            observations.size() - offset);
        const auto update = session.consume(observations.data() + offset,
                                            count);
        QCOMPARE(update.inputObservations, count);
        QVERIFY(update.linesPublished <= spec.height);
        offset += count;
    }
}

void compareRepresentativePixels(const SstvImageSnapshot& snapshot,
                                 std::uint8_t tolerance)
{
    for (std::uint32_t y : {0U, 1U, 31U, 127U, 255U}) {
        if (y >= snapshot.height) {
            continue;
        }
        // Pixel zero shares its acquisition edge with a 1500 Hz porch; use
        // interior/near-terminal pixels for the value oracle and leave edge
        // coverage to the completeness assertion.
        for (std::uint32_t x : {1U, 2U, 79U, 159U, 318U}) {
            const auto actual = snapshot.pixel(x, y);
            const auto expected = patternPixel(x, y);
            const QString location = QStringLiteral("x=%1 y=%2 actual=%3/%4/%5 expected=%6/%7/%8")
                .arg(x).arg(y).arg(actual.red).arg(actual.green)
                .arg(actual.blue).arg(expected.red).arg(expected.green)
                .arg(expected.blue);
            QVERIFY2(std::abs(static_cast<int>(actual.red)
                              - static_cast<int>(expected.red))
                         <= tolerance,
                     qPrintable(location));
            QVERIFY2(std::abs(static_cast<int>(actual.green)
                              - static_cast<int>(expected.green))
                         <= tolerance,
                     qPrintable(location));
            QVERIFY2(std::abs(static_cast<int>(actual.blue)
                              - static_cast<int>(expected.blue))
                         <= tolerance,
                     qPrintable(location));
        }
    }
}

} // namespace

class TestSstvScottieRxSession final : public QObject
{
    Q_OBJECT

private slots:
    void validatesConfigurationAndDoesNotInventInitialSync();
    void fullFramesAreCompleteAndChunkInvariant();
    void calibrationAndSlantRemainOperational();
    void missingFirstSyncAndDiscontinuityStayPartialAndBounded();
    void regressionCancellationAndHostileBoundsAreExplicit();
};

void TestSstvScottieRxSession::
validatesConfigurationAndDoesNotInventInitialSync()
{
    auto invalid = sessionConfig(SstvScottieMode::S1, 1U);
    invalid.sampleRate = 8'000U;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvScottieRxSession {invalid});
    invalid = sessionConfig(SstvScottieMode::S1, 1U);
    invalid.mode = static_cast<SstvScottieMode>(255U);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             SstvScottieRxSession {invalid});
    invalid = sessionConfig(SstvScottieMode::S1, 1U);
    invalid.imageStartSample = std::numeric_limits<std::uint64_t>::max();
    QVERIFY_THROWS_EXCEPTION(std::overflow_error,
                             SstvScottieRxSession {invalid});

    const auto mode = SstvScottieMode::S2;
    SstvScottieMapper mapper({mode, kRate, 0});
    const std::uint64_t beforeSync = mapper.embeddedSyncStartSample(0U);
    const auto observations = syntheticObservations(
        mode, 0, 0.0, 1U, beforeSync);
    SstvScottieRxSession session(sessionConfig(mode, 1U));
    consumeChunks(session, observations, {1U, 17U, 4'093U});
    QCOMPARE(session.decoderMetrics().storedSyncAnchors, std::size_t {0U});
    QCOMPARE(session.syncSnapshot().metrics.explicitPulses,
             std::uint64_t {0U});
    QCOMPARE(session.snapshot().coverage(), 0.0);
    QVERIFY(session.metrics().peakPendingObservations > 1'000U);
    QVERIFY(session.metrics().peakPendingObservations
            <= session.metrics().pendingObservationCapacity);

    SstvScottieRxSession empty(sessionConfig(mode, 1U));
    QCOMPARE(empty.finish(), SstvScottieRxSessionState::Partial);
    QCOMPARE(empty.syncSnapshot().metrics.clockRegressions,
             std::uint64_t {0U});
}

void TestSstvScottieRxSession::fullFramesAreCompleteAndChunkInvariant()
{
    for (const SstvScottieMode mode : kModes) {
        const SstvScottieModeSpec spec = SstvScottieProtocol::spec(mode);
        const std::uint32_t step = observationStep(mode);
        const auto observations = syntheticObservations(
            mode, 0, 0.0, step);
        SstvScottieRxSession contiguous(sessionConfig(mode, step));
        consumeChunks(contiguous, observations, {8'192U});
        QCOMPARE(observations.back().centreSample,
                 contiguous.imageEndSample() - 1U);
        QCOMPARE(contiguous.state(), SstvScottieRxSessionState::Complete);
        const auto first = contiguous.snapshot();
        QVERIFY(first.isComplete());
        compareRepresentativePixels(first, 3U);

        SstvScottieRxSession fragmented(sessionConfig(mode, step));
        consumeChunks(fragmented, observations,
                      {1U, 19U, 257U, 4'091U, 7'999U, 3U});
        QCOMPARE(fragmented.state(), SstvScottieRxSessionState::Complete);
        const auto second = fragmented.snapshot();
        QCOMPARE(second.pixels, first.pixels);
        QCOMPARE(second.channelCoverage, first.channelCoverage);
        QCOMPARE(fragmented.decoderMetrics().linesPublished,
                 static_cast<std::uint64_t>(spec.height));
        QVERIFY(fragmented.metrics().observedLineSyncs
                >= static_cast<std::uint64_t>(spec.height - 6U));
        QVERIFY(fragmented.metrics().peakPendingObservations
                <= fragmented.metrics().pendingObservationCapacity);
        QVERIFY(fragmented.metrics().pendingObservationCapacity
                <= SstvScottieRxSession::MaximumPendingObservations);
        QCOMPARE(fragmented.syncSnapshot().metrics.clockRegressions,
                 std::uint64_t {0U});
    }
}

void TestSstvScottieRxSession::calibrationAndSlantRemainOperational()
{
    const auto mode = SstvScottieMode::S1;
    SstvScottieMapper mapper({mode, kRate, 300});
    const std::uint64_t end = mapper.lineStartSample(40U);
    for (const double offset : {100.0, -100.0}) {
        const auto observations = syntheticObservations(
            mode, 300, offset, 3U, end);
        auto config = sessionConfig(mode, 3U);
        config.frequencyOffsetHz = offset;
        // Leave the image mapper nominal: observed embedded syncs must still
        // drive the slant estimator and bounded progressive decode.
        SstvScottieRxSession session(config);
        consumeChunks(session, observations, {1U, 127U, 4'093U});
        const auto sync = session.syncSnapshot();
        QVERIFY(sync.slant.valid);
        QVERIFY(sync.clockErrorPpm > 150.0);
        QVERIFY(sync.clockErrorPpm < 450.0);
        QCOMPARE(session.notifyDiscontinuity(kImageStart + end + 1U),
                 SstvScottieRxSessionState::Partial);
        QVERIFY(session.metrics().observedLineSyncs >= 35U);
        QVERIFY(session.snapshot().coverage() > 0.12);
    }
}

void TestSstvScottieRxSession::
missingFirstSyncAndDiscontinuityStayPartialAndBounded()
{
    const auto mode = SstvScottieMode::S2;
    SstvScottieMapper mapper({mode, kRate, 0});
    const std::uint64_t end = mapper.lineStartSample(5U);
    const auto observations = syntheticObservations(
        mode, 0, 0.0, 2U, end, true);
    SstvScottieRxSession session(sessionConfig(mode, 2U));
    consumeChunks(session, observations, {1U, 31U, 1'003U});
    QCOMPARE(session.notifyDiscontinuity(kImageStart + end + 500U),
             SstvScottieRxSessionState::Partial);
    const auto image = session.snapshot();
    QVERIFY(!image.isScanlineComplete(0U));
    QVERIFY(image.isScanlineComplete(1U));
    QVERIFY(image.coverage() > 0.01);
    QVERIFY(image.coverage() < 0.04);

    // No sync at all rolls the observation replay window instead of growing.
    auto noSync = syntheticObservations(
        SstvScottieMode::DX, 0, 0.0, 1U,
        SstvScottieMapper({SstvScottieMode::DX, kRate, 0})
            .lineStartSample(3U));
    for (auto& observation : noSync) {
        observation.rawFrequencyHz = SstvScottieProtocol::PorchFrequencyHz;
        observation.correctedFrequencyHz =
            SstvScottieProtocol::PorchFrequencyHz;
    }
    SstvScottieRxSession bounded(
        sessionConfig(SstvScottieMode::DX, 1U));
    consumeChunks(bounded, noSync, {8'192U});
    QVERIFY(bounded.metrics().pendingObservationsEvicted > 0U);
    QCOMPARE(bounded.metrics().peakPendingObservations,
             bounded.metrics().pendingObservationCapacity);
    QVERIFY(bounded.metrics().peakPendingObservations
            <= SstvScottieRxSession::MaximumPendingObservations);
}

void TestSstvScottieRxSession::
regressionCancellationAndHostileBoundsAreExplicit()
{
    auto observations = syntheticObservations(
        SstvScottieMode::S1, 0, 0.0, 3U, 10'000U);
    QVERIFY(observations.size() > 12U);
    SstvScottieRxSession session(
        sessionConfig(SstvScottieMode::S1, 3U));
    session.consume(observations.data(), 10U);
    const auto before = session.decoderMetrics();
    auto stale = observations[9];
    session.consume(&stale, 1U);
    QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {1U});
    QCOMPARE(session.decoderMetrics().observationInputs,
             before.observationInputs);
    auto staleSequence = observations[10];
    staleSequence.sequence = observations[9].sequence;
    session.consume(&staleSequence, 1U);
    QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {2U});
    session.consume(observations.data() + 10U, 1U);
    QCOMPARE(session.metrics().rejectedRegressions, std::uint64_t {2U});

    SstvFrequencyObservation item;
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                             session.consume(nullptr, 1U));
    QVERIFY_THROWS_EXCEPTION(
        std::length_error,
        session.consume(
            &item,
            SstvScottieRxSession::MaximumObservationsPerConsume + 1U));
    session.cancel();
    session.cancel();
    QCOMPARE(session.state(), SstvScottieRxSessionState::Cancelled);
    QCOMPARE(session.metrics().cancelCalls, std::uint64_t {2U});
    QVERIFY(session.snapshot().cancelled);
    QCOMPARE(session.consume(observations.data(), 1U)
                 .decoderAcceptedObservations,
             std::size_t {0U});
}

QTEST_MAIN_WRAPPER(TestSstvScottieRxSession, static_cast<void>(0);)
#include "test_sstv_scottie_rx_session.moc"
