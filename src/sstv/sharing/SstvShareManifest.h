// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvShareSecurity.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <cstdint>
#include <optional>

namespace decodium::sstv::sharing {

inline constexpr int kSstvShareManifestProtocolVersion = 1;
inline constexpr quint64 kMaximumSharedImageBytes = 64ULL * 1024ULL * 1024ULL;
inline constexpr quint32 kMaximumSharedImageDimension = 8'192U;
inline constexpr quint64 kMaximumSharedImagePixels = 16'777'216ULL;
inline constexpr quint32 kMaximumShareChunks = 4'096U;
inline constexpr qint64 kMaximumShareLifetimeSeconds = 30LL * 24LL * 60LL * 60LL;

enum class SstvShareMediaSource
{
    AnalogReception,
    AnalogTransmission,
    DigitalReception,
    DigitalTransmission,
};

enum class SstvShareContentCompletion
{
    Complete,
    Partial,
};

enum class SstvShareContentDisposition
{
    Attachment,
    InlinePreview,
};

enum class SstvShareEncryptionMode
{
    TransportTls,
    EndToEnd,
};

struct SstvShareEncryptionInfo final
{
    SstvShareEncryptionMode mode {SstvShareEncryptionMode::TransportTls};
    QString algorithm {QStringLiteral("none")};
    QString keyId;
    QString recipientKeyFingerprint;
    QString nonceBase64;
    bool manifestBoundAsAuthenticatedData {false};
    bool downgradeProtected {true};
};

struct SstvShareTransportSecurity final
{
    bool tlsRequired {true};
    bool certificateValidationRequired {true};
    bool sameOriginRedirectsOnly {true};
    bool providerCanReadContent {true};
};

struct SstvSharePrivacyFlags final
{
    bool automaticUploadAllowed {false};
    bool automaticIncomingDownloadAllowed {false};
    bool locationIncluded {false};
    bool exifRetained {false};
    bool callsignIncluded {false};
    bool gridIncluded {false};
    bool publicShare {false};
    bool recipientConfirmed {false};
    bool meteredNetworkAllowed {false};
    bool explicitExpiry {true};
};

struct SstvShareCallsignMetadata final
{
    QString senderCallsign;
    QString remoteCallsign;
    QString grid;
};

struct SstvShareManifestV1 final
{
    int protocolVersion {kSstvShareManifestProtocolVersion};
    QUuid transferId;
    QString providerId;
    QString senderId;
    QString recipientId;
    QDateTime createdUtc;
    QDateTime expiresUtc;
    QString originalFilename;
    QString safeDisplayFilename;
    QString mimeType;
    // These describe the exact bounded payload handed to the provider. For an
    // E2EE transfer that payload is the authenticated ciphertext envelope;
    // plaintext integrity is supplied by the audited AEAD implementation.
    quint64 byteSize {0U};
    QString sha256;
    quint32 width {0U};
    quint32 height {0U};
    QString sstvMode;
    SstvShareMediaSource source {SstvShareMediaSource::AnalogReception};
    QDateTime mediaUtc;
    SstvShareCallsignMetadata callsign;
    SstvShareContentCompletion completion {SstvShareContentCompletion::Complete};
    QString message;
    SstvShareEncryptionInfo encryption;
    quint32 chunkCount {1U};
    SstvShareContentDisposition disposition {SstvShareContentDisposition::Attachment};
    SstvSharePrivacyFlags privacy;
    SstvShareTransportSecurity transport;

    SstvShareValidationError validate(bool forTransmission = false,
                                      QDateTime nowUtc = {}) const;
    QJsonObject toJsonObject() const;
    QByteArray toCanonicalJson(SstvShareValidationError* error = nullptr) const;
};

struct SstvShareManifestParseResult final
{
    std::optional<SstvShareManifestV1> manifest;
    SstvShareValidationError error;

    bool ok() const noexcept { return manifest.has_value() && error.ok(); }
};

SstvShareManifestParseResult parseSstvShareManifestV1(
    const QByteArray& json,
    bool forTransmission = false,
    QDateTime nowUtc = {});

QString sstvShareMediaSourceName(SstvShareMediaSource source);
QString sstvShareContentCompletionName(SstvShareContentCompletion completion);
QString sstvShareContentDispositionName(SstvShareContentDisposition disposition);
QString sstvShareEncryptionModeName(SstvShareEncryptionMode mode);

} // namespace decodium::sstv::sharing
