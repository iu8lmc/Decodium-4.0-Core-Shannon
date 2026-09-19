// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvWavStreamWriter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace decodium::sstv {
namespace {

constexpr std::size_t kBytesPerSample = 2U;

void putLittleEndian16(std::uint8_t* destination, std::uint16_t value) noexcept
{
    destination[0] = static_cast<std::uint8_t>(value & 0xffU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void putLittleEndian32(std::uint8_t* destination, std::uint32_t value) noexcept
{
    destination[0] = static_cast<std::uint8_t>(value & 0xffU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    destination[2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    destination[3] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

std::array<std::uint8_t, SstvWavStreamWriter::kHeaderBytes> makeHeader(
    std::uint32_t sampleRate,
    std::uint64_t dataBytes) noexcept
{
    std::array<std::uint8_t, SstvWavStreamWriter::kHeaderBytes> header {};
    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    putLittleEndian32(header.data() + 4U,
                      static_cast<std::uint32_t>(36U + dataBytes));
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    putLittleEndian32(header.data() + 16U, 16U); // PCM fmt payload
    putLittleEndian16(header.data() + 20U, 1U); // integer PCM
    putLittleEndian16(header.data() + 22U, 1U); // mono
    putLittleEndian32(header.data() + 24U, sampleRate);
    putLittleEndian32(header.data() + 28U, sampleRate * 2U);
    putLittleEndian16(header.data() + 32U, 2U); // block alignment
    putLittleEndian16(header.data() + 34U, 16U);
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    putLittleEndian32(header.data() + 40U,
                      static_cast<std::uint32_t>(dataBytes));
    return header;
}

double normalizedMagnitude(std::int16_t sample) noexcept
{
    if (sample < 0) {
        return static_cast<double>(-static_cast<std::int32_t>(sample)) / 32'768.0;
    }
    return static_cast<double>(sample) / 32'767.0;
}

std::int16_t floatToPcm16(float sample) noexcept
{
    if (sample <= -1.0F) {
        return std::numeric_limits<std::int16_t>::min();
    }
    if (sample >= 1.0F) {
        return std::numeric_limits<std::int16_t>::max();
    }
    const double scale = sample < 0.0F ? 32'768.0 : 32'767.0;
    return static_cast<std::int16_t>(std::lround(static_cast<double>(sample) * scale));
}

} // namespace

SstvWavStreamWriter::SstvWavStreamWriter(SstvSeekableByteSink& sink) noexcept
    : m_sink(sink)
{
}

bool SstvWavStreamWriter::begin(std::uint32_t sampleRate,
                                std::uint64_t declaredMaximumSamples)
{
    if (m_state != State::Idle) {
        return reject(Error::InvalidState);
    }
    if (sampleRate < kMinimumSampleRate || sampleRate > kMaximumSampleRate) {
        return reject(Error::InvalidSampleRate);
    }
    if (!canRepresentPcmSamples(declaredMaximumSamples)) {
        return reject(Error::RiffSizeLimit);
    }

    clearSession();
    m_sampleRate = sampleRate;
    m_declaredMaximumSamples = declaredMaximumSamples;
    if (!m_sink.resize(0U)) {
        return fail(Error::SinkResizeFailed);
    }
    if (!m_sink.seek(0U)) {
        return fail(Error::SinkSeekFailed);
    }

    // A zero header deliberately withholds RIFF/WAVE magic until finalize().
    const std::array<std::uint8_t, kHeaderBytes> unfinishedHeader {};
    if (!writeExact(unfinishedHeader.data(), unfinishedHeader.size())) {
        return false;
    }
    m_state = State::Writing;
    m_lastError = Error::None;
    return true;
}

bool SstvWavStreamWriter::appendPcm16(const std::int16_t* samples,
                                      std::size_t sampleCount)
{
    if (!validateAppend(samples, sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        m_lastError = Error::None;
        return true;
    }

    std::size_t offset = 0U;
    while (offset < sampleCount) {
        const std::size_t block = std::min(kConversionBlockSamples,
                                           sampleCount - offset);
        double peak = 0.0;
        for (std::size_t index = 0U; index < block; ++index) {
            peak = std::max(peak, normalizedMagnitude(samples[offset + index]));
        }
        if (!writePcmBlock(samples + offset, block, 0U, peak)) {
            return false;
        }
        offset += block;
    }
    m_lastError = Error::None;
    return true;
}

bool SstvWavStreamWriter::appendFloat(const float* samples,
                                      std::size_t sampleCount)
{
    if (!validateAppend(samples, sampleCount)) {
        return false;
    }
    if (sampleCount == 0U) {
        m_lastError = Error::None;
        return true;
    }

    // Reject the complete bounded callback chunk transactionally before any
    // bytes or metrics are changed.
    for (std::size_t index = 0U; index < sampleCount; ++index) {
        if (!std::isfinite(samples[index])) {
            return reject(Error::NonFiniteSample);
        }
    }

    std::array<std::int16_t, kConversionBlockSamples> converted {};
    std::size_t offset = 0U;
    while (offset < sampleCount) {
        const std::size_t block = std::min(kConversionBlockSamples,
                                           sampleCount - offset);
        std::uint64_t clipped = 0U;
        double peak = 0.0;
        for (std::size_t index = 0U; index < block; ++index) {
            const float value = samples[offset + index];
            peak = std::max(peak, std::abs(static_cast<double>(value)));
            if (value < -1.0F || value > 1.0F) {
                ++clipped;
            }
            converted[index] = floatToPcm16(value);
        }
        if (!writePcmBlock(converted.data(), block, clipped, peak)) {
            return false;
        }
        offset += block;
    }
    m_lastError = Error::None;
    return true;
}

bool SstvWavStreamWriter::finalize()
{
    if (m_state != State::Writing) {
        return reject(Error::InvalidState);
    }
    const std::uint64_t dataBytes = m_metrics.dataBytesWritten;
    const std::uint64_t padding = dataBytes & 1U;
    if (dataBytes > kMaximumDataBytes
        || 36U + dataBytes + padding > std::numeric_limits<std::uint32_t>::max()
        || kHeaderBytes + dataBytes + padding > kMaximumRiffFileBytes) {
        return fail(Error::RiffSizeLimit);
    }

    if (padding != 0U) {
        const std::uint8_t zero = 0U;
        if (!writeExact(&zero, 1U)) {
            return false;
        }
    }
    const std::uint64_t finalSize = kHeaderBytes + dataBytes + padding;
    if (!m_sink.resize(finalSize)) {
        return fail(Error::SinkResizeFailed);
    }
    // Patch every size/format byte while the RIFF magic remains zero.  The
    // four-byte magic is written last, so a short body write cannot leave a
    // file that parsers mistake for a successfully finalized WAV.
    if (!m_sink.seek(4U)) {
        return fail(Error::SinkSeekFailed);
    }

    const auto header = makeHeader(m_sampleRate, dataBytes);
    if (!writeExact(header.data() + 4U, header.size() - 4U)) {
        return false;
    }
    // Make the payload and non-magic header durable before publishing RIFF as
    // the four-byte commit marker.
    if (!m_sink.flush()) {
        return fail(Error::SinkFlushFailed);
    }
    if (!m_sink.seek(0U)) {
        return fail(Error::SinkSeekFailed);
    }
    if (!writeExact(header.data(), 4U)) {
        invalidateMagicBestEffort();
        return false;
    }
    if (!m_sink.flush()) {
        invalidateMagicBestEffort();
        return fail(Error::SinkFlushFailed);
    }

    m_state = State::Finalized;
    m_lastError = Error::None;
    return true;
}

bool SstvWavStreamWriter::cancel() noexcept
{
    if (m_state != State::Writing) {
        return reject(Error::InvalidState);
    }
    if (!m_sink.resize(0U)) {
        return fail(Error::SinkResizeFailed);
    }
    m_state = State::Cancelled;
    m_lastError = Error::Cancelled;
    return true;
}

bool SstvWavStreamWriter::reset() noexcept
{
    if (m_state == State::Writing) {
        return reject(Error::InvalidState);
    }
    clearSession();
    m_state = State::Idle;
    m_lastError = Error::None;
    return true;
}

SstvWavStreamWriter::State SstvWavStreamWriter::state() const noexcept
{
    return m_state;
}

SstvWavStreamWriter::Error SstvWavStreamWriter::lastError() const noexcept
{
    return m_lastError;
}

std::uint32_t SstvWavStreamWriter::sampleRate() const noexcept
{
    return m_sampleRate;
}

std::uint64_t SstvWavStreamWriter::declaredMaximumSamples() const noexcept
{
    return m_declaredMaximumSamples;
}

SstvWavStreamWriter::Metrics SstvWavStreamWriter::metrics() const noexcept
{
    return m_metrics;
}

bool SstvWavStreamWriter::validateAppend(const void* samples,
                                         std::size_t sampleCount)
{
    if (m_state != State::Writing) {
        return reject(Error::InvalidState);
    }
    if (sampleCount > kMaximumSamplesPerAppend) {
        return reject(Error::ChunkTooLarge);
    }
    if (sampleCount != 0U && samples == nullptr) {
        return reject(Error::InvalidArgument);
    }
    const std::uint64_t count = static_cast<std::uint64_t>(sampleCount);
    if (count > m_declaredMaximumSamples - m_metrics.samplesWritten) {
        return reject(Error::DeclaredSampleLimit);
    }
    if (count > kMaximumPcmSamples - m_metrics.samplesWritten) {
        return reject(Error::RiffSizeLimit);
    }
    return true;
}

bool SstvWavStreamWriter::writePcmBlock(const std::int16_t* samples,
                                        std::size_t sampleCount,
                                        std::uint64_t clippedSamples,
                                        double peakAbsoluteInput)
{
    std::array<std::uint8_t, kConversionBlockSamples * kBytesPerSample> bytes {};
    for (std::size_t index = 0U; index < sampleCount; ++index) {
        putLittleEndian16(bytes.data() + index * kBytesPerSample,
                          static_cast<std::uint16_t>(samples[index]));
    }
    const std::size_t byteCount = sampleCount * kBytesPerSample;
    if (!writeExact(bytes.data(), byteCount)) {
        return false;
    }

    m_metrics.samplesWritten += static_cast<std::uint64_t>(sampleCount);
    m_metrics.dataBytesWritten += static_cast<std::uint64_t>(byteCount);
    m_metrics.clippedSamples += clippedSamples;
    m_metrics.peakAbsoluteInput
        = std::max(m_metrics.peakAbsoluteInput, peakAbsoluteInput);
    return true;
}

bool SstvWavStreamWriter::writeExact(const std::uint8_t* bytes,
                                     std::size_t byteCount)
{
    const std::size_t written = m_sink.write(bytes, byteCount);
    if (written != byteCount) {
        return fail(Error::SinkWriteFailed);
    }
    return true;
}

void SstvWavStreamWriter::invalidateMagicBestEffort() noexcept
{
    const std::array<std::uint8_t, 4U> invalidMagic {};
    if (!m_sink.seek(0U)) {
        return;
    }
    if (m_sink.write(invalidMagic.data(), invalidMagic.size())
        != invalidMagic.size()) {
        return;
    }
    static_cast<void>(m_sink.flush());
}

bool SstvWavStreamWriter::reject(Error error) noexcept
{
    m_lastError = error;
    return false;
}

bool SstvWavStreamWriter::fail(Error error) noexcept
{
    m_state = State::Failed;
    m_lastError = error;
    return false;
}

void SstvWavStreamWriter::clearSession() noexcept
{
    m_sampleRate = 0U;
    m_declaredMaximumSamples = 0U;
    m_metrics = {};
}

} // namespace decodium::sstv
