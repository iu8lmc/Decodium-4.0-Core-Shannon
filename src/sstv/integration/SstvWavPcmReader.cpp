// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvWavPcmReader.h"

#include <QByteArray>
#include <QFileInfo>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace decodium::sstv {
namespace {

constexpr quint16 kWaveFormatPcm = 0x0001U;
constexpr quint16 kWaveFormatIeeeFloat = 0x0003U;
constexpr quint16 kWaveFormatExtensible = 0xfffeU;
constexpr qsizetype kMinimumFormatBytes = 16;
constexpr qsizetype kExtensibleFormatBytes = 40;
static_assert(sizeof(short) == sizeof(qint16),
              "SSTV PCM ingress requires a 16-bit short");

quint16 u16(const char* bytes) noexcept
{
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar*>(bytes));
}

quint32 u32(const char* bytes) noexcept
{
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(bytes));
}

bool addWouldOverflow(quint64 left, quint64 right) noexcept
{
    return right > std::numeric_limits<quint64>::max() - left;
}

QString bounded(QString detail)
{
    detail.replace(QLatin1Char('\0'), QChar::ReplacementCharacter);
    return detail.left(512);
}

bool supportedIntegerBits(quint16 bits) noexcept
{
    return bits == 8U || bits == 16U || bits == 24U || bits == 32U;
}

bool supportedFloatBits(quint16 bits) noexcept
{
    return bits == 32U || bits == 64U;
}

bool extensibleSubtype(const QByteArray& format,
                       quint16* baseFormat) noexcept
{
    if (!baseFormat || format.size() < kExtensibleFormatBytes) {
        return false;
    }
    // KSDATAFORMAT_SUBTYPE_PCM / IEEE_FLOAT GUID in little-endian WAVE form:
    // 0000000{1,3}-0000-0010-8000-00aa00389b71.
    static const uchar tail[12] = {
        0x00U, 0x00U, 0x10U, 0x00U, 0x80U, 0x00U,
        0x00U, 0xaaU, 0x00U, 0x38U, 0x9bU, 0x71U,
    };
    const uchar* const guid = reinterpret_cast<const uchar*>(
        format.constData() + 24);
    if (guid[2] != 0U || guid[3] != 0U
        || std::memcmp(guid + 4, tail, sizeof(tail)) != 0) {
        return false;
    }
    *baseFormat = qFromLittleEndian<quint16>(guid);
    return *baseFormat == kWaveFormatPcm
        || *baseFormat == kWaveFormatIeeeFloat;
}

qint32 signed24(const uchar* bytes) noexcept
{
    quint32 value = static_cast<quint32>(bytes[0])
        | (static_cast<quint32>(bytes[1]) << 8U)
        | (static_cast<quint32>(bytes[2]) << 16U);
    if ((value & 0x0080'0000U) != 0U) {
        value |= 0xff00'0000U;
    }
    return static_cast<qint32>(value);
}

short clampPcm16(qint64 sample) noexcept
{
    return static_cast<short>(std::clamp<qint64>(
        sample,
        std::numeric_limits<qint16>::min(),
        std::numeric_limits<qint16>::max()));
}

qint64 signedPcmTo16(qint64 sample, unsigned int containerBits) noexcept
{
    if (containerBits <= 16U) {
        return sample;
    }
    const qint64 divisor = qint64 {1} << (containerBits - 16U);
    // Portable equivalent of an arithmetic right shift. C++17 leaves a
    // right shift of a negative signed value implementation-defined.
    return sample >= 0
        ? sample / divisor
        : -((-sample + divisor - 1) / divisor);
}

} // namespace

bool SstvWavPcmReaderLimits::valid() const noexcept
{
    return maximumFileBytes >= 44U
        && maximumFileBytes <= std::numeric_limits<quint32>::max() + 8ULL
        && maximumDataBytes > 0U
        && maximumDataBytes <= maximumFileBytes
        && maximumDurationMs > 0U
        && minimumSampleRate >= 1'000U
        && maximumSampleRate >= minimumSampleRate
        && maximumSampleRate <= 768'000U
        && maximumChannels > 0U && maximumChannels <= 64U
        && maximumChunks > 0U && maximumChunks <= 1'000'000U
        && maximumFormatChunkBytes >= kMinimumFormatBytes
        && maximumFormatChunkBytes <= 1024U * 1024U
        && maximumFramesPerRead > 0U
        && maximumFramesPerRead <= 1024U * 1024U;
}

SstvWavPcmReader::SstvWavPcmReader(SstvWavPcmReaderLimits limits)
    : m_limits(std::move(limits))
{
    if (!m_limits.valid()) {
        throw std::invalid_argument("invalid SSTV WAV reader limits");
    }
}

SstvWavPcmReader::~SstvWavPcmReader()
{
    close();
}

bool SstvWavPcmReader::open(const QString& path, QString* error)
{
    if (error) {
        error->clear();
    }
    close();
    m_cancelled.store(false, std::memory_order_release);
    if (path.trimmed().isEmpty() || !QFileInfo(path).isAbsolute()) {
        return fail(error, QStringLiteral("WAV path must be absolute"));
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isSymLink()
        || info.size() < 44
        || static_cast<quint64>(info.size()) > m_limits.maximumFileBytes) {
        return fail(error, QStringLiteral(
            "WAV input must be a bounded regular non-symbolic file"));
    }
    m_canonicalPath = info.canonicalFilePath();
    if (m_canonicalPath.isEmpty()) {
        return fail(error, QStringLiteral("WAV path cannot be canonicalized"));
    }
    m_file.setFileName(m_canonicalPath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        const QString detail = m_file.errorString();
        close();
        return fail(error, QStringLiteral("Cannot open WAV input: %1")
                               .arg(detail));
    }
    if (!parse(error)) {
        close();
        return false;
    }
    if (m_cancelled.load(std::memory_order_acquire)) {
        close();
        return fail(error, QStringLiteral("WAV import was cancelled"));
    }
    if (!m_file.seek(static_cast<qint64>(m_format.dataOffset))) {
        close();
        return fail(error, QStringLiteral("Cannot seek to WAV audio data"));
    }
    m_framesRead = 0U;
    return true;
}

void SstvWavPcmReader::close() noexcept
{
    m_cancelled.store(true, std::memory_order_release);
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_file.setFileName(QString {});
    m_canonicalPath.clear();
    m_format = {};
    m_riffEnd = 0U;
    m_framesRead = 0U;
}

bool SstvWavPcmReader::isOpen() const noexcept
{
    return m_file.isOpen() && m_format.totalFrames > 0U;
}

bool SstvWavPcmReader::parse(QString* error)
{
    if (m_cancelled.load(std::memory_order_acquire)) {
        return fail(error, QStringLiteral("WAV import was cancelled"));
    }
    if (!m_file.seek(0)) {
        return fail(error, QStringLiteral("Cannot seek to WAV header"));
    }
    const QByteArray header = m_file.read(12);
    if (header.size() != 12
        || std::memcmp(header.constData(), "RIFF", 4) != 0
        || std::memcmp(header.constData() + 8, "WAVE", 4) != 0) {
        return fail(error, QStringLiteral("Input is not a classic RIFF/WAVE file"));
    }
    const quint64 declared = static_cast<quint64>(
        u32(header.constData() + 4));
    if (declared < 36U || addWouldOverflow(declared, 8U)) {
        return fail(error, QStringLiteral("Invalid RIFF size"));
    }
    m_riffEnd = declared + 8U;
    const quint64 actual = static_cast<quint64>(m_file.size());
    if (m_riffEnd != actual || m_riffEnd > m_limits.maximumFileBytes) {
        return fail(error, QStringLiteral(
            "RIFF size does not exactly match the bounded input file"));
    }

    QByteArray formatBytes;
    quint64 dataOffset = 0U;
    quint64 dataBytes = 0U;
    quint32 chunks = 0U;
    quint64 cursor = 12U;
    while (cursor < m_riffEnd) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            return fail(error, QStringLiteral("WAV import was cancelled"));
        }
        if (++chunks > m_limits.maximumChunks
            || m_riffEnd - cursor < 8U
            || !m_file.seek(static_cast<qint64>(cursor))) {
            return fail(error, QStringLiteral("Malformed or excessive WAV chunks"));
        }
        const QByteArray chunkHeader = m_file.read(8);
        if (chunkHeader.size() != 8) {
            return fail(error, QStringLiteral("Truncated WAV chunk header"));
        }
        const quint64 chunkBytes = static_cast<quint64>(
            u32(chunkHeader.constData() + 4));
        const quint64 payload = cursor + 8U;
        const quint64 padded = chunkBytes + (chunkBytes & 1U);
        if (addWouldOverflow(payload, padded)
            || payload + padded > m_riffEnd) {
            return fail(error, QStringLiteral("WAV chunk escapes the RIFF boundary"));
        }
        const QByteArray id = chunkHeader.left(4);
        if (id == QByteArray("fmt ", 4)) {
            if (!formatBytes.isEmpty()
                || chunkBytes < static_cast<quint64>(kMinimumFormatBytes)
                || chunkBytes > m_limits.maximumFormatChunkBytes
                || chunkBytes > static_cast<quint64>(
                       std::numeric_limits<qsizetype>::max())) {
                return fail(error, QStringLiteral("Invalid or duplicate WAV format chunk"));
            }
            formatBytes = m_file.read(static_cast<qint64>(chunkBytes));
            if (formatBytes.size() != static_cast<qsizetype>(chunkBytes)) {
                return fail(error, QStringLiteral("Truncated WAV format chunk"));
            }
        } else if (id == QByteArray("data", 4)) {
            if (dataOffset != 0U || chunkBytes == 0U
                || chunkBytes > m_limits.maximumDataBytes) {
                return fail(error, QStringLiteral("Invalid or duplicate WAV data chunk"));
            }
            dataOffset = payload;
            dataBytes = chunkBytes;
        }
        cursor = payload + padded;
    }
    if (cursor != m_riffEnd || formatBytes.isEmpty()
        || dataOffset == 0U || dataBytes == 0U) {
        return fail(error, QStringLiteral("WAV format or audio data is missing"));
    }

    quint16 formatTag = u16(formatBytes.constData());
    const quint16 channels = u16(formatBytes.constData() + 2);
    const quint32 sampleRate = u32(formatBytes.constData() + 4);
    const quint32 byteRate = u32(formatBytes.constData() + 8);
    const quint16 blockAlign = u16(formatBytes.constData() + 12);
    const quint16 bits = u16(formatBytes.constData() + 14);
    if (formatBytes.size() > kMinimumFormatBytes) {
        if (formatBytes.size() < 18) {
            return fail(error, QStringLiteral("Truncated WAV format extension"));
        }
        const quint16 extensionBytes = u16(formatBytes.constData() + 16);
        const quint64 availableExtensionBytes = static_cast<quint64>(
            formatBytes.size() - 18);
        if (extensionBytes > availableExtensionBytes) {
            return fail(error, QStringLiteral("Invalid WAV format extension size"));
        }
    }
    quint16 validBits = bits;
    bool extensible = false;
    if (formatTag == kWaveFormatExtensible) {
        if (formatBytes.size() < kExtensibleFormatBytes
            || u16(formatBytes.constData() + 16) < 22U
            || !extensibleSubtype(formatBytes, &formatTag)) {
            return fail(error, QStringLiteral("Unsupported WAVE extensible format"));
        }
        validBits = u16(formatBytes.constData() + 18);
        extensible = true;
    }
    const bool integer = formatTag == kWaveFormatPcm;
    const bool floating = formatTag == kWaveFormatIeeeFloat;
    if ((!integer && !floating)
        || (integer && !supportedIntegerBits(bits))
        || (floating && !supportedFloatBits(bits))
        || channels == 0U || channels > m_limits.maximumChannels
        || sampleRate < m_limits.minimumSampleRate
        || sampleRate > m_limits.maximumSampleRate
        || validBits == 0U || validBits > bits || (bits % 8U) != 0U) {
        return fail(error, QStringLiteral("Unsupported or unsafe WAV sample format"));
    }
    const quint64 bytesPerSample = bits / 8U;
    const quint64 expectedAlign = static_cast<quint64>(channels)
        * bytesPerSample;
    const quint64 expectedByteRate = static_cast<quint64>(sampleRate)
        * expectedAlign;
    if (expectedAlign == 0U
        || expectedAlign > std::numeric_limits<quint16>::max()
        || expectedByteRate > std::numeric_limits<quint32>::max()
        || blockAlign != expectedAlign || byteRate != expectedByteRate
        || (dataBytes % blockAlign) != 0U) {
        return fail(error, QStringLiteral("Inconsistent WAV block or byte rate"));
    }
    const quint64 frames = dataBytes / blockAlign;
    if (frames == 0U
        || frames > std::numeric_limits<quint64>::max() / 1000U) {
        return fail(error, QStringLiteral("Invalid WAV frame count"));
    }
    const quint64 durationMs = (frames * 1000U + sampleRate - 1U)
        / sampleRate;
    if (durationMs == 0U || durationMs > m_limits.maximumDurationMs) {
        return fail(error, QStringLiteral("WAV duration exceeds the import limit"));
    }

    m_format.encoding = integer
        ? SstvWavSampleEncoding::IntegerPcm
        : SstvWavSampleEncoding::IeeeFloat;
    m_format.sampleRate = sampleRate;
    m_format.channels = channels;
    m_format.bitsPerSample = bits;
    m_format.validBitsPerSample = validBits;
    m_format.blockAlign = blockAlign;
    m_format.dataOffset = dataOffset;
    m_format.dataBytes = dataBytes;
    m_format.totalFrames = frames;
    m_format.durationMs = durationMs;
    m_format.extensible = extensible;
    return true;
}

SstvWavReadStatus SstvWavPcmReader::readNext(
    QVector<short>* monoPcm16, QString* error)
{
    if (error) {
        error->clear();
    }
    if (monoPcm16) {
        monoPcm16->clear();
    }
    if (!monoPcm16 || !isOpen()) {
        fail(error, QStringLiteral("WAV reader is not open"));
        return SstvWavReadStatus::Error;
    }
    if (m_cancelled.load(std::memory_order_acquire)) {
        return SstvWavReadStatus::Cancelled;
    }
    if (m_framesRead >= m_format.totalFrames) {
        return SstvWavReadStatus::End;
    }
    const quint64 frames = std::min<quint64>(
        m_limits.maximumFramesPerRead,
        m_format.totalFrames - m_framesRead);
    if (frames > static_cast<quint64>(
                     std::numeric_limits<qsizetype>::max())
        || frames > std::numeric_limits<quint64>::max()
                         / m_format.blockAlign) {
        fail(error, QStringLiteral("WAV read block exceeds platform limits"));
        return SstvWavReadStatus::Error;
    }
    const quint64 byteCount = frames * m_format.blockAlign;
    if (byteCount > static_cast<quint64>(
                        std::numeric_limits<qsizetype>::max())) {
        fail(error, QStringLiteral("WAV read byte count exceeds platform limits"));
        return SstvWavReadStatus::Error;
    }
    const QByteArray raw = m_file.read(static_cast<qint64>(byteCount));
    if (raw.size() != static_cast<qsizetype>(byteCount)) {
        fail(error, QStringLiteral("WAV audio data ended unexpectedly"));
        return SstvWavReadStatus::Error;
    }
    if (m_cancelled.load(std::memory_order_acquire)) {
        return SstvWavReadStatus::Cancelled;
    }
    if (!decodeFrames(raw, frames, monoPcm16, error)) {
        monoPcm16->clear();
        if (m_cancelled.load(std::memory_order_acquire)) {
            if (error) {
                error->clear();
            }
            return SstvWavReadStatus::Cancelled;
        }
        return SstvWavReadStatus::Error;
    }
    if (m_cancelled.load(std::memory_order_acquire)) {
        monoPcm16->clear();
        if (error) {
            error->clear();
        }
        return SstvWavReadStatus::Cancelled;
    }
    m_framesRead += frames;
    return SstvWavReadStatus::Chunk;
}

bool SstvWavPcmReader::decodeFrames(const QByteArray& raw,
                                    quint64 frameCount,
                                    QVector<short>* output,
                                    QString* error) const
{
    if (!output || frameCount > static_cast<quint64>(
                                  std::numeric_limits<qsizetype>::max())) {
        return fail(error, QStringLiteral("Invalid WAV decode output"));
    }
    if (m_format.blockAlign == 0U
        || frameCount > std::numeric_limits<quint64>::max()
                / m_format.blockAlign
        || frameCount * m_format.blockAlign
                != static_cast<quint64>(raw.size())) {
        return fail(error, QStringLiteral("Invalid WAV decode block size"));
    }
    output->resize(static_cast<qsizetype>(frameCount));
    const qsizetype bytesPerSample = m_format.bitsPerSample / 8U;
    const uchar* const bytes = reinterpret_cast<const uchar*>(raw.constData());
    for (quint64 frame = 0U; frame < frameCount; ++frame) {
        if ((frame & 0x03ffU) == 0U
            && m_cancelled.load(std::memory_order_acquire)) {
            return fail(error, QStringLiteral("WAV import was cancelled"));
        }
        long double sum = 0.0L;
        const quint64 frameOffset = frame * m_format.blockAlign;
        for (quint16 channel = 0U; channel < m_format.channels; ++channel) {
            const uchar* const sample = bytes + frameOffset
                + static_cast<quint64>(channel)
                    * static_cast<quint64>(bytesPerSample);
            if (m_format.encoding == SstvWavSampleEncoding::IntegerPcm) {
                qint64 value = 0;
                switch (m_format.bitsPerSample) {
                case 8U:
                    value = (static_cast<qint64>(sample[0]) - 128) * 256;
                    break;
                case 16U:
                    value = qFromLittleEndian<qint16>(sample);
                    break;
                case 24U:
                    value = signedPcmTo16(signed24(sample), 24U);
                    break;
                case 32U:
                    value = signedPcmTo16(
                        qFromLittleEndian<qint32>(sample), 32U);
                    break;
                default:
                    return fail(error,
                                QStringLiteral("Unsupported WAV integer sample"));
                }
                sum += static_cast<long double>(value);
            } else if (m_format.bitsPerSample == 32U) {
                const quint32 bits = qFromLittleEndian<quint32>(sample);
                float value = 0.0F;
                std::memcpy(&value, &bits, sizeof(value));
                if (!std::isfinite(value)) {
                    return fail(error,
                                QStringLiteral("Non-finite WAV float sample"));
                }
                sum += static_cast<long double>(
                    std::clamp(value, -1.0F, 1.0F)) * 32'767.0L;
            } else {
                const quint64 bits = qFromLittleEndian<quint64>(sample);
                double value = 0.0;
                std::memcpy(&value, &bits, sizeof(value));
                if (!std::isfinite(value)) {
                    return fail(error,
                                QStringLiteral("Non-finite WAV double sample"));
                }
                sum += static_cast<long double>(
                    std::clamp(value, -1.0, 1.0)) * 32'767.0L;
            }
        }
        const long double average = sum
            / static_cast<long double>(m_format.channels);
        (*output)[static_cast<qsizetype>(frame)] = clampPcm16(
            static_cast<qint64>(std::llround(average)));
    }
    return true;
}

void SstvWavPcmReader::cancel() noexcept
{
    m_cancelled.store(true, std::memory_order_release);
}

const SstvWavPcmFormat& SstvWavPcmReader::format() const noexcept
{
    return m_format;
}

const SstvWavPcmReaderLimits& SstvWavPcmReader::limits() const noexcept
{
    return m_limits;
}

QString SstvWavPcmReader::canonicalPath() const
{
    return m_canonicalPath;
}

quint64 SstvWavPcmReader::framesRead() const noexcept
{
    return m_framesRead;
}

double SstvWavPcmReader::progress() const noexcept
{
    return m_format.totalFrames == 0U
        ? 0.0
        : std::clamp(
              static_cast<double>(m_framesRead)
                  / static_cast<double>(m_format.totalFrames),
              0.0, 1.0);
}

bool SstvWavPcmReader::fail(QString* error, const QString& detail) noexcept
{
    if (error) {
        *error = bounded(detail);
    }
    return false;
}

} // namespace decodium::sstv
