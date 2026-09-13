// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFile>
#include <QString>
#include <QVector>

#include <atomic>
#include <cstdint>

namespace decodium::sstv {

enum class SstvWavSampleEncoding : std::uint8_t
{
    IntegerPcm,
    IeeeFloat,
};

struct SstvWavPcmReaderLimits final
{
    quint64 maximumFileBytes {1024ULL * 1024ULL * 1024ULL};
    quint64 maximumDataBytes {1024ULL * 1024ULL * 1024ULL};
    quint64 maximumDurationMs {30ULL * 60ULL * 1000ULL};
    quint32 minimumSampleRate {8'000U};
    quint32 maximumSampleRate {192'000U};
    quint16 maximumChannels {8U};
    quint32 maximumChunks {4'096U};
    quint32 maximumFormatChunkBytes {65'536U};
    quint32 maximumFramesPerRead {65'536U};

    bool valid() const noexcept;
};

struct SstvWavPcmFormat final
{
    SstvWavSampleEncoding encoding {SstvWavSampleEncoding::IntegerPcm};
    quint32 sampleRate {0U};
    quint16 channels {0U};
    quint16 bitsPerSample {0U};
    quint16 validBitsPerSample {0U};
    quint16 blockAlign {0U};
    quint64 dataOffset {0U};
    quint64 dataBytes {0U};
    quint64 totalFrames {0U};
    quint64 durationMs {0U};
    bool extensible {false};
};

enum class SstvWavReadStatus : std::uint8_t
{
    Chunk,
    End,
    Cancelled,
    Error,
};

// Single-worker streaming RIFF/WAVE reader. It validates the complete chunk
// structure before exposing audio, keeps only one bounded raw block and one
// mono PCM16 output block resident, and never performs resampling. The caller
// passes each returned block to Decodium's existing SSTV audio ingress, which
// remains the sole DSP/resampling path. Only cancel() is cross-thread safe.
class SstvWavPcmReader final
{
public:
    explicit SstvWavPcmReader(SstvWavPcmReaderLimits limits = {});
    ~SstvWavPcmReader();

    SstvWavPcmReader(const SstvWavPcmReader&) = delete;
    SstvWavPcmReader& operator=(const SstvWavPcmReader&) = delete;

    bool open(const QString& path, QString* error = nullptr);
    void close() noexcept;
    bool isOpen() const noexcept;

    SstvWavReadStatus readNext(QVector<short>* monoPcm16,
                               QString* error = nullptr);
    void cancel() noexcept;

    const SstvWavPcmFormat& format() const noexcept;
    const SstvWavPcmReaderLimits& limits() const noexcept;
    QString canonicalPath() const;
    quint64 framesRead() const noexcept;
    double progress() const noexcept;

private:
    bool parse(QString* error);
    bool decodeFrames(const QByteArray& raw,
                      quint64 frameCount,
                      QVector<short>* output,
                      QString* error) const;
    static bool fail(QString* error, const QString& detail) noexcept;

    SstvWavPcmReaderLimits m_limits;
    QFile m_file;
    QString m_canonicalPath;
    SstvWavPcmFormat m_format;
    quint64 m_riffEnd {0U};
    quint64 m_framesRead {0U};
    std::atomic_bool m_cancelled {false};
};

} // namespace decodium::sstv
