// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvShareProvider.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QPair>
#include <QUrl>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace decodium::sstv::sharing {

// A lease is the only object allowed to put authentication material on a
// request. Providers never ask it for a token, serialize it or include it in a
// diagnostic. Concrete leases should wipe their secret buffers on destruction.
class SstvShareCredentialLease
{
public:
    virtual ~SstvShareCredentialLease() = default;
    virtual bool applyTo(QNetworkRequest& request) const = 0;
};

enum class SstvShareCredentialPurpose
{
    CapabilityDiscovery,
    RecipientLookup,
    CreateUpload,
    UploadContent,
    QueryStatus,
    CompleteUpload,
    CancelUpload,
    DeleteRemoteObject,
    DownloadContent,
    IncomingList,
    AcknowledgeIncoming,
    RejectIncoming,
    DeleteIncoming,
    BlockSender,
};

class SstvShareCredentialSource
{
public:
    virtual ~SstvShareCredentialSource() = default;
    virtual SstvShareAuthenticationStatus status() const noexcept = 0;
    // Acquisition must be non-blocking. A source that needs UI, keychain I/O or
    // refresh first prepares a lease outside the network-provider call path.
    virtual std::shared_ptr<const SstvShareCredentialLease> acquireLease(
        const QString& providerId,
        SstvShareCredentialPurpose purpose) = 0;
};

// A signed URL is itself a bearer credential, so it is supplied by a lease as
// well. It must never be put in settings, persistence snapshots or result
// objects by these providers.
class SstvSharePresignedTargetLease : public SstvShareCredentialLease
{
public:
    ~SstvSharePresignedTargetLease() override = default;
    virtual QUrl targetUrl() const = 0;
};

class SstvSharePresignedTargetSource
{
public:
    virtual ~SstvSharePresignedTargetSource() = default;
    virtual SstvShareAuthenticationStatus status() const noexcept = 0;
    // Like acquireLease(), this must only hand out an already-prepared lease.
    virtual std::shared_ptr<const SstvSharePresignedTargetLease> acquireTarget(
        const QString& providerId,
        const QUuid& transferId,
        quint64 expectedBytes,
        const QString& expectedSha256) = 0;
};

struct SstvHttpTransportOptions final
{
    int timeoutMs {15'000};
    qsizetype maximumResponseBytes {64 * 1024};
    int maximumRedirects {2};

    // This is ignored unless the source is compiled with
    // DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT=1. Normal builds
    // therefore cannot enable plaintext transport at runtime.
    bool allowInsecureLocalhostForTests {false};
};

enum class SstvHttpResponseBody
{
    None,
    OptionalJson,
    RequiredJson,
};

struct SstvHttpResponse final
{
    int statusCode {0};
    QByteArray body;
    QJsonObject json;
    bool hasJson {false};
    QHash<QByteArray, QByteArray> headers;

    QByteArray header(const QByteArray& lowercaseName) const;
};

// Common asynchronous QNetworkAccessManager transport. Instances have QObject
// thread affinity: API methods must be called on their owning thread. No method
// spins a nested event loop. Redirects are handled manually so credentials are
// never forwarded to a different origin.
class SstvHttpShareProvider : public QObject, public SstvShareProvider
{
public:
    ~SstvHttpShareProvider() override;

    QString providerId() const override;
    SstvShareProviderCapabilities capabilities() const override;
    SstvShareAuthenticationStatus authenticationStatus() const override;

    SstvShareOperationId lookupRecipientAsync(
        const QString& stableRecipientId,
        SstvShareRecipientCompletion completion) override;
    SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& handle,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId resumeUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& handle,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId downloadAsync(
        const QString& opaqueIncomingId,
        quint64 offset,
        quint64 maximumBytes,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId acknowledgeAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId rejectAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId deleteIncomingAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId blockSenderAsync(
        const QString& senderId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId revokeAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId deleteRemoteObjectAsync(
        const QString& opaqueId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId refreshCredentialsAsync(
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId listIncomingAsync(
        qsizetype maximumItems,
        SstvShareIncomingCompletion completion) override;
    bool cancelOperation(SstvShareOperationId operationId) override;

protected:
    using ResponseHandler =
        std::function<SstvShareProviderResult(const SstvHttpResponse&)>;

    struct Request final
    {
        QByteArray method;
        QUrl url;
        QByteArray body;
        QList<QPair<QByteArray, QByteArray>> headers;
        QVector<int> acceptedStatusCodes;
        SstvHttpResponseBody responseBody {SstvHttpResponseBody::None};
        qsizetype maximumResponseBytes {0};
        QString diagnosticLabel;
        bool useProviderCredentials {false};
        SstvShareCredentialPurpose credentialPurpose {
            SstvShareCredentialPurpose::UploadContent};
        std::shared_ptr<const SstvShareCredentialLease> explicitLease;
        SstvShareProgressCallback progress;
        ResponseHandler responseHandler;
    };

    SstvHttpShareProvider(
        QString providerId,
        SstvShareProviderCapabilities capabilities,
        SstvHttpTransportOptions options,
        std::shared_ptr<SstvShareCredentialSource> credentialSource,
        bool credentialsRequired,
        QObject* parent = nullptr);

    SstvShareOperationId startRequest(
        Request request,
        SstvShareProviderCompletion completion);
    SstvShareOperationId completeSoon(
        SstvShareProviderResult result,
        SstvShareProviderCompletion completion);
    SstvShareOperationId completeRecipientSoon(
        SstvShareProviderResult result,
        SstvShareRecipientRecord recipient,
        SstvShareRecipientCompletion completion);
    SstvShareOperationId completeIncomingSoon(
        SstvShareProviderResult result,
        QVector<SstvShareIncomingItem> items,
        SstvShareIncomingCompletion completion);

    bool endpointAllowed(const QUrl& url) const;
    const SstvHttpTransportOptions& transportOptions() const noexcept;
    static bool sameOrigin(const QUrl& first, const QUrl& second);
    static QString makeOpaqueUploadId(
        const QString& prefix,
        const QString& stableIdempotencyBinding = {});
    static QByteArray sha256Hex(const QByteArray& bytes);
    static QByteArray sha256Base64(const QByteArray& bytes);
    void setCapabilities(const SstvShareProviderCapabilities& capabilities);

#if defined(DECODIUM_SSTV_PROVIDER_TESTING)
    // Test seam for exercising otherwise unreachable 64-bit exhaustion. It is
    // compiled only into the dedicated provider test binary.
    void setNextOperationIdForTesting(SstvShareOperationId operationId);
    qsizetype pendingOperationCountForTesting() const noexcept;
    quint64 pendingReservedBytesForTesting() const noexcept;
    static constexpr qsizetype maximumPendingOperationsForTesting() noexcept
    {
        return 16;
    }
    static constexpr quint64 maximumPendingBytesForTesting() noexcept
    {
        return 128U * 1024U * 1024U;
    }
#endif

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

struct SstvGenericRestProviderConfig final
{
    QString providerId;
    QUrl baseUrl;
    QString createUploadPath;
    QString uploadChunkPathTemplate;
    QString queryStatusPathTemplate;
    QString completeUploadPathTemplate;
    QString cancelUploadPathTemplate;
    QString capabilitiesPath {QStringLiteral("/api/v1/capabilities")};
    QString recipientLookupPathTemplate {
        QStringLiteral("/api/v1/recipients/{recipientId}")};
    QString incomingListPath {QStringLiteral("/api/v1/inbox")};
    QString downloadPathTemplate {
        QStringLiteral("/api/v1/inbox/{incomingId}/content")};
    QString acknowledgePathTemplate {
        QStringLiteral("/api/v1/inbox/{incomingId}/acknowledge")};
    QString rejectPathTemplate {
        QStringLiteral("/api/v1/inbox/{incomingId}/reject")};
    QString deleteIncomingPathTemplate {
        QStringLiteral("/api/v1/inbox/{incomingId}")};
    QString blockSenderPathTemplate {
        QStringLiteral("/api/v1/senders/{senderId}/block")};
    bool credentialsRequired {true};
    bool requireServerSha256 {true};
    quint64 maximumChunkBytes {1024U * 1024U};
    qsizetype maximumActiveSessions {64};
    qsizetype maximumTerminalRecords {64};
    SstvHttpTransportOptions transport;
};

class SstvGenericRestShareProvider final : public SstvHttpShareProvider
{
public:
    SstvGenericRestShareProvider(
        SstvGenericRestProviderConfig config,
        std::shared_ptr<SstvShareCredentialSource> credentialSource,
        QObject* parent = nullptr);
    ~SstvGenericRestShareProvider() override;

    bool isConfigurationValid() const noexcept;

    SstvShareOperationId refreshCapabilitiesAsync(
        SstvShareProviderCompletion completion);
    SstvShareOperationId lookupRecipientAsync(
        const QString& stableRecipientId,
        SstvShareRecipientCompletion completion) override;

    SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& handle,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId resumeUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& handle,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId revokeAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId downloadAsync(
        const QString& opaqueIncomingId,
        quint64 offset,
        quint64 maximumBytes,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId acknowledgeAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId rejectAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId deleteIncomingAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId blockSenderAsync(
        const QString& senderId,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId listIncomingAsync(
        qsizetype maximumItems,
        SstvShareIncomingCompletion completion) override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_rest;
};

struct SstvWebDavProviderConfig final
{
    QString providerId;
    QUrl collectionUrl;
    bool credentialsRequired {true};
    bool overwriteExisting {false};
    bool requireServerSha256 {true};
    qsizetype maximumActiveSessions {64};
    qsizetype maximumTerminalRecords {64};
    SstvHttpTransportOptions transport;
};

class SstvWebDavShareProvider final : public SstvHttpShareProvider
{
public:
    SstvWebDavShareProvider(
        SstvWebDavProviderConfig config,
        std::shared_ptr<SstvShareCredentialSource> credentialSource,
        QObject* parent = nullptr);
    ~SstvWebDavShareProvider() override;

    bool isConfigurationValid() const noexcept;

    SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& handle,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId resumeUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& handle,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId downloadAsync(
        const QString& opaqueIncomingId,
        quint64 offset,
        quint64 maximumBytes,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId deleteRemoteObjectAsync(
        const QString& opaqueId,
        SstvShareProviderCompletion completion) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_webDav;
};

struct SstvPresignedPutProviderConfig final
{
    QString providerId;
    bool requireServerSha256 {true};
    qsizetype maximumActiveSessions {64};
    qsizetype maximumTerminalRecords {64};
    SstvHttpTransportOptions transport;
};

class SstvPresignedPutShareProvider final : public SstvHttpShareProvider
{
public:
    SstvPresignedPutShareProvider(
        SstvPresignedPutProviderConfig config,
        std::shared_ptr<SstvSharePresignedTargetSource> targetSource,
        QObject* parent = nullptr);
    ~SstvPresignedPutShareProvider() override;

    bool isConfigurationValid() const noexcept;
    SstvShareAuthenticationStatus authenticationStatus() const override;

    SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& handle,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& handle,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;
    SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_presigned;
};

} // namespace decodium::sstv::sharing
