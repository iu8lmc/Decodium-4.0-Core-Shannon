// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvLocalIntegrationShareProvider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <utility>

namespace decodium::sstv::sharing {
namespace {

QString sha256Hex(const QByteArray& value)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}

QString deterministicId(const QString& prefix, const QByteArray& seed)
{
    return prefix + sha256Hex(seed).left(32);
}

SstvShareProviderResult notFound()
{
    return SstvShareProviderResult::failure(
        SstvShareProviderFailure::NotFound,
        QStringLiteral("local-integration:not-found"));
}

} // namespace

struct SstvLocalIntegrationShareProvider::State final
{
    struct Session final
    {
        SstvShareManifestV1 manifest;
        QByteArray canonicalManifest;
        QString idempotencyKey;
        QString sessionId;
        QString objectId;
        QByteArray payload;
        quint64 committedBytes {0U};
        bool cancelled {false};
        bool completed {false};
    };

    struct Object final
    {
        SstvShareManifestV1 manifest;
        QByteArray canonicalManifest;
        QString objectId;
        QByteArray payload;
        QDateTime receivedUtc;
        bool acknowledged {false};
        bool rejected {false};
        bool revoked {false};
        bool deleted {false};
    };

    QHash<QString, Session> sessions;
    QHash<QString, QString> sessionsByIdempotency;
    QHash<QString, Object> objects;
    QHash<QString, QString> objectsByIdempotency;
    QSet<QString> blockedSenders;
    QSet<SstvShareOperationId> pending;
    SstvShareOperationId nextOperation {1U};
    quint64 residentPayloadBytes {0U};
};

SstvLocalIntegrationShareProvider::SstvLocalIntegrationShareProvider(
    QObject* parent)
    : SstvLocalIntegrationShareProvider(Config {}, parent)
{
}

SstvLocalIntegrationShareProvider::SstvLocalIntegrationShareProvider(
    Config config, QObject* parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_state(std::make_unique<State>())
{
    m_valid = isSafeShareIdentifier(m_config.localRecipientId)
        && !m_config.participantIds.isEmpty()
        && m_config.participantIds.size() <= 128
        && m_config.participantIds.contains(m_config.localRecipientId)
        && m_config.maximumChunkBytes > 0U
        && m_config.maximumChunkBytes <= kMaximumSharedImageBytes
        && m_config.maximumTotalBytes >= m_config.maximumChunkBytes
        && m_config.maximumTotalBytes <= 256U * 1024U * 1024U
        && m_config.maximumSessions > 0
        && m_config.maximumSessions <= 1'024
        && m_config.maximumObjects > 0
        && m_config.maximumObjects <= 1'024
        && m_config.maximumPendingOperations > 0
        && m_config.maximumPendingOperations <= 4'096
        && static_cast<quint64>(m_config.maximumPendingOperations)
            <= m_config.maximumTotalBytes / m_config.maximumChunkBytes
        && m_config.firstOperationId != 0U
        && m_config.completionDelayMs >= 0
        && m_config.completionDelayMs <= 1'000;
    for (const QString& participant : std::as_const(m_config.participantIds)) {
        m_valid = m_valid && isSafeShareIdentifier(participant);
    }
    m_state->nextOperation = m_config.firstOperationId;
}

SstvLocalIntegrationShareProvider::~SstvLocalIntegrationShareProvider() = default;

bool SstvLocalIntegrationShareProvider::isConfigurationValid() const noexcept
{
    return m_valid;
}

QString SstvLocalIntegrationShareProvider::providerId() const
{
    return QStringLiteral("local-integration");
}

SstvShareProviderCapabilities
SstvLocalIntegrationShareProvider::capabilities() const
{
    SstvShareProviderCapabilities value;
    value.recipientLookup = true;
    value.chunkedUpload = true;
    value.resumableUpload = true;
    value.download = true;
    value.acknowledgement = true;
    value.rejection = true;
    value.incomingDelete = true;
    value.senderBlocking = true;
    value.revocation = true;
    value.remoteDelete = true;
    value.incomingList = true;
    value.endToEndEncryptionEnvelope = false;
    // The compatibility contract requires providers to reject manifests that
    // weaken transport security. This adapter has no transport at all.
    value.strictTlsRequired = true;
    value.maximumChunkBytes = m_config.maximumChunkBytes;
    value.maximumResponseBytes = std::min(
        m_config.maximumChunkBytes, kMaximumSharedImageBytes);
    return value;
}

SstvShareAuthenticationStatus
SstvLocalIntegrationShareProvider::authenticationStatus() const
{
    return m_valid ? SstvShareAuthenticationStatus::NotRequired
                   : SstvShareAuthenticationStatus::Unavailable;
}

SstvShareOperationId SstvLocalIntegrationShareProvider::lookupRecipientAsync(
    const QString& stableRecipientId,
    SstvShareRecipientCompletion completion)
{
    return schedule([this, stableRecipientId,
                     completion = std::move(completion)]() mutable {
        if (!m_valid || !isSafeShareIdentifier(stableRecipientId)
            || !m_config.participantIds.contains(stableRecipientId)) {
            completion(SstvShareProviderResult::failure(
                           SstvShareProviderFailure::RejectedRecipient,
                           QStringLiteral("local-integration:unknown-recipient")),
                       {});
            return;
        }
        SstvShareRecipientRecord recipient;
        recipient.providerId = providerId();
        recipient.stableRecipientId = stableRecipientId;
        recipient.displayName = QStringLiteral("Local integration participant");
        recipient.verification = SstvShareRecipientVerification::UserVerified;
        recipient.trust = SstvShareRecipientTrust::Trusted;
        completion(SstvShareProviderResult::success(), std::move(recipient));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::createUploadAsync(
    const SstvShareManifestV1& manifest,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    return schedule([this, manifest, idempotencyKey,
                     completion = std::move(completion)]() mutable {
        const QByteArray canonical = manifest.toCanonicalJson();
        const SstvShareValidationError manifestError = manifest.validate(
            true, nowUtc());
        if (!m_valid || !manifestError.ok() || canonical.isEmpty()
            || manifest.providerId != providerId()
            || !isLowercaseSha256(idempotencyKey)
            || !m_config.participantIds.contains(manifest.senderId)
            || !m_config.participantIds.contains(manifest.recipientId)
            || !validateShareProviderCompatibility(
                    manifest, providerId(), capabilities()).ok()) {
            completion(invalid(QStringLiteral("create-validation")));
            return;
        }
        const auto existingKey = m_state->sessionsByIdempotency.constFind(
            idempotencyKey);
        if (existingKey != m_state->sessionsByIdempotency.constEnd()) {
            const auto session = m_state->sessions.constFind(*existingKey);
            if (session == m_state->sessions.constEnd()
                || session->canonicalManifest != canonical) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("local-integration:idempotency-conflict")));
                return;
            }
            completion(SstvShareProviderResult::success(
                {session->sessionId,
                 session->committedBytes}));
            return;
        }
        if (m_state->sessions.size() >= m_config.maximumSessions) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::RateLimited,
                QStringLiteral("local-integration:session-bound")));
            return;
        }
        State::Session session;
        session.manifest = manifest;
        session.canonicalManifest = canonical;
        session.idempotencyKey = idempotencyKey;
        session.sessionId = deterministicId(
            QStringLiteral("local-upload:"),
            idempotencyKey.toUtf8() + canonical);
        m_state->sessionsByIdempotency.insert(idempotencyKey,
                                              session.sessionId);
        m_state->sessions.insert(session.sessionId, session);
        completion(SstvShareProviderResult::success(
            {session.sessionId, 0U}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::uploadChunkAsync(
    const SstvShareUploadHandle& handle,
    quint64 offset,
    const QByteArray& chunk,
    const QString& chunkSha256,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    return schedule([this, handle, offset, chunk, chunkSha256,
                     progress = std::move(progress),
                     completion = std::move(completion)]() mutable {
        auto session = m_state->sessions.find(handle.opaqueId);
        if (session == m_state->sessions.end()) {
            completion(notFound());
            return;
        }
        const quint64 committed = session->committedBytes;
        const quint64 chunkBytes = static_cast<quint64>(chunk.size());
        if (session->cancelled || session->completed || chunk.isEmpty()
            || chunkBytes > m_config.maximumChunkBytes
            || offset > committed
            || chunkBytes > session->manifest.byteSize
            || offset > session->manifest.byteSize - chunkBytes
            || !isLowercaseSha256(chunkSha256)
            || sha256Hex(chunk) != chunkSha256) {
            completion(invalid(QStringLiteral("chunk-validation")));
            return;
        }
        if (offset < committed) {
            const quint64 end = offset + chunkBytes;
            if (end > committed
                || session->payload.mid(static_cast<qsizetype>(offset),
                                        chunk.size()) != chunk) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("local-integration:chunk-conflict")));
                return;
            }
        } else {
            if (chunkBytes > m_config.maximumTotalBytes
                || m_state->residentPayloadBytes
                    > m_config.maximumTotalBytes - chunkBytes) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::RateLimited,
                    QStringLiteral("local-integration:resident-byte-bound")));
                return;
            }
            session->payload.append(chunk);
            session->committedBytes += chunkBytes;
            m_state->residentPayloadBytes += chunkBytes;
        }
        const quint64 updated = session->committedBytes;
        if (progress) {
            progress(updated, session->manifest.byteSize);
        }
        completion(SstvShareProviderResult::success(
            {session->sessionId, updated}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::resumeUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    return schedule([this, handle,
                     completion = std::move(completion)]() mutable {
        const auto session = m_state->sessions.constFind(handle.opaqueId);
        if (session == m_state->sessions.constEnd() || session->cancelled) {
            completion(notFound());
            return;
        }
        completion(SstvShareProviderResult::success(
            {session->sessionId,
             session->committedBytes}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::completeUploadAsync(
    const SstvShareUploadHandle& handle,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    return schedule([this, handle, idempotencyKey,
                     completion = std::move(completion)]() mutable {
        auto session = m_state->sessions.find(handle.opaqueId);
        if (session == m_state->sessions.end() || session->cancelled
            || session->idempotencyKey != idempotencyKey) {
            completion(invalid(QStringLiteral("complete-validation")));
            return;
        }
        if (session->completed) {
            const auto completedObject = m_state->objects.constFind(
                session->objectId);
            if (completedObject == m_state->objects.constEnd()
                || completedObject->deleted) {
                completion(notFound());
                return;
            }
            if (completedObject->canonicalManifest
                != session->canonicalManifest) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("local-integration:object-conflict")));
                return;
            }
            completion(SstvShareProviderResult::success(
                {completedObject->objectId, session->committedBytes}));
            return;
        }
        const QDateTime completionUtc = nowUtc();
        if (session->committedBytes != session->manifest.byteSize
            || static_cast<quint64>(session->payload.size())
                != session->committedBytes
            || sha256Hex(session->payload) != session->manifest.sha256
            || session->manifest.expiresUtc <= completionUtc) {
            completion(invalid(QStringLiteral("complete-validation")));
            return;
        }
        const auto existingObject = m_state->objectsByIdempotency.constFind(
            idempotencyKey);
        if (existingObject != m_state->objectsByIdempotency.constEnd()) {
            const auto object = m_state->objects.constFind(*existingObject);
            if (object == m_state->objects.constEnd()
                || object->deleted
                || object->canonicalManifest != session->canonicalManifest
                || object->payload != session->payload) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("local-integration:object-conflict")));
                return;
            }
            completion(SstvShareProviderResult::success(
                {object->objectId,
                 session->committedBytes}));
            return;
        }
        if (m_state->objects.size() >= m_config.maximumObjects) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::RateLimited,
                QStringLiteral("local-integration:object-bound")));
            return;
        }
        State::Object object;
        object.manifest = session->manifest;
        object.canonicalManifest = session->canonicalManifest;
        object.objectId = deterministicId(
            QStringLiteral("local-object:"),
            idempotencyKey.toUtf8() + session->manifest.sha256.toUtf8());
        object.payload = std::move(session->payload);
        object.receivedUtc = completionUtc;
        session->completed = true;
        session->objectId = object.objectId;
        m_state->objectsByIdempotency.insert(idempotencyKey,
                                             object.objectId);
        m_state->objects.insert(object.objectId, object);
        completion(SstvShareProviderResult::success(
            {object.objectId, session->committedBytes}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::cancelUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    return schedule([this, handle,
                     completion = std::move(completion)]() mutable {
        auto session = m_state->sessions.find(handle.opaqueId);
        if (session == m_state->sessions.end()) {
            completion(notFound());
            return;
        }
        if (session->completed) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Conflict,
                QStringLiteral("local-integration:already-completed")));
            return;
        }
        if (!session->cancelled) {
            const quint64 bytes = static_cast<quint64>(session->payload.size());
            m_state->residentPayloadBytes =
                bytes > m_state->residentPayloadBytes
                    ? 0U : m_state->residentPayloadBytes - bytes;
            session->payload.clear();
            session->committedBytes = 0U;
        }
        session->cancelled = true;
        completion(SstvShareProviderResult::success(
            {session->sessionId, 0U}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::queryStatusAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    return resumeUploadAsync(handle, std::move(completion));
}

SstvShareOperationId SstvLocalIntegrationShareProvider::downloadAsync(
    const QString& opaqueIncomingId,
    quint64 offset,
    quint64 maximumBytes,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    return schedule([this, opaqueIncomingId, offset, maximumBytes,
                     progress = std::move(progress),
                     completion = std::move(completion)]() mutable {
        const auto object = m_state->objects.constFind(opaqueIncomingId);
        if (object == m_state->objects.constEnd() || object->deleted
            || object->revoked || object->rejected
            || object->manifest.recipientId != m_config.localRecipientId
            || m_state->blockedSenders.contains(object->manifest.senderId)
            || object->manifest.expiresUtc <= nowUtc()) {
            completion(notFound());
            return;
        }
        const quint64 size = static_cast<quint64>(object->payload.size());
        if (maximumBytes == 0U
            || maximumBytes > capabilities().maximumResponseBytes
            || offset >= size) {
            completion(invalid(QStringLiteral("download-range")));
            return;
        }
        const quint64 length = std::min(maximumBytes, size - offset);
        const QByteArray payload = object->payload.mid(
            static_cast<qsizetype>(offset), static_cast<qsizetype>(length));
        if (progress) {
            progress(offset + length, size);
        }
        completion(SstvShareProviderResult::success({}, payload));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::acknowledgeAsync(
    const QString& opaqueIncomingId,
    SstvShareProviderCompletion completion)
{
    return schedule([this, opaqueIncomingId,
                     completion = std::move(completion)]() mutable {
        auto object = m_state->objects.find(opaqueIncomingId);
        if (object == m_state->objects.end() || object->deleted
            || object->revoked || object->rejected) {
            completion(notFound());
            return;
        }
        object->acknowledged = true;
        completion(SstvShareProviderResult::success(
            {object->objectId, 0U}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::rejectAsync(
    const QString& opaqueIncomingId,
    SstvShareProviderCompletion completion)
{
    return schedule([this, opaqueIncomingId,
                     completion = std::move(completion)]() mutable {
        auto object = m_state->objects.find(opaqueIncomingId);
        if (object == m_state->objects.end() || object->deleted) {
            completion(notFound());
            return;
        }
        if (!object->rejected) {
            const quint64 bytes = static_cast<quint64>(object->payload.size());
            m_state->residentPayloadBytes =
                bytes > m_state->residentPayloadBytes
                    ? 0U : m_state->residentPayloadBytes - bytes;
            object->payload.clear();
            object->rejected = true;
        }
        completion(SstvShareProviderResult::success(
            {object->objectId, 0U}));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::deleteIncomingAsync(
    const QString& opaqueIncomingId,
    SstvShareProviderCompletion completion)
{
    return deleteRemoteObjectAsync(opaqueIncomingId, std::move(completion));
}

SstvShareOperationId SstvLocalIntegrationShareProvider::blockSenderAsync(
    const QString& senderId,
    SstvShareProviderCompletion completion)
{
    return schedule([this, senderId,
                     completion = std::move(completion)]() mutable {
        const bool known = isSafeShareIdentifier(senderId)
            && std::any_of(m_state->objects.cbegin(), m_state->objects.cend(),
                           [&senderId](const State::Object& object) {
                               return object.manifest.senderId == senderId;
                           });
        if (!known) {
            completion(notFound());
            return;
        }
        m_state->blockedSenders.insert(senderId);
        completion(SstvShareProviderResult::success());
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::revokeAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    return schedule([this, handle,
                     completion = std::move(completion)]() mutable {
        QString objectId = handle.opaqueId;
        const auto session = m_state->sessions.constFind(handle.opaqueId);
        if (session != m_state->sessions.constEnd()) {
            objectId = session->objectId;
        }
        auto object = m_state->objects.find(objectId);
        if (object == m_state->objects.end()) {
            completion(notFound());
            return;
        }
        if (!object->revoked) {
            const quint64 bytes = static_cast<quint64>(object->payload.size());
            m_state->residentPayloadBytes =
                bytes > m_state->residentPayloadBytes
                    ? 0U : m_state->residentPayloadBytes - bytes;
            object->payload.clear();
            object->revoked = true;
        }
        completion(SstvShareProviderResult::success(
            {object->objectId, 0U}));
    });
}

SstvShareOperationId
SstvLocalIntegrationShareProvider::deleteRemoteObjectAsync(
    const QString& opaqueId,
    SstvShareProviderCompletion completion)
{
    return schedule([this, opaqueId,
                     completion = std::move(completion)]() mutable {
        auto object = m_state->objects.find(opaqueId);
        if (object == m_state->objects.end()) {
            completion(notFound());
            return;
        }
        if (!object->deleted) {
            const quint64 bytes = static_cast<quint64>(object->payload.size());
            m_state->residentPayloadBytes =
                bytes > m_state->residentPayloadBytes
                    ? 0U : m_state->residentPayloadBytes - bytes;
            object->payload.clear();
            object->deleted = true;
        }
        completion(SstvShareProviderResult::success(
            {object->objectId, 0U}));
    });
}

SstvShareOperationId
SstvLocalIntegrationShareProvider::refreshCredentialsAsync(
    SstvShareProviderCompletion completion)
{
    return schedule([this, completion = std::move(completion)]() mutable {
        completion(m_valid ? SstvShareProviderResult::success()
                           : invalid(QStringLiteral("configuration")));
    });
}

SstvShareOperationId SstvLocalIntegrationShareProvider::listIncomingAsync(
    qsizetype maximumItems,
    SstvShareIncomingCompletion completion)
{
    return schedule([this, maximumItems,
                     completion = std::move(completion)]() mutable {
        if (!m_valid || maximumItems <= 0 || maximumItems > 4'096) {
            completion(invalid(QStringLiteral("list-bound")), {});
            return;
        }
        const qsizetype resultLimit = std::min(
            maximumItems, m_config.maximumObjects);
        QVector<SstvShareIncomingItem> items;
        items.reserve(std::min(resultLimit, m_state->objects.size()));
        for (const State::Object& object : std::as_const(m_state->objects)) {
            if (items.size() >= resultLimit) {
                break;
            }
            if (object.deleted || object.revoked || object.rejected
                || object.manifest.recipientId != m_config.localRecipientId
                || object.manifest.expiresUtc <= nowUtc()
                || m_state->blockedSenders.contains(
                    object.manifest.senderId)) {
                continue;
            }
            SstvShareIncomingItem item;
            item.opaqueId = object.objectId;
            item.providerId = providerId();
            item.senderId = object.manifest.senderId;
            item.manifestSha256 = sha256Hex(object.canonicalManifest);
            item.canonicalManifestJson = object.canonicalManifest;
            item.byteSize = static_cast<quint64>(object.payload.size());
            item.receivedUtc = object.receivedUtc;
            item.expiresUtc = object.manifest.expiresUtc;
            if (validateShareIncomingItem(item).ok()) {
                items.push_back(std::move(item));
            }
        }
        std::sort(items.begin(), items.end(),
                  [](const SstvShareIncomingItem& left,
                     const SstvShareIncomingItem& right) {
            if (left.receivedUtc != right.receivedUtc) {
                return left.receivedUtc < right.receivedUtc;
            }
            return left.opaqueId < right.opaqueId;
        });
        completion(SstvShareProviderResult::success(), std::move(items));
    });
}

bool SstvLocalIntegrationShareProvider::cancelOperation(
    SstvShareOperationId operationId)
{
    return operationId != 0U && m_state->pending.remove(operationId);
}

SstvShareOperationId SstvLocalIntegrationShareProvider::schedule(
    std::function<void()> callback)
{
    if (!callback || !QCoreApplication::instance()
        || m_state->pending.size() >= m_config.maximumPendingOperations
        || m_state->nextOperation == 0U) {
        return 0U;
    }
    const SstvShareOperationId operationId = m_state->nextOperation;
    if (m_state->pending.contains(operationId)) {
        m_state->nextOperation = 0U;
        return 0U;
    }
    if (operationId
        == std::numeric_limits<SstvShareOperationId>::max()) {
        m_state->nextOperation = 0U;
    } else {
        ++m_state->nextOperation;
    }
    m_state->pending.insert(operationId);
    const QPointer<SstvLocalIntegrationShareProvider> guard(this);
    QTimer::singleShot(m_config.completionDelayMs, this,
        [guard, operationId, callback = std::move(callback)]() mutable {
            if (guard && guard->m_state->pending.remove(operationId)) {
                guard->reclaimExpired();
                callback();
            }
        });
    return operationId;
}

void SstvLocalIntegrationShareProvider::reclaimExpired()
{
    const QDateTime now = nowUtc();
    for (auto session = m_state->sessions.begin();
         session != m_state->sessions.end(); ++session) {
        if (!session->cancelled && !session->completed
            && session->manifest.expiresUtc <= now) {
            const quint64 bytes = static_cast<quint64>(session->payload.size());
            m_state->residentPayloadBytes =
                bytes > m_state->residentPayloadBytes
                    ? 0U : m_state->residentPayloadBytes - bytes;
            session->payload.clear();
            session->committedBytes = 0U;
            session->cancelled = true;
        }
    }
    for (auto object = m_state->objects.begin();
         object != m_state->objects.end(); ++object) {
        if (!object->deleted && object->manifest.expiresUtc <= now) {
            const quint64 bytes = static_cast<quint64>(object->payload.size());
            m_state->residentPayloadBytes =
                bytes > m_state->residentPayloadBytes
                    ? 0U : m_state->residentPayloadBytes - bytes;
            object->payload.clear();
            object->deleted = true;
        }
    }
}

QDateTime SstvLocalIntegrationShareProvider::nowUtc() const
{
    const QDateTime value = m_config.clock
        ? m_config.clock() : QDateTime::currentDateTimeUtc();
    return value.isValid() ? value.toUTC()
                           : QDateTime::currentDateTimeUtc();
}

SstvShareProviderResult SstvLocalIntegrationShareProvider::invalid(
    const QString& reasonCode)
{
    return SstvShareProviderResult::failure(
        SstvShareProviderFailure::Validation,
        QStringLiteral("local-integration:") + reasonCode);
}

} // namespace decodium::sstv::sharing
