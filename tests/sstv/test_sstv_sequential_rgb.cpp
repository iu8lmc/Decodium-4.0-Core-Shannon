// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvSequentialRgb.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
constexpr std::uint64_t kImageStart = 50'000U;
constexpr double kOffsetHz = 73.0;
constexpr std::array<SstvSequentialRgbMode, 6U> kModes {{
    SstvSequentialRgbMode::WraaseSc2_60,
    SstvSequentialRgbMode::WraaseSc2_120,
    SstvSequentialRgbMode::WraaseSc2_180,
    SstvSequentialRgbMode::PasokonP3,
    SstvSequentialRgbMode::PasokonP5,
    SstvSequentialRgbMode::PasokonP7,
}};

std::uint64_t unsignedDuration(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("negative test duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

std::uint64_t protocolSamples(std::uint64_t picoseconds,
                              std::uint32_t sampleRate = kSampleRate,
                              std::int32_t ppm = 0)
{
    const long double scaled = static_cast<long double>(picoseconds)
        * sampleRate * (1'000'000.0L + ppm)
        / (static_cast<long double>(kPicosecondsPerSecond) * 1'000'000.0L);
    return static_cast<std::uint64_t>(std::floor(scaled));
}

SstvRgbPixel patternPixel(std::uint32_t x, std::uint32_t y)
{
    return {
        static_cast<std::uint8_t>((x * 17U + y * 3U) & 0xffU),
        static_cast<std::uint8_t>((x * 5U + y * 19U) & 0xffU),
        static_cast<std::uint8_t>((x * 11U + y * 7U) & 0xffU),
    };
}

std::vector<SstvRgbPixel> testImage(SstvSequentialRgbMode mode)
{
    const SstvSequentialRgbModeSpec spec =
        SstvSequentialRgbProtocol::spec(mode);
    std::vector<SstvRgbPixel> result(
        static_cast<std::size_t>(spec.width) * spec.height);
    for (std::uint32_t y = 0U; y < spec.height; ++y) {
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            result[static_cast<std::size_t>(y) * spec.width + x]
                = patternPixel(x, y);
        }
    }
    return result;
}

std::array<std::uint64_t, 3U> componentStarts(
    const SstvSequentialRgbModeSpec& spec)
{
    const std::uint64_t sync = unsignedDuration(spec.syncDuration);
    const std::uint64_t component = unsignedDuration(spec.componentDuration);
    const std::uint64_t red = sync
        + unsignedDuration(spec.gapDurations[0]);
    const std::uint64_t green = red + component
        + unsignedDuration(spec.gapDurations[1]);
    const std::uint64_t blue = green + component
        + unsignedDuration(spec.gapDurations[2]);
    return {red, green, blue};
}

ColourComponent componentAt(std::size_t index)
{
    constexpr std::array<ColourComponent, 3U> components {{
        ColourComponent::Red,
        ColourComponent::Green,
        ColourComponent::Blue,
    }};
    return components.at(index);
}

std::uint8_t componentValue(const SstvRgbPixel& pixel,
                            ColourComponent component)
{
    switch (component) {
    case ColourComponent::Red:
        return pixel.red;
    case ColourComponent::Green:
        return pixel.green;
    case ColourComponent::Blue:
        return pixel.blue;
    default:
        throw std::logic_error("invalid sequential RGB test component");
    }
}

std::vector<SstvFrequencyObservation> observationsForLine(
    const SstvSequentialRgbModeSpec& spec,
    const SstvSequentialRgbMapper& mapper,
    std::uint32_t line,
    std::uint64_t& sequence)
{
    const auto starts = componentStarts(spec);
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(spec.width) * 3U + 1U);

    SstvFrequencyObservation sync;
    sync.status = SstvFrequencyStatus::Valid;
    sync.sequence = sequence++;
    sync.centreSample = kImageStart + mapper.lineStartSample(line) + 1U;
    sync.rawFrequencyHz = SstvSequentialRgbProtocol::SyncFrequencyHz + kOffsetHz;
    sync.correctedFrequencyHz = sync.rawFrequencyHz;
    sync.rms = 0.5;
    sync.snrDb = 35.0;
    sync.confidence = 0.98;
    sync.validSampleFraction = 1.0;
    result.push_back(sync);

    const std::uint64_t lineStart =
        static_cast<std::uint64_t>(line)
        * unsignedDuration(spec.lineDuration);
    const std::uint64_t component = unsignedDuration(spec.componentDuration);
    for (std::size_t channel = 0U; channel < starts.size(); ++channel) {
        const ColourComponent colour = componentAt(channel);
        for (std::uint32_t pixel = 0U; pixel < spec.width; ++pixel) {
            const std::uint64_t numerator =
                static_cast<std::uint64_t>(pixel) * 2U + 1U;
            const std::uint64_t middle =
                component * numerator / (2U * spec.width);
            const Picoseconds elapsed {static_cast<std::int64_t>(
                lineStart + starts[channel] + middle)};
            const SstvSequentialRgbPosition position =
                mapper.positionAtElapsedTime(elapsed);
            if (position.region != SstvSequentialRgbRegion::Pixel
                || position.line != line
                || position.component != colour
                || position.pixel != pixel) {
                throw std::logic_error(
                    "sequential RGB test oracle mapped the wrong pixel");
            }

            const SstvRgbPixel source = patternPixel(pixel, line);
            SstvFrequencyObservation observation;
            observation.status = SstvFrequencyStatus::Valid;
            observation.sequence = sequence++;
            observation.centreSample = kImageStart
                + (position.segmentStartSample
                   + position.segmentEndSample) / 2U;
            observation.rawFrequencyHz =
                SstvSequentialRgbProtocol::frequencyForValue(
                    componentValue(source, colour)) + kOffsetHz;
            observation.correctedFrequencyHz = observation.rawFrequencyHz;
            observation.rms = 0.5;
            observation.snrDb = 35.0;
            observation.confidence = 0.98;
            observation.validSampleFraction = 1.0;
            result.push_back(observation);
        }
    }
    return result;
}

std::vector<std::int16_t> renderPrefix(SstvSequentialRgbEncoder& encoder,
                                       std::size_t count,
                                       bool fragmented)
{
    std::vector<std::int16_t> result(count);
    std::size_t produced = 0U;
    const std::array<std::size_t, 6U> pattern {{1U, 17U, 511U, 4'093U,
                                                29U, 8'191U}};
    std::size_t patternIndex = 0U;
    while (produced < result.size()) {
        const std::size_t request = fragmented
            ? std::min(pattern[patternIndex++ % pattern.size()],
                       result.size() - produced)
            : result.size() - produced;
        const std::size_t generated = encoder.pullPcm16(
            result.data() + produced, request);
        if (generated == 0U) {
            break;
        }
        produced += generated;
    }
    result.resize(produced);
    return result;
}

} // namespace

class TestSstvSequentialRgb final : public QObject
{
    Q_OBJECT

private slots:
    void protocolTableResolvesVisGeometryAndStructuralSums()
    {
        const std::array<std::uint8_t, 6U> vis {{59U, 63U, 55U,
                                                113U, 114U, 115U}};
        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const SstvSequentialRgbMode mode = kModes[index];
            const SstvSequentialRgbModeSpec spec =
                SstvSequentialRgbProtocol::spec(mode);
            QCOMPARE(spec.mode, mode);
            QCOMPARE(spec.visPayload, vis[index]);
            QCOMPARE(SstvSequentialRgbProtocol::modeForVis(vis[index]),
                     std::optional<SstvSequentialRgbMode> {mode});
            QVERIFY(spec.width == 320U || spec.width == 640U);
            QVERIFY(spec.height == 256U || spec.height == 496U);
            std::uint64_t line = unsignedDuration(spec.syncDuration)
                + 3U * unsignedDuration(spec.componentDuration);
            for (const Picoseconds gap : spec.gapDurations) {
                line += unsignedDuration(gap);
            }
            QCOMPARE(line, unsignedDuration(spec.lineDuration));
            QCOMPARE(line * spec.height,
                     unsignedDuration(spec.imageDuration));
            QVERIFY(spec.compatibilityProfile != nullptr);
            QVERIFY(*spec.compatibilityProfile != '\0');
        }
        QVERIFY(!SstvSequentialRgbProtocol::modeForVis(0U).has_value());
        QCOMPARE(SstvSequentialRgbProtocol::spec(
                     SstvSequentialRgbMode::PasokonP3).effectiveSampledWidth,
                 std::uint32_t {320U});
        const SstvSequentialRgbModeSpec sc2_60 =
            SstvSequentialRgbProtocol::spec(
                SstvSequentialRgbMode::WraaseSc2_60);
        QCOMPARE(sc2_60.width, std::uint32_t {320U});
        QCOMPARE(sc2_60.height, std::uint32_t {256U});
        QCOMPARE(sc2_60.effectiveSampledWidth, std::uint32_t {256U});
        QCOMPARE(sc2_60.syncDuration.count,
                 std::int64_t {5'000'000'000LL});
        for (const Picoseconds gap : sc2_60.gapDurations) {
            QCOMPARE(gap.count, std::int64_t {1'000'000'000LL});
        }
        QCOMPARE(sc2_60.componentDuration.count,
                 std::int64_t {77'134'765'625LL});
        QCOMPARE(sc2_60.lineDuration.count,
                 std::int64_t {240'404'296'875LL});
        QCOMPARE(sc2_60.imageDuration.count,
                 std::int64_t {61'543'500'000'000LL});
        QVERIFY(QByteArray(sc2_60.compatibilityProfile).contains(
            QByteArrayLiteral("RX compatibility profile")));
        QCOMPARE(SstvSequentialRgbProtocol::spec(
                     SstvSequentialRgbMode::WraaseSc2_180)
                     .effectiveSampledWidth,
                 std::uint32_t {512U});
    }

    void mapperPreservesLineLeadingSyncRgbOrderAndCumulativeClock()
    {
        for (const SstvSequentialRgbMode mode : kModes) {
            const SstvSequentialRgbModeSpec spec =
                SstvSequentialRgbProtocol::spec(mode);
            const auto starts = componentStarts(spec);
            SstvSequentialRgbMapper mapper({mode, kSampleRate, 0});
            QCOMPARE(mapper.positionAtElapsedTime(Picoseconds {0}).region,
                     SstvSequentialRgbRegion::Sync);
            for (std::size_t index = 0U; index < starts.size(); ++index) {
                const SstvSequentialRgbPosition position =
                    mapper.positionAtElapsedTime(Picoseconds {
                        static_cast<std::int64_t>(starts[index])});
                QCOMPARE(position.region, SstvSequentialRgbRegion::Pixel);
                QCOMPARE(position.component, componentAt(index));
                QCOMPARE(position.pixel, std::uint32_t {0U});
            }
            QCOMPARE(mapper.positionAtElapsedTime(spec.lineDuration).line,
                     std::uint32_t {1U});
            QCOMPARE(mapper.imageSampleCount(),
                     protocolSamples(unsignedDuration(spec.imageDuration)));
            for (const std::int32_t ppm : {-300, 300}) {
                SstvSequentialRgbMapper corrected({mode, 44'100U, ppm});
                QCOMPARE(corrected.lineStartSample(spec.height),
                         protocolSamples(unsignedDuration(spec.imageDuration),
                                         44'100U,
                                         ppm));
            }
        }
    }

    void encoderIsVisCorrectChunkInvariantAndBounded()
    {
        for (const SstvSequentialRgbMode mode : kModes) {
            const auto pixels = testImage(mode);
            SstvSequentialRgbEncoderConfig config;
            config.mode = mode;
            SstvSequentialRgbEncoder contiguous(pixels, config);
            SstvSequentialRgbEncoder fragmented(pixels, config);
            const std::vector<std::int16_t> first =
                renderPrefix(contiguous, 100'000U, false);
            const std::vector<std::int16_t> second =
                renderPrefix(fragmented, 100'000U, true);
            QCOMPARE(second, first);
            QCOMPARE(contiguous.metrics().residentImageBytes,
                     pixels.size() * sizeof(SstvRgbPixel));
            QVERIFY(contiguous.metrics().segmentTransitions > 1'000U);
            QVERIFY(contiguous.metrics().tone.peakAfterClamp
                    <= kDefaultSstvTxHeadroom + 1.0e-6);
            QCOMPARE(contiguous.totalSamples(),
                     std::uint64_t {10'920U}
                         + SstvSequentialRgbMapper({mode, kSampleRate, 0})
                               .imageSampleCount());
        }

        const auto pixels = testImage(SstvSequentialRgbMode::PasokonP7);
        SstvSequentialRgbEncoderConfig config;
        config.mode = SstvSequentialRgbMode::PasokonP7;
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvSequentialRgbEncoder(pixels.data(), pixels.size() - 1U,
                                     config));
        SstvSequentialRgbEncoder encoder(pixels, config);
        float sample = 0.0F;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullFloat(
                &sample,
                SstvSequentialRgbEncoder::MaximumSamplesPerPull + 1U));
        encoder.cancel();
        QCOMPARE(encoder.pullFloat(&sample, 1U), std::size_t {0U});
        encoder.reset();
        QCOMPARE(encoder.pullFloat(&sample, 1U), std::size_t {1U});
    }

    void decoderCompletesAllModesWithOneBoundedScanline()
    {
        for (const SstvSequentialRgbMode mode : kModes) {
            const SstvSequentialRgbModeSpec spec =
                SstvSequentialRgbProtocol::spec(mode);
            SstvSequentialRgbMapper mapper({mode, kSampleRate, 0});
            SstvSequentialRgbDecoderConfig config;
            config.mode = mode;
            config.imageStartSample = kImageStart;
            config.frequencyOffsetHz = kOffsetHz;
            config.maximumPendingDirtyEvents = 8U;
            SstvSequentialRgbDecoder decoder(config);
            std::uint64_t sequence = 1U;
            for (std::uint32_t line = 0U; line < spec.height; ++line) {
                const auto observations = observationsForLine(
                    spec, mapper, line, sequence);
                std::size_t offset = 0U;
                while (offset < observations.size()) {
                    const std::size_t count = std::min<std::size_t>(
                        997U, observations.size() - offset);
                    static_cast<void>(decoder.consume(
                        observations.data() + offset, count));
                    offset += count;
                }
            }
            QCOMPARE(decoder.finish(),
                     SstvSequentialRgbDecodeState::Complete);
            const SstvImageSnapshot snapshot = decoder.snapshot();
            QVERIFY(snapshot.isComplete());
            QCOMPARE(snapshot.width, spec.width);
            QCOMPARE(snapshot.height, spec.height);
            QCOMPARE(decoder.metrics().linesPublished,
                     std::uint64_t {spec.height});
            QCOMPARE(decoder.metrics().observedLineSyncs,
                     std::uint64_t {spec.height});
            QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                    <= static_cast<std::size_t>(spec.width) * 3U);
            for (const std::uint32_t y : {
                     0U, spec.height / 2U, spec.height - 1U}) {
                for (const std::uint32_t x : {
                         0U, 1U, spec.width / 2U, spec.width - 1U}) {
                    const SstvRgbPixel actual = snapshot.pixel(x, y);
                    const SstvRgbPixel expected = patternPixel(x, y);
                    QVERIFY(std::abs(static_cast<int>(actual.red)
                                     - static_cast<int>(expected.red)) <= 1);
                    QVERIFY(std::abs(static_cast<int>(actual.green)
                                     - static_cast<int>(expected.green)) <= 1);
                    QVERIFY(std::abs(static_cast<int>(actual.blue)
                                     - static_cast<int>(expected.blue)) <= 1);
                }
            }
            QVERIFY(decoder.takeDirtyEvents().size() <= 8U);
        }
    }

    void pinnedPysstvLandmarksMatchSelectedProfiles()
    {
#ifdef DECODIUM_SSTV_SEQUENTIAL_RGB_FIXTURE
        QFile file(QString::fromUtf8(DECODIUM_SSTV_SEQUENTIAL_RGB_FIXTURE));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            file.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        const QJsonObject root = document.object();
        QCOMPARE(root.value(QStringLiteral("sourceCommit")).toString(),
                 QStringLiteral(
                     "d998fad154d3e6ad2d73af5add49beec0d2ab59f"));
        const QJsonArray modes = root.value(QStringLiteral("modes")).toArray();
        QCOMPARE(modes.size(), 5);
        for (const QJsonValue& value : modes) {
            const QJsonObject item = value.toObject();
            const QByteArray id = item.value(QStringLiteral("modeId"))
                                      .toString().toLatin1();
            const auto found = std::find_if(
                kModes.begin(), kModes.end(), [&id](SstvSequentialRgbMode mode) {
                    return id == SstvSequentialRgbProtocol::spec(mode).stableId;
                });
            QVERIFY(found != kModes.end());
            const SstvSequentialRgbModeSpec spec =
                SstvSequentialRgbProtocol::spec(*found);
            const SstvSequentialRgbMapper mapper({*found, kSampleRate, 0});
            QCOMPARE(item.value(QStringLiteral("visPayload")).toInt(),
                     static_cast<int>(spec.visPayload));
            QCOMPARE(static_cast<std::uint64_t>(
                         item.value(QStringLiteral("imageFrames")).toInteger()),
                     mapper.imageSampleCount());
            QCOMPARE(static_cast<std::uint64_t>(
                         item.value(QStringLiteral("lineFrames")).toInteger()),
                     mapper.lineStartSample(1U));
            const auto starts = componentStarts(spec);
            const QJsonObject landmarks = item.value(
                QStringLiteral("firstLineFrames")).toObject();
            QCOMPARE(static_cast<std::uint64_t>(
                         landmarks.value(QStringLiteral("sync")).toInteger()),
                     std::uint64_t {0U});
            QCOMPARE(static_cast<std::uint64_t>(
                         landmarks.value(QStringLiteral("red")).toInteger()),
                     protocolSamples(starts[0]));
            QCOMPARE(static_cast<std::uint64_t>(
                         landmarks.value(QStringLiteral("green")).toInteger()),
                     protocolSamples(starts[1]));
            QCOMPARE(static_cast<std::uint64_t>(
                         landmarks.value(QStringLiteral("blue")).toInteger()),
                     protocolSamples(starts[2]));
        }
#endif
    }
};

QTEST_APPLESS_MAIN(TestSstvSequentialRgb)

#include "test_sstv_sequential_rgb.moc"
