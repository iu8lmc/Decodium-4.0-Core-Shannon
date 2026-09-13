// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvShareManifest.h"
#include "SstvShareProvider.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>

#include <optional>

namespace decodium::sstv::sharing {

// This is the complete, storage-schema-independent result handed to the
// native Gallery importer. The staged file is a newly encoded metadata-free
// PNG in Decodium's private sharing area; source* fields still bind it to the
// exact remote payload and manifest that were verified before decoding.
struct SstvValidatedIncomingHandoff final
{
    int schemaVersion {1};
    QString transferId;
    QString providerId;
    QString incomingId;
    QString senderId;
    QString safeDisplayFilename;
    QString sstvMode;
    QString sourceMimeType;
    QString sourceSha256;
    quint64 sourceByteSize {0U};
    QString stagedCanonicalPath;
    QString stagedMimeType {QStringLiteral("image/png")};
    QString stagedSha256;
    quint64 stagedByteSize {0U};
    quint32 width {0U};
    quint32 height {0U};
    QDateTime receivedUtc;
    QDateTime expiresUtc;
};

struct SstvIncomingMediaLimits final
{
    quint64 maximumSourceBytes {kMaximumSharedImageBytes};
    quint32 maximumDimension {kMaximumSharedImageDimension};
    quint64 maximumPixels {kMaximumSharedImagePixels};
    // QImageReader has its own process-wide allocation limit. This local
    // ceiling is additionally checked using an eight-byte-per-pixel worst
    // case before decode and against QImage::sizeInBytes() afterwards.
    quint64 maximumDecodeAllocationBytes {128ULL * 1024ULL * 1024ULL};
    quint64 maximumStagedBytes {kMaximumSharedImageBytes};
};

struct SstvIncomingMediaValidationResult final
{
    std::optional<SstvValidatedIncomingHandoff> handoff;
    SstvShareProviderFailure failure {SstvShareProviderFailure::Validation};
    QString redactedDiagnostic;

    bool ok() const noexcept { return handoff.has_value(); }
};

SstvIncomingMediaValidationResult validateAndStageIncomingMedia(
    const QString& sourcePath,
    const QString& privateStagingRoot,
    const QString& localTransferId,
    const QString& incomingId,
    const SstvShareManifestV1& manifest,
    const QDateTime& receivedUtc,
    const QDateTime& expiresUtc,
    const SstvIncomingMediaLimits& limits = {});

// Revalidates an already normalized file and reconstructs the precise
// handoff. This is used after restart and immediately before accept/import.
SstvIncomingMediaValidationResult inspectStagedIncomingMedia(
    const QString& stagedPath,
    const QString& privateStagingRoot,
    const QString& localTransferId,
    const QString& incomingId,
    const SstvShareManifestV1& manifest,
    const QDateTime& receivedUtc,
    const QDateTime& expiresUtc,
    const SstvIncomingMediaLimits& limits = {});

} // namespace decodium::sstv::sharing

Q_DECLARE_METATYPE(decodium::sstv::sharing::SstvValidatedIncomingHandoff)
