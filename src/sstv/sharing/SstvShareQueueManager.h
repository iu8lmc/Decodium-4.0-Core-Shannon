// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvIncomingMediaValidator.h"
#include "SstvShareTransfer.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>

namespace decodium::sstv::sharing {

enum class SstvManagedTransferDirection
{
    Upload = 1,
    Download = 2,
};

enum class SstvManagedTransferState
{
    Queued,
    Preparing,
    Uploading,
    WaitingForAcknowledgement,
    DownloadQueued,
    Downloading,
    AwaitingAcceptance,
    Accepted,
    Acknowledging,
    Rejecting,
    RetryScheduled,
    Paused,
    Completed,
    RemoteDeleted,
    RemoteRevoked,
    Acknowledged,
    Cancelled,
    Rejected,
    Expired,
    Failed,
};

QString sstvManagedTransferStateName(SstvManagedTransferState state);
bool isTerminalManagedTransferState(SstvManagedTransferState state) noexcept;

enum class SstvRemoteCopyAction
{
    Unavailable,
    Delete,
    Revoke,
};

QString sstvRemoteCopyActionName(SstvRemoteCopyAction action);

enum class SstvInboxDisposition
{
    New = 0,
    DownloadQueued,
    AwaitingAcceptance,
    Accepted,
    Acknowledged,
    Cancelled,
    Rejected,
    Expired,
    BlockedLocally,
    BlockedByProvider,
    ProviderDeleted,
};

QString sstvInboxDispositionName(SstvInboxDisposition disposition);

enum class SstvSenderBlockScope
{
    LocalOnly,
    Provider,
};

QString sstvSenderBlockScopeName(SstvSenderBlockScope scope);

struct SstvShareQueueLimits final
{
    qsizetype maximumRecords {10'000};
    qsizetype maximumInboxItems {10'000};
    qsizetype maximumBlockedSenders {4'096};
    qsizetype maximumQueryItems {200};
    qsizetype maximumConcurrentTransfers {2};
    qsizetype maximumConcurrentPerProvider {1};
    quint64 uploadChunkBytes {1024U * 1024U};
    quint64 downloadChunkBytes {1024U * 1024U};

    SstvShareValidationError validate() const;
};

struct SstvShareQueueConfig final
{
    QString databasePath;
    QStringList allowedUploadRoots;
    QString downloadRoot;
    SstvShareQueueLimits limits;
    SstvShareRetryPolicy retryPolicy;
    // nullopt means the platform cannot establish whether the current route is
    // metered. Uploads whose manifest does not explicitly allow metered use
    // remain queued in both the metered and unknown cases.
    std::function<std::optional<bool>()> meteredNetworkProbe;
};

enum class SstvShareFaultPoint
{
    BeforeDatabaseCommit,
    AfterDownloadWriteBeforeCheckpoint,
    BeforeDownloadAtomicCommit,
};

class SstvShareFaultInjector
{
public:
    virtual ~SstvShareFaultInjector() = default;
    virtual bool shouldFail(SstvShareFaultPoint point,
                            const QString& transferId) = 0;
};

struct SstvManagedTransferRecord final
{
    QString transferId;
    SstvManagedTransferDirection direction {
        SstvManagedTransferDirection::Upload};
    SstvManagedTransferState state {SstvManagedTransferState::Queued};
    QString providerId;
    QString recipientId;
    QByteArray canonicalManifestJson;
    QByteArray transferPersistenceJson;
    QByteArray recipientJson;
    QString sourcePath;
    QString destinationPath;
    QString stagingPath;
    QString payloadSha256;
    quint64 byteSize {0U};
    quint64 byteOffset {0U};
    quint32 attempts {0U};
    QDateTime nextRetryUtc;
    SstvShareProviderFailure lastFailure {SstvShareProviderFailure::None};
    QString lastErrorRedacted;
    QString providerSessionId;
    QString incomingId;
    QString idempotencyKey;
    bool cancelRequested {false};
    bool cancelDispatched {false};
    quint32 restartRecoveries {0U};
    quint64 revision {0U};
    QDateTime createdUtc;
    QDateTime updatedUtc;
};

struct SstvPersistentInboxItem final
{
    QString providerId;
    QString incomingId;
    QString senderId;
    QString manifestSha256;
    QByteArray canonicalManifestJson;
    quint64 byteSize {0U};
    QDateTime receivedUtc;
    QDateTime expiresUtc;
    SstvInboxDisposition disposition {SstvInboxDisposition::New};
    QString transferId;
    QDateTime updatedUtc;
};

struct SstvSenderBlockRecord final
{
    QString providerId;
    QString senderId;
    SstvSenderBlockScope scope {SstvSenderBlockScope::LocalOnly};
    QDateTime createdUtc;
};

struct SstvShareQueueDiagnostics final
{
    quint64 uploadedBytes {0U};
    quint64 downloadedBytes {0U};
    quint64 reclaimedRows {0U};
    quint64 uploadBytesPerSecond {0U};
    quint64 downloadBytesPerSecond {0U};
    qsizetype activeQueueDepth {0};
    qsizetype uploadQueueDepth {0};
    qsizetype downloadQueueDepth {0};
    QDateTime resetUtc;
};

enum class SstvShareTransferView
{
    Active,
    History,
};

class SstvShareQueueStore final
{
public:
    explicit SstvShareQueueStore(
        SstvShareQueueConfig config,
        std::shared_ptr<SstvShareFaultInjector> faultInjector = {});
    ~SstvShareQueueStore();

    SstvShareQueueStore(const SstvShareQueueStore&) = delete;
    SstvShareQueueStore& operator=(const SstvShareQueueStore&) = delete;

    bool open(QString* error = nullptr);
    void close();
    bool isOpen() const noexcept;
    int schemaVersion() const noexcept;
    const SstvShareQueueConfig& config() const noexcept;
    quint64 reclaimedRows() const noexcept;
    void resetReclaimedRows() noexcept;

    bool insertTransfer(SstvManagedTransferRecord& record,
                        QString* error = nullptr);
    bool updateTransfer(SstvManagedTransferRecord& record,
                        QString* error = nullptr);
    bool insertDownloadTransferAndInbox(
        SstvManagedTransferRecord& record,
        const SstvPersistentInboxItem& item,
        QString* error = nullptr);
    bool updateDownloadTransferAndInbox(
        SstvManagedTransferRecord& record,
        const SstvPersistentInboxItem& item,
        QString* error = nullptr);
    std::optional<SstvManagedTransferRecord> transfer(
        const QString& transferId,
        QString* error = nullptr) const;
    QVector<SstvManagedTransferRecord> queryTransfers(
        SstvShareTransferView view,
        qsizetype maximumItems,
        QString* error = nullptr) const;

    bool upsertInboxItem(const SstvPersistentInboxItem& item,
                         QString* error = nullptr);
    std::optional<SstvPersistentInboxItem> inboxItem(
        const QString& providerId,
        const QString& incomingId,
        QString* error = nullptr) const;
    QVector<SstvPersistentInboxItem> queryInbox(
        qsizetype maximumItems,
        QString* error = nullptr) const;
    bool upsertSenderBlock(const SstvSenderBlockRecord& block,
                           QString* error = nullptr);
    std::optional<SstvSenderBlockRecord> senderBlock(
        const QString& providerId,
        const QString& senderId,
        QString* error = nullptr) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

using SstvShareManagerCompletion = std::function<void(
    SstvShareProviderResult)>;

// This manager is deliberately timer-agnostic. The owner calls processDue()
// on startup, provider completion and its own retry timer. It is QObject
// thread-affine and is intended to live with its SQLite store on a dedicated
// worker thread rather than on the GUI thread.
class SstvShareQueueManager final : public QObject
{
public:
    explicit SstvShareQueueManager(
        SstvShareQueueConfig config,
        std::shared_ptr<SstvShareFaultInjector> faultInjector = {},
        std::function<QDateTime()> clock = {},
        QObject* parent = nullptr);
    ~SstvShareQueueManager() override;

    SstvShareQueueManager(const SstvShareQueueManager&) = delete;
    SstvShareQueueManager& operator=(const SstvShareQueueManager&) = delete;

    bool registerProvider(std::shared_ptr<SstvShareProvider> provider,
                          QString* error = nullptr);
    bool initialize(QString* error = nullptr);
    bool isInitialized() const noexcept;

    QString queueUpload(const SstvShareManifestV1& manifest,
                        const QString& sourcePath,
                        QString* error = nullptr);
    SstvShareOperationId refreshInboxAsync(
        const QString& providerId,
        SstvShareManagerCompletion completion = {});
    QString queueDownload(const QString& providerId,
                          const QString& incomingId,
                          const QString& destinationRelativePath,
                          QString* error = nullptr);

    bool acceptDownload(const QString& transferId,
                        QString* error = nullptr);
    SstvShareOperationId acknowledgeDownloadAsync(
        const QString& transferId,
        SstvShareManagerCompletion completion = {});
    SstvShareOperationId rejectIncomingAsync(
        const QString& providerId,
        const QString& incomingId,
        SstvShareManagerCompletion completion = {});
    SstvShareOperationId requestIncomingDeletionAsync(
        const QString& providerId,
        const QString& incomingId,
        SstvShareManagerCompletion completion = {});
    SstvShareOperationId blockSenderAsync(
        const QString& providerId,
        const QString& incomingId,
        SstvSenderBlockScope scope,
        SstvShareManagerCompletion completion = {});
    bool saveValidatedCopy(const QString& transferId,
                           const QString& destinationPath,
                           QString* error = nullptr) const;
    bool deleteLocalCopy(const QString& transferId,
                         QString* error = nullptr);
    SstvRemoteCopyAction remoteCopyAction(
        const QString& transferId,
        QString* error = nullptr) const;
    SstvShareOperationId removeRemoteCopyAsync(
        const QString& transferId,
        SstvShareManagerCompletion completion = {});
    bool cancelRemoteCopyRemoval(const QString& transferId,
                                 QString* error = nullptr);
    bool pauseTransfer(const QString& transferId,
                       QString* error = nullptr);
    bool resumeTransfer(const QString& transferId,
                        QString* error = nullptr);
    bool cancelTransfer(const QString& transferId,
                        QString* error = nullptr);

    qsizetype processDue(QString* error = nullptr);
    qsizetype activeOperationCount() const noexcept;

    QVector<SstvManagedTransferRecord> activeTransfers(
        qsizetype maximumItems,
        QString* error = nullptr) const;
    QVector<SstvManagedTransferRecord> transferHistory(
        qsizetype maximumItems,
        QString* error = nullptr) const;
    QVector<SstvPersistentInboxItem> inbox(
        qsizetype maximumItems,
        QString* error = nullptr) const;
    std::optional<SstvValidatedIncomingHandoff> validatedIncomingHandoff(
        const QString& transferId,
        QString* error = nullptr) const;
    SstvShareQueueDiagnostics diagnostics(QString* error = nullptr) const;
    void resetDiagnostics();

    const SstvShareQueueStore& store() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace decodium::sstv::sharing
