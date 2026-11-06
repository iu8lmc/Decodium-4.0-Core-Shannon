// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvRobot.h"
#include "../../src/sstv/image/SstvColourConverter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kRate = 12'000U;
constexpr std::uint64_t kAbsoluteStart = 51'000U;

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

std::uint64_t unsignedDuration(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("negative Robot test duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

std::uint64_t oracleSamples(std::uint64_t protocolPicoseconds,
                            std::uint32_t sampleRate,
                            std::int32_t ppm)
{
    const long double seconds =
        static_cast<long double>(protocolPicoseconds) / 1.0e12L;
    const long double scale = 1.0L
        + static_cast<long double>(ppm) / 1.0e6L;
    return static_cast<std::uint64_t>(std::floor(
        seconds * static_cast<long double>(sampleRate) * scale));
}

SstvRgbPixel sourcePixel(const SstvRobotModeSpec& spec,
                         std::uint32_t x,
                         std::uint32_t y)
{
    const std::uint32_t blockX = x / 2U;
    const std::uint32_t blockY =
        spec.chromaSubsampling == ChromaSubsampling::Cs420 ? y / 2U : y;
    return {
        static_cast<std::uint8_t>((blockX * 13U + blockY * 7U + 31U) & 0xffU),
        static_cast<std::uint8_t>((blockX * 3U + blockY * 17U + 79U) & 0xffU),
        static_cast<std::uint8_t>((blockX * 19U + blockY * 5U + 127U) & 0xffU),
    };
}

std::vector<SstvRgbPixel> sourceImage(SstvRobotMode mode)
{
    const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(SstvRobotEncoder::pixelCount(mode));
    for (std::uint32_t y = 0U; y < spec.height; ++y) {
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            pixels[static_cast<std::size_t>(y) * spec.width + x] =
                sourcePixel(spec, x, y);
        }
    }
    return pixels;
}

std::uint8_t averagedChroma(const std::vector<SstvRgbPixel>& pixels,
                            const SstvRobotModeSpec& spec,
                            const SstvRobotPosition& position)
{
    const std::uint32_t x0 = position.pixel * 2U;
    const std::uint32_t x1 = std::min(x0 + 1U, spec.width - 1U);
    std::uint32_t sum = 0U;
    std::uint32_t count = 0U;
    const auto add = [&](std::uint32_t x, std::uint32_t y) {
        const SstvYCbCrPixel value = SstvColourConverter::rgbToYCbCr(
            pixels[static_cast<std::size_t>(y) * spec.width + x]);
        sum += position.component == ColourComponent::ChrominanceRed
            ? value.chrominanceRed : value.chrominanceBlue;
        ++count;
    };
    if (spec.chromaSubsampling == ChromaSubsampling::Cs420) {
        const std::uint32_t y0 = position.line & ~1U;
        const std::uint32_t y1 = std::min(y0 + 1U, spec.height - 1U);
        add(x0, y0);
        add(x1, y0);
        add(x0, y1);
        add(x1, y1);
    } else {
        add(x0, position.line);
        add(x1, position.line);
    }
    return static_cast<std::uint8_t>((sum + count / 2U) / count);
}

double pixelFrequency(const std::vector<SstvRgbPixel>& pixels,
                      const SstvRobotModeSpec& spec,
                      const SstvRobotPosition& position)
{
    std::uint8_t value = 0U;
    if (position.component == ColourComponent::Luminance
        || position.component == ColourComponent::Gray) {
        value = SstvColourConverter::rgbToGrayscale(
            pixels[static_cast<std::size_t>(position.line) * spec.width
                   + position.pixel]);
    } else {
        value = averagedChroma(pixels, spec, position);
    }
    return SstvRobotProtocol::frequencyForValue(value);
}

SstvFrequencyObservation observation(std::uint64_t sample,
                                     std::uint64_t sequence,
                                     double frequency)
{
    SstvFrequencyObservation result;
    result.status = SstvFrequencyStatus::Valid;
    result.sequence = sequence;
    result.centreSample = sample;
    result.rawFrequencyHz = frequency;
    result.correctedFrequencyHz = frequency;
    result.rms = 0.5;
    result.snrDb = 35.0;
    result.confidence = 0.99;
    result.validSampleFraction = 1.0;
    return result;
}

std::vector<SstvFrequencyObservation> lineObservations(
    const std::vector<SstvRgbPixel>& pixels,
    const SstvRobotModeSpec& spec,
    const SstvRobotMapper& mapper,
    std::uint32_t line,
    std::uint64_t& sequence)
{
    std::vector<SstvFrequencyObservation> result;
    std::uint64_t sample = mapper.lineStartSample(line);
    const std::uint64_t end = mapper.lineEndSample(line);
    while (sample < end) {
        const SstvRobotPosition position = mapper.positionAtSample(sample);
        if (position.segmentEndSample <= sample) {
            throw std::logic_error("Robot test mapper made no progress");
        }
        if (position.region == SstvRobotRegion::Pixel) {
            const std::uint64_t centre = position.segmentStartSample
                + (position.segmentEndSample
                   - position.segmentStartSample) / 2U;
            result.push_back(observation(
                kAbsoluteStart + centre,
                sequence++,
                pixelFrequency(pixels, spec, position)));
        }
        sample = position.segmentEndSample;
    }
    return result;
}

QJsonObject fixtureRoot()
{
    QFile file(QString::fromUtf8(DECODIUM_SSTV_ROBOT_LIBSSTV_FIXTURE));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("cannot open Robot libsstv fixture");
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error("invalid Robot libsstv fixture");
    }
    return document.object();
}

QJsonObject fixtureMode(const QString& id)
{
    for (const QJsonValue value :
         fixtureRoot().value(QStringLiteral("modes")).toArray()) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toString() == id) {
            return object;
        }
    }
    throw std::runtime_error("Robot fixture mode missing");
}

} // namespace

class TestSstvRobot final : public QObject
{
    Q_OBJECT

private slots:
    void handbookSpecsResolveGeometryTimingAndAliases()
    {
        const struct Expected {
            SstvRobotMode mode;
            const char* id;
            std::uint32_t width;
            std::uint32_t height;
            std::uint32_t chromaWidth;
            std::uint8_t vis;
            ChromaSubsampling subsampling;
            std::int64_t syncPs;
            std::int64_t markerPs;
            std::int64_t yPs;
            std::int64_t chromaPs;
            std::int64_t linePs;
            std::int64_t imagePs;
        } expected[] {
            {SstvRobotMode::Colour12, "robot-c12", 160U, 120U, 80U, 0U,
             ChromaSubsampling::Cs420, 7'000'000'000LL,
             3'000'000'000LL, 60'000'000'000LL, 30'000'000'000LL,
             100'000'000'000LL, 12'000'000'000'000LL},
            {SstvRobotMode::Colour24, "robot-c24", 320U, 120U, 160U, 4U,
             ChromaSubsampling::Cs422, 12'000'000'000LL,
             6'000'000'000LL, 88'000'000'000LL, 44'000'000'000LL,
             200'000'000'000LL, 24'000'000'000'000LL},
            {SstvRobotMode::Colour36, "robot-c36", 320U, 240U, 160U, 8U,
             ChromaSubsampling::Cs420, 10'500'000'000LL,
             4'500'000'000LL, 90'000'000'000LL, 45'000'000'000LL,
             150'000'000'000LL, 36'000'000'000'000LL},
            {SstvRobotMode::Colour72, "robot-c72", 320U, 240U, 160U, 12U,
             ChromaSubsampling::Cs422, 12'000'000'000LL,
             6'000'000'000LL, 138'000'000'000LL, 69'000'000'000LL,
             300'000'000'000LL, 72'000'000'000'000LL},
            {SstvRobotMode::Bw8, "robot-bw8", 160U, 120U, 0U, 2U,
             ChromaSubsampling::NotApplicable, 10'000'000'000LL, 0LL,
             56'000'000'000LL, 0LL, 66'000'000'000LL,
             7'920'000'000'000LL},
            {SstvRobotMode::Bw12, "robot-bw12", 160U, 120U, 0U, 6U,
             ChromaSubsampling::NotApplicable, 7'000'000'000LL, 0LL,
             93'000'000'000LL, 0LL, 100'000'000'000LL,
             12'000'000'000'000LL},
            {SstvRobotMode::Bw24, "robot-bw24", 320U, 240U, 0U, 10U,
             ChromaSubsampling::NotApplicable, 12'000'000'000LL, 0LL,
             93'000'000'000LL, 0LL, 105'000'000'000LL,
             25'200'000'000'000LL},
            {SstvRobotMode::Bw36, "robot-bw36", 320U, 240U, 0U, 14U,
             ChromaSubsampling::NotApplicable, 12'000'000'000LL, 0LL,
             138'000'000'000LL, 0LL, 150'000'000'000LL,
             36'000'000'000'000LL},
        };
        for (const Expected& item : expected) {
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(item.mode);
            QCOMPARE(QString::fromLatin1(spec.stableId),
                     QString::fromLatin1(item.id));
            QCOMPARE(spec.width, item.width);
            QCOMPARE(spec.height, item.height);
            QCOMPARE(spec.chromaWidth, item.chromaWidth);
            QCOMPARE(spec.visPayload, item.vis);
            QCOMPARE(spec.chromaSubsampling, item.subsampling);
            QCOMPARE(spec.syncDuration.count, item.syncPs);
            QCOMPARE(spec.markerDuration.count, item.markerPs);
            QCOMPARE(spec.luminanceDuration.count, item.yPs);
            QCOMPARE(spec.chromaDuration.count, item.chromaPs);
            QCOMPARE(spec.lineDuration.count, item.linePs);
            QCOMPARE(spec.imageDuration.count, item.imagePs);
            QCOMPARE(SstvRobotEncoder::pixelCount(item.mode),
                     static_cast<std::size_t>(item.width) * item.height);
        }

        for (std::uint8_t payload = 0U; payload < 16U; ++payload) {
            const auto mode = SstvRobotProtocol::modeForVis(payload);
            QVERIFY(mode.has_value());
            if (payload < 4U) {
                QCOMPARE(*mode, payload == 0U
                                  ? SstvRobotMode::Colour12
                                  : SstvRobotMode::Bw8);
            }
        }
        QVERIFY(!SstvRobotProtocol::modeForVis(16U).has_value());
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvRobotProtocol::spec(static_cast<SstvRobotMode>(255U)));
    }

    void mapperUsesEffectiveChromaAndCumulativeClock()
    {
        for (const SstvRobotMode mode : kModes) {
            for (const std::int32_t ppm : {-731, 0, 947}) {
                const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
                const SstvRobotMapper mapper({mode, kRate, ppm});
                QCOMPARE(mapper.imageSampleCount(),
                         oracleSamples(unsignedDuration(spec.imageDuration),
                                       kRate,
                                       ppm));
                QCOMPARE(mapper.lineStartSample(spec.height),
                         mapper.imageSampleCount());
                QCOMPARE(mapper.positionAtSample(0U).region,
                         SstvRobotRegion::Sync);
                const SstvRobotPosition y = mapper.positionAtSample(
                    oracleSamples(unsignedDuration(spec.syncDuration),
                                  kRate,
                                  ppm));
                QCOMPARE(y.region, SstvRobotRegion::Pixel);
                QCOMPARE(y.component,
                         spec.colour ? ColourComponent::Luminance
                                     : ColourComponent::Gray);
                if (spec.colour) {
                    const std::uint64_t markerAt =
                        unsignedDuration(spec.syncDuration)
                        + unsignedDuration(spec.luminanceDuration);
                    const SstvRobotPosition marker =
                        mapper.positionAtSample(
                            oracleSamples(markerAt, kRate, ppm));
                    QCOMPARE(marker.region, SstvRobotRegion::ChromaMarker);
                    QCOMPARE(marker.component,
                             ColourComponent::ChrominanceRed);
                    std::size_t chromaPixels = 0U;
                    std::uint64_t sample = mapper.lineStartSample(0U);
                    while (sample < mapper.lineEndSample(0U)) {
                        const SstvRobotPosition position =
                            mapper.positionAtSample(sample);
                        if (position.region == SstvRobotRegion::Pixel
                            && (position.component
                                    == ColourComponent::ChrominanceRed
                                || position.component
                                    == ColourComponent::ChrominanceBlue)) {
                            ++chromaPixels;
                        }
                        QVERIFY(position.segmentEndSample > sample);
                        sample = position.segmentEndSample;
                    }
                    QCOMPARE(chromaPixels,
                             static_cast<std::size_t>(spec.chromaWidth)
                                 * (spec.chromaSubsampling
                                            == ChromaSubsampling::Cs422
                                        ? 2U : 1U));
                }
            }
        }
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvRobotMapper({SstvRobotMode::Colour36, 7'999U, 0}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvRobotMapper({SstvRobotMode::Colour36, kRate, 100'001}));
    }

    void pinnedLibsstvFixtureSeparatesCompatibleFactsAndDivergences()
    {
        const QJsonObject root = fixtureRoot();
        QCOMPARE(root.value(QStringLiteral("producer")).toString(),
                 QStringLiteral("rimio/libsstv"));
        QCOMPARE(root.value(QStringLiteral("producerCommit")).toString(),
                 QStringLiteral(
                     "193157a993ac34bfa074074004c9ddadcfe6fd15"));
        QCOMPARE(root.value(QStringLiteral("generatorSourceSha256")).toString(),
                 QStringLiteral(
                     "c3367009c3516bf2045a6e398cee32bf1b38d2f1194d9a3814f7f318a1c6958f"));
        QCOMPARE(root.value(QStringLiteral("generationScope")).toString(),
                 QStringLiteral(
                     "developer-only offline oracle; no Decodium runtime dependency"));
        QVERIFY(root.value(QStringLiteral("generatorBuildCommand"))
                    .toString().contains(QStringLiteral("-Werror")));
        QCOMPARE(root.value(QStringLiteral("generatorRunCommand")).toString(),
                 QStringLiteral("bin/robot_fixture"));
        QCOMPARE(root.value(QStringLiteral("sampleRate")).toInt(), 12'000);
        QCOMPARE(root.value(QStringLiteral("headerFrames")).toInt(), 10'920);
        QCOMPARE(root.value(QStringLiteral("modes")).toArray().size(), 8);
        const QJsonArray notes = root.value(
            QStringLiteral("notes")).toArray();
        QVERIFY(std::any_of(
            notes.cbegin(),
            notes.cend(),
            [](const QJsonValue& note) {
                return note.toString().contains(
                    QStringLiteral("not live-radio"));
            }));

        const struct {
            std::uint64_t frames;
            const char* sha256;
        } expected[] {
            {166'440U,
             "332eca7c61d92dfdc9f96771c37c6b2b7da6893a385cd8d626e22e5fbcb0142d"},
            {298'920U,
             "97406292dc0b441e14f23d85a2cf19151127988e1a945c3fb9982cc0a854b26a"},
            {451'560U,
             "74bce0facee5a9d335dc546e0c401de565750ee29a52dc9d433c5250fd2f730d"},
            {874'920U,
             "de46a678de05876d255984a33dfa636c36c2025622fba757645cc3d8d69b84dd"},
            {105'960U,
             "dce197f0b7b43476a471b42fb06c2ac10bee420f06f46a4f91397feace6c0d4e"},
            {154'920U,
             "d0b51e9f0f065b2f350003ec6982766c7ba61c0a7af89e9347c38e282589cc32"},
            {313'320U,
             "bede7062756095e60437afa8e0f86fe2128619deb5b0c9ff0d7cc4c204dbd09f"},
            {442'920U,
             "5102c3098e71ccfe9069134fbc517ae515f4125d177286b6ba9c108f28a85caf"},
        };

        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const SstvRobotMode mode = kModes[index];
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
            const QJsonObject fixture = fixtureMode(
                QString::fromLatin1(spec.stableId));
            QCOMPARE(fixture.value(QStringLiteral("width")).toInt(),
                     static_cast<int>(spec.width));
            QCOMPARE(fixture.value(QStringLiteral("height")).toInt(),
                     static_cast<int>(spec.height));
            QCOMPARE(fixture.value(QStringLiteral("visPayload")).toInt(),
                     static_cast<int>(spec.visPayload));
            const std::uint64_t decodiumFrames =
                static_cast<std::uint64_t>(fixture.value(
                    QStringLiteral("decodiumTotalFrames")).toDouble());
            QCOMPARE(decodiumFrames,
                     std::uint64_t {10'920U}
                         + oracleSamples(unsignedDuration(spec.imageDuration),
                                         kRate,
                                         0));
            const std::uint64_t libFrames =
                static_cast<std::uint64_t>(fixture.value(
                    QStringLiteral("libsstvFullPcmFrames")).toDouble());
            QCOMPARE(libFrames, expected[index].frames);
            QCOMPARE(fixture.value(QStringLiteral("libsstvFullPcmSha256"))
                         .toString(),
                     QString::fromLatin1(expected[index].sha256));
            QVERIFY(!fixture.value(QStringLiteral("landmarks"))
                         .toArray().isEmpty());
            if (mode == SstvRobotMode::Colour12
                || mode == SstvRobotMode::Colour36) {
                QVERIFY(libFrames != decodiumFrames);
            } else {
                QCOMPARE(libFrames, decodiumFrames);
            }
        }
    }

    void encoderStreamsBoundsCancellationAndCanonicalVis()
    {
        for (const SstvRobotMode mode : kModes) {
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
            const auto pixels = sourceImage(mode);
            SstvRobotEncoder encoder(pixels, {mode, kRate, 0, 0.8, 0.95});
            QCOMPARE(encoder.mode(), mode);
            QCOMPARE(encoder.totalSamples(),
                     std::uint64_t {10'920U}
                         + oracleSamples(unsignedDuration(spec.imageDuration),
                                         kRate,
                                         0));
            std::vector<std::int16_t> output(4'093U);
            std::uint64_t streamed = 0U;
            while (!encoder.complete()) {
                const std::size_t produced = encoder.pullPcm16(
                    output.data(), output.size());
                QVERIFY(produced > 0U);
                streamed += produced;
            }
            QCOMPARE(streamed, encoder.totalSamples());
            QCOMPARE(encoder.position().stage,
                     SstvRobotEncoderStage::Complete);
            QCOMPARE(encoder.metrics().residentImageBytes,
                     pixels.size() * sizeof(SstvRgbPixel));
            QCOMPARE(encoder.metrics().tone.samplesGenerated,
                     encoder.totalSamples());
            encoder.reset();
            QCOMPARE(encoder.producedSamples(), std::uint64_t {0U});
            QCOMPARE(encoder.position().stage,
                     SstvRobotEncoderStage::Header);
        }

        const auto pixels = sourceImage(SstvRobotMode::Colour36);
        SstvRobotEncoder encoder(pixels);
        std::vector<float> output(32U);
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullFloat(output.data(),
                              SstvRobotEncoder::MaximumSamplesPerPull + 1U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 encoder.pullFloat(nullptr, 1U));
        std::thread cancel([&encoder] { encoder.cancel(); });
        cancel.join();
        QCOMPARE(encoder.pullFloat(output.data(), output.size()),
                 std::size_t {0U});
        QVERIFY(encoder.cancelled());
    }

    void decoderRoundTripsAllEightFamiliesWithRealSubsampling()
    {
        for (const SstvRobotMode mode : kModes) {
            const SstvRobotModeSpec spec = SstvRobotProtocol::spec(mode);
            const std::vector<SstvRgbPixel> pixels = sourceImage(mode);
            const SstvRobotMapper mapper({mode, kRate, 0});
            SstvRobotDecoder decoder({mode, kRate, 0, 0.0, 0.2, 32U});
            std::uint64_t sequence = 1U;
            for (std::uint32_t line = 0U; line < spec.height; ++line) {
                const SstvRobotLineSync sync {
                    line,
                    kAbsoluteStart + mapper.lineStartSample(line),
                    0.99,
                    false};
                QCOMPARE(decoder.consumeLineSyncs(&sync, 1U),
                         std::size_t {1U});
                const auto observations = lineObservations(
                    pixels, spec, mapper, line, sequence);
                QVERIFY(observations.size()
                        <= SstvRobotDecoder::MaximumObservationsPerConsume);
                QCOMPARE(decoder.consume(observations),
                         observations.size());
            }
            QCOMPARE(decoder.finish(), SstvRobotDecodeState::Complete);
            const SstvImageSnapshot snapshot = decoder.snapshot();
            QVERIFY(snapshot.isComplete());
            QCOMPARE(snapshot.completedPixels, pixels.size());
            QCOMPARE(decoder.metrics().linesPublished,
                     std::uint64_t {spec.height});
            QCOMPARE(decoder.metrics().storedSyncAnchors,
                     static_cast<std::size_t>(spec.height));
            QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                    <= SstvRobotDecoder::MaximumBufferedPixelAccumulators);
            const std::array<std::uint32_t, 4U> rows {{
                0U, 1U, spec.height / 2U, spec.height - 1U}};
            for (const std::uint32_t y : rows) {
                for (const std::uint32_t x :
                     {0U, 1U, spec.width / 2U, spec.width - 1U}) {
                    const SstvRgbPixel source = pixels[
                        static_cast<std::size_t>(y) * spec.width + x];
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
    }

    void decoderFaultsAndCancellationFailClosed()
    {
        SstvRobotDecoder decoder;
        const SstvRobotLineSync bad {
            SstvRobotProtocol::MaximumHeight, 0U, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&bad, 1U), std::size_t {0U});
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            decoder.consume(nullptr,
                            SstvRobotDecoder::MaximumObservationsPerConsume
                                + 1U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 decoder.consume(nullptr, 1U));
        decoder.cancel();
        QCOMPARE(decoder.state(), SstvRobotDecodeState::Cancelled);
        QVERIFY(decoder.snapshot().cancelled);
        QCOMPARE(decoder.finish(), SstvRobotDecodeState::Cancelled);
        decoder.reset();
        QCOMPARE(decoder.state(), SstvRobotDecodeState::Receiving);
        QVERIFY(!decoder.snapshot().cancelled);
    }
};

QTEST_APPLESS_MAIN(TestSstvRobot)

#include "test_sstv_robot.moc"
