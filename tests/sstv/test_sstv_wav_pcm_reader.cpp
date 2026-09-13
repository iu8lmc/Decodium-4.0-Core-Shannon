// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvWavPcmReader.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace decodium::sstv;

namespace {

struct WavChunk final
{
    QByteArray id;
    QByteArray payload;
};

void appendLe16(QByteArray& output, quint16 value)
{
    output.append(static_cast<char>(value & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
}

void appendLe24(QByteArray& output, qint32 value)
{
    const quint32 bits = static_cast<quint32>(value);
    output.append(static_cast<char>(bits & 0xffU));
    output.append(static_cast<char>((bits >> 8U) & 0xffU));
    output.append(static_cast<char>((bits >> 16U) & 0xffU));
}

void appendLe32(QByteArray& output, quint32 value)
{
    output.append(static_cast<char>(value & 0xffU));
    output.append(static_cast<char>((value >> 8U) & 0xffU));
    output.append(static_cast<char>((value >> 16U) & 0xffU));
    output.append(static_cast<char>((value >> 24U) & 0xffU));
}

void appendLe64(QByteArray& output, quint64 value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U) {
        output.append(static_cast<char>((value >> shift) & 0xffU));
    }
}

void replaceLe16(QByteArray& output, qsizetype offset, quint16 value)
{
    output[offset] = static_cast<char>(value & 0xffU);
    output[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void replaceLe32(QByteArray& output, qsizetype offset, quint32 value)
{
    output[offset] = static_cast<char>(value & 0xffU);
    output[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
    output[offset + 2] = static_cast<char>((value >> 16U) & 0xffU);
    output[offset + 3] = static_cast<char>((value >> 24U) & 0xffU);
}

QByteArray classicFormat(quint16 tag,
                         quint16 channels,
                         quint32 sampleRate,
                         quint16 bitsPerSample)
{
    const quint16 blockAlign = static_cast<quint16>(
        channels * static_cast<quint16>(bitsPerSample / 8U));
    QByteArray format;
    appendLe16(format, tag);
    appendLe16(format, channels);
    appendLe32(format, sampleRate);
    appendLe32(format, sampleRate * blockAlign);
    appendLe16(format, blockAlign);
    appendLe16(format, bitsPerSample);
    return format;
}

QByteArray extensiblePcmFormat()
{
    QByteArray format = classicFormat(0xfffeU, 1U, 12'000U, 16U);
    appendLe16(format, 22U); // cbSize
    appendLe16(format, 16U); // valid bits
    appendLe32(format, 0U); // unspecified channel mask
    // KSDATAFORMAT_SUBTYPE_PCM: 00000001-0000-0010-8000-00aa00389b71.
    const char guid[16] = {
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x10, 0x00,
        static_cast<char>(0x80), 0x00, 0x00, static_cast<char>(0xaa),
        0x00, 0x38, static_cast<char>(0x9b), 0x71,
    };
    format.append(guid, 16);
    return format;
}

QByteArray riff(const QList<WavChunk>& chunks)
{
    QByteArray body("WAVE", 4);
    for (const WavChunk& chunk : chunks) {
        Q_ASSERT(chunk.id.size() == 4);
        body.append(chunk.id);
        appendLe32(body, static_cast<quint32>(chunk.payload.size()));
        body.append(chunk.payload);
        if ((chunk.payload.size() & 1) != 0) {
            body.append('\0');
        }
    }
    QByteArray result("RIFF", 4);
    appendLe32(result, static_cast<quint32>(body.size()));
    result.append(body);
    return result;
}

QByteArray canonicalPcm16(const QByteArray& data,
                          quint16 channels = 1U,
                          quint32 sampleRate = 12'000U)
{
    return riff({{QByteArray("fmt ", 4),
                  classicFormat(1U, channels, sampleRate, 16U)},
                 {QByteArray("data", 4), data}});
}

QString writeFixture(QTemporaryDir& directory,
                     const QString& name,
                     const QByteArray& bytes)
{
    const QString path = directory.filePath(name);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(bytes) != bytes.size()
        || !file.commit()) {
        return {};
    }
    return path;
}

QByteArray pcm16(std::initializer_list<qint16> samples)
{
    QByteArray data;
    for (const qint16 sample : samples) {
        appendLe16(data, static_cast<quint16>(sample));
    }
    return data;
}

QByteArray float32(std::initializer_list<float> samples)
{
    QByteArray data;
    for (const float sample : samples) {
        quint32 bits = 0U;
        static_assert(sizeof(bits) == sizeof(sample));
        std::memcpy(&bits, &sample, sizeof(bits));
        appendLe32(data, bits);
    }
    return data;
}

QByteArray float64(std::initializer_list<double> samples)
{
    QByteArray data;
    for (const double sample : samples) {
        quint64 bits = 0U;
        static_assert(sizeof(bits) == sizeof(sample));
        std::memcpy(&bits, &sample, sizeof(bits));
        appendLe64(data, bits);
    }
    return data;
}

} // namespace

class TestSstvWavPcmReader final : public QObject
{
    Q_OBJECT

private slots:
    void limitsFailClosed()
    {
        SstvWavPcmReaderLimits limits;
        QVERIFY(limits.valid());

        limits.maximumFramesPerRead = 0U;
        QVERIFY(!limits.valid());
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument,
                                 (void) SstvWavPcmReader {limits});

        limits = {};
        limits.maximumFileBytes =
            static_cast<quint64>(std::numeric_limits<quint32>::max()) + 9U;
        QVERIFY(!limits.valid());
    }

    void readsPcm16StereoInBoundedChunks()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray data = pcm16({-32'768, -32'768,
                                      1'000, 3'000,
                                      32'767, 32'767});
        const QString path = writeFixture(
            directory, QStringLiteral("stereo.wav"),
            canonicalPcm16(data, 2U));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReaderLimits limits;
        limits.maximumFramesPerRead = 2U;
        SstvWavPcmReader reader(limits);
        QString error = QStringLiteral("stale");
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QVERIFY(error.isEmpty());
        QVERIFY(reader.isOpen());
        QCOMPARE(reader.canonicalPath(), QFileInfo(path).canonicalFilePath());
        QCOMPARE(reader.format().encoding,
                 SstvWavSampleEncoding::IntegerPcm);
        QCOMPARE(reader.format().channels, quint16 {2U});
        QCOMPARE(reader.format().sampleRate, quint32 {12'000U});
        QCOMPARE(reader.format().bitsPerSample, quint16 {16U});
        QCOMPARE(reader.format().totalFrames, quint64 {3U});
        QCOMPARE(reader.framesRead(), quint64 {0U});
        QCOMPARE(reader.progress(), 0.0);

        QVector<short> output {123};
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Chunk);
        QCOMPARE(output, QVector<short>({-32'768, 2'000}));
        QCOMPARE(reader.framesRead(), quint64 {2U});
        QVERIFY(std::abs(reader.progress() - (2.0 / 3.0)) < 1.0e-12);

        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Chunk);
        QCOMPARE(output, QVector<short>({32'767}));
        QCOMPARE(reader.progress(), 1.0);
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::End);
        QVERIFY(output.isEmpty());
        QVERIFY(error.isEmpty());
    }

    void readsIntegerPcmDepths_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QVector<short>>("expected");

        QTest::newRow("unsigned-8")
            << 8 << QByteArray::fromRawData("\x00\x80\xff", 3)
            << QVector<short>({-32'768, 0, 32'512});

        QByteArray pcm24;
        appendLe24(pcm24, -8'388'608);
        appendLe24(pcm24, -1);
        appendLe24(pcm24, 0);
        appendLe24(pcm24, 8'388'352);
        QTest::newRow("signed-24")
            << 24 << pcm24 << QVector<short>({-32'768, -1, 0, 32'767});

        QByteArray pcm32;
        appendLe32(pcm32, 0x8000'0000U);
        appendLe32(pcm32, 0xffff'ffffU);
        appendLe32(pcm32, 0U);
        appendLe32(pcm32, 0x7fff'0000U);
        QTest::newRow("signed-32")
            << 32 << pcm32 << QVector<short>({-32'768, -1, 0, 32'767});
    }

    void readsIntegerPcmDepths()
    {
        QFETCH(int, bits);
        QFETCH(QByteArray, data);
        QFETCH(QVector<short>, expected);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray wave = riff({
            {QByteArray("fmt ", 4),
             classicFormat(1U, 1U, 12'000U, static_cast<quint16>(bits))},
            {QByteArray("data", 4), data},
        });
        const QString path = writeFixture(
            directory, QStringLiteral("integer.wav"), wave);
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QVector<short> output;
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Chunk);
        QCOMPARE(output, expected);
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::End);
    }

    void readsFloatPcmAndClipsFiniteValues_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QVector<short>>("expected");

        QTest::newRow("float-32")
            << 32 << float32({-2.0F, -1.0F, 0.0F, 0.5F, 1.0F, 2.0F})
            << QVector<short>({-32'767, -32'767, 0, 16'384, 32'767,
                               32'767});
        QTest::newRow("float-64")
            << 64 << float64({-1.0, -0.5, 0.0, 0.5, 1.0})
            << QVector<short>({-32'767, -16'384, 0, 16'384, 32'767});
    }

    void readsFloatPcmAndClipsFiniteValues()
    {
        QFETCH(int, bits);
        QFETCH(QByteArray, data);
        QFETCH(QVector<short>, expected);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("float.wav"),
            riff({{QByteArray("fmt ", 4),
                   classicFormat(3U, 1U, 12'000U,
                                 static_cast<quint16>(bits))},
                  {QByteArray("data", 4), data}}));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QCOMPARE(reader.format().encoding, SstvWavSampleEncoding::IeeeFloat);
        QVector<short> output;
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Chunk);
        QCOMPARE(output, expected);
    }

    void acceptsExtensiblePcmDataBeforeFormatAndOddUnknownChunk()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("extensible.wav"),
            riff({{QByteArray("data", 4), pcm16({-123, 456})},
                  {QByteArray("JUNK", 4), QByteArray("abc", 3)},
                  {QByteArray("fmt ", 4), extensiblePcmFormat()}}));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QVERIFY(reader.format().extensible);
        QCOMPARE(reader.format().validBitsPerSample, quint16 {16U});
        QVector<short> output;
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Chunk);
        QCOMPARE(output, QVector<short>({-123, 456}));
    }

    void rejectsUnsafePaths()
    {
        SstvWavPcmReader reader;
        QString error;
        QVERIFY(!reader.open(QStringLiteral("relative.wav"), &error));
        QVERIFY(!error.isEmpty());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString target = writeFixture(
            directory, QStringLiteral("target.wav"),
            canonicalPcm16(pcm16({0, 1})));
        QVERIFY(!target.isEmpty());
        const QString link = directory.filePath(QStringLiteral("link.wav"));
        if (QFile::link(target, link)) {
            QVERIFY(QFileInfo(link).isSymLink());
            QVERIFY(!reader.open(link, &error));
            QVERIFY(error.contains(QStringLiteral("non-symbolic")));
        }
    }

    void rejectsMalformedRiffStructures_data()
    {
        QTest::addColumn<QByteArray>("wave");

        const QByteArray valid = canonicalPcm16(pcm16({0, 1}));

        QByteArray notRiff = valid;
        notRiff.replace(0, 4, "NOPE");
        QTest::newRow("not-riff") << notRiff;

        QByteArray notWave = valid;
        notWave.replace(8, 4, "NOPE");
        QTest::newRow("not-wave") << notWave;

        QByteArray sizeMismatch = valid;
        replaceLe32(sizeMismatch, 4,
                    static_cast<quint32>(sizeMismatch.size() - 9));
        QTest::newRow("riff-size-mismatch") << sizeMismatch;

        QTest::newRow("duplicate-format")
            << riff({{QByteArray("fmt ", 4),
                      classicFormat(1U, 1U, 12'000U, 16U)},
                     {QByteArray("fmt ", 4),
                      classicFormat(1U, 1U, 12'000U, 16U)},
                     {QByteArray("data", 4), pcm16({0})}});

        QTest::newRow("duplicate-data")
            << riff({{QByteArray("fmt ", 4),
                      classicFormat(1U, 1U, 12'000U, 16U)},
                     {QByteArray("data", 4), pcm16({0})},
                     {QByteArray("data", 4), pcm16({1})}});

        QByteArray escaping("RIFF", 4);
        QByteArray body("WAVEJUNK", 8);
        appendLe32(body, std::numeric_limits<quint32>::max());
        body.append(QByteArray(32, '\0'));
        appendLe32(escaping, static_cast<quint32>(body.size()));
        escaping.append(body);
        QTest::newRow("escaping-chunk") << escaping;

        QByteArray dangling("RIFF", 4);
        QByteArray danglingBody("WAVE", 4);
        danglingBody.append(QByteArray("JUNK", 4));
        appendLe32(danglingBody, 24U);
        danglingBody.append(QByteArray(24, '\0'));
        danglingBody.append(QByteArray("tail", 4));
        appendLe32(dangling, static_cast<quint32>(danglingBody.size()));
        dangling.append(danglingBody);
        QTest::newRow("truncated-next-chunk-header") << dangling;

        QByteArray fmt17 = classicFormat(1U, 1U, 12'000U, 16U);
        fmt17.append('\0');
        QTest::newRow("truncated-format-extension")
            << riff({{QByteArray("fmt ", 4), fmt17},
                     {QByteArray("data", 4), pcm16({0})}});

        QByteArray badExtension = classicFormat(1U, 1U, 12'000U, 16U);
        appendLe16(badExtension, 4U);
        badExtension.append('\0');
        badExtension.append('\0');
        QTest::newRow("bad-format-extension-size")
            << riff({{QByteArray("fmt ", 4), badExtension},
                     {QByteArray("data", 4), pcm16({0})}});
    }

    void rejectsMalformedRiffStructures()
    {
        QFETCH(QByteArray, wave);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("malformed.wav"), wave);
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY(!reader.open(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!reader.isOpen());
        QCOMPARE(reader.format().totalFrames, quint64 {0U});
    }

    void rejectsInconsistentOrUnsupportedFormats_data()
    {
        QTest::addColumn<QByteArray>("format");
        QTest::addColumn<QByteArray>("data");

        QTest::newRow("unsupported-codec")
            << classicFormat(7U, 1U, 12'000U, 16U) << pcm16({0});
        QTest::newRow("zero-channels")
            << classicFormat(1U, 0U, 12'000U, 16U) << pcm16({0});
        QTest::newRow("too-many-channels")
            << classicFormat(1U, 9U, 12'000U, 16U)
            << QByteArray(18, '\0');
        QTest::newRow("sample-rate-too-low")
            << classicFormat(1U, 1U, 7'999U, 16U) << pcm16({0});
        QTest::newRow("sample-rate-too-high")
            << classicFormat(1U, 1U, 192'001U, 16U) << pcm16({0});
        QTest::newRow("unsupported-bits")
            << classicFormat(1U, 1U, 12'000U, 12U) << pcm16({0});

        QByteArray badAlign = classicFormat(1U, 1U, 12'000U, 16U);
        replaceLe16(badAlign, 12, 4U);
        QTest::newRow("bad-block-align") << badAlign << pcm16({0, 1});

        QByteArray badRate = classicFormat(1U, 1U, 12'000U, 16U);
        replaceLe32(badRate, 8, 1U);
        QTest::newRow("bad-byte-rate") << badRate << pcm16({0});

        QTest::newRow("partial-frame")
            << classicFormat(1U, 1U, 12'000U, 16U)
            << QByteArray(3, '\0');

        QByteArray badExtensible = extensiblePcmFormat();
        badExtensible[24] = 0x07;
        QTest::newRow("bad-extensible-subtype")
            << badExtensible << pcm16({0});
    }

    void rejectsInconsistentOrUnsupportedFormats()
    {
        QFETCH(QByteArray, format);
        QFETCH(QByteArray, data);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("format.wav"),
            riff({{QByteArray("fmt ", 4), format},
                  {QByteArray("data", 4), data}}));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY(!reader.open(path, &error));
        QVERIFY(!error.isEmpty());
    }

    void appliesConfiguredDurationAndChunkLimits()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString durationPath = writeFixture(
            directory, QStringLiteral("duration.wav"),
            canonicalPcm16(QByteArray(48, '\0'), 1U, 12'000U));
        QVERIFY(!durationPath.isEmpty());

        SstvWavPcmReaderLimits durationLimits;
        durationLimits.maximumDurationMs = 1U;
        SstvWavPcmReader durationReader(durationLimits);
        QString error;
        QVERIFY(!durationReader.open(durationPath, &error));
        QVERIFY(error.contains(QStringLiteral("duration")));

        const QString chunksPath = writeFixture(
            directory, QStringLiteral("chunks.wav"),
            riff({{QByteArray("JUNK", 4), QByteArray(2, '\0')},
                  {QByteArray("fmt ", 4),
                   classicFormat(1U, 1U, 12'000U, 16U)},
                  {QByteArray("data", 4), pcm16({0})}}));
        QVERIFY(!chunksPath.isEmpty());
        SstvWavPcmReaderLimits chunkLimits;
        chunkLimits.maximumChunks = 2U;
        SstvWavPcmReader chunkReader(chunkLimits);
        QVERIFY(!chunkReader.open(chunksPath, &error));
        QVERIFY(error.contains(QStringLiteral("excessive")));
    }

    void rejectsNonFiniteFloatDuringStreaming()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const QString path = writeFixture(
            directory, QStringLiteral("nan.wav"),
            riff({{QByteArray("fmt ", 4),
                   classicFormat(3U, 1U, 12'000U, 32U)},
                  {QByteArray("data", 4), float32({0.0F, nan})}}));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QVector<short> output {99};
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Error);
        QVERIFY(output.isEmpty());
        QVERIFY(error.contains(QStringLiteral("Non-finite")));
        QCOMPARE(reader.framesRead(), quint64 {0U});
    }

    void cancellationIsStickyUntilReopen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("cancel.wav"),
            canonicalPcm16(pcm16({1, 2, 3})));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        reader.cancel();
        QVector<short> output {99};
        QCOMPARE(reader.readNext(&output, &error),
                 SstvWavReadStatus::Cancelled);
        QVERIFY(output.isEmpty());
        QCOMPARE(reader.readNext(&output, &error),
                 SstvWavReadStatus::Cancelled);

        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Chunk);
        QCOMPARE(output, QVector<short>({1, 2, 3}));
    }

    void detectsAudioTruncationAfterValidation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("truncate.wav"),
            canonicalPcm16(QByteArray(140'000, '\0')));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QFile mutator(path);
        QVERIFY(mutator.open(QIODevice::ReadWrite));
        QVERIFY(mutator.resize(
            static_cast<qint64>(reader.format().dataOffset + 2U)));
        mutator.close();

        QVector<short> output;
        QCOMPARE(reader.readNext(&output, &error), SstvWavReadStatus::Error);
        QVERIFY(output.isEmpty());
        QVERIFY(error.contains(QStringLiteral("ended unexpectedly")));
    }

    void nullOutputFailsWithoutAdvancing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = writeFixture(
            directory, QStringLiteral("null.wav"),
            canonicalPcm16(pcm16({1})));
        QVERIFY(!path.isEmpty());

        SstvWavPcmReader reader;
        QString error;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QCOMPARE(reader.readNext(nullptr, &error), SstvWavReadStatus::Error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(reader.framesRead(), quint64 {0U});
    }
};

QTEST_APPLESS_MAIN(TestSstvWavPcmReader)

#include "test_sstv_wav_pcm_reader.moc"
