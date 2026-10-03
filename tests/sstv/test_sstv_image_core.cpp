// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/image/SstvColourConverter.h"
#include "../../src/sstv/image/SstvImageFrame.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace decodium::sstv;

namespace {

void compareRgb(SstvRgbPixel actual, SstvRgbPixel expected)
{
    QCOMPARE(actual.red, expected.red);
    QCOMPARE(actual.green, expected.green);
    QCOMPARE(actual.blue, expected.blue);
}

void compareYCbCr(SstvYCbCrPixel actual, SstvYCbCrPixel expected)
{
    QCOMPARE(actual.luminance, expected.luminance);
    QCOMPARE(actual.chrominanceBlue, expected.chrominanceBlue);
    QCOMPARE(actual.chrominanceRed, expected.chrominanceRed);
}

std::vector<SstvRgbPixel> solidScanline(std::size_t width, std::uint8_t value)
{
    return std::vector<SstvRgbPixel>(width, SstvRgbPixel {value, value, value});
}

} // namespace

class TestSstvImageCore final : public QObject
{
    Q_OBJECT

private slots:
    void bt601GoldenVectorsAreExact()
    {
        // These canonical full-range vectors are calculated independently from
        // the published BT.601/JPEG equations, not through an inverse call.
        compareYCbCr(SstvColourConverter::rgbToYCbCr({0U, 0U, 0U}),
                     {0U, 128U, 128U});
        compareYCbCr(SstvColourConverter::rgbToYCbCr({255U, 255U, 255U}),
                     {255U, 128U, 128U});
        compareYCbCr(SstvColourConverter::rgbToYCbCr({255U, 0U, 0U}),
                     {76U, 85U, 255U});
        compareYCbCr(SstvColourConverter::rgbToYCbCr({0U, 255U, 0U}),
                     {150U, 44U, 21U});
        compareYCbCr(SstvColourConverter::rgbToYCbCr({0U, 0U, 255U}),
                     {29U, 255U, 107U});

        compareRgb(SstvColourConverter::yCbCrToRgb({0U, 128U, 128U}),
                   {0U, 0U, 0U});
        compareRgb(SstvColourConverter::yCbCrToRgb({255U, 128U, 128U}),
                   {255U, 255U, 255U});
        compareRgb(SstvColourConverter::yCbCrToRgb({76U, 85U, 255U}),
                   {254U, 0U, 0U});
        compareRgb(SstvColourConverter::yCbCrToRgb({150U, 44U, 21U}),
                   {0U, 255U, 1U});
        compareRgb(SstvColourConverter::yCbCrToRgb({29U, 255U, 107U}),
                   {0U, 0U, 254U});
    }

    void colourRoundTripHasOnlyQuantizationError()
    {
        for (unsigned red = 0U; red <= 255U; red += 17U) {
            for (unsigned green = 0U; green <= 255U; green += 17U) {
                for (unsigned blue = 0U; blue <= 255U; blue += 17U) {
                    const SstvRgbPixel original {
                        static_cast<std::uint8_t>(red),
                        static_cast<std::uint8_t>(green),
                        static_cast<std::uint8_t>(blue)};
                    const auto restored = SstvColourConverter::yCbCrToRgb(
                        SstvColourConverter::rgbToYCbCr(original));
                    QVERIFY(std::abs(static_cast<int>(restored.red) - static_cast<int>(red)) <= 2);
                    QVERIFY(std::abs(static_cast<int>(restored.green) - static_cast<int>(green)) <= 2);
                    QVERIFY(std::abs(static_cast<int>(restored.blue) - static_cast<int>(blue)) <= 2);
                }
            }
        }
    }

    void grayscaleUsesDocumentedLumaAndNeutralExpansion()
    {
        QCOMPARE(SstvColourConverter::rgbToGrayscale({255U, 0U, 0U}),
                 std::uint8_t {76U});
        QCOMPARE(SstvColourConverter::rgbToGrayscale({0U, 255U, 0U}),
                 std::uint8_t {150U});
        QCOMPARE(SstvColourConverter::rgbToGrayscale({0U, 0U, 255U}),
                 std::uint8_t {29U});
        compareRgb(SstvColourConverter::grayscaleToRgb(173U),
                   {173U, 173U, 173U});
    }

    void inverseConversionClampsExtremeChroma()
    {
        compareRgb(SstvColourConverter::yCbCrToRgb({0U, 128U, 255U}),
                   {178U, 0U, 0U});
        compareRgb(SstvColourConverter::yCbCrToRgb({255U, 128U, 0U}),
                   {76U, 255U, 255U});
        compareRgb(SstvColourConverter::yCbCrToRgb({0U, 0U, 128U}),
                   {0U, 44U, 0U});
    }

    void progressiveChannelsTrackCoverageAndCompletion()
    {
        SstvImageFrame frame(3U, 1U);
        const std::vector<std::uint8_t> red {10U, 20U, 30U};
        const std::vector<std::uint8_t> green {40U, 50U, 60U};
        const std::vector<std::uint8_t> blue {70U, 80U, 90U};

        QCOMPARE(frame.writeChannelScanline(0U, SstvImageChannel::Red, red),
                 SstvImageWriteResult::Changed);
        QCOMPARE(frame.coveredComponents(), std::size_t {3U});
        QVERIFY(std::abs(frame.coverage() - 1.0 / 3.0) < 1.0e-12);
        QVERIFY(!frame.isScanlineComplete(0U));
        QVERIFY(!frame.isComplete());

        QCOMPARE(frame.writeChannelScanline(0U, SstvImageChannel::Green, green),
                 SstvImageWriteResult::Changed);
        QCOMPARE(frame.writeChannelScanline(0U, SstvImageChannel::Blue, blue),
                 SstvImageWriteResult::Changed);
        QCOMPARE(frame.coveredComponents(), std::size_t {9U});
        QCOMPARE(frame.completedPixels(), std::size_t {3U});
        QVERIFY(frame.isScanlineComplete(0U));
        QVERIFY(frame.isComplete());

        const auto snapshot = frame.snapshot();
        compareRgb(snapshot.pixel(1U, 0U), {20U, 50U, 80U});
        QCOMPARE(snapshot.coverageMask(1U, 0U), std::uint8_t {7U});
        QVERIFY(snapshot.isComplete());
        QCOMPARE(frame.writeChannelScanline(0U, SstvImageChannel::Blue, blue),
                 SstvImageWriteResult::Unchanged);
    }

    void completeAndIncompleteScanlinesRemainDistinct()
    {
        SstvImageFrame frame(2U, 2U);
        const std::vector<SstvRgbPixel> firstLine {
            {1U, 2U, 3U}, {4U, 5U, 6U}};
        QCOMPARE(frame.writeScanline(0U, firstLine), SstvImageWriteResult::Changed);
        QVERIFY(frame.isScanlineComplete(0U));
        QVERIFY(!frame.isScanlineComplete(1U));
        QVERIFY(!frame.isComplete());
        QCOMPARE(frame.completedPixels(), std::size_t {2U});
        QVERIFY(std::abs(frame.coverage() - 0.5) < 1.0e-12);

        QCOMPARE(frame.writeChannel(0U, 1U, SstvImageChannel::Grayscale, 99U),
                 SstvImageWriteResult::Changed);
        compareRgb(frame.snapshot().pixel(0U, 1U), {99U, 99U, 99U});
        QVERIFY(!frame.isScanlineComplete(1U));
        QCOMPARE(frame.completedPixels(), std::size_t {3U});
    }

    void callerControlledPassMappingIsPreserved()
    {
        SstvImageFrame frame(2U, 4U);
        // Incoming pass order is even lines then odd lines.  The final-row map
        // is explicit caller data; SstvImageFrame never infers this sequence.
        const std::array<std::uint32_t, 4> destinationRows {0U, 2U, 1U, 3U};
        for (std::size_t decodedLine = 0U;
             decodedLine < destinationRows.size();
             ++decodedLine) {
            const auto value = static_cast<std::uint8_t>(10U + decodedLine);
            QCOMPARE(frame.writeScanline(destinationRows[decodedLine],
                                         solidScanline(2U, value)),
                     SstvImageWriteResult::Changed);
        }

        const auto snapshot = frame.snapshot();
        QCOMPARE(snapshot.pixel(0U, 0U).red, std::uint8_t {10U});
        QCOMPARE(snapshot.pixel(0U, 1U).red, std::uint8_t {12U});
        QCOMPARE(snapshot.pixel(0U, 2U).red, std::uint8_t {11U});
        QCOMPARE(snapshot.pixel(0U, 3U).red, std::uint8_t {13U});
        QVERIFY(snapshot.isComplete());

        const auto events = frame.takeDirtyEvents();
        QCOMPARE(events.size(), std::size_t {4U});
        QCOMPARE(events[0].region.y, std::uint32_t {0U});
        QCOMPARE(events[1].region.y, std::uint32_t {2U});
        QCOMPARE(events[2].region.y, std::uint32_t {1U});
        QCOMPARE(events[3].region.y, std::uint32_t {3U});
    }

    void linearChunkBoundariesDoNotChangeFrame()
    {
        constexpr std::uint32_t width = 7U;
        constexpr std::uint32_t height = 5U;
        std::vector<SstvRgbPixel> pixels(width * height);
        for (std::size_t index = 0U; index < pixels.size(); ++index) {
            pixels[index] = {static_cast<std::uint8_t>((index * 3U) & 0xffU),
                             static_cast<std::uint8_t>((index * 5U) & 0xffU),
                             static_cast<std::uint8_t>((index * 7U) & 0xffU)};
        }

        SstvImageFrame oneChunk(width, height);
        QCOMPARE(oneChunk.writeLinearPixels(0U, pixels),
                 SstvImageWriteResult::Changed);

        SstvImageFrame manyChunks(width, height);
        const std::array<std::size_t, 5> chunkSizes {1U, 9U, 2U, 11U, 3U};
        std::size_t offset = 0U;
        std::size_t chunkIndex = 0U;
        while (offset < pixels.size()) {
            const std::size_t count = std::min(
                chunkSizes[chunkIndex++ % chunkSizes.size()], pixels.size() - offset);
            const std::vector<SstvRgbPixel> chunk(
                pixels.begin() + static_cast<std::ptrdiff_t>(offset),
                pixels.begin() + static_cast<std::ptrdiff_t>(offset + count));
            QCOMPARE(manyChunks.writeLinearPixels(offset, chunk),
                     SstvImageWriteResult::Changed);
            offset += count;
        }

        const auto one = oneChunk.snapshot();
        const auto many = manyChunks.snapshot();
        QVERIFY(one.pixels == many.pixels);
        QCOMPARE(one.channelCoverage, many.channelCoverage);
        QCOMPARE(one.coveredComponents, many.coveredComponents);
        QCOMPARE(one.completedPixels, many.completedPixels);
        QVERIFY(many.isComplete());
    }

    void dirtyEventsAreDeterministicAndBounded()
    {
        SstvImageFrame frame(4U, 4U, 2U);
        QCOMPARE(frame.writePixel(0U, 0U, {1U, 1U, 1U}),
                 SstvImageWriteResult::Changed);
        QCOMPARE(frame.writePixel(1U, 1U, {2U, 2U, 2U}),
                 SstvImageWriteResult::Changed);
        QCOMPARE(frame.writePixel(3U, 3U, {3U, 3U, 3U}),
                 SstvImageWriteResult::Changed);
        QCOMPARE(frame.pendingDirtyEvents(), std::size_t {2U});

        const auto events = frame.takeDirtyEvents();
        QCOMPARE(events.size(), std::size_t {2U});
        QVERIFY((events[0].region == SstvDirtyRegion {0U, 0U, 1U, 1U}));
        QCOMPARE(events[0].firstSequence, std::uint64_t {1U});
        QCOMPARE(events[0].lastSequence, std::uint64_t {1U});
        QVERIFY(!events[0].coalesced);
        QVERIFY((events[1].region == SstvDirtyRegion {1U, 1U, 3U, 3U}));
        QCOMPARE(events[1].firstSequence, std::uint64_t {2U});
        QCOMPARE(events[1].lastSequence, std::uint64_t {3U});
        QCOMPARE(events[1].operationCount, std::uint64_t {2U});
        QVERIFY(events[1].coalesced);
        QCOMPARE(frame.pendingDirtyEvents(), std::size_t {0U});
    }

    void snapshotIsCoherentAndIndependent()
    {
        SstvImageFrame frame(2U, 1U);
        frame.writePixel(0U, 0U, {10U, 20U, 30U});
        const auto before = frame.snapshot();
        frame.writePixel(0U, 0U, {40U, 50U, 60U});
        const auto after = frame.snapshot();
        compareRgb(before.pixel(0U, 0U), {10U, 20U, 30U});
        compareRgb(after.pixel(0U, 0U), {40U, 50U, 60U});
        QVERIFY(after.revision > before.revision);
    }

    void cancelAndResetHaveExplicitSemantics()
    {
        SstvImageFrame frame(2U, 2U);
        frame.writePixel(0U, 0U, {1U, 2U, 3U});
        frame.takeDirtyEvents();
        const auto revisionBeforeCancel = frame.snapshot().revision;
        frame.cancel();
        QVERIFY(frame.isCancelled());
        QCOMPARE(frame.writePixel(1U, 1U, {4U, 5U, 6U}),
                 SstvImageWriteResult::Cancelled);
        const auto cancelled = frame.snapshot();
        QVERIFY(cancelled.cancelled);
        QCOMPARE(cancelled.completedPixels, std::size_t {1U});
        QVERIFY(cancelled.revision > revisionBeforeCancel);

        frame.reset();
        QVERIFY(!frame.isCancelled());
        QCOMPARE(frame.coveredComponents(), std::size_t {0U});
        QCOMPARE(frame.completedPixels(), std::size_t {0U});
        QVERIFY(!frame.isComplete());
        const auto resetSnapshot = frame.snapshot();
        compareRgb(resetSnapshot.pixel(0U, 0U), {0U, 0U, 0U});
        QVERIFY(resetSnapshot.revision > cancelled.revision);
        const auto events = frame.takeDirtyEvents();
        QCOMPARE(events.size(), std::size_t {1U});
        QVERIFY((events.front().region == SstvDirtyRegion {0U, 0U, 2U, 2U}));
    }

    void invalidDimensionsRangesAndChannelsAreRejected()
    {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvImageFrame(0U, 1U));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, SstvImageFrame(1U, 0U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvImageFrame(SstvImageFrame::kMaximumDimension + 1U, 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::length_error,
            SstvImageFrame(SstvImageFrame::kMaximumDimension,
                           SstvImageFrame::kMaximumDimension));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvImageFrame(1U, 1U, 0U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvImageFrame(1U, 1U, SstvImageFrame::kMaximumDirtyEvents + 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            SstvImageFrame(std::numeric_limits<std::uint32_t>::max(), 2U));

        SstvImageFrame frame(2U, 2U);
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 frame.writePixel(2U, 0U, {}));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            frame.writeChannel(0U, 0U, static_cast<SstvImageChannel>(255U), 1U));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            frame.writeScanline(0U, std::vector<SstvRgbPixel>(1U)));
        QVERIFY_THROWS_EXCEPTION(
            std::invalid_argument,
            frame.writeScanline(0U, nullptr, 2U));
        QVERIFY_THROWS_EXCEPTION(
            std::out_of_range,
            frame.writeLinearPixels(4U, static_cast<const SstvRgbPixel*>(nullptr), 1U));
        QCOMPARE(frame.writeLinearPixels(
                     4U, static_cast<const SstvRgbPixel*>(nullptr), 0U),
                 SstvImageWriteResult::Unchanged);
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 frame.snapshot().pixel(0U, 2U));
        QVERIFY_THROWS_EXCEPTION(std::out_of_range,
                                 frame.isScanlineComplete(2U));
    }
};

QTEST_APPLESS_MAIN(TestSstvImageCore)
#include "test_sstv_image_core.moc"
