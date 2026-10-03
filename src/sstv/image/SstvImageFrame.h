// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvColourConverter.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace decodium::sstv {

enum class SstvImageChannel : std::uint8_t
{
    Red,
    Green,
    Blue,
    Grayscale,
};

enum class SstvImageWriteResult : std::uint8_t
{
    Changed,
    Unchanged,
    Cancelled,
};

struct SstvDirtyRegion final
{
    std::uint32_t x {0U};
    std::uint32_t y {0U};
    std::uint32_t width {0U};
    std::uint32_t height {0U};
};

constexpr bool operator==(const SstvDirtyRegion& left,
                          const SstvDirtyRegion& right) noexcept
{
    return left.x == right.x && left.y == right.y
        && left.width == right.width && left.height == right.height;
}

constexpr bool operator!=(const SstvDirtyRegion& left,
                          const SstvDirtyRegion& right) noexcept
{
    return !(left == right);
}

struct SstvDirtyEvent final
{
    std::uint64_t firstSequence {0U};
    std::uint64_t lastSequence {0U};
    std::uint64_t operationCount {0U};
    SstvDirtyRegion region;
    bool coalesced {false};
};

// Immutable-in-practice copy of one coherent frame revision.  Its public
// vectors belong to the snapshot and can be handed to a renderer without
// holding the worker's mutex or observing subsequent writes.
struct SstvImageSnapshot final
{
    std::uint32_t width {0U};
    std::uint32_t height {0U};
    std::vector<SstvRgbPixel> pixels;
    // Bit 0/1/2 indicate that red/green/blue have been supplied.
    std::vector<std::uint8_t> channelCoverage;
    std::vector<std::size_t> completedPixelsPerScanline;
    std::size_t coveredComponents {0U};
    std::size_t completedPixels {0U};
    std::uint64_t revision {0U};
    bool cancelled {false};

    const SstvRgbPixel& pixel(std::uint32_t x, std::uint32_t y) const;
    std::uint8_t coverageMask(std::uint32_t x, std::uint32_t y) const;
    bool isScanlineComplete(std::uint32_t y) const;
    bool isComplete() const noexcept;
    double coverage() const noexcept;
};

// Thread-safe, bounded RGB8 assembly surface for progressive SSTV rendering.
// It owns no mode knowledge: coordinates are final row-major destinations.
// Sequential/pass/interlaced decoders must explicitly map their decoded line
// number to destination y before calling writeScanline().
class SstvImageFrame final
{
public:
    static constexpr std::uint32_t kMaximumDimension = 8'192U;
    static constexpr std::size_t kMaximumPixels = 8'388'608U;
    static constexpr std::size_t kDefaultMaximumDirtyEvents = 256U;
    static constexpr std::size_t kMaximumDirtyEvents = 4'096U;

    SstvImageFrame(std::uint32_t width,
                   std::uint32_t height,
                   std::size_t maximumPendingDirtyEvents
                       = kDefaultMaximumDirtyEvents);

    SstvImageFrame(const SstvImageFrame&) = delete;
    SstvImageFrame& operator=(const SstvImageFrame&) = delete;
    SstvImageFrame(SstvImageFrame&&) = delete;
    SstvImageFrame& operator=(SstvImageFrame&&) = delete;

    std::uint32_t width() const noexcept;
    std::uint32_t height() const noexcept;
    std::size_t pixelCount() const noexcept;

    SstvImageWriteResult writePixel(std::uint32_t x,
                                    std::uint32_t y,
                                    SstvRgbPixel pixel);
    SstvImageWriteResult writeChannel(std::uint32_t x,
                                      std::uint32_t y,
                                      SstvImageChannel channel,
                                      std::uint8_t value);

    // count must equal width().  destinationY is intentionally explicit; no
    // hidden progressive/interlace sequence is inferred by this class.
    SstvImageWriteResult writeScanline(std::uint32_t destinationY,
                                       const SstvRgbPixel* pixels,
                                       std::size_t count);
    SstvImageWriteResult writeScanline(std::uint32_t destinationY,
                                       const std::vector<SstvRgbPixel>& pixels);
    SstvImageWriteResult writeChannelScanline(std::uint32_t destinationY,
                                              SstvImageChannel channel,
                                              const std::uint8_t* values,
                                              std::size_t count);
    SstvImageWriteResult writeChannelScanline(
        std::uint32_t destinationY,
        SstvImageChannel channel,
        const std::vector<std::uint8_t>& values);

    // Writes an explicitly row-major destination range and may cross rows.
    // This is useful for decoders that publish bounded pixel chunks.
    SstvImageWriteResult writeLinearPixels(std::size_t firstDestinationPixel,
                                           const SstvRgbPixel* pixels,
                                           std::size_t count);
    SstvImageWriteResult writeLinearPixels(
        std::size_t firstDestinationPixel,
        const std::vector<SstvRgbPixel>& pixels);

    std::size_t coveredComponents() const noexcept;
    std::size_t completedPixels() const noexcept;
    double coverage() const noexcept;
    bool isScanlineComplete(std::uint32_t y) const;
    bool isComplete() const noexcept;

    SstvImageSnapshot snapshot() const;

    // Dirty events are bounded.  Once the configured event count is reached,
    // subsequent changed operations are unioned into the final event and
    // marked coalesced, so no rendered area is lost and memory cannot grow.
    std::vector<SstvDirtyEvent> takeDirtyEvents();
    std::size_t pendingDirtyEvents() const noexcept;
    std::size_t maximumPendingDirtyEvents() const noexcept;

    // cancel() rejects later writes without changing pixels.  reset() clears
    // pixels/coverage, reopens writes, drops stale events, and emits one
    // full-frame dirty event so an adapter can clear its previous image.
    void cancel() noexcept;
    bool isCancelled() const noexcept;
    void reset() noexcept;

private:
    struct DirtyAccumulator;

    static std::uint8_t channelMask(SstvImageChannel channel);
    static SstvDirtyRegion unite(SstvDirtyRegion left,
                                 SstvDirtyRegion right) noexcept;
    std::size_t checkedIndex(std::uint32_t x, std::uint32_t y) const;
    SstvImageWriteResult writePixelLocked(std::size_t index,
                                          SstvRgbPixel pixel,
                                          std::uint8_t mask,
                                          DirtyAccumulator& dirty);
    void recordDirtyLocked(SstvDirtyRegion region) noexcept;
    void incrementRevisionLocked() noexcept;

    const std::uint32_t m_width;
    const std::uint32_t m_height;
    const std::size_t m_pixelCount;
    const std::size_t m_maximumPendingDirtyEvents;

    mutable std::mutex m_mutex;
    std::vector<SstvRgbPixel> m_pixels;
    std::vector<std::uint8_t> m_channelCoverage;
    std::vector<std::size_t> m_completedPixelsPerScanline;
    std::size_t m_coveredComponents {0U};
    std::size_t m_completedPixels {0U};
    std::vector<SstvDirtyEvent> m_dirtyEvents;
    std::uint64_t m_eventSequence {0U};
    std::uint64_t m_revision {0U};
    bool m_cancelled {false};
};

} // namespace decodium::sstv
