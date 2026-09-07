// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace decodium::sstv {

// Minimal seekable byte contract needed by the streaming writer.  The native
// integration layer implements it over QSaveFile/QFileDevice without making
// the DSP core depend on Qt.  Implementations must report short writes and
// translate exceptions into the noexcept return values below.
class SstvSeekableByteSink
{
public:
    virtual ~SstvSeekableByteSink() = default;

    virtual bool resize(std::uint64_t size) noexcept = 0;
    virtual bool seek(std::uint64_t absolutePosition) noexcept = 0;
    virtual std::size_t write(const std::uint8_t* bytes,
                              std::size_t byteCount) noexcept = 0;
    virtual bool flush() noexcept = 0;
};

class SstvWavStreamWriter final
{
public:
    // The writer is deliberately thread-confined.  All state-changing calls
    // and getters for one instance must run on the same worker thread; UI
    // cancellation must be posted to that thread.
    static constexpr std::uint32_t kMinimumSampleRate = 1'000U;
    static constexpr std::uint32_t kMaximumSampleRate = 384'000U;
    static constexpr std::size_t kMaximumSamplesPerAppend = 1U << 20U;
    static constexpr std::size_t kConversionBlockSamples = 4'096U;
    static constexpr std::uint64_t kHeaderBytes = 44U;

    // Keep the complete classic-RIFF file at or below 4 GiB.  PCM16 mono is
    // word-aligned, and the canonical header consumes 44 bytes.
    static constexpr std::uint64_t kMaximumRiffFileBytes = 4'294'967'296ULL;
    static constexpr std::uint64_t kMaximumDataBytes
        = kMaximumRiffFileBytes - kHeaderBytes;
    static constexpr std::uint64_t kMaximumPcmSamples
        = kMaximumDataBytes / 2U;

    enum class State : std::uint8_t
    {
        Idle,
        Writing,
        Finalized,
        Cancelled,
        Failed,
    };

    enum class Error : std::uint8_t
    {
        None,
        InvalidState,
        InvalidSampleRate,
        InvalidArgument,
        NonFiniteSample,
        ChunkTooLarge,
        DeclaredSampleLimit,
        RiffSizeLimit,
        SinkResizeFailed,
        SinkSeekFailed,
        SinkWriteFailed,
        SinkFlushFailed,
        Cancelled,
    };

    struct Metrics final
    {
        std::uint64_t samplesWritten {0U};
        std::uint64_t dataBytesWritten {0U};
        std::uint64_t clippedSamples {0U};
        // Peak absolute normalized input.  PCM16 -32768 maps to 1.0.
        double peakAbsoluteInput {0.0};
    };

    explicit SstvWavStreamWriter(SstvSeekableByteSink& sink) noexcept;

    SstvWavStreamWriter(const SstvWavStreamWriter&) = delete;
    SstvWavStreamWriter& operator=(const SstvWavStreamWriter&) = delete;
    SstvWavStreamWriter(SstvWavStreamWriter&&) = delete;
    SstvWavStreamWriter& operator=(SstvWavStreamWriter&&) = delete;

    // declaredMaximumSamples is a safety upper bound, not an exact expected
    // duration.  Passing a value beyond classic RIFF is rejected immediately,
    // allowing limit validation without allocating/writing multiple GiB.
    bool begin(std::uint32_t sampleRate,
               std::uint64_t declaredMaximumSamples = kMaximumPcmSamples);

    bool appendPcm16(const std::int16_t* samples, std::size_t sampleCount);
    bool appendFloat(const float* samples, std::size_t sampleCount);

    // Patches a canonical 44-byte PCM16 mono little-endian header only after
    // all data, alignment, and final sizing operations have succeeded.  Until
    // this returns true, the sink retains an intentionally zeroed RIFF magic
    // and is not a finalized WAV produced by this class.  A post-magic sink
    // failure triggers a best-effort magic rollback.
    bool finalize();
    bool cancel() noexcept;

    // Re-arms a terminal writer for reuse.  It does not alter a finalized sink;
    // the next begin() atomically starts by resizing the sink to zero.  Reset is
    // rejected while actively writing.
    bool reset() noexcept;

    State state() const noexcept;
    Error lastError() const noexcept;
    std::uint32_t sampleRate() const noexcept;
    std::uint64_t declaredMaximumSamples() const noexcept;
    Metrics metrics() const noexcept;

    static constexpr bool canRepresentPcmSamples(std::uint64_t sampleCount) noexcept
    {
        // RF64 is intentionally not advertised or partially emitted.  Streams
        // beyond this boundary require a separate, complete RF64 writer.
        return sampleCount <= kMaximumPcmSamples;
    }

private:
    bool validateAppend(const void* samples, std::size_t sampleCount);
    bool writePcmBlock(const std::int16_t* samples,
                       std::size_t sampleCount,
                       std::uint64_t clippedSamples,
                       double peakAbsoluteInput);
    bool writeExact(const std::uint8_t* bytes, std::size_t byteCount);
    void invalidateMagicBestEffort() noexcept;
    bool reject(Error error) noexcept;
    bool fail(Error error) noexcept;
    void clearSession() noexcept;

    SstvSeekableByteSink& m_sink;
    State m_state {State::Idle};
    Error m_lastError {Error::None};
    std::uint32_t m_sampleRate {0U};
    std::uint64_t m_declaredMaximumSamples {0U};
    Metrics m_metrics;
};

} // namespace decodium::sstv
