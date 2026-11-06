// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvAvt.h"
#include "../../src/sstv/analog/SstvAvtRxSession.h"

#include "../../src/sstv/core/SstvTimingAccumulator.h"
#include "../../src/sstv/dsp/SstvResampler.h"
#include "../../src/sstv/dsp/SstvPreprocessor.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
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
        if (!position.valid()) {
            throw std::logic_error("invalid AVT test mapper position");
        }
        const std::uint64_t centre = position.segmentStartSample
            + (position.segmentEndSample - position.segmentStartSample) / 2U;
        const std::uint8_t value = valueFor(
            position.pixel, position.line, position.component);
        result.push_back(observation(
            sequence++,
            kImageStart + centre,
            SstvAvtProtocol::frequencyForValue(value, mapper.config().variant)));
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
    std::uint32_t sampleRate = kSampleRate,
    bool narrow = false)
{
    if (hop == 0U) {
        throw std::invalid_argument("AVT test observation hop is zero");
    }
    const SstvAvtSyncFrame frame = SstvAvtSyncCodec::encodeFrame(
        mode, counter, narrow);
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
    void terminalHoldPreservesFinalBlueTone()
    {
        for (const auto blue : {std::uint8_t {0}, std::uint8_t {255}}) {
            auto pixels = pixelsFor(SstvAvtMode::Avt94);
            pixels.back().blue = blue;
            SstvAvtEncoderConfig config;
            config.mode = SstvAvtMode::Avt94;
            config.variant = SstvAvtVariant::NarrowQrm;
            config.sampleRate = kSampleRate;
            SstvAvtEncoder reference(pixels, config);
            config.terminalHoldMs = 10U;
            SstvAvtEncoder held(pixels, config);
            QCOMPARE(held.totalSamples(), reference.totalSamples() + 120U);
            std::array<std::int16_t, 4096> normal {}, extended {};
            std::uint64_t remaining = reference.totalSamples();
            while (remaining) {
                const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, normal.size()));
                QCOMPARE(reference.pullPcm16(normal.data(), count), count);
                QCOMPARE(held.pullPcm16(extended.data(), count), count);
                QVERIFY(std::equal(normal.begin(), normal.begin() + count, extended.begin()));
                remaining -= count;
            }
            const auto tail = held.position();
            QCOMPARE(tail.stage, SstvAvtEncoderStage::TerminalHold);
            QCOMPARE(tail.image.component, ColourComponent::Blue);
            QCOMPARE(tail.frequencyHz, blue == 0U ? 1700.0 : 2100.0);
            QCOMPARE(held.pullPcm16(extended.data(), 120U), std::size_t {120U});
            QVERIFY(held.complete());
            double re = 0, im = 0;
            for (std::size_t i = 0; i < 120U; ++i) {
                const double phase = 2.0 * std::acos(-1.0) * tail.frequencyHz * double(i) / kSampleRate;
                re += extended[i] * std::cos(phase);
                im += extended[i] * std::sin(phase);
            }
            QVERIFY(std::hypot(re, im) > 100000.0);
        }
        auto pixels = pixelsFor(SstvAvtMode::Avt24);
        SstvAvtEncoderConfig invalid;
        invalid.terminalHoldMs = 101U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvAvtEncoder(pixels, invalid));
    }
    void countdownAcquiresNarrowAndNormalVariants()
    {
        constexpr std::uint64_t frameStart = 400'000U;
        for (const auto variant : {SstvAvtVariant::Normal, SstvAvtVariant::Narrow,
                                   SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
            std::uint64_t sequence = 1U;
            const auto observations = countdownFrameObservations(
                SstvAvtMode::Avt90, 7U, frameStart, sequence, 6U,
                kSampleRate, SstvAvtSyncCodec::isNarrow(variant));
            SstvAvtCountdownDetectorConfig config;
            config.expectedMode = SstvAvtMode::Avt90;
            config.searchStartSample = frameStart - 20U;
            config.frequencyOffsetHz = kOffsetHz;
            config.variant = variant;
            const auto result = SstvAvtCountdownDetector(config).consume(observations);
            QVERIFY(result.has_value());
            QCOMPARE(result->variant, variant);
            QCOMPARE(result->counter, std::uint8_t {7U});
            QCOMPARE(result->frameStartSample, frameStart);
        }
    }

    void variantLandmarksCompareNormalAndQrm()
    {
        for (const auto mode : kModes) {
            const auto spec = SstvAvtProtocol::spec(mode);
            for (const auto variant : {SstvAvtVariant::Normal, SstvAvtVariant::Narrow,
                                       SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
                const bool narrow = SstvAvtSyncCodec::isNarrow(variant);
                const bool qrm = SstvAvtSyncCodec::isQrm(variant);
                const SstvAvtMapper mapper({mode, kSampleRate, 0, variant});
                for (std::uint32_t wireLine = 0; wireLine < spec.height; ++wireLine) {
                    const auto pos = mapper.positionAtSample(mapper.lineStartSample(wireLine));
                    const auto expected = qrm
                        ? (wireLine < spec.height / 2U ? wireLine * 2U
                           : (wireLine - spec.height / 2U) * 2U + 1U)
                        : wireLine;
                    QCOMPARE(pos.line, expected);
                    const auto byTime = mapper.positionAtElapsedTime(
                        Picoseconds {spec.lineDuration.count * wireLine});
                    QCOMPARE(byTime.line, expected);
                }
                QCOMPARE(SstvAvtProtocol::frequencyForValue(0, variant), narrow ? 1700.0 : 1500.0);
                QCOMPARE(SstvAvtProtocol::frequencyForValue(255, variant), narrow ? 2100.0 : 2300.0);
                for (unsigned value = 0; value < 256; ++value) {
                    const auto frequency = SstvAvtProtocol::frequencyForValue(
                        static_cast<std::uint8_t>(value), variant);
                    QCOMPARE(SstvAvtProtocol::valueForFrequency(frequency, variant),
                             static_cast<std::uint8_t>(value));
                }
                const auto header = SstvAvtProtocol::header(mode, variant);
                const auto expectedCountdown = SstvAvtSyncCodec::encodeCountdown(mode, narrow);
                QCOMPARE(header.size(), SstvAvtProtocol::HeaderSegmentCount);
                for (std::size_t i = 0; i < expectedCountdown.size(); ++i) {
                    QCOMPARE(header[SstvAvtProtocol::TripleVisSegmentCount + i].frequencyHz,
                             expectedCountdown[i].frequencyHz);
                }
            }
        }
    }
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

    void qrmSessionClosesOnSilentTailWithoutInventingMissingRows()
    {
        const auto mode = SstvAvtMode::Avt24;
        const auto spec = SstvAvtProtocol::spec(mode);
        for (const auto variant : {SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
            const SstvAvtMapper mapper({mode, kSampleRate, 0, variant});
            SstvAvtRxSessionConfig config;
            config.mode = mode;
            config.variant = variant;
            config.sampleRate = kSampleRate;
            config.imageStartSample = kImageStart;
            config.observationSpanSamples = 1U;
            config.frequencyOffsetHz = kOffsetHz;
            config.maximumInterpolationGapPixels = 0U;
            SstvAvtRxSession complete(config), missing(config);
            std::uint64_t sequence = 1U;
            for (std::uint32_t line = 0; line < spec.height; ++line) {
                auto observations = lineObservations(mapper, line, sequence);
                complete.consume(observations);
                if (line == spec.height / 2U) {
                    for (auto& item : observations)
                        item.status = SstvFrequencyStatus::LowSignal;
                }
                missing.consume(observations);
            }
            auto tail = observation(sequence, complete.imageEndSample(), 1900.0);
            tail.status = SstvFrequencyStatus::LowSignal;
            complete.consume(&tail, 1U);
            missing.consume(&tail, 1U);
            QCOMPARE(complete.state(), SstvAvtRxSessionState::Complete);
            QCOMPARE(missing.state(), SstvAvtRxSessionState::Partial);
            QVERIFY(complete.snapshot().isComplete());
            QVERIFY(!missing.snapshot().isComplete());
            QCOMPARE(complete.decoderMetrics().linesPublished,
                     static_cast<std::uint64_t>(spec.height));
            QCOMPARE(missing.decoderMetrics().linesPublished,
                     static_cast<std::uint64_t>(spec.height - 1U));
        }
    }

    void variantDecoderReconstructsDistinctRows()
    {
        const auto mode = SstvAvtMode::Avt24;
        const auto spec = SstvAvtProtocol::spec(mode);
        for (const auto variant : {SstvAvtVariant::Normal, SstvAvtVariant::Narrow,
                                   SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
            const SstvAvtMapper mapper({mode, kSampleRate, 0, variant});
            SstvAvtDecoderConfig config;
            config.mode = mode;
            config.variant = variant;
            config.imageStartSample = kImageStart;
            config.observationSpanSamples = 1U;
            config.frequencyOffsetHz = kOffsetHz;
            config.maximumInterpolationGapPixels = 0U;
            SstvAvtDecoder decoder(config);
            std::uint64_t sequence = 1U;
            for (std::uint32_t line = 0; line < spec.height; ++line) {
                const auto observations = lineObservations(mapper, line, sequence);
                QCOMPARE(decoder.consume(observations), observations.size());
            }
            QCOMPARE(decoder.finish(), SstvAvtDecodeState::Complete);
            const auto snapshot = decoder.snapshot();
            QVERIFY(snapshot.isComplete());
            for (std::uint32_t y = 0; y < spec.height; ++y) {
                const auto pixel = snapshot.pixel(0, y);
                QCOMPARE(pixel.red, valueFor(0, y, ColourComponent::Red));
                QCOMPARE(pixel.green, valueFor(0, y, ColourComponent::Green));
                QCOMPARE(pixel.blue, valueFor(0, y, ColourComponent::Blue));
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

    void countdownRecoversImmediatelyAfterCorruptFrameAB()
    {
        constexpr std::uint64_t firstStart = 100'000U;
        SstvTimingAccumulator timing(kSampleRate);
        static_cast<void>(timing.samplesFor(SstvAvtSyncCodec::FrameDuration));
        const std::uint64_t secondStart = firstStart + timing.totalSamples();
        for (const auto variant : {SstvAvtVariant::Qrm, SstvAvtVariant::NarrowQrm}) {
            const bool narrow = SstvAvtSyncCodec::isNarrow(variant);
            std::uint64_t sequence = 1U;
            auto first = countdownFrameObservations(SstvAvtMode::Avt24, 0U,
                                                     firstStart, sequence, 6U, kSampleRate, narrow);
            auto second = countdownFrameObservations(SstvAvtMode::Avt24, 1U,
                                                      secondStart, sequence, 6U, kSampleRate, narrow);
            first.pop_back();
            second.erase(second.begin());
            // Damage one complete data symbol, retaining a valid start and
            // its unmodified inverse so the first frame must be rejected.
            const auto symbolStart = firstStart + 118U;
            const auto symbolEnd = firstStart + 234U;
            for (auto& item : first) {
                if (item.centreSample >= symbolStart && item.centreSample < symbolEnd) {
                    const double zero = narrow ? 1700.0 : 1600.0;
                    const double one = narrow ? 2100.0 : 2200.0;
                    const double frequency = item.correctedFrequencyHz - kOffsetHz;
                    item.correctedFrequencyHz = (frequency == zero ? one : zero) + kOffsetHz;
                }
            }
            first.insert(first.end(), second.begin(), second.end());
            for (const std::size_t batch : {std::size_t {1U}, std::size_t {193U}}) {
                SstvAvtCountdownDetectorConfig config;
                config.expectedMode = SstvAvtMode::Avt24;
                config.variant = variant;
                config.searchStartSample = firstStart - 20U;
                config.observationSpanSamples = 6U;
                config.frequencyOffsetHz = kOffsetHz;
                SstvAvtCountdownDetector detector(config);
                std::optional<SstvAvtCountdownDetection> result;
                for (std::size_t i = 0; i < first.size() && !result; i += batch)
                    result = detector.consume(first.data() + i, std::min(batch, first.size() - i));
                QVERIFY2(result.has_value(), qPrintable(QStringLiteral("variant=%1 batch=%2 rejected=%3")
                    .arg(static_cast<int>(variant)).arg(batch).arg(detector.metrics().framesRejected)));
                QCOMPARE(result->counter, std::uint8_t {1U});
                QCOMPARE(result->variant, variant);
                QCOMPARE(result->frameStartSample, secondStart);
                SstvTimingAccumulator full(kSampleRate);
                static_cast<void>(full.samplesFor(SstvAvtSyncCodec::CountdownDuration));
                QCOMPARE(result->imageStartSample,
                         secondStart + full.totalSamples() - timing.totalSamples());
                QVERIFY(detector.metrics().framesRejected >= 1U);
            }
        }
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
        qInfo("AVT countdown detected image start offset: %lld samples",
              static_cast<long long>(detection->imageStartSample)
                  - static_cast<long long>(encoder.headerSamples()));
        const std::uint64_t error = detection->imageStartSample
                > encoder.headerSamples()
            ? detection->imageStartSample - encoder.headerSamples()
            : encoder.headerSamples() - detection->imageStartSample;
        QVERIFY2(error <= 2U,
                 qPrintable(QStringLiteral("AVT countdown anchor error: %1")
                                .arg(static_cast<qulonglong>(error))));
    }

    void fullAvt24PcmLoopbackIsChunkedAndCompletesProgressively_data()
    {
        QTest::addColumn<int>("variant");
        QTest::addColumn<int>("mode");
        QTest::addColumn<int>("pattern");
        const int avt24 = static_cast<int>(SstvAvtMode::Avt24);
        const int avt90 = static_cast<int>(SstvAvtMode::Avt90);
        QTest::newRow("normal") << static_cast<int>(SstvAvtVariant::Normal) << avt24 << 0;
        QTest::newRow("narrow") << static_cast<int>(SstvAvtVariant::Narrow) << avt24 << 0;
        QTest::newRow("qrm") << static_cast<int>(SstvAvtVariant::Qrm) << avt24 << 0;
        QTest::newRow("narrow-qrm") << static_cast<int>(SstvAvtVariant::NarrowQrm) << avt24 << 0;
        QTest::newRow("24-narrow-qrm-pattern-a") << static_cast<int>(SstvAvtVariant::NarrowQrm) << avt24 << 11;
        QTest::newRow("24-narrow-qrm-pattern-b") << static_cast<int>(SstvAvtVariant::NarrowQrm) << avt24 << 12;
        QTest::newRow("24-narrow-qrm-blocks-a") << static_cast<int>(SstvAvtVariant::NarrowQrm) << avt24 << 13;
        QTest::newRow("24-narrow-qrm-blocks-b") << static_cast<int>(SstvAvtVariant::NarrowQrm) << avt24 << 14;
        QTest::newRow("90-qrm-a") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 1;
        QTest::newRow("90-qrm-b") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 2;
        QTest::newRow("90-qrm-gradient-a") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 3;
        QTest::newRow("90-qrm-gradient-b") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 4;
        QTest::newRow("90-qrm-extreme-a") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 5;
        QTest::newRow("90-qrm-extreme-b") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 6;
        QTest::newRow("90-qrm-smooth-a") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 7;
        QTest::newRow("90-qrm-smooth-b") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 8;
        QTest::newRow("90-qrm-random-a") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 9;
        QTest::newRow("90-qrm-random-b") << static_cast<int>(SstvAvtVariant::Qrm) << avt90 << 10;
    }

    void fullAvt24PcmLoopbackIsChunkedAndCompletesProgressively()
    {
        QFETCH(int, variant);
        QFETCH(int, mode);
        QFETCH(int, pattern);
        const auto avtVariant = static_cast<SstvAvtVariant>(variant);
        const auto avtMode = static_cast<SstvAvtMode>(mode);
        const auto spec = SstvAvtProtocol::spec(avtMode);
        std::vector<SstvRgbPixel> expected = pixelsFor(avtMode);
        if (pattern == 1 || pattern == 2) {
            const SstvRgbPixel colour = pattern == 1
                ? SstvRgbPixel {80, 120, 255} : SstvRgbPixel {170, 200, 0};
            std::fill(expected.begin(), expected.end(), colour);
            expected.back().blue = pattern == 1 ? 0 : 255;
        }
        if (pattern == 4) {
            for (auto& pixel : expected) {
                pixel.red = 255U - pixel.red;
                pixel.green = 255U - pixel.green;
                pixel.blue = 255U - pixel.blue;
            }
        }
        if (pattern == 5 || pattern == 6) {
            const SstvRgbPixel colour = pattern == 5
                ? SstvRgbPixel {0, 255, 0} : SstvRgbPixel {255, 0, 255};
            std::fill(expected.begin(), expected.end(), colour);
        }
        if (pattern == 7 || pattern == 8) {
            for (std::uint32_t y = 0; y < spec.height; ++y) {
                for (std::uint32_t x = 0; x < spec.width; ++x) {
                    const auto r = static_cast<std::uint8_t>(x * 255U / (spec.width - 1U));
                    const auto g = static_cast<std::uint8_t>(y * 255U / (spec.height - 1U));
                    const auto b = static_cast<std::uint8_t>((static_cast<unsigned>(r) + g) / 2U);
                    expected[static_cast<std::size_t>(y) * spec.width + x] = pattern == 7
                        ? SstvRgbPixel {r, g, b}
                        : SstvRgbPixel {static_cast<std::uint8_t>(255U-r),
                                        static_cast<std::uint8_t>(255U-g),
                                        static_cast<std::uint8_t>(255U-b)};
                }
            }
        }
        if (pattern == 9 || pattern == 10 || pattern == 13 || pattern == 14) {
            std::uint32_t state = pattern == 9 || pattern == 13 ? 0x12345678U : 0x9abcdef0U;
            const auto next = [&state]() {
                state = 1664525U * state + 1013904223U;
                return static_cast<std::uint8_t>(state >> 24U);
            };
            for (std::uint32_t y = 0; y < spec.height; ++y) {
                for (std::uint32_t x = 0; x < spec.width; x += 8U) {
                    const SstvRgbPixel color {next(), next(), next()};
                    for (std::uint32_t offset = 0; offset < 8U && x + offset < spec.width; ++offset)
                        expected[static_cast<std::size_t>(y) * spec.width + x + offset] = color;
                }
            }
        }
        if (pattern == 11 || pattern == 12) {
            for (std::uint32_t y = 0; y < spec.height; ++y) {
                for (std::uint32_t x = 0; x < spec.width; ++x) {
                    const SstvRgbPixel color = pattern == 11
                        ? SstvRgbPixel {static_cast<std::uint8_t>((x * 2U + y / 3U) % 256U),
                                        static_cast<std::uint8_t>((y * 2U + x / 2U) % 256U),
                                        static_cast<std::uint8_t>(((x / 16U + y / 12U) % 2U) * 160U + (x + y) % 96U)}
                        : SstvRgbPixel {static_cast<std::uint8_t>((255U - 2U * x + y) % 256U),
                                        static_cast<std::uint8_t>(((x / 12U + y / 15U) % 2U) * 150U + y % 106U),
                                        static_cast<std::uint8_t>((255U - 2U * y + x) % 256U)};
                    expected[static_cast<std::size_t>(y) * spec.width + x] = color;
                }
            }
        }
        const QString referencePath = qEnvironmentVariable("DECODIUM_AVT_EXPORT_REFERENCE");
        if (!referencePath.isEmpty()) {
            QFile reference(referencePath);
            QVERIFY(reference.open(QIODevice::WriteOnly | QIODevice::Truncate));
            const QByteArray header = QByteArray("P6\n") + QByteArray::number(spec.width)
                + ' ' + QByteArray::number(spec.height) + "\n255\n";
            QCOMPARE(reference.write(header), qint64(header.size()));
            for (const auto& pixel : expected) {
                const char rgb[3] {static_cast<char>(pixel.red), static_cast<char>(pixel.green),
                                   static_cast<char>(pixel.blue)};
                QCOMPARE(reference.write(rgb, 3), qint64(3));
            }
        }
        QFile audioExport(qEnvironmentVariable("DECODIUM_AVT_EXPORT_F32"));
        const bool exportAudio = !audioExport.fileName().isEmpty();
        if (exportAudio) QVERIFY(audioExport.open(QIODevice::WriteOnly | QIODevice::Truncate));
        SstvAvtEncoder encoder(expected,
                               {avtMode,
                                kSampleRate,
                                0,
                                0.7,
                                0.95,
                                avtVariant});
        SstvFrequencyDemodulatorConfig demodConfig =
            SstvFrequencyDemodulatorConfig::sstvDefaults();
        demodConfig.averagingSamples = 3U;
        demodConfig.hopSamples = 1U;
        SstvFrequencyDemodulator demodulator(demodConfig);

        const bool loopPreprocess = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_PREPROCESS") == 1;
        const bool loopSplitEdge = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_SPLIT_EDGE") == 1;
        const int loopDropPeriod = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_DROP_PERIOD");
        QVERIFY(loopDropPeriod == 0 || (loopSplitEdge && loopDropPeriod >= 3 && loopDropPeriod <= 100));
        QVERIFY(!loopSplitEdge || (loopPreprocess && avtMode == SstvAvtMode::Avt24
                                   && avtVariant == SstvAvtVariant::NarrowQrm));
        const int loopAnchorShift = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_ANCHOR_SHIFT");
        QVERIFY(loopAnchorShift >= -8 && loopAnchorShift <= 8);
        const int loopLastPixelLead = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_LAST_PIXEL_LEAD");
        const int loopLastPixelTail = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_LAST_PIXEL_TAIL");
        const int loopFirstPixelLead = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_FIRST_PIXEL_LEAD");
        const int loopFirstPixelTail = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_FIRST_PIXEL_TAIL");
        QVERIFY(loopLastPixelLead >= 0 && loopLastPixelLead <= 4
                && loopLastPixelTail >= 0 && loopLastPixelTail <= 4
                && loopFirstPixelLead >= 0 && loopFirstPixelLead <= 4
                && loopFirstPixelTail >= 0 && loopFirstPixelTail <= 4);
        const bool loopDualEdge = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_DUAL_EDGE") == 1;
        const bool adaptiveEdge = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_ADAPTIVE_EDGE") == 1;
        const bool midpointEdge = qEnvironmentVariableIntValue("DECODIUM_AVT_LOOP_MIDPOINT_EDGE") == 1;
        const int midpointTarget = qEnvironmentVariable("DECODIUM_AVT_LOOP_MIDPOINT_TARGET", "6").toInt();
        const int loopEdgeDelay = qEnvironmentVariable("DECODIUM_AVT_LOOP_EDGE_DELAY", "4").toInt();
        const double loopEdgeBlend = qEnvironmentVariable("DECODIUM_AVT_LOOP_EDGE_BLEND", "0").toDouble();
        QVERIFY(!loopDualEdge || (loopPreprocess && avtMode == SstvAvtMode::Avt90));
        QVERIFY(loopEdgeDelay >= 0 && loopEdgeDelay <= 5);
        QVERIFY(std::isfinite(loopEdgeBlend) && loopEdgeBlend >= -1.0 && loopEdgeBlend <= 1.0);
        QVERIFY(loopEdgeBlend == 0.0 || loopEdgeDelay < 5);
        QVERIFY(!adaptiveEdge || (loopDualEdge && loopEdgeDelay == 3 && loopEdgeBlend == 0.0));
        QVERIFY(!midpointEdge || (loopDualEdge && !adaptiveEdge && loopEdgeDelay == 3 && loopEdgeBlend == 0.0));
        QVERIFY(midpointTarget >= 4 && midpointTarget <= 8);
        SstvPreprocessorConfig loopConfig;
        loopConfig.bandPassLowHz = qEnvironmentVariable("DECODIUM_AVT_LOOP_LOW_HZ", "900").toDouble();
        loopConfig.bandPassHighHz = qEnvironmentVariable("DECODIUM_AVT_LOOP_HIGH_HZ", "2500").toDouble();
        const double loopInterferenceHz = qEnvironmentVariable("DECODIUM_AVT_LOOP_INTERFERENCE_HZ", "0").toDouble();
        const double loopInterferencePeak = qEnvironmentVariable("DECODIUM_AVT_LOOP_INTERFERENCE_PEAK", "0").toDouble();
        QVERIFY(std::isfinite(loopConfig.bandPassLowHz) && std::isfinite(loopConfig.bandPassHighHz)
                && loopConfig.bandPassLowHz >= 100 && loopConfig.bandPassHighHz > loopConfig.bandPassLowHz
                && loopConfig.bandPassHighHz <= 5500);
        QVERIFY(std::isfinite(loopInterferenceHz) && loopInterferenceHz >= 0 && loopInterferenceHz <= 5000
                && std::isfinite(loopInterferencePeak) && loopInterferencePeak >= 0 && loopInterferencePeak <= 0.2);
        SstvPreprocessor loopPreprocessor(loopConfig);
        SstvPreprocessorConfig loopSecondaryConfig;
        loopSecondaryConfig.bandPassLowHz = 500.0;
        loopSecondaryConfig.bandPassHighHz = 3500.0;
        SstvPreprocessor loopSecondaryPreprocessor(loopSecondaryConfig);
        SstvFrequencyDemodulator loopSecondaryDemodulator(demodConfig);
        std::vector<SstvFrequencyObservation> loopPreviousSecondary;
        const auto adaptiveDelay = [adaptiveEdge](const auto& values, std::size_t index, int fallback) {
            if (!adaptiveEdge || index < 8U) return fallback;
            const auto& before = values[index - 8U];
            const auto& three = values[index - 3U];
            const auto& four = values[index - 4U];
            if (!before.valid() || !three.valid() || !four.valid()) return fallback;
            return std::abs(three.correctedFrequencyHz - before.correctedFrequencyHz)
                    <= std::abs(four.correctedFrequencyHz - before.correctedFrequencyHz) ? 3 : 4;
        };
        SstvTimingAccumulator timing(kSampleRate);
        static_cast<void>(timing.samplesFor(
            SstvAvtProtocol::TripleVisDuration));
        const std::uint64_t countdownStart = timing.totalSamples();
        SstvAvtCountdownDetectorConfig detectorConfig;
        detectorConfig.expectedMode = avtMode;
        detectorConfig.variant = avtVariant;
        detectorConfig.sampleRate = kSampleRate;
        detectorConfig.searchStartSample = countdownStart - 64U;
        detectorConfig.observationSpanSamples = static_cast<std::uint32_t>(
            demodConfig.averagingSamples);
        SstvAvtCountdownDetector detector(detectorConfig);
        std::unique_ptr<SstvAvtRxSession> session;
        std::unique_ptr<SstvAvtRxSession> splitControlSession;
        std::unique_ptr<SstvAvtRxSession> earlierSession;
        std::unique_ptr<SstvAvtRxSession> laterSession;
        std::unique_ptr<SstvAvtRxSession> boundarySession;
        std::unique_ptr<SstvAvtRxSession> edgeEarlySession;
        std::unique_ptr<SstvAvtMapper> boundaryMapper;
        std::uint64_t boundaryImageStart = 0U;
        std::uint64_t lastInteriorValid = 0U;
        std::uint64_t lastInteriorInvalid = 0U;
        std::uint64_t otherInteriorValid = 0U;
        std::uint64_t otherInteriorInvalid = 0U;
        std::array<double, 4U> transitionFractionSum {};
        std::array<std::uint64_t, 4U> transitionFractionCount {};
        std::vector<SstvFrequencyObservation> diagnosticObservations;

        constexpr std::array<std::size_t, 6U> chunks {{
            1U, 73U, 2'048U, 8'192U, 509U, 4'093U}};
        std::vector<std::int16_t> pcm(8'192U);
        std::vector<float> floating(8'192U);
        std::size_t chunkIndex = 0U;
        std::uint64_t loopSampleIndex = 0U;
        while (!encoder.complete()) {
            const std::size_t request = chunks[
                chunkIndex++ % chunks.size()];
            const std::size_t produced = encoder.pullPcm16(pcm.data(),
                                                           request);
            QVERIFY(produced > 0U);
            for (std::size_t index = 0U; index < produced; ++index) {
                floating[index] = static_cast<float>(pcm[index])
                    / 32'768.0F + static_cast<float>(loopInterferencePeak * std::sin(
                        2.0 * 3.141592653589793 * loopInterferenceHz * loopSampleIndex / kSampleRate));
                ++loopSampleIndex;
            }
            if (exportAudio) {
                const auto bytes = static_cast<qint64>(produced * sizeof(float));
                QCOMPARE(audioExport.write(reinterpret_cast<const char*>(floating.data()), bytes), bytes);
            }
            const std::vector<float> loopInput(floating.begin(), floating.begin() + produced);
            const auto prepared = loopPreprocess ? loopPreprocessor.process(loopInput) : loopInput;
            const std::vector<SstvFrequencyObservation> observations = demodulator.consume(prepared);
            const auto loopSecondary = loopDualEdge
                ? loopSecondaryDemodulator.consume(loopSecondaryPreprocessor.process(loopInput))
                : std::vector<SstvFrequencyObservation> {};
            std::vector<SstvFrequencyObservation> loopDelayed = loopPreviousSecondary;
            if (loopDualEdge) {
                QCOMPARE(loopSecondary.size(), observations.size());
                loopDelayed.insert(loopDelayed.end(), loopSecondary.begin(), loopSecondary.end());
            }
            QVERIFY(observations.size()
                    <= SstvAvtCountdownDetector::MaximumObservationsPerConsume);
            if (!session) {
                const auto detection = detector.consume(observations);
                if (detection.has_value()) {
                    qInfo("AVT variant %d detected image start offset: %lld samples",
                          variant, static_cast<long long>(detection->imageStartSample)
                              - static_cast<long long>(encoder.headerSamples()));
                    SstvAvtRxSessionConfig sessionConfig;
                    sessionConfig.mode = detection->mode;
                    sessionConfig.variant = detection->variant;
                    sessionConfig.sampleRate = kSampleRate;
                    QVERIFY(static_cast<std::int64_t>(detection->imageStartSample) + loopAnchorShift > 0);
                    sessionConfig.imageStartSample = static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(detection->imageStartSample) + loopAnchorShift);
                    sessionConfig.observationSpanSamples =
                        static_cast<std::uint32_t>(demodConfig.hopSamples);
                    sessionConfig.experimentalAvt24LastPixelTiming = loopSplitEdge;
                    session = std::make_unique<SstvAvtRxSession>(
                        sessionConfig);
                    sessionConfig.experimentalAvt24LastPixelTiming = false;
                    if (loopSplitEdge)
                        splitControlSession = std::make_unique<SstvAvtRxSession>(sessionConfig);
                    boundaryImageStart = sessionConfig.imageStartSample;
                    boundarySession = std::make_unique<SstvAvtRxSession>(sessionConfig);
                    boundaryMapper = std::make_unique<SstvAvtMapper>(
                        SstvAvtMapperConfig {avtMode, kSampleRate, 0, avtVariant});
                    QVERIFY(sessionConfig.imageStartSample > 0U);
                    --sessionConfig.imageStartSample;
                    earlierSession = std::make_unique<SstvAvtRxSession>(sessionConfig);
                    sessionConfig.imageStartSample += 2U;
                    laterSession = std::make_unique<SstvAvtRxSession>(sessionConfig);
                    if (avtMode == SstvAvtMode::Avt90) {
                        sessionConfig.imageStartSample = boundaryImageStart - 2U;
                        edgeEarlySession = std::make_unique<SstvAvtRxSession>(sessionConfig);
                    }
                }
            }
            if (session) {
                if (avtMode == SstvAvtMode::Avt90)
                    diagnosticObservations.insert(diagnosticObservations.end(), observations.begin(), observations.end());
                auto imageObservations = observations;
                if (loopDualEdge) {
                    for (std::size_t i = 0; i < observations.size(); ++i) {
                        const auto centre = observations[i].centreSample;
                        if (centre < boundaryImageStart) continue;
                        const auto relative = centre - boundaryImageStart;
                        if (relative >= boundaryMapper->imageSampleCount() || relative % 1500U < 1490U) continue;
                        const auto pixel = boundaryMapper->positionAtSample(relative);
                        const auto delayedIndex = loopPreviousSecondary.size() + i;
                        if (pixel.pixel == 319U && delayedIndex >= static_cast<std::size_t>(midpointEdge ? 5 : loopEdgeDelay + (loopEdgeBlend != 0.0))) {
                            const int chosenDelay = midpointEdge
                                ? std::clamp(midpointTarget - int(pixel.segmentEndSample - relative), 0, 5)
                                : adaptiveDelay(loopDelayed, delayedIndex, loopEdgeDelay);
                            imageObservations[i] = loopDelayed[delayedIndex - chosenDelay];
                            if (loopEdgeBlend != 0.0) {
                                const auto& previous = loopDelayed[delayedIndex - loopEdgeDelay - 1U];
                                if (imageObservations[i].valid() && previous.valid())
                                    imageObservations[i].correctedFrequencyHz =
                                        (1.0 - loopEdgeBlend) * imageObservations[i].correctedFrequencyHz
                                        + loopEdgeBlend * previous.correctedFrequencyHz;
                            }
                            imageObservations[i].centreSample = centre;
                            imageObservations[i].sequence = observations[i].sequence;
                        }
                    }
                }
                if (loopLastPixelLead || loopLastPixelTail || loopFirstPixelLead || loopFirstPixelTail) {
                    imageObservations.erase(std::remove_if(imageObservations.begin(), imageObservations.end(),
                        [&](const SstvFrequencyObservation& item) {
                            if (item.centreSample < boundaryImageStart) return false;
                            const auto sample = item.centreSample - boundaryImageStart;
                            if (sample >= boundaryMapper->imageSampleCount()) return false;
                            const auto position = boundaryMapper->positionAtSample(sample);
                            if (!position.valid()) return false;
                            const bool first = position.pixel == 0U;
                            if (!first && position.pixel + 1U != spec.width) return false;
                            const int lead = first ? loopFirstPixelLead : loopLastPixelLead;
                            const int tail = first ? loopFirstPixelTail : loopLastPixelTail;
                            return sample - position.segmentStartSample < static_cast<std::uint64_t>(lead)
                                || position.segmentEndSample - sample <= static_cast<std::uint64_t>(tail);
                        }), imageObservations.end());
                }
                std::vector<SstvFrequencyObservation> controlObservations;
                if (splitControlSession) controlObservations = observations;
                if (loopDropPeriod) {
                    const auto drop = [loopDropPeriod](const SstvFrequencyObservation& item) {
                        return item.centreSample % static_cast<std::uint64_t>(loopDropPeriod) == 0U;
                    };
                    imageObservations.erase(std::remove_if(imageObservations.begin(), imageObservations.end(), drop),
                                            imageObservations.end());
                    controlObservations.erase(std::remove_if(controlObservations.begin(), controlObservations.end(), drop),
                                              controlObservations.end());
                }
                static_cast<void>(session->consume(imageObservations));
                if (splitControlSession) static_cast<void>(splitControlSession->consume(controlObservations));
                static_cast<void>(earlierSession->consume(observations));
                static_cast<void>(laterSession->consume(observations));
                if (edgeEarlySession) static_cast<void>(edgeEarlySession->consume(observations));
                std::vector<SstvFrequencyObservation> filtered;
                filtered.reserve(observations.size());
                for (const auto& item : observations) {
                    if (item.centreSample >= boundaryImageStart) {
                        const auto sample = item.centreSample - boundaryImageStart;
                        if (sample < boundaryMapper->imageSampleCount()) {
                            const auto pos = boundaryMapper->positionAtSample(sample);
                            const auto length = pos.segmentEndSample - pos.segmentStartSample;
                            const bool interior = length >= 4U
                                && sample > pos.segmentStartSample
                                && sample + 1U < pos.segmentEndSample;
                            if (interior) {
                                auto& count = pos.pixel + 1U == spec.width
                                    ? (item.valid() ? lastInteriorValid : lastInteriorInvalid)
                                    : (item.valid() ? otherInteriorValid : otherInteriorInvalid);
                                ++count;
                                if ((pattern == 5 || pattern == 6)
                                    && pos.pixel + 1U == spec.width && item.valid()
                                    && (pos.component == ColourComponent::Red
                                        || pos.component == ColourComponent::Green)) {
                                    const auto distance = pos.segmentEndSample - sample;
                                    if (distance < transitionFractionSum.size()) {
                                        const bool rising = (pos.component == ColourComponent::Red)
                                            == (pattern == 5);
                                        const double fraction = rising
                                            ? (item.correctedFrequencyHz - 1500.0) / 800.0
                                            : (2300.0 - item.correctedFrequencyHz) / 800.0;
                                        transitionFractionSum[distance] += fraction;
                                        ++transitionFractionCount[distance];
                                    }
                                }
                            }
                            if (pos.pixel + 1U == spec.width && length >= 4U
                                && sample >= pos.segmentStartSample + (length + 1U) / 2U) {
                                continue;
                            }
                        }
                    }
                    filtered.push_back(item);
                }
                static_cast<void>(boundarySession->consume(filtered));
            }
            if (loopDualEdge) {
                const auto keep = std::min<std::size_t>(9U, loopDelayed.size());
                loopPreviousSecondary.assign(loopDelayed.end() - keep, loopDelayed.end());
            }
        }
        QVERIFY(session != nullptr);
        QVERIFY(session->decoderMetrics().linesPublished > 0U);

        // Flush only the frontend's finite analytic/averaging delay. These
        // samples are not part of the encoded AVT payload.
        std::vector<float> silence(256U, 0.0F);
        const std::vector<SstvFrequencyObservation> trailing =
            demodulator.consume(loopPreprocess ? loopPreprocessor.process(silence) : silence);
        QVERIFY(trailing.size()
                <= SstvAvtCountdownDetector::MaximumObservationsPerConsume);
        auto imageTrailing = trailing;
        if (loopDualEdge) {
            const auto secondaryTrailing = loopSecondaryDemodulator.consume(loopSecondaryPreprocessor.process(silence));
            QCOMPARE(secondaryTrailing.size(), trailing.size());
            auto delayed = loopPreviousSecondary;
            delayed.insert(delayed.end(), secondaryTrailing.begin(), secondaryTrailing.end());
            for (std::size_t i = 0; i < trailing.size(); ++i) {
                const auto centre = trailing[i].centreSample;
                if (centre < boundaryImageStart) continue;
                const auto relative = centre - boundaryImageStart;
                if (relative >= boundaryMapper->imageSampleCount() || relative % 1500U < 1490U) continue;
                const auto delayedIndex = loopPreviousSecondary.size() + i;
                const auto pixel = boundaryMapper->positionAtSample(relative);
                if (pixel.pixel == 319U
                    && delayedIndex >= static_cast<std::size_t>(midpointEdge ? 5 : loopEdgeDelay + (loopEdgeBlend != 0.0))) {
                    const int chosenDelay = midpointEdge
                        ? std::clamp(midpointTarget - int(pixel.segmentEndSample - relative), 0, 5)
                        : adaptiveDelay(delayed, delayedIndex, loopEdgeDelay);
                    imageTrailing[i] = delayed[delayedIndex - chosenDelay];
                    if (loopEdgeBlend != 0.0) {
                        const auto& previous = delayed[delayedIndex - loopEdgeDelay - 1U];
                        if (imageTrailing[i].valid() && previous.valid())
                            imageTrailing[i].correctedFrequencyHz =
                                (1.0 - loopEdgeBlend) * imageTrailing[i].correctedFrequencyHz
                                + loopEdgeBlend * previous.correctedFrequencyHz;
                    }
                    imageTrailing[i].centreSample = centre;
                    imageTrailing[i].sequence = trailing[i].sequence;
                }
            }
        }
        static_cast<void>(session->consume(imageTrailing));
        if (splitControlSession) static_cast<void>(splitControlSession->consume(trailing));
        static_cast<void>(earlierSession->consume(trailing));
        static_cast<void>(laterSession->consume(trailing));
        static_cast<void>(boundarySession->consume(trailing));
        if (edgeEarlySession) static_cast<void>(edgeEarlySession->consume(trailing));
        if (avtMode == SstvAvtMode::Avt90)
            diagnosticObservations.insert(diagnosticObservations.end(), trailing.begin(), trailing.end());
        const auto tracePath = qEnvironmentVariable("DECODIUM_AVT_BOUNDARY_TRACE");
        if (!tracePath.isEmpty()) {
            QVERIFY(avtMode == SstvAvtMode::Avt90 && loopPreprocess);
            QFile trace(tracePath);
            QVERIFY(trace.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
            QTextStream out(&trace);
            out << "row,component,target";
            for (int d = -8; d <= 4; ++d) out << ",f" << d;
            out << '\n';
            const auto sampleFor = [&](std::uint64_t wanted) -> const SstvFrequencyObservation* {
                const auto it = std::lower_bound(diagnosticObservations.begin(), diagnosticObservations.end(), wanted,
                    [](const auto& item, std::uint64_t value) { return item.centreSample < value; });
                return it != diagnosticObservations.end() && it->centreSample == wanted && it->valid() ? &*it : nullptr;
            };
            for (std::uint64_t end = 1500U; end + 4U < boundaryMapper->imageSampleCount(); end += 1500U) {
                const auto pixel = boundaryMapper->positionAtSample(end - 1U);
                const auto source = expected[std::size_t(pixel.line) * spec.width + pixel.pixel];
                const int channel = pixel.component == ColourComponent::Red ? 0
                    : pixel.component == ColourComponent::Green ? 1 : 2;
                const int value = channel == 0 ? source.red : channel == 1 ? source.green : source.blue;
                std::array<const SstvFrequencyObservation*, 13U> samples {};
                bool valid = true;
                for (int d = -8; d <= 4; ++d) {
                    samples[static_cast<std::size_t>(d + 8)] = sampleFor(boundaryImageStart + end + d);
                    valid = valid && samples[static_cast<std::size_t>(d + 8)] != nullptr;
                }
                if (!valid) continue;
                out << pixel.line << ',' << channel << ',' << value;
                for (const auto* sample : samples) out << ',' << sample->correctedFrequencyHz;
                out << '\n';
            }
            out.flush();
            QVERIFY(out.status() == QTextStream::Ok);
        }
        QCOMPARE(session->finish(), SstvAvtRxSessionState::Complete);
        if (splitControlSession) QCOMPARE(splitControlSession->finish(), SstvAvtRxSessionState::Complete);
        QCOMPARE(earlierSession->finish(), SstvAvtRxSessionState::Complete);
        QCOMPARE(laterSession->finish(), SstvAvtRxSessionState::Complete);
        QCOMPARE(boundarySession->finish(), SstvAvtRxSessionState::Complete);
        if (edgeEarlySession) QCOMPARE(edgeEarlySession->finish(), SstvAvtRxSessionState::Complete);
        QCOMPARE(session->decoderMetrics().linesPublished,
                 static_cast<std::uint64_t>(spec.height));

        const SstvImageSnapshot actual = session->snapshot();
        const SstvImageSnapshot splitControl = splitControlSession ? splitControlSession->snapshot() : SstvImageSnapshot {};
        const SstvImageSnapshot earlier = earlierSession->snapshot();
        const SstvImageSnapshot later = laterSession->snapshot();
        const SstvImageSnapshot boundary = boundarySession->snapshot();
        SstvImageSnapshot edgeEarlySnapshot;
        if (edgeEarlySession) edgeEarlySnapshot = edgeEarlySession->snapshot();
        SstvImageSnapshot correctedSnapshot;
        if (avtMode == SstvAvtMode::Avt90) {
            double nextToneError = 0.0;
            double uncorrectedToneError = 0.0;
            double correctedToneError = 0.0;
            std::uint64_t diagnosticCount = 0U;
            // Offline diagnostic only: estimate the next tone from a future
            // observation and undo an empirically observed 11% mixture.
            // This is not a production algorithm or a calibrated channel model.
            for (auto& item : diagnosticObservations) {
                if (!item.valid() || item.centreSample < boundaryImageStart) continue;
                const auto sample = item.centreSample - boundaryImageStart;
                if (sample >= boundaryMapper->imageSampleCount()) continue;
                const auto pos = boundaryMapper->positionAtSample(sample);
                if (pos.pixel + 1U != spec.width || pos.segmentEndSample - sample != 2U
                    || pos.segmentEndSample + 2U >= boundaryMapper->imageSampleCount()) continue;
                const auto wanted = boundaryImageStart + pos.segmentEndSample + 2U;
                const auto next = std::lower_bound(diagnosticObservations.begin(), diagnosticObservations.end(), wanted,
                    [](const auto& observation, std::uint64_t target) { return observation.centreSample < target; });
                if (next == diagnosticObservations.end() || next->centreSample != wanted || !next->valid()) continue;
                const auto sourceFrequency = [&](const SstvAvtPosition& position) {
                    const auto pixel = expected[static_cast<std::size_t>(position.line) * spec.width + position.pixel];
                    const auto value = position.component == ColourComponent::Red ? pixel.red
                        : position.component == ColourComponent::Green ? pixel.green : pixel.blue;
                    return SstvAvtProtocol::frequencyForValue(value, avtVariant);
                };
                const auto futurePosition = boundaryMapper->positionAtSample(pos.segmentEndSample + 2U);
                const double currentExpected = sourceFrequency(pos);
                const double futureExpected = sourceFrequency(futurePosition);
                const double candidate = (item.correctedFrequencyHz - 0.11 * next->correctedFrequencyHz) / 0.89;
                nextToneError += std::abs(next->correctedFrequencyHz - futureExpected);
                uncorrectedToneError += std::abs(item.correctedFrequencyHz - currentExpected);
                correctedToneError += std::abs(candidate - currentExpected);
                ++diagnosticCount;
                item.correctedFrequencyHz = candidate;
            }
            QVERIFY(diagnosticCount > 0U);
            qInfo("AVT de-mix observation mean abs Hz next/before/after: %.2f/%.2f/%.2f over %llu",
                  nextToneError / diagnosticCount, uncorrectedToneError / diagnosticCount,
                  correctedToneError / diagnosticCount, static_cast<unsigned long long>(diagnosticCount));
            SstvAvtRxSessionConfig candidateConfig;
            candidateConfig.mode = avtMode;
            candidateConfig.variant = avtVariant;
            candidateConfig.sampleRate = kSampleRate;
            candidateConfig.imageStartSample = boundaryImageStart;
            candidateConfig.observationSpanSamples = 1U;
            SstvAvtRxSession candidate(candidateConfig);
            for (std::size_t offset = 0; offset < diagnosticObservations.size(); offset += 8192U) {
                const auto count = std::min<std::size_t>(8192U, diagnosticObservations.size() - offset);
                static_cast<void>(candidate.consume(diagnosticObservations.data() + offset, count));
            }
            QCOMPARE(candidate.finish(), SstvAvtRxSessionState::Complete);
            correctedSnapshot = candidate.snapshot();
        }
        QVERIFY(actual.isComplete());
        QCOMPARE(actual.width, spec.width);
        QCOMPARE(actual.height, spec.height);
        std::uint64_t absoluteError = 0U;
        std::uint64_t earlierError = 0U;
        std::uint64_t boundaryError = 0U;
        std::uint64_t splitControlBoundaryError = 0U;
        std::uint64_t firstColumnError = 0U;
        int boundaryMax = 0;
        int firstColumnMax = 0;
        std::uint32_t boundaryMaxRow = 0;
        int boundaryMaxChannel = 0;
        std::uint64_t earlierBoundaryError = 0U;
        std::uint64_t laterError = 0U;
        std::uint64_t laterBoundaryError = 0U;
        std::uint64_t boundaryFilteredError = 0U;
        std::uint64_t boundaryFilteredColumnError = 0U;
        std::uint64_t correctedError = 0U;
        std::uint64_t correctedColumnError = 0U;
        std::uint64_t earlyColumnError = 0U;
        std::uint64_t estimatedNeighbourBefore = 0U, estimatedNeighbourAfter = 0U;
        int estimatedNeighbourMax = 0;
        int earlyColumnMax = 0;
        for (std::uint32_t y = 0U; y < actual.height; ++y) {
            for (std::uint32_t x = 0U; x < actual.width; ++x) {
                const std::size_t index = static_cast<std::size_t>(y)
                    * actual.width + x;
                const SstvRgbPixel decoded = actual.pixel(x, y);
                const SstvRgbPixel shifted = earlier.pixel(x, y);
                const SstvRgbPixel delayed = later.pixel(x, y);
                const SstvRgbPixel filteredPixel = boundary.pixel(x, y);
                const auto componentError = [](const SstvRgbPixel& a, const SstvRgbPixel& b) {
                    return std::abs(int(a.red) - int(b.red))
                        + std::abs(int(a.green) - int(b.green))
                        + std::abs(int(a.blue) - int(b.blue));
                };
                if (splitControlSession) {
                    const auto control = splitControl.pixel(x, y);
                    if (x + 1U != actual.width) {
                        QCOMPARE(decoded.red, control.red);
                        QCOMPARE(decoded.green, control.green);
                        QCOMPARE(decoded.blue, control.blue);
                    } else {
                        splitControlBoundaryError += componentError(control, expected[index]);
                    }
                }
                earlierError += componentError(shifted, expected[index]);
                laterError += componentError(delayed, expected[index]);
                boundaryFilteredError += componentError(filteredPixel, expected[index]);
                if (avtMode == SstvAvtMode::Avt90) {
                    const auto error = componentError(correctedSnapshot.pixel(x, y), expected[index]);
                    correctedError += error;
                    if (x + 1U == actual.width) correctedColumnError += error;
                }
                if (x + 1U == actual.width) {
                    if (avtMode == SstvAvtMode::Avt90) {
                        const auto first = actual.pixel(0U, y);
                        const auto nextLine = y == 238U ? 1U : y + 2U;
                        const auto nextFirst = y == 239U ? first : actual.pixel(0U, nextLine);
                        const std::array<int, 3U> current {{decoded.red, decoded.green, decoded.blue}};
                        const std::array<int, 3U> next {{first.green, first.blue, nextFirst.red}};
                        const auto source = expected[index];
                        const std::array<int, 3U> wanted {{source.red, source.green, source.blue}};
                        for (std::size_t channel = 0; channel < 3U; ++channel) {
                            if (channel == 2U && y == 239U) continue;
                            const int corrected = std::clamp(static_cast<int>(std::lround(
                                (current[channel] - 0.33 * next[channel]) / 0.67)), 0, 255);
                            estimatedNeighbourBefore += std::abs(current[channel] - wanted[channel]);
                            const int error = std::abs(corrected - wanted[channel]);
                            estimatedNeighbourAfter += error;
                            estimatedNeighbourMax = std::max(estimatedNeighbourMax, error);
                        }
                    }
                    boundaryError += componentError(decoded, expected[index]);
                    const auto source = expected[index];
                    const std::array<int, 3U> errors {{std::abs(int(decoded.red) - int(source.red)),
                        std::abs(int(decoded.green) - int(source.green)),
                        std::abs(int(decoded.blue) - int(source.blue))}};
                    for (int channel = 0; channel < 3; ++channel) {
                        if (errors[channel] > boundaryMax) {
                            boundaryMax = errors[channel];
                            boundaryMaxRow = y;
                            boundaryMaxChannel = channel;
                        }
                    }
                    earlierBoundaryError += componentError(shifted, expected[index]);
                    laterBoundaryError += componentError(delayed, expected[index]);
                    boundaryFilteredColumnError += componentError(filteredPixel, expected[index]);
                    if (edgeEarlySession) {
                        const auto early = edgeEarlySnapshot.pixel(x, y);
                        earlyColumnError += componentError(early, expected[index]);
                        earlyColumnMax = std::max({earlyColumnMax,
                            std::abs(int(early.red) - int(expected[index].red)),
                            std::abs(int(early.green) - int(expected[index].green)),
                            std::abs(int(early.blue) - int(expected[index].blue))});
                    }
                }
                if (x == 0U) {
                    firstColumnError += componentError(decoded, expected[index]);
                    const auto source = expected[index];
                    firstColumnMax = std::max({firstColumnMax,
                        std::abs(int(decoded.red) - int(source.red)),
                        std::abs(int(decoded.green) - int(source.green)),
                        std::abs(int(decoded.blue) - int(source.blue))});
                }
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
        if (!loopPreprocess && (pattern == 7 || pattern == 8)) {
            QVERIFY2(meanAbsoluteError < 0.05,
                     qPrintable(QStringLiteral("AVT90 smooth loopback MAE: %1")
                                    .arg(meanAbsoluteError, 0, 'f', 5)));
        }
        if (!loopPreprocess && (pattern == 5 || pattern == 6)) {
            // The extreme A/B images are constant per component, so their
            // clean loopback should be much more accurate than a gradient.
            // The remaining boundary error is included in this whole-image
            // metric; it is not evidence of pixel-perfect decoding.
            QVERIFY2(meanAbsoluteError < 0.05,
                     qPrintable(QStringLiteral("AVT90 extreme loopback MAE: %1")
                                    .arg(meanAbsoluteError, 0, 'f', 5)));
        }
        qInfo("AVT variant %d normal/earlier total error: %llu/%llu; last-column: %llu/%llu",
              variant, static_cast<unsigned long long>(absoluteError),
              static_cast<unsigned long long>(earlierError),
              static_cast<unsigned long long>(boundaryError),
              static_cast<unsigned long long>(earlierBoundaryError));
        qInfo("AVT diagnostic last-column max component %d row %u channel %d",
              boundaryMax, boundaryMaxRow, boundaryMaxChannel);
        if (pattern >= 11 && pattern <= 14) {
            qInfo("AVT24 Narrow-QRM first/last-column maxima %d/%d", firstColumnMax, boundaryMax);
            qInfo("AVT24 Narrow-QRM first/last-column MAE %.3f/%.3f",
                  double(firstColumnError) / (actual.height * 3.0),
                  double(boundaryError) / (actual.height * 3.0));
            if (loopSplitEdge && loopInterferencePeak == 0.0) {
                QVERIFY(splitControlBoundaryError > 0U);
                QVERIFY2(boundaryError * 100U < splitControlBoundaryError * 60U,
                         "experimental edge timing did not reduce clean last-column error");
            }
            if (loopSplitEdge && loopInterferencePeak == 0.1
                && (loopInterferenceHz == 430.0 || loopInterferenceHz == 2900.0)) {
                QVERIFY(splitControlBoundaryError > 0U);
                QVERIFY2(boundaryError * 100U < splitControlBoundaryError * 98U,
                         "experimental edge timing did not reduce disturbed last-column error");
            }
            if ((pattern == 11 || pattern == 12) && !loopPreprocess && loopAnchorShift == 0 && loopInterferencePeak == 0.0
                && loopLastPixelLead == 0 && loopLastPixelTail == 0
                && loopFirstPixelLead == 0 && loopFirstPixelTail == 0) {
                QVERIFY2(firstColumnMax <= 20 && boundaryMax <= 20,
                         qPrintable(QStringLiteral("AVT24 unfiltered edge errors %1/%2")
                                        .arg(firstColumnMax).arg(boundaryMax)));
            }
        }
        if (!loopPreprocess && (pattern == 3 || pattern == 4 || pattern == 7 || pattern == 8)) {
            QVERIFY2(boundaryMax <= 20,
                     qPrintable(QStringLiteral("AVT90 unfiltered last-column error %1 at row %2 channel %3")
                                    .arg(boundaryMax).arg(boundaryMaxRow).arg(boundaryMaxChannel)));
        }
        if (!loopPreprocess && (pattern == 1 || pattern == 2 || pattern == 9 || pattern == 10)) {
            // The terminal sample can have higher error when the final
            // pixel contrasts with its neighbours, including random blocks.
            QVERIFY2(boundaryMax <= 40,
                     qPrintable(QStringLiteral("AVT90 terminal-pixel error %1 at row %2 channel %3")
                                    .arg(boundaryMax).arg(boundaryMaxRow).arg(boundaryMaxChannel)));
        }
        qInfo("AVT variant %d later total error: %llu; last-column: %llu",
              variant, static_cast<unsigned long long>(laterError),
              static_cast<unsigned long long>(laterBoundaryError));
        qInfo("AVT variant %d filtered total error: %llu; last-column: %llu",
              variant, static_cast<unsigned long long>(boundaryFilteredError),
              static_cast<unsigned long long>(boundaryFilteredColumnError));
        if (avtMode == SstvAvtMode::Avt90)
            qInfo("AVT diagnostic de-mix total error: %llu; last-column: %llu",
                  static_cast<unsigned long long>(correctedError),
                  static_cast<unsigned long long>(correctedColumnError));
        if (avtMode == SstvAvtMode::Avt90)
            qInfo("AVT estimated-neighbour correction original/corrected last-column error %llu/%llu; max %d",
                  static_cast<unsigned long long>(estimatedNeighbourBefore),
                  static_cast<unsigned long long>(estimatedNeighbourAfter), estimatedNeighbourMax);
        if (edgeEarlySession)
            qInfo("AVT diagnostic early-only last column: %llu; max component: %d",
                  static_cast<unsigned long long>(earlyColumnError), earlyColumnMax);
        qInfo("AVT interior observations last pixel valid/invalid: %llu/%llu; other pixels: %llu/%llu",
              static_cast<unsigned long long>(lastInteriorValid),
              static_cast<unsigned long long>(lastInteriorInvalid),
              static_cast<unsigned long long>(otherInteriorValid),
              static_cast<unsigned long long>(otherInteriorInvalid));
        QVERIFY(lastInteriorValid > 0U);
        QVERIFY(otherInteriorValid > 0U);
        if (pattern == 5 || pattern == 6) {
            for (std::size_t distance = 2U; distance < transitionFractionSum.size(); ++distance) {
                QVERIFY(transitionFractionCount[distance] > 0U);
                qInfo("AVT extreme distance-to-end %zu mean next-tone fraction %.5f (%llu observations)",
                      distance, transitionFractionSum[distance] / transitionFractionCount[distance],
                      static_cast<unsigned long long>(transitionFractionCount[distance]));
            }
        }
        QVERIFY2(meanAbsoluteError < 35.0,
                 qPrintable(QStringLiteral("AVT24 loopback MAE: %1")
                                .arg(meanAbsoluteError, 0, 'f', 3)));
    }

    void offlineCapturedAvt90TimingDiagnostic()
    {
        const QString path = qEnvironmentVariable("DECODIUM_AVT_CAPTURE");
        if (path.isEmpty()) QSKIP("Optional captured-audio diagnostic");
        const QString variant = qEnvironmentVariable("DECODIUM_AVT_CAPTURE_VARIANT");
        QVERIFY(variant == QStringLiteral("a") || variant == QStringLiteral("b"));
        const double interferenceHz = qEnvironmentVariable("DECODIUM_AVT_INTERFERENCE_HZ", "0").toDouble();
        const double interferencePeak = qEnvironmentVariable("DECODIUM_AVT_INTERFERENCE_PEAK", "0").toDouble();
        const double notchHz = qEnvironmentVariable("DECODIUM_AVT_NOTCH_HZ", QString::number(interferenceHz)).toDouble();
        const double notchQ = qEnvironmentVariable("DECODIUM_AVT_NOTCH_Q", "20").toDouble();
        const double noisePeak = qEnvironmentVariable("DECODIUM_AVT_NOISE_PEAK", "0").toDouble();
        QVERIFY(std::isfinite(interferenceHz) && interferenceHz >= 0.0 && interferenceHz <= 5000.0);
        QVERIFY(std::isfinite(interferencePeak) && interferencePeak >= 0.0 && interferencePeak <= 0.2);
        QVERIFY(std::isfinite(noisePeak) && noisePeak >= 0.0 && noisePeak <= 0.2);
        std::uint32_t noiseState = variant == "a" ? 0x12345678U : 0x9abcdef0U;
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        QVERIFY(!data.isEmpty() && data.size() % 8 == 0);
        SstvResampler resampler(48'000U);
        const QString preprocessing = qEnvironmentVariable("DECODIUM_AVT_PREPROCESS_CASE", "off");
        QVERIFY(preprocessing == "off" || preprocessing == "default" || preprocessing == "bandpass-off"
                || preprocessing == "dc-off" || preprocessing == "level-off"
                || preprocessing == "bandpass-wide" || preprocessing == "bandpass-wider"
                || preprocessing == "bandpass-custom"
                || preprocessing == "bandpass-highwide" || preprocessing == "bandpass-lowwide"
                || preprocessing == "bandpass-high5000" || preprocessing == "bandpass-high5500"
                || preprocessing == "fir9" || preprocessing == "fir17" || preprocessing == "fir33"
                || preprocessing == "allpass4" || preprocessing == "allpass6" || preprocessing == "allpass8"
                || preprocessing == "outside-notch" || preprocessing == "equalizer");
        SstvPreprocessorConfig preprocessorConfig;
        if (preprocessing == "bandpass-custom") {
            const double lowHz = qEnvironmentVariable("DECODIUM_AVT_BANDPASS_LOW", "900").toDouble();
            const double highHz = qEnvironmentVariable("DECODIUM_AVT_BANDPASS_HIGH", "2500").toDouble();
            QVERIFY(std::isfinite(lowHz) && std::isfinite(highHz)
                    && lowHz >= 100.0 && highHz > lowHz && highHz <= 5500.0);
            preprocessorConfig.bandPassLowHz = lowHz;
            preprocessorConfig.bandPassHighHz = highHz;
        }
        const QStringList equalizerValues = qEnvironmentVariable("DECODIUM_AVT_EQUALIZER").split(',', Qt::SkipEmptyParts);
        const int firTaps = preprocessing == "equalizer" ? equalizerValues.size()
            : preprocessing == "fir9" ? 9 : preprocessing == "fir17" ? 17
            : preprocessing == "fir33" ? 33 : 0;
        if (preprocessing == "equalizer") QVERIFY(firTaps >= 1 && firTaps <= 33);
        if (firTaps && preprocessing != "equalizer") preprocessorConfig.bandPassEnabled = false;
        if (preprocessing == "outside-notch") preprocessorConfig.bandPassEnabled = false;
        if (preprocessing == "bandpass-off") preprocessorConfig.bandPassEnabled = false;
        if (preprocessing == "dc-off") preprocessorConfig.dcBlockerEnabled = false;
        if (preprocessing == "level-off") preprocessorConfig.levelControlEnabled = false;
        if (preprocessing == "bandpass-wide") {
            preprocessorConfig.bandPassLowHz = 500.0;
            preprocessorConfig.bandPassHighHz = 3500.0;
        }
        if (preprocessing == "bandpass-wider") {
            preprocessorConfig.bandPassLowHz = 300.0;
            preprocessorConfig.bandPassHighHz = 4500.0;
        }
        if (preprocessing == "bandpass-highwide") preprocessorConfig.bandPassHighHz = 4500.0;
        if (preprocessing == "bandpass-lowwide") preprocessorConfig.bandPassLowHz = 300.0;
        if (preprocessing == "bandpass-high5000") preprocessorConfig.bandPassHighHz = 5000.0;
        if (preprocessing == "bandpass-high5500") preprocessorConfig.bandPassHighHz = 5500.0;
        SstvPreprocessor preprocessor(preprocessorConfig);
        const bool dualPath = qEnvironmentVariableIntValue("DECODIUM_AVT_DUAL_PATH") == 1;
        const bool dualEdge = qEnvironmentVariableIntValue("DECODIUM_AVT_DUAL_EDGE") == 1;
        const int dualEdgeDelay = qEnvironmentVariableIntValue("DECODIUM_AVT_DUAL_EDGE_DELAY");
        QVERIFY(!(dualPath && dualEdge));
        QVERIFY(dualEdgeDelay >= 0 && dualEdgeDelay <= 5);
        SstvPreprocessorConfig secondaryConfig;
        secondaryConfig.bandPassEnabled = false;
        if (qEnvironmentVariableIntValue("DECODIUM_AVT_DUAL_EDGE_FILTER") == 1) {
            secondaryConfig.bandPassEnabled = true;
            secondaryConfig.bandPassLowHz = qEnvironmentVariable("DECODIUM_AVT_DUAL_EDGE_LOW", "500").toDouble();
            secondaryConfig.bandPassHighHz = qEnvironmentVariable("DECODIUM_AVT_DUAL_EDGE_HIGH", "3500").toDouble();
            QVERIFY(std::isfinite(secondaryConfig.bandPassLowHz)
                    && std::isfinite(secondaryConfig.bandPassHighHz)
                    && secondaryConfig.bandPassLowHz >= 100.0
                    && secondaryConfig.bandPassHighHz > secondaryConfig.bandPassLowHz
                    && secondaryConfig.bandPassHighHz <= 5500.0);
        }
        SstvPreprocessor secondaryPreprocessor(secondaryConfig);
        std::vector<double> firCoefficients(static_cast<std::size_t>(firTaps));
        std::vector<float> firHistory(static_cast<std::size_t>(firTaps), 0.0F);
        const int allpassStages = preprocessing == "allpass4" ? 4 : preprocessing == "allpass6" ? 6
            : preprocessing == "allpass8" ? 8 : 0;
        std::array<double, 8U> allpassState {};
        double notchZ1 = 0.0, notchZ2 = 0.0;
        double notchB0 = 0.0, notchB1 = 0.0, notchB2 = 0.0, notchA1 = 0.0, notchA2 = 0.0;
        if (preprocessing == "outside-notch") {
            QVERIFY(std::isfinite(notchHz) && notchHz >= 100.0 && notchHz <= 5000.0);
            QVERIFY(std::isfinite(notchQ) && notchQ >= 2.0 && notchQ <= 100.0);
            const double omega = 2.0 * 3.141592653589793 * notchHz / 12000.0;
            const double alpha = std::sin(omega) / (2.0 * notchQ);
            const double a0 = 1.0 + alpha;
            notchB0 = 1.0 / a0; notchB1 = -2.0 * std::cos(omega) / a0; notchB2 = notchB0;
            notchA1 = notchB1; notchA2 = (1.0 - alpha) / a0;
        }
        std::size_t firCursor = 0U;
        if (preprocessing == "equalizer") {
            double absoluteSum = 0.0;
            for (int i = 0; i < firTaps; ++i) {
                bool valid = false;
                const double value = equalizerValues.at(i).toDouble(&valid);
                QVERIFY(valid && std::isfinite(value));
                firCoefficients[static_cast<std::size_t>(i)] = value;
                absoluteSum += std::abs(value);
            }
            QVERIFY(absoluteSum <= 5.0);
        } else if (firTaps) {
            constexpr double pi = 3.141592653589793;
            const int centre = (firTaps - 1) / 2;
            double gain = 0.0;
            for (int i = 0; i < firTaps; ++i) {
                const int offset = i - centre;
                const double ideal = offset == 0 ? 2.0 * (2500.0 - 900.0) / 12000.0
                    : (std::sin(2.0 * pi * 2500.0 * offset / 12000.0)
                       - std::sin(2.0 * pi * 900.0 * offset / 12000.0)) / (pi * offset);
                const double window = 0.54 - 0.46 * std::cos(2.0 * pi * i / (firTaps - 1));
                firCoefficients[static_cast<std::size_t>(i)] = ideal * window;
                gain += ideal * window * std::cos(2.0 * pi * 1900.0 * offset / 12000.0);
            }
            QVERIFY(std::abs(gain) > 0.1);
            for (double& coefficient : firCoefficients) coefficient /= gain;
        }
        auto demodConfig = SstvFrequencyDemodulatorConfig::sstvDefaults();
        const int diagnosticHilbertTaps = qEnvironmentVariableIntValue("DECODIUM_AVT_HILBERT_TAPS");
        if (diagnosticHilbertTaps != 0) {
            QVERIFY(diagnosticHilbertTaps >= 15 && diagnosticHilbertTaps <= 255
                    && diagnosticHilbertTaps % 2 == 1);
            demodConfig.hilbertTaps = static_cast<std::size_t>(diagnosticHilbertTaps);
        }
        demodConfig.averagingSamples = 3U;
        demodConfig.hopSamples = 1U;
        SstvFrequencyDemodulator demodulator(demodConfig);
        SstvFrequencyDemodulator secondaryDemodulator(demodConfig);
        SstvAvtCountdownDetectorConfig detectorConfig;
        detectorConfig.expectedMode = SstvAvtMode::Avt90;
        detectorConfig.variant = SstvAvtVariant::Qrm;
        detectorConfig.observationSpanSamples = 3U;
        qsizetype onset = -1;
        for (qsizetype position = 0; position < data.size(); position += 8) {
            float sample = 0;
            std::memcpy(&sample, data.constData() + position, 4U);
            if (std::abs(sample) > 0.02F) { onset = position; break; }
        }
        QVERIFY(onset >= 0);
        detectorConfig.searchStartSample = static_cast<std::uint64_t>(onset / 8) / 4U + kSampleRate;
        SstvAvtCountdownDetector detector(detectorConfig);
        std::array<std::unique_ptr<SstvAvtRxSession>, 9U> sessions;
        std::uint64_t acquiredImageStart = 0U;
        const SstvAvtMapper edgeMapper({SstvAvtMode::Avt90, kSampleRate, 0, SstvAvtVariant::Qrm});
        std::vector<SstvFrequencyObservation> previousSecondary;
        constexpr std::size_t framesPerBlock = 4096U;
        for (qsizetype position = 0; position < data.size(); position += framesPerBlock * 8U) {
            const auto frames = std::min<std::size_t>(framesPerBlock,
                static_cast<std::size_t>(data.size() - position) / 8U);
            std::vector<float> mono(frames);
            for (std::size_t i = 0; i < frames; ++i) {
                float left = 0, right = 0;
                std::memcpy(&left, data.constData() + position + i * 8U, 4U);
                std::memcpy(&right, data.constData() + position + i * 8U + 4U, 4U);
                const double time = (static_cast<double>(position / 8) + i) / 48000.0;
                noiseState = 1664525U * noiseState + 1013904223U;
                const double noise = (static_cast<double>(noiseState >> 8U) / 16777215.0 * 2.0 - 1.0) * noisePeak;
                mono[i] = (left + right) * 0.5F + static_cast<float>(
                    noise + interferencePeak * std::sin(2.0 * 3.141592653589793 * interferenceHz * time));
            }
            const auto converted = resampler.process(mono);
            auto processed = preprocessing == "off" ? converted : preprocessor.process(converted);
            if (firTaps) {
                for (float& sample : processed) {
                    firHistory[firCursor] = sample;
                    double filtered = 0.0;
                    for (std::size_t i = 0; i < firCoefficients.size(); ++i)
                        filtered += firCoefficients[i] * firHistory[(firCursor + firHistory.size() - i) % firHistory.size()];
                    firCursor = (firCursor + 1U) % firHistory.size();
                    sample = static_cast<float>(filtered);
                }
            }
            if (allpassStages) {
                for (float& sample : processed) {
                    double value = sample;
                    for (int stage = 0; stage < allpassStages; ++stage) {
                        const double next = 0.4 * value + allpassState[static_cast<std::size_t>(stage)];
                        allpassState[static_cast<std::size_t>(stage)] = value - 0.4 * next;
                        value = next;
                    }
                    sample = static_cast<float>(value);
                }
            }
            if (preprocessing == "outside-notch") {
                for (float& sample : processed) {
                    const double filtered = notchB0 * sample + notchZ1;
                    notchZ1 = notchB1 * sample - notchA1 * filtered + notchZ2;
                    notchZ2 = notchB2 * sample - notchA2 * filtered;
                    sample = static_cast<float>(filtered);
                }
            }
            const auto observations = demodulator.consume(processed);
            const auto secondaryObservations = (dualPath || dualEdge)
                ? secondaryDemodulator.consume(secondaryPreprocessor.process(converted))
                : std::vector<SstvFrequencyObservation> {};
            if (!sessions[0] && detector.state() == SstvAvtCountdownDetectorState::Searching) {
                const auto found = detector.consume(observations);
                if (found) {
                    QVERIFY(found->imageStartSample >= 6U);
                    acquiredImageStart = found->imageStartSample;
                    qInfo("Captured AVT image start: %llu", static_cast<unsigned long long>(found->imageStartSample));
                    for (std::size_t index = 0; index < sessions.size(); ++index) {
                        SstvAvtRxSessionConfig config;
                        config.mode = SstvAvtMode::Avt90;
                        config.variant = SstvAvtVariant::Qrm;
                        config.sampleRate = kSampleRate;
                        config.imageStartSample = found->imageStartSample + index - 6U;
                        config.observationSpanSamples = 1U;
                        sessions[index] = std::make_unique<SstvAvtRxSession>(config);
                    }
                }
            }
            if (sessions[0]) {
                std::vector<SstvFrequencyObservation> edgeObservations;
                if (dualEdge) {
                    QCOMPARE(secondaryObservations.size(), observations.size());
                    edgeObservations = observations;
                    std::vector<SstvFrequencyObservation> delayedSecondary = previousSecondary;
                    delayedSecondary.insert(delayedSecondary.end(), secondaryObservations.begin(), secondaryObservations.end());
                    for (std::size_t i = 0; i < observations.size(); ++i) {
                        QCOMPARE(secondaryObservations[i].centreSample, observations[i].centreSample);
                        const auto centre = observations[i].centreSample;
                        if (centre < acquiredImageStart) continue;
                        const auto relative = centre - acquiredImageStart;
                        if (relative >= edgeMapper.imageSampleCount() || relative % 1500U < 1490U) continue;
                        const auto pixel = edgeMapper.positionAtSample(relative);
                        const auto delayedIndex = previousSecondary.size() + i;
                        if (pixel.pixel == 319U && delayedIndex >= static_cast<std::size_t>(dualEdgeDelay)) {
                            edgeObservations[i] = delayedSecondary[delayedIndex - dualEdgeDelay];
                            edgeObservations[i].centreSample = observations[i].centreSample;
                            edgeObservations[i].sequence = observations[i].sequence;
                        }
                    }
                    const auto keep = std::min<std::size_t>(5U, delayedSecondary.size());
                    previousSecondary.assign(delayedSecondary.end() - keep, delayedSecondary.end());
                }
                for (std::size_t index = 0; index < sessions.size(); ++index)
                    static_cast<void>(sessions[index]->consume(dualPath ? secondaryObservations
                        : (dualEdge && index == 6U) ? edgeObservations : observations));
            }
        }
        QVERIFY2(sessions[0] != nullptr, "captured countdown was not acquired");
        for (std::size_t index = 0; index < sessions.size(); ++index) {
            QCOMPARE(sessions[index]->finish(), SstvAvtRxSessionState::Complete);
            const auto image = sessions[index]->snapshot();
            std::uint64_t total = 0;
            int edgeMax = 0;
            int firstMax = 0;
            std::array<int, 3U> lastChannelMax {};
            std::array<std::uint32_t, 3U> lastChannelMaxRow {};
            std::array<std::uint32_t, 3U> lastChannelOver20 {};
            std::array<double, 3U> edgeDeltaSquared {};
            std::array<double, 3U> edgeDeltaError {};
            std::array<double, 3U> edgeErrorSquared {};
            std::uint64_t compensatedError = 0U;
            std::uint64_t originalEdgeError = 0U;
            int compensatedMax = 0;
            std::uint32_t compensatedOver20 = 0U;
            for (std::uint32_t y = 0; y < 240U; ++y) {
                for (std::uint32_t x = 0; x < 320U; ++x) {
                    const int r = x * 255U / 319U, g = y * 255U / 239U, b = (r + g) / 2;
                    const auto actual = image.pixel(x, y);
                    const std::array<int, 3U> values {{actual.red, actual.green, actual.blue}};
                    const std::array<int, 3U> expected {{variant == "a" ? r : 255-r,
                                                        variant == "a" ? g : 255-g,
                                                        variant == "a" ? b : 255-b}};
                    for (std::size_t component = 0; component < 3U; ++component) {
                        const int error = std::abs(values[component] - expected[component]);
                        total += error;
                        if (x == 0U) firstMax = std::max(firstMax, error);
                        if (x == 319U) {
                            edgeMax = std::max(edgeMax, error);
                            if (error > lastChannelMax[component]) {
                                lastChannelMax[component] = error;
                                lastChannelMaxRow[component] = y;
                            }
                            if (error > 20) ++lastChannelOver20[component];
                            // The next transmitted component starts at x=0.
                            // The smooth source has r(0)=0 and b(0)=g/2;
                            // QRM line reordering does not change r(0).
                            const int nextA = component == 0U ? g
                                : component == 1U ? g / 2 : 0;
                            const int next = variant == "a" ? nextA : 255 - nextA;
                            const double delta = next - expected[component];
                            const double signedError = values[component] - expected[component];
                            edgeDeltaSquared[component] += delta * delta;
                            edgeDeltaError[component] += delta * signedError;
                            edgeErrorSquared[component] += signedError * signedError;
                            if (!(component == 2U && y == 239U)) {
                                const std::uint32_t nextRow = y == 238U ? 1U : y + 2U;
                                const auto nextPixel = component == 2U
                                    ? image.pixel(0U, nextRow) : image.pixel(0U, y);
                                const int nextDecoded = component == 0U ? nextPixel.green
                                    : component == 1U ? nextPixel.blue : nextPixel.red;
                                const int corrected = std::clamp(
                                    static_cast<int>(std::lround((values[component] - 0.33 * nextDecoded) / 0.67)),
                                    0, 255);
                                const int correctedError = std::abs(corrected - expected[component]);
                                compensatedError += correctedError;
                                originalEdgeError += error;
                                compensatedMax = std::max(compensatedMax, correctedError);
                                if (correctedError > 20) ++compensatedOver20;
                            }
                        }
                    }
                }
            }
            qInfo("Captured AVT shift %d: MAE %.4f first/last-column max %d/%d",
                  static_cast<int>(index) - 6, double(total) / (320.0 * 240.0 * 3.0), firstMax, edgeMax);
            if (index == 6U) {
                for (std::size_t channel = 0; channel < 3U; ++channel)
                    qInfo("Captured AVT last column channel %zu: max %d at row %u; %u rows above 20",
                          channel, lastChannelMax[channel], lastChannelMaxRow[channel],
                          lastChannelOver20[channel]);
                for (std::size_t channel = 0; channel < 3U; ++channel) {
                    const double slope = edgeDeltaSquared[channel] > 0.0
                        ? edgeDeltaError[channel] / edgeDeltaSquared[channel] : 0.0;
                    const double residual = edgeErrorSquared[channel]
                        - 2.0 * slope * edgeDeltaError[channel]
                        + slope * slope * edgeDeltaSquared[channel];
                    qInfo("Captured AVT last column channel %zu next-tone slope %.4f residual RMS %.3f",
                          channel, slope, std::sqrt(std::max(0.0, residual) / 240.0));
                }
                qInfo("Captured AVT estimated-neighbour correction original/corrected error %llu/%llu; corrected max %d, above 20 %u",
                      static_cast<unsigned long long>(originalEdgeError),
                      static_cast<unsigned long long>(compensatedError), compensatedMax,
                      compensatedOver20);
            }
        }
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
