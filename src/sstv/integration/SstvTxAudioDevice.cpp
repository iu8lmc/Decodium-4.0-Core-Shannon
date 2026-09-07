// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTxAudioDevice.h"

#include "../tx/SstvToneGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace decodium::sstv {

SstvTxAudioDevice::SstvTxAudioDevice(
    std::unique_ptr<SstvPcm16Source> source,
    unsigned channelCount,
    SstvTxChannelRoute route,
    QObject* parent)
    : QIODevice(parent)
    , source_(std::move(source))
    , channelCount_(channelCount)
    , route_(route)
{
    if (!source_) {
        throw std::invalid_argument("SSTV TX audio source must not be null");
    }
    if (channelCount_ == 0U || channelCount_ > MaximumChannelCount) {
        throw std::invalid_argument("SSTV TX audio channel count must be one or two");
    }
    if (!isKnownRoute(route_)) {
        throw std::invalid_argument("unknown SSTV TX audio channel route");
    }
    if (channelCount_ == 1U && route_ != SstvTxChannelRoute::Both) {
        throw std::invalid_argument("left/right SSTV routing requires stereo output");
    }
    if (!SstvToneGenerator::isSupportedSampleRate(source_->sampleRate())) {
        throw std::invalid_argument("unsupported SSTV TX audio source sample rate");
    }

    totalFrames_ = source_->totalSamples();
    constexpr std::uint64_t sampleBytes = sizeof(std::int16_t);
    const std::uint64_t bytesPerFrame = sampleBytes * channelCount_;
    const std::uint64_t maximumBytes = static_cast<std::uint64_t>(
        std::numeric_limits<qint64>::max());
    if (totalFrames_ > maximumBytes / bytesPerFrame) {
        throw std::overflow_error("SSTV TX audio byte length exceeds QIODevice limits");
    }
    totalBytes_ = static_cast<qint64>(totalFrames_ * bytesPerFrame);

    monoScratch_.resize(MaximumFramesPerPull);
    pendingBytes_.resize(MaximumFramesPerPull * bytesPerFrame);
}

SstvTxAudioDevice::~SstvTxAudioDevice()
{
    cancel();
    close();
}

bool SstvTxAudioDevice::open(OpenMode mode)
{
    if ((mode & WriteOnly) != 0 || (mode & ReadOnly) == 0) {
        setErrorString(QStringLiteral("SSTV TX audio device is read-only"));
        return false;
    }
    if (isOpen()) {
        setErrorString(QStringLiteral("SSTV TX audio device is already open"));
        return false;
    }
    return QIODevice::open(ReadOnly | Unbuffered);
}

void SstvTxAudioDevice::close()
{
    QIODevice::close();
}

bool SstvTxAudioDevice::isSequential() const
{
    return true;
}

qint64 SstvTxAudioDevice::pos() const
{
    return bytesRead_.load(std::memory_order_acquire);
}

qint64 SstvTxAudioDevice::size() const
{
    return totalBytes_;
}

bool SstvTxAudioDevice::atEnd() const
{
    return cancelled()
        || failed()
        || (pendingOffset_ >= pendingSize_ && source_->complete());
}

qint64 SstvTxAudioDevice::bytesAvailable() const
{
    if (cancelled() || failed()) {
        return 0;
    }
    return std::max<qint64>(
               0, totalBytes_ - bytesRead_.load(std::memory_order_acquire))
        + QIODevice::bytesAvailable();
}

bool SstvTxAudioDevice::reset()
{
    if (isOpen()) {
        setErrorString(QStringLiteral("close SSTV TX audio before resetting it"));
        return false;
    }
    source_->reset();
    pendingOffset_ = 0U;
    pendingSize_ = 0U;
    bytesRead_.store(0, std::memory_order_release);
    sourceFramesProduced_.store(0U, std::memory_order_release);
    peakPcm16Magnitude_.store(0U, std::memory_order_release);
    clippedFrames_.store(0U, std::memory_order_release);
    cancelRequested_.store(false, std::memory_order_release);
    failed_.store(false, std::memory_order_release);
    setErrorString({});
    return true;
}

std::uint32_t SstvTxAudioDevice::sampleRate() const noexcept
{
    return source_->sampleRate();
}

unsigned SstvTxAudioDevice::channelCount() const noexcept
{
    return channelCount_;
}

SstvTxChannelRoute SstvTxAudioDevice::channelRoute() const noexcept
{
    return route_;
}

std::uint64_t SstvTxAudioDevice::totalFrames() const noexcept
{
    return totalFrames_;
}

std::uint64_t SstvTxAudioDevice::framesProducedBySource() const noexcept
{
    return sourceFramesProduced_.load(std::memory_order_acquire);
}

std::uint64_t SstvTxAudioDevice::framesReadFromDevice() const noexcept
{
    const qint64 bytes = bytesRead_.load(std::memory_order_acquire);
    const qint64 bytesPerFrame = static_cast<qint64>(
        sizeof(std::int16_t) * channelCount_);
    return bytes <= 0 || bytesPerFrame <= 0
        ? 0U : static_cast<std::uint64_t>(bytes / bytesPerFrame);
}

qint64 SstvTxAudioDevice::bytesReadFromDevice() const noexcept
{
    return bytesRead_.load(std::memory_order_acquire);
}

std::uint32_t SstvTxAudioDevice::peakPcm16Magnitude() const noexcept
{
    return peakPcm16Magnitude_.load(std::memory_order_acquire);
}

double SstvTxAudioDevice::peakNormalized() const noexcept
{
    constexpr double pcm16FullScale = 32'768.0;
    return static_cast<double>(peakPcm16Magnitude()) / pcm16FullScale;
}

std::uint64_t SstvTxAudioDevice::clippedFrames() const noexcept
{
    return clippedFrames_.load(std::memory_order_acquire);
}

bool SstvTxAudioDevice::cancelled() const noexcept
{
    return cancelRequested_.load(std::memory_order_acquire)
        || source_->cancelled();
}

bool SstvTxAudioDevice::failed() const noexcept
{
    return failed_.load(std::memory_order_acquire);
}

void SstvTxAudioDevice::cancel() noexcept
{
    cancelRequested_.store(true, std::memory_order_release);
    source_->cancel();
}

qint64 SstvTxAudioDevice::readData(char* data, qint64 maxSize)
{
    if (maxSize < 0 || (maxSize > 0 && data == nullptr)) {
        return fail(QStringLiteral("invalid SSTV TX audio read buffer"));
    }
    if (maxSize == 0 || cancelled() || failed() || atEnd()) {
        return 0;
    }

    qint64 copied = 0;
    while (copied < maxSize) {
        if (pendingOffset_ >= pendingSize_) {
            if (!refillPendingFrames()) {
                if (failed() && copied == 0) {
                    return -1;
                }
                break;
            }
        }

        const std::size_t pending = pendingSize_ - pendingOffset_;
        const auto requested = static_cast<std::size_t>(maxSize - copied);
        const std::size_t count = std::min(pending, requested);
        std::memcpy(data + copied, pendingBytes_.data() + pendingOffset_, count);
        pendingOffset_ += count;
        copied += static_cast<qint64>(count);

        // A single source pull bounds the work and latency of every QIODevice
        // callback even when a hostile caller asks for an enormous buffer.
        if (pendingOffset_ >= pendingSize_) {
            break;
        }
    }

    bytesRead_.fetch_add(copied, std::memory_order_release);
    return copied;
}

qint64 SstvTxAudioDevice::writeData(const char*, qint64)
{
    return fail(QStringLiteral("SSTV TX audio device does not accept writes"));
}

bool SstvTxAudioDevice::isKnownRoute(SstvTxChannelRoute route) noexcept
{
    switch (route) {
    case SstvTxChannelRoute::Both:
    case SstvTxChannelRoute::Left:
    case SstvTxChannelRoute::Right:
        return true;
    }
    return false;
}

bool SstvTxAudioDevice::refillPendingFrames()
{
    if (cancelled() || failed() || source_->complete()) {
        return false;
    }

    const std::uint64_t before = source_->producedSamples();
    if (before > totalFrames_) {
        fail(QStringLiteral("SSTV TX source reported an invalid sample position"));
        return false;
    }
    const std::size_t capacity = static_cast<std::size_t>(
        std::min<std::uint64_t>(MaximumFramesPerPull, totalFrames_ - before));
    if (capacity == 0U) {
        if (!source_->complete()) {
            fail(QStringLiteral("SSTV TX source exceeded its declared length"));
        }
        return false;
    }

    std::size_t produced = 0U;
    try {
        produced = source_->pullPcm16(monoScratch_.data(), capacity);
    } catch (const std::exception& exception) {
        fail(QStringLiteral("SSTV TX source failed: %1")
                 .arg(QString::fromUtf8(exception.what())));
        return false;
    } catch (...) {
        fail(QStringLiteral("SSTV TX source failed with an unknown exception"));
        return false;
    }

    const std::uint64_t after = source_->producedSamples();
    if (produced == 0U) {
        if (!source_->complete() && !source_->cancelled()) {
            fail(QStringLiteral("SSTV TX source made no progress"));
        }
        return false;
    }
    if (produced > capacity
        || after < before
        || after - before != produced
        || after > totalFrames_) {
        fail(QStringLiteral("SSTV TX source violated its pull contract"));
        return false;
    }
    sourceFramesProduced_.store(after, std::memory_order_release);

    std::uint32_t blockPeak = 0U;
    std::uint64_t blockClipped = 0U;
    for (std::size_t index = 0U; index < produced; ++index) {
        const std::int32_t widened = monoScratch_[index];
        const std::uint32_t magnitude = static_cast<std::uint32_t>(
            widened < 0 ? -widened : widened);
        blockPeak = std::max(blockPeak, magnitude);
        if (magnitude >= 32'767U) {
            ++blockClipped;
        }
    }
    std::uint32_t observedPeak = peakPcm16Magnitude_.load(
        std::memory_order_relaxed);
    while (observedPeak < blockPeak
           && !peakPcm16Magnitude_.compare_exchange_weak(
               observedPeak,
               blockPeak,
               std::memory_order_release,
               std::memory_order_relaxed)) {
    }
    if (blockClipped != 0U) {
        std::uint64_t previous = clippedFrames_.load(std::memory_order_relaxed);
        for (;;) {
            const std::uint64_t maximum
                = std::numeric_limits<std::uint64_t>::max();
            const std::uint64_t next = previous > maximum - blockClipped
                ? maximum : previous + blockClipped;
            if (clippedFrames_.compare_exchange_weak(
                    previous,
                    next,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                break;
            }
        }
    }

    const std::size_t bytesPerFrame = sizeof(std::int16_t) * channelCount_;
    for (std::size_t index = 0U; index < produced; ++index) {
        encodeFrame(monoScratch_[index],
                    pendingBytes_.data() + index * bytesPerFrame);
    }
    pendingOffset_ = 0U;
    pendingSize_ = produced * bytesPerFrame;
    return true;
}

void SstvTxAudioDevice::encodeFrame(std::int16_t sample,
                                    char* output) const noexcept
{
    if (channelCount_ == 1U) {
        std::memcpy(output, &sample, sizeof(sample));
        return;
    }

    const std::int16_t silence = 0;
    const std::int16_t left = route_ == SstvTxChannelRoute::Right
        ? silence : sample;
    const std::int16_t right = route_ == SstvTxChannelRoute::Left
        ? silence : sample;
    std::memcpy(output, &left, sizeof(left));
    std::memcpy(output + sizeof(left), &right, sizeof(right));
}

qint64 SstvTxAudioDevice::fail(const QString& message) noexcept
{
    failed_.store(true, std::memory_order_release);
    setErrorString(message);
    return -1;
}

} // namespace decodium::sstv
