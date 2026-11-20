// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/sharing/SstvIncomingMediaValidator.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace decodium::sstv::sharing;

namespace {

constexpr std::size_t kMaximumFuzzInputBytes = 1024U * 1024U;
constexpr char kTransferId[] = "2e6492b8-f383-4aa4-b953-173ba74dfdea";

struct ScratchFiles final
{
    QTemporaryDir directory;
    QString sourcePath {directory.filePath(QStringLiteral("candidate.image"))};
    QString stagingRoot {directory.filePath(QStringLiteral("staging"))};
};

QByteArray decodedInput(const std::uint8_t* data, std::size_t size)
{
    const char* const bytes = size == 0U
        ? "" : reinterpret_cast<const char*>(data);
    QByteArray input(bytes, static_cast<qsizetype>(size));
    // Keep a readable corpus in Git without changing what the validator sees.
    // Raw binary inputs remain the normal libFuzzer mutation path.
    if (input.startsWith("hex:")) {
        input = QByteArray::fromHex(input.mid(4).trimmed());
    }
    return input;
}

bool isJpeg(const QByteArray& input)
{
    return input.size() >= 3
        && static_cast<quint8>(input.at(0)) == 0xffU
        && static_cast<quint8>(input.at(1)) == 0xd8U
        && static_cast<quint8>(input.at(2)) == 0xffU;
}

SstvShareManifestV1 manifestFor(const QByteArray& input)
{
    const bool jpeg = isJpeg(input);
    const QDateTime created = QDateTime::fromString(
        QStringLiteral("2026-08-24T10:00:00.000Z"), Qt::ISODateWithMs)
                                  .toUTC();
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid(QString::fromLatin1(kTransferId));
    manifest.providerId = QStringLiteral("fuzz");
    manifest.senderId = QStringLiteral("station:fuzz");
    manifest.recipientId = QStringLiteral("recipient:fuzz");
    manifest.createdUtc = created;
    manifest.expiresUtc = created.addDays(1);
    manifest.mediaUtc = created.addSecs(-1);
    manifest.originalFilename = jpeg
        ? QStringLiteral("candidate.jpg") : QStringLiteral("candidate.png");
    manifest.safeDisplayFilename = manifest.originalFilename;
    manifest.mimeType = jpeg
        ? QStringLiteral("image/jpeg") : QStringLiteral("image/png");
    manifest.byteSize = static_cast<quint64>(input.size());
    manifest.sha256 = QString::fromLatin1(QCryptographicHash::hash(
        input, QCryptographicHash::Sha256).toHex());
    // The committed 1x1 PNG corpus seed reaches the full decode/normalisation
    // path. Mutating its container dimensions exercises the manifest/image
    // consistency and allocation checks without trusting input dimensions.
    manifest.width = 1U;
    manifest.height = 1U;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

bool writeCandidate(const QString& path, const QByteArray& input)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(input) == input.size() && file.flush();
}

[[noreturn]] void invariantFailure()
{
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size)
{
    if ((!data && size != 0U) || size > kMaximumFuzzInputBytes) {
        return 0;
    }

    static ScratchFiles scratch;
    if (!scratch.directory.isValid()) {
        return 0;
    }
    const QByteArray input = decodedInput(data, size);
    if (input.isEmpty()
        || input.size() > static_cast<qsizetype>(kMaximumFuzzInputBytes)
        || !writeCandidate(scratch.sourcePath, input)) {
        return 0;
    }

    const SstvShareManifestV1 manifest = manifestFor(input);
    const QDateTime received = manifest.createdUtc.addSecs(1);
    const auto result = validateAndStageIncomingMedia(
        scratch.sourcePath, scratch.stagingRoot,
        QString::fromLatin1(kTransferId), QStringLiteral("incoming:fuzz"),
        manifest, received, manifest.expiresUtc);
    if (!result.ok()) {
        return 0;
    }

    const auto& handoff = *result.handoff;
    if (handoff.stagedMimeType != QStringLiteral("image/png")
        || handoff.width != manifest.width || handoff.height != manifest.height
        || handoff.stagedByteSize == 0U
        || handoff.stagedCanonicalPath.isEmpty()
        || !QFileInfo::exists(handoff.stagedCanonicalPath)) {
        invariantFailure();
    }
    const auto restarted = inspectStagedIncomingMedia(
        handoff.stagedCanonicalPath, scratch.stagingRoot,
        QString::fromLatin1(kTransferId), QStringLiteral("incoming:fuzz"),
        manifest, received, manifest.expiresUtc);
    if (!restarted.ok()
        || restarted.handoff->stagedSha256 != handoff.stagedSha256
        || restarted.handoff->stagedCanonicalPath != handoff.stagedCanonicalPath) {
        invariantFailure();
    }
    return 0;
}
