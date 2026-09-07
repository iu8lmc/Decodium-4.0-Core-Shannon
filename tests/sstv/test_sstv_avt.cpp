// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvAvt.h"
#include "../../src/sstv/analog/SstvAvtRxSession.h"

#include "../../src/sstv/core/SstvTimingAccumulator.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint32_t kSampleRate = 12'000U;
constexpr std::uint64_t kImageStart = 250'000U;
constexpr double kOffsetHz = 43.0;
constexpr std::array<SstvAvtMode, 3U> kModes {{
    SstvAvtMode::Avt24,
    SstvAvtMode::Avt90,
    SstvAvtMode::Avt94,
}};

QJsonObject readFixture()
{
    QFile file(QString::fromUtf8(DECODIUM_SSTV_AVT_FIXTURE));
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

QJsonObject fixtureMode(const QJsonObject& root, const char* id)
{
    for (const QJsonValue& value : root.value(QStringLiteral("modes")).toArray()) {
        const QJsonObject mode = value.toObject();
        if (mode.value(QStringLiteral("id")).toString()
            == QString::fromLatin1(id)) {
            return mode;
        }
    }
    return {};
}

std::uint8_t valueFor(std::uint32_t x,
                      std::uint32_t y,
                      ColourComponent component) noexcept
{
    const std::uint32_t componentSalt = component == ColourComponent::Red
        ? 17U : (component == ColourComponent::Green ? 83U : 149U);
    return static_cast<std::uint8_t>(
        (x * 29U + y * 11U + componentSalt) & 0xffU);
}

std::vector<SstvRgbPixel> pixelsFor(SstvAvtMode mode)
{
    const SstvAvtModeSpec spec = SstvAvtProtocol::spec(mode);
    std::vector<SstvRgbPixel> result;
    result.reserve(SstvAvtEncoder::pixelCount(mode));
    for (std::uint32_t y = 0U; y < spec.height; ++y) {
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            result.push_back({valueFor(x, y, ColourComponent::Red),
                              valueFor(x, y, ColourComponent::Green),
                              valueFor(x, y, ColourComponent::Blue)});
        }
    }
    return result;
}

SstvFrequencyObservation observation(std::uint64_t sequence,
                                     std::uint64_t sample,
                                     double frequency)
{
    SstvFrequencyObservation result;
    result.status = SstvFrequencyStatus::Valid;
    result.sequence = sequence;
    result.centreSample = sample;
    result.rawFrequencyHz = frequency + kOffsetHz;
    result.correctedFrequencyHz = result.rawFrequencyHz;
    result.rms = 0.5;
    result.snrDb = 35.0;
    result.confidence = 0.99;
    result.validSampleFraction = 1.0;
    return result;
}

std::vector<SstvFrequencyObservation> lineObservations(
    const SstvAvtMapper& mapper,
    std::uint32_t line,
    std::uint64_t& sequence)
{
    const SstvAvtModeSpec spec = mapper.modeSpec();
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(spec.width) * 3U);
    const std::uint64_t first = mapper.lineStartSample(line);
    const std::uint64_t end = mapper.lineEndSample(line);
    std::uint64_t sample = first;
    while (sample < end) {
        const SstvAvtPosition position = mapper.positionAtSample(sample);
        if (!position.valid() || position.line != line) {
            throw std::logic_error("invalid AVT test mapper position");
        }
        const std::uint64_t centre = position.segmentStartSample
            + (position.segmentEndSample - position.segmentStartSample) / 2U;
        const std::uint8_t value = valueFor(
            position.pixel, line, position.component);
        result.push_back(observation(
            sequence++,
            kImageStart + centre,
            SstvAvtProtocol::frequencyForValue(value)));
        if (position.segmentEndSample <= sample) {
            throw std::logic_error("AVT test mapper made no progress");
        }
        sample = position.segmentEndSample;
    }
    return result;
}

std::vector<std::int16_t> renderPrefix(SstvAvtEncoder& encoder,
                                       std::size_t count,
                                       bool fragmented)
{
    constexpr std::array<std::size_t, 6U> chunks {{
        1U, 11U, 509U, 4'093U, 37U, 8'191U}};
    std::vector<std::int16_t> result(count);
    std::size_t produced = 0U;
    std::size_t chunk = 0U;
    while (produced < result.size()) {
        const std::size_t request = fragmented
            ? std::min(chunks[chunk++ % chunks.size()],
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

std::vector<SstvFrequencyObservation> countdownFrameObservations(
    SstvAvtMode mode,
    std::uint8_t counter,
    std::uint64_t frameStart,
    std::uint64_t& sequence,
    std::uint64_t hop = 6U,
    std::uint32_t sampleRate = kSampleRate)
{
    if (hop == 0U) {
        throw std::invalid_argument("AVT test observation hop is zero");
    }
    const SstvAvtSyncFrame frame = SstvAvtSyncCodec::encodeFrame(
        mode, counter, false);
    SstvTimingAccumulator timing(sampleRate);
    std::array<std::uint64_t, SstvAvtSyncCodec::TonesPerFrame + 1U>
        boundaries {};
    for (std::size_t index = 0U; index < frame.tones.size(); ++index) {
        static_cast<void>(timing.samplesFor(frame.tones[index].duration));
        boundaries[index + 1U] = timing.totalSamples();
    }

    std::vector<SstvFrequencyObservation> result;
    const std::uint64_t halfHop = hop / 2U;
    result.push_back(observation(sequence++,
                                 frameStart - halfHop,
                                 1'200.0));
    for (std::size_t tone = 0U; tone < frame.tones.size(); ++tone) {
        for (std::uint64_t sample = boundaries[tone] + halfHop;
             sample < boundaries[tone + 1U];
             sample += hop) {
            result.push_back(observation(sequence++,
                                         frameStart + sample,
                                         frame.tones[tone].frequencyHz));
        }
    }
    result.push_back(observation(sequence++,
                                 frameStart + boundaries.back() + halfHop,
                                 1'200.0));
    return result;
}

} // namespace

class TestSstvAvt final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureAndProtocolTableKeepEffectiveGeometrySeparate()
    {
        const QJsonObject fixture = readFixture();
        QCOMPARE(fixture.value(QStringLiteral("schema")).toInt(), 1);
        QCOMPARE(fixture.value(QStringLiteral("kind")).toString(),
                 QStringLiteral("clean-room-source-landmarks"));
        QCOMPARE(fixture.value(QStringLiteral("sources")).toArray().size(), 3);
        const QJsonArray sources = fixture.value(
            QStringLiteral("sources")).toArray();
        QCOMPARE(sources.at(0).toObject().value(
                     QStringLiteral("sha256")).toString(),
                 QStringLiteral(
                     "e244de9d5cbba525d33b25906c3751ab0ed62af2a3b373feffda44de4f13909d"));
        QCOMPARE(sources.at(0).toObject().value(
                     QStringLiteral("pages")).toInt(),
                 175);
        QCOMPARE(sources.at(1).toObject().value(
                     QStringLiteral("commit")).toString(),
                 QStringLiteral(
                     "8c27d6d169d8c6c197eb47c2089870e39bc06a02"));
        QCOMPARE(sources.at(2).toObject().value(
                     QStringLiteral("commit")).toString(),
                 QStringLiteral(
                     "8060b5f1e9727b0052d74108081c6db7b26babad"));
        QCOMPARE(fixture.value(QStringLiteral("catalogueOnlyVariants"))
                     .toArray().size(),
                 3);

        constexpr std::array<const char*, 3U> ids {{
            "avt-24", "avt-90", "avt-94"}};
        constexpr std::array<std::uint32_t, 3U> widths {{128U, 320U, 320U}};
        constexpr std::array<std::uint32_t, 3U> effective {{128U, 256U, 320U}};
        constexpr std::array<std::uint32_t, 3U> heights {{120U, 240U, 200U}};
        constexpr std::array<std::uint8_t, 3U> vis {{64U, 68U, 72U}};
        constexpr std::array<std::int64_t, 3U> component {{
            62'500'000'000LL, 125'000'000'000LL, 156'250'000'000LL}};
        constexpr std::array<std::int64_t, 3U> image {{
            22'500'000'000'000LL,
            90'000'000'000'000LL,
            93'750'000'000'000LL}};

        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const SstvAvtModeSpec spec = SstvAvtProtocol::spec(kModes[index]);
            QCOMPARE(QByteArray(spec.stableId), QByteArray(ids[index]));
            QCOMPARE(spec.width, widths[index]);
            QCOMPARE(spec.effectiveSampledWidth, effective[index]);
            QCOMPARE(spec.height, heights[index]);
            QCOMPARE(spec.visPayload, vis[index]);
            QCOMPARE(spec.componentDuration.count, component[index]);
            QCOMPARE(spec.lineDuration.count, component[index] * 3LL);
            QCOMPARE(spec.imageDuration.count, image[index]);
            QCOMPARE(SstvAvtProtocol::normalModeForVis(vis[index]),
                     std::optional<SstvAvtMode> {kModes[index]});

            const QJsonObject landmark = fixtureMode(fixture, ids[index]);
            QVERIFY(!landmark.isEmpty());
            QCOMPARE(landmark.value(QStringLiteral("preparedWidth")).toInt(),
                     static_cast<int>(widths[index]));
            QCOMPARE(landmark.value(QStringLiteral("effectiveWidth")).toInt(),
                     static_cast<int>(effective[index]));
            QCOMPARE(landmark.value(QStringLiteral("height")).toInt(),
                     static_cast<int>(heights[index]));
            QCOMPARE(landmark.value(QStringLiteral("visNormal")).toInt(),
                     static_cast<int>(vis[index]));
        }
        QVERIFY(!SstvAvtProtocol::normalModeForVis(65U).has_value());
        QVERIFY(!SstvAvtProtocol::normalModeForVis(70U).has_value());
        QVERIFY(!SstvAvtProtocol::normalModeForVis(75U).has_value());
    }

    void physicalHeaderIsTripleVisThenExactCountdown()
    {
        for (const SstvAvtMode mode : kModes) {
            const SstvAvtModeSpec spec = SstvAvtProtocol::spec(mode);
            const std::vector<SstvAvtToneSegment> header =
                SstvAvtProtocol::normalHeader(mode);
            QCOMPARE(header.size(), SstvAvtProtocol::HeaderSegmentCount);
            for (std::size_t repeat = 0U; repeat < 3U; ++repeat) {
                const std::size_t base =
                    repeat * SstvAvtProtocol::StandardVisSegmentCount;
                QCOMPARE(header[base].frequencyHz, 1'900.0);
                QCOMPARE(header[base].duration.count,
                         std::int64_t {300'000'000'000LL});
                QCOMPARE(header[base + 1U].frequencyHz, 1'200.0);
                QCOMPARE(header[base + 2U].frequencyHz, 1'900.0);
                for (std::size_t segment = 3U;
                     segment < SstvAvtProtocol::StandardVisSegmentCount;
                     ++segment) {
                    QCOMPARE(header[base + segment].duration.count,
                             std::int64_t {30'000'000'000LL});
                }
            }
            const std::vector<SstvAvtSyncTone> countdown =
                SstvAvtSyncCodec::encodeCountdown(mode, false);
            QCOMPARE(countdown.size(),
                     SstvAvtSyncCodec::CountdownToneCount);
            for (std::size_t index = 0U; index < countdown.size(); ++index) {
                const auto& actual = header[
                    SstvAvtProtocol::TripleVisSegmentCount + index];
                QCOMPARE(actual.frequencyHz, countdown[index].frequencyHz);
                QCOMPARE(actual.duration, countdown[index].duration);
            }
            std::int64_t total = 0LL;
            for (const SstvAvtToneSegment& segment : header) {
                total += segment.duration.count;
            }
            QCOMPARE(total, SstvAvtProtocol::HeaderDuration.count);
            QCOMPARE(SstvAvtSyncCodec::modeForVis(spec.visPayload),
                     std::optional<SstvAvtMode> {mode});
        }
    }

    void cumulativeMapperHasOnlyContinuousRgbPixels()
    {
        constexpr std::array<std::uint64_t, 3U> expectedSamples {{
            270'000U, 1'080'000U, 1'125'000U}};
        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const SstvAvtModeSpec spec = SstvAvtProtocol::spec(kModes[index]);
            const SstvAvtMapper mapper({kModes[index], kSampleRate, 0});
            QCOMPARE(mapper.imageSampleCount(), expectedSamples[index]);
            QCOMPARE(mapper.lineStartSample(0U), std::uint64_t {0U});
            QCOMPARE(mapper.lineStartSample(spec.height),
                     expectedSamples[index]);
            const SstvAvtPosition first = mapper.positionAtSample(0U);
            QCOMPARE(first.region, SstvAvtRegion::Pixel);
            QCOMPARE(first.line, 0U);
            QCOMPARE(first.component, ColourComponent::Red);
            QCOMPARE(first.pixel, 0U);
            QCOMPARE(mapper.positionAtElapsedTime(Picoseconds {-1LL}).region,
                     SstvAvtRegion::Outside);
            QVERIFY(mapper.positionAtElapsedTime(Picoseconds {0LL}).valid());

            const std::uint64_t firstLineEnd = mapper.lineEndSample(0U);
            const SstvAvtPosition green = mapper.positionAtSample(
                firstLineEnd / 3U);
            QCOMPARE(green.region, SstvAvtRegion::Pixel);
            QCOMPARE(green.component, ColourComponent::Green);
            const SstvAvtPosition blue = mapper.positionAtSample(
                firstLineEnd * 2U / 3U);
            QCOMPARE(blue.component, ColourComponent::Blue);
            const SstvAvtPosition complete = mapper.positionAtSample(
                mapper.imageSampleCount());
            QCOMPARE(complete.region, SstvAvtRegion::Complete);
        }
    }

    void encoderIsBoundedChunkInvariantAndUsesCorrectAvt90Prefix()
    {
        constexpr std::array<std::uint64_t, 3U> totals {{
            366'510U, 1'176'510U, 1'221'510U}};
        for (std::size_t index = 0U; index < kModes.size(); ++index) {
            const std::vector<SstvRgbPixel> pixels = pixelsFor(kModes[index]);
            SstvAvtEncoder encoder(pixels,
                                   {kModes[index], kSampleRate, 0, 0.7, 0.95});
            QCOMPARE(encoder.headerSamples(), std::uint64_t {96'510U});
            QCOMPARE(encoder.totalSamples(), totals[index]);
            QCOMPARE(encoder.position().stage, SstvAvtEncoderStage::Header);
            std::vector<std::int16_t> header(
                static_cast<std::size_t>(encoder.headerSamples()));
            QCOMPARE(encoder.pullPcm16(header.data(), header.size()),
                     header.size());
            const SstvAvtEncoderPosition image = encoder.position();
            QCOMPARE(image.stage, SstvAvtEncoderStage::Image);
            QCOMPARE(image.image.line, 0U);
            QCOMPARE(image.image.component, ColourComponent::Red);
            QCOMPARE(image.image.pixel, 0U);
            QVERIFY(encoder.metrics().residentImageBytes
                    == pixels.size() * sizeof(SstvRgbPixel));
            QVERIFY(encoder.metrics().residentHeaderBytes > 0U);
        }

        const std::vector<SstvRgbPixel> pixels = pixelsFor(SstvAvtMode::Avt24);
        SstvAvtEncoder contiguous(pixels,
                                 {SstvAvtMode::Avt24, kSampleRate});
        SstvAvtEncoder fragmented(pixels,
                                 {SstvAvtMode::Avt24, kSampleRate});
        const std::size_t prefix = static_cast<std::size_t>(
            contiguous.headerSamples() + 20'000U);
        QCOMPARE(renderPrefix(contiguous, prefix, false),
                 renderPrefix(fragmented, prefix, true));

        const SstvAvtSyncFrame avt90 = SstvAvtSyncCodec::encodeFrame(
            SstvAvtMode::Avt90, 0U, false);
        QCOMPARE(static_cast<unsigned>(avt90.normalWord >> 5U), 0b101U);
        QCOMPARE(avt90.normalWord, std::uint8_t {0xa0U});
        QCOMPARE(avt90.invertedWord, std::uint8_t {0x5fU});
    }

    void decoderReconstructsEveryModeWithoutInventingLineSync()
    {
        for (const SstvAvtMode mode : kModes) {
            const SstvAvtModeSpec spec = SstvAvtProtocol::spec(mode);
            const SstvAvtMapper mapper({mode, kSampleRate, 0});
            SstvAvtDecoderConfig config;
            config.mode = mode;
            config.sampleRate = kSampleRate;
            config.imageStartSample = kImageStart;
            config.observationSpanSamples = 1U;
            config.frequencyOffsetHz = kOffsetHz;
            config.maximumInterpolationGapPixels = 0U;
            SstvAvtDecoder decoder(config);
            std::uint64_t sequence = 1U;
            for (std::uint32_t line = 0U; line < spec.height; ++line) {
                const std::vector<SstvFrequencyObservation> observations =
                    lineObservations(mapper, line, sequence);
                QCOMPARE(decoder.consume(observations), observations.size());
            }
            QCOMPARE(decoder.finish(), SstvAvtDecodeState::Complete);
            const SstvImageSnapshot snapshot = decoder.snapshot();
            QCOMPARE(snapshot.width, spec.width);
            QCOMPARE(snapshot.height, spec.height);
            QVERIFY(snapshot.isComplete());
            QCOMPARE(decoder.metrics().linesPublished,
                     static_cast<std::uint64_t>(spec.height));
            QCOMPARE(decoder.metrics().interpolatedPixels,
                     std::uint64_t {0U});
            for (const std::uint32_t y : {0U, spec.height / 2U,
                                          spec.height - 1U}) {
                for (const std::uint32_t x : {0U, spec.width / 2U,
                                              spec.width - 1U}) {
                    const SstvRgbPixel& pixel = snapshot.pixel(x, y);
                    QCOMPARE(pixel.red,
                             valueFor(x, y, ColourComponent::Red));
                    QCOMPARE(pixel.green,
                             valueFor(x, y, ColourComponent::Green));
                    QCOMPARE(pixel.blue,
                             valueFor(x, y, ColourComponent::Blue));
                }
            }
        }
    }

    void countdownDetectorAnchorsFromAnyProtectedFrame()
    {
        constexpr std::uint64_t frameStart = 400'000U;
        constexpr std::uint8_t counter = 7U;
        std::uint64_t sequence = 1U;
        const std::vector<SstvFrequencyObservation> observations =
            countdownFrameObservations(SstvAvtMode::Avt90,
                                       counter,
                                       frameStart,
                                       sequence);
        SstvAvtCountdownDetectorConfig config;
        config.expectedMode = SstvAvtMode::Avt90;
        config.sampleRate = kSampleRate;
        config.searchStartSample = frameStart - 20U;
        config.observationSpanSamples = 6U;
        config.frequencyOffsetHz = kOffsetHz;
        SstvAvtCountdownDetector detector(config);

        std::optional<SstvAvtCountdownDetection> detection;
        std::size_t offset = 0U;
        while (offset < observations.size() && !detection.has_value()) {
            const std::size_t count = std::min<std::size_t>(
                37U, observations.size() - offset);
            detection = detector.consume(observations.data() + offset, count);
            offset += count;
        }
        QVERIFY(detection.has_value());
        QVERIFY(detection->acquired);
        QCOMPARE(detection->mode, SstvAvtMode::Avt90);
        QCOMPARE(detection->counter, counter);
        QCOMPARE(detection->frameStartSample, frameStart);
        QCOMPARE(detector.state(), SstvAvtCountdownDetectorState::Acquired);
        QCOMPARE(detector.metrics().framesDecoded, std::uint64_t {1U});
        QVERIFY(detection->confidence > 0.95);

        SstvTimingAccumulator countdownTiming(kSampleRate);
        static_cast<void>(countdownTiming.samplesFor(
            SstvAvtSyncCodec::CountdownDuration));
        SstvTimingAccumulator elapsedTiming(kSampleRate);
        static_cast<void>(elapsedTiming.samplesFor(Picoseconds {
            SstvAvtSyncCodec::FrameDuration.count
            * static_cast<std::int64_t>(counter)}));
        QCOMPARE(detection->imageStartSample,
                 frameStart + countdownTiming.totalSamples()
                     - elapsedTiming.totalSamples());
        QVERIFY(detector.metrics().peakBufferedObservations
                <= SstvAvtSyncCodec::TonesPerFrame);
    }

    void cumulativeCountdownAnchorMatchesEveryCounterAtCommonRates()
    {
        constexpr std::array<std::uint32_t, 3U> rates {{
            8'000U, 12'000U, 44'100U}};
        for (const std::uint32_t rate : rates) {
            const std::uint64_t hop = std::max<std::uint64_t>(
                1U, rate / 2'000U);
            for (std::uint8_t counter = 0U; counter < 32U; ++counter) {
                std::uint64_t sequence = 1U;
                constexpr std::uint64_t frameStart = 500'000U;
                const std::vector<SstvFrequencyObservation> observations =
                    countdownFrameObservations(SstvAvtMode::Avt90,
                                               counter,
                                               frameStart,
                                               sequence,
                                               hop,
                                               rate);
                SstvAvtCountdownDetectorConfig config;
                config.expectedMode = SstvAvtMode::Avt90;
                config.sampleRate = rate;
                config.searchStartSample = frameStart;
                config.observationSpanSamples =
                    static_cast<std::uint32_t>(hop);
                config.frequencyOffsetHz = kOffsetHz;
                SstvAvtCountdownDetector detector(config);
                const auto detection = detector.consume(observations);
                QVERIFY(detection.has_value());

                SstvTimingAccumulator countdownTiming(rate);
                static_cast<void>(countdownTiming.samplesFor(
                    SstvAvtSyncCodec::CountdownDuration));
                SstvTimingAccumulator beforeTiming(rate);
                static_cast<void>(beforeTiming.samplesFor(Picoseconds {
                    SstvAvtSyncCodec::FrameDuration.count
                    * static_cast<std::int64_t>(counter)}));
                SstvTimingAccumulator throughTiming(rate);
                static_cast<void>(throughTiming.samplesFor(Picoseconds {
                    SstvAvtSyncCodec::FrameDuration.count
                    * static_cast<std::int64_t>(counter + 1U)}));
                QCOMPARE(detection->imageStartSample,
                         frameStart + countdownTiming.totalSamples()
                             - beforeTiming.totalSamples());
                QCOMPARE(detection->frameEndSample,
                         frameStart + throughTiming.totalSamples()
                             - beforeTiming.totalSamples());
            }
        }
    }

    void oneSampleFrontendHopKeepsCountdownMemoryFixed()
    {
        std::uint64_t sequence = 1U;
        constexpr std::uint64_t frameStart = 100'000U;
        const std::vector<SstvFrequencyObservation> observations =
            countdownFrameObservations(SstvAvtMode::Avt94,
                                       11U,
                                       frameStart,
                                       sequence,
                                       1U);
        QVERIFY(observations.size() > 1'024U);

        SstvAvtCountdownDetectorConfig config;
        config.expectedMode = SstvAvtMode::Avt94;
        config.sampleRate = kSampleRate;
        config.searchStartSample = frameStart;
        config.observationSpanSamples = 1U;
        config.frequencyOffsetHz = kOffsetHz;
        SstvAvtCountdownDetector detector(config);

        std::optional<SstvAvtCountdownDetection> detection;
        std::size_t offset = 0U;
        while (offset < observations.size() && !detection.has_value()) {
            const std::size_t count = std::min<std::size_t>(
                731U, observations.size() - offset);
            detection = detector.consume(observations.data() + offset,
                                         count);
            offset += count;
        }
        QVERIFY(detection.has_value());
        QCOMPARE(detection->mode, SstvAvtMode::Avt94);
        QCOMPARE(detection->counter, std::uint8_t {11U});
        QCOMPARE(detector.metrics().bufferedObservations,
                 std::size_t {0U});
        QVERIFY(detector.metrics().peakBufferedObservations
                <= SstvAvtSyncCodec::TonesPerFrame);
    }

    void realFrequencyFrontendHopOneAcquiresEncoderCountdown()
    {
        const std::vector<SstvRgbPixel> pixels = pixelsFor(
            SstvAvtMode::Avt24);
        SstvAvtEncoder encoder(pixels,
                               {SstvAvtMode::Avt24,
                                kSampleRate,
                                0,
                                0.7,
                                0.95});
        SstvTimingAccumulator timing(kSampleRate);
        static_cast<void>(timing.samplesFor(
            SstvAvtProtocol::TripleVisDuration));
        const std::uint64_t countdownStart = timing.totalSamples();
        static_cast<void>(timing.samplesFor(
            SstvAvtSyncCodec::FrameDuration));
        const std::size_t prefixSamples = static_cast<std::size_t>(
            timing.totalSamples() + 256U);
        std::vector<std::int16_t> pcm(prefixSamples);
        QCOMPARE(encoder.pullPcm16(pcm.data(), pcm.size()), pcm.size());

        SstvFrequencyDemodulatorConfig demodConfig =
            SstvFrequencyDemodulatorConfig::sstvDefaults();
        demodConfig.averagingSamples = 3U;
        demodConfig.hopSamples = 1U;
        SstvFrequencyDemodulator demodulator(demodConfig);
        SstvAvtCountdownDetectorConfig detectorConfig;
        detectorConfig.expectedMode = SstvAvtMode::Avt24;
        detectorConfig.sampleRate = kSampleRate;
        detectorConfig.searchStartSample = countdownStart - 32U;
        detectorConfig.observationSpanSamples = static_cast<std::uint32_t>(
            demodConfig.averagingSamples);
        detectorConfig.minimumObservationConfidence = 0.20;
        SstvAvtCountdownDetector detector(detectorConfig);

        std::optional<SstvAvtCountdownDetection> detection;
        constexpr std::size_t chunkSamples = 2'048U;
        std::vector<float> floating(chunkSamples);
        for (std::size_t offset = 0U;
             offset < pcm.size() && !detection.has_value();
             offset += chunkSamples) {
            const std::size_t count = std::min(
                chunkSamples, pcm.size() - offset);
            for (std::size_t index = 0U; index < count; ++index) {
                floating[index] = static_cast<float>(pcm[offset + index])
                    / 32'768.0F;
            }
            const std::vector<SstvFrequencyObservation> observations =
                demodulator.consume(floating.data(), count);
            detection = detector.consume(observations);
        }
        QVERIFY(detection.has_value());
        QCOMPARE(detection->counter, std::uint8_t {0U});
        const std::uint64_t error = detection->imageStartSample
                > encoder.headerSamples()
            ? detection->imageStartSample - encoder.headerSamples()
            : encoder.headerSamples() - detection->imageStartSample;
        QVERIFY2(error <= 2U,
                 qPrintable(QStringLiteral("AVT countdown anchor error: %1")
                                .arg(static_cast<qulonglong>(error))));
    }

    void fullAvt24PcmLoopbackIsChunkedAndCompletesProgressively()
    {
        const std::vector<SstvRgbPixel> expected = pixelsFor(
            SstvAvtMode::Avt24);
        SstvAvtEncoder encoder(expected,
                               {SstvAvtMode::Avt24,
                                kSampleRate,
                                0,
                                0.7,
                                0.95});
        SstvFrequencyDemodulatorConfig demodConfig =
            SstvFrequencyDemodulatorConfig::sstvDefaults();
        demodConfig.averagingSamples = 3U;
        demodConfig.hopSamples = 1U;
        SstvFrequencyDemodulator demodulator(demodConfig);

        SstvTimingAccumulator timing(kSampleRate);
        static_cast<void>(timing.samplesFor(
            SstvAvtProtocol::TripleVisDuration));
        const std::uint64_t countdownStart = timing.totalSamples();
        SstvAvtCountdownDetectorConfig detectorConfig;
        detectorConfig.expectedMode = SstvAvtMode::Avt24;
        detectorConfig.sampleRate = kSampleRate;
        detectorConfig.searchStartSample = countdownStart - 64U;
        detectorConfig.observationSpanSamples = static_cast<std::uint32_t>(
            demodConfig.averagingSamples);
        SstvAvtCountdownDetector detector(detectorConfig);
        std::unique_ptr<SstvAvtRxSession> session;

        constexpr std::array<std::size_t, 6U> chunks {{
            1U, 73U, 2'048U, 8'192U, 509U, 4'093U}};
        std::vector<std::int16_t> pcm(8'192U);
        std::vector<float> floating(8'192U);
        std::size_t chunkIndex = 0U;
        while (!encoder.complete()) {
            const std::size_t request = chunks[
                chunkIndex++ % chunks.size()];
            const std::size_t produced = encoder.pullPcm16(pcm.data(),
                                                           request);
            QVERIFY(produced > 0U);
            for (std::size_t index = 0U; index < produced; ++index) {
                floating[index] = static_cast<float>(pcm[index])
                    / 32'768.0F;
            }
            const std::vector<SstvFrequencyObservation> observations =
                demodulator.consume(floating.data(), produced);
            QVERIFY(observations.size()
                    <= SstvAvtCountdownDetector::MaximumObservationsPerConsume);
            if (!session) {
                const auto detection = detector.consume(observations);
                if (detection.has_value()) {
                    SstvAvtRxSessionConfig sessionConfig;
                    sessionConfig.mode = detection->mode;
                    sessionConfig.sampleRate = kSampleRate;
                    sessionConfig.imageStartSample =
                        detection->imageStartSample;
                    sessionConfig.observationSpanSamples =
                        static_cast<std::uint32_t>(demodConfig.hopSamples);
                    session = std::make_unique<SstvAvtRxSession>(
                        sessionConfig);
                }
            }
            if (session) {
                static_cast<void>(session->consume(observations));
            }
        }
        QVERIFY(session != nullptr);
        QVERIFY(session->decoderMetrics().linesPublished > 0U);

        // Flush only the frontend's finite analytic/averaging delay. These
        // samples are not part of the encoded AVT payload.
        std::vector<float> silence(256U, 0.0F);
        const std::vector<SstvFrequencyObservation> trailing =
            demodulator.consume(silence);
        QVERIFY(trailing.size()
                <= SstvAvtCountdownDetector::MaximumObservationsPerConsume);
        static_cast<void>(session->consume(trailing));
        QCOMPARE(session->finish(), SstvAvtRxSessionState::Complete);
        QCOMPARE(session->decoderMetrics().linesPublished,
                 std::uint64_t {120U});

        const SstvImageSnapshot actual = session->snapshot();
        QVERIFY(actual.isComplete());
        QCOMPARE(actual.width, std::uint32_t {128U});
        QCOMPARE(actual.height, std::uint32_t {120U});
        std::uint64_t absoluteError = 0U;
        for (std::uint32_t y = 0U; y < actual.height; ++y) {
            for (std::uint32_t x = 0U; x < actual.width; ++x) {
                const std::size_t index = static_cast<std::size_t>(y)
                    * actual.width + x;
                const SstvRgbPixel decoded = actual.pixel(x, y);
                absoluteError += static_cast<std::uint64_t>(std::abs(
                    static_cast<int>(decoded.red)
                    - static_cast<int>(expected[index].red)));
                absoluteError += static_cast<std::uint64_t>(std::abs(
                    static_cast<int>(decoded.green)
                    - static_cast<int>(expected[index].green)));
                absoluteError += static_cast<std::uint64_t>(std::abs(
                    static_cast<int>(decoded.blue)
                    - static_cast<int>(expected[index].blue)));
            }
        }
        const double meanAbsoluteError = static_cast<double>(absoluteError)
            / static_cast<double>(expected.size() * 3U);
        QVERIFY2(meanAbsoluteError < 35.0,
                 qPrintable(QStringLiteral("AVT24 loopback MAE: %1")
                                .arg(meanAbsoluteError, 0, 'f', 3)));
    }

    void malformedInputAndWrongModeFailClosed()
    {
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvAvtMapper({SstvAvtMode::Avt24, 1U, 0}));
        SstvAvtDecoderConfig invalid;
        invalid.maximumInterpolationGapPixels =
            SstvAvtDecoder::MaximumInterpolationGapPixels + 1U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvAvtDecoder {invalid});

        const std::vector<SstvRgbPixel> pixels = pixelsFor(SstvAvtMode::Avt24);
        SstvAvtEncoder encoder(pixels);
        std::int16_t sample = 0;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullPcm16(
                &sample, SstvAvtEncoder::MaximumSamplesPerPull + 1U));

        std::uint64_t sequence = 1U;
        const auto wrong = countdownFrameObservations(
            SstvAvtMode::Avt24, 0U, 100'000U, sequence);
        SstvAvtCountdownDetectorConfig config;
        config.expectedMode = SstvAvtMode::Avt90;
        config.searchStartSample = 99'000U;
        config.frequencyOffsetHz = kOffsetHz;
        SstvAvtCountdownDetector detector(config);
        QVERIFY(!detector.consume(wrong).has_value());
        QVERIFY(detector.metrics().framesRejected >= 1U);
        QVERIFY(!detector.detection().has_value());
    }
};

QTEST_APPLESS_MAIN(TestSstvAvt)
#include "test_sstv_avt.moc"
