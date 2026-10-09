// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvIncomingMediaValidator.h"

#include "SstvShareSecurity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace decodium::sstv::sharing {
namespace {

constexpr qint64 kHashBlockBytes = 256 * 1024;
constexpr quint64 kBytesPerMebibyte = 1024U * 1024U;

SstvIncomingMediaValidationResult failure(
    SstvShareProviderFailure category,
    const QString& diagnostic)
{
    SstvIncomingMediaValidationResult result;
    result.failure = category;
    result.redactedDiagnostic = redactShareSecrets(diagnostic).left(512);
    return result;
}

bool canonicalUuid(const QString& value)
{
    const QUuid uuid(value);
    return !uuid.isNull() && value.size() == 36 && value == value.toLower()
        && uuid.toString(QUuid::WithoutBraces) == value;
}

bool validLimits(const SstvIncomingMediaLimits& limits)
{
    return limits.maximumSourceBytes > 0U
        && limits.maximumSourceBytes <= kMaximumSharedImageBytes
        && limits.maximumDimension > 0U
        && limits.maximumDimension <= kMaximumSharedImageDimension
        && limits.maximumPixels > 0U
        && limits.maximumPixels <= kMaximumSharedImagePixels
        && limits.maximumDecodeAllocationBytes >= kBytesPerMebibyte
        && limits.maximumDecodeAllocationBytes
            <= static_cast<quint64>(std::numeric_limits<qint64>::max())
        && limits.maximumStagedBytes > 0U
        && limits.maximumStagedBytes <= kMaximumSharedImageBytes;
}

bool pathInside(const QString& canonicalRoot, const QString& path)
{
    const QFileInfo rootInfo(canonicalRoot);
    const QFileInfo pathInfo(path);
    if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()
        || !pathInfo.exists() || !pathInfo.isFile() || pathInfo.isSymLink()) {
        return false;
    }
    const QString root = QDir::cleanPath(rootInfo.canonicalFilePath());
    const QString candidate = QDir::cleanPath(pathInfo.canonicalFilePath());
    if (root.isEmpty() || candidate.isEmpty()) {
        return false;
    }
    const QString relative = QDir(root).relativeFilePath(candidate);
    return relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(relative);
}

bool ensurePrivateDirectory(const QString& path)
{
    const QFileInfo before(path);
    if ((before.exists() && (!before.isDir() || before.isSymLink()))
        || (!before.exists() && !QDir().mkpath(path))) {
        return false;
    }
    const QFileInfo after(path);
    return after.exists() && after.isDir() && !after.isSymLink()
        && !after.canonicalFilePath().isEmpty()
        && QFile::setPermissions(path, QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner);
}

std::optional<QString> hashFile(const QString& path,
                                quint64 maximumBytes,
                                quint64* bytes)
{
    QFile file(path);
    if (!bytes || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash digest(QCryptographicHash::Sha256);
    quint64 total = 0U;
    while (!file.atEnd()) {
        const QByteArray block = file.read(kHashBlockBytes);
        if (block.isEmpty() && file.error() != QFileDevice::NoError) {
            return {};
        }
        if (static_cast<quint64>(block.size()) > maximumBytes - total) {
            return {};
        }
        total += static_cast<quint64>(block.size());
        digest.addData(block);
    }
    *bytes = total;
    return QString::fromLatin1(digest.result().toHex());
}

bool magicMatchesMime(const QByteArray& prefix, const QString& mimeType)
{
    static const QByteArray pngMagic = QByteArray::fromHex(
        QByteArrayLiteral("89504e470d0a1a0a"));
    if (mimeType == QStringLiteral("image/png")) {
        return prefix.startsWith(pngMagic);
    }
    if (mimeType == QStringLiteral("image/jpeg")) {
        return prefix.size() >= 3
            && static_cast<quint8>(prefix.at(0)) == 0xffU
            && static_cast<quint8>(prefix.at(1)) == 0xd8U
            && static_cast<quint8>(prefix.at(2)) == 0xffU;
    }
    return false;
}

bool dimensionsWithin(const QSize& size,
                      const SstvIncomingMediaLimits& limits)
{
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0
        || static_cast<quint32>(size.width()) > limits.maximumDimension
        || static_cast<quint32>(size.height()) > limits.maximumDimension) {
        return false;
    }
    const quint64 width = static_cast<quint64>(size.width());
    const quint64 height = static_cast<quint64>(size.height());
    return width <= limits.maximumPixels / height;
}

bool decodeBudgetAllows(const QSize& size,
                        const SstvIncomingMediaLimits& limits)
{
    const int readerLimitMb = QImageReader::allocationLimit();
    if (readerLimitMb <= 0 || !dimensionsWithin(size, limits)) {
        return false;
    }
    const quint64 qtLimit = static_cast<quint64>(readerLimitMb)
        * kBytesPerMebibyte;
    const quint64 effective = std::min(
        qtLimit, limits.maximumDecodeAllocationBytes);
    const quint64 pixels = static_cast<quint64>(size.width())
        * static_cast<quint64>(size.height());
    constexpr quint64 worstCaseBytesPerPixel = 8U;
    return pixels <= effective / worstCaseBytesPerPixel;
}

SstvIncomingMediaValidationResult buildHandoff(
    const QString& stagedPath,
    const QString& privateStagingRoot,
    const QString& localTransferId,
    const QString& incomingId,
    const SstvShareManifestV1& manifest,
    const QDateTime& receivedUtc,
    const QDateTime& expiresUtc,
    const SstvIncomingMediaLimits& limits)
{
    if (!pathInside(privateStagingRoot, stagedPath)) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("validated incoming staging path is unsafe"));
    }
    const QFileInfo info(stagedPath);
    if (info.size() <= 0
        || static_cast<quint64>(info.size()) > limits.maximumStagedBytes) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("validated incoming PNG exceeded its byte bound"));
    }
    const auto permissions = info.permissions();
    const auto requiredPermissions = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner;
    const auto forbiddenPermissions = QFileDevice::ExeOwner
        | QFileDevice::ReadGroup | QFileDevice::WriteGroup
        | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther
        | QFileDevice::ExeOther;
    if ((permissions & requiredPermissions) != requiredPermissions
        || (permissions & forbiddenPermissions)) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("validated incoming PNG permissions are unsafe"));
    }
    QFile prefixFile(stagedPath);
    if (!prefixFile.open(QIODevice::ReadOnly)
        || !magicMatchesMime(prefixFile.read(8), QStringLiteral("image/png"))) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("validated incoming staging magic changed"));
    }
    prefixFile.close();

    QImageReader reader(stagedPath, QByteArrayLiteral("png"));
    reader.setAutoDetectImageFormat(false);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(false);
    const QSize size = reader.size();
    if (reader.format().toLower() != QByteArrayLiteral("png")
        || !decodeBudgetAllows(size, limits)
        || size.width() != static_cast<int>(manifest.width)
        || size.height() != static_cast<int>(manifest.height)
        || reader.supportsAnimation() || reader.imageCount() > 1
        || !reader.textKeys().isEmpty()) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("validated incoming PNG metadata or dimensions are invalid"));
    }
    const QImage decoded = reader.read();
    if (decoded.isNull() || decoded.size() != size
        || static_cast<quint64>(decoded.sizeInBytes())
            > limits.maximumDecodeAllocationBytes) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("validated incoming PNG could not be fully decoded"));
    }
    quint64 stagedBytes = 0U;
    const auto stagedHash = hashFile(
        stagedPath, limits.maximumStagedBytes, &stagedBytes);
    if (!stagedHash || stagedBytes != static_cast<quint64>(info.size())) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("validated incoming PNG could not be hashed"));
    }

    SstvValidatedIncomingHandoff handoff;
    handoff.transferId = localTransferId;
    handoff.providerId = manifest.providerId;
    handoff.incomingId = incomingId;
    handoff.senderId = manifest.senderId;
    handoff.safeDisplayFilename = manifest.safeDisplayFilename;
    handoff.sstvMode = manifest.sstvMode;
    handoff.sourceMimeType = manifest.mimeType;
    handoff.sourceSha256 = manifest.sha256;
    handoff.sourceByteSize = manifest.byteSize;
    handoff.stagedCanonicalPath = info.canonicalFilePath();
    handoff.stagedSha256 = *stagedHash;
    handoff.stagedByteSize = stagedBytes;
    handoff.width = manifest.width;
    handoff.height = manifest.height;
    handoff.receivedUtc = receivedUtc;
    handoff.expiresUtc = expiresUtc;
    SstvIncomingMediaValidationResult result;
    result.handoff = std::move(handoff);
    result.failure = SstvShareProviderFailure::None;
    return result;
}

bool commonInputValid(const QString& privateStagingRoot,
                      const QString& localTransferId,
                      const QString& incomingId,
                      const SstvShareManifestV1& manifest,
                      const QDateTime& receivedUtc,
                      const QDateTime& expiresUtc,
                      const SstvIncomingMediaLimits& limits)
{
    return validLimits(limits) && canonicalUuid(localTransferId)
        && isSafeShareIdentifier(incomingId)
        && manifest.validate(false).ok()
        && isSafeShareFilename(manifest.originalFilename)
        && isSafeShareFilename(manifest.safeDisplayFilename)
        && manifest.byteSize <= limits.maximumSourceBytes
        && manifest.width <= limits.maximumDimension
        && manifest.height <= limits.maximumDimension
        && static_cast<quint64>(manifest.width)
            <= limits.maximumPixels / manifest.height
        && receivedUtc.isValid() && receivedUtc.offsetFromUtc() == 0
        && expiresUtc.isValid() && expiresUtc.offsetFromUtc() == 0
        && expiresUtc == manifest.expiresUtc && expiresUtc > receivedUtc
        && !privateStagingRoot.isEmpty();
}

} // namespace

SstvIncomingMediaValidationResult validateAndStageIncomingMedia(
    const QString& sourcePath,
    const QString& privateStagingRoot,
    const QString& localTransferId,
    const QString& incomingId,
    const SstvShareManifestV1& manifest,
    const QDateTime& receivedUtc,
    const QDateTime& expiresUtc,
    const SstvIncomingMediaLimits& limits)
{
    if (!commonInputValid(privateStagingRoot, localTransferId, incomingId,
                          manifest, receivedUtc, expiresUtc, limits)) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("incoming media validation request is invalid"));
    }
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile() || sourceInfo.isSymLink()
        || sourceInfo.size() <= 0
        || static_cast<quint64>(sourceInfo.size()) != manifest.byteSize) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("incoming source is not the declared regular file"));
    }
    quint64 sourceBytes = 0U;
    const auto sourceHash = hashFile(
        sourcePath, limits.maximumSourceBytes, &sourceBytes);
    if (!sourceHash || sourceBytes != manifest.byteSize
        || *sourceHash != manifest.sha256) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("incoming source SHA-256 did not match its manifest"));
    }
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("incoming source could not be opened"));
    }
    const QByteArray prefix = source.peek(8);
    if (!magicMatchesMime(prefix, manifest.mimeType)) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("incoming source magic and MIME do not match"));
    }

    QImageReader reader(&source);
    reader.setAutoDetectImageFormat(true);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(false);
    const QByteArray expectedFormat = manifest.mimeType == QStringLiteral("image/png")
        ? QByteArrayLiteral("png") : QByteArrayLiteral("jpeg");
    const QByteArray detectedFormat = reader.format().toLower();
    const QSize advertisedSize = reader.size();
    if ((detectedFormat != expectedFormat
         && !(expectedFormat == QByteArrayLiteral("jpeg")
              && detectedFormat == QByteArrayLiteral("jpg")))
        || !decodeBudgetAllows(advertisedSize, limits)
        || advertisedSize.width() != static_cast<int>(manifest.width)
        || advertisedSize.height() != static_cast<int>(manifest.height)
        || reader.supportsAnimation() || reader.imageCount() > 1) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("incoming image header exceeded its format or allocation limits"));
    }
    const QImage decoded = reader.read();
    if (decoded.isNull() || decoded.size() != advertisedSize
        || static_cast<quint64>(decoded.sizeInBytes())
            > limits.maximumDecodeAllocationBytes) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("incoming image could not be decoded within its allocation cap"));
    }

    QImage sanitized(decoded.size(), QImage::Format_RGB888);
    if (sanitized.isNull()
        || static_cast<quint64>(sanitized.sizeInBytes())
            > limits.maximumDecodeAllocationBytes) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("incoming sanitized pixels exceeded their allocation cap"));
    }
    sanitized.fill(Qt::black);
    QPainter painter(&sanitized);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(QPoint(0, 0), decoded);
    painter.end();

    if (!ensurePrivateDirectory(privateStagingRoot)) {
        return failure(SstvShareProviderFailure::PermanentProviderFailure,
                       QStringLiteral("private incoming staging directory is unavailable"));
    }
    const QString canonicalRoot = QFileInfo(privateStagingRoot).canonicalFilePath();
    const QString stagedPath = QDir(canonicalRoot).absoluteFilePath(
        localTransferId + QStringLiteral(".png"));
    const QFileInfo existing(stagedPath);
    if (existing.isSymLink()
        || (existing.exists() && !existing.isFile())) {
        return failure(SstvShareProviderFailure::Integrity,
                       QStringLiteral("incoming sanitized PNG target is unsafe"));
    }
    QSaveFile output(stagedPath);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)
        || !output.setPermissions(QFileDevice::ReadOwner
                                  | QFileDevice::WriteOwner)
        || !sanitized.save(&output, "PNG") || !output.commit()) {
        output.cancelWriting();
        return failure(SstvShareProviderFailure::PermanentProviderFailure,
                       QStringLiteral("incoming sanitized PNG could not be staged atomically"));
    }
    return buildHandoff(stagedPath, canonicalRoot, localTransferId,
                        incomingId, manifest, receivedUtc, expiresUtc, limits);
}

SstvIncomingMediaValidationResult inspectStagedIncomingMedia(
    const QString& stagedPath,
    const QString& privateStagingRoot,
    const QString& localTransferId,
    const QString& incomingId,
    const SstvShareManifestV1& manifest,
    const QDateTime& receivedUtc,
    const QDateTime& expiresUtc,
    const SstvIncomingMediaLimits& limits)
{
    if (!commonInputValid(privateStagingRoot, localTransferId, incomingId,
                          manifest, receivedUtc, expiresUtc, limits)) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("incoming handoff inspection request is invalid"));
    }
    return buildHandoff(stagedPath, privateStagingRoot, localTransferId,
                        incomingId, manifest, receivedUtc, expiresUtc, limits);
}

} // namespace decodium::sstv::sharing
