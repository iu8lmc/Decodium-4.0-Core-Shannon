// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/sharing/SstvShareQueueManager.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using namespace decodium::sstv::sharing;

namespace {

int g_checks = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(condition)) {                                                     \
            std::cerr << __func__ << ':' << __LINE__                            \
                      << ": check failed: " #condition << '\n';                \
            return false;                                                       \
        }                                                                       \
    } while (false)

QString sha256Hex(const QByteArray& bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QByteArray payload(qsizetype size, char seed)
{
    QByteArray bytes(size, Qt::Uninitialized);
    quint32 value = static_cast<quint8>(seed) + 0x9e3779b9U;
    for (qsizetype index = 0; index < size; ++index) {
        value ^= value << 13U;
        value ^= value >> 17U;
        value ^= value << 5U;
        bytes[index] = static_cast<char>(value & 0xffU);
    }
    return bytes;
}

QByteArray pngPayload(int width, int height, char seed)
{
    QImage image(width, height, QImage::Format_RGB32);
    if (image.isNull()) {
        return {};
    }
    quint32 value = static_cast<quint8>(seed) + 0x6d2b79f5U;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            value ^= value << 13U;
            value ^= value >> 17U;
            value ^= value << 5U;
            image.setPixelColor(x, y, QColor(
                static_cast<int>(value & 0xffU),
                static_cast<int>((value >> 8U) & 0xffU),
                static_cast<int>((value >> 16U) & 0xffU)));
        }
    }
    QByteArray bytes;
    QBuffer output(&bytes);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG")) {
        return {};
    }
    return bytes;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size() && file.flush();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

bool spinUntil(const std::function<bool()>& predicate,
               int timeoutMs = 4'000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1U);
    }
    return predicate();
}

struct TestClock final
{
    QDateTime utc {
        QDateTime::fromString(QStringLiteral("2026-08-24T12:00:00.000Z"),
                              Qt::ISODateWithMs).toUTC()};
};

SstvShareManifestV1 manifestFor(const QByteArray& bytes,
                                const TestClock& clock,
                                const QString& sender,
                                const QString& recipient,
                                const QUuid& transferId = QUuid::createUuid())
{
    SstvShareManifestV1 manifest;
    manifest.transferId = transferId;
    manifest.providerId = QStringLiteral("queue-test");
    manifest.senderId = sender;
    manifest.recipientId = recipient;
    manifest.createdUtc = clock.utc.addSecs(-10);
    manifest.expiresUtc = clock.utc.addDays(1);
    manifest.originalFilename = QStringLiteral("sstv-test.png");
    manifest.safeDisplayFilename = QStringLiteral("SSTV test.png");
    manifest.mimeType = QStringLiteral("image/png");
    manifest.byteSize = static_cast<quint64>(bytes.size());
    manifest.sha256 = sha256Hex(bytes);
    manifest.width = 320U;
    manifest.height = 256U;
    QBuffer probeBuffer;
    probeBuffer.setData(bytes);
    if (probeBuffer.open(QIODevice::ReadOnly)) {
        QImageReader reader(&probeBuffer);
        reader.setDecideFormatFromContent(true);
        const QSize dimensions = reader.size();
        if (reader.format().toLower() == QByteArrayLiteral("png")
            && dimensions.isValid()) {
            manifest.width = static_cast<quint32>(dimensions.width());
            manifest.height = static_cast<quint32>(dimensions.height());
        }
    }
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.source = SstvShareMediaSource::AnalogReception;
    manifest.mediaUtc = clock.utc.addSecs(-20);
    manifest.completion = SstvShareContentCompletion::Complete;
    manifest.chunkCount = static_cast<quint32>(
        std::max<qsizetype>(1, (bytes.size() + 1'023) / 1'024));
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

SstvShareIncomingItem incomingFor(const QString& opaqueId,
                                  const SstvShareManifestV1& manifest,
                                  const TestClock& clock)
{
    SstvShareIncomingItem item;
    item.opaqueId = opaqueId;
    item.providerId = manifest.providerId;
    item.senderId = manifest.senderId;
    item.canonicalManifestJson = manifest.toCanonicalJson();
    item.manifestSha256 = sha256Hex(item.canonicalManifestJson);
    item.byteSize = manifest.byteSize;
    item.receivedUtc = clock.utc;
    item.expiresUtc = manifest.expiresUtc;
    return item;
}

std::optional<SstvManagedTransferRecord> uploadRecordFor(
    const SstvShareManifestV1& manifest,
    const QString& sourcePath,
    const SstvShareRetryPolicy& retryPolicy,
    SstvManagedTransferState state,
    const QDateTime& nowUtc)
{
    SstvShareTransfer transfer(manifest, retryPolicy);
    if (!transfer.isValid() || !transfer.enqueue(nowUtc)) {
        return std::nullopt;
    }
    if (state == SstvManagedTransferState::Completed) {
        const QString idempotencyKey = transfer.snapshot().idempotencyKey;
        if (!transfer.beginPreparing(nowUtc)
            || !transfer.bindProviderUpload(
                idempotencyKey, QStringLiteral("session:reclaim"))
            || !transfer.beginUploading(nowUtc)
            || !transfer.recordProgress(manifest.byteSize, nowUtc)
            || !transfer.waitForAcknowledgement(nowUtc)
            || !transfer.markCompleted(
                idempotencyKey, QStringLiteral("remote:reclaim"), nowUtc)) {
            return std::nullopt;
        }
    } else if (state == SstvManagedTransferState::RetryScheduled) {
        if (!transfer.handleFailure(
                SstvShareProviderFailure::TransientNetwork, nowUtc)) {
            return std::nullopt;
        }
    } else if (state != SstvManagedTransferState::Queued) {
        return std::nullopt;
    }

    const auto& snapshot = transfer.snapshot();
    SstvShareValidationError persistenceError;
    SstvManagedTransferRecord record;
    record.transferId = manifest.transferId.toString(QUuid::WithoutBraces);
    record.direction = SstvManagedTransferDirection::Upload;
    record.state = state;
    record.providerId = manifest.providerId;
    record.recipientId = manifest.recipientId;
    record.canonicalManifestJson = manifest.toCanonicalJson();
    record.transferPersistenceJson = transfer.toPersistenceJson(
        &persistenceError);
    record.sourcePath = QFileInfo(sourcePath).canonicalFilePath();
    record.payloadSha256 = manifest.sha256;
    record.byteSize = manifest.byteSize;
    record.byteOffset = snapshot.bytesTransferred;
    record.attempts = snapshot.retryCount;
    record.nextRetryUtc = snapshot.retryAtUtc;
    record.lastFailure = snapshot.lastFailure;
    record.providerSessionId = snapshot.providerUploadId;
    record.idempotencyKey = snapshot.idempotencyKey;
    record.restartRecoveries = snapshot.restartRecoveries;
    record.createdUtc = nowUtc;
    record.updatedUtc = nowUtc;
    if (!persistenceError.ok() || record.transferPersistenceJson.isEmpty()
        || record.sourcePath.isEmpty()) {
        return std::nullopt;
    }
    return record;
}

SstvPersistentInboxItem persistentInboxFor(
    const SstvShareIncomingItem& incoming,
    SstvInboxDisposition disposition,
    const QDateTime& updatedUtc)
{
    SstvPersistentInboxItem item;
    item.providerId = incoming.providerId;
    item.incomingId = incoming.opaqueId;
    item.senderId = incoming.senderId;
    item.manifestSha256 = incoming.manifestSha256;
    item.canonicalManifestJson = incoming.canonicalManifestJson;
    item.byteSize = incoming.byteSize;
    item.receivedUtc = incoming.receivedUtc;
    item.expiresUtc = incoming.expiresUtc;
    item.disposition = disposition;
    item.updatedUtc = updatedUtc;
    return item;
}

struct ManagedDownloadPair final
{
    SstvManagedTransferRecord transfer;
    SstvPersistentInboxItem inbox;
};

ManagedDownloadPair acknowledgedDownloadFor(
    const SstvShareManifestV1& manifest,
    const SstvShareIncomingItem& incoming,
    const QString& transferId,
    const QString& destinationPath,
    const QDateTime& nowUtc)
{
    ManagedDownloadPair pair;
    pair.inbox = persistentInboxFor(
        incoming, SstvInboxDisposition::Acknowledged, nowUtc);
    pair.inbox.transferId = transferId;
    pair.transfer.transferId = transferId;
    pair.transfer.direction = SstvManagedTransferDirection::Download;
    pair.transfer.state = SstvManagedTransferState::Acknowledged;
    pair.transfer.providerId = manifest.providerId;
    pair.transfer.recipientId = manifest.recipientId;
    pair.transfer.canonicalManifestJson = manifest.toCanonicalJson();
    pair.transfer.destinationPath = destinationPath.isEmpty()
        ? QString {} : QFileInfo(destinationPath).canonicalFilePath();
    pair.transfer.payloadSha256 = manifest.sha256;
    pair.transfer.byteSize = manifest.byteSize;
    pair.transfer.byteOffset = manifest.byteSize;
    pair.transfer.incomingId = incoming.opaqueId;
    pair.transfer.idempotencyKey =
        SstvShareTransfer::deriveIdempotencyKey(manifest);
    pair.transfer.createdUtc = nowUtc;
    pair.transfer.updatedUtc = nowUtc;
    return pair;
}

struct FakeBackend final
{
    QString sessionId {QStringLiteral("session:queue-test")};
    QByteArray uploaded;
    QHash<QString, QByteArray> downloads;
    QVector<SstvShareIncomingItem> incoming;
    QVector<quint64> requestedDownloadOffsets;
    bool failNextUploadChunk {false};
    bool chunkedUpload {true};
    bool resumableUpload {true};
    bool revocation {false};
    bool remoteDelete {false};
    bool incomingDelete {false};
    bool senderBlocking {false};
    bool failRemoteRemoval {false};
    QString remoteObjectId {QStringLiteral("remote:queue-object")};
    qint64 uploadRetryAfterMs {0};
    int delayMs {1};
    int lookupCalls {0};
    int createCalls {0};
    int resumeCalls {0};
    int uploadCalls {0};
    int completeCalls {0};
    int remoteCancelCalls {0};
    int revokeCalls {0};
    int remoteDeleteCalls {0};
    int incomingDeleteCalls {0};
    int blockSenderCalls {0};
    int cancelOperationCalls {0};
    int downloadCalls {0};
    int acknowledgeCalls {0};
    int rejectCalls {0};
};

class FakeProvider final : public SstvShareProvider
{
public:
    explicit FakeProvider(std::shared_ptr<FakeBackend> backend)
        : m_backend(std::move(backend))
    {
    }

    QString providerId() const override
    {
        return QStringLiteral("queue-test");
    }

    SstvShareProviderCapabilities capabilities() const override
    {
        SstvShareProviderCapabilities value;
        value.recipientLookup = true;
        value.chunkedUpload = m_backend->chunkedUpload;
        value.resumableUpload = m_backend->resumableUpload;
        value.download = true;
        value.acknowledgement = true;
        value.rejection = true;
        value.incomingDelete = m_backend->incomingDelete;
        value.senderBlocking = m_backend->senderBlocking;
        value.revocation = m_backend->revocation;
        value.remoteDelete = m_backend->remoteDelete;
        value.incomingList = true;
        value.strictTlsRequired = true;
        value.maximumChunkBytes = 1'024U;
        value.maximumResponseBytes = 1'024U;
        return value;
    }

    SstvShareAuthenticationStatus authenticationStatus() const override
    {
        return SstvShareAuthenticationStatus::Authenticated;
    }

    SstvShareOperationId lookupRecipientAsync(
        const QString& stableRecipientId,
        SstvShareRecipientCompletion completion) override
    {
        ++m_backend->lookupCalls;
        const QString provider = providerId();
        return schedule([provider, stableRecipientId,
                         completion = std::move(completion)]() mutable {
            SstvShareRecipientRecord recipient;
            recipient.providerId = provider;
            recipient.stableRecipientId = stableRecipientId;
            recipient.displayCallsign = QStringLiteral("9H1TEST");
            recipient.displayName = QStringLiteral("Queue recipient");
            recipient.verification =
                SstvShareRecipientVerification::ProviderVerified;
            recipient.trust = SstvShareRecipientTrust::Trusted;
            completion(SstvShareProviderResult::success(),
                       std::move(recipient));
        });
    }

    SstvShareOperationId createUploadAsync(
        const SstvShareManifestV1& manifest,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(manifest)
        Q_UNUSED(idempotencyKey)
        ++m_backend->createCalls;
        const auto backend = m_backend;
        return schedule([backend, completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success(
                {backend->sessionId,
                 static_cast<quint64>(backend->uploaded.size())}));
        });
    }

    SstvShareOperationId uploadChunkAsync(
        const SstvShareUploadHandle& handle,
        quint64 offset,
        const QByteArray& chunk,
        const QString& chunkSha256,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(progress)
        ++m_backend->uploadCalls;
        const auto backend = m_backend;
        return schedule([backend, handle, offset, chunk, chunkSha256,
                         completion = std::move(completion)]() mutable {
            if (backend->failNextUploadChunk) {
                backend->failNextUploadChunk = false;
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::RateLimited,
                    QStringLiteral("Bearer SUPER_SECRET at https://host.invalid/object?token=SUPER_SECRET"),
                    backend->uploadRetryAfterMs));
                return;
            }
            if (!backend->resumableUpload && offset == 0U) {
                backend->uploaded.clear();
            }
            if (handle.opaqueId != backend->sessionId
                || offset != static_cast<quint64>(backend->uploaded.size())
                || sha256Hex(chunk) != chunkSha256) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("fake upload integrity failure")));
                return;
            }
            backend->uploaded += chunk;
            completion(SstvShareProviderResult::success(
                {backend->sessionId,
                 static_cast<quint64>(backend->uploaded.size())}));
        });
    }

    SstvShareOperationId resumeUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(handle)
        ++m_backend->resumeCalls;
        const auto backend = m_backend;
        return schedule([backend, completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success(
                {backend->sessionId,
                 static_cast<quint64>(backend->uploaded.size())}));
        });
    }

    SstvShareOperationId completeUploadAsync(
        const SstvShareUploadHandle& handle,
        const QString& idempotencyKey,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(handle)
        Q_UNUSED(idempotencyKey)
        ++m_backend->completeCalls;
        const auto backend = m_backend;
        return schedule([backend, completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success(
                {backend->remoteObjectId, 0U}));
        });
    }

    SstvShareOperationId cancelUploadAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override
    {
        ++m_backend->remoteCancelCalls;
        return schedule([handle, completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success(handle));
        });
    }

    SstvShareOperationId queryStatusAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override
    {
        return resumeUploadAsync(handle, std::move(completion));
    }

    SstvShareOperationId downloadAsync(
        const QString& opaqueIncomingId,
        quint64 offset,
        quint64 maximumBytes,
        SstvShareProgressCallback progress,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(progress)
        ++m_backend->downloadCalls;
        m_backend->requestedDownloadOffsets.push_back(offset);
        const auto backend = m_backend;
        return schedule([backend, opaqueIncomingId, offset, maximumBytes,
                         completion = std::move(completion)]() mutable {
            const auto found = backend->downloads.constFind(opaqueIncomingId);
            if (found == backend->downloads.constEnd()
                || offset >= static_cast<quint64>(found->size())) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::NotFound,
                    QStringLiteral("fake incoming object unavailable")));
                return;
            }
            const quint64 available =
                static_cast<quint64>(found->size()) - offset;
            const qsizetype length = static_cast<qsizetype>(
                std::min(available, maximumBytes));
            completion(SstvShareProviderResult::success(
                {}, found->mid(static_cast<qsizetype>(offset), length)));
        });
    }

    SstvShareOperationId acknowledgeAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(opaqueIncomingId)
        ++m_backend->acknowledgeCalls;
        return schedule([completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success());
        });
    }

    SstvShareOperationId rejectAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override
    {
        Q_UNUSED(opaqueIncomingId)
        ++m_backend->rejectCalls;
        return schedule([completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success());
        });
    }

    SstvShareOperationId deleteIncomingAsync(
        const QString& opaqueIncomingId,
        SstvShareProviderCompletion completion) override
    {
        ++m_backend->incomingDeleteCalls;
        const auto backend = m_backend;
        return schedule([backend, opaqueIncomingId,
                         completion = std::move(completion)]() mutable {
            if (!backend->incomingDelete
                || !backend->downloads.contains(opaqueIncomingId)) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::PermanentProviderFailure,
                    QStringLiteral("incoming deletion unsupported")));
                return;
            }
            backend->downloads.remove(opaqueIncomingId);
            completion(SstvShareProviderResult::success(
                {opaqueIncomingId, 0U}));
        });
    }

    SstvShareOperationId blockSenderAsync(
        const QString& senderId,
        SstvShareProviderCompletion completion) override
    {
        ++m_backend->blockSenderCalls;
        const auto backend = m_backend;
        return schedule([backend, senderId,
                         completion = std::move(completion)]() mutable {
            const bool listed = std::any_of(
                backend->incoming.cbegin(), backend->incoming.cend(),
                [&senderId](const SstvShareIncomingItem& item) {
                    return item.senderId == senderId;
                });
            completion(backend->senderBlocking && listed
                ? SstvShareProviderResult::success()
                : SstvShareProviderResult::failure(
                    SstvShareProviderFailure::PermanentProviderFailure,
                    QStringLiteral("sender blocking unsupported")));
        });
    }

    SstvShareOperationId revokeAsync(
        const SstvShareUploadHandle& handle,
        SstvShareProviderCompletion completion) override
    {
        ++m_backend->revokeCalls;
        const auto backend = m_backend;
        return schedule([backend, handle,
                         completion = std::move(completion)]() mutable {
            if (backend->failRemoteRemoval) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Authorization,
                    QStringLiteral("provider refused remote revocation")));
                return;
            }
            completion(SstvShareProviderResult::success(handle));
        });
    }

    SstvShareOperationId deleteRemoteObjectAsync(
        const QString& opaqueId,
        SstvShareProviderCompletion completion) override
    {
        ++m_backend->remoteDeleteCalls;
        const auto backend = m_backend;
        return schedule([backend, opaqueId,
                         completion = std::move(completion)]() mutable {
            if (!backend->remoteDelete
                || opaqueId != backend->remoteObjectId
                || backend->failRemoteRemoval) {
                completion(SstvShareProviderResult::failure(
                    backend->failRemoteRemoval
                        ? SstvShareProviderFailure::Authorization
                        : SstvShareProviderFailure::PermanentProviderFailure,
                    QStringLiteral("remote delete intentionally unsupported")));
                return;
            }
            completion(SstvShareProviderResult::success({opaqueId, 0U}));
        });
    }

    SstvShareOperationId refreshCredentialsAsync(
        SstvShareProviderCompletion completion) override
    {
        return schedule([completion = std::move(completion)]() mutable {
            completion(SstvShareProviderResult::success());
        });
    }

    SstvShareOperationId listIncomingAsync(
        qsizetype maximumItems,
        SstvShareIncomingCompletion completion) override
    {
        const auto backend = m_backend;
        return schedule([backend, maximumItems,
                         completion = std::move(completion)]() mutable {
            QVector<SstvShareIncomingItem> items = backend->incoming;
            if (items.size() > maximumItems) {
                items.resize(maximumItems);
            }
            completion(SstvShareProviderResult::success(), std::move(items));
        });
    }

    bool cancelOperation(SstvShareOperationId operationId) override
    {
        if (!m_pending->remove(operationId)) {
            return false;
        }
        ++m_backend->cancelOperationCalls;
        return true;
    }

private:
    SstvShareOperationId schedule(std::function<void()> callback)
    {
        const SstvShareOperationId operation = m_nextOperation++;
        m_pending->insert(operation);
        const auto pending = m_pending;
        QTimer::singleShot(
            std::max(0, m_backend->delayMs), QCoreApplication::instance(),
            [pending, operation, callback = std::move(callback)]() mutable {
                if (pending->remove(operation)) {
                    callback();
                }
            });
        return operation;
    }

    std::shared_ptr<FakeBackend> m_backend;
    std::shared_ptr<QSet<SstvShareOperationId>> m_pending {
        std::make_shared<QSet<SstvShareOperationId>>()};
    SstvShareOperationId m_nextOperation {1U};
};

class TestFaultInjector final : public SstvShareFaultInjector
{
public:
    void arm(SstvShareFaultPoint point, int occurrences = 1)
    {
        m_point = point;
        m_remaining = occurrences;
    }

    void disarm() { m_remaining = 0; }
    int remaining() const noexcept { return m_remaining; }

    bool shouldFail(SstvShareFaultPoint point,
                    const QString& transferId) override
    {
        Q_UNUSED(transferId)
        if (m_remaining <= 0 || point != m_point) {
            return false;
        }
        --m_remaining;
        return true;
    }

private:
    SstvShareFaultPoint m_point {SstvShareFaultPoint::BeforeDatabaseCommit};
    int m_remaining {0};
};

struct Fixture final
{
    Fixture()
    {
        uploadRoot = temporary.path() + QStringLiteral("/uploads");
        downloadRoot = temporary.path() + QStringLiteral("/downloads");
        QDir().mkpath(uploadRoot);
        config.databasePath = temporary.path() + QStringLiteral("/queue.sqlite3");
        config.allowedUploadRoots = {uploadRoot};
        config.downloadRoot = downloadRoot;
        config.limits.maximumRecords = 100;
        config.limits.maximumInboxItems = 100;
        config.limits.maximumQueryItems = 100;
        config.limits.maximumConcurrentTransfers = 1;
        config.limits.maximumConcurrentPerProvider = 1;
        config.limits.uploadChunkBytes = 1'024U;
        config.limits.downloadChunkBytes = 1'024U;
        config.retryPolicy.baseDelayMs = 100;
        config.retryPolicy.maximumDelayMs = 2'000;
        config.retryPolicy.maximumProviderRetryAfterMs = 10'000;
        config.retryPolicy.maximumRetries = 3U;
        config.retryPolicy.jitterPermille = 100U;
        config.meteredNetworkProbe = [] {
            return std::optional<bool> {false};
        };
    }

    std::unique_ptr<SstvShareQueueManager> manager(
        const std::shared_ptr<FakeBackend>& backend,
        const std::shared_ptr<TestFaultInjector>& fault = {})
    {
        auto value = std::make_unique<SstvShareQueueManager>(
            config, fault, [this] { return clock.utc; });
        QString error;
        if (!value->registerProvider(
                std::make_shared<FakeProvider>(backend), &error)
            || !value->initialize(&error)) {
            std::cerr << "fixture manager init failed: "
                      << error.toStdString() << '\n';
            return {};
        }
        return value;
    }

    QTemporaryDir temporary;
    QString uploadRoot;
    QString downloadRoot;
    SstvShareQueueConfig config;
    TestClock clock;
};

bool uploadRetryRestartResumeIsDurableAndRedacted()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const QByteArray bytes = payload(3'300, 'u');
    const QString source = fixture.uploadRoot + QStringLiteral("/upload.png");
    CHECK(writeFile(source, bytes));
    const auto manifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    const auto backend = std::make_shared<FakeBackend>();
    backend->failNextUploadChunk = true;
    backend->uploadRetryAfterMs = 750;
    QString transferId;
    QDateTime retryAt;
    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        transferId = manager->queueUpload(manifest, source, &error);
        if (transferId.isEmpty()) {
            std::cerr << "queue upload failed: " << error.toStdString() << '\n';
        }
        CHECK(!transferId.isEmpty());
        CHECK(manager->processDue(&error) == 1);
        const bool retryScheduled = spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state == SstvManagedTransferState::RetryScheduled;
        });
        if (!retryScheduled) {
            const auto record = manager->store().transfer(transferId);
            std::cerr << "upload stalled state="
                      << (record ? sstvManagedTransferStateName(record->state)
                                      .toStdString()
                                 : std::string("missing"))
                      << " active=" << manager->activeOperationCount()
                      << " lookup=" << backend->lookupCalls
                      << " create=" << backend->createCalls
                      << " chunks=" << backend->uploadCalls << '\n';
        }
        CHECK(retryScheduled);
        const auto record = manager->store().transfer(transferId);
        CHECK(record);
        CHECK(record->attempts == 1U);
        CHECK(record->nextRetryUtc >= fixture.clock.utc.addMSecs(750));
        CHECK(!record->lastErrorRedacted.contains(
            QStringLiteral("SUPER_SECRET")));
        CHECK(!record->lastErrorRedacted.contains(QStringLiteral("/object")));
        CHECK(record->lastErrorRedacted.contains(
            QStringLiteral("<redacted-url>")));
        CHECK(!record->recipientJson.isEmpty());
        retryAt = record->nextRetryUtc;
        CHECK(manager->activeTransfers(100).size() == 1);
    }

    CHECK(!readFile(fixture.config.databasePath).contains("SUPER_SECRET"));
    CHECK(!readFile(fixture.config.databasePath + QStringLiteral("-wal"))
               .contains("SUPER_SECRET"));
    fixture.clock.utc = retryAt;
    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        CHECK(manager->processDue(&error) == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state == SstvManagedTransferState::Completed;
        }));
        CHECK(manager->activeTransfers(100).isEmpty());
        const auto history = manager->transferHistory(100);
        CHECK(history.size() == 1);
        CHECK(history.front().restartRecoveries == 0U);
    }
    CHECK(backend->uploaded == bytes);
    CHECK(backend->resumeCalls == 1);
    CHECK(backend->completeCalls == 1);
    CHECK(QFileInfo::exists(source));
    return true;
}

bool inboxDownloadAcceptanceAckAndRejectAreExplicit()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const auto backend = std::make_shared<FakeBackend>();
    const QByteArray firstBytes = pngPayload(48, 36, 'd');
    const QByteArray secondBytes = pngPayload(24, 18, 'r');
    const QByteArray thirdBytes = pngPayload(32, 20, 'x');
    CHECK(firstBytes.size() > 1'024);
    CHECK(!secondBytes.isEmpty());
    CHECK(!thirdBytes.isEmpty());
    const auto firstManifest = manifestFor(
        firstBytes, fixture.clock, QStringLiteral("station:remote-one"),
        QStringLiteral("station:local"));
    const auto secondManifest = manifestFor(
        secondBytes, fixture.clock, QStringLiteral("station:remote-two"),
        QStringLiteral("station:local"));
    const auto thirdManifest = manifestFor(
        thirdBytes, fixture.clock, QStringLiteral("station:remote-three"),
        QStringLiteral("station:local"));
    backend->incoming = {
        incomingFor(QStringLiteral("incoming:one"), firstManifest, fixture.clock),
        incomingFor(QStringLiteral("incoming:two"), secondManifest, fixture.clock),
        incomingFor(QStringLiteral("incoming:three"), thirdManifest, fixture.clock),
    };
    backend->downloads.insert(QStringLiteral("incoming:one"), firstBytes);
    backend->downloads.insert(QStringLiteral("incoming:two"), secondBytes);
    backend->downloads.insert(QStringLiteral("incoming:three"), thirdBytes);
    const auto fault = std::make_shared<TestFaultInjector>();
    auto manager = fixture.manager(backend, fault);
    CHECK(manager);
    bool refreshDone = false;
    bool refreshOk = false;
    CHECK(manager->refreshInboxAsync(
        QStringLiteral("queue-test"),
        [&](SstvShareProviderResult result) {
            refreshDone = true;
            refreshOk = result.ok();
            if (!result.ok()) {
                std::cerr << "refresh failed: "
                          << result.redactedDiagnostic().toStdString() << '\n';
            }
        }) != 0U);
    CHECK(spinUntil([&] { return refreshDone; }));
    CHECK(refreshOk);
    CHECK(manager->inbox(100).size() == 3);

    QString error;
    CHECK(manager->queueDownload(
        QStringLiteral("queue-test"), QStringLiteral("incoming:one"),
        QStringLiteral("../escape.png"), &error).isEmpty());
    CHECK(!error.isEmpty());
    error.clear();
    const QString transferId = manager->queueDownload(
        QStringLiteral("queue-test"), QStringLiteral("incoming:one"),
        QStringLiteral("received/one.png"), &error);
    CHECK(!transferId.isEmpty());
    CHECK(manager->processDue(&error) == 1);
    CHECK(spinUntil([&] {
        const auto record = manager->store().transfer(transferId);
        return record
            && record->state
                == SstvManagedTransferState::AwaitingAcceptance;
    }));
    const auto staged = manager->store().transfer(transferId);
    CHECK(staged);
    CHECK(QFileInfo::exists(staged->destinationPath));
    CHECK(QFileInfo::exists(staged->stagingPath));
    const auto previewHandoff = manager->validatedIncomingHandoff(transferId);
    CHECK(previewHandoff);
    CHECK(previewHandoff->sourceSha256 == firstManifest.sha256);
    CHECK(previewHandoff->stagedCanonicalPath
          == QFileInfo(staged->destinationPath).canonicalFilePath());
    QImageReader previewReader(staged->destinationPath);
    CHECK(previewReader.textKeys().isEmpty());

    fault->arm(SstvShareFaultPoint::BeforeDownloadAtomicCommit);
    CHECK(!manager->acceptDownload(transferId, &error));
    CHECK(QFileInfo::exists(staged->destinationPath));
    CHECK(QFileInfo::exists(staged->stagingPath));
    fault->disarm();
    error.clear();
    CHECK(manager->acceptDownload(transferId, &error));
    CHECK(!readFile(staged->destinationPath).isEmpty());
    CHECK(!QFileInfo::exists(staged->stagingPath));
    CHECK(manager->validatedIncomingHandoff(transferId));

    bool ackDone = false;
    bool ackOk = false;
    CHECK(manager->acknowledgeDownloadAsync(
        transferId, [&](SstvShareProviderResult result) {
            ackDone = true;
            ackOk = result.ok();
        }) != 0U);
    CHECK(spinUntil([&] { return ackDone; }));
    CHECK(ackOk);
    CHECK(manager->store().transfer(transferId)->state
          == SstvManagedTransferState::Acknowledged);
    CHECK(QFileInfo::exists(staged->destinationPath));
    CHECK(backend->acknowledgeCalls == 1);

    bool rejectDone = false;
    bool rejectOk = false;
    CHECK(manager->rejectIncomingAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:two"),
        [&](SstvShareProviderResult result) {
            rejectDone = true;
            rejectOk = result.ok();
        }) != 0U);
    CHECK(spinUntil([&] { return rejectDone; }));
    CHECK(rejectOk);
    const auto rejectedInbox = manager->store().inboxItem(
        QStringLiteral("queue-test"), QStringLiteral("incoming:two"));
    CHECK(rejectedInbox);
    CHECK(rejectedInbox->disposition == SstvInboxDisposition::Rejected);
    CHECK(manager->store().transfer(rejectedInbox->transferId)->state
          == SstvManagedTransferState::Rejected);
    CHECK(backend->rejectCalls == 1);

    error.clear();
    const QString rejectedTransferId = manager->queueDownload(
        QStringLiteral("queue-test"), QStringLiteral("incoming:three"),
        QStringLiteral("received/three.png"), &error);
    CHECK(!rejectedTransferId.isEmpty());
    CHECK(manager->processDue(&error) == 1);
    CHECK(spinUntil([&] {
        const auto record = manager->store().transfer(rejectedTransferId);
        return record
            && record->state
                == SstvManagedTransferState::AwaitingAcceptance;
    }));
    const auto rejectedStaging = manager->store().transfer(rejectedTransferId);
    CHECK(rejectedStaging);
    CHECK(QFileInfo::exists(rejectedStaging->stagingPath));
    CHECK(QFileInfo::exists(rejectedStaging->destinationPath));
    rejectDone = false;
    rejectOk = false;
    CHECK(manager->rejectIncomingAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:three"),
        [&](SstvShareProviderResult result) {
            rejectDone = true;
            rejectOk = result.ok();
        }) != 0U);
    CHECK(spinUntil([&] { return rejectDone; }));
    CHECK(rejectOk);
    CHECK(manager->store().transfer(rejectedTransferId)->state
          == SstvManagedTransferState::Rejected);
    CHECK(!QFileInfo::exists(rejectedStaging->stagingPath));
    CHECK(!QFileInfo::exists(rejectedStaging->destinationPath));
    CHECK(backend->rejectCalls == 2);
    return true;
}

bool singleShotUploadRehydratesAndReplaysAfterRestart()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const QByteArray bytes = payload(900, 'p');
    const QString source = fixture.uploadRoot
        + QStringLiteral("/single-shot.png");
    CHECK(writeFile(source, bytes));
    const auto manifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    const auto backend = std::make_shared<FakeBackend>();
    backend->chunkedUpload = false;
    backend->resumableUpload = false;
    backend->delayMs = 20;
    QString transferId;
    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        transferId = manager->queueUpload(manifest, source, &error);
        CHECK(!transferId.isEmpty());
        CHECK(manager->processDue(&error) == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state
                    == SstvManagedTransferState::WaitingForAcknowledgement
                && manager->activeOperationCount() == 1;
        }));
        CHECK(backend->uploaded == bytes);
    }
    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        const auto recovered = manager->store().transfer(transferId);
        CHECK(recovered);
        CHECK(recovered->state == SstvManagedTransferState::RetryScheduled);
        CHECK(recovered->restartRecoveries == 1U);
        CHECK(manager->processDue() == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state == SstvManagedTransferState::Completed;
        }));
    }
    CHECK(backend->uploadCalls == 2);
    CHECK(backend->resumeCalls == 0);
    CHECK(backend->uploaded == bytes);
    CHECK(QFileInfo::exists(source));
    return true;
}

bool downloadCrashBoundaryResumesFromDurableFileOffset()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const auto backend = std::make_shared<FakeBackend>();
    const auto fault = std::make_shared<TestFaultInjector>();
    const QByteArray bytes = pngPayload(52, 40, 'c');
    CHECK(bytes.size() > 2'048);
    const auto manifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:remote"),
        QStringLiteral("station:local"));
    backend->incoming = {incomingFor(
        QStringLiteral("incoming:crash"), manifest, fixture.clock)};
    backend->downloads.insert(QStringLiteral("incoming:crash"), bytes);
    QString transferId;
    QString stagingPath;
    {
        auto manager = fixture.manager(backend, fault);
        CHECK(manager);
        bool refreshDone = false;
        manager->refreshInboxAsync(QStringLiteral("queue-test"),
            [&](SstvShareProviderResult result) { refreshDone = result.ok(); });
        CHECK(spinUntil([&] { return refreshDone; }));
        QString error;
        transferId = manager->queueDownload(
            QStringLiteral("queue-test"), QStringLiteral("incoming:crash"),
            QStringLiteral("crash/recovered.png"), &error);
        CHECK(!transferId.isEmpty());
        stagingPath = manager->store().transfer(transferId)->stagingPath;
        fault->arm(SstvShareFaultPoint::AfterDownloadWriteBeforeCheckpoint);
        CHECK(manager->processDue(&error) == 1);
        CHECK(spinUntil([&] {
            return fault->remaining() == 0
                && QFileInfo(stagingPath).size() == 1'024;
        }));
        const auto beforeCrash = manager->store().transfer(transferId);
        CHECK(beforeCrash->state == SstvManagedTransferState::Downloading);
        CHECK(beforeCrash->byteOffset == 0U);
    }

    {
        auto manager = fixture.manager(backend, fault);
        CHECK(manager);
        const auto recovered = manager->store().transfer(transferId);
        CHECK(recovered);
        CHECK(recovered->state == SstvManagedTransferState::DownloadQueued);
        CHECK(recovered->byteOffset == 1'024U);
        CHECK(recovered->restartRecoveries == 1U);
        CHECK(manager->processDue() == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state
                    == SstvManagedTransferState::AwaitingAcceptance;
        }));
    }
    CHECK(std::count(backend->requestedDownloadOffsets.cbegin(),
                     backend->requestedDownloadOffsets.cend(), 0U) == 1);
    CHECK(QFileInfo::exists(stagingPath));
    CHECK(readFile(stagingPath) == bytes);
    return true;
}

bool completedRemoteCopyRemovalIsCapabilityGatedDurableAndCancellable()
{
    {
        Fixture fixture;
        CHECK(fixture.temporary.isValid());
        const QByteArray bytes = payload(2'300, 'r');
        const QString source = fixture.uploadRoot
            + QStringLiteral("/remote-revoke.png");
        CHECK(writeFile(source, bytes));
        const auto backend = std::make_shared<FakeBackend>();
        backend->revocation = true;
        const auto manifest = manifestFor(
            bytes, fixture.clock, QStringLiteral("station:sender"),
            QStringLiteral("recipient:remote"));
        QString transferId;
        {
            auto manager = fixture.manager(backend);
            CHECK(manager);
            QString error;
            transferId = manager->queueUpload(manifest, source, &error);
            CHECK(!transferId.isEmpty());
            CHECK(manager->remoteCopyAction(transferId)
                  == SstvRemoteCopyAction::Unavailable);
            CHECK(manager->processDue(&error) == 1);
            CHECK(spinUntil([&] {
                const auto record = manager->store().transfer(transferId);
                return record
                    && record->state == SstvManagedTransferState::Completed;
            }));
            CHECK(manager->remoteCopyAction(transferId)
                  == SstvRemoteCopyAction::Revoke);
            bool completed = false;
            bool succeeded = false;
            CHECK(manager->removeRemoteCopyAsync(
                transferId,
                [&](SstvShareProviderResult result) {
                    completed = true;
                    succeeded = result.ok();
                }) != 0U);
            CHECK(spinUntil([&] { return completed; }));
            CHECK(succeeded);
            const auto revoked = manager->store().transfer(transferId);
            CHECK(revoked);
            CHECK(revoked->state == SstvManagedTransferState::RemoteRevoked);
            CHECK(revoked->lastFailure == SstvShareProviderFailure::None);
            CHECK(backend->revokeCalls == 1);
            CHECK(backend->remoteDeleteCalls == 0);
            CHECK(backend->createCalls >= 2);
        }
        {
            auto restarted = fixture.manager(backend);
            CHECK(restarted);
            const auto persisted = restarted->store().transfer(transferId);
            CHECK(persisted);
            CHECK(persisted->state
                  == SstvManagedTransferState::RemoteRevoked);
            CHECK(restarted->remoteCopyAction(transferId)
                  == SstvRemoteCopyAction::Unavailable);
            bool rejected = false;
            CHECK(restarted->removeRemoteCopyAsync(
                transferId,
                [&](SstvShareProviderResult result) {
                    rejected = !result.ok()
                        && result.category()
                            == SstvShareProviderFailure::Validation;
                }) == 0U);
            CHECK(rejected);
        }
    }

    {
        Fixture fixture;
        CHECK(fixture.temporary.isValid());
        const QByteArray bytes = payload(1'700, 'd');
        const QString source = fixture.uploadRoot
            + QStringLiteral("/remote-delete.png");
        CHECK(writeFile(source, bytes));
        const auto backend = std::make_shared<FakeBackend>();
        backend->revocation = true;
        backend->remoteDelete = true;
        const auto manifest = manifestFor(
            bytes, fixture.clock, QStringLiteral("station:sender"),
            QStringLiteral("recipient:remote"));
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        const QString transferId = manager->queueUpload(
            manifest, source, &error);
        CHECK(!transferId.isEmpty());
        CHECK(manager->processDue(&error) == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state == SstvManagedTransferState::Completed;
        }));
        CHECK(manager->remoteCopyAction(transferId)
              == SstvRemoteCopyAction::Delete);

        backend->failRemoteRemoval = true;
        bool failed = false;
        CHECK(manager->removeRemoteCopyAsync(
            transferId,
            [&](SstvShareProviderResult result) {
                failed = !result.ok()
                    && result.category()
                        == SstvShareProviderFailure::Authorization;
            }) != 0U);
        CHECK(spinUntil([&] { return failed; }));
        const auto retained = manager->store().transfer(transferId);
        CHECK(retained);
        CHECK(retained->state == SstvManagedTransferState::Completed);
        CHECK(retained->lastFailure
              == SstvShareProviderFailure::Authorization);
        CHECK(!retained->lastErrorRedacted.isEmpty());
        CHECK(manager->remoteCopyAction(transferId)
              == SstvRemoteCopyAction::Delete);

        backend->failRemoteRemoval = false;
        bool deleted = false;
        CHECK(manager->removeRemoteCopyAsync(
            transferId,
            [&](SstvShareProviderResult result) { deleted = result.ok(); })
            != 0U);
        CHECK(spinUntil([&] { return deleted; }));
        CHECK(manager->store().transfer(transferId)->state
              == SstvManagedTransferState::RemoteDeleted);
        CHECK(backend->remoteDeleteCalls == 2);
        CHECK(backend->revokeCalls == 0);
        manager.reset();
        auto restarted = fixture.manager(backend);
        CHECK(restarted);
        CHECK(restarted->store().transfer(transferId)->state
              == SstvManagedTransferState::RemoteDeleted);
        CHECK(restarted->remoteCopyAction(transferId)
              == SstvRemoteCopyAction::Unavailable);
    }

    {
        Fixture fixture;
        CHECK(fixture.temporary.isValid());
        const QByteArray bytes = payload(1'500, 'c');
        const QString source = fixture.uploadRoot
            + QStringLiteral("/remote-cancel.png");
        CHECK(writeFile(source, bytes));
        const auto backend = std::make_shared<FakeBackend>();
        backend->remoteDelete = true;
        const auto manifest = manifestFor(
            bytes, fixture.clock, QStringLiteral("station:sender"),
            QStringLiteral("recipient:remote"));
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        const QString transferId = manager->queueUpload(
            manifest, source, &error);
        CHECK(!transferId.isEmpty());
        CHECK(manager->processDue(&error) == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(transferId);
            return record
                && record->state == SstvManagedTransferState::Completed;
        }));
        backend->delayMs = 100;
        bool callbackCancelled = false;
        CHECK(manager->removeRemoteCopyAsync(
            transferId,
            [&](SstvShareProviderResult result) {
                callbackCancelled = !result.ok()
                    && result.category()
                        == SstvShareProviderFailure::Cancelled;
            }) != 0U);
        CHECK(manager->activeOperationCount() == 1);
        CHECK(manager->cancelRemoteCopyRemoval(transferId, &error));
        CHECK(callbackCancelled);
        CHECK(manager->activeOperationCount() == 0);
        QThread::msleep(120U);
        QCoreApplication::processEvents();
        CHECK(manager->store().transfer(transferId)->state
              == SstvManagedTransferState::Completed);

        bool shutdownCallback = false;
        CHECK(manager->removeRemoteCopyAsync(
            transferId,
            [&](SstvShareProviderResult) { shutdownCallback = true; }) != 0U);
        manager.reset();
        QThread::msleep(120U);
        QCoreApplication::processEvents();
        CHECK(!shutdownCallback);
    }
    return true;
}

bool cancellationAndDatabaseFaultsAreIdempotentAndNonDestructive()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const QByteArray bytes = payload(4'000, 'x');
    const QString source = fixture.uploadRoot + QStringLiteral("/cancel.png");
    CHECK(writeFile(source, bytes));
    const auto backend = std::make_shared<FakeBackend>();
    backend->delayMs = 20;
    const auto fault = std::make_shared<TestFaultInjector>();
    auto manager = fixture.manager(backend, fault);
    CHECK(manager);

    fault->arm(SstvShareFaultPoint::BeforeDatabaseCommit);
    QString error;
    const auto failedManifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    CHECK(manager->queueUpload(failedManifest, source, &error).isEmpty());
    CHECK(manager->activeTransfers(100).isEmpty());
    CHECK(QFileInfo::exists(source));

    fault->disarm();
    const auto cancelManifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    const QString transferId = manager->queueUpload(
        cancelManifest, source, &error);
    if (transferId.isEmpty()) {
        std::cerr << "queue cancel upload failed: "
                  << error.toStdString() << '\n';
    }
    CHECK(!transferId.isEmpty());
    CHECK(manager->processDue(&error) == 1);
    CHECK(spinUntil([&] {
        const auto record = manager->store().transfer(transferId);
        return record && !record->providerSessionId.isEmpty()
            && manager->activeOperationCount() == 1;
    }));
    CHECK(manager->cancelTransfer(transferId, &error));
    CHECK(manager->cancelTransfer(transferId, &error));
    CHECK(spinUntil([&] {
        const auto record = manager->store().transfer(transferId);
        return record
            && record->state == SstvManagedTransferState::Cancelled;
    }));
    const auto cancelled = manager->store().transfer(transferId);
    CHECK(cancelled->cancelDispatched);
    CHECK(cancelled->lastFailure == SstvShareProviderFailure::Cancelled);
    CHECK(backend->remoteCancelCalls == 1);
    CHECK(backend->cancelOperationCalls == 1);
    CHECK(QFileInfo::exists(source));

    const QString outside = fixture.temporary.path()
        + QStringLiteral("/outside.png");
    CHECK(writeFile(outside, bytes));
    const auto outsideManifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    CHECK(manager->queueUpload(outsideManifest, outside, &error).isEmpty());

    const QString link = fixture.uploadRoot + QStringLiteral("/linked.png");
    CHECK(QFile::link(outside, link));
    const auto linkedManifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    CHECK(manager->queueUpload(linkedManifest, link, &error).isEmpty());

    const QString oversized = fixture.uploadRoot
        + QStringLiteral("/oversized.png");
    QFile oversizedFile(oversized);
    CHECK(oversizedFile.open(QIODevice::WriteOnly));
    CHECK(oversizedFile.resize(
        static_cast<qint64>(kMaximumSharedImageBytes + 1U)));
    oversizedFile.close();
    const auto boundedManifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    CHECK(manager->queueUpload(
        boundedManifest, oversized, &error).isEmpty());

    SstvPersistentInboxItem hostile;
    hostile.providerId = QStringLiteral("queue-test");
    hostile.incomingId = QStringLiteral("incoming:hostile");
    hostile.senderId = QStringLiteral("station:hostile");
    hostile.manifestSha256 = QString(64, QLatin1Char('0'));
    hostile.canonicalManifestJson = QByteArray("{malformed");
    hostile.byteSize = 1U;
    hostile.receivedUtc = fixture.clock.utc;
    hostile.expiresUtc = fixture.clock.utc.addDays(1);
    hostile.updatedUtc = fixture.clock.utc;
    SstvShareQueueConfig hostileConfig = fixture.config;
    hostileConfig.databasePath = fixture.temporary.path()
        + QStringLiteral("/hostile.sqlite3");
    SstvShareQueueStore hostileStore(hostileConfig);
    CHECK(hostileStore.open(&error));
    CHECK(!hostileStore.upsertInboxItem(hostile, &error));
    hostile.canonicalManifestJson = QByteArray(
        kMaximumManifestJsonBytes + 1, 'x');
    CHECK(!hostileStore.upsertInboxItem(hostile, &error));
    return true;
}

bool persistentPauseResumeCoversUploadAndDownload()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const auto backend = std::make_shared<FakeBackend>();
    backend->delayMs = 40;
    const QByteArray uploadBytes = payload(3'300, 'p');
    const QString source = fixture.uploadRoot
        + QStringLiteral("/pause-upload.png");
    CHECK(writeFile(source, uploadBytes));
    const auto uploadManifest = manifestFor(
        uploadBytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    QString uploadId;

    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        uploadId = manager->queueUpload(uploadManifest, source, &error);
        CHECK(!uploadId.isEmpty());
        CHECK(manager->processDue(&error) == 1);
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(uploadId);
            return record
                && record->state == SstvManagedTransferState::Uploading
                && manager->activeOperationCount() == 1;
        }));
        CHECK(manager->pauseTransfer(uploadId, &error));
        CHECK(manager->pauseTransfer(uploadId, &error));
        const auto paused = manager->store().transfer(uploadId);
        CHECK(paused);
        CHECK(paused->state == SstvManagedTransferState::Paused);
        CHECK(manager->activeOperationCount() == 0);
        CHECK(manager->processDue(&error) == 0);
        QThread::msleep(60U);
        QCoreApplication::processEvents();
        CHECK(manager->store().transfer(uploadId)->state
              == SstvManagedTransferState::Paused);
    }

    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        CHECK(manager->store().transfer(uploadId)->state
              == SstvManagedTransferState::Paused);
        CHECK(manager->processDue(&error) == 0);
        CHECK(manager->resumeTransfer(uploadId, &error));
        CHECK(!manager->resumeTransfer(uploadId, &error));
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(uploadId);
            return record
                && record->state == SstvManagedTransferState::Completed;
        }));
    }
    CHECK(backend->uploaded == uploadBytes);
    CHECK(backend->cancelOperationCalls >= 1);

    const QByteArray downloadBytes = pngPayload(64, 48, 'q');
    CHECK(downloadBytes.size() > 1'024);
    const auto downloadManifest = manifestFor(
        downloadBytes, fixture.clock, QStringLiteral("station:remote"),
        QStringLiteral("station:local"));
    backend->incoming = {incomingFor(
        QStringLiteral("incoming:pause"), downloadManifest, fixture.clock)};
    backend->downloads.insert(QStringLiteral("incoming:pause"), downloadBytes);
    QString downloadId;

    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        bool refreshed = false;
        CHECK(manager->refreshInboxAsync(
            QStringLiteral("queue-test"),
            [&](SstvShareProviderResult result) {
                refreshed = result.ok();
            }) != 0U);
        CHECK(spinUntil([&] { return refreshed; }));
        QString error;
        downloadId = manager->queueDownload(
            QStringLiteral("queue-test"), QStringLiteral("incoming:pause"),
            QStringLiteral("received/pause.png"), &error);
        CHECK(!downloadId.isEmpty());
        CHECK(manager->processDue(&error) == 1);
        CHECK(manager->activeOperationCount() == 1);
        CHECK(manager->pauseTransfer(downloadId, &error));
        const auto paused = manager->store().transfer(downloadId);
        CHECK(paused);
        CHECK(paused->state == SstvManagedTransferState::Paused);
        CHECK(paused->byteOffset == 0U);
        CHECK(manager->processDue(&error) == 0);
    }

    {
        auto manager = fixture.manager(backend);
        CHECK(manager);
        QString error;
        CHECK(manager->store().transfer(downloadId)->state
              == SstvManagedTransferState::Paused);
        CHECK(manager->processDue(&error) == 0);
        CHECK(manager->resumeTransfer(downloadId, &error));
        CHECK(spinUntil([&] {
            const auto record = manager->store().transfer(downloadId);
            return record
                && record->state
                    == SstvManagedTransferState::AwaitingAcceptance;
        }));
        const auto completedDownload = manager->store().transfer(downloadId);
        CHECK(completedDownload->byteOffset == completedDownload->byteSize);
        CHECK(!manager->pauseTransfer(downloadId, &error));
    }
    CHECK(backend->downloads.value(QStringLiteral("incoming:pause"))
          == downloadBytes);
    return true;
}

bool checksumMismatchNeverBecomesAcceptable()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const QByteArray advertised = payload(1'500, 'a');
    const QByteArray delivered = payload(1'500, 'b');
    const auto manifest = manifestFor(
        advertised, fixture.clock, QStringLiteral("station:remote"),
        QStringLiteral("station:local"));
    const auto backend = std::make_shared<FakeBackend>();
    backend->incoming = {incomingFor(
        QStringLiteral("incoming:bad-hash"), manifest, fixture.clock)};
    backend->downloads.insert(QStringLiteral("incoming:bad-hash"), delivered);
    auto manager = fixture.manager(backend);
    CHECK(manager);
    bool refreshed = false;
    manager->refreshInboxAsync(QStringLiteral("queue-test"),
        [&](SstvShareProviderResult result) { refreshed = result.ok(); });
    CHECK(spinUntil([&] { return refreshed; }));
    QString error;
    const QString transferId = manager->queueDownload(
        QStringLiteral("queue-test"), QStringLiteral("incoming:bad-hash"),
        QStringLiteral("bad/hash.png"), &error);
    CHECK(!transferId.isEmpty());
    CHECK(manager->processDue() == 1);
    CHECK(spinUntil([&] {
        const auto record = manager->store().transfer(transferId);
        return record && record->state == SstvManagedTransferState::Failed;
    }));
    const auto failed = manager->store().transfer(transferId);
    CHECK(failed->lastFailure == SstvShareProviderFailure::Integrity);
    CHECK(!QFileInfo::exists(failed->destinationPath));
    CHECK(QFileInfo::exists(failed->stagingPath));
    CHECK(!manager->acceptDownload(transferId, &error));
    return true;
}

bool meteredPolicyAndBoundedDiagnosticsAreFailClosedAndMonotonic()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    const auto networkState =
        std::make_shared<std::optional<bool>>(std::nullopt);
    fixture.config.meteredNetworkProbe = [networkState] {
        return *networkState;
    };
    const QByteArray bytes = payload(4'300, 'm');
    const QString source = fixture.uploadRoot
        + QStringLiteral("/metered-policy.png");
    CHECK(writeFile(source, bytes));
    const auto manifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:sender"),
        QStringLiteral("recipient:remote"));
    const auto backend = std::make_shared<FakeBackend>();
    backend->delayMs = 8;
    auto manager = fixture.manager(backend);
    CHECK(manager);
    QString error;
    const QString transferId = manager->queueUpload(manifest, source, &error);
    CHECK(!transferId.isEmpty());
    const auto queuedRecord = manager->store().transfer(transferId, &error);
    CHECK(queuedRecord);
    CHECK(!queuedRecord->canonicalManifestJson.contains(source.toUtf8()));
    CHECK(!queuedRecord->canonicalManifestJson.contains(
        fixture.uploadRoot.toUtf8()));

    CHECK(manager->processDue(&error) == 0);
    CHECK(backend->lookupCalls == 0);
    SstvShareQueueDiagnostics diagnostic = manager->diagnostics(&error);
    CHECK(error.isEmpty());
    CHECK(diagnostic.activeQueueDepth == 1);
    CHECK(diagnostic.uploadQueueDepth == 1);
    CHECK(diagnostic.downloadQueueDepth == 0);
    CHECK(diagnostic.uploadedBytes == 0U);

    *networkState = true;
    CHECK(manager->processDue(&error) == 0);
    CHECK(backend->lookupCalls == 0);

    *networkState = false;
    CHECK(manager->processDue(&error) == 1);
    quint64 previousUploaded = 0U;
    QElapsedTimer timer;
    timer.start();
    bool completed = false;
    while (!completed && timer.elapsed() < 5'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        diagnostic = manager->diagnostics(&error);
        CHECK(error.isEmpty());
        CHECK(diagnostic.uploadedBytes >= previousUploaded);
        CHECK(diagnostic.uploadedBytes <= static_cast<quint64>(bytes.size()));
        CHECK(diagnostic.uploadBytesPerSecond
              <= ((quint64 {1U} << 53U) - 1U));
        previousUploaded = diagnostic.uploadedBytes;
        const auto record = manager->store().transfer(transferId);
        completed = record
            && record->state == SstvManagedTransferState::Completed;
        QThread::msleep(1U);
    }
    CHECK(completed);
    diagnostic = manager->diagnostics(&error);
    CHECK(diagnostic.uploadedBytes == static_cast<quint64>(bytes.size()));
    CHECK(diagnostic.activeQueueDepth == 0);

    manager->resetDiagnostics();
    diagnostic = manager->diagnostics(&error);
    CHECK(diagnostic.uploadedBytes == 0U);
    CHECK(diagnostic.downloadedBytes == 0U);
    CHECK(diagnostic.uploadBytesPerSecond == 0U);
    CHECK(diagnostic.downloadBytesPerSecond == 0U);
    CHECK(diagnostic.resetUtc == fixture.clock.utc);

    const QByteArray incomingBytes = pngPayload(38, 29, 'n');
    CHECK(!incomingBytes.isEmpty());
    const auto incomingManifest = manifestFor(
        incomingBytes, fixture.clock, QStringLiteral("station:incoming"),
        QStringLiteral("station:local"));
    backend->incoming = {incomingFor(QStringLiteral("incoming:metered"),
                                     incomingManifest, fixture.clock)};
    backend->downloads.insert(QStringLiteral("incoming:metered"),
                              incomingBytes);
    bool refreshed = false;
    manager->refreshInboxAsync(
        QStringLiteral("queue-test"),
        [&refreshed](SstvShareProviderResult result) {
            refreshed = result.ok();
        });
    CHECK(spinUntil([&] { return refreshed; }));
    const QString downloadId = manager->queueDownload(
        QStringLiteral("queue-test"), QStringLiteral("incoming:metered"),
        QStringLiteral("metered/download.png"), &error);
    CHECK(!downloadId.isEmpty());
    CHECK(manager->processDue(&error) == 1);
    quint64 previousDownloaded = 0U;
    timer.restart();
    bool downloaded = false;
    while (!downloaded && timer.elapsed() < 5'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        diagnostic = manager->diagnostics(&error);
        CHECK(error.isEmpty());
        CHECK(diagnostic.downloadedBytes >= previousDownloaded);
        CHECK(diagnostic.downloadedBytes
              <= static_cast<quint64>(incomingBytes.size()));
        previousDownloaded = diagnostic.downloadedBytes;
        const auto record = manager->store().transfer(downloadId);
        downloaded = record
            && record->state
                == SstvManagedTransferState::AwaitingAcceptance;
        QThread::msleep(1U);
    }
    CHECK(downloaded);
    diagnostic = manager->diagnostics(&error);
    CHECK(diagnostic.uploadedBytes == 0U);
    CHECK(diagnostic.downloadedBytes
          == static_cast<quint64>(incomingBytes.size()));
    CHECK(diagnostic.activeQueueDepth == 1);
    CHECK(diagnostic.downloadQueueDepth == 1);
    return true;
}

bool incomingLifecycleIsCapabilityDrivenIdempotentAndLocallyBounded()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    fixture.config.limits.maximumBlockedSenders = 1;
    const auto backend = std::make_shared<FakeBackend>();
    backend->incomingDelete = true;
    backend->senderBlocking = true;
    const QByteArray firstBytes = pngPayload(30, 22, 'b');
    const QByteArray secondBytes = pngPayload(31, 23, 'c');
    const QByteArray thirdBytes = pngPayload(32, 24, 'd');
    CHECK(!firstBytes.isEmpty());
    CHECK(!secondBytes.isEmpty());
    CHECK(!thirdBytes.isEmpty());
    const auto firstManifest = manifestFor(
        firstBytes, fixture.clock, QStringLiteral("station:blocked"),
        QStringLiteral("station:local"));
    const auto secondManifest = manifestFor(
        secondBytes, fixture.clock, QStringLiteral("station:blocked"),
        QStringLiteral("station:local"));
    const auto thirdManifest = manifestFor(
        thirdBytes, fixture.clock, QStringLiteral("station:allowed"),
        QStringLiteral("station:local"));
    backend->incoming = {
        incomingFor(QStringLiteral("incoming:block-one"), firstManifest,
                    fixture.clock),
        incomingFor(QStringLiteral("incoming:block-two"), secondManifest,
                    fixture.clock),
        incomingFor(QStringLiteral("incoming:save"), thirdManifest,
                    fixture.clock),
    };
    backend->downloads.insert(QStringLiteral("incoming:block-one"),
                              firstBytes);
    backend->downloads.insert(QStringLiteral("incoming:block-two"),
                              secondBytes);
    backend->downloads.insert(QStringLiteral("incoming:save"), thirdBytes);
    auto manager = fixture.manager(backend);
    CHECK(manager);
    bool refreshed = false;
    manager->refreshInboxAsync(
        QStringLiteral("queue-test"),
        [&refreshed](SstvShareProviderResult result) {
            refreshed = result.ok();
        });
    CHECK(spinUntil([&] { return refreshed; }));

    bool localBlockDone = false;
    bool localBlockOk = false;
    CHECK(manager->blockSenderAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:block-one"),
        SstvSenderBlockScope::LocalOnly,
        [&](SstvShareProviderResult result) {
            localBlockDone = true;
            localBlockOk = result.ok();
        }) == 0U);
    CHECK(localBlockDone);
    CHECK(localBlockOk);
    CHECK(backend->blockSenderCalls == 0);
    CHECK(manager->store().inboxItem(
              QStringLiteral("queue-test"),
              QStringLiteral("incoming:block-one"))->disposition
          == SstvInboxDisposition::BlockedLocally);
    CHECK(manager->store().inboxItem(
              QStringLiteral("queue-test"),
              QStringLiteral("incoming:block-two"))->disposition
          == SstvInboxDisposition::BlockedLocally);

    bool boundedBlockDone = false;
    bool boundedBlockOk = true;
    CHECK(manager->blockSenderAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:save"),
        SstvSenderBlockScope::LocalOnly,
        [&](SstvShareProviderResult result) {
            boundedBlockDone = true;
            boundedBlockOk = result.ok();
        }) == 0U);
    CHECK(boundedBlockDone);
    CHECK(!boundedBlockOk);
    CHECK(manager->store().inboxItem(
              QStringLiteral("queue-test"),
              QStringLiteral("incoming:save"))->disposition
          == SstvInboxDisposition::New);

    bool providerBlockDone = false;
    bool providerBlockOk = false;
    CHECK(manager->blockSenderAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:block-one"),
        SstvSenderBlockScope::Provider,
        [&](SstvShareProviderResult result) {
            providerBlockDone = true;
            providerBlockOk = result.ok();
        }) != 0U);
    CHECK(spinUntil([&] { return providerBlockDone; }));
    CHECK(providerBlockOk);
    CHECK(backend->blockSenderCalls == 1);
    CHECK(manager->store().inboxItem(
              QStringLiteral("queue-test"),
              QStringLiteral("incoming:block-two"))->disposition
          == SstvInboxDisposition::BlockedByProvider);
    providerBlockDone = false;
    CHECK(manager->blockSenderAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:block-two"),
        SstvSenderBlockScope::Provider,
        [&](SstvShareProviderResult result) {
            providerBlockDone = result.ok();
        }) == 0U);
    CHECK(providerBlockDone);
    CHECK(backend->blockSenderCalls == 1);

    bool deletionDone = false;
    bool deletionOk = false;
    CHECK(manager->requestIncomingDeletionAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:block-one"),
        [&](SstvShareProviderResult result) {
            deletionDone = true;
            deletionOk = result.ok();
        }) != 0U);
    CHECK(spinUntil([&] { return deletionDone; }));
    CHECK(deletionOk);
    CHECK(backend->incomingDeleteCalls == 1);
    CHECK(!backend->downloads.contains(QStringLiteral("incoming:block-one")));
    CHECK(manager->store().inboxItem(
              QStringLiteral("queue-test"),
              QStringLiteral("incoming:block-one"))->disposition
          == SstvInboxDisposition::ProviderDeleted);
    deletionDone = false;
    CHECK(manager->requestIncomingDeletionAsync(
        QStringLiteral("queue-test"), QStringLiteral("incoming:block-one"),
        [&](SstvShareProviderResult result) {
            deletionDone = result.ok();
        }) == 0U);
    CHECK(deletionDone);
    CHECK(backend->incomingDeleteCalls == 1);

    QString error;
    const QString downloadId = manager->queueDownload(
        QStringLiteral("queue-test"), QStringLiteral("incoming:save"),
        QStringLiteral("private/save.png"), &error);
    CHECK(!downloadId.isEmpty());
    CHECK(manager->processDue(&error) == 1);
    CHECK(spinUntil([&] {
        const auto record = manager->store().transfer(downloadId);
        return record && record->state
            == SstvManagedTransferState::AwaitingAcceptance;
    }));
    CHECK(manager->acceptDownload(downloadId, &error));
    const auto handoff = manager->validatedIncomingHandoff(
        downloadId, &error);
    CHECK(handoff);
    const QString exportRoot = fixture.temporary.path()
        + QStringLiteral("/exports");
    CHECK(QDir().mkpath(exportRoot));
    const QString exported = exportRoot + QStringLiteral("/saved-copy.png");
    CHECK(manager->saveValidatedCopy(downloadId, exported, &error));
    CHECK(QFileInfo::exists(exported));
    CHECK(readFile(exported) == readFile(handoff->stagedCanonicalPath));
    CHECK(!manager->saveValidatedCopy(downloadId, exported, &error));
    CHECK(!readFile(fixture.config.databasePath).contains(
        exported.toUtf8()));
    CHECK(!readFile(fixture.config.databasePath + QStringLiteral("-wal"))
               .contains(exported.toUtf8()));
    error.clear();
    const bool deletedLocalCopy = manager->deleteLocalCopy(downloadId, &error);
    if (!deletedLocalCopy) {
        std::cerr << "delete local copy failed: "
                  << error.toStdString() << '\n';
    }
    CHECK(deletedLocalCopy);
    CHECK(!QFileInfo::exists(handoff->stagedCanonicalPath));
    CHECK(QFileInfo::exists(exported));
    CHECK(!manager->validatedIncomingHandoff(downloadId));
    error.clear();
    CHECK(manager->deleteLocalCopy(downloadId, &error));
    return true;
}

bool terminalReclamationIsDeterministicAndRetainsRetryAcrossRestart()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    fixture.config.limits.maximumRecords = 4;
    fixture.config.limits.maximumQueryItems = 4;
    const QByteArray bytes = payload(1'024, 'q');
    const QString source = fixture.uploadRoot
        + QStringLiteral("/reclamation.png");
    CHECK(writeFile(source, bytes));

    const auto makeRecord = [&](const QString& uuid,
                                SstvManagedTransferState state) {
        const auto manifest = manifestFor(
            bytes, fixture.clock, QStringLiteral("station:sender"),
            QStringLiteral("recipient:remote"), QUuid(uuid));
        return uploadRecordFor(manifest, source, fixture.config.retryPolicy,
                               state, fixture.clock.utc);
    };
    const QString oldestId = QStringLiteral(
        "00000000-0000-4000-8000-000000000001");
    const QString newerTerminalId = QStringLiteral(
        "00000000-0000-4000-8000-000000000002");
    const QString retryId = QStringLiteral(
        "00000000-0000-4000-8000-000000000003");
    const QString queuedId = QStringLiteral(
        "00000000-0000-4000-8000-000000000004");
    const QString localCopyId = QStringLiteral(
        "00000000-0000-4000-8000-000000000000");
    auto oldest = makeRecord(oldestId, SstvManagedTransferState::Completed);
    auto newerTerminal = makeRecord(
        newerTerminalId, SstvManagedTransferState::Completed);
    auto retry = makeRecord(
        retryId, SstvManagedTransferState::RetryScheduled);
    auto queued = makeRecord(queuedId, SstvManagedTransferState::Queued);
    CHECK(oldest);
    CHECK(newerTerminal);
    CHECK(retry);
    CHECK(queued);

    QString error;
    {
        SstvShareQueueStore store(fixture.config);
        CHECK(store.open(&error));
        CHECK(store.schemaVersion() == 3);
        const QString localCopyPath = fixture.downloadRoot
            + QStringLiteral("/validated/") + localCopyId
            + QStringLiteral(".png");
        CHECK(writeFile(localCopyPath, bytes));
        const auto localManifest = manifestFor(
            bytes, fixture.clock, QStringLiteral("station:remote"),
            QStringLiteral("station:local"));
        const auto localIncoming = incomingFor(
            QStringLiteral("incoming:local-copy"), localManifest,
            fixture.clock);
        ManagedDownloadPair localCopy = acknowledgedDownloadFor(
            localManifest, localIncoming, localCopyId, localCopyPath,
            fixture.clock.utc);
        localCopy.transfer.createdUtc = fixture.clock.utc.addSecs(-1);
        localCopy.transfer.updatedUtc = localCopy.transfer.createdUtc;
        CHECK(store.upsertInboxItem(localCopy.inbox, &error));
        CHECK(store.insertDownloadTransferAndInbox(
            localCopy.transfer, localCopy.inbox, &error));
        CHECK(store.insertTransfer(*oldest, &error));
        CHECK(store.insertTransfer(*newerTerminal, &error));
        CHECK(store.insertTransfer(*retry, &error));
        CHECK(store.reclaimedRows() == 0U);
    }

    {
        const auto fault = std::make_shared<TestFaultInjector>();
        SstvShareQueueStore restarted(fixture.config, fault);
        CHECK(restarted.open(&error));
        fault->arm(SstvShareFaultPoint::BeforeDatabaseCommit);
        CHECK(!restarted.insertTransfer(*queued, &error));
        CHECK(restarted.reclaimedRows() == 0U);
        error.clear();
        CHECK(restarted.transfer(oldestId, &error));
        CHECK(!restarted.transfer(queuedId, &error));
        fault->disarm();
        CHECK(restarted.insertTransfer(*queued, &error));
        CHECK(restarted.reclaimedRows() == 1U);
        CHECK(!restarted.transfer(oldestId, &error));
        const auto retainedTerminal = restarted.transfer(
            newerTerminalId, &error);
        const auto retainedRetry = restarted.transfer(retryId, &error);
        const auto retainedQueued = restarted.transfer(queuedId, &error);
        const auto retainedLocalCopy = restarted.transfer(localCopyId, &error);
        CHECK(retainedTerminal);
        CHECK(retainedTerminal->state
              == SstvManagedTransferState::Completed);
        CHECK(retainedRetry);
        CHECK(retainedRetry->state
              == SstvManagedTransferState::RetryScheduled);
        CHECK(retainedQueued);
        CHECK(retainedQueued->state == SstvManagedTransferState::Queued);
        CHECK(retainedLocalCopy);
        CHECK(retainedLocalCopy->state
              == SstvManagedTransferState::Acknowledged);
        CHECK(QFileInfo::exists(retainedLocalCopy->destinationPath));
        CHECK(restarted.inboxItem(
            retainedLocalCopy->providerId,
            retainedLocalCopy->incomingId, &error));
        const auto active = restarted.queryTransfers(
            SstvShareTransferView::Active, 4, &error);
        CHECK(error.isEmpty());
        CHECK(active.size() == 2);
        restarted.resetReclaimedRows();
        CHECK(restarted.reclaimedRows() == 0U);
    }
    return true;
}

bool pairedInboxReclamationIsAtomicAndProtectsLocalCopy()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    fixture.config.limits.maximumInboxItems = 2;
    fixture.config.limits.maximumRecords = 4;
    const QByteArray bytes = pngPayload(18, 14, 'p');
    CHECK(!bytes.isEmpty());
    const auto manifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:remote"),
        QStringLiteral("station:local"));
    const auto localIncoming = incomingFor(
        QStringLiteral("closed:000"), manifest, fixture.clock);
    const auto prunableIncoming = incomingFor(
        QStringLiteral("closed:001"), manifest, fixture.clock);
    const QString localId = QStringLiteral(
        "00000000-0000-4000-8000-000000000010");
    const QString prunableId = QStringLiteral(
        "00000000-0000-4000-8000-000000000011");
    QString error;
    SstvShareQueueStore store(fixture.config);
    CHECK(store.open(&error));
    const QString localPath = fixture.downloadRoot
        + QStringLiteral("/validated/") + localId + QStringLiteral(".png");
    CHECK(writeFile(localPath, bytes));
    ManagedDownloadPair local = acknowledgedDownloadFor(
        manifest, localIncoming, localId, localPath, fixture.clock.utc);
    ManagedDownloadPair prunable = acknowledgedDownloadFor(
        manifest, prunableIncoming, prunableId, {}, fixture.clock.utc);
    CHECK(store.upsertInboxItem(local.inbox, &error));
    CHECK(store.insertDownloadTransferAndInbox(
        local.transfer, local.inbox, &error));
    CHECK(store.upsertInboxItem(prunable.inbox, &error));
    CHECK(store.insertDownloadTransferAndInbox(
        prunable.transfer, prunable.inbox, &error));

    const auto replacementIncoming = incomingFor(
        QStringLiteral("closed:new"), manifest, fixture.clock);
    const SstvPersistentInboxItem replacement = persistentInboxFor(
        replacementIncoming, SstvInboxDisposition::Expired,
        fixture.clock.utc);
    CHECK(store.upsertInboxItem(replacement, &error));
    CHECK(store.reclaimedRows() == 2U);
    CHECK(store.transfer(localId, &error));
    CHECK(store.inboxItem(
        local.inbox.providerId, local.inbox.incomingId, &error));
    CHECK(QFileInfo::exists(localPath));
    CHECK(!store.transfer(prunableId, &error));
    CHECK(!store.inboxItem(
        prunable.inbox.providerId, prunable.inbox.incomingId, &error));
    CHECK(store.inboxItem(
        replacement.providerId, replacement.incomingId, &error));
    CHECK(store.queryInbox(2, &error).size() == 2);
    return true;
}

bool schemaV3ReclaimsMoreThanTenThousandInboxCyclesAndRetainsActive()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    fixture.config.limits.maximumInboxItems = 10'000;
    const QByteArray bytes = pngPayload(20, 16, 'i');
    CHECK(!bytes.isEmpty());
    const auto manifest = manifestFor(
        bytes, fixture.clock, QStringLiteral("station:remote"),
        QStringLiteral("station:local"));
    const auto incomingTemplate = incomingFor(
        QStringLiteral("active:retain"), manifest, fixture.clock);
    const SstvPersistentInboxItem active = persistentInboxFor(
        incomingTemplate, SstvInboxDisposition::New, fixture.clock.utc);
    QString error;
    {
        SstvShareQueueStore store(fixture.config);
        CHECK(store.open(&error));
        CHECK(store.schemaVersion() == 3);
        CHECK(store.upsertInboxItem(active, &error));
    }

    const QString seedConnection = QStringLiteral(
        "sstv_queue_reclaim_seed");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), seedConnection);
        database.setDatabaseName(fixture.config.databasePath);
        CHECK(database.open());
        QSqlQuery schema(database);
        CHECK(schema.exec(QStringLiteral(
            "DROP INDEX IF EXISTS idx_sstv_share_reclaim")));
        CHECK(schema.exec(QStringLiteral(
            "DROP INDEX IF EXISTS idx_sstv_share_inbox_reclaim")));
        CHECK(schema.exec(QStringLiteral("PRAGMA user_version=2")));
        schema.finish();
        CHECK(database.transaction());
        QSqlQuery insert(database);
        insert.prepare(QStringLiteral(
            "INSERT INTO sstv_share_inbox("
            "provider_id,incoming_id,sender_id,manifest_sha256,manifest_json,"
            "byte_size,received_ms,expires_ms,disposition,transfer_id,"
            "updated_ms) VALUES("
            ":provider,:incoming,:sender,:manifest_sha256,:manifest_json,"
            ":byte_size,:received_ms,:expires_ms,'Expired','',:updated_ms)"));
        const qint64 receivedMs = fixture.clock.utc.addDays(-2)
            .toMSecsSinceEpoch();
        const qint64 expiresMs = fixture.clock.utc.addDays(-1)
            .toMSecsSinceEpoch();
        for (int index = 0; index < 10'000; ++index) {
            insert.bindValue(QStringLiteral(":provider"),
                             incomingTemplate.providerId);
            insert.bindValue(QStringLiteral(":incoming"),
                QStringLiteral("bulk:%1").arg(index, 5, 10, QLatin1Char('0')));
            insert.bindValue(QStringLiteral(":sender"),
                             incomingTemplate.senderId);
            insert.bindValue(QStringLiteral(":manifest_sha256"),
                             incomingTemplate.manifestSha256);
            insert.bindValue(QStringLiteral(":manifest_json"),
                             incomingTemplate.canonicalManifestJson);
            insert.bindValue(QStringLiteral(":byte_size"),
                             static_cast<qulonglong>(incomingTemplate.byteSize));
            insert.bindValue(QStringLiteral(":received_ms"), receivedMs);
            insert.bindValue(QStringLiteral(":expires_ms"), expiresMs);
            insert.bindValue(QStringLiteral(":updated_ms"), receivedMs);
            CHECK(insert.exec());
        }
        CHECK(database.commit());
        insert.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(seedConnection);

    const auto firstNewIncoming = incomingFor(
        QStringLiteral("closed:new-a"), manifest, fixture.clock);
    const SstvPersistentInboxItem firstNew = persistentInboxFor(
        firstNewIncoming, SstvInboxDisposition::Expired, fixture.clock.utc);
    {
        SstvShareQueueStore migrated(fixture.config);
        CHECK(migrated.open(&error));
        CHECK(migrated.schemaVersion() == 3);
        CHECK(migrated.upsertInboxItem(firstNew, &error));
        CHECK(migrated.reclaimedRows() == 2U);
        const auto retainedActive = migrated.inboxItem(
            active.providerId, active.incomingId, &error);
        CHECK(retainedActive);
        CHECK(retainedActive->disposition == SstvInboxDisposition::New);
    }

    const QString verifyConnection = QStringLiteral(
        "sstv_queue_reclaim_verify");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), verifyConnection);
        database.setDatabaseName(fixture.config.databasePath);
        CHECK(database.open());
        QSqlQuery query(database);
        CHECK(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sstv_share_inbox")));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 10'000);
        query.finish();
        CHECK(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sstv_share_inbox "
            "WHERE incoming_id IN('bulk:00000','bulk:00001')")));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 0);
        query.finish();
        CHECK(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND "
            "name IN('idx_sstv_share_reclaim',"
            "'idx_sstv_share_inbox_reclaim')")));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 2);
        query.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(verifyConnection);

    const auto secondNewIncoming = incomingFor(
        QStringLiteral("closed:new-b"), manifest, fixture.clock);
    const SstvPersistentInboxItem secondNew = persistentInboxFor(
        secondNewIncoming, SstvInboxDisposition::Expired, fixture.clock.utc);
    {
        SstvShareQueueStore restarted(fixture.config);
        CHECK(restarted.open(&error));
        CHECK(restarted.reclaimedRows() == 0U);
        CHECK(restarted.upsertInboxItem(secondNew, &error));
        CHECK(restarted.reclaimedRows() == 1U);
        CHECK(restarted.inboxItem(
            active.providerId, active.incomingId, &error));
    }
    return true;
}

bool unknownSchemaVersionIsRejectedWithoutDestruction()
{
    Fixture fixture;
    CHECK(fixture.temporary.isValid());
    {
        auto manager = fixture.manager(std::make_shared<FakeBackend>());
        CHECK(manager);
        CHECK(manager->store().schemaVersion() == 3);
    }

    const QString connectionName = QStringLiteral("sstv_queue_schema_test");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(fixture.config.databasePath);
        CHECK(database.open());
        QSqlQuery query(database);
        CHECK(query.exec(QStringLiteral("DROP TABLE sstv_share_sender_blocks")));
        CHECK(query.exec(QStringLiteral("PRAGMA user_version=1")));
        query.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    {
        auto migrated = fixture.manager(std::make_shared<FakeBackend>());
        CHECK(migrated);
        CHECK(migrated->store().schemaVersion() == 3);
    }

    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(fixture.config.databasePath);
        CHECK(database.open());
        QSqlQuery query(database);
        CHECK(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
            "AND name='sstv_share_sender_blocks'")));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 1);
        query.finish();
        CHECK(query.exec(QStringLiteral("PRAGMA user_version=99")));
        query.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QString error;
    SstvShareQueueStore rejected(fixture.config);
    CHECK(!rejected.open(&error));
    CHECK(error.contains(QStringLiteral("unsupported")));

    const QString verifyConnection = QStringLiteral("sstv_queue_schema_verify");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), verifyConnection);
        database.setDatabaseName(fixture.config.databasePath);
        CHECK(database.open());
        QSqlQuery query(database);
        CHECK(query.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
            "AND name IN('sstv_share_transfers','sstv_share_inbox',"
            "'sstv_share_sender_blocks')")));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 3);
        query.finish();
        CHECK(query.exec(QStringLiteral("PRAGMA user_version")));
        CHECK(query.next());
        CHECK(query.value(0).toInt() == 99);
        query.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(verifyConnection);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)
    const std::vector<std::pair<const char*, bool (*)()>> tests {
        {"uploadRetryRestartResumeIsDurableAndRedacted",
         uploadRetryRestartResumeIsDurableAndRedacted},
        {"inboxDownloadAcceptanceAckAndRejectAreExplicit",
         inboxDownloadAcceptanceAckAndRejectAreExplicit},
        {"singleShotUploadRehydratesAndReplaysAfterRestart",
         singleShotUploadRehydratesAndReplaysAfterRestart},
        {"downloadCrashBoundaryResumesFromDurableFileOffset",
         downloadCrashBoundaryResumesFromDurableFileOffset},
        {"completedRemoteCopyRemovalIsCapabilityGatedDurableAndCancellable",
         completedRemoteCopyRemovalIsCapabilityGatedDurableAndCancellable},
        {"cancellationAndDatabaseFaultsAreIdempotentAndNonDestructive",
         cancellationAndDatabaseFaultsAreIdempotentAndNonDestructive},
        {"persistentPauseResumeCoversUploadAndDownload",
         persistentPauseResumeCoversUploadAndDownload},
        {"checksumMismatchNeverBecomesAcceptable",
         checksumMismatchNeverBecomesAcceptable},
        {"meteredPolicyAndBoundedDiagnosticsAreFailClosedAndMonotonic",
         meteredPolicyAndBoundedDiagnosticsAreFailClosedAndMonotonic},
        {"incomingLifecycleIsCapabilityDrivenIdempotentAndLocallyBounded",
         incomingLifecycleIsCapabilityDrivenIdempotentAndLocallyBounded},
        {"terminalReclamationIsDeterministicAndRetainsRetryAcrossRestart",
         terminalReclamationIsDeterministicAndRetainsRetryAcrossRestart},
        {"pairedInboxReclamationIsAtomicAndProtectsLocalCopy",
         pairedInboxReclamationIsAtomicAndProtectsLocalCopy},
        {"schemaV3ReclaimsMoreThanTenThousandInboxCyclesAndRetainsActive",
         schemaV3ReclaimsMoreThanTenThousandInboxCyclesAndRetainsActive},
        {"unknownSchemaVersionIsRejectedWithoutDestruction",
         unknownSchemaVersionIsRejectedWithoutDestruction},
    };

    int failed = 0;
    qsizetype executed = 0;
    for (const auto& test : tests) {
        if (argc > 1 && QByteArray(argv[1]) != test.first) {
            continue;
        }
        ++executed;
        const bool passed = test.second();
        std::cout << (passed ? "PASS " : "FAIL ") << test.first << '\n';
        failed += passed ? 0 : 1;
    }
    std::cout << "checks=" << g_checks << " tests=" << executed
              << " failed=" << failed << '\n';
    return failed == 0 && executed > 0 ? 0 : 1;
}
