// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/analog/SstvMartinM1.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

constexpr std::uint64_t kLinePicoseconds = 446'446'000'000ULL;
constexpr std::array<std::uint64_t, 3> kComponentStarts {{
    5'434'000'000ULL,
    152'438'000'000ULL,
    299'442'000'000ULL,
}};
constexpr std::uint64_t kPixelPicoseconds = 457'600'000ULL;

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

ColourComponent componentForIndex(std::size_t index)
{
    switch (index) {
    case 0U:
        return ColourComponent::Green;
    case 1U:
        return ColourComponent::Blue;
    case 2U:
        return ColourComponent::Red;
    }
    throw std::logic_error("invalid test component");
}

std::uint8_t valueFor(const SstvRgbPixel& pixel,
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

std::vector<SstvRgbPixel> testImage()
{
    std::vector<SstvRgbPixel> pixels(SstvMartinM1Encoder::PixelCount);
    for (std::uint32_t y = 0U; y < SstvMartinM1Protocol::Height; ++y) {
        for (std::uint32_t x = 0U; x < SstvMartinM1Protocol::Width; ++x) {
            pixels[static_cast<std::size_t>(y)
                       * SstvMartinM1Protocol::Width
                   + x] = {
                static_cast<std::uint8_t>((x * 17U + y * 3U) & 0xffU),
                static_cast<std::uint8_t>((x < 160U ? 32U : 224U)
                                          ^ (y & 0x0fU)),
                static_cast<std::uint8_t>((255U - x + y) & 0xffU)};
        }
    }
    return pixels;
}

SstvFrequencyObservation frequencyObservation(std::uint64_t centre,
                                              double frequency,
                                              double confidence = 1.0)
{
    SstvFrequencyObservation observation;
    observation.status = SstvFrequencyStatus::Valid;
    observation.centreSample = centre;
    observation.correctedFrequencyHz = frequency;
    observation.rawFrequencyHz = frequency;
    observation.confidence = confidence;
    observation.validSampleFraction = 1.0;
    return observation;
}

std::vector<SstvFrequencyObservation> observationsForLine(
    const std::vector<SstvRgbPixel>& pixels,
    std::uint32_t line,
    std::uint64_t absoluteSyncStart,
    std::uint32_t sampleRate,
    std::int32_t ppm,
    double frequencyOffset)
{
    const std::uint64_t lineTime = line * kLinePicoseconds;
    const std::uint64_t lineSample = oracleSamples(
        lineTime, sampleRate, ppm);
    std::vector<SstvFrequencyObservation> result;
    result.reserve(SstvMartinM1Protocol::Width * 3U);

    for (std::size_t componentIndex = 0U;
         componentIndex < kComponentStarts.size();
         ++componentIndex) {
        const auto component = componentForIndex(componentIndex);
        for (std::uint32_t x = 0U;
             x < SstvMartinM1Protocol::Width;
             ++x) {
            const std::uint64_t startTime = lineTime
                + kComponentStarts[componentIndex]
                + x * kPixelPicoseconds;
            const std::uint64_t endTime = startTime + kPixelPicoseconds;
            const std::uint64_t startSample = oracleSamples(
                startTime, sampleRate, ppm);
            const std::uint64_t endSample = oracleSamples(
                endTime, sampleRate, ppm);
            if (endSample <= startSample) {
                throw std::logic_error(
                    "the independent oracle produced an empty pixel");
            }
            const std::uint64_t localCentre =
                startSample + (endSample - startSample) / 2U - lineSample;
            const auto& pixel = pixels[static_cast<std::size_t>(line)
                                           * SstvMartinM1Protocol::Width
                                       + x];
            result.push_back(frequencyObservation(
                absoluteSyncStart + localCentre,
                1'500.0 + 800.0 * valueFor(pixel, component) / 255.0
                    + frequencyOffset));
        }
    }
    return result;
}

std::vector<float> renderPrefix(SstvMartinM1Encoder& encoder,
                                std::size_t count,
                                bool randomChunks)
{
    std::vector<float> result(count);
    std::uint32_t random = 0x12345678U;
    std::size_t offset = 0U;
    while (offset < result.size()) {
        random = random * 1'664'525U + 1'013'904'223U;
        const std::size_t requested = randomChunks
            ? 1U + random % 997U
            : result.size() - offset;
        const std::size_t amount = std::min(requested,
                                            result.size() - offset);
        const std::size_t produced = encoder.pullFloat(
            result.data() + offset, amount);
        if (produced == 0U) {
            throw std::runtime_error("test encoder made no progress");
        }
        offset += produced;
    }
    return result;
}

void consumeRandomChunks(SstvMartinM1Decoder& decoder,
                         const std::vector<SstvFrequencyObservation>& input)
{
    std::uint32_t random = 0xa5a55a5aU;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        random = random * 1'103'515'245U + 12'345U;
        const std::size_t count = std::min<std::size_t>(
            1U + random % 47U, input.size() - offset);
        decoder.consume(input.data() + offset, count);
        offset += count;
    }
}

} // namespace

class TestSstvMartinM1 final : public QObject
{
    Q_OBJECT

private slots:
    void protocolConstantsAndIndependentTimeBoundaries()
    {
        QCOMPARE(SstvMartinM1Protocol::Width, std::uint32_t {320U});
        QCOMPARE(SstvMartinM1Protocol::Height, std::uint32_t {256U});
        QCOMPARE(SstvMartinM1Protocol::VisCode, std::uint8_t {44U});
        QCOMPARE(SstvMartinM1Protocol::SyncDuration.count,
                 std::int64_t {4'862'000'000LL});
        QCOMPARE(SstvMartinM1Protocol::PorchDuration.count,
                 std::int64_t {572'000'000LL});
        QCOMPARE(SstvMartinM1Protocol::PixelDuration.count,
                 std::int64_t {457'600'000LL});
        QCOMPARE(SstvMartinM1Protocol::ComponentDuration.count,
                 std::int64_t {146'432'000'000LL});
        QCOMPARE(SstvMartinM1Protocol::LineDuration.count,
                 std::int64_t {446'446'000'000LL});
        QCOMPARE(SstvMartinM1Protocol::ImageDuration.count,
                 std::int64_t {114'290'176'000'000LL});

        SstvMartinM1Mapper mapper;
        QCOMPARE(mapper.imageSampleCount(), std::uint64_t {1'371'482U});

        struct Boundary final
        {
            std::int64_t time;
            SstvMartinM1Region region;
            ColourComponent component;
            std::uint32_t pixel;
        };
        const std::array<Boundary, 10> boundaries {{
            {0LL, SstvMartinM1Region::Sync,
             ColourComponent::ModeSpecific, 0U},
            {4'862'000'000LL, SstvMartinM1Region::Porch,
             ColourComponent::ModeSpecific, 0U},
            {5'434'000'000LL, SstvMartinM1Region::Pixel,
             ColourComponent::Green, 0U},
            {151'866'000'000LL, SstvMartinM1Region::Separator,
             ColourComponent::ModeSpecific, 0U},
            {152'438'000'000LL, SstvMartinM1Region::Pixel,
             ColourComponent::Blue, 0U},
            {298'870'000'000LL, SstvMartinM1Region::Separator,
             ColourComponent::ModeSpecific, 0U},
            {299'442'000'000LL, SstvMartinM1Region::Pixel,
             ColourComponent::Red, 0U},
            {445'874'000'000LL, SstvMartinM1Region::Separator,
             ColourComponent::ModeSpecific, 0U},
            {446'446'000'000LL, SstvMartinM1Region::Sync,
             ColourComponent::ModeSpecific, 0U},
            {114'290'176'000'000LL, SstvMartinM1Region::Complete,
             ColourComponent::ModeSpecific, 0U},
        }};
        for (const auto& boundary : boundaries) {
            const auto position = mapper.positionAtElapsedTime(
                Picoseconds {boundary.time});
            QCOMPARE(position.region, boundary.region);
            if (boundary.region == SstvMartinM1Region::Pixel) {
                QCOMPARE(position.component, boundary.component);
                QCOMPARE(position.pixel, boundary.pixel);
            }
        }

        const auto lastGreen = mapper.positionAtElapsedTime(
            Picoseconds {151'866'000'000LL - 1LL});
        QCOMPARE(lastGreen.region, SstvMartinM1Region::Pixel);
        QCOMPARE(lastGreen.component, ColourComponent::Green);
        QCOMPARE(lastGreen.pixel, std::uint32_t {319U});
    }

    void sampleMappingCarriesFractionAndExplicitClockError()
    {
        SstvMartinM1Mapper nominal;
        QCOMPARE(nominal.lineStartSample(0U), std::uint64_t {0U});
        QCOMPARE(nominal.lineEndSample(0U), std::uint64_t {5'357U});
        QCOMPARE(nominal.lineStartSample(256U),
                 std::uint64_t {1'371'482U});

        std::size_t fiveSamplePixels = 0U;
        std::size_t sixSamplePixels = 0U;
        std::uint64_t sample = nominal.lineStartSample(0U);
        while (sample < nominal.lineEndSample(0U)) {
            const auto position = nominal.positionAtSample(sample);
            if (position.region == SstvMartinM1Region::Pixel
                && position.component == ColourComponent::Green) {
                const auto length = position.segmentEndSample
                    - position.segmentStartSample;
                fiveSamplePixels += length == 5U;
                sixSamplePixels += length == 6U;
                sample = position.segmentEndSample;
            } else {
                sample = position.segmentEndSample;
            }
        }
        QCOMPARE(fiveSamplePixels + sixSamplePixels,
                 std::size_t {320U});
        QVERIFY(fiveSamplePixels > 0U);
        QVERIFY(sixSamplePixels > 0U);

        SstvMartinM1Mapper slow({12'000U, 300});
        SstvMartinM1Mapper fast({12'000U, -300});
        QCOMPARE(slow.imageSampleCount(), std::uint64_t {1'371'893U});
        QCOMPARE(fast.imageSampleCount(), std::uint64_t {1'371'070U});
        const auto slowGreen = slow.positionAtElapsedTime(
            Picoseconds {5'435'630'200LL});
        QCOMPARE(slowGreen.region, SstvMartinM1Region::Pixel);
        QCOMPARE(slowGreen.component, ColourComponent::Green);
        QCOMPARE(slowGreen.pixel, std::uint32_t {0U});

        SstvMartinM1Mapper beyondCommon({12'000U, 1'500});
        QVERIFY(beyondCommon.imageSampleCount() > slow.imageSampleCount());

        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount, {0U, 0U, 0U});
        SstvMartinM1EncoderConfig slowTxConfig;
        slowTxConfig.clockErrorPpm = 300;
        SstvMartinM1Encoder slowTx(pixels, slowTxConfig);
        QCOMPARE(slowTx.totalSamples(), std::uint64_t {1'382'813U});
        SstvMartinM1EncoderConfig fastTxConfig;
        fastTxConfig.clockErrorPpm = -300;
        SstvMartinM1Encoder fastTx(pixels, fastTxConfig);
        QCOMPARE(fastTx.totalSamples(), std::uint64_t {1'381'990U});
    }

    void standardHeaderUsesCodecPlanAndExactDurations()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount, {0U, 0U, 0U});
        SstvMartinM1Encoder encoder(pixels);
        const std::array<double, 13> expectedFrequencies {{
            1'900.0, 1'200.0, 1'900.0, 1'200.0,
            1'300.0, 1'300.0, 1'100.0, 1'100.0,
            1'300.0, 1'100.0, 1'300.0, 1'100.0, 1'200.0,
        }};
        const std::array<std::size_t, 13> expectedSamples {{
            3'600U, 120U, 3'600U,
            360U, 360U, 360U, 360U, 360U,
            360U, 360U, 360U, 360U, 360U,
        }};

        std::vector<float> output(3'600U);
        for (std::size_t index = 0U;
             index < expectedFrequencies.size();
             ++index) {
            const auto position = encoder.position();
            QCOMPARE(position.stage, SstvMartinM1EncoderStage::Header);
            QCOMPARE(position.headerSegment, index);
            QVERIFY(std::abs(position.frequencyHz
                             - expectedFrequencies[index]) < 1.0e-12);
            QCOMPARE(encoder.pullFloat(output.data(), expectedSamples[index]),
                     expectedSamples[index]);
            for (std::size_t sample = 0U;
                 sample < expectedSamples[index];
                 ++sample) {
                QVERIFY(std::isfinite(output[sample]));
            }
        }
        const auto imageStart = encoder.position();
        QCOMPARE(imageStart.stage, SstvMartinM1EncoderStage::Image);
        QCOMPARE(imageStart.image.region, SstvMartinM1Region::Sync);
        QCOMPARE(imageStart.image.line, std::uint32_t {0U});
        QCOMPARE(encoder.producedSamples(), std::uint64_t {10'920U});
    }

    void encoderIsChunkInvariantAcrossPixelBoundaries()
    {
        const auto pixels = testImage();
        SstvMartinM1Encoder contiguous(pixels);
        SstvMartinM1Encoder fragmented(pixels);
        const auto first = renderPrefix(contiguous, 100'000U, false);
        const auto second = renderPrefix(fragmented, 100'000U, true);
        QVERIFY(first == second);
        QCOMPARE(contiguous.producedSamples(), fragmented.producedSamples());
        QCOMPARE(contiguous.position().stage,
                 SstvMartinM1EncoderStage::Image);
    }

    void encoderIsBoundedCancellableAndResettable()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount, {17U, 34U, 51U});
        SstvMartinM1Encoder encoder(pixels);
        QCOMPARE(encoder.totalSamples(), std::uint64_t {1'382'402U});
        QCOMPARE(encoder.metrics().residentImageBytes,
                 SstvMartinM1Encoder::PixelCount * sizeof(SstvRgbPixel));

        std::vector<float> chunk(SstvMartinM1Encoder::MaximumSamplesPerPull);
        double peak = 0.0;
        while (!encoder.complete()) {
            const std::size_t produced = encoder.pullFloat(
                chunk.data(), chunk.size());
            QVERIFY(produced > 0U);
            for (std::size_t index = 0U; index < produced; ++index) {
                QVERIFY(std::isfinite(chunk[index]));
                peak = std::max(peak,
                                std::abs(static_cast<double>(chunk[index])));
            }
        }
        QCOMPARE(encoder.producedSamples(), encoder.totalSamples());
        QVERIFY(peak <= kDefaultSstvTxHeadroom + 1.0e-6);
        QVERIFY(encoder.metrics().segmentTransitions > 245'000U);

        encoder.reset();
        QCOMPARE(encoder.producedSamples(), std::uint64_t {0U});
        QVERIFY(!encoder.cancelled());
        QCOMPARE(encoder.position().stage,
                 SstvMartinM1EncoderStage::Header);
        QCOMPARE(encoder.pullFloat(chunk.data(), 127U), std::size_t {127U});
        encoder.cancel();
        const auto stoppedAt = encoder.producedSamples();
        QCOMPARE(encoder.pullFloat(chunk.data(), chunk.size()),
                 std::size_t {0U});
        QCOMPARE(encoder.producedSamples(), stoppedAt);
        QVERIFY(encoder.cancelled());
        QVERIFY(!encoder.complete());
        encoder.reset();
        QCOMPARE(encoder.pullFloat(chunk.data(), 127U), std::size_t {127U});
    }

    void publicInputBoundsAreTransactional()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount, {0U, 0U, 0U});
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMartinM1Encoder(nullptr,
                                SstvMartinM1Encoder::PixelCount));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMartinM1Encoder(pixels.data(), pixels.size() - 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMartinM1Mapper({7'999U, 0}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvMartinM1Mapper({12'000U, 100'001}));
        SstvMartinM1EncoderConfig nonFiniteLevel;
        nonFiniteLevel.level =
            std::numeric_limits<double>::quiet_NaN();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMartinM1Encoder(pixels,
                                                     nonFiniteLevel));
        nonFiniteLevel.level = SstvMartinM1Encoder::MaximumLevel + 0.01;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMartinM1Encoder(pixels,
                                                     nonFiniteLevel));

        SstvMartinM1Encoder encoder(pixels);
        const auto before = encoder.position();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 encoder.pullFloat(nullptr, 1U));
        QCOMPARE(encoder.producedSamples(), before.producedSamples);
        float sample = 0.0F;
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            encoder.pullFloat(
                &sample,
                SstvMartinM1Encoder::MaximumSamplesPerPull + 1U));
        QCOMPARE(encoder.producedSamples(), before.producedSamples);
        QCOMPARE(encoder.pullFloat(nullptr, 0U), std::size_t {0U});
        QCOMPARE(encoder.metrics().rejectedInputCalls, std::uint64_t {2U});

        SstvMartinM1Mapper mapper;
        QCOMPARE(mapper.positionAtElapsedTime(
                     Picoseconds {-1LL}).region,
                 SstvMartinM1Region::Outside);
        QCOMPARE(mapper.positionAtElapsedTime(Picoseconds {
                     std::numeric_limits<std::int64_t>::max()}).region,
                 SstvMartinM1Region::Complete);
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 mapper.lineStartSample(257U));

        SstvMartinM1Decoder decoder;
        const auto rxBefore = decoder.snapshot().revision;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 decoder.consume(nullptr, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            decoder.consume(nullptr,
                            SstvMartinM1Decoder::MaximumObservationsPerConsume
                                + 1U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 decoder.consumeLineSyncs(nullptr, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            decoder.consumeLineSyncs(
                nullptr,
                SstvMartinM1Decoder::MaximumSyncsPerConsume + 1U));
        QCOMPARE(decoder.snapshot().revision, rxBefore);
        QCOMPARE(decoder.metrics().rejectedInputCalls, std::uint64_t {4U});

        auto bad = SstvMartinM1DecoderConfig {};
        bad.frequencyOffsetHz =
            std::numeric_limits<double>::quiet_NaN();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMartinM1Decoder {bad});
        bad = {};
        bad.clockErrorPpm = -100'001;
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 SstvMartinM1Decoder {bad});

        QCOMPARE(decoder.setFrequencyOffsetHz(125.0), 125.0);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 decoder.setFrequencyOffsetHz(500.1));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            decoder.setFrequencyOffsetHz(
                std::numeric_limits<double>::infinity()));
        QCOMPARE(decoder.frequencyOffsetHz(), 125.0);
    }

    void decoderHandlesOffsetSlantPredictionAndProgressiveDirty()
    {
        const auto pixels = testImage();
        constexpr std::uint32_t sampleRate = 12'000U;
        constexpr std::int32_t ppm = 300;
        constexpr double offset = 100.0;
        SstvMartinM1Mapper mapper({sampleRate, ppm});
        SstvMartinM1Decoder decoder(
            {sampleRate, ppm, offset, 0.2, 8U});
        const std::uint64_t base = 50'000U;
        const std::array<SstvMartinM1LineSync, 2> anchors {{
            {0U, base, 0.95, false},
            {1U, base + mapper.lineStartSample(1U), 0.60, true},
        }};
        QCOMPARE(decoder.consumeLineSyncs(anchors.data(), anchors.size()),
                 anchors.size());

        const auto line0 = observationsForLine(
            pixels, 0U, anchors[0].syncStartSample,
            sampleRate, ppm, offset);
        const auto line1 = observationsForLine(
            pixels, 1U, anchors[1].syncStartSample,
            sampleRate, ppm, offset);
        consumeRandomChunks(decoder, line0);
        QCOMPARE(decoder.snapshot().completedPixels, std::size_t {0U});
        QCOMPARE(decoder.consume(line1.data(), 1U), std::size_t {1U});

        const auto progressive = decoder.snapshot();
        QVERIFY(progressive.isScanlineComplete(0U));
        QCOMPARE(progressive.completedPixels,
                 std::size_t {SstvMartinM1Protocol::Width});
        QVERIFY(!decoder.takeDirtyEvents().empty());
        consumeRandomChunks(
            decoder,
            std::vector<SstvFrequencyObservation>(line1.begin() + 1,
                                                  line1.end()));
        QCOMPARE(decoder.finish(), SstvMartinM1DecodeState::Partial);
        const auto snapshot = decoder.snapshot();
        QVERIFY(snapshot.isScanlineComplete(0U));
        QVERIFY(snapshot.isScanlineComplete(1U));
        for (std::uint32_t line = 0U; line < 2U; ++line) {
            for (std::uint32_t x = 0U;
                 x < SstvMartinM1Protocol::Width;
                 ++x) {
                const auto expected = pixels[static_cast<std::size_t>(line)
                                                 * SstvMartinM1Protocol::Width
                                             + x];
                QVERIFY(snapshot.pixel(x, line) == expected);
            }
        }
        QCOMPARE(decoder.metrics().predictedSyncs, std::uint64_t {1U});
        QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                <= SstvMartinM1Decoder::MaximumBufferedPixelAccumulators);
    }

    void decoderIsChunkInvariantAndPublishesTruncation()
    {
        const auto pixels = testImage();
        const auto input = observationsForLine(
            pixels, 0U, 10'000U, 12'000U, -300, -100.0);
        const SstvMartinM1LineSync sync {0U, 10'000U, 1.0, false};

        SstvMartinM1DecoderConfig config;
        config.clockErrorPpm = -300;
        config.frequencyOffsetHz = -100.0;
        SstvMartinM1Decoder contiguous(config);
        SstvMartinM1Decoder fragmented(config);
        QCOMPARE(contiguous.consumeLineSyncs(&sync, 1U), std::size_t {1U});
        QCOMPARE(fragmented.consumeLineSyncs(&sync, 1U), std::size_t {1U});
        QCOMPARE(contiguous.consume(input), input.size());
        consumeRandomChunks(fragmented, input);
        QCOMPARE(contiguous.finish(), SstvMartinM1DecodeState::Partial);
        QCOMPARE(fragmented.finish(), SstvMartinM1DecodeState::Partial);
        const auto first = contiguous.snapshot();
        const auto second = fragmented.snapshot();
        QVERIFY(first.pixels == second.pixels);
        QVERIFY(first.channelCoverage == second.channelCoverage);
        QCOMPARE(first.completedPixels, second.completedPixels);
        QVERIFY(first.isScanlineComplete(0U));

        SstvMartinM1Decoder truncated;
        QCOMPARE(truncated.consumeLineSyncs(&sync, 1U), std::size_t {1U});
        QCOMPARE(truncated.consume(input.data(), 160U), std::size_t {160U});
        QCOMPARE(truncated.finish(), SstvMartinM1DecodeState::Partial);
        const auto partial = truncated.snapshot();
        QCOMPARE(partial.coveredComponents, std::size_t {160U});
        QCOMPARE(partial.completedPixels, std::size_t {0U});
        QVERIFY(!partial.isComplete());
    }

    void encoderDecoderToneMappingLoopbackIsSelfTestOnly()
    {
        // This checks Decodium's own public TX mapping against its RX mapping.
        // It is deliberately not an interoperability or independent fixture.
        const auto pixels = testImage();
        SstvMartinM1Encoder encoder(pixels);
        std::vector<float> audio(10'920U);
        QCOMPARE(encoder.pullFloat(audio.data(), audio.size()), audio.size());

        std::vector<SstvFrequencyObservation> observations;
        observations.reserve(SstvMartinM1Protocol::Width * 3U);
        std::vector<float> scratch(16U);
        while (encoder.position().stage == SstvMartinM1EncoderStage::Image
               && encoder.position().image.line == 0U) {
            const auto position = encoder.position();
            if (position.image.region == SstvMartinM1Region::Pixel) {
                observations.push_back(frequencyObservation(
                    20'000U
                        + position.image.segmentStartSample
                        + (position.image.segmentEndSample
                           - position.image.segmentStartSample) / 2U,
                    position.frequencyHz));
            }
            const std::size_t count = static_cast<std::size_t>(
                position.image.segmentEndSample
                - (encoder.producedSamples() - 10'920U));
            if (scratch.size() < count) {
                scratch.resize(count);
            }
            QCOMPARE(encoder.pullFloat(scratch.data(), count), count);
        }
        QCOMPARE(observations.size(), std::size_t {960U});

        SstvMartinM1Decoder decoder;
        const SstvMartinM1LineSync anchor {0U, 20'000U, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&anchor, 1U), std::size_t {1U});
        consumeRandomChunks(decoder, observations);
        QCOMPARE(decoder.finish(), SstvMartinM1DecodeState::Partial);
        const auto snapshot = decoder.snapshot();
        QVERIFY(snapshot.isScanlineComplete(0U));
        for (std::uint32_t x = 0U;
             x < SstvMartinM1Protocol::Width;
             ++x) {
            QVERIFY(snapshot.pixel(x, 0U) == pixels[x]);
        }
    }

    void decoderAggregatesBoundedNoisyPixelObservations()
    {
        SstvMartinM1Mapper mapper;
        SstvMartinM1Decoder decoder;
        const SstvMartinM1LineSync sync {0U, 7'000U, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&sync, 1U), std::size_t {1U});

        const auto pixel = mapper.positionAtElapsedTime(
            Picoseconds {5'434'000'000LL});
        const std::uint64_t centre = sync.syncStartSample
            + pixel.segmentStartSample
            + (pixel.segmentEndSample - pixel.segmentStartSample) / 2U;
        const double target = 1'500.0 + 800.0 * 128.0 / 255.0;
        const std::array<SstvFrequencyObservation, 3> noisy {{
            frequencyObservation(centre, target - 60.0, 0.5),
            frequencyObservation(centre, target, 1.0),
            frequencyObservation(centre, target + 60.0, 0.8),
        }};
        QCOMPARE(decoder.consume(noisy.data(), noisy.size()), noisy.size());
        QCOMPARE(decoder.metrics().bufferedPixelAccumulators,
                 std::size_t {1U});
        QCOMPARE(decoder.finish(), SstvMartinM1DecodeState::Partial);
        const auto snapshot = decoder.snapshot();
        QCOMPARE(snapshot.pixel(0U, 0U).green, std::uint8_t {128U});
        QCOMPARE(snapshot.coverageMask(0U, 0U), std::uint8_t {2U});
        QCOMPARE(decoder.metrics().componentsPublished, std::uint64_t {1U});

        QCOMPARE(decoder.consume(noisy.data(), noisy.size()), std::size_t {0U});
        QCOMPARE(decoder.consumeLineSyncs(&sync, 1U), std::size_t {0U});
        QCOMPARE(decoder.metrics().droppedObservationsAfterEnd,
                 std::uint64_t {3U});
        QCOMPARE(decoder.metrics().droppedSyncsAfterEnd,
                 std::uint64_t {1U});
    }

    void completeConstantFrameUsesFixedMemory()
    {
        const std::vector<SstvRgbPixel> pixels(
            SstvMartinM1Encoder::PixelCount, {23U, 127U, 241U});
        SstvMartinM1Mapper mapper;
        SstvMartinM1Decoder decoder;
        const std::uint64_t base = 123'456U;
        for (std::uint32_t line = 0U;
             line < SstvMartinM1Protocol::Height;
             ++line) {
            const SstvMartinM1LineSync sync {
                line,
                base + mapper.lineStartSample(line),
                1.0,
                false};
            QCOMPARE(decoder.consumeLineSyncs(&sync, 1U), std::size_t {1U});
            const auto observations = observationsForLine(
                pixels,
                line,
                sync.syncStartSample,
                12'000U,
                0,
                0.0);
            const std::size_t accepted = decoder.consume(observations);
            const auto lineMetrics = decoder.metrics();
            QVERIFY2(accepted == observations.size(),
                     qPrintable(QStringLiteral("line %1 accepted %2 of %3; stale=%4 invalid=%5 unanchored=%6 numeric=%7")
                                    .arg(line)
                                    .arg(accepted)
                                    .arg(observations.size())
                                    .arg(lineMetrics.staleObservations)
                                    .arg(lineMetrics.invalidObservations)
                                    .arg(lineMetrics.unanchoredObservations)
                                    .arg(lineMetrics.numericFaults)));
            QVERIFY(decoder.metrics().bufferedPixelAccumulators
                    <= SstvMartinM1Decoder::MaximumBufferedPixelAccumulators);
            QVERIFY(decoder.metrics().storedSyncAnchors
                    <= SstvMartinM1Protocol::Height);
        }
        QCOMPARE(decoder.finish(), SstvMartinM1DecodeState::Complete);
        const auto snapshot = decoder.snapshot();
        QVERIFY(snapshot.isComplete());
        QCOMPARE(snapshot.completedPixels,
                 SstvMartinM1Encoder::PixelCount);
        QVERIFY((snapshot.pixel(0U, 0U)
                 == SstvRgbPixel {23U, 127U, 241U}));
        QVERIFY(snapshot.pixel(319U, 255U)
                == (SstvRgbPixel {23U, 127U, 241U}));
        QVERIFY(decoder.metrics().peakBufferedPixelAccumulators
                <= SstvMartinM1Decoder::MaximumBufferedPixelAccumulators);

        decoder.reset();
        QCOMPARE(decoder.state(), SstvMartinM1DecodeState::Receiving);
        QCOMPARE(decoder.snapshot().completedPixels, std::size_t {0U});
        QCOMPARE(decoder.metrics().storedSyncAnchors, std::size_t {0U});
        decoder.cancel();
        QCOMPARE(decoder.state(), SstvMartinM1DecodeState::Cancelled);
        QVERIFY(decoder.snapshot().cancelled);
    }

    void invalidObservationsNeverCreateNonFiniteState()
    {
        SstvMartinM1Decoder decoder;
        auto unanchored = frequencyObservation(1'000U, 1'900.0);
        QCOMPARE(decoder.consume(&unanchored, 1U), std::size_t {0U});

        const SstvMartinM1LineSync invalidSync {
            SstvMartinM1Protocol::Height, 2'000U, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&invalidSync, 1U),
                 std::size_t {0U});
        const SstvMartinM1LineSync sync {0U, 2'000U, 1.0, false};
        QCOMPARE(decoder.consumeLineSyncs(&sync, 1U), std::size_t {1U});

        SstvMartinM1Mapper mapper;
        const auto pixel = mapper.positionAtElapsedTime(
            Picoseconds {5'434'000'000LL});
        const std::uint64_t centre = 2'000U
            + pixel.segmentStartSample
            + (pixel.segmentEndSample - pixel.segmentStartSample) / 2U;
        std::array<SstvFrequencyObservation, 4> invalid {{
            frequencyObservation(
                centre, std::numeric_limits<double>::quiet_NaN()),
            frequencyObservation(centre + 1U, 1'900.0, 0.1),
            frequencyObservation(centre + 2U,
                                 std::numeric_limits<double>::infinity()),
            frequencyObservation(centre + 3U, 9'000.0),
        }};
        invalid[2].confidence =
            std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(decoder.consume(invalid.data(), invalid.size()),
                 std::size_t {0U});

        const auto valid = frequencyObservation(centre + 10U, 1'900.0);
        QCOMPARE(decoder.consume(&valid, 1U), std::size_t {1U});
        const auto stale = frequencyObservation(centre + 9U, 1'900.0);
        QCOMPARE(decoder.consume(&stale, 1U), std::size_t {0U});
        QCOMPARE(decoder.finish(), SstvMartinM1DecodeState::Partial);
        const auto snapshot = decoder.snapshot();
        QVERIFY(snapshot.coverage() >= 0.0);
        QVERIFY(snapshot.coverage() <= 1.0);
        QVERIFY(decoder.metrics().invalidObservations >= 4U);
        QVERIFY(decoder.metrics().unanchoredObservations >= 1U);
        QVERIFY(decoder.metrics().staleObservations >= 1U);
        QVERIFY(decoder.metrics().numericFaults == 0U);
    }
};

QTEST_APPLESS_MAIN(TestSstvMartinM1)
#include "test_sstv_martin_m1.moc"
