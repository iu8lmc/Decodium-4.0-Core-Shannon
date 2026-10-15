// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/sharing/SstvIncomingMediaValidator.h"

#include <QBuffer>
#include <QColor>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace decodium::sstv::sharing;

namespace {

QString sha256(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        bytes, QCryptographicHash::Sha256).toHex());
}

QByteArray imageBytes(const char* format,
                      int width,
                      int height,
                      bool withMetadata = false,
                      const QColor& color = QColor(QStringLiteral("#316a97")))
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(color);
    if (withMetadata) {
        image.setText(QStringLiteral("Comment"),
                      QStringLiteral("private-location=35.8,14.5"));
    }
    QByteArray bytes;
    QBuffer output(&bytes);
    if (!output.open(QIODevice::WriteOnly)
        || !image.save(&output, format)) {
        return {};
    }
    return bytes;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size() && file.flush();
}

SstvShareManifestV1 manifestFor(const QByteArray& bytes,
                                const QString& mime,
                                quint32 width,
                                quint32 height)
{
    const QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-08-24T12:00:00.000Z"),
        Qt::ISODateWithMs).toUTC();
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid::createUuid();
    manifest.providerId = QStringLiteral("rest-test");
    manifest.senderId = QStringLiteral("station:remote");
    manifest.recipientId = QStringLiteral("station:local");
    manifest.createdUtc = now.addSecs(-30);
    manifest.expiresUtc = now.addDays(1);
    manifest.mediaUtc = now.addSecs(-60);
    manifest.originalFilename = mime == QStringLiteral("image/png")
        ? QStringLiteral("received.png") : QStringLiteral("received.jpg");
    manifest.safeDisplayFilename = manifest.originalFilename;
    manifest.mimeType = mime;
    manifest.byteSize = static_cast<quint64>(bytes.size());
    manifest.sha256 = sha256(bytes);
    manifest.width = width;
    manifest.height = height;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.source = SstvShareMediaSource::AnalogReception;
    manifest.completion = SstvShareContentCompletion::Complete;
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

quint32 pngCrc32(const QByteArray& bytes)
{
    quint32 crc = 0xffffffffU;
    for (const char character : bytes) {
        crc ^= static_cast<quint8>(character);
        for (int bit = 0; bit < 8; ++bit) {
            const quint32 mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return crc ^ 0xffffffffU;
}

void writeBigEndian32(QByteArray& bytes, qsizetype offset, quint32 value)
{
    bytes[offset] = static_cast<char>((value >> 24U) & 0xffU);
    bytes[offset + 1] = static_cast<char>((value >> 16U) & 0xffU);
    bytes[offset + 2] = static_cast<char>((value >> 8U) & 0xffU);
    bytes[offset + 3] = static_cast<char>(value & 0xffU);
}

QByteArray dimensionBombPng()
{
    QByteArray bytes = imageBytes("PNG", 4, 4);
    if (bytes.size() < 33 || bytes.mid(12, 4) != QByteArrayLiteral("IHDR")) {
        return {};
    }
    writeBigEndian32(bytes, 16, 8'192U);
    writeBigEndian32(bytes, 20, 8'192U);
    writeBigEndian32(bytes, 29, pngCrc32(bytes.mid(12, 17)));
    return bytes;
}

} // namespace

class TestSstvIncomingMediaValidator final : public QObject
{
    Q_OBJECT

private slots:
    void validatesNormalizesAndReconstructsRestartHandoff()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray sourceBytes = imageBytes("PNG", 64, 48, true);
        QVERIFY(!sourceBytes.isEmpty());
        const QString source = temporary.filePath(QStringLiteral("source.png"));
        const QString staging = temporary.filePath(QStringLiteral("validated"));
        QVERIFY(writeBytes(source, sourceBytes));
        QImageReader sourceReader(source);
        QVERIFY(sourceReader.textKeys().contains(QStringLiteral("Comment")));
        const auto manifest = manifestFor(
            sourceBytes, QStringLiteral("image/png"), 64U, 48U);
        const QString transferId = QUuid::createUuid().toString(
            QUuid::WithoutBraces);
        const QDateTime received = manifest.createdUtc.addSecs(10);

        const auto first = validateAndStageIncomingMedia(
            source, staging, transferId, QStringLiteral("incoming:one"),
            manifest, received, manifest.expiresUtc);
        QVERIFY2(first.ok(), qPrintable(first.redactedDiagnostic));
        QCOMPARE(first.handoff->schemaVersion, 1);
        QCOMPARE(first.handoff->sstvMode, manifest.sstvMode);
        QCOMPARE(first.handoff->sourceSha256, manifest.sha256);
        QCOMPARE(first.handoff->sourceMimeType, QStringLiteral("image/png"));
        QCOMPARE(first.handoff->stagedMimeType, QStringLiteral("image/png"));
        QCOMPARE(first.handoff->width, 64U);
        QCOMPARE(first.handoff->height, 48U);
        QVERIFY(QFileInfo::exists(first.handoff->stagedCanonicalPath));
        QImageReader normalized(first.handoff->stagedCanonicalPath);
        QCOMPARE(normalized.format().toLower(), QByteArrayLiteral("png"));
        QVERIFY(normalized.textKeys().isEmpty());
        const auto permissions = QFileInfo(
            first.handoff->stagedCanonicalPath).permissions();
        QVERIFY(!(permissions & QFileDevice::ReadGroup));
        QVERIFY(!(permissions & QFileDevice::ReadOther));
        QVERIFY(!(permissions & QFileDevice::WriteGroup));
        QVERIFY(!(permissions & QFileDevice::WriteOther));

        const auto restarted = inspectStagedIncomingMedia(
            first.handoff->stagedCanonicalPath, staging, transferId,
            QStringLiteral("incoming:one"), manifest, received,
            manifest.expiresUtc);
        QVERIFY2(restarted.ok(), qPrintable(restarted.redactedDiagnostic));
        QCOMPARE(restarted.handoff->stagedSha256,
                 first.handoff->stagedSha256);
        QCOMPARE(restarted.handoff->stagedCanonicalPath,
                 first.handoff->stagedCanonicalPath);
        const auto idempotent = validateAndStageIncomingMedia(
            source, staging, transferId, QStringLiteral("incoming:one"),
            manifest, received, manifest.expiresUtc);
        QVERIFY2(idempotent.ok(), qPrintable(idempotent.redactedDiagnostic));
        QCOMPARE(idempotent.handoff->stagedSha256,
                 first.handoff->stagedSha256);

        const QByteArray unrelated = imageBytes(
            "PNG", 64, 48, false, QColor(QStringLiteral("#9b421f")));
        QVERIFY(!unrelated.isEmpty());
        QVERIFY(sha256(unrelated) != first.handoff->stagedSha256);
        QVERIFY(writeBytes(first.handoff->stagedCanonicalPath, unrelated));
        const auto rebuilt = validateAndStageIncomingMedia(
            source, staging, transferId, QStringLiteral("incoming:one"),
            manifest, received, manifest.expiresUtc);
        QVERIFY2(rebuilt.ok(), qPrintable(rebuilt.redactedDiagnostic));
        QCOMPARE(rebuilt.handoff->stagedSha256,
                 first.handoff->stagedSha256);

        QFile corrupted(rebuilt.handoff->stagedCanonicalPath);
        QVERIFY(corrupted.open(QIODevice::ReadWrite));
        QByteArray stagedBytes = corrupted.readAll();
        const qsizetype idat = stagedBytes.indexOf(QByteArrayLiteral("IDAT"));
        QVERIFY(idat >= 0 && idat + 4 < stagedBytes.size());
        stagedBytes[idat + 4] = static_cast<char>(
            static_cast<quint8>(stagedBytes.at(idat + 4)) ^ 0xffU);
        QVERIFY(corrupted.seek(0));
        QCOMPARE(corrupted.write(stagedBytes), stagedBytes.size());
        QVERIFY(corrupted.resize(stagedBytes.size()));
        corrupted.close();
        const auto corruptedRestart = inspectStagedIncomingMedia(
            rebuilt.handoff->stagedCanonicalPath, staging, transferId,
            QStringLiteral("incoming:one"), manifest, received,
            manifest.expiresUtc);
        QVERIFY(!corruptedRestart.ok());
        QCOMPARE(corruptedRestart.failure,
                 SstvShareProviderFailure::Integrity);
    }

    void rejectsHashMimeMagicFilenameAndAllocationBombs()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString staging = temporary.filePath(QStringLiteral("validated"));
        const QString transferId = QUuid::createUuid().toString(
            QUuid::WithoutBraces);

        const QByteArray png = imageBytes("PNG", 32, 24);
        const QString pngPath = temporary.filePath(QStringLiteral("normal.png"));
        QVERIFY(writeBytes(pngPath, png));
        auto manifest = manifestFor(
            png, QStringLiteral("image/png"), 32U, 24U);
        const QDateTime received = manifest.createdUtc.addSecs(10);
        manifest.sha256.fill(QLatin1Char('0'));
        auto result = validateAndStageIncomingMedia(
            pngPath, staging, transferId, QStringLiteral("incoming:hash"),
            manifest, received, manifest.expiresUtc);
        QVERIFY(!result.ok());
        QCOMPARE(result.failure, SstvShareProviderFailure::Integrity);

        const QByteArray jpeg = imageBytes("JPEG", 32, 24);
        const QString jpegPath = temporary.filePath(QStringLiteral("wrong.png"));
        QVERIFY(writeBytes(jpegPath, jpeg));
        manifest = manifestFor(
            jpeg, QStringLiteral("image/png"), 32U, 24U);
        result = validateAndStageIncomingMedia(
            jpegPath, staging, transferId, QStringLiteral("incoming:mime"),
            manifest, received, manifest.expiresUtc);
        QVERIFY(!result.ok());
        QCOMPARE(result.failure, SstvShareProviderFailure::Validation);

        manifest = manifestFor(
            png, QStringLiteral("image/png"), 32U, 24U);
        manifest.safeDisplayFilename = QStringLiteral("../escape.png");
        result = validateAndStageIncomingMedia(
            pngPath, staging, transferId, QStringLiteral("incoming:path"),
            manifest, received, manifest.expiresUtc);
        QVERIFY(!result.ok());
        QCOMPARE(result.failure, SstvShareProviderFailure::Validation);

        const QByteArray bomb = dimensionBombPng();
        QVERIFY(!bomb.isEmpty());
        const QString bombPath = temporary.filePath(QStringLiteral("bomb.png"));
        QVERIFY(writeBytes(bombPath, bomb));
        manifest = manifestFor(
            bomb, QStringLiteral("image/png"), 32U, 24U);
        result = validateAndStageIncomingMedia(
            bombPath, staging, transferId, QStringLiteral("incoming:bomb"),
            manifest, received, manifest.expiresUtc);
        QVERIFY(!result.ok());
        QCOMPARE(result.failure, SstvShareProviderFailure::Validation);

        const QByteArray large = imageBytes("PNG", 512, 512);
        const QString largePath = temporary.filePath(QStringLiteral("large.png"));
        QVERIFY(writeBytes(largePath, large));
        manifest = manifestFor(
            large, QStringLiteral("image/png"), 512U, 512U);
        SstvIncomingMediaLimits tight;
        tight.maximumDecodeAllocationBytes = 1024U * 1024U;
        result = validateAndStageIncomingMedia(
            largePath, staging, transferId, QStringLiteral("incoming:alloc"),
            manifest, received, manifest.expiresUtc, tight);
        QVERIFY(!result.ok());
        QCOMPARE(result.failure, SstvShareProviderFailure::Validation);
    }
};

QTEST_GUILESS_MAIN(TestSstvIncomingMediaValidator)

#include "test_sstv_incoming_media_validator.moc"
