// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvImageFrame.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {
namespace {

constexpr std::uint8_t kRedCoverage = 1U << 0U;
constexpr std::uint8_t kGreenCoverage = 1U << 1U;
constexpr std::uint8_t kBlueCoverage = 1U << 2U;
constexpr std::uint8_t kCompleteCoverage
    = kRedCoverage | kGreenCoverage | kBlueCoverage;

std::size_t componentCount(std::uint8_t mask) noexcept
{
    return static_cast<std::size_t>((mask & kRedCoverage) != 0U)
        + static_cast<std::size_t>((mask & kGreenCoverage) != 0U)
        + static_cast<std::size_t>((mask & kBlueCoverage) != 0U);
}

std::size_t snapshotIndex(const SstvImageSnapshot& snapshot,
                          std::uint32_t x,
                          std::uint32_t y)
{
    if (x >= snapshot.width || y >= snapshot.height) {
        throw std::out_of_range("SSTV snapshot coordinate is outside the frame");
    }
    return static_cast<std::size_t>(y) * snapshot.width + x;
}

} // namespace

struct SstvImageFrame::DirtyAccumulator final
{
    bool changed {false};
    std::uint32_t minimumX {0U};
    std::uint32_t minimumY {0U};
    std::uint32_t maximumX {0U};
    std::uint32_t maximumY {0U};

    void include(std::uint32_t x, std::uint32_t y) noexcept
    {
        if (!changed) {
            changed = true;
            minimumX = maximumX = x;
            minimumY = maximumY = y;
            return;
        }
        minimumX = std::min(minimumX, x);
        minimumY = std::min(minimumY, y);
        maximumX = std::max(maximumX, x);
        maximumY = std::max(maximumY, y);
    }

    SstvDirtyRegion region() const noexcept
    {
        return {minimumX,
                minimumY,
                maximumX - minimumX + 1U,
                maximumY - minimumY + 1U};
    }
};

const SstvRgbPixel& SstvImageSnapshot::pixel(std::uint32_t x,
                                             std::uint32_t y) const
{
    const std::size_t index = snapshotIndex(*this, x, y);
    if (index >= pixels.size()) {
        throw std::logic_error("SSTV snapshot pixel storage is inconsistent");
    }
    return pixels[index];
}

std::uint8_t SstvImageSnapshot::coverageMask(std::uint32_t x,
                                             std::uint32_t y) const
{
    const std::size_t index = snapshotIndex(*this, x, y);
    if (index >= channelCoverage.size()) {
        throw std::logic_error("SSTV snapshot coverage storage is inconsistent");
    }
    return channelCoverage[index];
}

bool SstvImageSnapshot::isScanlineComplete(std::uint32_t y) const
{
    if (y >= height) {
        throw std::out_of_range("SSTV snapshot scanline is outside the frame");
    }
    if (y >= completedPixelsPerScanline.size()) {
        throw std::logic_error("SSTV snapshot scanline storage is inconsistent");
    }
    return completedPixelsPerScanline[y] == width;
}

bool SstvImageSnapshot::isComplete() const noexcept
{
    return !pixels.empty() && completedPixels == pixels.size();
}

double SstvImageSnapshot::coverage() const noexcept
{
    if (pixels.empty()) {
        return 0.0;
    }
    return static_cast<double>(coveredComponents)
        / (static_cast<double>(pixels.size()) * 3.0);
}

SstvImageFrame::SstvImageFrame(std::uint32_t width,
                               std::uint32_t height,
                               std::size_t maximumPendingDirtyEvents)
    : m_width(width)
    , m_height(height)
    , m_pixelCount([width, height] {
        if (width == 0U || height == 0U || width > kMaximumDimension
            || height > kMaximumDimension) {
            throw std::invalid_argument("invalid SSTV image dimensions");
        }
        if (static_cast<std::size_t>(height) > kMaximumPixels / width) {
            throw std::length_error("SSTV image exceeds its pixel bound");
        }
        return static_cast<std::size_t>(width) * height;
    }())
    , m_maximumPendingDirtyEvents([maximumPendingDirtyEvents] {
        if (maximumPendingDirtyEvents == 0U
            || maximumPendingDirtyEvents > kMaximumDirtyEvents) {
            throw std::invalid_argument("invalid SSTV dirty-event capacity");
        }
        return maximumPendingDirtyEvents;
    }())
    , m_pixels(m_pixelCount)
    , m_channelCoverage(m_pixelCount, 0U)
    , m_completedPixelsPerScanline(m_height, 0U)
{
    m_dirtyEvents.reserve(maximumPendingDirtyEvents);
}

std::uint32_t SstvImageFrame::width() const noexcept
{
    return m_width;
}

std::uint32_t SstvImageFrame::height() const noexcept
{
    return m_height;
}

std::size_t SstvImageFrame::pixelCount() const noexcept
{
    return m_pixelCount;
}

SstvImageWriteResult SstvImageFrame::writePixel(std::uint32_t x,
                                                std::uint32_t y,
                                                SstvRgbPixel pixel)
{
    const std::size_t index = checkedIndex(x, y);
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled) {
        return SstvImageWriteResult::Cancelled;
    }

    DirtyAccumulator dirty;
    const auto result = writePixelLocked(index, pixel, kCompleteCoverage, dirty);
    if (dirty.changed) {
        incrementRevisionLocked();
        recordDirtyLocked(dirty.region());
    }
    return result;
}

SstvImageWriteResult SstvImageFrame::writeChannel(std::uint32_t x,
                                                  std::uint32_t y,
                                                  SstvImageChannel channel,
                                                  std::uint8_t value)
{
    const std::size_t index = checkedIndex(x, y);
    const std::uint8_t mask = channelMask(channel);
    SstvRgbPixel pixel;
    switch (channel) {
    case SstvImageChannel::Red:
        pixel.red = value;
        break;
    case SstvImageChannel::Green:
        pixel.green = value;
        break;
    case SstvImageChannel::Blue:
        pixel.blue = value;
        break;
    case SstvImageChannel::Grayscale:
        pixel = {value, value, value};
        break;
    }

    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled) {
        return SstvImageWriteResult::Cancelled;
    }
    // Reload channels not selected by this write under the mutex.  This avoids
    // overwriting a concurrent writer's other components.
    if (channel != SstvImageChannel::Grayscale) {
        pixel = m_pixels[index];
        if (channel == SstvImageChannel::Red) {
            pixel.red = value;
        } else if (channel == SstvImageChannel::Green) {
            pixel.green = value;
        } else {
            pixel.blue = value;
        }
    }

    DirtyAccumulator dirty;
    const auto result = writePixelLocked(index, pixel, mask, dirty);
    if (dirty.changed) {
        incrementRevisionLocked();
        recordDirtyLocked(dirty.region());
    }
    return result;
}

SstvImageWriteResult SstvImageFrame::writeScanline(std::uint32_t destinationY,
                                                   const SstvRgbPixel* pixels,
                                                   std::size_t count)
{
    if (destinationY >= m_height) {
        throw std::out_of_range("SSTV destination scanline is outside the frame");
    }
    if (count != m_width) {
        throw std::invalid_argument("SSTV scanline width does not match the frame");
    }
    if (pixels == nullptr) {
        throw std::invalid_argument("SSTV scanline pixels must not be null");
    }
    return writeLinearPixels(static_cast<std::size_t>(destinationY) * m_width,
                             pixels,
                             count);
}

SstvImageWriteResult SstvImageFrame::writeScanline(
    std::uint32_t destinationY,
    const std::vector<SstvRgbPixel>& pixels)
{
    return writeScanline(destinationY, pixels.data(), pixels.size());
}

SstvImageWriteResult SstvImageFrame::writeChannelScanline(
    std::uint32_t destinationY,
    SstvImageChannel channel,
    const std::uint8_t* values,
    std::size_t count)
{
    if (destinationY >= m_height) {
        throw std::out_of_range("SSTV destination scanline is outside the frame");
    }
    if (count != m_width) {
        throw std::invalid_argument("SSTV channel width does not match the frame");
    }
    if (values == nullptr) {
        throw std::invalid_argument("SSTV channel values must not be null");
    }
    const std::uint8_t mask = channelMask(channel);

    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled) {
        return SstvImageWriteResult::Cancelled;
    }
    DirtyAccumulator dirty;
    const std::size_t first = static_cast<std::size_t>(destinationY) * m_width;
    for (std::size_t x = 0U; x < count; ++x) {
        SstvRgbPixel pixel = m_pixels[first + x];
        switch (channel) {
        case SstvImageChannel::Red:
            pixel.red = values[x];
            break;
        case SstvImageChannel::Green:
            pixel.green = values[x];
            break;
        case SstvImageChannel::Blue:
            pixel.blue = values[x];
            break;
        case SstvImageChannel::Grayscale:
            pixel = {values[x], values[x], values[x]};
            break;
        }
        writePixelLocked(first + x, pixel, mask, dirty);
    }
    if (!dirty.changed) {
        return SstvImageWriteResult::Unchanged;
    }
    incrementRevisionLocked();
    recordDirtyLocked(dirty.region());
    return SstvImageWriteResult::Changed;
}

SstvImageWriteResult SstvImageFrame::writeChannelScanline(
    std::uint32_t destinationY,
    SstvImageChannel channel,
    const std::vector<std::uint8_t>& values)
{
    return writeChannelScanline(destinationY, channel, values.data(), values.size());
}

SstvImageWriteResult SstvImageFrame::writeLinearPixels(
    std::size_t firstDestinationPixel,
    const SstvRgbPixel* pixels,
    std::size_t count)
{
    if (firstDestinationPixel > m_pixelCount
        || count > m_pixelCount - firstDestinationPixel) {
        throw std::out_of_range("SSTV linear pixel range is outside the frame");
    }
    if (count != 0U && pixels == nullptr) {
        throw std::invalid_argument("SSTV linear pixels must not be null");
    }

    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cancelled) {
        return SstvImageWriteResult::Cancelled;
    }
    DirtyAccumulator dirty;
    for (std::size_t offset = 0U; offset < count; ++offset) {
        writePixelLocked(firstDestinationPixel + offset,
                         pixels[offset],
                         kCompleteCoverage,
                         dirty);
    }
    if (!dirty.changed) {
        return SstvImageWriteResult::Unchanged;
    }
    incrementRevisionLocked();
    recordDirtyLocked(dirty.region());
    return SstvImageWriteResult::Changed;
}

SstvImageWriteResult SstvImageFrame::writeLinearPixels(
    std::size_t firstDestinationPixel,
    const std::vector<SstvRgbPixel>& pixels)
{
    return writeLinearPixels(firstDestinationPixel, pixels.data(), pixels.size());
}

std::size_t SstvImageFrame::coveredComponents() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_coveredComponents;
}

std::size_t SstvImageFrame::completedPixels() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_completedPixels;
}

double SstvImageFrame::coverage() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<double>(m_coveredComponents)
        / (static_cast<double>(m_pixelCount) * 3.0);
}

bool SstvImageFrame::isScanlineComplete(std::uint32_t y) const
{
    if (y >= m_height) {
        throw std::out_of_range("SSTV scanline is outside the frame");
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_completedPixelsPerScanline[y] == m_width;
}

bool SstvImageFrame::isComplete() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_completedPixels == m_pixelCount;
}

SstvImageSnapshot SstvImageFrame::snapshot() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return {m_width,
            m_height,
            m_pixels,
            m_channelCoverage,
            m_completedPixelsPerScanline,
            m_coveredComponents,
            m_completedPixels,
            m_revision,
            m_cancelled};
}

std::vector<SstvDirtyEvent> SstvImageFrame::takeDirtyEvents()
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SstvDirtyEvent> events(m_dirtyEvents.begin(), m_dirtyEvents.end());
    m_dirtyEvents.clear();
    return events;
}

std::size_t SstvImageFrame::pendingDirtyEvents() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_dirtyEvents.size();
}

std::size_t SstvImageFrame::maximumPendingDirtyEvents() const noexcept
{
    return m_maximumPendingDirtyEvents;
}

void SstvImageFrame::cancel() noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cancelled) {
        m_cancelled = true;
        incrementRevisionLocked();
    }
}

bool SstvImageFrame::isCancelled() const noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_cancelled;
}

void SstvImageFrame::reset() noexcept
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_pixels.begin(), m_pixels.end(), SstvRgbPixel {});
    std::fill(m_channelCoverage.begin(), m_channelCoverage.end(), 0U);
    std::fill(m_completedPixelsPerScanline.begin(),
              m_completedPixelsPerScanline.end(),
              0U);
    m_coveredComponents = 0U;
    m_completedPixels = 0U;
    m_cancelled = false;
    m_dirtyEvents.clear();
    incrementRevisionLocked();
    recordDirtyLocked({0U, 0U, m_width, m_height});
}

std::uint8_t SstvImageFrame::channelMask(SstvImageChannel channel)
{
    switch (channel) {
    case SstvImageChannel::Red:
        return kRedCoverage;
    case SstvImageChannel::Green:
        return kGreenCoverage;
    case SstvImageChannel::Blue:
        return kBlueCoverage;
    case SstvImageChannel::Grayscale:
        return kCompleteCoverage;
    }
    throw std::invalid_argument("invalid SSTV image channel");
}

SstvDirtyRegion SstvImageFrame::unite(SstvDirtyRegion left,
                                      SstvDirtyRegion right) noexcept
{
    const std::uint32_t minimumX = std::min(left.x, right.x);
    const std::uint32_t minimumY = std::min(left.y, right.y);
    const std::uint32_t maximumX
        = std::max(left.x + left.width, right.x + right.width);
    const std::uint32_t maximumY
        = std::max(left.y + left.height, right.y + right.height);
    return {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
}

std::size_t SstvImageFrame::checkedIndex(std::uint32_t x,
                                         std::uint32_t y) const
{
    if (x >= m_width || y >= m_height) {
        throw std::out_of_range("SSTV pixel coordinate is outside the frame");
    }
    return static_cast<std::size_t>(y) * m_width + x;
}

SstvImageWriteResult SstvImageFrame::writePixelLocked(
    std::size_t index,
    SstvRgbPixel pixel,
    std::uint8_t mask,
    DirtyAccumulator& dirty)
{
    SstvRgbPixel& destination = m_pixels[index];
    const std::uint8_t oldCoverage = m_channelCoverage[index];
    const std::uint8_t newCoverage = oldCoverage | mask;
    bool changed = newCoverage != oldCoverage;

    if ((mask & kRedCoverage) != 0U && destination.red != pixel.red) {
        destination.red = pixel.red;
        changed = true;
    }
    if ((mask & kGreenCoverage) != 0U && destination.green != pixel.green) {
        destination.green = pixel.green;
        changed = true;
    }
    if ((mask & kBlueCoverage) != 0U && destination.blue != pixel.blue) {
        destination.blue = pixel.blue;
        changed = true;
    }
    if (!changed) {
        return SstvImageWriteResult::Unchanged;
    }

    m_channelCoverage[index] = newCoverage;
    m_coveredComponents += componentCount(newCoverage) - componentCount(oldCoverage);
    if (oldCoverage != kCompleteCoverage && newCoverage == kCompleteCoverage) {
        ++m_completedPixels;
        const std::size_t y = index / m_width;
        ++m_completedPixelsPerScanline[y];
    }
    dirty.include(static_cast<std::uint32_t>(index % m_width),
                  static_cast<std::uint32_t>(index / m_width));
    return SstvImageWriteResult::Changed;
}

void SstvImageFrame::recordDirtyLocked(SstvDirtyRegion region) noexcept
{
    if (m_eventSequence != std::numeric_limits<std::uint64_t>::max()) {
        ++m_eventSequence;
    }
    const SstvDirtyEvent event {
        m_eventSequence, m_eventSequence, 1U, region, false};
    if (m_dirtyEvents.size() < m_maximumPendingDirtyEvents) {
        m_dirtyEvents.push_back(event);
        return;
    }

    SstvDirtyEvent& finalEvent = m_dirtyEvents.back();
    finalEvent.lastSequence = event.lastSequence;
    if (finalEvent.operationCount != std::numeric_limits<std::uint64_t>::max()) {
        ++finalEvent.operationCount;
    }
    finalEvent.region = unite(finalEvent.region, region);
    finalEvent.coalesced = true;
}

void SstvImageFrame::incrementRevisionLocked() noexcept
{
    if (m_revision != std::numeric_limits<std::uint64_t>::max()) {
        ++m_revision;
    }
}

} // namespace decodium::sstv
