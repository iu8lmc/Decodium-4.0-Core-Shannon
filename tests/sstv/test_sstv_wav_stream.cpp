// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/tx/SstvWavStreamWriter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

using namespace decodium::sstv;

namespace {

class MemorySink final : public SstvSeekableByteSink
{
public:
    bool resize(std::uint64_t size) noexcept override
    {
        ++resizeCalls;
        if (failResizeCall != 0U && resizeCalls == failResizeCall) {
            return false;
        }
        if (size > static_cast<std::uint64_t>(
                       std::numeric_limits<std::size_t>::max())) {
            return false;
        }
        try {
            bytes.resize(static_cast<std::size_t>(size));
        } catch (...) {
            return false;
        }
        if (position > size) {
            position = size;
        }
        return true;
    }

    bool seek(std::uint64_t absolutePosition) noexcept override
    {
        ++seekCalls;
        if (failSeekCall != 0U && seekCalls == failSeekCall) {
            return false;
        }
        if (absolutePosition > bytes.size()) {
            return false;
        }
        position = absolutePosition;
        return true;
    }

    std::size_t write(const std::uint8_t* source,
                      std::size_t byteCount) noexcept override
    {
        ++writeCalls;
        std::size_t accepted = byteCount;
        if (shortWriteCall != 0U && writeCalls == shortWriteCall
            && accepted != 0U) {
            --accepted;
        }
        if (source == nullptr && accepted != 0U) {
            return 0U;
        }
        if (position > std::numeric_limits<std::uint64_t>::max() - accepted) {
            return 0U;
        }
        const std::uint64_t end = position + accepted;
        if (end > static_cast<std::uint64_t>(
                      std::numeric_limits<std::size_t>::max())) {
            return 0U;
        }
        try {
            if (end > bytes.size()) {
                bytes.resize(static_cast<std::size_t>(end));
            }
            std::copy_n(source,
                        accepted,
                        bytes.begin() + static_cast<std::ptrdiff_t>(position));
        } catch (...) {
            return 0U;
        }
        position = end;
        return accepted;
    }

    bool flush() noexcept override
    {
        ++flushCalls;
        return failFlushCall == 0U || flushCalls != failFlushCall;
    }

    std::vector<std::uint8_t> bytes;
    std::uint64_t position {0U};
    std::size_t resizeCalls {0U};
    std::size_t seekCalls {0U};
    std::size_t writeCalls {0U};
    std::size_t flushCalls {0U};
    std::size_t failResizeCall {0U};
    std::size_t failSeekCall {0U};
    std::size_t shortWriteCall {0U};
    std::size_t failFlushCall {0U};
};

std::uint32_t littleEndian32(const std::vector<std::uint8_t>& bytes,
                             std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes.at(offset))
        | (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 3U)) << 24U);
}

std::int16_t littleEndianPcm16(const std::vector<std::uint8_t>& bytes,
                               std::size_t sampleIndex)
{
    const std::size_t offset = 44U + sampleIndex * 2U;
    const std::uint16_t bits = static_cast<std::uint16_t>(bytes.at(offset))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes.at(offset + 1U)) << 8U);
    return static_cast<std::int16_t>(bits);
}

} // namespace

class TestSstvWavStream final : public QObject
{
    Q_OBJECT

private slots:
    void pcm16GoldenHeaderAndLittleEndianPayload()
    {
        MemorySink sink;
        SstvWavStreamWriter writer(sink);
        QVERIFY(writer.begin(12'000U));
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Writing);
        QCOMPARE(sink.bytes.size(), std::size_t {44U});
        QVERIFY(std::all_of(sink.bytes.begin(), sink.bytes.end(),
                            [](std::uint8_t value) { return value == 0U; }));

        const std::int16_t samples[] {
            std::numeric_limits<std::int16_t>::min(), -1, 0, 1,
            std::numeric_limits<std::int16_t>::max()};
        QVERIFY(writer.appendPcm16(samples, std::size(samples)));
        // The header remains deliberately invalid until finalize.
        QCOMPARE(sink.bytes[0], std::uint8_t {0U});
        QVERIFY(writer.finalize());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Finalized);

        const std::vector<std::uint8_t> expected {
            'R', 'I', 'F', 'F', 0x2e, 0x00, 0x00, 0x00,
            'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
            0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
            0xe0, 0x2e, 0x00, 0x00, 0xc0, 0x5d, 0x00, 0x00,
            0x02, 0x00, 0x10, 0x00, 'd', 'a', 't', 'a',
            0x0a, 0x00, 0x00, 0x00,
            0x00, 0x80, 0xff, 0xff, 0x00, 0x00, 0x01, 0x00,
            0xff, 0x7f};
        QVERIFY(sink.bytes == expected);
        QCOMPARE(sink.flushCalls, std::size_t {2U});

        const auto metrics = writer.metrics();
        QCOMPARE(metrics.samplesWritten, std::uint64_t {5U});
        QCOMPARE(metrics.dataBytesWritten, std::uint64_t {10U});
        QCOMPARE(metrics.clippedSamples, std::uint64_t {0U});
        QCOMPARE(metrics.peakAbsoluteInput, 1.0);
    }

    void floatChunksProduceIdenticalBytes()
    {
        std::vector<float> samples(10'123U);
        for (std::size_t index = 0U; index < samples.size(); ++index) {
            samples[index] = static_cast<float>(
                0.8 * std::sin(static_cast<double>(index) * 0.071));
        }

        MemorySink oneSink;
        SstvWavStreamWriter one(oneSink);
        QVERIFY(one.begin(44'100U));
        QVERIFY(one.appendFloat(samples.data(), samples.size()));
        QVERIFY(one.finalize());

        MemorySink chunkedSink;
        SstvWavStreamWriter chunked(chunkedSink);
        QVERIFY(chunked.begin(44'100U));
        const std::size_t sizes[] {1U, 17U, 4'097U, 3U, 511U};
        std::size_t offset = 0U;
        std::size_t chunk = 0U;
        while (offset < samples.size()) {
            const std::size_t count = std::min(
                sizes[chunk++ % std::size(sizes)], samples.size() - offset);
            QVERIFY(chunked.appendFloat(samples.data() + offset, count));
            offset += count;
        }
        QVERIFY(chunked.finalize());
        QVERIFY(chunkedSink.bytes == oneSink.bytes);
        QCOMPARE(chunked.metrics().samplesWritten, one.metrics().samplesWritten);
        QCOMPARE(chunked.metrics().clippedSamples, one.metrics().clippedSamples);
    }

    void floatConversionClipsAndReportsMetrics()
    {
        MemorySink sink;
        SstvWavStreamWriter writer(sink);
        QVERIFY(writer.begin(12'000U));
        const float samples[] {-2.0F, -1.0F, -0.5F, 0.0F,
                               0.5F, 1.0F, 2.0F};
        QVERIFY(writer.appendFloat(samples, std::size(samples)));
        QVERIFY(writer.finalize());

        const std::int16_t expected[] {-32'768, -32'768, -16'384, 0,
                                       16'384, 32'767, 32'767};
        for (std::size_t index = 0U; index < std::size(expected); ++index) {
            QCOMPARE(littleEndianPcm16(sink.bytes, index), expected[index]);
        }
        const auto metrics = writer.metrics();
        QCOMPARE(metrics.samplesWritten, std::uint64_t {7U});
        QCOMPARE(metrics.clippedSamples, std::uint64_t {2U});
        QCOMPARE(metrics.peakAbsoluteInput, 2.0);
    }

    void emptyFileHasCanonicalSizesAndNoPadding()
    {
        MemorySink sink;
        SstvWavStreamWriter writer(sink);
        QVERIFY(writer.begin(48'000U, 0U));
        QVERIFY(writer.appendPcm16(nullptr, 0U));
        QVERIFY(writer.appendFloat(nullptr, 0U));
        QVERIFY(writer.finalize());
        QCOMPARE(sink.bytes.size(), std::size_t {44U});
        QCOMPARE(littleEndian32(sink.bytes, 4U), std::uint32_t {36U});
        QCOMPARE(littleEndian32(sink.bytes, 40U), std::uint32_t {0U});
        QCOMPARE(writer.metrics().dataBytesWritten, std::uint64_t {0U});

        // PCM16 mono data is intrinsically even-sized; one sample therefore
        // extends the file by exactly two bytes and needs no RIFF pad byte.
        QVERIFY(writer.reset());
        QVERIFY(writer.begin(48'000U, 1U));
        const std::int16_t zero = 0;
        QVERIFY(writer.appendPcm16(&zero, 1U));
        QVERIFY(writer.finalize());
        QCOMPARE(sink.bytes.size(), std::size_t {46U});
        QCOMPARE(littleEndian32(sink.bytes, 4U), std::uint32_t {38U});
        QCOMPARE(littleEndian32(sink.bytes, 40U), std::uint32_t {2U});
    }

    void invalidOrderAndInputAreTransactional()
    {
        MemorySink sink;
        SstvWavStreamWriter writer(sink);
        const std::int16_t zero = 0;
        QVERIFY(!writer.appendPcm16(&zero, 1U));
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Idle);
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::InvalidState);
        QVERIFY(!writer.begin(999U));
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::InvalidSampleRate);
        QVERIFY(writer.begin(12'000U, 4U));
        QVERIFY(!writer.begin(12'000U));
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Writing);

        QVERIFY(!writer.appendPcm16(nullptr, 1U));
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::InvalidArgument);
        const float invalid[] {0.0F, std::numeric_limits<float>::quiet_NaN()};
        QVERIFY(!writer.appendFloat(invalid, std::size(invalid)));
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::NonFiniteSample);
        QCOMPARE(writer.metrics().samplesWritten, std::uint64_t {0U});

        const float finite = 0.0F;
        QVERIFY(!writer.appendFloat(
            &finite, SstvWavStreamWriter::kMaximumSamplesPerAppend + 1U));
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::ChunkTooLarge);
        QCOMPARE(writer.metrics().samplesWritten, std::uint64_t {0U});
        QVERIFY(writer.appendPcm16(&zero, 1U));
        QVERIFY(!writer.reset());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Writing);
        QVERIFY(writer.finalize());
        QVERIFY(!writer.finalize());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Finalized);
    }

    void classicRiffAndDeclaredLimitsNeedNoHugeAllocation()
    {
        QVERIFY(SstvWavStreamWriter::canRepresentPcmSamples(
            SstvWavStreamWriter::kMaximumPcmSamples));
        QVERIFY(!SstvWavStreamWriter::canRepresentPcmSamples(
            SstvWavStreamWriter::kMaximumPcmSamples + 1U));
        QCOMPARE(SstvWavStreamWriter::kMaximumPcmSamples * 2U + 44U,
                 SstvWavStreamWriter::kMaximumRiffFileBytes);
        QCOMPARE(SstvWavStreamWriter::kMaximumPcmSamples * 2U + 36U,
                 std::uint64_t {4'294'967'288ULL});

        MemorySink sink;
        SstvWavStreamWriter writer(sink);
        QVERIFY(!writer.begin(12'000U,
                              SstvWavStreamWriter::kMaximumPcmSamples + 1U));
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::RiffSizeLimit);
        QVERIFY(sink.bytes.empty());

        QVERIFY(writer.begin(12'000U, 2U));
        const std::int16_t three[] {0, 0, 0};
        QVERIFY(!writer.appendPcm16(three, std::size(three)));
        QCOMPARE(writer.lastError(),
                 SstvWavStreamWriter::Error::DeclaredSampleLimit);
        QCOMPARE(writer.metrics().samplesWritten, std::uint64_t {0U});
        QVERIFY(writer.appendPcm16(three, 2U));
        QVERIFY(writer.finalize());
    }

    void shortWritesAreFatalAndHeaderStaysInvalid()
    {
        MemorySink dataSink;
        SstvWavStreamWriter dataWriter(dataSink);
        QVERIFY(dataWriter.begin(12'000U)); // write call 1: zero header
        dataSink.shortWriteCall = 2U;
        const std::int16_t samples[] {1, 2};
        QVERIFY(!dataWriter.appendPcm16(samples, std::size(samples)));
        QCOMPARE(dataWriter.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(dataWriter.lastError(),
                 SstvWavStreamWriter::Error::SinkWriteFailed);
        QCOMPARE(dataSink.bytes[0], std::uint8_t {0U});

        MemorySink headerSink;
        SstvWavStreamWriter headerWriter(headerSink);
        QVERIFY(headerWriter.begin(12'000U)); // write 1
        QVERIFY(headerWriter.appendPcm16(samples, std::size(samples))); // write 2
        headerSink.shortWriteCall = 3U; // header body, before RIFF magic
        QVERIFY(!headerWriter.finalize());
        QCOMPARE(headerWriter.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(headerSink.bytes[0], std::uint8_t {0U});
        QCOMPARE(headerSink.bytes[1], std::uint8_t {0U});
        QCOMPARE(headerSink.bytes[2], std::uint8_t {0U});
        QCOMPARE(headerSink.bytes[3], std::uint8_t {0U});

        MemorySink magicSink;
        SstvWavStreamWriter magicWriter(magicSink);
        QVERIFY(magicWriter.begin(12'000U)); // write 1
        QVERIFY(magicWriter.appendPcm16(samples, std::size(samples))); // write 2
        magicSink.shortWriteCall = 4U; // write 3 is body; write 4 is RIFF magic
        QVERIFY(!magicWriter.finalize());
        QCOMPARE(magicWriter.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(magicSink.bytes[0], std::uint8_t {0U});
        QCOMPARE(magicSink.bytes[1], std::uint8_t {0U});
        QCOMPARE(magicSink.bytes[2], std::uint8_t {0U});
        QCOMPARE(magicSink.bytes[3], std::uint8_t {0U});

        dataSink.shortWriteCall = 0U;
        QVERIFY(dataWriter.reset());
        QVERIFY(dataWriter.begin(12'000U, 0U));
        QVERIFY(dataWriter.finalize());
    }

    void seekResizeAndFlushFailuresAreExplicit()
    {
        const std::int16_t sample = 1;

        MemorySink beginResizeSink;
        beginResizeSink.failResizeCall = 1U;
        SstvWavStreamWriter beginResize(beginResizeSink);
        QVERIFY(!beginResize.begin(12'000U));
        QCOMPARE(beginResize.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(beginResize.lastError(),
                 SstvWavStreamWriter::Error::SinkResizeFailed);

        MemorySink beginSeekSink;
        beginSeekSink.failSeekCall = 1U;
        SstvWavStreamWriter beginSeek(beginSeekSink);
        QVERIFY(!beginSeek.begin(12'000U));
        QCOMPARE(beginSeek.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(beginSeek.lastError(),
                 SstvWavStreamWriter::Error::SinkSeekFailed);

        MemorySink finalSeekSink;
        SstvWavStreamWriter finalSeek(finalSeekSink);
        QVERIFY(finalSeek.begin(12'000U)); // seek 1
        QVERIFY(finalSeek.appendPcm16(&sample, 1U));
        finalSeekSink.failSeekCall = 2U; // finalize header body seek
        QVERIFY(!finalSeek.finalize());
        QCOMPARE(finalSeek.lastError(),
                 SstvWavStreamWriter::Error::SinkSeekFailed);
        QCOMPARE(finalSeekSink.bytes[0], std::uint8_t {0U});

        MemorySink resizeSink;
        SstvWavStreamWriter resizeWriter(resizeSink);
        QVERIFY(resizeWriter.begin(12'000U)); // resize 1
        QVERIFY(resizeWriter.appendPcm16(&sample, 1U));
        resizeSink.failResizeCall = 2U;
        QVERIFY(!resizeWriter.finalize());
        QCOMPARE(resizeWriter.lastError(),
                 SstvWavStreamWriter::Error::SinkResizeFailed);

        MemorySink cancelSink;
        SstvWavStreamWriter cancelWriter(cancelSink);
        QVERIFY(cancelWriter.begin(12'000U)); // resize 1
        cancelSink.failResizeCall = 2U;
        QVERIFY(!cancelWriter.cancel());
        QCOMPARE(cancelWriter.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(cancelWriter.lastError(),
                 SstvWavStreamWriter::Error::SinkResizeFailed);

        MemorySink flushSink;
        SstvWavStreamWriter flushWriter(flushSink);
        QVERIFY(flushWriter.begin(12'000U));
        QVERIFY(flushWriter.appendPcm16(&sample, 1U));
        flushSink.failFlushCall = 1U;
        QVERIFY(!flushWriter.finalize());
        QCOMPARE(flushWriter.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(flushWriter.lastError(),
                 SstvWavStreamWriter::Error::SinkFlushFailed);
        QCOMPARE(flushSink.bytes[0], std::uint8_t {0U});
        QCOMPARE(flushSink.bytes[1], std::uint8_t {0U});
        QCOMPARE(flushSink.bytes[2], std::uint8_t {0U});
        QCOMPARE(flushSink.bytes[3], std::uint8_t {0U});

        MemorySink finalFlushSink;
        SstvWavStreamWriter finalFlushWriter(finalFlushSink);
        QVERIFY(finalFlushWriter.begin(12'000U));
        QVERIFY(finalFlushWriter.appendPcm16(&sample, 1U));
        finalFlushSink.failFlushCall = 2U;
        QVERIFY(!finalFlushWriter.finalize());
        QCOMPARE(finalFlushWriter.state(), SstvWavStreamWriter::State::Failed);
        QCOMPARE(finalFlushWriter.lastError(),
                 SstvWavStreamWriter::Error::SinkFlushFailed);
        QCOMPARE(finalFlushSink.bytes[0], std::uint8_t {0U});
        QCOMPARE(finalFlushSink.bytes[1], std::uint8_t {0U});
        QCOMPARE(finalFlushSink.bytes[2], std::uint8_t {0U});
        QCOMPARE(finalFlushSink.bytes[3], std::uint8_t {0U});
    }

    void cancelInvalidatesAndTerminalWriterCanBeReused()
    {
        MemorySink sink;
        SstvWavStreamWriter writer(sink);
        QVERIFY(writer.begin(12'000U));
        const std::int16_t samples[] {1, 2, 3};
        QVERIFY(writer.appendPcm16(samples, std::size(samples)));
        QVERIFY(writer.cancel());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Cancelled);
        QCOMPARE(writer.lastError(), SstvWavStreamWriter::Error::Cancelled);
        QVERIFY(sink.bytes.empty());
        QVERIFY(!writer.finalize());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Cancelled);

        QVERIFY(writer.reset());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Idle);
        QCOMPARE(writer.metrics().samplesWritten, std::uint64_t {0U});
        QVERIFY(writer.begin(8'000U, 0U));
        QVERIFY(writer.finalize());
        QCOMPARE(writer.state(), SstvWavStreamWriter::State::Finalized);
        QCOMPARE(sink.bytes.size(), std::size_t {44U});

        // Resetting a finalized writer leaves its valid bytes in place until a
        // subsequent begin explicitly starts the next stream.
        QVERIFY(writer.reset());
        QCOMPARE(sink.bytes.size(), std::size_t {44U});
        QVERIFY(writer.begin(16'000U, 0U));
        QVERIFY(writer.finalize());
        QCOMPARE(littleEndian32(sink.bytes, 24U), std::uint32_t {16'000U});
    }
};

QTEST_APPLESS_MAIN(TestSstvWavStream)
#include "test_sstv_wav_stream.moc"
