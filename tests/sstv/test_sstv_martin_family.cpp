// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvMartinM1.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::array<SstvMartinMode, 3U> kNewModes {{
    SstvMartinMode::M2,
    SstvMartinMode::M3,
    SstvMartinMode::M4,
}};

std::uint64_t unsignedDuration(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("negative Martin duration in test");
    }
    return static_cast<std::uint64_t>(duration.count);
}

std::uint64_t oracleSamples(std::uint64_t picoseconds,
                            std::uint32_t sampleRate,
                            std::int32_t ppm)
{
    const long double samples =
        static_cast<long double>(picoseconds)
        * static_cast<long double>(sampleRate)
        * (1.0L + static_cast<long double>(ppm) / 1.0e6L)
        / 1.0e12L;
    return static_cast<std::uint64_t>(std::floor(samples));
}

SstvRgbPixel testPixel(std::uint32_t x, std::uint32_t y)
{
    return {
        static_cast<std::uint8_t>((x * 13U + y * 7U + 31U) & 0xffU),
        static_cast<std::uint8_t>((x * 5U + y * 19U + 73U) & 0xffU),
        static_cast<std::uint8_t>((255U - x + y * 11U) & 0xffU),
    };
}

std::vector<SstvRgbPixel> testImage(SstvMartinMode mode)
{
    const SstvMartinModeSpec spec = SstvMartinM1Protocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvMartinM1Encoder::pixelCount(mode));
    for (std::uint32_t y = 0U; y < spec.height; ++y) {
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            pixels[static_cast<std::size_t>(y) * spec.width + x] =
                testPixel(x, y);
        }
    }
    return pixels;
}

std::uint8_t componentValue(const SstvRgbPixel& pixel,
                            ColourComponent component)
{
    switch (component) {
    case ColourComponent::Green:
        return pixel.green;
    case ColourComponent::Blue:
        return pixel.blue;
    case ColourComponent::Red:
        return pixel.red;
    default:
        throw std::logic_error("invalid Martin component in test");
    }
}

std::array<std::uint64_t, 3U> componentStarts(
    const SstvMartinModeSpec& spec)
{
    const std::uint64_t sync = unsignedDuration(spec.syncDuration);
    const std::uint64_t porch = unsignedDuration(spec.porchDuration);
    const std::uint64_t separator =
        unsignedDuration(spec.separatorDuration);
    const std::uint64_t component =
        unsignedDuration(spec.componentDuration);
    return {{
        sync + porch,
        sync + porch + component + separator,
        sync + porch + 2U * component + 2U * separator,
    }};
}

ColourComponent componentAt(std::size_t index)
{
    switch (index) {
    case 0U:
        return ColourComponent::Green;
    case 1U:
        return ColourComponent::Blue;
    case 2U:
        return ColourComponent::Red;
    default:
        throw std::logic_error("invalid Martin component index in test");
    }
}

SstvFrequencyObservation frequencyObservation(std::uint64_t centreSample,
                                              double frequencyHz)
{
    SstvFrequencyObservation result;
    result.status = SstvFrequencyStatus::Valid;
    result.centreSample = centreSample;
    result.rawFrequencyHz = frequencyHz;
    result.correctedFrequencyHz = frequencyHz;
    result.confidence = 1.0;
    result.validSampleFraction = 1.0;
    return result;
}

std::vector<SstvFrequencyObservation> observationsForLine(
    const std::vector<SstvRgbPixel>& pixels,
    const SstvMartinModeSpec& spec,
    std::uint32_t line,
    std::uint64_t absoluteImageStart,
    std::uint32_t sampleRate,
    std::int32_t ppm,
    double frequencyOffset)
{
    const auto starts = componentStarts(spec);
    const std::uint64_t pixelDuration =
        unsignedDuration(spec.pixelDuration);
    const std::uint64_t lineTime = static_cast<std::uint64_t>(line)
        * unsignedDuration(spec.lineDuration);
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(spec.width) * 3U);
    for (std::size_t componentIndex = 0U;
         componentIndex < starts.size();
         ++componentIndex) {
        const ColourComponent component = componentAt(componentIndex);
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            const std::uint64_t startTime = lineTime
                + starts[componentIndex]
                + static_cast<std::uint64_t>(x) * pixelDuration;
            const std::uint64_t endTime = startTime + pixelDuration;
            const std::uint64_t startSample = oracleSamples(
                startTime, sampleRate, ppm);
            const std::uint64_t endSample = oracleSamples(
                endTime, sampleRate, ppm);
            if (endSample <= startSample) {
                throw std::logic_error("Martin test pixel rounded to zero");
            }
            const auto& pixel = pixels[static_cast<std::size_t>(line)
                                           * spec.width
                                       + x];
            result.push_back(frequencyObservation(
                absoluteImageStart + startSample
                    + (endSample - startSample) / 2U,
                SstvMartinM1Protocol::frequencyForValue(
                    componentValue(pixel, component))
                    + frequencyOffset));
        }
    }
    return result;
}

std::array<double, SstvMartinM1Encoder::HeaderSegmentCount>
expectedHeader(std::uint8_t payload)
{
    std::array<double, SstvMartinM1Encoder::HeaderSegmentCount> result {};
    result[0] = 1'900.0;
    result[1] = 1'200.0;
    result[2] = 1'900.0;
    result[3] = 1'200.0;
    unsigned ones = 0U;
    for (std::size_t bit = 0U; bit < 7U; ++bit) {
        const bool one = ((payload >> bit) & 1U) != 0U;
        ones += static_cast<unsigned>(one);
        result[4U + bit] = one ? 1'100.0 : 1'300.0;
    }
    result[11] = (ones & 1U) != 0U ? 1'100.0 : 1'300.0;
    result[12] = 1'200.0;
    return result;
}

std::vector<float> renderPrefix(SstvMartinM1Encoder& encoder,
                                std::size_t count,
                                bool fragmented)
{
    std::vector<float> result(count);
    std::uint32_t random = 0x41c6ce57U;
    std::size_t offset = 0U;
    while (offset < result.size()) {
        random = random * 1'664'525U + 1'013'904'223U;
        const std::size_t requested = fragmented
            ? 1U + random % 997U
            : result.size() - offset;
        const std::size_t amount = std::min(
            requested, result.size() - offset);
        const std::size_t produced = encoder.pullFloat(
            result.data() + offset, amount);
        if (produced == 0U) {
            throw std::runtime_error("Martin encoder made no progress");
        }
        offset += produced;
    }
    return result;
}

SstvMartinMode modeForId(const QString& id)
{
    if (id == QStringLiteral("martin-m2")) {
        return SstvMartinMode::M2;
    }
    if (id == QStringLiteral("martin-m3")) {
        return SstvMartinMode::M3;
    }
    if (id == QStringLiteral("martin-m4")) {
        return SstvMartinMode::M4;
    }
    throw std::invalid_argument("unexpected Martin fixture id");
}

} // namespace

class TestSstvMartinFamily final : public QObject
{
    Q_OBJECT

private slots:
    void canonicalFactsResolveHistoricalConflicts()
    {
        const auto m1 = SstvMartinM1Protocol::spec(SstvMartinMode::M1);
        const auto m2 = SstvMartinM1Protocol::spec(SstvMartinMode::M2);
        const auto m3 = SstvMartinM1Protocol::spec(SstvMartinMode::M3);
        const auto m4 = SstvMartinM1Protocol::spec(SstvMartinMode::M4);

        QCOMPARE(m2.visPayload, std::uint8_t {40U});
        QCOMPARE(m3.visPayload, std::uint8_t {36U});
        QCOMPARE(m4.visPayload, std::uint8_t {32U});
        QCOMPARE(m2.height, std::uint32_t {256U});
        QCOMPARE(m3.height, std::uint32_t {128U});
        QCOMPARE(m4.height, std::uint32_t {128U});
        QCOMPARE(m2.width, std::uint32_t {320U});
        QCOMPARE(m2.effectiveSampledWidth, std::uint32_t {160U});
        QCOMPARE(m3.effectiveSampledWidth, std::uint32_t {320U});
        QCOMPARE(m4.effectiveSampledWidth, std::uint32_t {160U});
        QCOMPARE(m2.pixelDuration.count, std::int64_t {228'800'000LL});
        QCOMPARE(m3.pixelDuration.count, m1.pixelDuration.count);
        QCOMPARE(m4.pixelDuration.count, m2.pixelDuration.count);
        QCOMPARE(m2.lineDuration.count, std::int64_t {226'798'000'000LL});
        QCOMPARE(m3.lineDuration.count, m1.lineDuration.count);
        QCOMPARE(m4.lineDuration.count, m2.lineDuration.count);
        QCOMPARE(m2.imageDuration.count,
                 std::int64_t {58'060'288'000'000LL});
        QCOMPARE(m3.imageDuration.count,
                 std::int64_t {57'145'088'000'000LL});
        QCOMPARE(m4.imageDuration.count,
                 std::int64_t {29'030'144'000'000LL});

        // SlowRX's M3 pixel field is 228.8 us despite its M1-length line.
        // Handbook table 4.4 and pinned libsstv instead agree on 457.6 us.
        QVERIFY(m3.pixelDuration.count != 228'800'000LL);
        for (const SstvMartinMode mode : kNewModes) {
            const SstvMartinModeSpec spec =
                SstvMartinM1Protocol::spec(mode);
            QCOMPARE(spec.componentDuration.count,
                     spec.pixelDuration.count
                         * static_cast<std::int64_t>(spec.width));
            QCOMPARE(spec.lineDuration.count,
                     spec.syncDuration.count + spec.porchDuration.count
                         + 3LL * spec.componentDuration.count
                         + 3LL * spec.separatorDuration.count);
            QCOMPARE(spec.imageDuration.count,
                     spec.lineDuration.count
                         * static_cast<std::int64_t>(spec.height));
        }
    }

    void pinnedLibsstvLandmarksMatchNativeTimingModel()
    {
#ifndef DECODIUM_SSTV_MARTIN_LIBSSTV_FIXTURE
        QFAIL("pinned Martin libsstv fixture path was not configured");
#else
        QFile fixture(QString::fromUtf8(
            DECODIUM_SSTV_MARTIN_LIBSSTV_FIXTURE));
        QVERIFY2(fixture.open(QIODevice::ReadOnly),
                 qPrintable(fixture.errorString()));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            fixture.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        const QJsonObject root = document.object();
        QCOMPARE(root.value(QStringLiteral("producer")).toString(),
                 QStringLiteral("rimio/libsstv"));
        QCOMPARE(root.value(QStringLiteral("producerCommit")).toString(),
                 QStringLiteral(
                     "193157a993ac34bfa074074004c9ddadcfe6fd15"));
        QCOMPARE(root.value(QStringLiteral("sampleRate")).toInt(), 12'000);
        QVERIFY(root.value(QStringLiteral("generationScope")).toString()
                    .contains(QStringLiteral("no Decodium runtime dependency")));
        const QJsonArray modes = root.value(QStringLiteral("modes")).toArray();
        QCOMPARE(modes.size(), 3);
        constexpr std::uint64_t headerFrames = 10'920U;
        for (const QJsonValue& value : modes) {
            const QJsonObject item = value.toObject();
            const SstvMartinMode mode = modeForId(
                item.value(QStringLiteral("id")).toString());
            const SstvMartinModeSpec spec =
                SstvMartinM1Protocol::spec(mode);
            QCOMPARE(item.value(QStringLiteral("visPayload")).toInt(),
                     static_cast<int>(spec.visPayload));
            QCOMPARE(item.value(QStringLiteral("width")).toInt(),
                     static_cast<int>(spec.width));
            QCOMPARE(item.value(QStringLiteral("height")).toInt(),
                     static_cast<int>(spec.height));
            QCOMPARE(item.value(QStringLiteral("effectiveSampledWidth"))
                         .toInt(),
                     static_cast<int>(spec.effectiveSampledWidth));
            QCOMPARE(item.value(QStringLiteral("pixelDurationNanoseconds"))
                         .toInteger() * 1'000LL,
                     spec.pixelDuration.count);
            QCOMPARE(item.value(QStringLiteral("componentDurationNanoseconds"))
                         .toInteger() * 1'000LL,
                     spec.componentDuration.count);
            SstvMartinM1Mapper mapper({12'000U, 0, mode});
            QCOMPARE(static_cast<std::uint64_t>(item.value(
                         QStringLiteral("fullPcmFrames")).toInteger()),
                     headerFrames + mapper.imageSampleCount());
            QCOMPARE(item.value(QStringLiteral("fullPcmSha256"))
                         .toString().size(),
                     64);

            const auto starts = componentStarts(spec);
            const QJsonArray landmarks = item.value(
                QStringLiteral("landmarks")).toArray();
            QCOMPARE(landmarks.size(), 5);
            for (const QJsonValue& landmarkValue : landmarks) {
                const QJsonObject landmark = landmarkValue.toObject();
                const QString name = landmark.value(
                    QStringLiteral("name")).toString();
                std::uint64_t expected = 0U;
                if (name == QStringLiteral("first-sync-start")) {
                    expected = headerFrames;
                } else if (name == QStringLiteral("first-green-start")) {
                    expected = headerFrames
                        + oracleSamples(starts[0], 12'000U, 0);
                } else if (name == QStringLiteral("first-blue-start")) {
                    expected = headerFrames
                        + oracleSamples(starts[1], 12'000U, 0);
                } else if (name == QStringLiteral("first-red-start")) {
                    expected = headerFrames
                        + oracleSamples(starts[2], 12'000U, 0);
                } else if (name == QStringLiteral("second-line-start")) {
                    expected = headerFrames + mapper.lineStartSample(1U);
                } else {
                    QFAIL("unexpected Martin libsstv landmark");
                }
                QCOMPARE(static_cast<std::uint64_t>(landmark.value(
                             QStringLiteral("frame")).toInteger()),
                         expected);
            }
        }
#endif
    }

    void mapperPreservesGbrLayoutAndExplicitClockError()
    {
        for (const SstvMartinMode mode : kNewModes) {
            const auto spec = SstvMartinM1Protocol::spec(mode);
            const auto starts = componentStarts(spec);
            SstvMartinM1Mapper mapper({12'000U, 0, mode});
            auto position = mapper.positionAtElapsedTime(Picoseconds {0});
            QCOMPARE(position.region, SstvMartinM1Region::Sync);
            QCOMPARE(position.line, std::uint32_t {0U});

            for (std::size_t index = 0U; index < starts.size(); ++index) {
                position = mapper.positionAtElapsedTime(Picoseconds {
                    static_cast<std::int64_t>(starts[index])});
                QCOMPARE(position.region, SstvMartinM1Region::Pixel);
                QCOMPARE(position.component, componentAt(index));
                QCOMPARE(position.pixel, std::uint32_t {0U});
            }
            position = mapper.positionAtElapsedTime(Picoseconds {
                spec.lineDuration.count});
            QCOMPARE(position.region, SstvMartinM1Region::Sync);
            QCOMPARE(position.line, std::uint32_t {1U});
            QCOMPARE(mapper.imageSampleCount(),
                     oracleSamples(unsignedDuration(spec.imageDuration),
                                   12'000U,
                                   0));

            for (const std::int32_t ppm : {-300, 300}) {
                SstvMartinM1Mapper corrected({44'100U, ppm, mode});
                QCOMPARE(corrected.lineStartSample(spec.height),
                         oracleSamples(unsignedDuration(spec.imageDuration),
                                       44'100U,
                                       ppm));
                QCOMPARE(corrected.imageSampleCount(),
                         corrected.lineStartSample(spec.height));
            }
        }
    }

    void encoderUsesModeVisAndIsChunkInvariantAndBounded()
    {
        const std::array<std::size_t,
                         SstvMartinM1Encoder::HeaderSegmentCount>
            headerSamples {{
                3'600U, 120U, 3'600U,
                360U, 360U, 360U, 360U, 360U,
                360U, 360U, 360U, 360U, 360U,
            }};
        for (const SstvMartinMode mode : kNewModes) {
            const auto spec = SstvMartinM1Protocol::spec(mode);
            const auto pixels = testImage(mode);
            SstvMartinM1EncoderConfig config;
            config.mode = mode;
            SstvMartinM1Encoder headerEncoder(pixels, config);
            const auto frequencies = expectedHeader(spec.visPayload);
            std::vector<float> scratch(3'600U);
            for (std::size_t index = 0U;
                 index < frequencies.size();
                 ++index) {
                const auto position = headerEncoder.position();
                QCOMPARE(position.stage, SstvMartinM1EncoderStage::Header);
                QCOMPARE(position.headerSegment, index);
                QVERIFY(std::abs(position.frequencyHz - frequencies[index])
                        < 1.0e-12);
                QCOMPARE(headerEncoder.pullFloat(scratch.data(),
                                                 headerSamples[index]),
                         headerSamples[index]);
            }
            QCOMPARE(headerEncoder.producedSamples(), std::uint64_t {10'920U});
            QCOMPARE(headerEncoder.position().image.region,
                     SstvMartinM1Region::Sync);
            QCOMPARE(headerEncoder.totalSamples(),
                     std::uint64_t {10'920U}
                         + oracleSamples(unsignedDuration(spec.imageDuration),
                                         12'000U,
                                         0));

            SstvMartinM1Encoder contiguous(pixels, config);
            SstvMartinM1Encoder fragmented(pixels, config);
            const auto first = renderPrefix(contiguous, 80'000U, false);
            const auto second = renderPrefix(fragmented, 80'000U, true);
            QVERIFY(first == second);
            QCOMPARE(contiguous.metrics().residentImageBytes,
                     SstvMartinM1Encoder::pixelCount(mode)
                         * sizeof(SstvRgbPixel));
            QVERIFY(contiguous.metrics().segmentTransitions > 1'000U);
            QVERIFY(contiguous.metrics().tone.peakAfterClamp
                    <= kDefaultSstvTxHeadroom + 1.0e-6);
        }

        SstvMartinM1EncoderConfig m4Config;
        m4Config.mode = SstvMartinMode::M4;
        const auto m4Pixels = testImage(SstvMartinMode::M4);
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMartinM1Encoder(m4Pixels.data(),
                                m4Pixels.size() - 1U,
                                m4Config));
        SstvMartinM1Encoder encoder(m4Pixels, m4Config);
        float sample = 0.0F;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullFloat(
                &sample, SstvMartinM1Encoder::MaximumSamplesPerPull + 1U));
        encoder.cancel();
        QCOMPARE(encoder.pullFloat(&sample, 1U), std::size_t {0U});
        encoder.reset();
        QCOMPARE(encoder.mode(), SstvMartinMode::M4);
        QCOMPARE(encoder.pullFloat(&sample, 1U), std::size_t {1U});
    }

    void decoderCompletesEveryModeWithBoundedStateAndClockError()
    {
        constexpr std::uint64_t base = 80'000U;
        constexpr std::int32_t ppm = 300;
        constexpr double offset = 75.0;
        for (const SstvMartinMode mode : kNewModes) {
            const auto spec = SstvMartinM1Protocol::spec(mode);
            const auto pixels = testImage(mode);
            SstvMartinM1DecoderConfig config;
            config.clockErrorPpm = ppm;
            config.frequencyOffsetHz = offset;
            config.maximumPendingDirtyEvents = 8U;
            config.mode = mode;
            SstvMartinM1Decoder decoder(config);
            QCOMPARE(decoder.mode(), mode);
            for (std::uint32_t line = 0U; line < spec.height; ++line) {
                const SstvMartinM1LineSync sync {
                    line,
                    base + oracleSamples(
                        static_cast<std::uint64_t>(line)
                            * unsignedDuration(spec.lineDuration),
                        12'000U,
                        ppm),
                    (line & 7U) == 3U ? 0.65 : 0.95,
                    (line & 7U) == 3U};
                QCOMPARE(decoder.consumeLineSyncs(&sync, 1U),
                         std::size_t {1U});
                const auto observations = observationsForLine(
                    pixels,
                    spec,
                    line,
                    base,
                    12'000U,
                    ppm,
                    offset);
                QCOMPARE(decoder.consume(observations), observations.size());
            }
            QCOMPARE(decoder.finish(), SstvMartinM1DecodeState::Complete);
            const auto snapshot = decoder.snapshot();
            QCOMPARE(snapshot.width, spec.width);
            QCOMPARE(snapshot.height, spec.height);
            QVERIFY(snapshot.isComplete());
            QCOMPARE(snapshot.completedPixels,
                     SstvMartinM1Encoder::pixelCount(mode));
            QVERIFY(snapshot.pixel(0U, 0U) == testPixel(0U, 0U));
            QVERIFY(snapshot.pixel(159U, spec.height / 2U)
                    == testPixel(159U, spec.height / 2U));
            QVERIFY(snapshot.pixel(319U, spec.height - 1U)
                    == testPixel(319U, spec.height - 1U));
            QCOMPARE(decoder.metrics().linesPublished,
                     std::uint64_t {spec.height});
            QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                    <= SstvMartinM1Decoder::MaximumBufferedPixelAccumulators);
            QVERIFY(decoder.takeDirtyEvents().size() <= 8U);
        }
    }

    void encoderStreamsEveryLogicalDurationWithoutWaveformStorage()
    {
        for (const SstvMartinMode mode : kNewModes) {
            const auto spec = SstvMartinM1Protocol::spec(mode);
            const std::vector<SstvRgbPixel> pixels(
                SstvMartinM1Encoder::pixelCount(mode),
                {41U, 127U, 233U});
            SstvMartinM1EncoderConfig config;
            config.mode = mode;
            SstvMartinM1Encoder encoder(pixels, config);
            std::vector<std::int16_t> chunk(4'093U);
            std::uint64_t streamed = 0U;
            while (!encoder.complete()) {
                const std::size_t produced = encoder.pullPcm16(
                    chunk.data(), chunk.size());
                QVERIFY(produced > 0U);
                streamed += produced;
            }
            QCOMPARE(streamed, encoder.totalSamples());
            QCOMPARE(encoder.position().stage,
                     SstvMartinM1EncoderStage::Complete);
            QCOMPARE(encoder.totalSamples(),
                     std::uint64_t {10'920U}
                         + oracleSamples(unsignedDuration(spec.imageDuration),
                                         12'000U,
                                         0));
            QCOMPARE(encoder.metrics().tone.samplesGenerated,
                     encoder.totalSamples());
            QCOMPARE(encoder.metrics().residentImageBytes,
                     SstvMartinM1Encoder::pixelCount(mode)
                         * sizeof(SstvRgbPixel));
        }
    }

    void invalidModesAndModeSpecificLineBoundsFailClosed()
    {
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMartinM1Protocol::spec(static_cast<SstvMartinMode>(255U)));
        SstvMartinM1Mapper m4({12'000U, 0, SstvMartinMode::M4});
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 m4.lineStartSample(129U));
        SstvMartinM1DecoderConfig config;
        config.mode = SstvMartinMode::M4;
        SstvMartinM1Decoder decoder(config);
        const SstvMartinM1LineSync bad {
            128U, 0U, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&bad, 1U), std::size_t {0U});
        QCOMPARE(decoder.metrics().rejectedSyncs, std::uint64_t {1U});
    }
};

QTEST_APPLESS_MAIN(TestSstvMartinFamily)

#include "test_sstv_martin_family.moc"
