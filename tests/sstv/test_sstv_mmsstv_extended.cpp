// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../src/sstv/analog/SstvMmsstvExtended.h"
#include "../../src/sstv/analog/SstvMmsstvExtendedRxSession.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace decodium::sstv;

namespace {

const std::array<SstvMmsstvMode, 19U> kModes {{
    SstvMmsstvMode::Mp73,
    SstvMmsstvMode::Mp115,
    SstvMmsstvMode::Mp140,
    SstvMmsstvMode::Mp175,
    SstvMmsstvMode::Mr73,
    SstvMmsstvMode::Mr90,
    SstvMmsstvMode::Mr115,
    SstvMmsstvMode::Mr140,
    SstvMmsstvMode::Mr175,
    SstvMmsstvMode::Ml180,
    SstvMmsstvMode::Ml240,
    SstvMmsstvMode::Ml280,
    SstvMmsstvMode::Ml320,
    SstvMmsstvMode::Mp73Narrow,
    SstvMmsstvMode::Mp110Narrow,
    SstvMmsstvMode::Mp140Narrow,
    SstvMmsstvMode::Mc110Narrow,
    SstvMmsstvMode::Mc140Narrow,
    SstvMmsstvMode::Mc180Narrow,
}};

QJsonObject fixture()
{
#ifndef DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE
#error "MMSSTV extended fixture path is required"
#endif
    QFile input(QString::fromUtf8(
        DECODIUM_SSTV_MMSSTV_EXTENDED_FIXTURE));
    if (!input.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll());
    return document.object();
}

QJsonObject fixtureMode(const QJsonObject& root, const char* id)
{
    const QJsonObject section = root.value(
        QString::fromLatin1(std::string(id).find("narrow")
                                == std::string::npos
                            ? "wideExtendedVis" : "narrowVis"))
                                    .toObject();
    for (const QJsonValue& value : section.value(QStringLiteral("modes"))
                                       .toArray()) {
        const QJsonObject row = value.toObject();
        if (row.value(QStringLiteral("id")).toString()
            == QString::fromLatin1(id)) {
            return row;
        }
    }
    return {};
}

std::uint64_t microseconds(Picoseconds duration)
{
    return static_cast<std::uint64_t>(
        duration.count / kPicosecondsPerMicrosecond);
}

std::uint8_t average(std::uint8_t a, std::uint8_t b)
{
    return static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(a) + b) / 2U);
}

double expectedFrequency(const SstvMmsstvModeSpec& spec,
                         const SstvMmsstvPosition& position,
                         SstvRgbPixel source)
{
    if (position.region == SstvMmsstvRegion::Sync) {
        return spec.syncFrequencyHz;
    }
    if (position.region == SstvMmsstvRegion::Porch) {
        return spec.porchFrequencyHz;
    }
    const SstvYCbCrPixel converted =
        SstvColourConverter::rgbToYCbCr(source);
    std::uint8_t value = 0U;
    if (spec.layout == SstvMmsstvLayout::McSequentialRgb) {
        value = position.componentIndex == 0U ? source.red
            : position.componentIndex == 1U ? source.green : source.blue;
    } else {
        value = position.componentIndex == 0U
                || position.componentIndex == 3U
            ? converted.luminance
            : position.componentIndex == 1U
                ? average(converted.chrominanceRed,
                          converted.chrominanceRed)
                : average(converted.chrominanceBlue,
                          converted.chrominanceBlue);
    }
    return SstvMmsstvProtocol::frequencyForValue(spec, value);
}

std::vector<SstvFrequencyObservation> observationsFor(
    SstvMmsstvMode mode,
    SstvRgbPixel source,
    std::uint64_t imageStart = 100U)
{
    constexpr std::uint32_t rate = 12'000U;
    SstvMmsstvMapper mapper({mode, rate, 0});
    const SstvMmsstvModeSpec spec = mapper.modeSpec();
    std::vector<SstvFrequencyObservation> result;
    std::uint64_t cursor = 0U;
    std::uint64_t sequence = 1U;
    while (cursor < mapper.imageSampleCount()) {
        const SstvMmsstvPosition position = mapper.positionAtSample(cursor);
        if (position.segmentEndSample <= cursor) {
            throw std::logic_error("test mapper did not advance");
        }
        if (position.region == SstvMmsstvRegion::Pixel
            || position.region == SstvMmsstvRegion::Sync) {
            const std::uint64_t centre = position.segmentStartSample
                + (position.segmentEndSample
                   - position.segmentStartSample) / 2U;
            const double frequency = expectedFrequency(spec, position, source);
            SstvFrequencyObservation observation;
            observation.status = SstvFrequencyStatus::Valid;
            observation.sequence = sequence++;
            observation.centreSample = imageStart + centre;
            observation.rawFrequencyHz = frequency;
            observation.correctedFrequencyHz = frequency;
            observation.confidence = 1.0;
            result.push_back(observation);
        }
        cursor = position.segmentEndSample;
    }
    return result;
}

std::uint64_t renderHash(SstvMmsstvMode mode, std::size_t chunk)
{
    const std::size_t count = SstvMmsstvEncoder::pixelCount(mode);
    std::vector<SstvRgbPixel> pixels(count, {41U, 127U, 219U});
    SstvMmsstvEncoder encoder(pixels, {mode, 8'000U, 0, 0.6, 0.95});
    std::vector<std::int16_t> buffer(chunk);
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    while (!encoder.complete()) {
        const std::size_t produced = encoder.pullPcm16(
            buffer.data(), buffer.size());
        if (produced == 0U) {
            throw std::logic_error("test encoder stalled");
        }
        for (std::size_t index = 0U; index < produced; ++index) {
            const std::uint16_t bits = static_cast<std::uint16_t>(buffer[index]);
            hash ^= static_cast<std::uint8_t>(bits & 0xffU);
            hash *= 1'099'511'628'211ULL;
            hash ^= static_cast<std::uint8_t>(bits >> 8U);
            hash *= 1'099'511'628'211ULL;
        }
    }
    return hash;
}

} // namespace

class TestSstvMmsstvExtended final : public QObject
{
    Q_OBJECT

private slots:
    void allCanonicalFixtureRowsMatch()
    {
        const QJsonObject root = fixture();
        QVERIFY(!root.isEmpty());
        for (const SstvMmsstvMode mode : kModes) {
            const SstvMmsstvModeSpec spec = SstvMmsstvProtocol::spec(mode);
            const QJsonObject row = fixtureMode(root, spec.stableId);
            QVERIFY2(!row.isEmpty(), spec.stableId);
            QCOMPARE(spec.width,
                     static_cast<std::uint32_t>(
                         row.value(QStringLiteral("width")).toInt()));
            QCOMPARE(spec.height,
                     static_cast<std::uint32_t>(
                         row.value(QStringLiteral("height")).toInt()));
            QCOMPARE(spec.scanCount,
                     static_cast<std::uint32_t>(
                         row.value(QStringLiteral("scans")).toInt()));
            QCOMPARE(spec.linesPerScan,
                     static_cast<std::uint8_t>(
                         row.value(QStringLiteral("linesPerScan")).toInt()));
            QCOMPARE(microseconds(spec.syncDuration),
                     static_cast<std::uint64_t>(
                         row.value(QStringLiteral("syncUs")).toDouble()));
            QCOMPARE(microseconds(spec.porchDuration),
                     static_cast<std::uint64_t>(
                         row.value(QStringLiteral("porchUs")).toDouble()));
            QCOMPARE(microseconds(spec.scanDuration),
                     static_cast<std::uint64_t>(
                         row.value(QStringLiteral("scanUs")).toDouble()));
            QCOMPARE(microseconds(spec.imageDuration),
                     static_cast<std::uint64_t>(
                         row.value(QStringLiteral("imageBodyUs")).toDouble()));
            const QString codeKey = spec.narrow
                ? QStringLiteral("payload")
                : QStringLiteral("extensionRawOctet");
            QCOMPARE(spec.visWireCode,
                     static_cast<std::uint8_t>(row.value(codeKey).toInt()));
        }
    }

    void visMappingsAreUniqueAndConflictResolved()
    {
        std::array<bool, 256U> seen {};
        for (const SstvMmsstvMode mode : kModes) {
            const auto spec = SstvMmsstvProtocol::spec(mode);
            if (spec.narrow) {
                QCOMPARE(SstvMmsstvProtocol::modeForNarrowPayload(
                             spec.visWireCode),
                         std::optional<SstvMmsstvMode>(mode));
            } else {
                QVERIFY(!seen[spec.visWireCode]);
                seen[spec.visWireCode] = true;
                QCOMPARE(SstvMmsstvProtocol::modeForExtendedRaw(
                             spec.visWireCode),
                         std::optional<SstvMmsstvMode>(mode));
            }
        }
        QCOMPARE(SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr140).visWireCode,
                 static_cast<std::uint8_t>(0x4aU));
        QCOMPARE(SstvMmsstvProtocol::spec(SstvMmsstvMode::Mr175).visWireCode,
                 static_cast<std::uint8_t>(0x4cU));
        QCOMPARE(microseconds(SstvMmsstvProtocol::spec(
                                  SstvMmsstvMode::Mc110Narrow)
                                  .primaryComponentDuration),
                 140'000U);
    }

    void mapperCoversEverySampleAndKeepsHoldSegments()
    {
        for (const SstvMmsstvMode mode : kModes) {
            SstvMmsstvMapper mapper({mode, 12'000U, 0});
            const auto spec = mapper.modeSpec();
            QCOMPARE(mapper.scanEndSample(spec.scanCount - 1U),
                     mapper.imageSampleCount());
            std::uint64_t cursor = 0U;
            bool sawHold = false;
            while (cursor < mapper.imageSampleCount()) {
                const auto position = mapper.positionAtSample(cursor);
                QVERIFY(position.valid());
                QCOMPARE(position.segmentStartSample, cursor);
                QVERIFY(position.segmentEndSample > cursor);
                sawHold = sawHold
                    || position.region == SstvMmsstvRegion::HoldLast;
                cursor = position.segmentEndSample;
            }
            QCOMPARE(sawHold,
                     spec.layout == SstvMmsstvLayout::MrHorizontal422);
        }
    }

    void encoderIsStreamingAndChunkInvariant()
    {
        const SstvMmsstvMode modes[] {
            SstvMmsstvMode::Mp73,
            SstvMmsstvMode::Mr175,
            SstvMmsstvMode::Mc110Narrow,
        };
        for (const SstvMmsstvMode mode : modes) {
            QCOMPARE(renderHash(mode, 257U), renderHash(mode, 8'191U));
        }

        std::vector<SstvRgbPixel> pixels(
            SstvMmsstvEncoder::pixelCount(SstvMmsstvMode::Mp73));
        SstvMmsstvEncoder encoder(pixels);
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullPcm16(nullptr,
                             SstvMmsstvEncoder::MaximumSamplesPerPull + 1U));
        encoder.cancel();
        QVERIFY(encoder.cancelled());
        encoder.reset();
        QVERIFY(!encoder.cancelled());
        QCOMPARE(encoder.producedSamples(), 0U);
    }

    void representativeLayoutsDecodeProgressively()
    {
        const SstvMmsstvMode modes[] {
            SstvMmsstvMode::Mp73Narrow,
            SstvMmsstvMode::Mr175,
            SstvMmsstvMode::Mc110Narrow,
        };
        const SstvRgbPixel source {49U, 133U, 211U};
        for (const SstvMmsstvMode mode : modes) {
            const auto observations = observationsFor(mode, source);
            SstvMmsstvRxSession session(
                {mode, 12'000U, 100U, 1U, 0, 0.0, 0.2,
                 SstvImageFrame::kDefaultMaximumDirtyEvents});
            for (std::size_t offset = 0U; offset < observations.size();) {
                const std::size_t count = std::min<std::size_t>(
                    4'093U, observations.size() - offset);
                static_cast<void>(session.consume(
                    observations.data() + offset, count));
                offset += count;
            }
            QCOMPARE(session.finish(), SstvMmsstvRxSessionState::Complete);
            const SstvImageSnapshot image = session.snapshot();
            QVERIFY(image.isComplete());
            const auto spec = SstvMmsstvProtocol::spec(mode);
            QCOMPARE(image.width, spec.width);
            QCOMPARE(image.height, spec.height);
            const SstvRgbPixel expected =
                spec.layout == SstvMmsstvLayout::McSequentialRgb
                ? source
                : SstvColourConverter::yCbCrToRgb(
                      SstvColourConverter::rgbToYCbCr(source));
            QCOMPARE(image.pixel(0U, 0U), expected);
            QCOMPARE(image.pixel(spec.width - 1U, spec.height - 1U),
                     expected);
            QCOMPARE(session.decoderMetrics().linesPublished,
                     static_cast<std::uint64_t>(spec.height));
        }
    }

    void hostileInputIsBounded()
    {
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMmsstvMapper({SstvMmsstvMode::Mp73, 1U, 0}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMmsstvMapper(
                {SstvMmsstvMode::Mp73, 12'000U, 100'001}));
        SstvMmsstvDecoder decoder;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            decoder.consume(nullptr,
                            SstvMmsstvDecoder::MaximumObservationsPerConsume
                                + 1U));
    }
};

QTEST_APPLESS_MAIN(TestSstvMmsstvExtended)
#include "test_sstv_mmsstv_extended.moc"
