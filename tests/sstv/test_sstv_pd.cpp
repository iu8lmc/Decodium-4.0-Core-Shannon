// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvPd.h"

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
constexpr std::uint64_t kImageStart = 70'000U;
constexpr double kOffsetHz = 61.0;
constexpr std::array<SstvPdMode, 7U> kModes {{
    SstvPdMode::Pd50,
    SstvPdMode::Pd90,
    SstvPdMode::Pd120,
    SstvPdMode::Pd160,
    SstvPdMode::Pd180,
    SstvPdMode::Pd240,
    SstvPdMode::Pd290,
}};

constexpr std::array<std::uint64_t, 7U> kCanonicalTotalFrames {{
    607'133U,
    1'090'789U,
    1'524'156U,
    1'941'518U,
    2'255'538U,
    2'986'920U,
    3'475'106U,
}};

std::uint64_t duration(Picoseconds value)
{
    if (value.count < 0) {
        throw std::logic_error("negative PD test duration");
    }
    return static_cast<std::uint64_t>(value.count);
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

std::array<std::uint64_t, 4U> scanStarts(const SstvPdModeSpec& spec)
{
    const std::uint64_t first = duration(spec.syncDuration)
        + duration(spec.porchDuration);
    const std::uint64_t component = duration(spec.componentDuration);
    return {first, first + component, first + component * 2U,
            first + component * 3U};
}

const QJsonObject fixtureMode(const QJsonObject& root, const char* stableId)
{
    for (const QJsonValue& value : root.value(QStringLiteral("modes")).toArray()) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("modeId")).toString()
            == QString::fromLatin1(stableId)) {
            return item;
        }
    }
    return {};
}

QJsonObject readFixture(const char* path)
{
    QFile file(QString::fromUtf8(path));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(file.errorString().toStdString());
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error(error.errorString().toStdString());
    }
    return document.object();
}

std::uint8_t yEvenValue(std::uint32_t x, std::uint32_t pair) noexcept
{
    return static_cast<std::uint8_t>((x * 7U + pair * 11U + 31U) & 0xffU);
}

std::uint8_t yOddValue(std::uint32_t x, std::uint32_t pair) noexcept
{
    return static_cast<std::uint8_t>((x * 13U + pair * 5U + 67U) & 0xffU);
}

std::uint8_t crValue(std::uint32_t x, std::uint32_t pair) noexcept
{
    return static_cast<std::uint8_t>((x * 3U + pair * 17U + 101U) & 0xffU);
}

std::uint8_t cbValue(std::uint32_t x, std::uint32_t pair) noexcept
{
    return static_cast<std::uint8_t>((x * 19U + pair * 2U + 149U) & 0xffU);
}

std::vector<SstvFrequencyObservation> observationsForPair(
    const SstvPdModeSpec& spec,
    std::uint32_t pair,
    std::uint64_t& sequence,
    std::int32_t clockErrorPpm = 0)
{
    const auto starts = scanStarts(spec);
    const std::uint64_t pairStart = static_cast<std::uint64_t>(pair)
        * duration(spec.linePairDuration);
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(spec.width) * 4U + 1U);

    auto append = [&](std::uint64_t protocolTime, double frequency) {
        SstvFrequencyObservation observation;
        observation.status = SstvFrequencyStatus::Valid;
        observation.sequence = sequence++;
        observation.centreSample = kImageStart
            + protocolSamples(protocolTime, kSampleRate, clockErrorPpm);
        observation.rawFrequencyHz = frequency + kOffsetHz;
        observation.correctedFrequencyHz = observation.rawFrequencyHz;
        observation.rms = 0.5;
        observation.snrDb = 34.0;
        observation.confidence = 0.98;
        observation.validSampleFraction = 1.0;
        result.push_back(observation);
    };
    append(pairStart + 1'000'000'000ULL,
           SstvPdProtocol::SyncFrequencyHz);
    for (std::uint8_t scan = 0U; scan < 4U; ++scan) {
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            const std::uint64_t pixelMiddle =
                duration(spec.pixelDuration)
                * (static_cast<std::uint64_t>(x) * 2U + 1U) / 2U;
            std::uint8_t value = 0U;
            switch (scan) {
            case 0U:
                value = yEvenValue(x, pair);
                break;
            case 1U:
                value = crValue(x, pair);
                break;
            case 2U:
                value = cbValue(x, pair);
                break;
            case 3U:
                value = yOddValue(x, pair);
                break;
            default:
                throw std::logic_error("invalid PD test scan");
            }
            append(pairStart + starts[scan] + pixelMiddle,
                   SstvPdProtocol::frequencyForValue(value));
        }
    }
    return result;
}

std::vector<SstvRgbPixel> constantImage(SstvPdMode mode)
{
    return std::vector<SstvRgbPixel>(
        SstvPdEncoder::pixelCount(mode), SstvRgbPixel {210U, 48U, 133U});
}

std::vector<std::int16_t> renderPrefix(SstvPdEncoder& encoder,
                                       std::size_t count,
                                       bool fragmented)
{
    std::vector<std::int16_t> result(count);
    constexpr std::array<std::size_t, 6U> pattern {{
        1U, 17U, 509U, 4'093U, 31U, 8'191U}};
    std::size_t produced = 0U;
    std::size_t patternIndex = 0U;
    while (produced < result.size()) {
        const std::size_t request = fragmented
            ? std::min(pattern[patternIndex++ % pattern.size()],
                       result.size() - produced)
            : result.size() - produced;
        const std::size_t pulled = encoder.pullPcm16(
            result.data() + produced, request);
        if (pulled == 0U) {
            break;
        }
        produced += pulled;
    }
    result.resize(produced);
    return result;
}

} // namespace

class TestSstvPd final : public QObject
{
    Q_OBJECT

private slots:
    void protocolTableAndCumulativeMapperAreCanonical()
    {
        constexpr std::array<const char*, 7U> ids {{
            "pd-50", "pd-90", "pd-120", "pd-160", "pd-180", "pd-240",
            "pd-290"}};
        constexpr std::array<std::uint8_t, 7U> vis {{
            93U, 99U, 95U, 98U, 96U, 97U, 94U}};
        constexpr std::array<std::uint32_t, 7U> widths {{
            320U, 320U, 640U, 512U, 640U, 640U, 800U}};
        constexpr std::array<std::uint32_t, 7U> heights {{
            256U, 256U, 496U, 400U, 496U, 496U, 616U}};
        constexpr std::array<std::int64_t, 7U> pixels {{
            286'000'000LL, 532'000'000LL, 190'000'000LL, 382'000'000LL,
            286'000'000LL, 382'000'000LL, 286'000'000LL}};
        constexpr std::array<std::int64_t, 7U> pairs {{
            388'160'000'000LL, 703'040'000'000LL, 508'480'000'000LL,
            804'416'000'000LL, 754'240'000'000LL, 1'000'000'000'000LL,
            937'280'000'000LL}};

        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const SstvPdModeSpec spec = SstvPdProtocol::spec(kModes[index]);
            QCOMPARE(QByteArray(spec.stableId), QByteArray(ids[index]));
            QCOMPARE(spec.visPayload, vis[index]);
            QCOMPARE(spec.width, widths[index]);
            QCOMPARE(spec.height, heights[index]);
            QCOMPARE(spec.syncDuration.count,
                     std::int64_t {20'000'000'000LL});
            QCOMPARE(spec.porchDuration.count,
                     std::int64_t {2'080'000'000LL});
            QCOMPARE(spec.pixelDuration.count, pixels[index]);
            QCOMPARE(spec.componentDuration.count,
                     pixels[index] * widths[index]);
            QCOMPARE(spec.linePairDuration.count, pairs[index]);
            QCOMPARE(spec.imageDuration.count,
                     pairs[index] * (heights[index] / 2U));
            QCOMPARE(SstvPdProtocol::modeForVis(vis[index]),
                     std::optional<SstvPdMode> {kModes[index]});

            const auto starts = scanStarts(spec);
            SstvPdMapper mapper({kModes[index], kSampleRate, 0});
            QCOMPARE(mapper.linePairCount(), heights[index] / 2U);
            QCOMPARE(mapper.positionAtElapsedTime(Picoseconds {0}).region,
                     SstvPdRegion::Sync);
            QCOMPARE(mapper.positionAtElapsedTime(spec.syncDuration).region,
                     SstvPdRegion::Porch);
            constexpr std::array<ColourComponent, 4U> components {{
                ColourComponent::Luminance,
                ColourComponent::ChrominanceRed,
                ColourComponent::ChrominanceBlue,
                ColourComponent::Luminance}};
            for (std::uint8_t scan = 0U; scan < 4U; ++scan) {
                const SstvPdPosition position = mapper.positionAtElapsedTime(
                    Picoseconds {static_cast<std::int64_t>(starts[scan])});
                QCOMPARE(position.region, SstvPdRegion::Pixel);
                QCOMPARE(position.scanIndex, scan);
                QCOMPARE(position.component, components[scan]);
                QCOMPARE(position.pixel, std::uint32_t {0U});
            }
            QCOMPARE(mapper.imageSampleCount(),
                     kCanonicalTotalFrames[index] - 10'920U);
            QCOMPARE(mapper.linePairStartSample(mapper.linePairCount()),
                     mapper.imageSampleCount());
            QCOMPARE(mapper.positionAtSample(mapper.imageSampleCount()).region,
                     SstvPdRegion::Complete);
            for (const std::int32_t ppm : {-300, 300}) {
                SstvPdMapper corrected({kModes[index], 44'100U, ppm});
                QCOMPARE(corrected.linePairStartSample(
                             corrected.linePairCount()),
                         protocolSamples(duration(spec.imageDuration),
                                         44'100U,
                                         ppm));
            }
        }
        QVERIFY(!SstvPdProtocol::modeForVis(0U).has_value());
    }

    void encoderIsChunkInvariantBoundedAndStopsBeforeDefectiveExtraPair()
    {
        std::array<std::int16_t, 65'536U> scratch {};
        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const auto pixels = constantImage(kModes[index]);
            SstvPdEncoderConfig config;
            config.mode = kModes[index];
            SstvPdEncoder contiguous(pixels, config);
            SstvPdEncoder fragmented(pixels, config);
            QCOMPARE(renderPrefix(fragmented, 100'000U, true),
                     renderPrefix(contiguous, 100'000U, false));
            QCOMPARE(contiguous.totalSamples(), kCanonicalTotalFrames[index]);

            std::uint64_t drained = fragmented.producedSamples();
            while (!fragmented.complete()) {
                const std::size_t pulled = fragmented.pullPcm16(
                    scratch.data(), scratch.size());
                QVERIFY(pulled > 0U);
                drained += pulled;
            }
            QCOMPARE(drained, kCanonicalTotalFrames[index]);
            QCOMPARE(fragmented.pullPcm16(scratch.data(), 1U),
                     std::size_t {0U});
            QCOMPARE(fragmented.metrics().residentImageBytes,
                     pixels.size() * sizeof(SstvYCbCrPixel));
            QVERIFY(fragmented.metrics().tone.peakAfterClamp
                    <= kDefaultSstvTxHeadroom + 1.0e-6);
        }

        const auto pixels = constantImage(SstvPdMode::Pd290);
        SstvPdEncoderConfig config;
        config.mode = SstvPdMode::Pd290;
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvPdEncoder(pixels.data(), pixels.size() - 1U, config));
        SstvPdEncoder encoder(pixels, config);
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullPcm16(
                scratch.data(), SstvPdEncoder::MaximumSamplesPerPull + 1U));
        encoder.cancel();
        QCOMPARE(encoder.pullPcm16(scratch.data(), 1U), std::size_t {0U});
        encoder.reset();
        QCOMPARE(encoder.pullPcm16(scratch.data(), 1U), std::size_t {1U});
    }

    void rxAndTxShareExplicitClockCorrectionAndFrequencyOffset()
    {
        constexpr std::int32_t ppm = -300;
        constexpr SstvPdMode mode = SstvPdMode::Pd120;
        const SstvPdModeSpec spec = SstvPdProtocol::spec(mode);
        SstvPdDecoderConfig decoderConfig;
        decoderConfig.mode = mode;
        decoderConfig.clockErrorPpm = ppm;
        decoderConfig.imageStartSample = kImageStart;
        decoderConfig.frequencyOffsetHz = kOffsetHz;
        SstvPdDecoder decoder(decoderConfig);
        std::uint64_t sequence = 1U;
        for (std::uint32_t pair = 0U; pair < 2U; ++pair) {
            const auto observations = observationsForPair(
                spec, pair, sequence, ppm);
            std::size_t offset = 0U;
            while (offset < observations.size()) {
                const std::size_t count = std::min<std::size_t>(
                    503U, observations.size() - offset);
                static_cast<void>(decoder.consume(
                    observations.data() + offset, count));
                offset += count;
            }
        }
        QCOMPARE(decoder.finish(), SstvPdDecodeState::Partial);
        QCOMPARE(decoder.metrics().linePairsPublished, std::uint64_t {2U});
        QCOMPARE(decoder.metrics().linesPublished, std::uint64_t {4U});
        QCOMPARE(decoder.frequencyOffsetHz(), kOffsetHz);

        SstvPdEncoderConfig encoderConfig;
        encoderConfig.mode = SstvPdMode::Pd290;
        encoderConfig.sampleRate = 44'100U;
        encoderConfig.clockErrorPpm = 300;
        SstvPdEncoder encoder(constantImage(SstvPdMode::Pd290),
                              encoderConfig);
        SstvTimingAccumulator header(encoderConfig.sampleRate);
        const SstvPdMapper mapper({SstvPdMode::Pd290,
                                   encoderConfig.sampleRate,
                                   encoderConfig.clockErrorPpm});
        QCOMPARE(encoder.totalSamples(),
                 header.samplesFor(SstvPdProtocol::HeaderDuration)
                     + mapper.imageSampleCount());
    }

    void decoderCompletesEveryModeInYCrCbPairOrder()
    {
        for (const SstvPdMode mode : kModes) {
            const SstvPdModeSpec spec = SstvPdProtocol::spec(mode);
            SstvPdDecoderConfig config;
            config.mode = mode;
            config.imageStartSample = kImageStart;
            config.frequencyOffsetHz = kOffsetHz;
            config.maximumPendingDirtyEvents = 8U;
            SstvPdDecoder decoder(config);
            std::uint64_t sequence = 1U;
            for (std::uint32_t pair = 0U; pair < spec.height / 2U; ++pair) {
                const auto observations = observationsForPair(
                    spec, pair, sequence);
                std::size_t offset = 0U;
                while (offset < observations.size()) {
                    const std::size_t count = std::min<std::size_t>(
                        997U, observations.size() - offset);
                    static_cast<void>(decoder.consume(
                        observations.data() + offset, count));
                    offset += count;
                }
            }
            QCOMPARE(decoder.finish(), SstvPdDecodeState::Complete);
            const SstvImageSnapshot image = decoder.snapshot();
            QVERIFY(image.isComplete());
            QCOMPARE(image.width, spec.width);
            QCOMPARE(image.height, spec.height);
            QCOMPARE(decoder.metrics().linePairsPublished,
                     std::uint64_t {spec.height / 2U});
            QCOMPARE(decoder.metrics().linesPublished,
                     std::uint64_t {spec.height});
            QCOMPARE(decoder.metrics().observedPairSyncs,
                     std::uint64_t {spec.height / 2U});
            QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                    <= static_cast<std::size_t>(spec.width) * 4U);
            for (const std::uint32_t pair : {
                     0U, spec.height / 4U, spec.height / 2U - 1U}) {
                for (const std::uint32_t x : {
                         0U, 1U, spec.width / 2U, spec.width - 1U}) {
                    const SstvRgbPixel even =
                        SstvColourConverter::yCbCrToRgb(
                            {yEvenValue(x, pair), cbValue(x, pair),
                             crValue(x, pair)});
                    const SstvRgbPixel odd =
                        SstvColourConverter::yCbCrToRgb(
                            {yOddValue(x, pair), cbValue(x, pair),
                             crValue(x, pair)});
                    QCOMPARE(image.pixel(x, pair * 2U), even);
                    QCOMPARE(image.pixel(x, pair * 2U + 1U), odd);
                }
            }
            QVERIFY(decoder.takeDirtyEvents().size() <= 8U);
        }
    }

    void pinnedFixturesMatchCanonicalBoundaryAndRecordLibsstvDefect()
    {
#if defined(DECODIUM_SSTV_PD_PYSSTV_FIXTURE) \
    && defined(DECODIUM_SSTV_PD_LIBSSTV_FIXTURE)
        const QJsonObject pysstv = readFixture(
            DECODIUM_SSTV_PD_PYSSTV_FIXTURE);
        const QJsonObject libsstv = readFixture(
            DECODIUM_SSTV_PD_LIBSSTV_FIXTURE);
        QCOMPARE(pysstv.value(QStringLiteral("sourceCommit")).toString(),
                 QStringLiteral(
                     "d998fad154d3e6ad2d73af5add49beec0d2ab59f"));
        QCOMPARE(libsstv.value(QStringLiteral("producerCommit")).toString(),
                 QStringLiteral(
                     "193157a993ac34bfa074074004c9ddadcfe6fd15"));
        QCOMPARE(pysstv.value(QStringLiteral("modes")).toArray().size(), 6);
        QCOMPARE(libsstv.value(QStringLiteral("modes")).toArray().size(), 7);
        QVERIFY(fixtureMode(pysstv, "pd-50").isEmpty());

        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const SstvPdModeSpec spec = SstvPdProtocol::spec(kModes[index]);
            const SstvPdMapper mapper({kModes[index], kSampleRate, 0});
            const QJsonObject lib = fixtureMode(libsstv, spec.stableId);
            QVERIFY2(!lib.isEmpty(), spec.stableId);
            QCOMPARE(lib.value(QStringLiteral("visPayload")).toInt(),
                     static_cast<int>(spec.visPayload));
            QCOMPARE(lib.value(QStringLiteral("width")).toInt(),
                     static_cast<int>(spec.width));
            QCOMPARE(lib.value(QStringLiteral("height")).toInt(),
                     static_cast<int>(spec.height));
            QCOMPARE(static_cast<std::uint64_t>(
                         lib.value(QStringLiteral("canonicalTotalFrames"))
                             .toInteger()),
                     kCanonicalTotalFrames[index]);
            const std::uint64_t full = static_cast<std::uint64_t>(
                lib.value(QStringLiteral("libsstvFullPcmFrames")).toInteger());
            const std::uint64_t trailing = static_cast<std::uint64_t>(
                lib.value(QStringLiteral("libsstvTrailingPairFrames"))
                    .toInteger());
            QCOMPARE(full, kCanonicalTotalFrames[index] + trailing);
            QVERIFY(trailing > 0U);
            QVERIFY(mapper.imageSampleCount() < full - 10'920U);

            const QJsonObject py = fixtureMode(pysstv, spec.stableId);
            if (kModes[index] == SstvPdMode::Pd50) {
                QVERIFY(py.isEmpty());
                continue;
            }
            QVERIFY2(!py.isEmpty(), spec.stableId);
            QCOMPARE(static_cast<std::uint64_t>(
                         py.value(QStringLiteral("imageFrames")).toInteger()),
                     mapper.imageSampleCount());
            QCOMPARE(static_cast<std::uint64_t>(
                         py.value(QStringLiteral("totalFramesWithHeader"))
                             .toInteger()),
                     kCanonicalTotalFrames[index]);
            const QJsonObject landmarks = py.value(
                QStringLiteral("firstPairFrames")).toObject();
            const auto starts = scanStarts(spec);
            QCOMPARE(landmarks.value(QStringLiteral("sync")).toInteger(),
                     qint64 {0});
            QCOMPARE(landmarks.value(QStringLiteral("porch")).toInteger(),
                     static_cast<qint64>(
                         protocolSamples(duration(spec.syncDuration))));
            QCOMPARE(landmarks.value(QStringLiteral("yEven")).toInteger(),
                     static_cast<qint64>(protocolSamples(starts[0])));
            QCOMPARE(landmarks.value(QStringLiteral("cr")).toInteger(),
                     static_cast<qint64>(protocolSamples(starts[1])));
            QCOMPARE(landmarks.value(QStringLiteral("cb")).toInteger(),
                     static_cast<qint64>(protocolSamples(starts[2])));
            QCOMPARE(landmarks.value(QStringLiteral("yOdd")).toInteger(),
                     static_cast<qint64>(protocolSamples(starts[3])));
            QCOMPARE(landmarks.value(QStringLiteral("nextSync")).toInteger(),
                     static_cast<qint64>(
                         protocolSamples(duration(spec.linePairDuration))));
        }
#endif
    }
};

QTEST_APPLESS_MAIN(TestSstvPd)

#include "test_sstv_pd.moc"
