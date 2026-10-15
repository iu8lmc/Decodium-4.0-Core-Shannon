// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvScottie.h"

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

constexpr long double kTwoPi =
    6.2831853071795864769252867665590057683943387987502L;

const std::array<SstvScottieMode, 5U> kModes {{
    SstvScottieMode::S1,
    SstvScottieMode::S2,
    SstvScottieMode::S3,
    SstvScottieMode::S4,
    SstvScottieMode::DX,
}};

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

std::uint64_t unsignedDuration(Picoseconds duration)
{
    if (duration.count < 0) {
        throw std::logic_error("test received a negative duration");
    }
    return static_cast<std::uint64_t>(duration.count);
}

SstvRgbPixel testPixel(std::uint32_t x, std::uint32_t y)
{
    return {
        static_cast<std::uint8_t>((x * 17U + y * 3U + 11U) & 0xffU),
        static_cast<std::uint8_t>((x * 5U + y * 29U + 73U) & 0xffU),
        static_cast<std::uint8_t>((255U - x + y * 7U) & 0xffU),
    };
}

std::vector<SstvRgbPixel> testImage(SstvScottieMode mode)
{
    const SstvScottieModeSpec spec = SstvScottieProtocol::spec(mode);
    std::vector<SstvRgbPixel> pixels(
        SstvScottieEncoder::pixelCount(mode));
    for (std::uint32_t y = 0U; y < spec.height; ++y) {
        for (std::uint32_t x = 0U; x < SstvScottieProtocol::Width; ++x) {
            pixels[static_cast<std::size_t>(y)
                       * SstvScottieProtocol::Width
                   + x] = testPixel(x, y);
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
        break;
    }
    throw std::logic_error("invalid test component");
}

SstvFrequencyObservation frequencyObservation(std::uint64_t centreSample,
                                              double frequencyHz,
                                              double confidence = 1.0)
{
    SstvFrequencyObservation observation;
    observation.status = SstvFrequencyStatus::Valid;
    observation.centreSample = centreSample;
    observation.rawFrequencyHz = frequencyHz;
    observation.correctedFrequencyHz = frequencyHz;
    observation.confidence = confidence;
    observation.validSampleFraction = 1.0;
    return observation;
}

std::array<std::uint64_t, 3U> componentStarts(
    const SstvScottieModeSpec& spec)
{
    const std::uint64_t porch = unsignedDuration(spec.porchDuration);
    const std::uint64_t component = unsignedDuration(spec.componentDuration);
    const std::uint64_t sync = unsignedDuration(spec.syncDuration);
    return {{
        porch,
        2U * porch + component,
        unsignedDuration(spec.embeddedSyncOffset) + sync + porch,
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
    }
    throw std::logic_error("invalid component index");
}

std::vector<SstvFrequencyObservation> observationsForLine(
    const std::vector<SstvRgbPixel>& pixels,
    const SstvScottieModeSpec& spec,
    std::uint32_t line,
    std::uint64_t absoluteImageStart,
    std::uint32_t sampleRate,
    std::int32_t ppm,
    double frequencyOffset)
{
    const std::uint64_t lineTime = static_cast<std::uint64_t>(line)
        * unsignedDuration(spec.lineDuration);
    const auto starts = componentStarts(spec);
    const std::uint64_t pixelDuration =
        unsignedDuration(spec.pixelDuration);
    std::vector<SstvFrequencyObservation> result;
    result.reserve(static_cast<std::size_t>(spec.width) * 3U);

    for (std::size_t componentIndex = 0U;
         componentIndex < starts.size();
         ++componentIndex) {
        const auto component = componentAt(componentIndex);
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
                throw std::logic_error("test oracle produced an empty pixel");
            }
            const std::uint64_t centre = absoluteImageStart
                + startSample + (endSample - startSample) / 2U;
            const auto& pixel = pixels[static_cast<std::size_t>(line)
                                           * spec.width
                                       + x];
            result.push_back(frequencyObservation(
                centre,
                SstvScottieProtocol::frequencyForValue(
                    componentValue(pixel, component))
                    + frequencyOffset));
        }
    }
    return result;
}

SstvScottieLineSync lineSync(const SstvScottieModeSpec& spec,
                             std::uint32_t line,
                             std::uint64_t absoluteImageStart,
                             std::uint32_t sampleRate,
                             std::int32_t ppm,
                             bool predicted = false)
{
    const std::uint64_t syncTime = static_cast<std::uint64_t>(line)
        * unsignedDuration(spec.lineDuration)
        + unsignedDuration(spec.embeddedSyncOffset);
    return {line,
            absoluteImageStart + oracleSamples(syncTime, sampleRate, ppm),
            predicted ? 0.65 : 0.95,
            predicted};
}

std::vector<float> renderPrefix(SstvScottieEncoder& encoder,
                                std::size_t count,
                                bool randomChunks)
{
    std::vector<float> output(count);
    std::uint32_t random = 0x5a17c9e3U;
    std::size_t offset = 0U;
    while (offset < output.size()) {
        random = random * 1'664'525U + 1'013'904'223U;
        const std::size_t requested = randomChunks
            ? 1U + random % 997U
            : output.size() - offset;
        const std::size_t amount = std::min(requested,
                                            output.size() - offset);
        const std::size_t produced = encoder.pullFloat(
            output.data() + offset, amount);
        if (produced == 0U) {
            throw std::runtime_error("Scottie test encoder made no progress");
        }
        offset += produced;
    }
    return output;
}

void consumeRandomChunks(
    SstvScottieDecoder& decoder,
    const std::vector<SstvFrequencyObservation>& observations)
{
    std::uint32_t random = 0x1badd00dU;
    std::size_t offset = 0U;
    while (offset < observations.size()) {
        random = random * 1'103'515'245U + 12'345U;
        const std::size_t count = std::min<std::size_t>(
            1U + random % 53U, observations.size() - offset);
        decoder.consume(observations.data() + offset, count);
        offset += count;
    }
}

std::array<double, SstvScottieEncoder::HeaderSegmentCount>
expectedHeader(std::uint8_t payload)
{
    std::array<double, SstvScottieEncoder::HeaderSegmentCount> result {};
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

} // namespace

class TestSstvScottie final : public QObject
{
    Q_OBJECT

private slots:
    void protocolFactsAndAuditedConflictsAreExplicit()
    {
        // Timing oracle: N7CXI-derived segment values recorded by both audited
        // SlowRX revisions, cross-checked against Robot36 and libsstv.  These
        // are protocol facts only.  No upstream expression/waveform is used.
        const auto s1 = SstvScottieProtocol::spec(SstvScottieMode::S1);
        QCOMPARE(s1.width, std::uint32_t {320U});
        QCOMPARE(s1.height, std::uint32_t {256U});
        QCOMPARE(s1.visPayload, std::uint8_t {60U});
        QCOMPARE(s1.pixelDuration.count, std::int64_t {432'000'000LL});
        QCOMPARE(s1.componentDuration.count,
                 std::int64_t {138'240'000'000LL});
        QCOMPARE(s1.embeddedSyncOffset.count,
                 std::int64_t {279'480'000'000LL});
        QCOMPARE(s1.lineDuration.count,
                 std::int64_t {428'220'000'000LL});
        QCOMPARE(s1.imageDuration.count,
                 std::int64_t {109'624'320'000'000LL});

        // SlowRX separately lists 428.380 ms for S1, 160 us longer than its
        // own G/B/R, porch and sync fields.  Robot36/libsstv yield the exact
        // 428.220 ms structural sum selected here.  This conflict remains an
        // evidence limitation, not a silently rounded constant.
        QVERIFY(s1.lineDuration.count != 428'380'000'000LL);

        const auto s2 = SstvScottieProtocol::spec(SstvScottieMode::S2);
        QCOMPARE(s2.width, std::uint32_t {320U});
        QCOMPARE(s2.visPayload, std::uint8_t {56U});
        QCOMPARE(s2.pixelDuration.count, std::int64_t {275'200'000LL});
        QCOMPARE(s2.componentDuration.count,
                 std::int64_t {88'064'000'000LL});
        QCOMPARE(s2.embeddedSyncOffset.count,
                 std::int64_t {179'128'000'000LL});
        QCOMPARE(s2.lineDuration.count,
                 std::int64_t {277'692'000'000LL});
        QCOMPARE(s2.imageDuration.count,
                 std::int64_t {71'089'152'000'000LL});

        // pySSTV is the one audited S2 path declaring width 160; QSSTV,
        // SlowRX, Robot36 and libsstv declare 320.  Selecting the four-source
        // geometry does not turn this self-tested implementation into a
        // Verified/interoperable mode.
        QVERIFY(s2.width != 160U);

        const auto s3 = SstvScottieProtocol::spec(SstvScottieMode::S3);
        QCOMPARE(s3.width, std::uint32_t {320U});
        QCOMPARE(s3.height, std::uint32_t {128U});
        QCOMPARE(s3.visPayload, std::uint8_t {52U});
        QCOMPARE(s3.pixelDuration.count, s1.pixelDuration.count);
        QCOMPARE(s3.componentDuration.count, s1.componentDuration.count);
        QCOMPARE(s3.lineDuration.count, s1.lineDuration.count);
        QCOMPARE(s3.imageDuration.count,
                 std::int64_t {54'812'160'000'000LL});

        const auto s4 = SstvScottieProtocol::spec(SstvScottieMode::S4);
        QCOMPARE(s4.width, std::uint32_t {320U});
        QCOMPARE(s4.height, std::uint32_t {128U});
        QCOMPARE(s4.visPayload, std::uint8_t {48U});
        QCOMPARE(s4.pixelDuration.count, s2.pixelDuration.count);
        QCOMPARE(s4.componentDuration.count, s2.componentDuration.count);
        QCOMPARE(s4.lineDuration.count, s2.lineDuration.count);
        QCOMPARE(s4.imageDuration.count,
                 std::int64_t {35'544'576'000'000LL});

        const auto dx = SstvScottieProtocol::spec(SstvScottieMode::DX);
        QCOMPARE(dx.width, std::uint32_t {320U});
        QCOMPARE(dx.visPayload, std::uint8_t {76U});
        QCOMPARE(dx.pixelDuration.count, std::int64_t {1'080'000'000LL});
        QCOMPARE(dx.componentDuration.count,
                 std::int64_t {345'600'000'000LL});
        QCOMPARE(dx.lineDuration.count,
                 std::int64_t {1'050'300'000'000LL});
        QCOMPARE(dx.imageDuration.count,
                 std::int64_t {268'876'800'000'000LL});

        // SlowRX lists 1.08053 ms for DX while its declared 1.0503 s line is
        // exactly consistent with 1.080 ms from Robot36/libsstv.  The chosen
        // structural value remains deterministic evidence, not an independent
        // long-duration fixture.
        QVERIFY(dx.pixelDuration.count != 1'080'530'000LL);

        for (const auto mode : kModes) {
            const auto spec = SstvScottieProtocol::spec(mode);
            QCOMPARE(spec.componentDuration.count,
                     spec.pixelDuration.count
                         * static_cast<std::int64_t>(spec.width));
            QCOMPARE(spec.lineDuration.count,
                     3LL * spec.componentDuration.count
                         + 3LL * spec.porchDuration.count
                         + spec.syncDuration.count);
            QCOMPARE(spec.imageDuration.count,
                     spec.lineDuration.count
                         * static_cast<std::int64_t>(spec.height));
        }
    }

    void pinnedLibsstvLandmarksMatchNativeTimingModel()
    {
#ifndef DECODIUM_SSTV_SCOTTIE_LIBSSTV_FIXTURE
        QFAIL("pinned libsstv fixture path was not configured");
#else
        QFile fixture(QString::fromUtf8(
            DECODIUM_SSTV_SCOTTIE_LIBSSTV_FIXTURE));
        QVERIFY2(fixture.open(QIODevice::ReadOnly),
                 qPrintable(fixture.errorString()));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            fixture.readAll(), &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());
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
        QCOMPARE(modes.size(), 2);
        for (const QJsonValue& value : modes) {
            QVERIFY(value.isObject());
            const QJsonObject item = value.toObject();
            const QString id = item.value(QStringLiteral("id")).toString();
            const SstvScottieMode mode = id == QStringLiteral("scottie-s3")
                ? SstvScottieMode::S3 : SstvScottieMode::S4;
            QVERIFY(id == QStringLiteral("scottie-s3")
                    || id == QStringLiteral("scottie-s4"));
            const SstvScottieModeSpec spec = SstvScottieProtocol::spec(mode);
            QCOMPARE(item.value(QStringLiteral("visPayload")).toInt(),
                     static_cast<int>(spec.visPayload));
            QCOMPARE(item.value(QStringLiteral("width")).toInt(),
                     static_cast<int>(spec.width));
            QCOMPARE(item.value(QStringLiteral("height")).toInt(),
                     static_cast<int>(spec.height));
            QCOMPARE(item.value(QStringLiteral("pixelDurationNanoseconds"))
                         .toInteger() * 1'000LL,
                     spec.pixelDuration.count);
            QCOMPARE(item.value(QStringLiteral("componentDurationNanoseconds"))
                         .toInteger() * 1'000LL,
                     spec.componentDuration.count);

            SstvScottieMapper mapper({mode, 12'000U, 0});
            constexpr std::uint64_t headerFrames = 10'920U;
            const auto starts = componentStarts(spec);
            const std::uint64_t extraSync = static_cast<std::uint64_t>(
                item.value(QStringLiteral("libsstvExtraLeadingSyncFrames"))
                    .toInteger());
            QCOMPARE(extraSync, std::uint64_t {108U});
            const std::uint64_t decodiumTotal = static_cast<std::uint64_t>(
                item.value(QStringLiteral("decodiumTotalFrames")).toInteger());
            QCOMPARE(decodiumTotal,
                     headerFrames + mapper.imageSampleCount());
            const std::uint64_t libsstvTotal = static_cast<std::uint64_t>(
                item.value(QStringLiteral("fullPcmFrames")).toInteger());
            QCOMPARE(libsstvTotal, decodiumTotal + extraSync);
            const QString digest = item.value(
                QStringLiteral("fullPcmSha256")).toString();
            QCOMPARE(digest.size(), 64);

            const QJsonArray landmarks = item.value(
                QStringLiteral("landmarks")).toArray();
            QCOMPARE(landmarks.size(), 5);
            for (const QJsonValue& landmarkValue : landmarks) {
                const QJsonObject landmark = landmarkValue.toObject();
                const QString name = landmark.value(
                    QStringLiteral("name")).toString();
                std::uint64_t expected = 0U;
                if (name == QStringLiteral("first-green-start")) {
                    expected = headerFrames + oracleSamples(
                        starts[0], 12'000U, 0);
                } else if (name == QStringLiteral("first-blue-start")) {
                    expected = headerFrames + oracleSamples(
                        starts[1], 12'000U, 0);
                } else if (name
                           == QStringLiteral("first-embedded-sync-start")) {
                    expected = headerFrames
                        + mapper.embeddedSyncStartSample(0U);
                } else if (name == QStringLiteral("first-red-start")) {
                    expected = headerFrames + oracleSamples(
                        starts[2], 12'000U, 0);
                } else if (name == QStringLiteral("second-line-start")) {
                    expected = headerFrames + mapper.lineStartSample(1U);
                } else {
                    QFAIL("unexpected pinned libsstv landmark name");
                }
                const std::uint64_t decodiumFrame =
                    static_cast<std::uint64_t>(landmark.value(
                        QStringLiteral("decodiumFrame")).toInteger());
                const std::uint64_t libsstvFrame =
                    static_cast<std::uint64_t>(landmark.value(
                        QStringLiteral("libFrame")).toInteger());
                QCOMPARE(decodiumFrame, expected);
                QCOMPARE(libsstvFrame, decodiumFrame + extraSync);
            }
        }
#endif
    }

    void mapperEncodesMidLineSyncAndFirstLineOrder()
    {
        for (const auto mode : kModes) {
            const auto spec = SstvScottieProtocol::spec(mode);
            SstvScottieMapper mapper({mode, 12'000U, 0});
            const auto starts = componentStarts(spec);

            auto position = mapper.positionAtElapsedTime(Picoseconds {0});
            QCOMPARE(position.region, SstvScottieRegion::Porch);
            QCOMPARE(position.line, std::uint32_t {0U});
            QCOMPARE(position.component, ColourComponent::Green);

            position = mapper.positionAtElapsedTime(Picoseconds {
                static_cast<std::int64_t>(starts[0])});
            QCOMPARE(position.region, SstvScottieRegion::Pixel);
            QCOMPARE(position.component, ColourComponent::Green);
            QCOMPARE(position.pixel, std::uint32_t {0U});

            position = mapper.positionAtElapsedTime(Picoseconds {
                static_cast<std::int64_t>(starts[1])});
            QCOMPARE(position.region, SstvScottieRegion::Pixel);
            QCOMPARE(position.component, ColourComponent::Blue);
            QCOMPARE(position.pixel, std::uint32_t {0U});

            position = mapper.positionAtElapsedTime(
                spec.embeddedSyncOffset);
            QCOMPARE(position.region, SstvScottieRegion::Sync);
            QCOMPARE(position.line, std::uint32_t {0U});

            const std::uint64_t redStart = starts[2];
            position = mapper.positionAtElapsedTime(Picoseconds {
                static_cast<std::int64_t>(redStart)});
            QCOMPARE(position.region, SstvScottieRegion::Pixel);
            QCOMPARE(position.component, ColourComponent::Red);
            QCOMPARE(position.pixel, std::uint32_t {0U});

            position = mapper.positionAtElapsedTime(spec.lineDuration);
            QCOMPARE(position.region, SstvScottieRegion::Porch);
            QCOMPARE(position.line, std::uint32_t {1U});
            QCOMPARE(position.component, ColourComponent::Green);

            position = mapper.positionAtElapsedTime(spec.imageDuration);
            QCOMPARE(position.region, SstvScottieRegion::Complete);
            QCOMPARE(position.line, spec.height);

            QCOMPARE(mapper.embeddedSyncStartSample(0U),
                     oracleSamples(unsignedDuration(spec.embeddedSyncOffset),
                                   12'000U,
                                   0));
            QCOMPARE(mapper.imageSampleCount(),
                     oracleSamples(unsignedDuration(spec.imageDuration),
                                   12'000U,
                                   0));
        }
    }

    void fractionalMappingCarriesAcrossPixelsAndClockError()
    {
        for (const auto mode : kModes) {
            const auto spec = SstvScottieProtocol::spec(mode);
            SstvScottieMapper mapper({mode, 44'100U, 0});
            std::size_t shorter = 0U;
            std::size_t longer = 0U;
            std::uint64_t sample = mapper.lineStartSample(0U);
            while (sample < mapper.embeddedSyncStartSample(0U)) {
                const auto position = mapper.positionAtSample(sample);
                if (position.region == SstvScottieRegion::Pixel
                    && position.component == ColourComponent::Green) {
                    const auto length = position.segmentEndSample
                        - position.segmentStartSample;
                    const auto exact = static_cast<long double>(
                        spec.pixelDuration.count)
                        * 44'100.0L / 1.0e12L;
                    shorter += length
                        == static_cast<std::uint64_t>(std::floor(exact));
                    longer += length
                        == static_cast<std::uint64_t>(std::ceil(exact));
                }
                QVERIFY(position.segmentEndSample > sample);
                sample = position.segmentEndSample;
            }
            QCOMPARE(shorter + longer, std::size_t {spec.width});
            QVERIFY(shorter > 0U || longer > 0U);

            SstvScottieMapper slow({mode, 12'000U, 300});
            SstvScottieMapper fast({mode, 12'000U, -300});
            QCOMPARE(slow.imageSampleCount(),
                     oracleSamples(unsignedDuration(spec.imageDuration),
                                   12'000U,
                                   300));
            QCOMPARE(fast.imageSampleCount(),
                     oracleSamples(unsignedDuration(spec.imageDuration),
                                   12'000U,
                                   -300));
            QVERIFY(slow.imageSampleCount() > mapper.lineEndSample(0U));
            QVERIFY(slow.imageSampleCount() > fast.imageSampleCount());

            const std::int64_t scaledSync = spec.embeddedSyncOffset.count
                + spec.embeddedSyncOffset.count * 300LL / 1'000'000LL;
            const auto sync = slow.positionAtElapsedTime(
                Picoseconds {scaledSync});
            QCOMPARE(sync.region, SstvScottieRegion::Sync);
        }
    }

    void standardHeaderAndNoInventedLeadingSync()
    {
        const std::array<std::size_t,
                         SstvScottieEncoder::HeaderSegmentCount>
            expectedSamples {{
                3'600U, 120U, 3'600U,
                360U, 360U, 360U, 360U, 360U,
                360U, 360U, 360U, 360U, 360U,
            }};

        for (const auto mode : kModes) {
            const auto spec = SstvScottieProtocol::spec(mode);
            const std::vector<SstvRgbPixel> pixels(
                SstvScottieEncoder::pixelCount(mode), {0U, 0U, 0U});
            SstvScottieEncoderConfig config;
            config.mode = mode;
            SstvScottieEncoder encoder(pixels, config);
            const auto frequencies = expectedHeader(spec.visPayload);
            std::vector<float> scratch(3'600U);
            for (std::size_t index = 0U;
                 index < frequencies.size();
                 ++index) {
                const auto position = encoder.position();
                QCOMPARE(position.stage, SstvScottieEncoderStage::Header);
                QCOMPARE(position.headerSegment, index);
                QVERIFY(std::abs(position.frequencyHz - frequencies[index])
                        < 1.0e-12);
                QCOMPARE(encoder.pullFloat(scratch.data(),
                                           expectedSamples[index]),
                         expectedSamples[index]);
            }

            const auto firstImage = encoder.position();
            QCOMPARE(firstImage.stage, SstvScottieEncoderStage::Image);
            QCOMPARE(firstImage.image.region, SstvScottieRegion::Porch);
            QCOMPARE(firstImage.image.component, ColourComponent::Green);
            QCOMPARE(firstImage.image.line, std::uint32_t {0U});
            QCOMPARE(firstImage.frequencyHz,
                     SstvScottieProtocol::PorchFrequencyHz);
            QCOMPARE(encoder.producedSamples(), std::uint64_t {10'920U});
            QCOMPARE(encoder.totalSamples(),
                     std::uint64_t {10'920U}
                         + oracleSamples(unsignedDuration(spec.imageDuration),
                                         12'000U,
                                         0));
        }

        // Audited libsstv inserts a leading 9 ms sync here.  QSSTV TX,
        // pySSTV TX, SlowRX RX and Robot36 RX instead place the first embedded
        // sync after G/B.  This test intentionally fixes the latter ordering;
        // it is not an independent received-audio fixture.
    }

    void encoderIsChunkInvariantPhaseContinuousAndBounded()
    {
        for (const auto mode : kModes) {
            const auto pixels = testImage(mode);
            SstvScottieEncoderConfig config;
            config.mode = mode;
            SstvScottieEncoder contiguous(pixels, config);
            SstvScottieEncoder fragmented(pixels, config);
            const auto first = renderPrefix(contiguous, 120'000U, false);
            const auto second = renderPrefix(fragmented, 120'000U, true);
            QVERIFY(first == second);
            QCOMPARE(contiguous.producedSamples(),
                     fragmented.producedSamples());
            QCOMPARE(contiguous.metrics().residentImageBytes,
                     SstvScottieEncoder::pixelCount(mode)
                         * sizeof(SstvRgbPixel));
            QVERIFY(contiguous.metrics().segmentTransitions > 1'000U);
            QVERIFY(contiguous.metrics().tone.peakAfterClamp
                    <= kDefaultSstvTxHeadroom + 1.0e-6);
        }

        SstvScottieEncoderConfig config;
        config.mode = SstvScottieMode::S2;
        const auto pixels = testImage(config.mode);
        SstvScottieEncoder phaseEncoder(pixels, config);
        std::vector<float> scratch(10'920U);
        QCOMPARE(phaseEncoder.pullFloat(scratch.data(), scratch.size()),
                 scratch.size());
        auto position = phaseEncoder.position();
        QCOMPARE(position.image.region, SstvScottieRegion::Porch);
        const std::size_t porchSamples = static_cast<std::size_t>(
            position.image.segmentEndSample);
        scratch.resize(porchSamples);
        QCOMPARE(phaseEncoder.pullFloat(scratch.data(), scratch.size()),
                 scratch.size());

        position = phaseEncoder.position();
        QCOMPARE(position.image.region, SstvScottieRegion::Pixel);
        QCOMPARE(position.image.component, ColourComponent::Green);
        QCOMPARE(position.image.pixel, std::uint32_t {0U});
        const std::uint64_t imageProduced = phaseEncoder.producedSamples()
            - 10'920U;
        const std::size_t firstPixelSamples = static_cast<std::size_t>(
            position.image.segmentEndSample - imageProduced);
        scratch.resize(firstPixelSamples);
        QCOMPARE(phaseEncoder.pullFloat(scratch.data(), scratch.size()),
                 scratch.size());
        const double phaseBeforeNextPixel = phaseEncoder.phaseTurns();
        float firstNextPixelSample = 0.0F;
        QCOMPARE(phaseEncoder.pullFloat(&firstNextPixelSample, 1U),
                 std::size_t {1U});
        const double expected = kDefaultSstvTxHeadroom
            * std::sin(static_cast<double>(
                static_cast<long double>(phaseBeforeNextPixel) * kTwoPi));
        QVERIFY(std::abs(static_cast<double>(firstNextPixelSample) - expected)
                < 1.0e-6);
        QVERIFY(std::abs(firstNextPixelSample) > 1.0e-4F);

        const auto stoppedAt = phaseEncoder.producedSamples();
        phaseEncoder.cancel();
        scratch.resize(127U);
        QCOMPARE(phaseEncoder.pullFloat(scratch.data(), scratch.size()),
                 std::size_t {0U});
        QCOMPARE(phaseEncoder.producedSamples(), stoppedAt);
        QVERIFY(phaseEncoder.cancelled());
        QVERIFY(!phaseEncoder.complete());
        phaseEncoder.reset();
        QCOMPARE(phaseEncoder.producedSamples(), std::uint64_t {0U});
        QCOMPARE(phaseEncoder.phaseTurns(), 0.0);
        QCOMPARE(phaseEncoder.pullFloat(scratch.data(), scratch.size()),
                 scratch.size());
    }

    void decoderMapsPreSyncGreenBlueAndPublishesProgressively()
    {
        constexpr std::uint32_t sampleRate = 12'000U;
        constexpr std::int32_t ppm = 300;
        constexpr double offset = 100.0;
        constexpr std::uint64_t base = 80'000U;

        for (const auto mode : kModes) {
            const auto pixels = testImage(mode);
            const auto spec = SstvScottieProtocol::spec(mode);
            SstvScottieDecoderConfig config;
            config.mode = mode;
            config.sampleRate = sampleRate;
            config.clockErrorPpm = ppm;
            config.frequencyOffsetHz = offset;
            config.maximumPendingDirtyEvents = 8U;
            SstvScottieDecoder decoder(config);
            const std::array<SstvScottieLineSync, 2U> anchors {{
                lineSync(spec, 0U, base, sampleRate, ppm, false),
                lineSync(spec, 1U, base, sampleRate, ppm, true),
            }};
            QCOMPARE(decoder.consumeLineSyncs(anchors.data(), anchors.size()),
                     anchors.size());

            const auto line0 = observationsForLine(
                pixels, spec, 0U, base, sampleRate, ppm, offset);
            const auto line1 = observationsForLine(
                pixels, spec, 1U, base, sampleRate, ppm, offset);
            consumeRandomChunks(decoder, line0);
            QCOMPARE(decoder.snapshot().completedPixels, std::size_t {0U});
            QCOMPARE(decoder.consume(line1.data(), 1U), std::size_t {1U});

            const auto progressive = decoder.snapshot();
            QVERIFY(progressive.isScanlineComplete(0U));
            QCOMPARE(progressive.completedPixels,
                     std::size_t {SstvScottieProtocol::Width});
            const auto dirty = decoder.takeDirtyEvents();
            QVERIFY(!dirty.empty());
            QVERIFY(dirty.size() <= 8U);

            decoder.consume(line1.data() + 1U, line1.size() - 1U);
            QCOMPARE(decoder.finish(), SstvScottieDecodeState::Partial);
            const auto snapshot = decoder.snapshot();
            QVERIFY(snapshot.isScanlineComplete(0U));
            QVERIFY(snapshot.isScanlineComplete(1U));
            for (std::uint32_t line = 0U; line < 2U; ++line) {
                for (std::uint32_t x = 0U; x < spec.width; ++x) {
                    QVERIFY(snapshot.pixel(x, line)
                            == pixels[static_cast<std::size_t>(line)
                                          * spec.width
                                      + x]);
                }
            }
            QCOMPARE(decoder.metrics().predictedSyncs, std::uint64_t {1U});
            QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                    <= SstvScottieDecoder::MaximumBufferedPixelAccumulators);
        }
    }

    void encoderCancellationIsSafeFromAWatchdogThread()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvScottieEncoder::PixelCount, {23U, 67U, 149U});
        SstvScottieEncoderConfig config;
        config.mode = SstvScottieMode::DX;
        SstvScottieEncoder encoder(pixels, config);
        std::atomic_uint32_t completedPulls {0U};
        std::atomic_bool workerDone {false};

        std::thread worker([&] {
            std::vector<float> chunk(1'024U);
            while (!encoder.cancelled() && !encoder.complete()) {
                const std::size_t produced = encoder.pullFloat(
                    chunk.data(), chunk.size());
                completedPulls.fetch_add(1U, std::memory_order_release);
                if (produced == 0U) {
                    break;
                }
            }
            workerDone.store(true, std::memory_order_release);
        });

        while (completedPulls.load(std::memory_order_acquire) < 4U
               && !workerDone.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        encoder.cancel();
        worker.join();

        QVERIFY(encoder.cancelled());
        QVERIFY(workerDone.load(std::memory_order_acquire));
        QVERIFY(completedPulls.load(std::memory_order_acquire) > 0U);
        QVERIFY(encoder.producedSamples() < encoder.totalSamples());
    }

    void encoderStreamsAllLogicalDurationsWithoutWaveformStorage()
    {
        for (const auto mode : kModes) {
            const auto spec = SstvScottieProtocol::spec(mode);
            const std::vector<SstvRgbPixel> pixels(
                SstvScottieEncoder::pixelCount(mode), {41U, 127U, 233U});
            SstvScottieEncoderConfig config;
            config.mode = mode;
            SstvScottieEncoder encoder(pixels, config);
            std::vector<float> floats(4'093U);
            std::vector<std::int16_t> pcm(4'093U);
            std::uint64_t streamed = 0U;
            while (!encoder.complete()) {
                const std::size_t produced = mode == SstvScottieMode::S2
                    ? encoder.pullPcm16(pcm.data(), pcm.size())
                    : encoder.pullFloat(floats.data(), floats.size());
                QVERIFY(produced > 0U);
                streamed += produced;
            }
            QCOMPARE(streamed, encoder.totalSamples());
            QCOMPARE(encoder.producedSamples(), encoder.totalSamples());
            QCOMPARE(encoder.position().stage,
                     SstvScottieEncoderStage::Complete);
            QCOMPARE(encoder.totalSamples(),
                     std::uint64_t {10'920U}
                         + oracleSamples(unsignedDuration(spec.imageDuration),
                                         12'000U,
                                         0));
            QCOMPARE(encoder.metrics().tone.samplesGenerated,
                     encoder.totalSamples());
            QCOMPARE(encoder.metrics().residentImageBytes,
                     SstvScottieEncoder::pixelCount(mode)
                         * sizeof(SstvRgbPixel));
        }
    }

    void decoderIsChunkInvariantAndPreservesPartialRows()
    {
        const auto pixels = testImage(SstvScottieMode::S1);
        const auto spec = SstvScottieProtocol::spec(SstvScottieMode::S1);
        constexpr std::uint64_t base = 60'000U;
        const auto sync = lineSync(spec, 0U, base, 12'000U, -300, false);
        const auto input = observationsForLine(
            pixels, spec, 0U, base, 12'000U, -300, -100.0);

        SstvScottieDecoderConfig config;
        config.mode = SstvScottieMode::S1;
        config.clockErrorPpm = -300;
        config.frequencyOffsetHz = -100.0;
        SstvScottieDecoder contiguous(config);
        SstvScottieDecoder fragmented(config);
        QCOMPARE(contiguous.consumeLineSyncs(&sync, 1U), std::size_t {1U});
        QCOMPARE(fragmented.consumeLineSyncs(&sync, 1U), std::size_t {1U});
        QCOMPARE(contiguous.consume(input), input.size());
        consumeRandomChunks(fragmented, input);
        QCOMPARE(contiguous.finish(), SstvScottieDecodeState::Partial);
        QCOMPARE(fragmented.finish(), SstvScottieDecodeState::Partial);
        const auto first = contiguous.snapshot();
        const auto second = fragmented.snapshot();
        QVERIFY(first.pixels == second.pixels);
        QVERIFY(first.channelCoverage == second.channelCoverage);
        QCOMPARE(first.completedPixels, second.completedPixels);
        QVERIFY(first.isScanlineComplete(0U));

        SstvScottieDecoder truncated(config);
        QCOMPARE(truncated.consumeLineSyncs(&sync, 1U), std::size_t {1U});
        QCOMPARE(truncated.consume(input.data(), 160U), std::size_t {160U});
        QCOMPARE(truncated.finish(), SstvScottieDecodeState::Partial);
        const auto partial = truncated.snapshot();
        QCOMPARE(partial.coveredComponents, std::size_t {160U});
        QCOMPARE(partial.completedPixels, std::size_t {0U});
        QVERIFY(!partial.isComplete());
    }

    void selfGeneratedFrequencyFixtureCompletesButIsNotInteropProof()
    {
        // This exercises every destination row and component.  The frequency
        // observations are generated in this test, so success is only a
        // Decodium implementation/self-roundtrip-style invariant.  It must
        // never be reported as an independent fixture or interoperability.
        constexpr std::uint64_t base = 100'000U;
        for (const auto mode : kModes) {
            const auto spec = SstvScottieProtocol::spec(mode);
            const auto pixels = testImage(mode);
            SstvScottieDecoderConfig config;
            config.mode = mode;
            SstvScottieDecoder decoder(config);
            for (std::uint32_t line = 0U; line < spec.height; ++line) {
                const auto sync = lineSync(
                    spec, line, base, 12'000U, 0, (line & 7U) == 3U);
                QCOMPARE(decoder.consumeLineSyncs(&sync, 1U),
                         std::size_t {1U});
                const auto observations = observationsForLine(
                    pixels, spec, line, base, 12'000U, 0, 0.0);
                QCOMPARE(decoder.consume(observations), observations.size());
            }
            QCOMPARE(decoder.finish(), SstvScottieDecodeState::Complete);
            const auto snapshot = decoder.snapshot();
            QVERIFY(snapshot.isComplete());
            QCOMPARE(snapshot.completedPixels,
                     SstvScottieEncoder::pixelCount(mode));
            QVERIFY(snapshot.pixel(0U, 0U) == testPixel(0U, 0U));
            QVERIFY(snapshot.pixel(159U, 127U)
                    == testPixel(159U, 127U));
            QVERIFY(snapshot.pixel(319U, spec.height - 1U)
                    == testPixel(319U, spec.height - 1U));
            QCOMPARE(decoder.metrics().linesPublished,
                     std::uint64_t {spec.height});
            QCOMPARE(decoder.metrics().componentsPublished,
                     static_cast<std::uint64_t>(
                        SstvScottieEncoder::pixelCount(mode) * 3U));
        }
    }

    void hostileBoundsAreTransactionalAndLifecycleIsExplicit()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvScottieEncoder::PixelCount, {1U, 2U, 3U});
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvScottieEncoder(nullptr, SstvScottieEncoder::PixelCount));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvScottieEncoder(pixels.data(), pixels.size() - 1U));

        SstvScottieEncoderConfig badTx;
        badTx.mode = static_cast<SstvScottieMode>(255U);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieEncoder(pixels, badTx));
        badTx = {};
        badTx.sampleRate = 7'999U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieEncoder(pixels, badTx));
        badTx = {};
        badTx.clockErrorPpm = 100'001;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieEncoder(pixels, badTx));
        badTx = {};
        badTx.level = std::numeric_limits<double>::quiet_NaN();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieEncoder(pixels, badTx));

        SstvScottieEncoder encoder(pixels);
        const auto txBefore = encoder.producedSamples();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 encoder.pullFloat(nullptr, 1U));
        float sample = 0.0F;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullFloat(
                &sample, SstvScottieEncoder::MaximumSamplesPerPull + 1U));
        QCOMPARE(encoder.producedSamples(), txBefore);
        QCOMPARE(encoder.pullFloat(nullptr, 0U), std::size_t {0U});
        QCOMPARE(encoder.metrics().rejectedInputCalls, std::uint64_t {2U});

        SstvScottieMapper mapper;
        QCOMPARE(mapper.positionAtElapsedTime(Picoseconds {-1}).region,
                 SstvScottieRegion::Outside);
        QCOMPARE(mapper.positionAtElapsedTime(Picoseconds {
                     std::numeric_limits<std::int64_t>::max()}).region,
                 SstvScottieRegion::Complete);
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 mapper.lineStartSample(257U));
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 mapper.embeddedSyncStartSample(256U));

        SstvScottieDecoder decoder;
        const auto rxBefore = decoder.snapshot().revision;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 decoder.consume(nullptr, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            decoder.consume(
                nullptr,
                SstvScottieDecoder::MaximumObservationsPerConsume + 1U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 decoder.consumeLineSyncs(nullptr, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            decoder.consumeLineSyncs(
                nullptr,
                SstvScottieDecoder::MaximumSyncsPerConsume + 1U));
        QCOMPARE(decoder.snapshot().revision, rxBefore);
        QCOMPARE(decoder.metrics().rejectedInputCalls, std::uint64_t {4U});

        const auto spec = SstvScottieProtocol::spec(SstvScottieMode::S1);
        const auto validSync = lineSync(spec, 0U, 50'000U, 12'000U, 0);
        QCOMPARE(decoder.consumeLineSyncs(&validSync, 1U), std::size_t {1U});
        QCOMPARE(decoder.consumeLineSyncs(&validSync, 1U), std::size_t {0U});
        const SstvScottieLineSync badLine {
            256U, validSync.syncStartSample, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&badLine, 1U), std::size_t {0U});
        const SstvScottieLineSync overflowSync {
            1U, std::numeric_limits<std::uint64_t>::max(), 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&overflowSync, 1U),
                 std::size_t {0U});

        auto invalidObservation = frequencyObservation(
            validSync.syncStartSample, 1'900.0);
        invalidObservation.confidence =
            std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(decoder.consume(&invalidObservation, 1U), std::size_t {0U});
        invalidObservation = frequencyObservation(
            validSync.syncStartSample, 9'000.0);
        QCOMPARE(decoder.consume(&invalidObservation, 1U), std::size_t {0U});

        QCOMPARE(decoder.finish(), SstvScottieDecodeState::Partial);
        QCOMPARE(decoder.consume(&invalidObservation, 1U), std::size_t {0U});
        QVERIFY(decoder.metrics().droppedObservationsAfterEnd > 0U);
        decoder.reset();
        QCOMPARE(decoder.state(), SstvScottieDecodeState::Receiving);
        QCOMPARE(decoder.snapshot().completedPixels, std::size_t {0U});
        decoder.cancel();
        QCOMPARE(decoder.state(), SstvScottieDecodeState::Cancelled);
        QVERIFY(decoder.snapshot().cancelled);

        SstvScottieDecoderConfig badRx;
        badRx.frequencyOffsetHz =
            std::numeric_limits<double>::infinity();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieDecoder {badRx});
        badRx = {};
        badRx.minimumObservationConfidence = 1.1;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieDecoder {badRx});
        badRx = {};
        badRx.maximumPendingDirtyEvents = 0U;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvScottieDecoder {badRx});
    }
};

// Qt 6.11's QTEST_APPLESS_MAIN invokes a variadic wrapper with an omitted
// argument, which Clang diagnoses under strict C++17 -Wpedantic.  Passing an
// explicit no-op retains the same app-less runner without a warning waiver.
QTEST_MAIN_WRAPPER(TestSstvScottie, static_cast<void>(0);)
#include "test_sstv_scottie.moc"
