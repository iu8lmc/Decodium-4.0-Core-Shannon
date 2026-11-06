// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIODevice>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace decodium::sstv {

// Minimal pull contract shared by native SSTV encoders and the Qt audio
// adapter.  Implementations own their image/codec state and must keep pull()
// allocation-free.  pull/reset have one owner; cancel is the concurrent path.
class SstvPcm16Source
{
public:
    virtual ~SstvPcm16Source() = default;

    virtual std::uint32_t sampleRate() const noexcept = 0;
    virtual std::uint64_t totalSamples() const noexcept = 0;
    virtual std::uint64_t producedSamples() const noexcept = 0;
    virtual bool complete() const noexcept = 0;
    virtual bool cancelled() const noexcept = 0;
    virtual std::size_t pullPcm16(std::int16_t* output,
                                  std::size_t capacity) = 0;
    virtual void cancel() noexcept = 0;
    virtual void reset() = 0;
};

enum class SstvTxChannelRoute : std::uint8_t
{
    Both,
    Left,
    Right,
};

// Sequential QIODevice consumed directly by Decodium's existing SoundOutput.
// It converts a mono native SSTV source to the selected one/two-channel 48 kHz
// Int16 stream in bounded chunks.  It never owns a complete PCM waveform.
class SstvTxAudioDevice final : public QIODevice
{
public:
    static constexpr std::size_t MaximumFramesPerPull = 4'096U;
    static constexpr unsigned MaximumChannelCount = 2U;

    explicit SstvTxAudioDevice(
        std::unique_ptr<SstvPcm16Source> source,
        unsigned channelCount,
        SstvTxChannelRoute route = SstvTxChannelRoute::Both,
        QObject* parent = nullptr);
    ~SstvTxAudioDevice() override;

    SstvTxAudioDevice(const SstvTxAudioDevice&) = delete;
    SstvTxAudioDevice& operator=(const SstvTxAudioDevice&) = delete;

    bool open(OpenMode mode) override;
    void close() override;
    bool isSequential() const override;
    qint64 pos() const override;
    qint64 size() const override;
    bool atEnd() const override;
    qint64 bytesAvailable() const override;
    bool reset() override;

    std::uint32_t sampleRate() const noexcept;
    unsigned channelCount() const noexcept;
    SstvTxChannelRoute channelRoute() const noexcept;
    std::uint64_t totalFrames() const noexcept;
    std::uint64_t framesProducedBySource() const noexcept;
    std::uint64_t framesReadFromDevice() const noexcept;
    qint64 bytesReadFromDevice() const noexcept;
    std::uint32_t peakPcm16Magnitude() const noexcept;
    double peakNormalized() const noexcept;
    std::uint64_t clippedFrames() const noexcept;
    bool cancelled() const noexcept;
    bool failed() const noexcept;

    // May be invoked concurrently with readData().  At most one already-read
    // sink buffer remains outside this object; no further PCM is returned.
    void cancel() noexcept;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    static bool isKnownRoute(SstvTxChannelRoute route) noexcept;
    bool refillPendingFrames();
    void encodeFrame(std::int16_t sample, char* output) const noexcept;
    qint64 fail(const QString& message) noexcept;

    std::unique_ptr<SstvPcm16Source> source_;
    unsigned channelCount_ {1U};
    SstvTxChannelRoute route_ {SstvTxChannelRoute::Both};
    std::uint64_t totalFrames_ {0U};
    qint64 totalBytes_ {0};
    std::atomic<qint64> bytesRead_ {0};
    std::atomic<std::uint64_t> sourceFramesProduced_ {0U};
    std::atomic<std::uint32_t> peakPcm16Magnitude_ {0U};
    std::atomic<std::uint64_t> clippedFrames_ {0U};

    std::vector<std::int16_t> monoScratch_;
    std::vector<char> pendingBytes_;
    std::size_t pendingOffset_ {0U};
    std::size_t pendingSize_ {0U};

    std::atomic_bool cancelRequested_ {false};
    std::atomic_bool failed_ {false};
};

} // namespace decodium::sstv
