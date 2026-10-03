// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvShareManifest.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QString>
#include <QVector>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>

namespace decodium::sstv::sharing {

enum class SstvShareProviderFailure
{
    None,
    TransientNetwork,
    ProviderUnavailable,
    Offline,
    RateLimited,
    Authentication,
    Authorization,
    Validation,
    RejectedRecipient,
    Conflict,
    NotFound,
    Integrity,
    TlsValidation,
    Cancelled,
    PermanentProviderFailure,
};

bool isRetryableShareProviderFailure(SstvShareProviderFailure failure) noexcept;
QString sstvShareProviderFailureName(SstvShareProviderFailure failure);

enum class SstvShareAuthenticationStatus
{
    NotRequired,
    Authenticated,
    CredentialsRequired,
    RefreshRequired,
    Unavailable,
};

enum class SstvShareRecipientVerification
{
    Unknown,
    ProviderVerified,
    UserVerified,
};

enum class SstvShareRecipientTrust
{
    Unknown,
    Trusted,
    Blocked,
};

struct SstvShareRecipientRecord final
{
    QString providerId;
    QString stableRecipientId;
    QString displayCallsign;
    QString displayName;
    QString publicEncryptionKey;
    QString publicKeyFingerprint;
    SstvShareRecipientVerification verification {
        SstvShareRecipientVerification::Unknown};
    SstvShareRecipientTrust trust {SstvShareRecipientTrust::Unknown};
    QDateTime lastUsedUtc;
};

struct SstvShareProviderCapabilities final
{
    bool recipientLookup {false};
    bool chunkedUpload {false};
    bool resumableUpload {false};
    bool download {false};
    bool acknowledgement {false};
    bool rejection {false};
    bool incomingDelete {false};
    bool senderBlocking {false};
    bool revocation {false};
    bool remoteDelete {false};
    bool incomingList {false};
    bool endToEndEncryptionEnvelope {false};
    bool strictTlsRequired {true};
    quint64 maximumChunkBytes {0U};
    quint64 maximumResponseBytes {kMaximumSharedImageBytes};
};

inline SstvShareValidationError validateShareRecipientRecord(
    const SstvShareRecipientRecord& recipient)
{
    if (!isSafeShareIdentifier(recipient.providerId)
        || !isSafeShareIdentifier(recipient.stableRecipientId)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidIdentifier,
            QStringLiteral("recipient.identity"));
    }
    if (recipient.displayCallsign.size() > 32
        || recipient.displayName.size() > 128
        || sanitizeShareDisplayText(recipient.displayCallsign, 32)
               != recipient.displayCallsign
        || sanitizeShareDisplayText(recipient.displayName, 128)
               != recipient.displayName
        || containsNetworkUrl(recipient.displayCallsign)
        || containsNetworkUrl(recipient.displayName)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidText,
            QStringLiteral("recipient.display"));
    }
    const bool hasPublicKey = !recipient.publicEncryptionKey.isEmpty();
    const bool hasFingerprint = !recipient.publicKeyFingerprint.isEmpty();
    if (hasPublicKey != hasFingerprint
        || recipient.publicEncryptionKey.size() > 8'192
        || (hasPublicKey
            && (sanitizeShareDisplayText(recipient.publicEncryptionKey,
                                         8'192, true)
                    != recipient.publicEncryptionKey
                || containsNetworkUrl(recipient.publicEncryptionKey)
                || !isLowercaseSha256(recipient.publicKeyFingerprint)))) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidEncryption,
            QStringLiteral("recipient.publicKey"));
    }
    if (recipient.lastUsedUtc.isValid()
        && recipient.lastUsedUtc.offsetFromUtc() != 0) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidTimestamp,
            QStringLiteral("recipient.lastUsedUtc"));
    }
    return {};
}

inline SstvShareValidationError validateShareProviderCompatibility(
    const SstvShareManifestV1& manifest,
    const QString& selectedProviderId,
    const SstvShareProviderCapabilities& capabilities)
{
    if (manifest.providerId != selectedProviderId
        || !isSafeShareIdentifier(selectedProviderId)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidIdentifier,
            QStringLiteral("providerId"));
    }
    if (!capabilities.strictTlsRequired || !manifest.transport.tlsRequired
        || !manifest.transport.certificateValidationRequired) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidTransportSecurity,
            QStringLiteral("providerCapabilities.strictTlsRequired"));
    }
    if (capabilities.maximumResponseBytes == 0U
        || capabilities.maximumResponseBytes > kMaximumSharedImageBytes
        || (capabilities.chunkedUpload
            && (capabilities.maximumChunkBytes == 0U
                || capabilities.maximumChunkBytes > kMaximumSharedImageBytes))) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidByteSize,
            QStringLiteral("providerCapabilities.bounds"));
    }
    if (manifest.chunkCount > 1U && !capabilities.chunkedUpload) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidChunkCount,
            QStringLiteral("providerCapabilities.chunkedUpload"));
    }
    if (manifest.encryption.mode == SstvShareEncryptionMode::EndToEnd
        && !capabilities.endToEndEncryptionEnvelope) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidEncryption,
            QStringLiteral("providerCapabilities.endToEndEncryptionEnvelope"));
    }
    return {};
}

struct SstvShareUploadHandle final
{
    // Provider-opaque identifier only. It must never contain a URL, bearer
    // token, cookie or filesystem path.
    QString opaqueId;
    quint64 committedBytes {0U};
};

struct SstvShareIncomingItem final
{
    QString opaqueId;
    QString providerId;
    QString senderId;
    QString manifestSha256;
    // Exact canonical manifest supplied by the provider. Keeping it with the
    // inbox item lets the manager bind the advertised item to the payload hash
    // before any downloaded media can be accepted.
    QByteArray canonicalManifestJson;
    quint64 byteSize {0U};
    QDateTime receivedUtc;
    QDateTime expiresUtc;
};

inline SstvShareValidationError validateShareIncomingItem(
    const SstvShareIncomingItem& item)
{
    if (!isSafeShareIdentifier(item.opaqueId)
        || !isSafeShareIdentifier(item.providerId)
        || !isSafeShareIdentifier(item.senderId)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidIdentifier,
            QStringLiteral("incoming.identity"));
    }
    if (!isLowercaseSha256(item.manifestSha256)) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidHash,
            QStringLiteral("incoming.manifestSha256"));
    }
    if (item.byteSize == 0U || item.byteSize > kMaximumSharedImageBytes) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidByteSize,
            QStringLiteral("incoming.byteSize"));
    }
    if (item.canonicalManifestJson.isEmpty()
        || item.canonicalManifestJson.size() > kMaximumManifestJsonBytes) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::JsonTooLarge,
            QStringLiteral("incoming.canonicalManifestJson"));
    }
    const auto parsedManifest = parseSstvShareManifestV1(
        item.canonicalManifestJson);
    if (!parsedManifest.ok()
        || parsedManifest.manifest->toCanonicalJson()
               != item.canonicalManifestJson
        || parsedManifest.manifest->providerId != item.providerId
        || parsedManifest.manifest->senderId != item.senderId
        || parsedManifest.manifest->byteSize != item.byteSize
        || parsedManifest.manifest->expiresUtc != item.expiresUtc
        || QString::fromLatin1(QCryptographicHash::hash(
               item.canonicalManifestJson,
               QCryptographicHash::Sha256).toHex())
               != item.manifestSha256) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidHash,
            QStringLiteral("incoming.manifestBinding"));
    }
    if (!item.receivedUtc.isValid() || item.receivedUtc.offsetFromUtc() != 0
        || !item.expiresUtc.isValid() || item.expiresUtc.offsetFromUtc() != 0
        || item.expiresUtc <= item.receivedUtc) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidExpiry,
            QStringLiteral("incoming.expiry"));
    }
    return {};
}

class SstvShareProviderResult final
{
public:
    static SstvShareProviderResult success(
        SstvShareUploadHandle handle = {},
        QByteArray boundedPayload = {});
    static SstvShareProviderResult failure(
        SstvShareProviderFailure category,
        const QString& diagnostic,
        qint64 retryAfterMs = 0);

    bool ok() const noexcept { return m_ok; }
    SstvShareProviderFailure category() const noexcept { return m_category; }
    qint64 retryAfterMs() const noexcept { return m_retryAfterMs; }
    const QString& redactedDiagnostic() const noexcept { return m_diagnostic; }
    const SstvShareUploadHandle& handle() const noexcept { return m_handle; }
    const QByteArray& boundedPayload() const noexcept { return m_payload; }

private:
    bool m_ok {false};
    SstvShareProviderFailure m_category {
        SstvShareProviderFailure::PermanentProviderFailure};
    qint64 m_retryAfterMs {0};
    QString m_diagnostic;
    SstvShareUploadHandle m_handle;
    QByteArray m_payload;
};

using SstvShareOperationId = quint64;
using SstvShareProviderCompletion =
    std::function<void(SstvShareProviderResult)>;
using SstvShareRecipientCompletion = std::function<void(
    SstvShareProviderResult, SstvShareRecipientRecord)>;
using SstvShareIncomingCompletion = std::function<void(
    SstvShareProviderResult, QVector<SstvShareIncomingItem>)>;
using SstvShareProgressCallback = std::function<void(quint64, quint64)>;

// Provider implementations own credentials and remote locators internally.
// Callers only receive opaque IDs and redacted diagnostics. Every operation is
// asynchronous and completes exactly once unless the provider itself is being
// destroyed. Network implementations must never use a nested event loop.
class SstvShareProvider
{
public:
    virtual ~SstvShareProvider() = default;

    virtual QString providerId() const = 0;
    virtual SstvShareProviderCapabilities capabilities() const = 0;
    virtual SstvShareAuthenticationStatus authenticationStatus() const = 0;

    virtual SstvShareOperationId lookupRecipientAsync(
        const QString& stableRecipientId,
        SstvShareRecipientCompletion completion) = 0;
    virtual SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& handle,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId resumeUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) = 0;
    // On success, handle().opaqueId is the durable remote-object identifier,
    // not the provider upload-session handle supplied above. Queue history
    // persists it only after strict safe-identifier validation.
    virtual SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& handle,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId downloadAsync(
        const QString& opaqueIncomingId,
        quint64 offset,
        quint64 maximumBytes,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId acknowledgeAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId rejectAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId deleteIncomingAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId blockSenderAsync(
        const QString& senderId,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId revokeAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId deleteRemoteObjectAsync(
        const QString& opaqueId,
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId refreshCredentialsAsync(
        SstvShareProviderCompletion completion) = 0;
    virtual SstvShareOperationId listIncomingAsync(
        qsizetype maximumItems,
        SstvShareIncomingCompletion completion) = 0;

    virtual bool cancelOperation(SstvShareOperationId operationId) = 0;
};

inline bool isRetryableShareProviderFailure(
    SstvShareProviderFailure failure) noexcept
{
    return failure == SstvShareProviderFailure::TransientNetwork
        || failure == SstvShareProviderFailure::ProviderUnavailable
        || failure == SstvShareProviderFailure::Offline
        || failure == SstvShareProviderFailure::RateLimited;
}

inline QString sstvShareProviderFailureName(SstvShareProviderFailure failure)
{
    switch (failure) {
    case SstvShareProviderFailure::None: return QStringLiteral("none");
    case SstvShareProviderFailure::TransientNetwork: return QStringLiteral("transient-network");
    case SstvShareProviderFailure::ProviderUnavailable: return QStringLiteral("provider-unavailable");
    case SstvShareProviderFailure::Offline: return QStringLiteral("offline");
    case SstvShareProviderFailure::RateLimited: return QStringLiteral("rate-limited");
    case SstvShareProviderFailure::Authentication: return QStringLiteral("authentication");
    case SstvShareProviderFailure::Authorization: return QStringLiteral("authorization");
    case SstvShareProviderFailure::Validation: return QStringLiteral("validation");
    case SstvShareProviderFailure::RejectedRecipient: return QStringLiteral("rejected-recipient");
    case SstvShareProviderFailure::Conflict: return QStringLiteral("conflict");
    case SstvShareProviderFailure::NotFound: return QStringLiteral("not-found");
    case SstvShareProviderFailure::Integrity: return QStringLiteral("integrity");
    case SstvShareProviderFailure::TlsValidation: return QStringLiteral("tls-validation");
    case SstvShareProviderFailure::Cancelled: return QStringLiteral("cancelled");
    case SstvShareProviderFailure::PermanentProviderFailure: return QStringLiteral("permanent-provider-failure");
    }
    return QStringLiteral("unknown");
}

inline SstvShareProviderResult SstvShareProviderResult::success(
    SstvShareUploadHandle handle,
    QByteArray boundedPayload)
{
    if (static_cast<quint64>(boundedPayload.size())
        > kMaximumSharedImageBytes) {
        return failure(SstvShareProviderFailure::Validation,
                       QStringLiteral("provider payload exceeded the public bound"));
    }
    SstvShareProviderResult result;
    result.m_ok = true;
    result.m_category = SstvShareProviderFailure::None;
    result.m_handle = std::move(handle);
    result.m_payload = std::move(boundedPayload);
    return result;
}

inline SstvShareProviderResult SstvShareProviderResult::failure(
    SstvShareProviderFailure category,
    const QString& diagnostic,
    qint64 retryAfterMs)
{
    SstvShareProviderResult result;
    result.m_ok = false;
    result.m_category = category == SstvShareProviderFailure::None
        ? SstvShareProviderFailure::PermanentProviderFailure
        : category;
    result.m_retryAfterMs = std::max<qint64>(0, retryAfterMs);
    result.m_diagnostic = redactShareSecrets(diagnostic).left(512);
    return result;
}

} // namespace decodium::sstv::sharing
