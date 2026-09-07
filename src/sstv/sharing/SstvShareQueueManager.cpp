// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvShareQueueManager.h"

#include "SstvIncomingMediaValidator.h"
#include "SstvShareSecurity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace decodium::sstv::sharing {
namespace {

constexpr int kShareQueueSchemaVersion = 3;
constexpr qsizetype kMaximumStoredRecipientJsonBytes = 16 * 1024;
constexpr qsizetype kMaximumStoredPathCharacters = 4 * 1024;
constexpr qsizetype kFileHashReadBlockBytes = 256 * 1024;
constexpr quint64 kMaximumDiagnosticsCounter =
    (quint64 {1U} << 53U) - 1U;

SstvManagedTransferState managedStateForCore(SstvShareTransferState state);

bool fail(QString* error, const QString& detail)
{
    if (error) {
        *error = redactShareSecrets(detail).left(512);
    }
    return false;
}

bool isCanonicalUuid(const QString& value)
{
    const QUuid uuid(value);
    return value.size() == 36 && value == value.toLower() && !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces) == value;
}

bool utcDateTime(const QDateTime& value)
{
    return value.isValid() && value.offsetFromUtc() == 0;
}

qint64 toMs(const QDateTime& value)
{
    return value.isValid() ? value.toUTC().toMSecsSinceEpoch() : 0;
}

QDateTime fromMs(qint64 value)
{
    return value > 0 ? QDateTime::fromMSecsSinceEpoch(value, QTimeZone::UTC)
                     : QDateTime {};
}

QString absoluteCleanPath(const QString& path)
{
    return path.isEmpty()
        ? QString {}
        : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool relativePathEscapes(const QString& relative)
{
    const QString cleaned = QDir::cleanPath(relative);
    return relative.isEmpty() || QDir::isAbsolutePath(relative)
        || cleaned == QStringLiteral(".") || cleaned == QStringLiteral("..")
        || cleaned.startsWith(QStringLiteral("../"))
        || cleaned.startsWith(QStringLiteral("..\\"))
        || relative.contains(QLatin1Char('\0'));
}

bool pathWithin(const QString& canonicalRoot,
                const QString& candidate,
                bool candidateMustExist)
{
    const QFileInfo rootInfo(canonicalRoot);
    if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()) {
        return false;
    }
    const QString root = QDir::cleanPath(rootInfo.canonicalFilePath());
    if (root.isEmpty()) {
        return false;
    }
    const QFileInfo candidateInfo(candidate);
    QString resolved;
    if (candidateMustExist) {
        if (!candidateInfo.exists() || candidateInfo.isSymLink()) {
            return false;
        }
        resolved = QDir::cleanPath(candidateInfo.canonicalFilePath());
    } else {
        const QFileInfo parentInfo(candidateInfo.absolutePath());
        if (!parentInfo.exists() || !parentInfo.isDir()
            || parentInfo.isSymLink()) {
            return false;
        }
        const QString canonicalParent = QDir::cleanPath(
            parentInfo.canonicalFilePath());
        if (canonicalParent.isEmpty()) {
            return false;
        }
        resolved = QDir(canonicalParent).absoluteFilePath(
            candidateInfo.fileName());
        resolved = QDir::cleanPath(resolved);
    }
    const QString relative = QDir(root).relativeFilePath(resolved);
    return !relativePathEscapes(relative);
}

bool ensurePlainDirectory(const QString& path, QString* error)
{
    const QString absolute = absoluteCleanPath(path);
    if (absolute.isEmpty() || absolute.size() > kMaximumStoredPathCharacters) {
        return fail(error, QStringLiteral("invalid sharing directory"));
    }
    const QFileInfo before(absolute);
    if (before.exists() && (before.isSymLink() || !before.isDir())) {
        return fail(error, QStringLiteral("sharing directory is not a plain directory"));
    }
    if (!before.exists() && !QDir().mkpath(absolute)) {
        return fail(error, QStringLiteral("could not create sharing directory"));
    }
    const QFileInfo after(absolute);
    if (!after.exists() || !after.isDir() || after.isSymLink()
        || after.canonicalFilePath().isEmpty()) {
        return fail(error, QStringLiteral("sharing directory failed canonical validation"));
    }
    return true;
}

std::optional<QString> secureUploadPath(
    const SstvShareQueueConfig& config,
    const QString& path,
    QString* error)
{
    const QFileInfo info(path);
    if (!info.isAbsolute() || !info.exists() || !info.isFile()
        || info.isSymLink() || info.size() <= 0
        || info.size() > static_cast<qint64>(kMaximumSharedImageBytes)) {
        fail(error, QStringLiteral("upload source is not a bounded regular file"));
        return std::nullopt;
    }
    const QString canonical = QDir::cleanPath(info.canonicalFilePath());
    if (canonical.isEmpty() || canonical.size() > kMaximumStoredPathCharacters) {
        fail(error, QStringLiteral("upload source has an invalid canonical path"));
        return std::nullopt;
    }
    for (const QString& configuredRoot : config.allowedUploadRoots) {
        const QFileInfo rootInfo(configuredRoot);
        if (!rootInfo.exists() || rootInfo.isSymLink() || !rootInfo.isDir()) {
            continue;
        }
        if (pathWithin(rootInfo.canonicalFilePath(), canonical, true)) {
            return canonical;
        }
    }
    fail(error, QStringLiteral("upload source is outside every allowed root"));
    return std::nullopt;
}

bool storedUploadPathIsBounded(const SstvShareQueueConfig& config,
                               const QString& path)
{
    if (!QFileInfo(path).isAbsolute()
        || absoluteCleanPath(path) != path
        || path.size() > kMaximumStoredPathCharacters) {
        return false;
    }
    const QFileInfo candidate(path);
    if (candidate.isSymLink()
        || (candidate.exists() && !candidate.isFile())) {
        return false;
    }
    for (const QString& configuredRoot : config.allowedUploadRoots) {
        const QFileInfo rootInfo(configuredRoot);
        const QString root = QDir::cleanPath(rootInfo.canonicalFilePath());
        if (root.isEmpty()) {
            continue;
        }
        const QString relative = QDir(root).relativeFilePath(path);
        if (relativePathEscapes(relative)) {
            continue;
        }
        return !candidate.exists()
            || pathWithin(root, path, true);
    }
    return false;
}

std::optional<QString> secureDestinationPath(
    const SstvShareQueueConfig& config,
    const QString& relative,
    const QString& mimeType,
    QString* error)
{
    if (relativePathEscapes(relative) || relative.size() > 512
        || relative.contains(QLatin1Char('\\'))) {
        fail(error, QStringLiteral("download destination escapes its storage root"));
        return std::nullopt;
    }
    const QString expectedSuffix = mimeType == QStringLiteral("image/png")
        ? QStringLiteral(".png") : QStringLiteral(".jpg");
    if (!relative.endsWith(expectedSuffix, Qt::CaseInsensitive)) {
        fail(error, QStringLiteral("download destination extension does not match MIME"));
        return std::nullopt;
    }
    const QFileInfo rootInfo(config.downloadRoot);
    const QString root = QDir::cleanPath(rootInfo.canonicalFilePath());
    if (root.isEmpty()) {
        fail(error, QStringLiteral("download root is not canonical"));
        return std::nullopt;
    }
    const QString destination = QDir::cleanPath(
        QDir(root).absoluteFilePath(QDir::cleanPath(relative)));
    const QString parent = QFileInfo(destination).absolutePath();
    if (!ensurePlainDirectory(parent, error)
        || !pathWithin(root, destination, false)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("download destination is outside its root");
        }
        return std::nullopt;
    }
    const QFileInfo destinationInfo(destination);
    if (destinationInfo.exists() || destinationInfo.isSymLink()) {
        fail(error, QStringLiteral("download destination already exists"));
        return std::nullopt;
    }
    return destination;
}

std::optional<QString> sha256File(const QString& path,
                                  quint64 maximumBytes,
                                  quint64* bytesRead,
                                  QString* error)
{
    if (bytesRead) {
        *bytesRead = 0U;
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isSymLink()
        || info.size() <= 0
        || static_cast<quint64>(info.size()) > maximumBytes) {
        fail(error, QStringLiteral("file is missing, linked or oversized"));
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("could not open bounded media file"));
        return std::nullopt;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    quint64 total = 0U;
    while (!file.atEnd()) {
        const QByteArray block = file.read(kFileHashReadBlockBytes);
        if (block.isEmpty() && file.error() != QFileDevice::NoError) {
            fail(error, QStringLiteral("could not read bounded media file"));
            return std::nullopt;
        }
        if (static_cast<quint64>(block.size()) > maximumBytes - total) {
            fail(error, QStringLiteral("media file exceeded its declared bound"));
            return std::nullopt;
        }
        total += static_cast<quint64>(block.size());
        hash.addData(block);
    }
    if (total == 0U || total != static_cast<quint64>(info.size())) {
        fail(error, QStringLiteral("media file changed while hashing"));
        return std::nullopt;
    }
    if (bytesRead) {
        *bytesRead = total;
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool execSql(QSqlDatabase& database,
             const QString& sql,
             QString* error)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        return fail(error, QStringLiteral("SQLite operation failed: %1")
                               .arg(query.lastError().text()));
    }
    return true;
}

bool beginTransaction(QSqlDatabase& database, QString* error)
{
    return database.transaction()
        || fail(error, QStringLiteral("could not begin SQLite transaction"));
}

bool commitTransaction(QSqlDatabase& database,
                       const std::shared_ptr<SstvShareFaultInjector>& fault,
                       const QString& transferId,
                       QString* error)
{
    if (fault && fault->shouldFail(
            SstvShareFaultPoint::BeforeDatabaseCommit, transferId)) {
        database.rollback();
        return fail(error, QStringLiteral("injected database commit boundary"));
    }
    if (!database.commit()) {
        database.rollback();
        return fail(error, QStringLiteral("could not commit SQLite transaction"));
    }
    return true;
}

bool managedStateFromName(const QString& text,
                          SstvManagedTransferState* output)
{
    if (!output) {
        return false;
    }
    for (SstvManagedTransferState state : {
             SstvManagedTransferState::Queued,
             SstvManagedTransferState::Preparing,
             SstvManagedTransferState::Uploading,
             SstvManagedTransferState::WaitingForAcknowledgement,
             SstvManagedTransferState::DownloadQueued,
             SstvManagedTransferState::Downloading,
             SstvManagedTransferState::AwaitingAcceptance,
             SstvManagedTransferState::Accepted,
             SstvManagedTransferState::Acknowledging,
             SstvManagedTransferState::Rejecting,
             SstvManagedTransferState::RetryScheduled,
             SstvManagedTransferState::Paused,
             SstvManagedTransferState::Completed,
             SstvManagedTransferState::RemoteDeleted,
             SstvManagedTransferState::RemoteRevoked,
             SstvManagedTransferState::Acknowledged,
             SstvManagedTransferState::Cancelled,
             SstvManagedTransferState::Rejected,
             SstvManagedTransferState::Expired,
             SstvManagedTransferState::Failed}) {
        if (sstvManagedTransferStateName(state) == text) {
            *output = state;
            return true;
        }
    }
    return false;
}

bool inboxDispositionFromName(const QString& text,
                              SstvInboxDisposition* output)
{
    if (!output) {
        return false;
    }
    for (SstvInboxDisposition disposition : {
             SstvInboxDisposition::New,
             SstvInboxDisposition::DownloadQueued,
             SstvInboxDisposition::AwaitingAcceptance,
             SstvInboxDisposition::Accepted,
             SstvInboxDisposition::Acknowledged,
             SstvInboxDisposition::Cancelled,
             SstvInboxDisposition::Rejected,
             SstvInboxDisposition::Expired,
             SstvInboxDisposition::BlockedLocally,
             SstvInboxDisposition::BlockedByProvider,
             SstvInboxDisposition::ProviderDeleted}) {
        if (sstvInboxDispositionName(disposition) == text) {
            *output = disposition;
            return true;
        }
    }
    return false;
}

bool senderBlockScopeFromName(const QString& text,
                              SstvSenderBlockScope* output)
{
    if (!output) {
        return false;
    }
    for (const SstvSenderBlockScope scope : {
             SstvSenderBlockScope::LocalOnly,
             SstvSenderBlockScope::Provider}) {
        if (sstvSenderBlockScopeName(scope) == text) {
            *output = scope;
            return true;
        }
    }
    return false;
}

bool providerFailureFromName(const QString& text,
                             SstvShareProviderFailure* output)
{
    if (!output) {
        return false;
    }
    for (SstvShareProviderFailure value : {
             SstvShareProviderFailure::None,
             SstvShareProviderFailure::TransientNetwork,
             SstvShareProviderFailure::ProviderUnavailable,
             SstvShareProviderFailure::Offline,
             SstvShareProviderFailure::RateLimited,
             SstvShareProviderFailure::Authentication,
             SstvShareProviderFailure::Authorization,
             SstvShareProviderFailure::Validation,
             SstvShareProviderFailure::RejectedRecipient,
             SstvShareProviderFailure::Conflict,
             SstvShareProviderFailure::NotFound,
             SstvShareProviderFailure::Integrity,
             SstvShareProviderFailure::TlsValidation,
             SstvShareProviderFailure::Cancelled,
             SstvShareProviderFailure::PermanentProviderFailure}) {
        if (sstvShareProviderFailureName(value) == text) {
            *output = value;
            return true;
        }
    }
    return false;
}

bool directionStateCompatible(const SstvManagedTransferRecord& record)
{
    if (record.direction == SstvManagedTransferDirection::Upload) {
        return record.state != SstvManagedTransferState::DownloadQueued
            && record.state != SstvManagedTransferState::Downloading
            && record.state != SstvManagedTransferState::AwaitingAcceptance
            && record.state != SstvManagedTransferState::Accepted
            && record.state != SstvManagedTransferState::Acknowledging
            && record.state != SstvManagedTransferState::Rejecting
            && record.state != SstvManagedTransferState::Acknowledged;
    }
    return record.state != SstvManagedTransferState::Queued
        && record.state != SstvManagedTransferState::Preparing
        && record.state != SstvManagedTransferState::Uploading
        && record.state != SstvManagedTransferState::WaitingForAcknowledgement
        && record.state != SstvManagedTransferState::Completed
        && record.state != SstvManagedTransferState::RemoteDeleted
        && record.state != SstvManagedTransferState::RemoteRevoked;
}

bool validStoredRecipientJson(const QByteArray& json,
                              const QString& providerId,
                              const QString& recipientId)
{
    if (json.isEmpty()) {
        return true;
    }
    const auto parsed = parseBoundedJsonObject(
        json, kMaximumStoredRecipientJsonBytes, 4, 64);
    if (!parsed.ok()) {
        return false;
    }
    static const QSet<QString> allowed {
        QStringLiteral("displayCallsign"), QStringLiteral("displayName"),
        QStringLiteral("providerId"), QStringLiteral("stableRecipientId"),
        QStringLiteral("verification"), QStringLiteral("trust"),
        QStringLiteral("publicEncryptionKey"),
        QStringLiteral("publicKeyFingerprint"),
    };
    if (parsed.object.size() != allowed.size()) {
        return false;
    }
    for (auto it = parsed.object.constBegin(); it != parsed.object.constEnd(); ++it) {
        if (!allowed.contains(it.key()) || !it->isString()
            || it->toString().size() > 8'192
            || containsNetworkUrl(it->toString())) {
            return false;
        }
    }
    bool verificationOk = false;
    bool trustOk = false;
    const int verification = parsed.object.value(
        QStringLiteral("verification")).toString().toInt(&verificationOk);
    const int trust = parsed.object.value(
        QStringLiteral("trust")).toString().toInt(&trustOk);
    if (!verificationOk || !trustOk
        || verification < static_cast<int>(
            SstvShareRecipientVerification::Unknown)
        || verification > static_cast<int>(
            SstvShareRecipientVerification::UserVerified)
        || trust < static_cast<int>(SstvShareRecipientTrust::Unknown)
        || trust > static_cast<int>(SstvShareRecipientTrust::Blocked)) {
        return false;
    }
    SstvShareRecipientRecord recipient;
    recipient.providerId = parsed.object.value(
        QStringLiteral("providerId")).toString();
    recipient.stableRecipientId = parsed.object.value(
        QStringLiteral("stableRecipientId")).toString();
    recipient.displayCallsign = parsed.object.value(
        QStringLiteral("displayCallsign")).toString();
    recipient.displayName = parsed.object.value(
        QStringLiteral("displayName")).toString();
    recipient.publicEncryptionKey = parsed.object.value(
        QStringLiteral("publicEncryptionKey")).toString();
    recipient.publicKeyFingerprint = parsed.object.value(
        QStringLiteral("publicKeyFingerprint")).toString();
    recipient.verification =
        static_cast<SstvShareRecipientVerification>(verification);
    recipient.trust = static_cast<SstvShareRecipientTrust>(trust);
    return recipient.providerId == providerId
        && recipient.stableRecipientId == recipientId
        && validateShareRecipientRecord(recipient).ok();
}

bool validateManagedRecord(const SstvShareQueueConfig& config,
                           const SstvManagedTransferRecord& record,
                           QString* error)
{
    if (!isCanonicalUuid(record.transferId)
        || !isSafeShareIdentifier(record.providerId)
        || !isSafeShareIdentifier(record.recipientId)
        || !directionStateCompatible(record)
        || !isLowercaseSha256(record.payloadSha256)
        || record.byteSize == 0U || record.byteSize > kMaximumSharedImageBytes
        || record.byteOffset > record.byteSize
        || record.attempts > config.retryPolicy.maximumRetries
        || record.restartRecoveries > 1'000'000U
        || record.lastErrorRedacted.size() > 512
        || redactShareSecrets(record.lastErrorRedacted)
               != record.lastErrorRedacted
        || (!record.providerSessionId.isEmpty()
            && !isSafeShareIdentifier(record.providerSessionId))
        || (!record.incomingId.isEmpty()
            && !isSafeShareIdentifier(record.incomingId))
        || !isLowercaseSha256(record.idempotencyKey)
        || !utcDateTime(record.createdUtc) || !utcDateTime(record.updatedUtc)
        || record.updatedUtc < record.createdUtc
        || (record.nextRetryUtc.isValid()
            && record.nextRetryUtc.offsetFromUtc() != 0)) {
        return fail(error, QStringLiteral("managed transfer scalar validation failed"));
    }
    const auto manifest = parseSstvShareManifestV1(record.canonicalManifestJson);
    if (!manifest.ok()
        || manifest.manifest->toCanonicalJson() != record.canonicalManifestJson
        || manifest.manifest->transferId.toString(QUuid::WithoutBraces)
               != (record.direction == SstvManagedTransferDirection::Upload
                       ? record.transferId
                       : manifest.manifest->transferId.toString(
                             QUuid::WithoutBraces))
        || manifest.manifest->providerId != record.providerId
        || manifest.manifest->recipientId != record.recipientId
        || manifest.manifest->byteSize != record.byteSize
        || manifest.manifest->sha256 != record.payloadSha256
        || SstvShareTransfer::deriveIdempotencyKey(*manifest.manifest)
               != record.idempotencyKey) {
        return fail(error, QStringLiteral("managed transfer manifest binding failed"));
    }
    if (!validStoredRecipientJson(record.recipientJson, record.providerId,
                                  record.recipientId)) {
        return fail(error, QStringLiteral("stored recipient metadata is invalid"));
    }
    if (record.direction == SstvManagedTransferDirection::Upload) {
        if (record.sourcePath.isEmpty() || !record.destinationPath.isEmpty()
            || !record.stagingPath.isEmpty() || !record.incomingId.isEmpty()
            || record.transferPersistenceJson.isEmpty()
            || record.transferPersistenceJson.size()
                > kMaximumPersistenceJsonBytes) {
            return fail(error, QStringLiteral("upload persistence shape is invalid"));
        }
        const auto restored = restoreSstvShareTransfer(
            record.transferPersistenceJson, record.updatedUtc, false);
        const bool remoteRemovalTerminal =
            record.state == SstvManagedTransferState::RemoteDeleted
            || record.state == SstvManagedTransferState::RemoteRevoked;
        if (!restored.ok()
            || restored.transfer->manifest().toCanonicalJson()
                   != record.canonicalManifestJson
            || restored.transfer->snapshot().bytesTransferred
                   != record.byteOffset
            || restored.transfer->snapshot().providerUploadId
                   != record.providerSessionId
            || (record.state
                    != managedStateForCore(restored.transfer->snapshot().state)
                && !(remoteRemovalTerminal
                    && restored.transfer->snapshot().state
                        == SstvShareTransferState::Completed))
            || (remoteRemovalTerminal
                && !isSafeShareIdentifier(
                    restored.transfer->snapshot().remoteObjectId))) {
            return fail(error, QStringLiteral("upload state projection is inconsistent"));
        }
        if (!storedUploadPathIsBounded(config, record.sourcePath)) {
            return fail(error, QStringLiteral("persisted upload source path is unsafe"));
        }
    } else {
        if (record.incomingId.isEmpty()
            || !record.transferPersistenceJson.isEmpty()
            || !record.sourcePath.isEmpty()) {
            return fail(error, QStringLiteral("download persistence shape is invalid"));
        }
        if (!record.destinationPath.isEmpty()) {
            const QFileInfo rootInfo(config.downloadRoot);
            const QFileInfo destinationInfo(record.destinationPath);
            if (destinationInfo.isSymLink()) {
                return fail(error, QStringLiteral("persisted download destination is linked"));
            }
            if (!pathWithin(rootInfo.canonicalFilePath(),
                            record.destinationPath,
                            destinationInfo.exists())) {
                return fail(error, QStringLiteral("persisted download destination is unsafe"));
            }
        }
        if (!record.stagingPath.isEmpty()) {
            const QString partialRoot = QDir(config.downloadRoot).absoluteFilePath(
                QStringLiteral(".partial"));
            const QFileInfo stagingInfo(record.stagingPath);
            if (stagingInfo.isSymLink()
                || !pathWithin(QFileInfo(partialRoot).canonicalFilePath(),
                               record.stagingPath, stagingInfo.exists())) {
                return fail(error, QStringLiteral("persisted staging path is unsafe"));
            }
        }
    }
    return true;
}

bool validateInboxItem(const SstvPersistentInboxItem& item, QString* error)
{
    SstvShareIncomingItem incoming;
    incoming.opaqueId = item.incomingId;
    incoming.providerId = item.providerId;
    incoming.senderId = item.senderId;
    incoming.manifestSha256 = item.manifestSha256;
    incoming.canonicalManifestJson = item.canonicalManifestJson;
    incoming.byteSize = item.byteSize;
    incoming.receivedUtc = item.receivedUtc;
    incoming.expiresUtc = item.expiresUtc;
    if (!validateShareIncomingItem(incoming).ok()
        || (!item.transferId.isEmpty() && !isCanonicalUuid(item.transferId))
        || !utcDateTime(item.updatedUtc)
        || item.updatedUtc < item.receivedUtc
        || ((item.disposition == SstvInboxDisposition::DownloadQueued
             || item.disposition == SstvInboxDisposition::AwaitingAcceptance
             || item.disposition == SstvInboxDisposition::Accepted
             || item.disposition == SstvInboxDisposition::Acknowledged
             || item.disposition == SstvInboxDisposition::Cancelled
             || item.disposition == SstvInboxDisposition::Rejected)
            && item.transferId.isEmpty())) {
        return fail(error, QStringLiteral("persistent inbox item is invalid"));
    }
    return true;
}

QString transferColumns()
{
    return QStringLiteral(
        "transfer_id,direction,state,provider_id,recipient_id,manifest_json,"
        "transfer_json,recipient_json,source_path,destination_path,staging_path,"
        "payload_sha256,byte_size,byte_offset,attempts,next_retry_ms,"
        "last_failure,last_error_redacted,provider_session_id,incoming_id,"
        "idempotency_key,cancel_requested,cancel_dispatched,restart_recoveries,"
        "revision,created_ms,updated_ms");
}

QString inboxColumns()
{
    return QStringLiteral(
        "provider_id,incoming_id,sender_id,manifest_sha256,manifest_json,"
        "byte_size,received_ms,expires_ms,disposition,transfer_id,updated_ms");
}

QString inboxUpsertStatement()
{
    return QStringLiteral(
        "INSERT INTO sstv_share_inbox(%1) VALUES("
        ":provider_id,:incoming_id,:sender_id,:manifest_sha256,:manifest_json,"
        ":byte_size,:received_ms,:expires_ms,:disposition,:transfer_id,"
        ":updated_ms) ON CONFLICT(provider_id,incoming_id) DO UPDATE SET "
        "disposition=excluded.disposition,transfer_id=excluded.transfer_id,"
        "updated_ms=excluded.updated_ms WHERE "
        "sstv_share_inbox.sender_id=excluded.sender_id AND "
        "sstv_share_inbox.manifest_sha256=excluded.manifest_sha256 AND "
        "sstv_share_inbox.manifest_json=excluded.manifest_json AND "
        "sstv_share_inbox.byte_size=excluded.byte_size AND "
        "sstv_share_inbox.received_ms=excluded.received_ms AND "
        "sstv_share_inbox.expires_ms=excluded.expires_ms")
        .arg(inboxColumns());
}

QString terminalManagedStatesSql()
{
    return QStringLiteral(
        "'Completed','RemoteDeleted','RemoteRevoked','Acknowledged',"
        "'Cancelled','Rejected','Expired','Failed'");
}

QString closedInboxDispositionsSql()
{
    return QStringLiteral(
        "'Acknowledged','Cancelled','Rejected','Expired','BlockedLocally',"
        "'BlockedByProvider','ProviderDeleted'");
}

struct ReclaimedRowCounts final
{
    quint64 transfers {0U};
    quint64 inbox {0U};

    quint64 total() const noexcept
    {
        return transfers > std::numeric_limits<quint64>::max() - inbox
            ? std::numeric_limits<quint64>::max() : transfers + inbox;
    }
};

bool requiredReclaimCount(QSqlDatabase& database,
                          const QString& table,
                          qsizetype maximumRows,
                          qint64* required,
                          QString* error)
{
    if (!required || maximumRows <= 0
        || maximumRows > static_cast<qsizetype>(100'000)
        || (table != QStringLiteral("sstv_share_transfers")
            && table != QStringLiteral("sstv_share_inbox"))) {
        return fail(error, QStringLiteral("invalid persistent queue bound"));
    }
    QSqlQuery count(database);
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table))
        || !count.next()) {
        return fail(error, QStringLiteral("could not inspect persistent queue bound"));
    }
    bool countOk = false;
    const qint64 currentRows = count.value(0).toLongLong(&countOk);
    const qint64 maximum = static_cast<qint64>(maximumRows);
    if (!countOk || currentRows < 0) {
        return fail(error, QStringLiteral("persistent queue count is invalid"));
    }
    *required = currentRows < maximum ? 0 : (currentRows - maximum) + 1;
    return true;
}

bool absentManagedPath(const QString& path)
{
    if (path.isEmpty()) {
        return true;
    }
    const QFileInfo info(path);
    return !info.exists() && !info.isSymLink();
}

bool reclaimTerminalTransfersForInsert(QSqlDatabase& database,
                                       qsizetype maximumRows,
                                       ReclaimedRowCounts* reclaimed,
                                       QString* error)
{
    if (!reclaimed) {
        return fail(error, QStringLiteral("missing transfer reclamation result"));
    }
    qint64 required = 0;
    if (!requiredReclaimCount(database,
                              QStringLiteral("sstv_share_transfers"),
                              maximumRows, &required, error)) {
        return false;
    }
    if (required == 0) {
        return true;
    }

    struct Candidate final
    {
        QString transferId;
        SstvManagedTransferDirection direction {
            SstvManagedTransferDirection::Upload};
        QString providerId;
        QString incomingId;
        QString destinationPath;
        QString stagingPath;
    };
    QVector<Candidate> candidates;
    candidates.reserve(static_cast<qsizetype>(std::min<qint64>(
        100'000, std::max<qint64>(required, 1))));
    QSqlQuery select(database);
    select.prepare(QStringLiteral(
        "SELECT transfer_id,direction,provider_id,incoming_id,"
        "destination_path,staging_path FROM sstv_share_transfers "
        "WHERE state IN(%1) "
        "ORDER BY updated_ms ASC,created_ms ASC,transfer_id ASC LIMIT 100000")
                       .arg(terminalManagedStatesSql()));
    if (!select.exec()) {
        return fail(error, QStringLiteral("could not select terminal transfers"));
    }
    while (select.next()) {
        Candidate candidate;
        candidate.transferId = select.value(0).toString();
        const int direction = select.value(1).toInt();
        if (direction != static_cast<int>(SstvManagedTransferDirection::Upload)
            && direction
                != static_cast<int>(SstvManagedTransferDirection::Download)) {
            return fail(error, QStringLiteral(
                "terminal transfer has invalid persisted direction"));
        }
        candidate.direction =
            static_cast<SstvManagedTransferDirection>(direction);
        candidate.providerId = select.value(2).toString();
        candidate.incomingId = select.value(3).toString();
        candidate.destinationPath = select.value(4).toString();
        candidate.stagingPath = select.value(5).toString();
        candidates.push_back(std::move(candidate));
    }
    select.finish();

    qint64 removedTransfers = 0;
    for (const Candidate& candidate : std::as_const(candidates)) {
        if (candidate.direction == SstvManagedTransferDirection::Download
            && (!absentManagedPath(candidate.destinationPath)
                || !absentManagedPath(candidate.stagingPath))) {
            continue;
        }
        if (candidate.direction == SstvManagedTransferDirection::Download) {
            QSqlQuery deleteInbox(database);
            deleteInbox.prepare(QStringLiteral(
                "DELETE FROM sstv_share_inbox WHERE provider_id=:provider "
                "AND incoming_id=:incoming AND transfer_id=:transfer"));
            deleteInbox.bindValue(QStringLiteral(":provider"),
                                  candidate.providerId);
            deleteInbox.bindValue(QStringLiteral(":incoming"),
                                  candidate.incomingId);
            deleteInbox.bindValue(QStringLiteral(":transfer"),
                                  candidate.transferId);
            if (!deleteInbox.exec() || deleteInbox.numRowsAffected() < 0
                || deleteInbox.numRowsAffected() > 1) {
                return fail(error, QStringLiteral(
                    "could not reclaim paired terminal inbox row"));
            }
            reclaimed->inbox += static_cast<quint64>(
                deleteInbox.numRowsAffected());
        }

        QSqlQuery remove(database);
        remove.prepare(QStringLiteral(
            "DELETE FROM sstv_share_transfers WHERE transfer_id=:id "
            "AND state IN(%1)").arg(terminalManagedStatesSql()));
        remove.bindValue(QStringLiteral(":id"), candidate.transferId);
        if (!remove.exec() || remove.numRowsAffected() != 1) {
            return fail(error, QStringLiteral(
                "terminal transfer changed during reclamation"));
        }
        ++reclaimed->transfers;
        ++removedTransfers;
        if (removedTransfers == required) {
            return true;
        }
    }
    return fail(error, QStringLiteral(
        "share transfer queue has no safely reclaimable terminal rows"));
}

bool reclaimClosedInboxForInsert(QSqlDatabase& database,
                                 qsizetype maximumRows,
                                 ReclaimedRowCounts* reclaimed,
                                 QString* error)
{
    if (!reclaimed) {
        return fail(error, QStringLiteral("missing inbox reclamation result"));
    }
    qint64 required = 0;
    if (!requiredReclaimCount(database, QStringLiteral("sstv_share_inbox"),
                              maximumRows, &required, error)) {
        return false;
    }
    if (required == 0) {
        return true;
    }

    struct Candidate final
    {
        QString providerId;
        QString incomingId;
        QString transferId;
        SstvManagedTransferDirection direction {
            SstvManagedTransferDirection::Upload};
        QString destinationPath;
        QString stagingPath;
        bool hasTransfer {false};
    };
    QSqlQuery select(database);
    select.prepare(QStringLiteral(
        "SELECT inbox.provider_id,inbox.incoming_id,inbox.transfer_id,"
        "transfer.direction,transfer.destination_path,transfer.staging_path "
        "FROM sstv_share_inbox AS inbox LEFT JOIN sstv_share_transfers "
        "AS transfer ON transfer.transfer_id=inbox.transfer_id "
        "WHERE inbox.disposition IN(%1) AND (transfer.transfer_id IS NULL "
        "OR transfer.state IN(%2)) "
        "ORDER BY inbox.updated_ms ASC,inbox.received_ms ASC,"
        "inbox.provider_id ASC,inbox.incoming_id ASC LIMIT 100000")
                       .arg(closedInboxDispositionsSql(),
                            terminalManagedStatesSql()));
    if (!select.exec()) {
        return fail(error, QStringLiteral("could not select closed inbox rows"));
    }
    QVector<Candidate> candidates;
    while (select.next()) {
        Candidate candidate;
        candidate.providerId = select.value(0).toString();
        candidate.incomingId = select.value(1).toString();
        candidate.transferId = select.value(2).toString();
        candidate.hasTransfer = !select.value(3).isNull();
        if (candidate.hasTransfer) {
            const int direction = select.value(3).toInt();
            if (direction
                    != static_cast<int>(SstvManagedTransferDirection::Upload)
                && direction != static_cast<int>(
                    SstvManagedTransferDirection::Download)) {
                return fail(error, QStringLiteral(
                    "closed inbox transfer has invalid persisted direction"));
            }
            candidate.direction =
                static_cast<SstvManagedTransferDirection>(direction);
            candidate.destinationPath = select.value(4).toString();
            candidate.stagingPath = select.value(5).toString();
        }
        candidates.push_back(std::move(candidate));
    }
    select.finish();

    qint64 removed = 0;
    for (const Candidate& candidate : std::as_const(candidates)) {
        if (candidate.hasTransfer
            && candidate.direction == SstvManagedTransferDirection::Download
            && (!absentManagedPath(candidate.destinationPath)
                || !absentManagedPath(candidate.stagingPath))) {
            continue;
        }
        if (candidate.hasTransfer) {
            QSqlQuery removeTransfer(database);
            removeTransfer.prepare(QStringLiteral(
                "DELETE FROM sstv_share_transfers WHERE transfer_id=:id "
                "AND state IN(%1)").arg(terminalManagedStatesSql()));
            removeTransfer.bindValue(QStringLiteral(":id"),
                                     candidate.transferId);
            if (!removeTransfer.exec()
                || removeTransfer.numRowsAffected() < 0
                || removeTransfer.numRowsAffected() > 1) {
                return fail(error, QStringLiteral(
                    "could not reclaim terminal transfer paired with inbox"));
            }
            reclaimed->transfers += static_cast<quint64>(
                removeTransfer.numRowsAffected());
        }
        QSqlQuery remove(database);
        remove.prepare(QStringLiteral(
            "DELETE FROM sstv_share_inbox "
            "WHERE provider_id=:provider AND incoming_id=:incoming "
            "AND disposition IN(%1) AND NOT EXISTS("
            "SELECT 1 FROM sstv_share_transfers AS transfer "
            "WHERE transfer.transfer_id=sstv_share_inbox.transfer_id)")
                           .arg(closedInboxDispositionsSql()));
        remove.bindValue(QStringLiteral(":provider"), candidate.providerId);
        remove.bindValue(QStringLiteral(":incoming"), candidate.incomingId);
        if (!remove.exec() || remove.numRowsAffected() != 1) {
            return fail(error, QStringLiteral(
                "closed inbox row changed during reclamation"));
        }
        ++reclaimed->inbox;
        ++removed;
        if (removed == required) {
            return true;
        }
    }
    return fail(error, QStringLiteral(
        "persistent inbox has no safely reclaimable closed rows"));
}

QString nonNullSqlText(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QByteArray nonNullSqlBlob(const QByteArray& value)
{
    return value.isNull() ? QByteArray("") : value;
}

void bindTransfer(QSqlQuery& query, const SstvManagedTransferRecord& record)
{
    query.bindValue(QStringLiteral(":transfer_id"), record.transferId);
    query.bindValue(QStringLiteral(":direction"),
                    static_cast<int>(record.direction));
    query.bindValue(QStringLiteral(":state"),
                    sstvManagedTransferStateName(record.state));
    query.bindValue(QStringLiteral(":provider_id"), record.providerId);
    query.bindValue(QStringLiteral(":recipient_id"), record.recipientId);
    query.bindValue(QStringLiteral(":manifest_json"), record.canonicalManifestJson);
    query.bindValue(QStringLiteral(":transfer_json"),
                    nonNullSqlBlob(record.transferPersistenceJson));
    query.bindValue(QStringLiteral(":recipient_json"),
                    nonNullSqlBlob(record.recipientJson));
    query.bindValue(QStringLiteral(":source_path"),
                    nonNullSqlText(record.sourcePath));
    query.bindValue(QStringLiteral(":destination_path"),
                    nonNullSqlText(record.destinationPath));
    query.bindValue(QStringLiteral(":staging_path"),
                    nonNullSqlText(record.stagingPath));
    query.bindValue(QStringLiteral(":payload_sha256"), record.payloadSha256);
    query.bindValue(QStringLiteral(":byte_size"),
                    static_cast<qulonglong>(record.byteSize));
    query.bindValue(QStringLiteral(":byte_offset"),
                    static_cast<qulonglong>(record.byteOffset));
    query.bindValue(QStringLiteral(":attempts"), record.attempts);
    query.bindValue(QStringLiteral(":next_retry_ms"), toMs(record.nextRetryUtc));
    query.bindValue(QStringLiteral(":last_failure"),
                    sstvShareProviderFailureName(record.lastFailure));
    query.bindValue(QStringLiteral(":last_error_redacted"),
                    nonNullSqlText(record.lastErrorRedacted));
    query.bindValue(QStringLiteral(":provider_session_id"),
                    nonNullSqlText(record.providerSessionId));
    query.bindValue(QStringLiteral(":incoming_id"),
                    nonNullSqlText(record.incomingId));
    query.bindValue(QStringLiteral(":idempotency_key"), record.idempotencyKey);
    query.bindValue(QStringLiteral(":cancel_requested"),
                    record.cancelRequested ? 1 : 0);
    query.bindValue(QStringLiteral(":cancel_dispatched"),
                    record.cancelDispatched ? 1 : 0);
    query.bindValue(QStringLiteral(":restart_recoveries"),
                    record.restartRecoveries);
    query.bindValue(QStringLiteral(":revision"),
                    static_cast<qulonglong>(record.revision));
    query.bindValue(QStringLiteral(":created_ms"), toMs(record.createdUtc));
    query.bindValue(QStringLiteral(":updated_ms"), toMs(record.updatedUtc));
}

std::optional<SstvManagedTransferRecord> readTransfer(
    const QSqlQuery& query,
    const SstvShareQueueConfig& config,
    QString* error)
{
    SstvManagedTransferRecord record;
    int index = 0;
    record.transferId = query.value(index++).toString();
    const int direction = query.value(index++).toInt();
    if (direction != static_cast<int>(SstvManagedTransferDirection::Upload)
        && direction != static_cast<int>(
            SstvManagedTransferDirection::Download)) {
        fail(error, QStringLiteral("database transfer has invalid direction"));
        return std::nullopt;
    }
    record.direction = static_cast<SstvManagedTransferDirection>(direction);
    if (!managedStateFromName(query.value(index++).toString(), &record.state)) {
        fail(error, QStringLiteral("database transfer has invalid state"));
        return std::nullopt;
    }
    record.providerId = query.value(index++).toString();
    record.recipientId = query.value(index++).toString();
    record.canonicalManifestJson = query.value(index++).toByteArray();
    record.transferPersistenceJson = query.value(index++).toByteArray();
    record.recipientJson = query.value(index++).toByteArray();
    record.sourcePath = query.value(index++).toString();
    record.destinationPath = query.value(index++).toString();
    record.stagingPath = query.value(index++).toString();
    record.payloadSha256 = query.value(index++).toString();
    record.byteSize = query.value(index++).toULongLong();
    record.byteOffset = query.value(index++).toULongLong();
    record.attempts = query.value(index++).toUInt();
    record.nextRetryUtc = fromMs(query.value(index++).toLongLong());
    if (!providerFailureFromName(query.value(index++).toString(),
                                 &record.lastFailure)) {
        fail(error, QStringLiteral("database transfer has invalid failure"));
        return std::nullopt;
    }
    record.lastErrorRedacted = query.value(index++).toString();
    record.providerSessionId = query.value(index++).toString();
    record.incomingId = query.value(index++).toString();
    record.idempotencyKey = query.value(index++).toString();
    record.cancelRequested = query.value(index++).toInt() != 0;
    record.cancelDispatched = query.value(index++).toInt() != 0;
    record.restartRecoveries = query.value(index++).toUInt();
    record.revision = query.value(index++).toULongLong();
    record.createdUtc = fromMs(query.value(index++).toLongLong());
    record.updatedUtc = fromMs(query.value(index++).toLongLong());
    if (!validateManagedRecord(config, record, error)) {
        return std::nullopt;
    }
    return record;
}

void bindInbox(QSqlQuery& query, const SstvPersistentInboxItem& item)
{
    query.bindValue(QStringLiteral(":provider_id"), item.providerId);
    query.bindValue(QStringLiteral(":incoming_id"), item.incomingId);
    query.bindValue(QStringLiteral(":sender_id"), item.senderId);
    query.bindValue(QStringLiteral(":manifest_sha256"), item.manifestSha256);
    query.bindValue(QStringLiteral(":manifest_json"),
                    item.canonicalManifestJson);
    query.bindValue(QStringLiteral(":byte_size"),
                    static_cast<qulonglong>(item.byteSize));
    query.bindValue(QStringLiteral(":received_ms"), toMs(item.receivedUtc));
    query.bindValue(QStringLiteral(":expires_ms"), toMs(item.expiresUtc));
    query.bindValue(QStringLiteral(":disposition"),
                    sstvInboxDispositionName(item.disposition));
    query.bindValue(QStringLiteral(":transfer_id"),
                    nonNullSqlText(item.transferId));
    query.bindValue(QStringLiteral(":updated_ms"), toMs(item.updatedUtc));
}

std::optional<SstvPersistentInboxItem> readInbox(const QSqlQuery& query,
                                                 QString* error)
{
    SstvPersistentInboxItem item;
    int index = 0;
    item.providerId = query.value(index++).toString();
    item.incomingId = query.value(index++).toString();
    item.senderId = query.value(index++).toString();
    item.manifestSha256 = query.value(index++).toString();
    item.canonicalManifestJson = query.value(index++).toByteArray();
    item.byteSize = query.value(index++).toULongLong();
    item.receivedUtc = fromMs(query.value(index++).toLongLong());
    item.expiresUtc = fromMs(query.value(index++).toLongLong());
    if (!inboxDispositionFromName(query.value(index++).toString(),
                                  &item.disposition)) {
        fail(error, QStringLiteral("database inbox item has invalid disposition"));
        return std::nullopt;
    }
    item.transferId = query.value(index++).toString();
    item.updatedUtc = fromMs(query.value(index++).toLongLong());
    if (!validateInboxItem(item, error)) {
        return std::nullopt;
    }
    return item;
}

} // namespace

QString sstvManagedTransferStateName(SstvManagedTransferState state)
{
    switch (state) {
    case SstvManagedTransferState::Queued: return QStringLiteral("Queued");
    case SstvManagedTransferState::Preparing: return QStringLiteral("Preparing");
    case SstvManagedTransferState::Uploading: return QStringLiteral("Uploading");
    case SstvManagedTransferState::WaitingForAcknowledgement:
        return QStringLiteral("WaitingForAcknowledgement");
    case SstvManagedTransferState::DownloadQueued:
        return QStringLiteral("DownloadQueued");
    case SstvManagedTransferState::Downloading:
        return QStringLiteral("Downloading");
    case SstvManagedTransferState::AwaitingAcceptance:
        return QStringLiteral("AwaitingAcceptance");
    case SstvManagedTransferState::Accepted: return QStringLiteral("Accepted");
    case SstvManagedTransferState::Acknowledging:
        return QStringLiteral("Acknowledging");
    case SstvManagedTransferState::Rejecting: return QStringLiteral("Rejecting");
    case SstvManagedTransferState::RetryScheduled:
        return QStringLiteral("RetryScheduled");
    case SstvManagedTransferState::Paused: return QStringLiteral("Paused");
    case SstvManagedTransferState::Completed: return QStringLiteral("Completed");
    case SstvManagedTransferState::RemoteDeleted:
        return QStringLiteral("RemoteDeleted");
    case SstvManagedTransferState::RemoteRevoked:
        return QStringLiteral("RemoteRevoked");
    case SstvManagedTransferState::Acknowledged:
        return QStringLiteral("Acknowledged");
    case SstvManagedTransferState::Cancelled: return QStringLiteral("Cancelled");
    case SstvManagedTransferState::Rejected: return QStringLiteral("Rejected");
    case SstvManagedTransferState::Expired: return QStringLiteral("Expired");
    case SstvManagedTransferState::Failed: return QStringLiteral("Failed");
    }
    return {};
}

bool isTerminalManagedTransferState(SstvManagedTransferState state) noexcept
{
    return state == SstvManagedTransferState::Completed
        || state == SstvManagedTransferState::RemoteDeleted
        || state == SstvManagedTransferState::RemoteRevoked
        || state == SstvManagedTransferState::Acknowledged
        || state == SstvManagedTransferState::Cancelled
        || state == SstvManagedTransferState::Rejected
        || state == SstvManagedTransferState::Expired
        || state == SstvManagedTransferState::Failed;
}

QString sstvRemoteCopyActionName(SstvRemoteCopyAction action)
{
    switch (action) {
    case SstvRemoteCopyAction::Unavailable:
        return QStringLiteral("unavailable");
    case SstvRemoteCopyAction::Delete:
        return QStringLiteral("delete");
    case SstvRemoteCopyAction::Revoke:
        return QStringLiteral("revoke");
    }
    return QStringLiteral("unavailable");
}

QString sstvInboxDispositionName(SstvInboxDisposition disposition)
{
    switch (disposition) {
    case SstvInboxDisposition::New: return QStringLiteral("New");
    case SstvInboxDisposition::DownloadQueued:
        return QStringLiteral("DownloadQueued");
    case SstvInboxDisposition::AwaitingAcceptance:
        return QStringLiteral("AwaitingAcceptance");
    case SstvInboxDisposition::Accepted: return QStringLiteral("Accepted");
    case SstvInboxDisposition::Acknowledged:
        return QStringLiteral("Acknowledged");
    case SstvInboxDisposition::Cancelled:
        return QStringLiteral("Cancelled");
    case SstvInboxDisposition::Rejected: return QStringLiteral("Rejected");
    case SstvInboxDisposition::Expired: return QStringLiteral("Expired");
    case SstvInboxDisposition::BlockedLocally:
        return QStringLiteral("BlockedLocally");
    case SstvInboxDisposition::BlockedByProvider:
        return QStringLiteral("BlockedByProvider");
    case SstvInboxDisposition::ProviderDeleted:
        return QStringLiteral("ProviderDeleted");
    }
    return {};
}

QString sstvSenderBlockScopeName(SstvSenderBlockScope scope)
{
    switch (scope) {
    case SstvSenderBlockScope::LocalOnly:
        return QStringLiteral("local");
    case SstvSenderBlockScope::Provider:
        return QStringLiteral("provider");
    }
    return {};
}

SstvShareValidationError SstvShareQueueLimits::validate() const
{
    if (maximumRecords <= 0 || maximumRecords > 100'000
        || maximumInboxItems <= 0 || maximumInboxItems > 100'000
        || maximumBlockedSenders <= 0 || maximumBlockedSenders > 100'000
        || maximumQueryItems <= 0 || maximumQueryItems > 1'000
        || maximumConcurrentTransfers <= 0 || maximumConcurrentTransfers > 16
        || maximumConcurrentPerProvider <= 0
        || maximumConcurrentPerProvider > maximumConcurrentTransfers
        || uploadChunkBytes == 0U || uploadChunkBytes > kMaximumSharedImageBytes
        || downloadChunkBytes == 0U
        || downloadChunkBytes > kMaximumSharedImageBytes) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidByteSize,
            QStringLiteral("shareQueueLimits"));
    }
    return {};
}

struct SstvShareQueueStore::Impl final
{
    Impl(SstvShareQueueConfig suppliedConfig,
         std::shared_ptr<SstvShareFaultInjector> suppliedFault)
        : config(std::move(suppliedConfig))
        , fault(std::move(suppliedFault))
        , connectionName(QStringLiteral("sstv_share_queue_%1")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        config.databasePath = absoluteCleanPath(config.databasePath);
        config.downloadRoot = absoluteCleanPath(config.downloadRoot);
        for (QString& root : config.allowedUploadRoots) {
            root = absoluteCleanPath(root);
        }
    }

    bool ownerThread(QString* error) const
    {
        return thread == nullptr || QThread::currentThread() == thread
            || fail(error, QStringLiteral("share queue store used from wrong thread"));
    }

    SstvShareQueueConfig config;
    std::shared_ptr<SstvShareFaultInjector> fault;
    QString connectionName;
    QSqlDatabase database;
    QThread* thread {nullptr};
    int version {0};
    quint64 reclaimedRows {0U};
    bool opened {false};

    void addReclaimedRows(quint64 rows) noexcept
    {
        reclaimedRows = rows >= kMaximumDiagnosticsCounter - reclaimedRows
            ? kMaximumDiagnosticsCounter : reclaimedRows + rows;
    }
};

SstvShareQueueStore::SstvShareQueueStore(
    SstvShareQueueConfig config,
    std::shared_ptr<SstvShareFaultInjector> faultInjector)
    : m_impl(std::make_unique<Impl>(std::move(config),
                                    std::move(faultInjector)))
{
}

SstvShareQueueStore::~SstvShareQueueStore()
{
    close();
}

bool SstvShareQueueStore::open(QString* error)
{
    if (!m_impl->ownerThread(error)) {
        return false;
    }
    if (!m_impl->thread) {
        m_impl->thread = QThread::currentThread();
    }
    if (m_impl->opened) {
        return true;
    }
    if (!m_impl->config.limits.validate().ok()
        || !m_impl->config.retryPolicy.validate().ok()
        || m_impl->config.databasePath.isEmpty()
        || m_impl->config.downloadRoot.isEmpty()
        || m_impl->config.allowedUploadRoots.isEmpty()) {
        return fail(error, QStringLiteral("invalid share queue configuration"));
    }
    if (!ensurePlainDirectory(QFileInfo(m_impl->config.databasePath).absolutePath(),
                              error)
        || !ensurePlainDirectory(m_impl->config.downloadRoot, error)) {
        return false;
    }
    for (const QString& root : m_impl->config.allowedUploadRoots) {
        const QFileInfo info(root);
        if (!info.exists() || !info.isDir() || info.isSymLink()
            || info.canonicalFilePath().isEmpty()) {
            return fail(error, QStringLiteral("allowed upload root is unsafe"));
        }
    }
    const QString partialRoot = QDir(m_impl->config.downloadRoot)
        .absoluteFilePath(QStringLiteral(".partial"));
    const QString validatedRoot = QDir(m_impl->config.downloadRoot)
        .absoluteFilePath(QStringLiteral("validated"));
    if (!ensurePlainDirectory(partialRoot, error)
        || !ensurePlainDirectory(validatedRoot, error)
        || !QFile::setPermissions(partialRoot,
                                  QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)
        || !QFile::setPermissions(validatedRoot,
                                  QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)) {
        return false;
    }
    const QFileInfo databaseInfo(m_impl->config.databasePath);
    if (databaseInfo.isSymLink()
        || (databaseInfo.exists() && !databaseInfo.isFile())) {
        return fail(error, QStringLiteral("share queue database path is unsafe"));
    }
    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        return fail(error, QStringLiteral("QSQLITE driver is unavailable"));
    }
    m_impl->database = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"), m_impl->connectionName);
    m_impl->database.setDatabaseName(m_impl->config.databasePath);
    if (!m_impl->database.open()) {
        const QString detail = m_impl->database.lastError().text();
        m_impl->database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_impl->connectionName);
        return fail(error, QStringLiteral("could not open share queue SQLite: %1")
                               .arg(detail));
    }
    if (!QFile::setPermissions(m_impl->config.databasePath,
                               QFileDevice::ReadOwner
                                   | QFileDevice::WriteOwner)) {
        close();
        return fail(error, QStringLiteral(
            "could not restrict share queue database permissions"));
    }
    if (!execSql(m_impl->database, QStringLiteral("PRAGMA busy_timeout=5000"), error)
        || !execSql(m_impl->database, QStringLiteral("PRAGMA foreign_keys=ON"), error)
        || !execSql(m_impl->database, QStringLiteral("PRAGMA synchronous=FULL"), error)) {
        close();
        return false;
    }
    QSqlQuery journal(m_impl->database);
    if (!journal.exec(QStringLiteral("PRAGMA journal_mode=WAL"))
        || !journal.next()
        || journal.value(0).toString().compare(
               QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
        close();
        return fail(error, QStringLiteral("could not enable SQLite WAL"));
    }
    journal.finish();
    QSqlQuery versionQuery(m_impl->database);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version"))
        || !versionQuery.next()) {
        close();
        return fail(error, QStringLiteral("could not read share queue schema version"));
    }
    bool versionOk = false;
    const int version = versionQuery.value(0).toInt(&versionOk);
    versionQuery.finish();
    if (!versionOk || version < 0 || version > kShareQueueSchemaVersion) {
        close();
        return fail(error, QStringLiteral("unsupported share queue schema version"));
    }
    if (version == 0) {
        if (!beginTransaction(m_impl->database, error)) {
            close();
            return false;
        }
        const QString createTransfers = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sstv_share_transfers("
            "transfer_id TEXT PRIMARY KEY NOT NULL CHECK(length(transfer_id)=36),"
            "direction INTEGER NOT NULL CHECK(direction IN(1,2)),"
            "state TEXT NOT NULL CHECK(length(state) BETWEEN 1 AND 64),"
            "provider_id TEXT NOT NULL CHECK(length(provider_id) BETWEEN 1 AND 128),"
            "recipient_id TEXT NOT NULL CHECK(length(recipient_id) BETWEEN 1 AND 128),"
            "manifest_json BLOB NOT NULL CHECK(length(manifest_json) BETWEEN 1 AND 65536),"
            "transfer_json BLOB NOT NULL CHECK(length(transfer_json)<=131072),"
            "recipient_json BLOB NOT NULL CHECK(length(recipient_json)<=16384),"
            "source_path TEXT NOT NULL CHECK(length(source_path)<=4096),"
            "destination_path TEXT NOT NULL CHECK(length(destination_path)<=4096),"
            "staging_path TEXT NOT NULL CHECK(length(staging_path)<=4096),"
            "payload_sha256 TEXT NOT NULL CHECK(length(payload_sha256)=64),"
            "byte_size INTEGER NOT NULL CHECK(byte_size BETWEEN 1 AND 67108864),"
            "byte_offset INTEGER NOT NULL CHECK(byte_offset>=0 AND byte_offset<=byte_size),"
            "attempts INTEGER NOT NULL CHECK(attempts BETWEEN 0 AND 20),"
            "next_retry_ms INTEGER NOT NULL DEFAULT 0 CHECK(next_retry_ms>=0),"
            "last_failure TEXT NOT NULL CHECK(length(last_failure) BETWEEN 1 AND 64),"
            "last_error_redacted TEXT NOT NULL CHECK(length(last_error_redacted)<=512),"
            "provider_session_id TEXT NOT NULL CHECK(length(provider_session_id)<=128),"
            "incoming_id TEXT NOT NULL CHECK(length(incoming_id)<=128),"
            "idempotency_key TEXT NOT NULL CHECK(length(idempotency_key)=64),"
            "cancel_requested INTEGER NOT NULL CHECK(cancel_requested IN(0,1)),"
            "cancel_dispatched INTEGER NOT NULL CHECK(cancel_dispatched IN(0,1)),"
            "restart_recoveries INTEGER NOT NULL CHECK(restart_recoveries>=0),"
            "revision INTEGER NOT NULL CHECK(revision>0),"
            "created_ms INTEGER NOT NULL CHECK(created_ms>0),"
            "updated_ms INTEGER NOT NULL CHECK(updated_ms>=created_ms))");
        const QString createInbox = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sstv_share_inbox("
            "provider_id TEXT NOT NULL CHECK(length(provider_id) BETWEEN 1 AND 128),"
            "incoming_id TEXT NOT NULL CHECK(length(incoming_id) BETWEEN 1 AND 128),"
            "sender_id TEXT NOT NULL CHECK(length(sender_id) BETWEEN 1 AND 128),"
            "manifest_sha256 TEXT NOT NULL CHECK(length(manifest_sha256)=64),"
            "manifest_json BLOB NOT NULL CHECK(length(manifest_json) BETWEEN 1 AND 65536),"
            "byte_size INTEGER NOT NULL CHECK(byte_size BETWEEN 1 AND 67108864),"
            "received_ms INTEGER NOT NULL CHECK(received_ms>0),"
            "expires_ms INTEGER NOT NULL CHECK(expires_ms>received_ms),"
            "disposition TEXT NOT NULL CHECK(length(disposition) BETWEEN 1 AND 32),"
            "transfer_id TEXT NOT NULL CHECK(length(transfer_id) IN(0,36)),"
            "updated_ms INTEGER NOT NULL CHECK(updated_ms>=received_ms),"
            "PRIMARY KEY(provider_id,incoming_id))");
        const QString createSenderBlocks = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sstv_share_sender_blocks("
            "provider_id TEXT NOT NULL CHECK(length(provider_id) BETWEEN 1 AND 128),"
            "sender_id TEXT NOT NULL CHECK(length(sender_id) BETWEEN 1 AND 128),"
            "scope TEXT NOT NULL CHECK(scope IN('local','provider')),"
            "created_ms INTEGER NOT NULL CHECK(created_ms>0),"
            "PRIMARY KEY(provider_id,sender_id))");
        if (!execSql(m_impl->database, createTransfers, error)
            || !execSql(m_impl->database, createInbox, error)
            || !execSql(m_impl->database, createSenderBlocks, error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_active "
                               "ON sstv_share_transfers(state,next_retry_ms,updated_ms)"),
                error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_history "
                               "ON sstv_share_transfers(updated_ms DESC,transfer_id DESC)"),
                error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_provider "
                               "ON sstv_share_transfers(provider_id,state)"), error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_inbox_page "
                               "ON sstv_share_inbox(received_ms DESC,incoming_id DESC)"),
                error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_reclaim "
                               "ON sstv_share_transfers("
                               "updated_ms,created_ms,transfer_id)"), error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_inbox_reclaim "
                               "ON sstv_share_inbox("
                               "updated_ms,received_ms,provider_id,incoming_id)"),
                error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_sender_block "
                               "ON sstv_share_sender_blocks(provider_id,sender_id)"),
                error)
            || !execSql(m_impl->database,
                        QStringLiteral("PRAGMA user_version=3"), error)
            || !commitTransaction(m_impl->database, m_impl->fault,
                                  QStringLiteral("schema"), error)) {
            m_impl->database.rollback();
            close();
            return false;
        }
    }
    if (version == 1) {
        if (!beginTransaction(m_impl->database, error)
            || !execSql(m_impl->database,
                QStringLiteral(
                    "CREATE TABLE IF NOT EXISTS sstv_share_sender_blocks("
                    "provider_id TEXT NOT NULL CHECK(length(provider_id) BETWEEN 1 AND 128),"
                    "sender_id TEXT NOT NULL CHECK(length(sender_id) BETWEEN 1 AND 128),"
                    "scope TEXT NOT NULL CHECK(scope IN('local','provider')),"
                    "created_ms INTEGER NOT NULL CHECK(created_ms>0),"
                    "PRIMARY KEY(provider_id,sender_id))"), error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_sender_block "
                               "ON sstv_share_sender_blocks(provider_id,sender_id)"),
                error)
            || !execSql(m_impl->database,
                        QStringLiteral("PRAGMA user_version=2"), error)
            || !commitTransaction(m_impl->database, m_impl->fault,
                                  QStringLiteral("schema-v2"), error)) {
            m_impl->database.rollback();
            close();
            return false;
        }
    }
    if (version == 1 || version == 2) {
        if (!beginTransaction(m_impl->database, error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_reclaim "
                               "ON sstv_share_transfers("
                               "updated_ms,created_ms,transfer_id)"), error)
            || !execSql(m_impl->database,
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sstv_share_inbox_reclaim "
                               "ON sstv_share_inbox("
                               "updated_ms,received_ms,provider_id,incoming_id)"),
                error)
            || !execSql(m_impl->database,
                        QStringLiteral("PRAGMA user_version=3"), error)
            || !commitTransaction(m_impl->database, m_impl->fault,
                                  QStringLiteral("schema-v3"), error)) {
            m_impl->database.rollback();
            close();
            return false;
        }
    }
    QSqlQuery schemaCheck(m_impl->database);
    if (!schemaCheck.exec(QStringLiteral("SELECT %1 FROM sstv_share_transfers LIMIT 0")
                              .arg(transferColumns()))
        || !schemaCheck.exec(QStringLiteral("SELECT %1 FROM sstv_share_inbox LIMIT 0")
                                 .arg(inboxColumns()))
        || !schemaCheck.exec(QStringLiteral(
            "SELECT provider_id,sender_id,scope,created_ms "
            "FROM sstv_share_sender_blocks LIMIT 0"))) {
        close();
        return fail(error, QStringLiteral("share queue schema validation failed"));
    }
    m_impl->version = kShareQueueSchemaVersion;
    m_impl->opened = true;
    return true;
}

void SstvShareQueueStore::close()
{
    if (!m_impl || (!m_impl->opened && !m_impl->database.isValid())) {
        return;
    }
    if (m_impl->thread && QThread::currentThread() != m_impl->thread) {
        return;
    }
    const QString connection = m_impl->connectionName;
    if (m_impl->database.isValid()) {
        m_impl->database.close();
    }
    m_impl->database = QSqlDatabase();
    if (QSqlDatabase::contains(connection)) {
        QSqlDatabase::removeDatabase(connection);
    }
    m_impl->opened = false;
    m_impl->version = 0;
}

bool SstvShareQueueStore::isOpen() const noexcept
{
    return m_impl->opened;
}

int SstvShareQueueStore::schemaVersion() const noexcept
{
    return m_impl->version;
}

const SstvShareQueueConfig& SstvShareQueueStore::config() const noexcept
{
    return m_impl->config;
}

quint64 SstvShareQueueStore::reclaimedRows() const noexcept
{
    return m_impl->reclaimedRows;
}

void SstvShareQueueStore::resetReclaimedRows() noexcept
{
    if (m_impl->ownerThread(nullptr)) {
        m_impl->reclaimedRows = 0U;
    }
}

bool SstvShareQueueStore::insertTransfer(SstvManagedTransferRecord& record,
                                         QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->opened) {
        return fail(error, QStringLiteral("share queue store is not open"));
    }
    record.revision = 1U;
    if (!validateManagedRecord(m_impl->config, record, error)
        || !beginTransaction(m_impl->database, error)) {
        return false;
    }
    ReclaimedRowCounts reclaimed;
    if (!reclaimTerminalTransfersForInsert(
            m_impl->database, m_impl->config.limits.maximumRecords,
            &reclaimed, error)) {
        m_impl->database.rollback();
        return false;
    }
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral(
        "INSERT INTO sstv_share_transfers(%1) VALUES("
        ":transfer_id,:direction,:state,:provider_id,:recipient_id,"
        ":manifest_json,:transfer_json,:recipient_json,:source_path,"
        ":destination_path,:staging_path,:payload_sha256,:byte_size,"
        ":byte_offset,:attempts,:next_retry_ms,:last_failure,"
        ":last_error_redacted,:provider_session_id,:incoming_id,"
        ":idempotency_key,:cancel_requested,:cancel_dispatched,"
        ":restart_recoveries,:revision,:created_ms,:updated_ms)")
                      .arg(transferColumns()));
    bindTransfer(query, record);
    if (!query.exec()) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not insert share transfer: %1")
                               .arg(query.lastError().text()));
    }
    if (!commitTransaction(m_impl->database, m_impl->fault,
                           record.transferId, error)) {
        return false;
    }
    m_impl->addReclaimedRows(reclaimed.total());
    return true;
}

bool SstvShareQueueStore::updateTransfer(SstvManagedTransferRecord& record,
                                         QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || record.revision == 0U
        || !validateManagedRecord(m_impl->config, record, error)
        || !beginTransaction(m_impl->database, error)) {
        return false;
    }
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral(
        "UPDATE sstv_share_transfers SET direction=:direction,state=:state,"
        "provider_id=:provider_id,recipient_id=:recipient_id,"
        "manifest_json=:manifest_json,transfer_json=:transfer_json,"
        "recipient_json=:recipient_json,source_path=:source_path,"
        "destination_path=:destination_path,staging_path=:staging_path,"
        "payload_sha256=:payload_sha256,byte_size=:byte_size,"
        "byte_offset=:byte_offset,attempts=:attempts,"
        "next_retry_ms=:next_retry_ms,last_failure=:last_failure,"
        "last_error_redacted=:last_error_redacted,"
        "provider_session_id=:provider_session_id,incoming_id=:incoming_id,"
        "idempotency_key=:idempotency_key,cancel_requested=:cancel_requested,"
        "cancel_dispatched=:cancel_dispatched,"
        "restart_recoveries=:restart_recoveries,revision=revision+1,"
        "updated_ms=:updated_ms WHERE transfer_id=:transfer_id "
        "AND revision=:revision"));
    bindTransfer(query, record);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("share transfer update conflict"));
    }
    if (!commitTransaction(m_impl->database, m_impl->fault,
                           record.transferId, error)) {
        return false;
    }
    ++record.revision;
    return true;
}

bool SstvShareQueueStore::insertDownloadTransferAndInbox(
    SstvManagedTransferRecord& record,
    const SstvPersistentInboxItem& item,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->opened) {
        return fail(error, QStringLiteral("share queue store is not open"));
    }
    record.revision = 1U;
    if (record.direction != SstvManagedTransferDirection::Download
        || record.providerId != item.providerId
        || record.incomingId != item.incomingId
        || record.transferId != item.transferId
        || !validateManagedRecord(m_impl->config, record, error)
        || !validateInboxItem(item, error)
        || !beginTransaction(m_impl->database, error)) {
        return false;
    }
    ReclaimedRowCounts reclaimed;
    if (!reclaimTerminalTransfersForInsert(
            m_impl->database, m_impl->config.limits.maximumRecords,
            &reclaimed, error)) {
        m_impl->database.rollback();
        return false;
    }
    QSqlQuery bounds(m_impl->database);
    bounds.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM sstv_share_inbox WHERE provider_id=:provider "
        "AND incoming_id=:incoming"));
    bounds.bindValue(QStringLiteral(":provider"), item.providerId);
    bounds.bindValue(QStringLiteral(":incoming"), item.incomingId);
    if (!bounds.exec() || !bounds.next() || bounds.value(0).toInt() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("download inbox identity is missing"));
    }
    bounds.finish();

    QSqlQuery transferQuery(m_impl->database);
    transferQuery.prepare(QStringLiteral(
        "INSERT INTO sstv_share_transfers(%1) VALUES("
        ":transfer_id,:direction,:state,:provider_id,:recipient_id,"
        ":manifest_json,:transfer_json,:recipient_json,:source_path,"
        ":destination_path,:staging_path,:payload_sha256,:byte_size,"
        ":byte_offset,:attempts,:next_retry_ms,:last_failure,"
        ":last_error_redacted,:provider_session_id,:incoming_id,"
        ":idempotency_key,:cancel_requested,:cancel_dispatched,"
        ":restart_recoveries,:revision,:created_ms,:updated_ms)")
                              .arg(transferColumns()));
    bindTransfer(transferQuery, record);
    if (!transferQuery.exec()) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not insert download transfer"));
    }
    QSqlQuery inboxQuery(m_impl->database);
    inboxQuery.prepare(inboxUpsertStatement());
    bindInbox(inboxQuery, item);
    if (!inboxQuery.exec() || inboxQuery.numRowsAffected() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not pair download with inbox"));
    }
    if (!commitTransaction(m_impl->database, m_impl->fault,
                           record.transferId, error)) {
        return false;
    }
    m_impl->addReclaimedRows(reclaimed.total());
    return true;
}

bool SstvShareQueueStore::updateDownloadTransferAndInbox(
    SstvManagedTransferRecord& record,
    const SstvPersistentInboxItem& item,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || record.revision == 0U
        || record.direction != SstvManagedTransferDirection::Download
        || record.providerId != item.providerId
        || record.incomingId != item.incomingId
        || record.transferId != item.transferId
        || !validateManagedRecord(m_impl->config, record, error)
        || !validateInboxItem(item, error)
        || !beginTransaction(m_impl->database, error)) {
        return false;
    }
    QSqlQuery transferQuery(m_impl->database);
    transferQuery.prepare(QStringLiteral(
        "UPDATE sstv_share_transfers SET direction=:direction,state=:state,"
        "provider_id=:provider_id,recipient_id=:recipient_id,"
        "manifest_json=:manifest_json,transfer_json=:transfer_json,"
        "recipient_json=:recipient_json,source_path=:source_path,"
        "destination_path=:destination_path,staging_path=:staging_path,"
        "payload_sha256=:payload_sha256,byte_size=:byte_size,"
        "byte_offset=:byte_offset,attempts=:attempts,"
        "next_retry_ms=:next_retry_ms,last_failure=:last_failure,"
        "last_error_redacted=:last_error_redacted,"
        "provider_session_id=:provider_session_id,incoming_id=:incoming_id,"
        "idempotency_key=:idempotency_key,cancel_requested=:cancel_requested,"
        "cancel_dispatched=:cancel_dispatched,"
        "restart_recoveries=:restart_recoveries,revision=revision+1,"
        "updated_ms=:updated_ms WHERE transfer_id=:transfer_id "
        "AND revision=:revision"));
    bindTransfer(transferQuery, record);
    if (!transferQuery.exec() || transferQuery.numRowsAffected() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("download transfer update conflict"));
    }
    QSqlQuery inboxQuery(m_impl->database);
    inboxQuery.prepare(inboxUpsertStatement());
    bindInbox(inboxQuery, item);
    if (!inboxQuery.exec() || inboxQuery.numRowsAffected() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not atomically update download inbox"));
    }
    if (!commitTransaction(m_impl->database, m_impl->fault,
                           record.transferId, error)) {
        return false;
    }
    ++record.revision;
    return true;
}

std::optional<SstvManagedTransferRecord> SstvShareQueueStore::transfer(
    const QString& transferId,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || !isCanonicalUuid(transferId)) {
        fail(error, QStringLiteral("invalid transfer lookup"));
        return std::nullopt;
    }
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral("SELECT %1 FROM sstv_share_transfers "
                                 "WHERE transfer_id=:id")
                      .arg(transferColumns()));
    query.bindValue(QStringLiteral(":id"), transferId);
    if (!query.exec()) {
        fail(error, QStringLiteral("share transfer lookup failed"));
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return readTransfer(query, m_impl->config, error);
}

QVector<SstvManagedTransferRecord> SstvShareQueueStore::queryTransfers(
    SstvShareTransferView view,
    qsizetype maximumItems,
    QString* error) const
{
    QVector<SstvManagedTransferRecord> records;
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || maximumItems <= 0
        || maximumItems > m_impl->config.limits.maximumRecords) {
        fail(error, QStringLiteral("invalid transfer query bound"));
        return records;
    }
    static const QString terminals = QStringLiteral(
        "'Completed','RemoteDeleted','RemoteRevoked','Acknowledged',"
        "'Cancelled','Rejected','Expired','Failed'");
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral(
        "SELECT %1 FROM sstv_share_transfers WHERE state %2 IN(%3) "
        "ORDER BY updated_ms %4,transfer_id %4 LIMIT :limit")
        .arg(transferColumns(),
             view == SstvShareTransferView::Active
                 ? QStringLiteral("NOT") : QString {},
             terminals,
             view == SstvShareTransferView::Active
                 ? QStringLiteral("ASC") : QStringLiteral("DESC")));
    query.bindValue(QStringLiteral(":limit"), maximumItems);
    if (!query.exec()) {
        fail(error, QStringLiteral("share transfer query failed"));
        return {};
    }
    while (query.next()) {
        auto record = readTransfer(query, m_impl->config, error);
        if (!record) {
            return {};
        }
        records.push_back(std::move(*record));
    }
    return records;
}

bool SstvShareQueueStore::upsertInboxItem(
    const SstvPersistentInboxItem& item,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || !validateInboxItem(item, error)
        || !beginTransaction(m_impl->database, error)) {
        return false;
    }
    QSqlQuery count(m_impl->database);
    count.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM sstv_share_inbox WHERE provider_id=:provider "
        "AND incoming_id=:incoming"));
    count.bindValue(QStringLiteral(":provider"), item.providerId);
    count.bindValue(QStringLiteral(":incoming"), item.incomingId);
    if (!count.exec() || !count.next()) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not inspect inbox bound"));
    }
    const bool exists = count.value(0).toInt() == 1;
    ReclaimedRowCounts reclaimed;
    if (!exists) {
        if (!reclaimClosedInboxForInsert(
                m_impl->database,
                m_impl->config.limits.maximumInboxItems,
                &reclaimed, error)) {
            m_impl->database.rollback();
            return false;
        }
    }
    QSqlQuery query(m_impl->database);
    query.prepare(inboxUpsertStatement());
    bindInbox(query, item);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral(
            "could not persist inbox item or immutable identity changed"));
    }
    if (!commitTransaction(m_impl->database, m_impl->fault,
                           item.transferId.isEmpty()
                               ? item.incomingId : item.transferId,
                           error)) {
        return false;
    }
    m_impl->addReclaimedRows(reclaimed.total());
    return true;
}

std::optional<SstvPersistentInboxItem> SstvShareQueueStore::inboxItem(
    const QString& providerId,
    const QString& incomingId,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || !isSafeShareIdentifier(providerId)
        || !isSafeShareIdentifier(incomingId)) {
        fail(error, QStringLiteral("invalid inbox lookup"));
        return std::nullopt;
    }
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral("SELECT %1 FROM sstv_share_inbox "
                                 "WHERE provider_id=:provider "
                                 "AND incoming_id=:incoming")
                      .arg(inboxColumns()));
    query.bindValue(QStringLiteral(":provider"), providerId);
    query.bindValue(QStringLiteral(":incoming"), incomingId);
    if (!query.exec()) {
        fail(error, QStringLiteral("inbox lookup failed"));
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return readInbox(query, error);
}

QVector<SstvPersistentInboxItem> SstvShareQueueStore::queryInbox(
    qsizetype maximumItems,
    QString* error) const
{
    QVector<SstvPersistentInboxItem> items;
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || maximumItems <= 0
        || maximumItems > m_impl->config.limits.maximumInboxItems) {
        fail(error, QStringLiteral("invalid inbox query bound"));
        return items;
    }
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral("SELECT %1 FROM sstv_share_inbox "
                                 "ORDER BY received_ms DESC,incoming_id DESC "
                                 "LIMIT :limit")
                      .arg(inboxColumns()));
    query.bindValue(QStringLiteral(":limit"), maximumItems);
    if (!query.exec()) {
        fail(error, QStringLiteral("inbox query failed"));
        return {};
    }
    while (query.next()) {
        auto item = readInbox(query, error);
        if (!item) {
            return {};
        }
        items.push_back(std::move(*item));
    }
    return items;
}

bool SstvShareQueueStore::upsertSenderBlock(
    const SstvSenderBlockRecord& block,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || !isSafeShareIdentifier(block.providerId)
        || !isSafeShareIdentifier(block.senderId)
        || !utcDateTime(block.createdUtc)
        || !beginTransaction(m_impl->database, error)) {
        return fail(error, QStringLiteral("invalid sender block"));
    }
    QSqlQuery exists(m_impl->database);
    exists.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM sstv_share_sender_blocks "
        "WHERE provider_id=:provider AND sender_id=:sender"));
    exists.bindValue(QStringLiteral(":provider"), block.providerId);
    exists.bindValue(QStringLiteral(":sender"), block.senderId);
    if (!exists.exec() || !exists.next()) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not inspect sender block"));
    }
    if (exists.value(0).toInt() == 0) {
        QSqlQuery count(m_impl->database);
        if (!count.exec(QStringLiteral(
                "SELECT COUNT(*) FROM sstv_share_sender_blocks"))
            || !count.next()
            || count.value(0).toLongLong()
                >= m_impl->config.limits.maximumBlockedSenders) {
            m_impl->database.rollback();
            return fail(error, QStringLiteral("sender block list reached its bound"));
        }
    }
    QSqlQuery upsert(m_impl->database);
    upsert.prepare(QStringLiteral(
        "INSERT INTO sstv_share_sender_blocks("
        "provider_id,sender_id,scope,created_ms) VALUES("
        ":provider,:sender,:scope,:created) "
        "ON CONFLICT(provider_id,sender_id) DO UPDATE SET "
        "scope=CASE WHEN excluded.scope='provider' THEN 'provider' "
        "ELSE sstv_share_sender_blocks.scope END,"
        "created_ms=MIN(sstv_share_sender_blocks.created_ms,excluded.created_ms)"));
    upsert.bindValue(QStringLiteral(":provider"), block.providerId);
    upsert.bindValue(QStringLiteral(":sender"), block.senderId);
    upsert.bindValue(QStringLiteral(":scope"),
                     sstvSenderBlockScopeName(block.scope));
    upsert.bindValue(QStringLiteral(":created"), toMs(block.createdUtc));
    if (!upsert.exec() || upsert.numRowsAffected() != 1) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not persist sender block"));
    }
    QSqlQuery updateInbox(m_impl->database);
    updateInbox.prepare(QStringLiteral(
        "UPDATE sstv_share_inbox SET disposition=:disposition,"
        "updated_ms=MAX(updated_ms,:updated) "
        "WHERE provider_id=:provider AND sender_id=:sender "
        "AND (disposition='New' OR "
        "(:scope='provider' AND disposition='BlockedLocally'))"));
    updateInbox.bindValue(
        QStringLiteral(":disposition"),
        sstvInboxDispositionName(
            block.scope == SstvSenderBlockScope::Provider
                ? SstvInboxDisposition::BlockedByProvider
                : SstvInboxDisposition::BlockedLocally));
    updateInbox.bindValue(QStringLiteral(":updated"), toMs(block.createdUtc));
    updateInbox.bindValue(QStringLiteral(":scope"),
                          sstvSenderBlockScopeName(block.scope));
    updateInbox.bindValue(QStringLiteral(":provider"), block.providerId);
    updateInbox.bindValue(QStringLiteral(":sender"), block.senderId);
    if (!updateInbox.exec()) {
        m_impl->database.rollback();
        return fail(error, QStringLiteral("could not apply sender block to inbox"));
    }
    return commitTransaction(m_impl->database, m_impl->fault,
                             block.senderId, error);
}

std::optional<SstvSenderBlockRecord> SstvShareQueueStore::senderBlock(
    const QString& providerId,
    const QString& senderId,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->opened
        || !isSafeShareIdentifier(providerId)
        || !isSafeShareIdentifier(senderId)) {
        fail(error, QStringLiteral("invalid sender block lookup"));
        return {};
    }
    QSqlQuery query(m_impl->database);
    query.prepare(QStringLiteral(
        "SELECT scope,created_ms FROM sstv_share_sender_blocks "
        "WHERE provider_id=:provider AND sender_id=:sender"));
    query.bindValue(QStringLiteral(":provider"), providerId);
    query.bindValue(QStringLiteral(":sender"), senderId);
    if (!query.exec()) {
        fail(error, QStringLiteral("sender block lookup failed"));
        return {};
    }
    if (!query.next()) {
        return {};
    }
    SstvSenderBlockRecord block;
    block.providerId = providerId;
    block.senderId = senderId;
    if (!senderBlockScopeFromName(query.value(0).toString(), &block.scope)) {
        fail(error, QStringLiteral("sender block scope is invalid"));
        return {};
    }
    block.createdUtc = fromMs(query.value(1).toLongLong());
    if (!utcDateTime(block.createdUtc)) {
        fail(error, QStringLiteral("sender block timestamp is invalid"));
        return {};
    }
    return block;
}

namespace {

SstvManagedTransferState managedStateForCore(SstvShareTransferState state)
{
    switch (state) {
    case SstvShareTransferState::Draft:
    case SstvShareTransferState::Queued:
        return SstvManagedTransferState::Queued;
    case SstvShareTransferState::Preparing:
    case SstvShareTransferState::Encrypting:
        return SstvManagedTransferState::Preparing;
    case SstvShareTransferState::Uploading:
        return SstvManagedTransferState::Uploading;
    case SstvShareTransferState::WaitingForAcknowledgement:
        return SstvManagedTransferState::WaitingForAcknowledgement;
    case SstvShareTransferState::Completed:
        return SstvManagedTransferState::Completed;
    case SstvShareTransferState::RetryScheduled:
        return SstvManagedTransferState::RetryScheduled;
    case SstvShareTransferState::Paused:
        return SstvManagedTransferState::Paused;
    case SstvShareTransferState::Cancelled:
        return SstvManagedTransferState::Cancelled;
    case SstvShareTransferState::Rejected:
        return SstvManagedTransferState::Rejected;
    case SstvShareTransferState::Expired:
        return SstvManagedTransferState::Expired;
    case SstvShareTransferState::Failed:
        return SstvManagedTransferState::Failed;
    }
    return SstvManagedTransferState::Failed;
}

QByteArray storedRecipientJson(const SstvShareRecipientRecord& recipient)
{
    SstvShareValidationError error;
    const QJsonObject object {
        {QStringLiteral("displayCallsign"), recipient.displayCallsign},
        {QStringLiteral("displayName"), recipient.displayName},
        {QStringLiteral("providerId"), recipient.providerId},
        {QStringLiteral("publicEncryptionKey"),
         recipient.publicEncryptionKey},
        {QStringLiteral("publicKeyFingerprint"),
         recipient.publicKeyFingerprint},
        {QStringLiteral("stableRecipientId"), recipient.stableRecipientId},
        {QStringLiteral("trust"),
         QString::number(static_cast<int>(recipient.trust))},
        {QStringLiteral("verification"),
         QString::number(static_cast<int>(recipient.verification))},
    };
    const QByteArray encoded = canonicalJson(object, &error);
    return error.ok() && encoded.size() <= kMaximumStoredRecipientJsonBytes
        ? encoded : QByteArray {};
}

QDateTime notBefore(const QDateTime& candidate, const QDateTime& floor)
{
    const QDateTime utcCandidate = candidate.toUTC();
    const QDateTime utcFloor = floor.toUTC();
    return utcCandidate < utcFloor ? utcFloor : utcCandidate;
}

template<typename Callback>
void deliverToManager(QPointer<SstvShareQueueManager> manager,
                      Callback callback)
{
    if (!manager) {
        return;
    }
    if (QThread::currentThread() == manager->thread()) {
        callback(*manager);
        return;
    }
    QMetaObject::invokeMethod(
        manager,
        [manager, callback = std::move(callback)]() mutable {
            if (manager) {
                callback(*manager);
            }
        },
        Qt::QueuedConnection);
}

} // namespace

struct SstvShareQueueManager::Impl final
{
    struct ActiveOperation final
    {
        std::shared_ptr<SstvShareProvider> provider;
        SstvShareOperationId operationId {0U};
        QString stage;
    };

    Impl(SstvShareQueueManager* suppliedOwner,
         SstvShareQueueConfig config,
         std::shared_ptr<SstvShareFaultInjector> suppliedFault,
         std::function<QDateTime()> suppliedClock)
        : owner(suppliedOwner)
        , store(std::move(config), suppliedFault)
        , fault(std::move(suppliedFault))
        , clock(std::move(suppliedClock))
    {
        diagnosticsResetUtc = now();
    }

    void addDiagnosticBytes(bool upload, quint64 bytes)
    {
        quint64& counter = upload ? uploadedBytes : downloadedBytes;
        counter = bytes >= kMaximumDiagnosticsCounter - counter
            ? kMaximumDiagnosticsCounter : counter + bytes;
    }

    QDateTime now() const
    {
        const QDateTime value = clock ? clock() : QDateTime::currentDateTimeUtc();
        return value.isValid() ? value.toUTC() : QDateTime::currentDateTimeUtc();
    }

    bool ownerThread(QString* error) const
    {
        return QThread::currentThread() == owner->thread()
            || fail(error, QStringLiteral("share queue manager used from wrong thread"));
    }

    std::shared_ptr<SstvShareProvider> provider(const QString& providerId) const
    {
        const auto found = providers.constFind(providerId);
        return found == providers.constEnd() ? nullptr : *found;
    }

    qsizetype providerActiveCount(const QString& providerId) const
    {
        qsizetype count = 0;
        for (auto it = active.constBegin(); it != active.constEnd(); ++it) {
            if (it->provider && it->provider->providerId() == providerId) {
                ++count;
            }
        }
        return count;
    }

    bool hasCapacityFor(const QString& providerId) const
    {
        const auto& limits = store.config().limits;
        return active.size() < limits.maximumConcurrentTransfers
            && providerActiveCount(providerId)
                < limits.maximumConcurrentPerProvider;
    }

    bool uploadNetworkPolicyAllows(
        const SstvManagedTransferRecord& record) const
    {
        const auto manifest = parseSstvShareManifestV1(
            record.canonicalManifestJson);
        if (!manifest.ok()) {
            return false;
        }
        if (manifest.manifest->privacy.meteredNetworkAllowed) {
            return true;
        }
        if (!store.config().meteredNetworkProbe) {
            return false;
        }
        const std::optional<bool> metered =
            store.config().meteredNetworkProbe();
        return metered.has_value() && !*metered;
    }

    bool claim(const QString& key, const QString& stage)
    {
        const auto found = active.find(key);
        if (found == active.end() || found->stage != stage) {
            return false;
        }
        active.erase(found);
        return true;
    }

    void rememberOperation(const QString& key,
                           const QString& stage,
                           const std::shared_ptr<SstvShareProvider>& shareProvider,
                           SstvShareOperationId operationId)
    {
        if (operationId != 0U) {
            active.insert(key, {shareProvider, operationId, stage});
        }
    }

    bool updateRecord(SstvManagedTransferRecord& record, QString* error = nullptr)
    {
        record.updatedUtc = notBefore(now(), record.createdUtc);
        return store.updateTransfer(record, error);
    }

    bool projectUpload(SstvManagedTransferRecord& record,
                       const SstvShareTransfer& transfer,
                       QString* error = nullptr)
    {
        SstvShareValidationError persistenceError;
        record.transferPersistenceJson =
            transfer.toPersistenceJson(&persistenceError);
        if (!persistenceError.ok() || record.transferPersistenceJson.isEmpty()) {
            return fail(error, QStringLiteral("could not serialize upload state"));
        }
        const auto& snapshot = transfer.snapshot();
        record.state = managedStateForCore(snapshot.state);
        record.byteOffset = snapshot.bytesTransferred;
        record.attempts = snapshot.retryCount;
        record.nextRetryUtc = snapshot.retryAtUtc;
        record.lastFailure = snapshot.lastFailure;
        record.providerSessionId = snapshot.providerUploadId;
        record.idempotencyKey = snapshot.idempotencyKey;
        record.restartRecoveries = snapshot.restartRecoveries;
        return true;
    }

    bool saveUpload(SstvManagedTransferRecord& record,
                    const SstvShareTransfer& transfer,
                    QString* error = nullptr)
    {
        return projectUpload(record, transfer, error)
            && updateRecord(record, error);
    }

    void completeAction(const QString& transferId,
                        const SstvShareProviderResult& result)
    {
        auto found = completions.find(transferId);
        if (found == completions.end()) {
            return;
        }
        SstvShareManagerCompletion completion = std::move(*found);
        completions.erase(found);
        if (completion) {
            completion(result);
        }
    }

    SstvRemoteCopyAction remoteCopyActionFor(
        const SstvManagedTransferRecord& record) const
    {
        if (record.direction != SstvManagedTransferDirection::Upload
            || record.state != SstvManagedTransferState::Completed
            || active.contains(record.transferId)) {
            return SstvRemoteCopyAction::Unavailable;
        }
        const auto shareProvider = provider(record.providerId);
        if (!shareProvider || !hasCapacityFor(record.providerId)) {
            return SstvRemoteCopyAction::Unavailable;
        }
        const auto authentication = shareProvider->authenticationStatus();
        if (authentication != SstvShareAuthenticationStatus::Authenticated
            && authentication != SstvShareAuthenticationStatus::NotRequired) {
            return SstvRemoteCopyAction::Unavailable;
        }
        const auto restored = restoreSstvShareTransfer(
            record.transferPersistenceJson, now(), false);
        if (!restored.ok()
            || restored.transfer->snapshot().state
                != SstvShareTransferState::Completed
            || restored.transfer->snapshot().providerUploadId
                != record.providerSessionId
            || !isSafeShareIdentifier(
                restored.transfer->snapshot().remoteObjectId)) {
            return SstvRemoteCopyAction::Unavailable;
        }
        const auto capabilities = shareProvider->capabilities();
        if (capabilities.remoteDelete) {
            return SstvRemoteCopyAction::Delete;
        }
        return capabilities.revocation ? SstvRemoteCopyAction::Revoke
                                       : SstvRemoteCopyAction::Unavailable;
    }

    void completeRemoteCopyOutcome(
        const QString& transferId,
        quint64 expectedRevision,
        SstvRemoteCopyAction action,
        SstvShareProviderResult result)
    {
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record || record->revision != expectedRevision
            || record->state != SstvManagedTransferState::Completed
            || record->direction != SstvManagedTransferDirection::Upload) {
            completeAction(transferId, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Conflict,
                QStringLiteral("remote copy history state changed")));
            return;
        }
        if (result.ok()) {
            record->state = action == SstvRemoteCopyAction::Delete
                ? SstvManagedTransferState::RemoteDeleted
                : SstvManagedTransferState::RemoteRevoked;
            record->lastFailure = SstvShareProviderFailure::None;
            record->lastErrorRedacted.clear();
        } else {
            record->lastFailure = result.category();
            record->lastErrorRedacted = redactShareSecrets(
                result.redactedDiagnostic()).left(512);
        }
        record->nextRetryUtc = {};
        if (!updateRecord(*record, &error)) {
            completeAction(transferId, SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                error.isEmpty()
                    ? QStringLiteral("remote copy result was not persisted")
                    : error));
            return;
        }
        completeAction(transferId, result);
    }

    void onRemoteCopyRemoved(
        const QString& transferId,
        quint64 expectedRevision,
        SstvRemoteCopyAction action,
        SstvShareProviderResult result)
    {
        const QString stage = action == SstvRemoteCopyAction::Delete
            ? QStringLiteral("remote-copy-delete")
            : QStringLiteral("remote-copy-revoke");
        if (!claim(transferId, stage)) {
            return;
        }
        completeRemoteCopyOutcome(transferId, expectedRevision, action,
                                  std::move(result));
    }

    void onRemoteRevokeRehydrated(
        const QString& transferId,
        quint64 expectedRevision,
        SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("remote-copy-rehydrate"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        const auto shareProvider = record ? provider(record->providerId) : nullptr;
        if (!record || record->revision != expectedRevision
            || record->state != SstvManagedTransferState::Completed
            || !shareProvider) {
            completeAction(transferId, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Conflict,
                QStringLiteral("remote revoke history state changed")));
            return;
        }
        if (!result.ok()) {
            completeRemoteCopyOutcome(
                transferId, expectedRevision, SstvRemoteCopyAction::Revoke,
                std::move(result));
            return;
        }
        if (!shareProvider->capabilities().revocation
            || result.handle().opaqueId != record->providerSessionId
            || result.handle().committedBytes > record->byteSize) {
            completeRemoteCopyOutcome(
                transferId, expectedRevision, SstvRemoteCopyAction::Revoke,
                SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("provider restart identity changed before revoke")));
            return;
        }
        const QString stage = QStringLiteral("remote-copy-revoke");
        const QPointer<SstvShareQueueManager> guard(owner);
        const SstvShareOperationId operation = shareProvider->revokeAsync(
            result.handle(),
            [guard, id = transferId, expectedRevision](
                SstvShareProviderResult revokeResult) mutable {
                deliverToManager(
                    guard,
                    [id, expectedRevision,
                     revokeResult = std::move(revokeResult)](
                        SstvShareQueueManager& manager) mutable {
                        manager.m_impl->onRemoteCopyRemoved(
                            id, expectedRevision,
                            SstvRemoteCopyAction::Revoke,
                            std::move(revokeResult));
                    });
            });
        rememberOperation(transferId, stage, shareProvider, operation);
        if (operation == 0U) {
            completeRemoteCopyOutcome(
                transferId, expectedRevision, SstvRemoteCopyAction::Revoke,
                SstvShareProviderResult::failure(
                    SstvShareProviderFailure::PermanentProviderFailure,
                    QStringLiteral("provider did not start remote revocation")));
        }
    }

    SstvShareOperationId dispatchRemoteCopyRemoval(
        const SstvManagedTransferRecord& record,
        SstvRemoteCopyAction action,
        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        const auto restored = restoreSstvShareTransfer(
            record.transferPersistenceJson, now(), false);
        if (!restored.ok()) {
            return 0U;
        }
        const QPointer<SstvShareQueueManager> guard(owner);
        if (action == SstvRemoteCopyAction::Delete) {
            const QString stage = QStringLiteral("remote-copy-delete");
            const SstvShareOperationId operation =
                shareProvider->deleteRemoteObjectAsync(
                    restored.transfer->snapshot().remoteObjectId,
                    [guard, id = record.transferId,
                     expectedRevision = record.revision](
                        SstvShareProviderResult result) mutable {
                        deliverToManager(
                            guard,
                            [id, expectedRevision,
                             result = std::move(result)](
                                SstvShareQueueManager& manager) mutable {
                                manager.m_impl->onRemoteCopyRemoved(
                                    id, expectedRevision,
                                    SstvRemoteCopyAction::Delete,
                                    std::move(result));
                            });
                    });
            rememberOperation(record.transferId, stage, shareProvider,
                              operation);
            return operation;
        }
        if (action != SstvRemoteCopyAction::Revoke) {
            return 0U;
        }
        const QString stage = QStringLiteral("remote-copy-rehydrate");
        const SstvShareOperationId operation = shareProvider->createUploadAsync(
            restored.transfer->manifest(), record.idempotencyKey,
            [guard, id = record.transferId,
             expectedRevision = record.revision](
                SstvShareProviderResult result) mutable {
                deliverToManager(
                    guard,
                    [id, expectedRevision,
                     result = std::move(result)](
                        SstvShareQueueManager& manager) mutable {
                        manager.m_impl->onRemoteRevokeRehydrated(
                            id, expectedRevision, std::move(result));
                    });
            });
        rememberOperation(record.transferId, stage, shareProvider, operation);
        return operation;
    }

    void driveAgain()
    {
        QString ignored;
        owner->processDue(&ignored);
    }

    bool markUploadFailure(SstvManagedTransferRecord record,
                           const SstvShareProviderResult& result)
    {
        const auto restored = restoreSstvShareTransfer(
            record.transferPersistenceJson, now(), false);
        if (!restored.ok()) {
            return false;
        }
        SstvShareTransfer transfer = *restored.transfer;
        if (!transfer.handleFailure(result.category(), now(),
                                    result.retryAfterMs())) {
            transfer.expireIfNeeded(now());
        }
        record.lastErrorRedacted =
            redactShareSecrets(result.redactedDiagnostic()).left(512);
        if (isRetryableShareProviderFailure(result.category())
            && !record.providerSessionId.isEmpty()) {
            uploadResumeNeeded.insert(record.transferId);
        }
        return saveUpload(record, transfer);
    }

    bool markDownloadFailure(SstvManagedTransferRecord record,
                             const SstvShareProviderResult& result,
                             bool preserveActionState = false)
    {
        record.lastFailure = result.category();
        record.lastErrorRedacted =
            redactShareSecrets(result.redactedDiagnostic()).left(512);
        record.nextRetryUtc = {};
        if (result.category() == SstvShareProviderFailure::Cancelled) {
            record.state = SstvManagedTransferState::Cancelled;
        } else if (result.category()
                   == SstvShareProviderFailure::RejectedRecipient) {
            record.state = SstvManagedTransferState::Rejected;
        } else if (isRetryableShareProviderFailure(result.category())
                   && record.attempts < store.config().retryPolicy.maximumRetries) {
            ++record.attempts;
            const auto manifest = parseSstvShareManifestV1(
                record.canonicalManifestJson);
            if (!manifest.ok()) {
                record.state = SstvManagedTransferState::Failed;
            } else {
                const SstvShareTransfer delaySource(
                    *manifest.manifest, store.config().retryPolicy);
                record.nextRetryUtc = now().addMSecs(
                    delaySource.deterministicRetryDelayMs(
                        record.attempts, result.retryAfterMs()));
                if (!preserveActionState) {
                    record.state = SstvManagedTransferState::RetryScheduled;
                }
            }
        } else {
            record.state = SstvManagedTransferState::Failed;
        }
        return updateRecord(record);
    }

    bool updateRecordAndInbox(SstvManagedTransferRecord& record,
                              SstvInboxDisposition disposition,
                              QString* error = nullptr)
    {
        auto item = store.inboxItem(record.providerId,
                                    record.incomingId, error);
        if (!item) {
            return false;
        }
        record.updatedUtc = notBefore(now(), record.createdUtc);
        if (item->disposition != SstvInboxDisposition::ProviderDeleted) {
            item->disposition = disposition;
        }
        item->transferId = record.transferId;
        item->updatedUtc = notBefore(now(), item->receivedUtc);
        return store.updateDownloadTransferAndInbox(record, *item, error);
    }

    void onUploadLookup(const QString& transferId,
                        SstvShareProviderResult result,
                        SstvShareRecipientRecord recipient)
    {
        if (!claim(transferId, QStringLiteral("lookup"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markUploadFailure(*record, result);
            driveAgain();
            return;
        }
        const QByteArray encoded = storedRecipientJson(recipient);
        if (!validateShareRecipientRecord(recipient).ok()
            || recipient.providerId != record->providerId
            || recipient.stableRecipientId != record->recipientId
            || recipient.trust == SstvShareRecipientTrust::Blocked
            || encoded.isEmpty()) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::RejectedRecipient,
                QStringLiteral("recipient lookup returned an invalid identity")));
            driveAgain();
            return;
        }
        record->recipientJson = encoded;
        if (!updateRecord(*record)) {
            return;
        }
        driveAgain();
    }

    void onUploadCreate(const QString& transferId,
                        SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("create"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markUploadFailure(*record, result);
            driveAgain();
            return;
        }
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, now(), false);
        if (!restored.ok() || !isSafeShareIdentifier(result.handle().opaqueId)
            || result.handle().committedBytes > record->byteSize) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("provider returned an invalid upload session")));
            driveAgain();
            return;
        }
        SstvShareTransfer transfer = *restored.transfer;
        if (transfer.snapshot().state == SstvShareTransferState::Preparing
            && transfer.manifest().encryption.mode
                == SstvShareEncryptionMode::EndToEnd
            && !transfer.beginEncrypting(now())) {
            return;
        }
        if (!transfer.bindProviderUpload(record->idempotencyKey,
                                         result.handle().opaqueId)
            || !transfer.beginUploading(now())
            || !transfer.recordProgress(result.handle().committedBytes, now())) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("provider upload session violated state invariants")));
            driveAgain();
            return;
        }
        uploadResumeNeeded.remove(transferId);
        if (saveUpload(*record, transfer)) {
            driveAgain();
        }
    }

    void onUploadRehydrate(const QString& transferId,
                           SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("rehydrate"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()
            || result.handle().opaqueId != record->providerSessionId
            || result.handle().committedBytes > record->byteSize) {
            if (record->cancelRequested) {
                const auto restored = restoreSstvShareTransfer(
                    record->transferPersistenceJson, now(), false);
                if (restored.ok()) {
                    SstvShareTransfer transfer = *restored.transfer;
                    if (!transfer.handleFailure(
                            SstvShareProviderFailure::Cancelled, now())) {
                        transfer.cancel();
                    }
                    record->lastErrorRedacted = result.ok()
                        ? QStringLiteral("provider restart identity changed")
                        : result.redactedDiagnostic();
                    saveUpload(*record, transfer);
                }
            } else {
                markUploadFailure(*record,
                    result.ok() ? SstvShareProviderResult::failure(
                        SstvShareProviderFailure::Conflict,
                        QStringLiteral("provider restart identity changed"))
                                : result);
            }
            driveAgain();
            return;
        }
        uploadRecreateNeeded.remove(transferId);
        const auto shareProvider = provider(record->providerId);
        if (record->cancelRequested) {
            driveAgain();
            return;
        }
        if (shareProvider && shareProvider->capabilities().resumableUpload) {
            uploadResumeNeeded.insert(transferId);
        } else if (record->byteOffset == record->byteSize) {
            singleShotReplayNeeded.insert(transferId);
        }
        driveAgain();
    }

    void onUploadResume(const QString& transferId,
                        quint64 previousOffset,
                        SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("resume"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markUploadFailure(*record, result);
            driveAgain();
            return;
        }
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, now(), false);
        if (!restored.ok()
            || result.handle().opaqueId != record->providerSessionId
            || result.handle().committedBytes < previousOffset
            || result.handle().committedBytes > record->byteSize) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("remote resume offset moved backwards or out of bounds")));
            driveAgain();
            return;
        }
        SstvShareTransfer transfer = *restored.transfer;
        if (!transfer.recordProgress(result.handle().committedBytes, now())) {
            return;
        }
        uploadResumeNeeded.remove(transferId);
        if (saveUpload(*record, transfer)) {
            driveAgain();
        }
    }

    void onUploadChunk(const QString& transferId,
                       quint64 expectedCommitted,
                       SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("upload-chunk"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markUploadFailure(*record, result);
            driveAgain();
            return;
        }
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, now(), false);
        if (!restored.ok()
            || result.handle().opaqueId != record->providerSessionId
            || expectedCommitted < record->byteOffset
            || result.handle().committedBytes != expectedCommitted) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("provider committed an unexpected upload offset")));
            driveAgain();
            return;
        }
        SstvShareTransfer transfer = *restored.transfer;
        const quint64 newlyCommitted = expectedCommitted - record->byteOffset;
        if (!transfer.recordProgress(expectedCommitted, now())) {
            return;
        }
        if (expectedCommitted == record->byteSize) {
            quint64 bytes = 0U;
            const auto digest = sha256File(record->sourcePath,
                                           record->byteSize, &bytes, &error);
            if (!digest || bytes != record->byteSize
                || *digest != record->payloadSha256
                || !transfer.waitForAcknowledgement(now())) {
                markUploadFailure(*record, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("upload source changed before completion")));
                driveAgain();
                return;
            }
        }
        addDiagnosticBytes(true, newlyCommitted);
        if (saveUpload(*record, transfer)) {
            driveAgain();
        }
    }

    void onUploadComplete(const QString& transferId,
                          SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("complete"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markUploadFailure(*record, result);
            driveAgain();
            return;
        }
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, now(), false);
        if (!restored.ok() || !isSafeShareIdentifier(result.handle().opaqueId)) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("provider returned an invalid remote object")));
            driveAgain();
            return;
        }
        SstvShareTransfer transfer = *restored.transfer;
        if (!transfer.markCompleted(record->idempotencyKey,
                                    result.handle().opaqueId, now())) {
            return;
        }
        record->lastErrorRedacted.clear();
        saveUpload(*record, transfer);
        driveAgain();
    }

    void onUploadReplay(const QString& transferId,
                        SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("replay-upload"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markUploadFailure(*record, result);
            driveAgain();
            return;
        }
        if (result.handle().opaqueId != record->providerSessionId
            || result.handle().committedBytes != record->byteSize) {
            markUploadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("single-shot replay returned an invalid offset")));
            driveAgain();
            return;
        }
        addDiagnosticBytes(true, record->byteSize);
        singleShotReplayNeeded.remove(transferId);
        driveAgain();
    }

    void onRemoteCancel(const QString& transferId,
                        SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("cancel-upload"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, now(), false);
        if (!restored.ok()) {
            return;
        }
        SstvShareTransfer transfer = *restored.transfer;
        if (!transfer.handleFailure(
                SstvShareProviderFailure::Cancelled, now())) {
            transfer.cancel();
        }
        record->lastErrorRedacted = result.ok() ? QString {}
            : redactShareSecrets(result.redactedDiagnostic()).left(512);
        record->cancelDispatched = true;
        saveUpload(*record, transfer);
        completeAction(transferId, SstvShareProviderResult::failure(
            SstvShareProviderFailure::Cancelled,
            QStringLiteral("transfer cancelled")));
        driveAgain();
    }

    bool finishDownloadedPayload(SstvManagedTransferRecord& record)
    {
        QString error;
        quint64 bytes = 0U;
        const auto digest = sha256File(record.stagingPath,
                                       record.byteSize, &bytes, &error);
        if (!digest || bytes != record.byteSize
            || *digest != record.payloadSha256) {
            record.state = SstvManagedTransferState::Failed;
            record.lastFailure = SstvShareProviderFailure::Integrity;
            record.lastErrorRedacted = QStringLiteral(
                "downloaded payload checksum verification failed");
            updateRecord(record);
            return false;
        }
        const auto parsed = parseSstvShareManifestV1(
            record.canonicalManifestJson);
        const auto item = store.inboxItem(
            record.providerId, record.incomingId, &error);
        const QString validatedRoot = QDir(store.config().downloadRoot)
            .absoluteFilePath(QStringLiteral("validated"));
        const auto validation = parsed.ok() && item
            ? validateAndStageIncomingMedia(
                  record.stagingPath, validatedRoot, record.transferId,
                  record.incomingId, *parsed.manifest, item->receivedUtc,
                  item->expiresUtc)
            : SstvIncomingMediaValidationResult {};
        if (!validation.ok()
            || validation.handoff->stagedCanonicalPath
                != QFileInfo(record.destinationPath).canonicalFilePath()) {
            record.state = SstvManagedTransferState::Failed;
            record.lastFailure = validation.ok()
                ? SstvShareProviderFailure::Integrity : validation.failure;
            record.lastErrorRedacted = validation.ok()
                ? QStringLiteral("validated staging identity changed")
                : (validation.redactedDiagnostic.isEmpty()
                       ? QStringLiteral("downloaded payload failed native image validation")
                       : validation.redactedDiagnostic);
            updateRecord(record);
            return false;
        }
        record.byteOffset = record.byteSize;
        record.state = SstvManagedTransferState::AwaitingAcceptance;
        record.lastFailure = SstvShareProviderFailure::None;
        record.lastErrorRedacted.clear();
        record.nextRetryUtc = {};
        return updateRecordAndInbox(
            record, SstvInboxDisposition::AwaitingAcceptance);
    }

    bool reconcileStaging(SstvManagedTransferRecord& record)
    {
        const QFileInfo info(record.stagingPath);
        if (!info.exists()) {
            return record.byteOffset == 0U;
        }
        const QString partialRoot = QDir(store.config().downloadRoot)
            .absoluteFilePath(QStringLiteral(".partial"));
        if (!info.isFile() || info.isSymLink()
            || !pathWithin(QFileInfo(partialRoot).canonicalFilePath(),
                           record.stagingPath, true)
            || info.size() < 0
            || static_cast<quint64>(info.size()) > record.byteSize
            || static_cast<quint64>(info.size()) < record.byteOffset) {
            return false;
        }
        const quint64 size = static_cast<quint64>(info.size());
        if (size > record.byteOffset) {
            record.byteOffset = size;
            record.state = SstvManagedTransferState::DownloadQueued;
            if (!updateRecord(record)) {
                return false;
            }
        }
        return true;
    }

    void onDownloadChunk(const QString& transferId,
                         quint64 requestedOffset,
                         quint64 maximumBytes,
                         SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("download"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markDownloadFailure(*record, result);
            driveAgain();
            return;
        }
        const QByteArray payload = result.boundedPayload();
        const quint64 payloadSize = static_cast<quint64>(payload.size());
        if (payload.isEmpty() || payloadSize > maximumBytes
            || requestedOffset != record->byteOffset
            || payloadSize > record->byteSize - requestedOffset) {
            markDownloadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("provider returned an invalid download chunk")));
            driveAgain();
            return;
        }
        const QFileInfo stagingInfo(record->stagingPath);
        if (stagingInfo.isSymLink()
            || (stagingInfo.exists()
                && static_cast<quint64>(stagingInfo.size())
                    != requestedOffset)) {
            markDownloadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("download staging file changed unexpectedly")));
            driveAgain();
            return;
        }
        QFile staging(record->stagingPath);
        if (!staging.open(QIODevice::ReadWrite)
            || !staging.setPermissions(QFileDevice::ReadOwner
                                       | QFileDevice::WriteOwner)
            || !staging.seek(static_cast<qint64>(requestedOffset))
            || staging.write(payload) != payload.size()
            || !staging.flush()) {
            markDownloadFailure(*record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                QStringLiteral("could not durably append download chunk")));
            driveAgain();
            return;
        }
        staging.close();
        addDiagnosticBytes(false, payloadSize);
        if (fault && fault->shouldFail(
                SstvShareFaultPoint::AfterDownloadWriteBeforeCheckpoint,
                transferId)) {
            // Deliberately leave the durable file ahead of the DB checkpoint.
            // initialize()/reconcileStaging() treats the file length as the
            // crash-recovery authority, without downloading the bytes twice.
            return;
        }
        record->byteOffset = requestedOffset + payloadSize;
        record->state = SstvManagedTransferState::DownloadQueued;
        record->lastFailure = SstvShareProviderFailure::None;
        record->lastErrorRedacted.clear();
        record->nextRetryUtc = {};
        if (record->byteOffset == record->byteSize) {
            finishDownloadedPayload(*record);
        } else {
            updateRecord(*record);
        }
        driveAgain();
    }

    void onAcknowledge(const QString& transferId,
                       SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("acknowledge"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markDownloadFailure(*record, result, true);
            const auto updated = store.transfer(transferId);
            if (!updated || isTerminalManagedTransferState(updated->state)) {
                completeAction(transferId, result);
            }
            driveAgain();
            return;
        }
        record->state = SstvManagedTransferState::Acknowledged;
        record->lastFailure = SstvShareProviderFailure::None;
        record->lastErrorRedacted.clear();
        record->nextRetryUtc = {};
        updateRecordAndInbox(*record, SstvInboxDisposition::Acknowledged);
        completeAction(transferId, result);
        driveAgain();
    }

    void onReject(const QString& transferId,
                  SstvShareProviderResult result)
    {
        if (!claim(transferId, QStringLiteral("reject"))) {
            return;
        }
        QString error;
        auto record = store.transfer(transferId, &error);
        if (!record) {
            return;
        }
        if (!result.ok()) {
            markDownloadFailure(*record, result, true);
            const auto updated = store.transfer(transferId);
            if (!updated || isTerminalManagedTransferState(updated->state)) {
                completeAction(transferId, result);
            }
            driveAgain();
            return;
        }
        record->state = SstvManagedTransferState::Rejected;
        record->lastFailure = SstvShareProviderFailure::None;
        record->lastErrorRedacted.clear();
        record->nextRetryUtc = {};
        updateRecordAndInbox(*record, SstvInboxDisposition::Rejected);
        if (!record->stagingPath.isEmpty()) {
            QFile::remove(record->stagingPath);
        }
        if (!record->destinationPath.isEmpty()) {
            QFile::remove(record->destinationPath);
        }
        completeAction(transferId, result);
        driveAgain();
    }

    bool dispatchUploadRehydrate(
        const SstvManagedTransferRecord& record,
        const SstvShareManifestV1& manifest,
        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        const QString stage = QStringLiteral("rehydrate");
        const QPointer<SstvShareQueueManager> guard(owner);
        const SstvShareOperationId operation =
            shareProvider->createUploadAsync(
                manifest, record.idempotencyKey,
                [guard, id = record.transferId](
                    SstvShareProviderResult result) mutable {
                    deliverToManager(guard,
                        [id, result = std::move(result)](
                            SstvShareQueueManager& manager) mutable {
                            manager.m_impl->onUploadRehydrate(
                                id, std::move(result));
                        });
                });
        rememberOperation(record.transferId, stage, shareProvider, operation);
        if (operation == 0U) {
            if (!record.cancelRequested) {
                markUploadFailure(record,
                    SstvShareProviderResult::failure(
                        SstvShareProviderFailure::PermanentProviderFailure,
                        QStringLiteral("provider did not start session rehydration")));
            }
            return false;
        }
        return true;
    }

    bool dispatchSingleShotReplay(
        SstvManagedTransferRecord record,
        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        QString error;
        const auto source = secureUploadPath(
            store.config(), record.sourcePath, &error);
        QFile file(source ? *source : QString {});
        if (!source || !file.open(QIODevice::ReadOnly)
            || QFileInfo(*source).size()
                != static_cast<qint64>(record.byteSize)
            || record.byteSize > store.config().limits.uploadChunkBytes) {
            markUploadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("single-shot replay source is unavailable")));
            return false;
        }
        const QByteArray bytes = file.read(static_cast<qint64>(record.byteSize));
        const QString chunkDigest = QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                .toHex());
        if (static_cast<quint64>(bytes.size()) != record.byteSize
            || chunkDigest != record.payloadSha256) {
            markUploadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("single-shot replay source changed")));
            return false;
        }
        const QPointer<SstvShareQueueManager> guard(owner);
        const SstvShareUploadHandle handle {record.providerSessionId, 0U};
        const SstvShareOperationId operation =
            shareProvider->uploadChunkAsync(
                handle, 0U, bytes, chunkDigest, {},
                [guard, id = record.transferId](
                    SstvShareProviderResult result) mutable {
                    deliverToManager(guard,
                        [id, result = std::move(result)](
                            SstvShareQueueManager& manager) mutable {
                            manager.m_impl->onUploadReplay(
                                id, std::move(result));
                        });
                });
        rememberOperation(record.transferId, QStringLiteral("replay-upload"),
                          shareProvider, operation);
        if (operation == 0U) {
            markUploadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                QStringLiteral("provider did not start single-shot replay")));
            return false;
        }
        return true;
    }

    bool dispatchUpload(SstvManagedTransferRecord record,
                        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        auto restored = restoreSstvShareTransfer(
            record.transferPersistenceJson, now(), false);
        if (!restored.ok()) {
            return false;
        }
        SstvShareTransfer transfer = *restored.transfer;
        if (transfer.expireIfNeeded(now())) {
            saveUpload(record, transfer);
            return false;
        }
        // A persisted user pause is authoritative. In particular, restart
        // recovery may have marked a provider session for rehydration; that
        // network operation must not start until the user explicitly resumes.
        if (transfer.snapshot().state == SstvShareTransferState::Paused) {
            return false;
        }
        if (transfer.snapshot().state == SstvShareTransferState::RetryScheduled) {
            if (!transfer.activateScheduledRetry(now())) {
                return false;
            }
            if (!saveUpload(record, transfer)) {
                return false;
            }
        }
        if (uploadRecreateNeeded.contains(record.transferId)) {
            return dispatchUploadRehydrate(
                record, transfer.manifest(), shareProvider);
        }
        if (transfer.snapshot().state == SstvShareTransferState::Queued) {
            if (!transfer.beginPreparing(now())) {
                return false;
            }
            if (transfer.manifest().encryption.mode
                    == SstvShareEncryptionMode::EndToEnd
                && !transfer.beginEncrypting(now())) {
                return false;
            }
            if (!saveUpload(record, transfer)) {
                return false;
            }
        }

        const auto state = transfer.snapshot().state;
        if (state == SstvShareTransferState::Preparing
            || state == SstvShareTransferState::Encrypting) {
            if (record.providerSessionId.isEmpty()) {
                if (shareProvider->capabilities().recipientLookup
                    && record.recipientJson.isEmpty()) {
                    const QString stage = QStringLiteral("lookup");
                    const QPointer<SstvShareQueueManager> guard(owner);
                    const SstvShareOperationId operation =
                        shareProvider->lookupRecipientAsync(
                            record.recipientId,
                            [guard, id = record.transferId](
                                SstvShareProviderResult result,
                                SstvShareRecipientRecord recipient) mutable {
                                deliverToManager(guard,
                                    [id, result = std::move(result),
                                     recipient = std::move(recipient)](
                                        SstvShareQueueManager& manager) mutable {
                                        manager.m_impl->onUploadLookup(
                                            id, std::move(result),
                                            std::move(recipient));
                                    });
                            });
                    rememberOperation(record.transferId, stage,
                                      shareProvider, operation);
                    if (operation == 0U) {
                        markUploadFailure(record,
                            SstvShareProviderResult::failure(
                                SstvShareProviderFailure::PermanentProviderFailure,
                                QStringLiteral("provider did not start recipient lookup")));
                        return false;
                    }
                    return true;
                }
                const QString stage = QStringLiteral("create");
                const QPointer<SstvShareQueueManager> guard(owner);
                const SstvShareOperationId operation =
                    shareProvider->createUploadAsync(
                        transfer.manifest(), record.idempotencyKey,
                        [guard, id = record.transferId](
                            SstvShareProviderResult result) mutable {
                            deliverToManager(guard,
                                [id, result = std::move(result)](
                                    SstvShareQueueManager& manager) mutable {
                                    manager.m_impl->onUploadCreate(
                                        id, std::move(result));
                                });
                        });
                rememberOperation(record.transferId, stage,
                                  shareProvider, operation);
                if (operation == 0U) {
                    markUploadFailure(record,
                        SstvShareProviderResult::failure(
                            SstvShareProviderFailure::PermanentProviderFailure,
                            QStringLiteral("provider did not start upload creation")));
                    return false;
                }
                return true;
            }
            if (!transfer.beginUploading(now()) || !saveUpload(record, transfer)) {
                return false;
            }
        }

        if (transfer.snapshot().state == SstvShareTransferState::Uploading) {
            if (uploadResumeNeeded.contains(record.transferId)
                && shareProvider->capabilities().resumableUpload) {
                const QString stage = QStringLiteral("resume");
                const QPointer<SstvShareQueueManager> guard(owner);
                const SstvShareUploadHandle handle {
                    record.providerSessionId, record.byteOffset};
                const SstvShareOperationId operation =
                    shareProvider->resumeUploadAsync(
                        handle,
                        [guard, id = record.transferId,
                         previous = record.byteOffset](
                            SstvShareProviderResult result) mutable {
                            deliverToManager(guard,
                                [id, previous, result = std::move(result)](
                                    SstvShareQueueManager& manager) mutable {
                                    manager.m_impl->onUploadResume(
                                        id, previous, std::move(result));
                                });
                        });
                rememberOperation(record.transferId, stage,
                                  shareProvider, operation);
                if (operation == 0U) {
                    markUploadFailure(record,
                        SstvShareProviderResult::failure(
                            SstvShareProviderFailure::PermanentProviderFailure,
                            QStringLiteral("provider did not start upload resume")));
                    return false;
                }
                return true;
            }
            uploadResumeNeeded.remove(record.transferId);
            const quint64 remaining = record.byteSize - record.byteOffset;
            if (remaining == 0U) {
                if (!transfer.waitForAcknowledgement(now())
                    || !saveUpload(record, transfer)) {
                    return false;
                }
                driveAgain();
                return false;
            }
            quint64 chunkBytes = store.config().limits.uploadChunkBytes;
            const auto capabilities = shareProvider->capabilities();
            if (capabilities.chunkedUpload) {
                chunkBytes = std::min(chunkBytes,
                                      capabilities.maximumChunkBytes);
            } else if (remaining > chunkBytes) {
                markUploadFailure(record, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Validation,
                    QStringLiteral("single-shot upload exceeds manager memory bound")));
                return false;
            }
            chunkBytes = std::min(chunkBytes, remaining);
            QString pathError;
            const auto safeSource = secureUploadPath(
                store.config(), record.sourcePath, &pathError);
            QFile file(safeSource ? *safeSource : QString {});
            if (!safeSource || !file.open(QIODevice::ReadOnly)
                || !file.seek(static_cast<qint64>(record.byteOffset))) {
                markUploadFailure(record, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("upload source became unavailable")));
                return false;
            }
            const QByteArray chunk = file.read(static_cast<qint64>(chunkBytes));
            if (static_cast<quint64>(chunk.size()) != chunkBytes) {
                markUploadFailure(record, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("upload source changed while reading")));
                return false;
            }
            const QString digest = QString::fromLatin1(
                QCryptographicHash::hash(chunk, QCryptographicHash::Sha256)
                    .toHex());
            const QString stage = QStringLiteral("upload-chunk");
            const QPointer<SstvShareQueueManager> guard(owner);
            const quint64 expected = record.byteOffset + chunkBytes;
            const SstvShareUploadHandle handle {
                record.providerSessionId, record.byteOffset};
            const SstvShareOperationId operation =
                shareProvider->uploadChunkAsync(
                    handle, record.byteOffset, chunk, digest, {},
                    [guard, id = record.transferId, expected](
                        SstvShareProviderResult result) mutable {
                        deliverToManager(guard,
                            [id, expected, result = std::move(result)](
                                SstvShareQueueManager& manager) mutable {
                                manager.m_impl->onUploadChunk(
                                    id, expected, std::move(result));
                            });
                    });
            rememberOperation(record.transferId, stage,
                              shareProvider, operation);
            if (operation == 0U) {
                markUploadFailure(record, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::PermanentProviderFailure,
                    QStringLiteral("provider did not start upload chunk")));
                return false;
            }
            return true;
        }

        if (transfer.snapshot().state
            == SstvShareTransferState::WaitingForAcknowledgement) {
            if (singleShotReplayNeeded.contains(record.transferId)) {
                return dispatchSingleShotReplay(record, shareProvider);
            }
            const QString stage = QStringLiteral("complete");
            const QPointer<SstvShareQueueManager> guard(owner);
            const SstvShareUploadHandle handle {
                record.providerSessionId, record.byteOffset};
            const SstvShareOperationId operation =
                shareProvider->completeUploadAsync(
                    handle, record.idempotencyKey,
                    [guard, id = record.transferId](
                        SstvShareProviderResult result) mutable {
                        deliverToManager(guard,
                            [id, result = std::move(result)](
                                SstvShareQueueManager& manager) mutable {
                                manager.m_impl->onUploadComplete(
                                    id, std::move(result));
                            });
                    });
            rememberOperation(record.transferId, stage,
                              shareProvider, operation);
            if (operation == 0U) {
                markUploadFailure(record, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::PermanentProviderFailure,
                    QStringLiteral("provider did not start upload completion")));
                return false;
            }
            return true;
        }
        return false;
    }

    bool dispatchRemoteCancel(
        SstvManagedTransferRecord record,
        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        if (uploadRecreateNeeded.contains(record.transferId)) {
            const auto manifest = parseSstvShareManifestV1(
                record.canonicalManifestJson);
            if (!manifest.ok()) {
                return false;
            }
            return dispatchUploadRehydrate(
                record, *manifest.manifest, shareProvider);
        }
        if (record.providerSessionId.isEmpty() || record.cancelDispatched) {
            const auto restored = restoreSstvShareTransfer(
                record.transferPersistenceJson, now(), false);
            if (!restored.ok()) {
                return false;
            }
            SstvShareTransfer transfer = *restored.transfer;
            if (!transfer.handleFailure(
                    SstvShareProviderFailure::Cancelled, now())) {
                transfer.cancel();
            }
            return saveUpload(record, transfer);
        }
        // The transport cancellation is idempotent (idempotency key / DELETE
        // accepting 404). The completion marker is persisted afterwards, so a
        // crash can repeat a request but cannot lose the logical cancellation.
        const QString stage = QStringLiteral("cancel-upload");
        const QPointer<SstvShareQueueManager> guard(owner);
        const SstvShareUploadHandle handle {
            record.providerSessionId, record.byteOffset};
        const SstvShareOperationId operation =
            shareProvider->cancelUploadAsync(
                handle,
                [guard, id = record.transferId](
                    SstvShareProviderResult result) mutable {
                    deliverToManager(guard,
                        [id, result = std::move(result)](
                            SstvShareQueueManager& manager) mutable {
                            manager.m_impl->onRemoteCancel(
                                id, std::move(result));
                        });
                });
        rememberOperation(record.transferId, stage, shareProvider, operation);
        if (operation == 0U) {
            const auto restored = restoreSstvShareTransfer(
                record.transferPersistenceJson, now(), false);
            if (restored.ok()) {
                SstvShareTransfer transfer = *restored.transfer;
                if (!transfer.handleFailure(
                        SstvShareProviderFailure::Cancelled, now())) {
                    transfer.cancel();
                }
                saveUpload(record, transfer);
            }
            return false;
        }
        return true;
    }

    bool dispatchDownload(
        SstvManagedTransferRecord record,
        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        if (record.state == SstvManagedTransferState::RetryScheduled) {
            if (!record.nextRetryUtc.isValid() || now() < record.nextRetryUtc) {
                return false;
            }
            record.state = SstvManagedTransferState::DownloadQueued;
            record.nextRetryUtc = {};
            if (!updateRecord(record)) {
                return false;
            }
        }
        if (!reconcileStaging(record)) {
            markDownloadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("download staging file is unsafe or truncated")));
            return false;
        }
        if (record.byteOffset == record.byteSize) {
            return finishDownloadedPayload(record);
        }
        if (record.state != SstvManagedTransferState::DownloadQueued
            || !shareProvider->capabilities().download) {
            return false;
        }
        const quint64 providerBound =
            shareProvider->capabilities().maximumResponseBytes;
        const quint64 requested = std::min(
            {store.config().limits.downloadChunkBytes, providerBound,
             record.byteSize - record.byteOffset});
        if (requested == 0U) {
            markDownloadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("provider advertised an invalid download bound")));
            return false;
        }
        record.state = SstvManagedTransferState::Downloading;
        if (!updateRecord(record)) {
            return false;
        }
        const QString stage = QStringLiteral("download");
        const QPointer<SstvShareQueueManager> guard(owner);
        const quint64 offset = record.byteOffset;
        const SstvShareOperationId operation = shareProvider->downloadAsync(
            record.incomingId, offset, requested, {},
            [guard, id = record.transferId, offset, requested](
                SstvShareProviderResult result) mutable {
                deliverToManager(guard,
                    [id, offset, requested, result = std::move(result)](
                        SstvShareQueueManager& manager) mutable {
                        manager.m_impl->onDownloadChunk(
                            id, offset, requested, std::move(result));
                    });
            });
        rememberOperation(record.transferId, stage, shareProvider, operation);
        if (operation == 0U) {
            markDownloadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                QStringLiteral("provider did not start download")));
            return false;
        }
        return true;
    }

    bool dispatchAcknowledge(
        SstvManagedTransferRecord record,
        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        if (record.nextRetryUtc.isValid() && now() < record.nextRetryUtc) {
            return false;
        }
        const QString stage = QStringLiteral("acknowledge");
        const QPointer<SstvShareQueueManager> guard(owner);
        const SstvShareOperationId operation = shareProvider->acknowledgeAsync(
            record.incomingId,
            [guard, id = record.transferId](
                SstvShareProviderResult result) mutable {
                deliverToManager(guard,
                    [id, result = std::move(result)](
                        SstvShareQueueManager& manager) mutable {
                        manager.m_impl->onAcknowledge(id, std::move(result));
                    });
            });
        rememberOperation(record.transferId, stage, shareProvider, operation);
        if (operation == 0U) {
            markDownloadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                QStringLiteral("provider did not start acknowledgement")), true);
            return false;
        }
        return true;
    }

    bool dispatchReject(SstvManagedTransferRecord record,
                        const std::shared_ptr<SstvShareProvider>& shareProvider)
    {
        if (record.nextRetryUtc.isValid() && now() < record.nextRetryUtc) {
            return false;
        }
        const QString stage = QStringLiteral("reject");
        const QPointer<SstvShareQueueManager> guard(owner);
        const SstvShareOperationId operation = shareProvider->rejectAsync(
            record.incomingId,
            [guard, id = record.transferId](
                SstvShareProviderResult result) mutable {
                deliverToManager(guard,
                    [id, result = std::move(result)](
                        SstvShareQueueManager& manager) mutable {
                        manager.m_impl->onReject(id, std::move(result));
                    });
            });
        rememberOperation(record.transferId, stage, shareProvider, operation);
        if (operation == 0U) {
            markDownloadFailure(record, SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                QStringLiteral("provider did not start rejection")), true);
            return false;
        }
        return true;
    }

    SstvShareQueueManager* owner {nullptr};
    SstvShareQueueStore store;
    std::shared_ptr<SstvShareFaultInjector> fault;
    std::function<QDateTime()> clock;
    QHash<QString, std::shared_ptr<SstvShareProvider>> providers;
    QHash<QString, ActiveOperation> active;
    QHash<QString, SstvShareManagerCompletion> completions;
    QSet<QString> uploadRecreateNeeded;
    QSet<QString> uploadResumeNeeded;
    QSet<QString> singleShotReplayNeeded;
    quint64 uploadedBytes {0U};
    quint64 downloadedBytes {0U};
    QDateTime diagnosticsResetUtc;
    bool initialized {false};
    bool pumping {false};
    bool destroying {false};
};

SstvShareQueueManager::SstvShareQueueManager(
    SstvShareQueueConfig config,
    std::shared_ptr<SstvShareFaultInjector> faultInjector,
    std::function<QDateTime()> clock,
    QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(this, std::move(config),
                                    std::move(faultInjector),
                                    std::move(clock)))
{
}

SstvShareQueueManager::~SstvShareQueueManager()
{
    m_impl->destroying = true;
    const QList<Impl::ActiveOperation> operations = m_impl->active.values();
    m_impl->active.clear();
    m_impl->completions.clear();
    for (const auto& operation : operations) {
        if (operation.provider && operation.operationId != 0U) {
            operation.provider->cancelOperation(operation.operationId);
        }
    }
}

bool SstvShareQueueManager::registerProvider(
    std::shared_ptr<SstvShareProvider> provider,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !provider
        || !isSafeShareIdentifier(provider->providerId())
        || !provider->capabilities().strictTlsRequired
        || m_impl->providers.contains(provider->providerId())) {
        return fail(error, QStringLiteral("invalid or duplicate sharing provider"));
    }
    m_impl->providers.insert(provider->providerId(), std::move(provider));
    return true;
}

bool SstvShareQueueManager::initialize(QString* error)
{
    if (!m_impl->ownerThread(error)) {
        return false;
    }
    if (m_impl->initialized) {
        return true;
    }
    if (!m_impl->store.open(error)) {
        return false;
    }
    QString queryError;
    const auto active = m_impl->store.queryTransfers(
        SstvShareTransferView::Active,
        m_impl->store.config().limits.maximumRecords, &queryError);
    if (!queryError.isEmpty()) {
        return fail(error, queryError);
    }
    for (SstvManagedTransferRecord record : active) {
        if (record.direction == SstvManagedTransferDirection::Upload) {
            const auto restored = restoreSstvShareTransfer(
                record.transferPersistenceJson, m_impl->now(), true);
            if (!restored.ok()) {
                return fail(error, QStringLiteral("could not recover upload state"));
            }
            SstvShareTransfer transfer = *restored.transfer;
            if (!transfer.snapshot().providerUploadId.isEmpty()
                && !isTerminalShareTransferState(transfer.snapshot().state)
                && !(record.cancelRequested && record.cancelDispatched)) {
                m_impl->uploadRecreateNeeded.insert(record.transferId);
            }
            if (transfer.snapshot().restartRecoveries
                    != record.restartRecoveries
                || (transfer.snapshot().state
                        == SstvShareTransferState::RetryScheduled
                    && transfer.snapshot().retryResumeState
                        == SstvShareTransferState::Uploading
                    && !transfer.snapshot().providerUploadId.isEmpty())) {
                m_impl->uploadResumeNeeded.insert(record.transferId);
            }
            if (!m_impl->projectUpload(record, transfer, error)) {
                return false;
            }
            if (record.cancelRequested && record.cancelDispatched) {
                if (!transfer.handleFailure(
                        SstvShareProviderFailure::Cancelled, m_impl->now())) {
                    transfer.cancel();
                }
                if (!m_impl->projectUpload(record, transfer, error)) {
                    return false;
                }
            }
            if (!m_impl->updateRecord(record, error)) {
                return false;
            }
        } else {
            if (record.state == SstvManagedTransferState::Downloading) {
                record.state = SstvManagedTransferState::DownloadQueued;
                ++record.restartRecoveries;
                if (!m_impl->reconcileStaging(record)
                    || !m_impl->updateRecord(record, error)) {
                    return fail(error, QStringLiteral(
                        "could not recover download checkpoint"));
                }
            }
            if (record.state == SstvManagedTransferState::AwaitingAcceptance
                || record.state == SstvManagedTransferState::Accepted
                || record.state == SstvManagedTransferState::Acknowledging) {
                const auto manifest = parseSstvShareManifestV1(
                    record.canonicalManifestJson);
                const auto item = m_impl->store.inboxItem(
                    record.providerId, record.incomingId, error);
                const QString validatedRoot = QDir(
                    m_impl->store.config().downloadRoot)
                    .absoluteFilePath(QStringLiteral("validated"));
                const auto validation = manifest.ok() && item
                    ? inspectStagedIncomingMedia(
                          record.destinationPath, validatedRoot,
                          record.transferId, record.incomingId,
                          *manifest.manifest, item->receivedUtc,
                          item->expiresUtc)
                    : SstvIncomingMediaValidationResult {};
                if (!validation.ok()) {
                    record.state = SstvManagedTransferState::Failed;
                    record.lastFailure = validation.failure;
                    record.lastErrorRedacted =
                        validation.redactedDiagnostic.isEmpty()
                        ? QStringLiteral("validated incoming staging failed restart inspection")
                        : validation.redactedDiagnostic;
                    if (!m_impl->updateRecord(record, error)) {
                        return false;
                    }
                }
            }
        }
    }
    m_impl->initialized = true;
    return true;
}

bool SstvShareQueueManager::isInitialized() const noexcept
{
    return m_impl->initialized;
}

QString SstvShareQueueManager::queueUpload(
    const SstvShareManifestV1& manifest,
    const QString& sourcePath,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        fail(error, QStringLiteral("share queue manager is not initialized"));
        return {};
    }
    const auto shareProvider = m_impl->provider(manifest.providerId);
    if (!shareProvider || !manifest.validate(true, m_impl->now()).ok()
        || !validateShareProviderCompatibility(
                manifest, manifest.providerId,
                shareProvider->capabilities()).ok()) {
        fail(error, QStringLiteral("manifest/provider compatibility failed"));
        return {};
    }
    const auto source = secureUploadPath(m_impl->store.config(),
                                         sourcePath, error);
    quint64 bytes = 0U;
    const auto digest = source
        ? sha256File(*source, manifest.byteSize, &bytes, error)
        : std::optional<QString> {};
    if (!source || !digest || bytes != manifest.byteSize
        || *digest != manifest.sha256) {
        fail(error, QStringLiteral("upload source does not match its manifest"));
        return {};
    }
    if (!shareProvider->capabilities().chunkedUpload
        && manifest.byteSize > m_impl->store.config().limits.uploadChunkBytes) {
        fail(error, QStringLiteral("single-shot upload exceeds manager bound"));
        return {};
    }
    SstvShareTransfer transfer(manifest, m_impl->store.config().retryPolicy);
    if (!transfer.isValid() || !transfer.enqueue(m_impl->now())) {
        fail(error, QStringLiteral("could not enqueue upload state"));
        return {};
    }
    SstvManagedTransferRecord record;
    record.transferId = manifest.transferId.toString(QUuid::WithoutBraces);
    record.direction = SstvManagedTransferDirection::Upload;
    record.providerId = manifest.providerId;
    record.recipientId = manifest.recipientId;
    record.canonicalManifestJson = manifest.toCanonicalJson();
    record.sourcePath = *source;
    record.payloadSha256 = manifest.sha256;
    record.byteSize = manifest.byteSize;
    record.idempotencyKey = transfer.snapshot().idempotencyKey;
    record.createdUtc = m_impl->now();
    record.updatedUtc = record.createdUtc;
    if (!m_impl->projectUpload(record, transfer, error)
        || !m_impl->store.insertTransfer(record, error)) {
        return {};
    }
    return record.transferId;
}

SstvShareOperationId SstvShareQueueManager::refreshInboxAsync(
    const QString& providerId,
    SstvShareManagerCompletion completion)
{
    if (!m_impl->ownerThread(nullptr) || !m_impl->initialized
        || !isSafeShareIdentifier(providerId)) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("invalid inbox refresh request")));
        }
        return 0U;
    }
    const auto shareProvider = m_impl->provider(providerId);
    const QString key = QStringLiteral("inbox:") + providerId;
    if (!shareProvider || !shareProvider->capabilities().incomingList
        || m_impl->active.contains(key)
        || !m_impl->hasCapacityFor(providerId)) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::ProviderUnavailable,
                QStringLiteral("inbox provider is unavailable or busy")));
        }
        return 0U;
    }
    const qsizetype bound = std::min(
        m_impl->store.config().limits.maximumInboxItems,
        m_impl->store.config().limits.maximumQueryItems);
    const QPointer<SstvShareQueueManager> guard(this);
    const SstvShareOperationId operation = shareProvider->listIncomingAsync(
        bound,
        [guard, key, providerId, bound,
         completion = std::move(completion)](
            SstvShareProviderResult result,
            QVector<SstvShareIncomingItem> items) mutable {
            deliverToManager(guard,
                [key, providerId, bound, result = std::move(result),
                 items = std::move(items),
                 completion = std::move(completion)](
                    SstvShareQueueManager& manager) mutable {
                    if (!manager.m_impl->claim(key, QStringLiteral("inbox"))) {
                        return;
                    }
                    if (!result.ok()) {
                        if (completion) {
                            completion(result);
                        }
                        return;
                    }
                    if (items.size() > bound) {
                        result = SstvShareProviderResult::failure(
                            SstvShareProviderFailure::Validation,
                            QStringLiteral("provider inbox exceeded its bound"));
                    }
                    QSet<QString> incomingIds;
                    QVector<SstvPersistentInboxItem> pending;
                    if (result.ok()) {
                        for (const auto& item : items) {
                            if (!validateShareIncomingItem(item).ok()
                                || item.providerId != providerId
                                || incomingIds.contains(item.opaqueId)) {
                                result = SstvShareProviderResult::failure(
                                    SstvShareProviderFailure::Validation,
                                    QStringLiteral("provider inbox contained an invalid item"));
                                break;
                            }
                            incomingIds.insert(item.opaqueId);
                            QString lookupError;
                            const auto existing = manager.m_impl->store.inboxItem(
                                providerId, item.opaqueId, &lookupError);
                            if (!lookupError.isEmpty()) {
                                result = SstvShareProviderResult::failure(
                                    SstvShareProviderFailure::PermanentProviderFailure,
                                    lookupError);
                                break;
                            }
                            const auto senderBlock =
                                manager.m_impl->store.senderBlock(
                                    providerId, item.senderId, &lookupError);
                            if (!lookupError.isEmpty()) {
                                result = SstvShareProviderResult::failure(
                                    SstvShareProviderFailure::PermanentProviderFailure,
                                    lookupError);
                                break;
                            }
                            if (existing
                                && (existing->manifestSha256 != item.manifestSha256
                                    || existing->canonicalManifestJson
                                        != item.canonicalManifestJson
                                    || existing->senderId != item.senderId
                                    || existing->byteSize != item.byteSize
                                    || existing->expiresUtc != item.expiresUtc)) {
                                result = SstvShareProviderResult::failure(
                                    SstvShareProviderFailure::Conflict,
                                    QStringLiteral("incoming identity changed across refreshes"));
                                break;
                            }
                            SstvPersistentInboxItem persisted;
                            persisted.providerId = item.providerId;
                            persisted.incomingId = item.opaqueId;
                            persisted.senderId = item.senderId;
                            persisted.manifestSha256 = item.manifestSha256;
                            persisted.canonicalManifestJson =
                                item.canonicalManifestJson;
                            persisted.byteSize = item.byteSize;
                            persisted.receivedUtc = item.receivedUtc;
                            persisted.expiresUtc = item.expiresUtc;
                            persisted.disposition = existing
                                ? existing->disposition
                                : (senderBlock
                                    ? (senderBlock->scope
                                               == SstvSenderBlockScope::Provider
                                           ? SstvInboxDisposition::BlockedByProvider
                                           : SstvInboxDisposition::BlockedLocally)
                                    : (item.expiresUtc <= manager.m_impl->now()
                                        ? SstvInboxDisposition::Expired
                                        : SstvInboxDisposition::New));
                            persisted.transferId = existing
                                ? existing->transferId : QString {};
                            persisted.updatedUtc = notBefore(
                                manager.m_impl->now(), persisted.receivedUtc);
                            pending.push_back(std::move(persisted));
                        }
                    }
                    if (result.ok()) {
                        for (const auto& item : pending) {
                            QString persistError;
                            if (!manager.m_impl->store.upsertInboxItem(
                                    item, &persistError)) {
                                result = SstvShareProviderResult::failure(
                                    SstvShareProviderFailure::PermanentProviderFailure,
                                    persistError);
                                break;
                            }
                        }
                    }
                    if (completion) {
                        completion(result);
                    }
                    manager.m_impl->driveAgain();
                });
        });
    m_impl->rememberOperation(key, QStringLiteral("inbox"),
                              shareProvider, operation);
    if (operation == 0U && completion) {
        completion(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("provider did not start inbox refresh")));
    }
    return operation;
}

QString SstvShareQueueManager::queueDownload(
    const QString& providerId,
    const QString& incomingId,
    const QString& destinationRelativePath,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        fail(error, QStringLiteral("share queue manager is not initialized"));
        return {};
    }
    const auto shareProvider = m_impl->provider(providerId);
    auto item = m_impl->store.inboxItem(providerId, incomingId, error);
    if (!shareProvider || !shareProvider->capabilities().download || !item
        || item->disposition != SstvInboxDisposition::New
        || !item->transferId.isEmpty() || item->expiresUtc <= m_impl->now()) {
        fail(error, QStringLiteral("inbox item cannot be queued for download"));
        return {};
    }
    const auto parsed = parseSstvShareManifestV1(item->canonicalManifestJson);
    if (!parsed.ok()
        || !validateShareProviderCompatibility(
                *parsed.manifest, providerId,
                shareProvider->capabilities()).ok()) {
        fail(error, QStringLiteral("incoming manifest/provider compatibility failed"));
        return {};
    }
    const auto destination = secureDestinationPath(
        m_impl->store.config(), destinationRelativePath,
        parsed.manifest->mimeType, error);
    if (!destination) {
        return {};
    }
    SstvManagedTransferRecord record;
    record.transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.direction = SstvManagedTransferDirection::Download;
    record.state = SstvManagedTransferState::DownloadQueued;
    record.providerId = providerId;
    record.recipientId = parsed.manifest->recipientId;
    record.canonicalManifestJson = item->canonicalManifestJson;
    const QString validatedRoot = QDir(m_impl->store.config().downloadRoot)
        .absoluteFilePath(QStringLiteral("validated"));
    if (!ensurePlainDirectory(validatedRoot, error)) {
        return {};
    }
    record.destinationPath = QDir(validatedRoot).absoluteFilePath(
        record.transferId + QStringLiteral(".png"));
    if (QFileInfo::exists(record.destinationPath)
        || QFileInfo(record.destinationPath).isSymLink()
        || !pathWithin(QFileInfo(validatedRoot).canonicalFilePath(),
                       record.destinationPath, false)) {
        fail(error, QStringLiteral("validated download staging name is unsafe"));
        return {};
    }
    record.stagingPath = QDir(m_impl->store.config().downloadRoot)
        .absoluteFilePath(QStringLiteral(".partial/%1.part").arg(record.transferId));
    record.payloadSha256 = parsed.manifest->sha256;
    record.byteSize = parsed.manifest->byteSize;
    record.incomingId = incomingId;
    record.idempotencyKey =
        SstvShareTransfer::deriveIdempotencyKey(*parsed.manifest);
    record.createdUtc = m_impl->now();
    record.updatedUtc = record.createdUtc;
    item->disposition = SstvInboxDisposition::DownloadQueued;
    item->transferId = record.transferId;
    item->updatedUtc = notBefore(m_impl->now(), item->receivedUtc);
    if (!m_impl->store.insertDownloadTransferAndInbox(
            record, *item, error)) {
        return {};
    }
    return record.transferId;
}

bool SstvShareQueueManager::acceptDownload(const QString& transferId,
                                            QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        return fail(error, QStringLiteral("share queue manager is not initialized"));
    }
    auto record = m_impl->store.transfer(transferId, error);
    if (!record || record->direction != SstvManagedTransferDirection::Download
        || (record->state != SstvManagedTransferState::AwaitingAcceptance
            && record->state != SstvManagedTransferState::Accepted)) {
        return fail(error, QStringLiteral("download is not awaiting acceptance"));
    }
    const auto handoff = validatedIncomingHandoff(transferId, error);
    if (!handoff) {
        return fail(error, QStringLiteral(
            "download failed native validation before acceptance"));
    }
    if (m_impl->fault && m_impl->fault->shouldFail(
            SstvShareFaultPoint::BeforeDownloadAtomicCommit, transferId)) {
        return fail(error, QStringLiteral("atomic acceptance boundary failed"));
    }
    record->state = SstvManagedTransferState::Accepted;
    record->lastFailure = SstvShareProviderFailure::None;
    record->lastErrorRedacted.clear();
    record->nextRetryUtc = {};
    if (!m_impl->updateRecordAndInbox(
            *record, SstvInboxDisposition::Accepted, error)) {
        return fail(error, QStringLiteral("could not persist accepted download"));
    }
    // The raw remote bytes are quarantined only until explicit acceptance.
    // The metadata-free PNG named by the validated handoff remains private;
    // Gallery publication is intentionally owned by a later integration.
    if (QFileInfo::exists(record->stagingPath)) {
        QFile::remove(record->stagingPath);
    }
    return true;
}

SstvShareOperationId SstvShareQueueManager::acknowledgeDownloadAsync(
    const QString& transferId,
    SstvShareManagerCompletion completion)
{
    if (!m_impl->ownerThread(nullptr) || !m_impl->initialized) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("invalid acknowledgement request")));
        }
        return 0U;
    }
    QString error;
    auto record = m_impl->store.transfer(transferId, &error);
    const auto shareProvider = record ? m_impl->provider(record->providerId) : nullptr;
    if (!record || !shareProvider
        || record->direction != SstvManagedTransferDirection::Download
        || record->state != SstvManagedTransferState::Accepted) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("download is not accepted")));
        }
        return 0U;
    }
    record->state = SstvManagedTransferState::Acknowledging;
    record->attempts = 0U;
    record->nextRetryUtc = {};
    if (!m_impl->updateRecordAndInbox(
            *record, SstvInboxDisposition::Accepted, &error)) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure, error));
        }
        return 0U;
    }
    if (!shareProvider->capabilities().acknowledgement) {
        record->state = SstvManagedTransferState::Acknowledged;
        m_impl->updateRecordAndInbox(*record,
                                     SstvInboxDisposition::Acknowledged);
        if (completion) {
            completion(SstvShareProviderResult::success());
        }
        return 0U;
    }
    if (completion) {
        m_impl->completions.insert(transferId, std::move(completion));
    }
    processDue();
    const auto active = m_impl->active.constFind(transferId);
    return active == m_impl->active.constEnd() ? 0U : active->operationId;
}

SstvShareOperationId SstvShareQueueManager::rejectIncomingAsync(
    const QString& providerId,
    const QString& incomingId,
    SstvShareManagerCompletion completion)
{
    if (!m_impl->ownerThread(nullptr) || !m_impl->initialized) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("invalid rejection request")));
        }
        return 0U;
    }
    const auto shareProvider = m_impl->provider(providerId);
    QString error;
    auto item = m_impl->store.inboxItem(providerId, incomingId, &error);
    if (!shareProvider || !item
        || item->disposition == SstvInboxDisposition::Accepted
        || item->disposition == SstvInboxDisposition::Acknowledged
        || item->disposition == SstvInboxDisposition::Cancelled
        || item->disposition == SstvInboxDisposition::Rejected
        || item->disposition == SstvInboxDisposition::BlockedLocally
        || item->disposition == SstvInboxDisposition::BlockedByProvider
        || item->disposition == SstvInboxDisposition::ProviderDeleted) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("inbox item cannot be rejected")));
        }
        return 0U;
    }
    std::optional<SstvManagedTransferRecord> existing;
    if (!item->transferId.isEmpty()) {
        existing = m_impl->store.transfer(item->transferId, &error);
    }
    SstvManagedTransferRecord record;
    const bool isNewTransfer = !existing.has_value();
    if (existing) {
        record = std::move(*existing);
        if (record.state == SstvManagedTransferState::Accepted
            || record.state == SstvManagedTransferState::Acknowledged) {
            if (completion) {
                completion(SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Conflict,
                    QStringLiteral("accepted media cannot be rejected")));
            }
            return 0U;
        }
        const auto active = m_impl->active.find(record.transferId);
        if (active != m_impl->active.end()) {
            active->provider->cancelOperation(active->operationId);
            m_impl->active.erase(active);
        }
        record.state = SstvManagedTransferState::Rejecting;
        record.attempts = 0U;
        record.nextRetryUtc = {};
    } else {
        const auto manifest = parseSstvShareManifestV1(
            item->canonicalManifestJson);
        if (!manifest.ok()) {
            return 0U;
        }
        record.transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        record.direction = SstvManagedTransferDirection::Download;
        record.state = SstvManagedTransferState::Rejecting;
        record.providerId = providerId;
        record.recipientId = manifest.manifest->recipientId;
        record.canonicalManifestJson = item->canonicalManifestJson;
        record.payloadSha256 = manifest.manifest->sha256;
        record.byteSize = manifest.manifest->byteSize;
        record.incomingId = incomingId;
        record.idempotencyKey =
            SstvShareTransfer::deriveIdempotencyKey(*manifest.manifest);
        record.createdUtc = m_impl->now();
        record.updatedUtc = record.createdUtc;
    }
    item->transferId = record.transferId;
    item->updatedUtc = notBefore(m_impl->now(), item->receivedUtc);
    const bool persisted = isNewTransfer
        ? m_impl->store.insertDownloadTransferAndInbox(record, *item, &error)
        : m_impl->store.updateDownloadTransferAndInbox(record, *item, &error);
    if (!persisted) {
        return 0U;
    }
    if (!shareProvider->capabilities().rejection) {
        record.state = SstvManagedTransferState::Rejected;
        m_impl->updateRecordAndInbox(record, SstvInboxDisposition::Rejected);
        if (!record.stagingPath.isEmpty()) {
            QFile::remove(record.stagingPath);
        }
        if (!record.destinationPath.isEmpty()) {
            QFile::remove(record.destinationPath);
        }
        if (completion) {
            completion(SstvShareProviderResult::success());
        }
        return 0U;
    }
    if (completion) {
        m_impl->completions.insert(record.transferId, std::move(completion));
    }
    processDue();
    const auto active = m_impl->active.constFind(record.transferId);
    return active == m_impl->active.constEnd() ? 0U : active->operationId;
}

SstvShareOperationId SstvShareQueueManager::requestIncomingDeletionAsync(
    const QString& providerId,
    const QString& incomingId,
    SstvShareManagerCompletion completion)
{
    const auto reject = [&completion](SstvShareProviderFailure failure,
                                      const QString& diagnostic) {
        if (completion) {
            completion(SstvShareProviderResult::failure(failure, diagnostic));
        }
        return SstvShareOperationId {0U};
    };
    if (!m_impl->ownerThread(nullptr) || !m_impl->initialized
        || m_impl->destroying || !isSafeShareIdentifier(providerId)
        || !isSafeShareIdentifier(incomingId)) {
        return reject(SstvShareProviderFailure::Validation,
                      QStringLiteral("invalid incoming deletion request"));
    }
    QString error;
    auto item = m_impl->store.inboxItem(providerId, incomingId, &error);
    const auto shareProvider = m_impl->provider(providerId);
    if (!item || !shareProvider) {
        return reject(SstvShareProviderFailure::NotFound,
                      QStringLiteral("incoming item was not found"));
    }
    if (item->disposition == SstvInboxDisposition::ProviderDeleted) {
        if (completion) {
            completion(SstvShareProviderResult::success(
                {incomingId, 0U}));
        }
        return 0U;
    }
    const QString key = QStringLiteral("incoming-delete:") + providerId
        + QLatin1Char(':') + incomingId;
    if (!shareProvider->capabilities().incomingDelete
        || m_impl->active.contains(key)
        || !m_impl->hasCapacityFor(providerId)) {
        return reject(SstvShareProviderFailure::PermanentProviderFailure,
                      QStringLiteral(
                          "provider has no available verified incoming deletion capability"));
    }
    const QString expectedManifest = item->manifestSha256;
    auto callback = std::make_shared<SstvShareManagerCompletion>(
        std::move(completion));
    const QPointer<SstvShareQueueManager> guard(this);
    const SstvShareOperationId operation = shareProvider->deleteIncomingAsync(
        incomingId,
        [guard, key, providerId, incomingId, expectedManifest, callback](
            SstvShareProviderResult result) mutable {
            deliverToManager(guard,
                [key, providerId, incomingId, expectedManifest, callback,
                 result = std::move(result)](
                    SstvShareQueueManager& manager) mutable {
                    if (!manager.m_impl->claim(
                            key, QStringLiteral("incoming-delete"))) {
                        return;
                    }
                    if (result.ok()) {
                        QString persistError;
                        auto current = manager.m_impl->store.inboxItem(
                            providerId, incomingId, &persistError);
                        if (!current
                            || current->manifestSha256 != expectedManifest) {
                            result = SstvShareProviderResult::failure(
                                SstvShareProviderFailure::Conflict,
                                QStringLiteral("incoming deletion identity changed"));
                        } else {
                            current->disposition =
                                SstvInboxDisposition::ProviderDeleted;
                            current->updatedUtc = notBefore(
                                manager.m_impl->now(), current->receivedUtc);
                            if (!manager.m_impl->store.upsertInboxItem(
                                    *current, &persistError)) {
                                result = SstvShareProviderResult::failure(
                                    SstvShareProviderFailure::PermanentProviderFailure,
                                    persistError);
                            }
                        }
                    }
                    if (*callback) {
                        (*callback)(result);
                    }
                    manager.m_impl->driveAgain();
                });
        });
    m_impl->rememberOperation(key, QStringLiteral("incoming-delete"),
                              shareProvider, operation);
    if (operation == 0U && *callback) {
        (*callback)(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("provider did not start incoming deletion")));
    }
    return operation;
}

SstvShareOperationId SstvShareQueueManager::blockSenderAsync(
    const QString& providerId,
    const QString& incomingId,
    SstvSenderBlockScope scope,
    SstvShareManagerCompletion completion)
{
    const auto reject = [&completion](SstvShareProviderFailure failure,
                                      const QString& diagnostic) {
        if (completion) {
            completion(SstvShareProviderResult::failure(failure, diagnostic));
        }
        return SstvShareOperationId {0U};
    };
    if (!m_impl->ownerThread(nullptr) || !m_impl->initialized
        || m_impl->destroying || !isSafeShareIdentifier(providerId)
        || !isSafeShareIdentifier(incomingId)) {
        return reject(SstvShareProviderFailure::Validation,
                      QStringLiteral("invalid sender block request"));
    }
    QString error;
    const auto item = m_impl->store.inboxItem(providerId, incomingId, &error);
    const auto shareProvider = m_impl->provider(providerId);
    if (!item
        || (item->disposition != SstvInboxDisposition::New
            && item->disposition != SstvInboxDisposition::BlockedLocally
            && item->disposition != SstvInboxDisposition::BlockedByProvider)
        || item->expiresUtc <= m_impl->now()) {
        return reject(SstvShareProviderFailure::Conflict,
                      QStringLiteral("incoming sender cannot be blocked from this state"));
    }
    SstvSenderBlockRecord block;
    block.providerId = providerId;
    block.senderId = item->senderId;
    block.scope = scope;
    block.createdUtc = m_impl->now();
    const auto existingBlock = m_impl->store.senderBlock(
        providerId, item->senderId, &error);
    if (!error.isEmpty()) {
        return reject(SstvShareProviderFailure::PermanentProviderFailure,
                      error);
    }
    if (existingBlock
        && (existingBlock->scope == SstvSenderBlockScope::Provider
            || scope == SstvSenderBlockScope::LocalOnly)) {
        if (completion) {
            completion(SstvShareProviderResult::success());
        }
        return 0U;
    }
    if (scope == SstvSenderBlockScope::LocalOnly) {
        const bool persisted = m_impl->store.upsertSenderBlock(block, &error);
        if (completion) {
            completion(persisted
                ? SstvShareProviderResult::success()
                : SstvShareProviderResult::failure(
                    SstvShareProviderFailure::PermanentProviderFailure,
                    error));
        }
        return 0U;
    }
    const QString key = QStringLiteral("sender-block:") + providerId
        + QLatin1Char(':') + item->senderId;
    if (!shareProvider || !shareProvider->capabilities().senderBlocking
        || m_impl->active.contains(key)
        || !m_impl->hasCapacityFor(providerId)) {
        return reject(SstvShareProviderFailure::PermanentProviderFailure,
                      QStringLiteral(
                          "provider has no available verified sender blocking capability"));
    }
    auto callback = std::make_shared<SstvShareManagerCompletion>(
        std::move(completion));
    const QPointer<SstvShareQueueManager> guard(this);
    const SstvShareOperationId operation = shareProvider->blockSenderAsync(
        item->senderId,
        [guard, key, block, callback](SstvShareProviderResult result) mutable {
            deliverToManager(guard,
                [key, block, callback, result = std::move(result)](
                    SstvShareQueueManager& manager) mutable {
                    if (!manager.m_impl->claim(
                            key, QStringLiteral("sender-block"))) {
                        return;
                    }
                    if (result.ok()) {
                        QString persistError;
                        if (!manager.m_impl->store.upsertSenderBlock(
                                block, &persistError)) {
                            result = SstvShareProviderResult::failure(
                                SstvShareProviderFailure::PermanentProviderFailure,
                                persistError);
                        }
                    }
                    if (*callback) {
                        (*callback)(result);
                    }
                    manager.m_impl->driveAgain();
                });
        });
    m_impl->rememberOperation(key, QStringLiteral("sender-block"),
                              shareProvider, operation);
    if (operation == 0U && *callback) {
        (*callback)(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("provider did not start sender blocking")));
    }
    return operation;
}

bool SstvShareQueueManager::saveValidatedCopy(
    const QString& transferId,
    const QString& destinationPath,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized
        || !QFileInfo(destinationPath).isAbsolute()
        || destinationPath.size() > kMaximumStoredPathCharacters
        || !destinationPath.endsWith(QStringLiteral(".png"),
                                     Qt::CaseInsensitive)) {
        return fail(error, QStringLiteral("invalid validated export path"));
    }
    const QFileInfo destinationInfo(destinationPath);
    const QFileInfo parentInfo(destinationInfo.absolutePath());
    if (destinationInfo.exists() || destinationInfo.isSymLink()
        || !parentInfo.exists() || !parentInfo.isDir()
        || parentInfo.isSymLink() || parentInfo.canonicalFilePath().isEmpty()) {
        return fail(error, QStringLiteral(
            "validated export destination is not a new file in a plain directory"));
    }
    const auto handoff = validatedIncomingHandoff(transferId, error);
    if (!handoff) {
        return false;
    }
    QFile source(handoff->stagedCanonicalPath);
    QSaveFile destination(destinationPath);
    destination.setDirectWriteFallback(false);
    if (!source.open(QIODevice::ReadOnly)
        || !destination.open(QIODevice::WriteOnly)
        || !destination.setPermissions(QFileDevice::ReadOwner
                                       | QFileDevice::WriteOwner)) {
        destination.cancelWriting();
        return fail(error, QStringLiteral("could not open validated export"));
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    quint64 written = 0U;
    while (!source.atEnd()) {
        const QByteArray block = source.read(kFileHashReadBlockBytes);
        if ((block.isEmpty() && source.error() != QFileDevice::NoError)
            || written > handoff->stagedByteSize
            || static_cast<quint64>(block.size())
                > handoff->stagedByteSize - written
            || destination.write(block) != block.size()) {
            destination.cancelWriting();
            return fail(error, QStringLiteral("validated export write failed"));
        }
        written += static_cast<quint64>(block.size());
        hash.addData(block);
    }
    if (written != handoff->stagedByteSize
        || QString::fromLatin1(hash.result().toHex()) != handoff->stagedSha256
        || !destination.commit()) {
        destination.cancelWriting();
        return fail(error, QStringLiteral("validated export integrity failed"));
    }
    return true;
}

bool SstvShareQueueManager::deleteLocalCopy(
    const QString& transferId,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        return fail(error, QStringLiteral("share queue manager is not initialized"));
    }
    auto record = m_impl->store.transfer(transferId, error);
    if (!record || record->direction != SstvManagedTransferDirection::Download
        || (record->state != SstvManagedTransferState::Accepted
            && record->state != SstvManagedTransferState::Acknowledged)) {
        return fail(error, QStringLiteral(
            "only accepted local incoming copies can be deleted"));
    }
    if (record->destinationPath.isEmpty()) {
        return true;
    }
    if (QFileInfo::exists(record->destinationPath)) {
        const auto handoff = validatedIncomingHandoff(transferId, error);
        const QString canonicalDestination = QFileInfo(record->destinationPath)
            .canonicalFilePath();
        if (!handoff || canonicalDestination.isEmpty()
            || handoff->stagedCanonicalPath != canonicalDestination
            || !QFile::remove(record->destinationPath)
            || QFileInfo::exists(record->destinationPath)) {
            return fail(error, QStringLiteral("validated local copy was not deleted"));
        }
    }
    if (!record->stagingPath.isEmpty()) {
        QFile::remove(record->stagingPath);
    }
    record->destinationPath.clear();
    record->stagingPath.clear();
    const auto item = m_impl->store.inboxItem(
        record->providerId, record->incomingId, error);
    if (!item) {
        return false;
    }
    const SstvInboxDisposition disposition =
        item->disposition == SstvInboxDisposition::ProviderDeleted
        ? SstvInboxDisposition::ProviderDeleted
        : (record->state == SstvManagedTransferState::Acknowledged
            ? SstvInboxDisposition::Acknowledged
            : SstvInboxDisposition::Accepted);
    return m_impl->updateRecordAndInbox(*record, disposition, error);
}

SstvRemoteCopyAction SstvShareQueueManager::remoteCopyAction(
    const QString& transferId,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        fail(error, QStringLiteral("share queue manager is not initialized"));
        return SstvRemoteCopyAction::Unavailable;
    }
    const auto record = m_impl->store.transfer(transferId, error);
    if (!record) {
        fail(error, QStringLiteral("transfer was not found"));
        return SstvRemoteCopyAction::Unavailable;
    }
    const SstvRemoteCopyAction action = m_impl->remoteCopyActionFor(*record);
    if (action == SstvRemoteCopyAction::Unavailable) {
        fail(error, QStringLiteral(
            "completed remote copy has no executable provider action"));
    }
    return action;
}

SstvShareOperationId SstvShareQueueManager::removeRemoteCopyAsync(
    const QString& transferId,
    SstvShareManagerCompletion completion)
{
    const auto reject = [&completion](const QString& diagnostic) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation, diagnostic));
        }
        return SstvShareOperationId {0U};
    };
    if (!m_impl->ownerThread(nullptr) || !m_impl->initialized
        || m_impl->destroying) {
        return reject(QStringLiteral("invalid remote copy removal request"));
    }
    QString error;
    const auto record = m_impl->store.transfer(transferId, &error);
    if (!record) {
        return reject(QStringLiteral("completed upload was not found"));
    }
    const SstvRemoteCopyAction action = m_impl->remoteCopyActionFor(*record);
    const auto shareProvider = m_impl->provider(record->providerId);
    if (action == SstvRemoteCopyAction::Unavailable || !shareProvider) {
        return reject(QStringLiteral(
            "provider has no verified remote copy removal capability"));
    }
    if (completion) {
        m_impl->completions.insert(transferId, std::move(completion));
    }
    const SstvShareOperationId operation =
        m_impl->dispatchRemoteCopyRemoval(*record, action, shareProvider);
    if (operation == 0U) {
        const SstvShareProviderResult result = SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("provider did not start remote copy removal"));
        m_impl->completeRemoteCopyOutcome(
            transferId, record->revision, action, result);
    }
    return operation;
}

bool SstvShareQueueManager::cancelRemoteCopyRemoval(
    const QString& transferId,
    QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        return fail(error, QStringLiteral("share queue manager is not initialized"));
    }
    const auto running = m_impl->active.find(transferId);
    if (running == m_impl->active.end()
        || !running->stage.startsWith(QStringLiteral("remote-copy-"))) {
        return fail(error, QStringLiteral("remote copy removal is not active"));
    }
    const SstvShareQueueManager::Impl::ActiveOperation operation = *running;
    m_impl->active.erase(running);
    if (!operation.provider
        || !operation.provider->cancelOperation(operation.operationId)) {
        m_impl->active.insert(transferId, operation);
        return fail(error, QStringLiteral(
            "provider could not cancel remote copy removal"));
    }
    m_impl->completeAction(transferId, SstvShareProviderResult::failure(
        SstvShareProviderFailure::Cancelled,
        QStringLiteral("remote copy removal cancelled")));
    return true;
}

bool SstvShareQueueManager::pauseTransfer(const QString& transferId,
                                           QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        return fail(error, QStringLiteral("share queue manager is not initialized"));
    }
    auto record = m_impl->store.transfer(transferId, error);
    if (!record) {
        return fail(error, QStringLiteral("transfer was not found"));
    }
    if (record->state == SstvManagedTransferState::Paused) {
        return true;
    }
    if (record->cancelRequested || isTerminalManagedTransferState(record->state)) {
        return fail(error, QStringLiteral("transfer cannot be paused in its current state"));
    }
    if (record->direction == SstvManagedTransferDirection::Download
        && record->state != SstvManagedTransferState::DownloadQueued
        && record->state != SstvManagedTransferState::Downloading
        && record->state != SstvManagedTransferState::RetryScheduled) {
        return fail(error, QStringLiteral(
            "only a queued, downloading, or retrying download can be paused"));
    }

    // Remove the claim before asking the provider to cancel so even a
    // synchronous completion is stale and cannot overwrite the durable pause.
    const auto running = m_impl->active.find(transferId);
    if (running != m_impl->active.end()) {
        const auto provider = running->provider;
        const SstvShareOperationId operationId = running->operationId;
        m_impl->active.erase(running);
        if (provider && operationId != 0U) {
            provider->cancelOperation(operationId);
        }
    }

    if (record->direction == SstvManagedTransferDirection::Upload) {
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, m_impl->now(), false);
        if (!restored.ok()) {
            return fail(error, QStringLiteral("could not restore upload state"));
        }
        SstvShareTransfer transfer = *restored.transfer;
        const SstvShareTransferState previous = transfer.snapshot().state;
        if (!transfer.pause(m_impl->now())) {
            // pause() may have observed expiry; persist that terminal state so
            // a failed pause can never leave an expired transfer apparently live.
            if (transfer.snapshot().state == SstvShareTransferState::Expired) {
                m_impl->saveUpload(*record, transfer, nullptr);
            }
            return fail(error, QStringLiteral("upload cannot be paused in its current state"));
        }
        if (previous == SstvShareTransferState::Uploading
            && !record->providerSessionId.isEmpty()) {
            const auto provider = m_impl->provider(record->providerId);
            if (provider && provider->capabilities().resumableUpload) {
                m_impl->uploadResumeNeeded.insert(record->transferId);
            }
        }
        return m_impl->saveUpload(*record, transfer, error);
    }

    if (!m_impl->reconcileStaging(*record)) {
        return fail(error, QStringLiteral("download checkpoint is unsafe"));
    }
    record->state = SstvManagedTransferState::Paused;
    record->nextRetryUtc = {};
    return m_impl->updateRecordAndInbox(
        *record, SstvInboxDisposition::DownloadQueued, error);
}

bool SstvShareQueueManager::resumeTransfer(const QString& transferId,
                                            QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        return fail(error, QStringLiteral("share queue manager is not initialized"));
    }
    auto record = m_impl->store.transfer(transferId, error);
    if (!record) {
        return fail(error, QStringLiteral("transfer was not found"));
    }
    if (record->cancelRequested) {
        return fail(error, QStringLiteral("a cancelled transfer cannot be resumed"));
    }
    if (record->state != SstvManagedTransferState::Paused) {
        return fail(error, QStringLiteral("only a paused transfer can be resumed"));
    }

    bool persisted = false;
    if (record->direction == SstvManagedTransferDirection::Upload) {
        const auto restored = restoreSstvShareTransfer(
            record->transferPersistenceJson, m_impl->now(), false);
        if (!restored.ok()) {
            return fail(error, QStringLiteral("could not restore paused upload state"));
        }
        SstvShareTransfer transfer = *restored.transfer;
        const SstvShareTransferState target =
            transfer.snapshot().pausedResumeState;
        if (!transfer.resume(m_impl->now())) {
            if (transfer.snapshot().state == SstvShareTransferState::Expired) {
                m_impl->saveUpload(*record, transfer, nullptr);
            }
            return fail(error, QStringLiteral("paused upload has expired or is invalid"));
        }
        if (target == SstvShareTransferState::Uploading
            && !record->providerSessionId.isEmpty()) {
            const auto provider = m_impl->provider(record->providerId);
            if (provider && provider->capabilities().resumableUpload) {
                m_impl->uploadResumeNeeded.insert(record->transferId);
            }
        }
        persisted = m_impl->saveUpload(*record, transfer, error);
    } else {
        if (!m_impl->reconcileStaging(*record)) {
            return fail(error, QStringLiteral("download checkpoint is unsafe"));
        }
        record->state = SstvManagedTransferState::DownloadQueued;
        record->nextRetryUtc = {};
        persisted = m_impl->updateRecordAndInbox(
            *record, SstvInboxDisposition::DownloadQueued, error);
    }
    if (!persisted) {
        return false;
    }
    m_impl->driveAgain();
    return true;
}

bool SstvShareQueueManager::cancelTransfer(const QString& transferId,
                                            QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        return fail(error, QStringLiteral("share queue manager is not initialized"));
    }
    auto record = m_impl->store.transfer(transferId, error);
    if (!record) {
        return fail(error, QStringLiteral("transfer was not found"));
    }
    if (record->state == SstvManagedTransferState::Cancelled) {
        return true;
    }
    if (record->cancelRequested) {
        return true;
    }
    if (isTerminalManagedTransferState(record->state)) {
        return fail(error, QStringLiteral("terminal transfer cannot be cancelled"));
    }
    const auto running = m_impl->active.find(transferId);
    if (running != m_impl->active.end()) {
        running->provider->cancelOperation(running->operationId);
        m_impl->active.erase(running);
    }
    record->cancelRequested = true;
    bool persisted = false;
    if (record->direction == SstvManagedTransferDirection::Download) {
        const auto item = m_impl->store.inboxItem(
            record->providerId, record->incomingId, error);
        persisted = item && m_impl->updateRecordAndInbox(
            *record, item->disposition, error);
    } else {
        persisted = m_impl->updateRecord(*record, error);
    }
    if (!persisted) {
        return false;
    }
    processDue(error);
    return true;
}

qsizetype SstvShareQueueManager::processDue(QString* error)
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized
        || m_impl->destroying || m_impl->pumping) {
        return 0;
    }
    m_impl->pumping = true;
    qsizetype started = 0;
    const auto records = m_impl->store.queryTransfers(
        SstvShareTransferView::Active,
        m_impl->store.config().limits.maximumRecords, error);
    for (SstvManagedTransferRecord record : records) {
        if (m_impl->active.size()
            >= m_impl->store.config().limits.maximumConcurrentTransfers) {
            break;
        }
        if (m_impl->active.contains(record.transferId)) {
            continue;
        }
        const auto shareProvider = m_impl->provider(record.providerId);
        if (!shareProvider || !m_impl->hasCapacityFor(record.providerId)) {
            continue;
        }
        if (record.cancelRequested) {
            if (record.direction == SstvManagedTransferDirection::Upload) {
                if (m_impl->dispatchRemoteCancel(record, shareProvider)) {
                    ++started;
                }
            } else {
                record.state = SstvManagedTransferState::Cancelled;
                record.lastFailure = SstvShareProviderFailure::Cancelled;
                record.lastErrorRedacted = QStringLiteral("transfer cancelled");
                record.nextRetryUtc = {};
                m_impl->updateRecordAndInbox(
                    record, SstvInboxDisposition::Cancelled);
                if (!record.stagingPath.isEmpty()) {
                    QFile::remove(record.stagingPath);
                }
                if (!record.destinationPath.isEmpty()) {
                    QFile::remove(record.destinationPath);
                }
            }
            continue;
        }
        if (record.direction == SstvManagedTransferDirection::Upload) {
            if (!m_impl->uploadNetworkPolicyAllows(record)) {
                continue;
            }
            if (m_impl->dispatchUpload(record, shareProvider)) {
                ++started;
            }
            continue;
        }
        if (record.state != SstvManagedTransferState::Accepted
            && record.state != SstvManagedTransferState::Acknowledging
            && record.state != SstvManagedTransferState::Rejecting
            && record.state != SstvManagedTransferState::AwaitingAcceptance
            && record.canonicalManifestJson.size() > 0) {
            const auto manifest = parseSstvShareManifestV1(
                record.canonicalManifestJson);
            if (manifest.ok() && manifest.manifest->expiresUtc <= m_impl->now()) {
                record.state = SstvManagedTransferState::Expired;
                record.lastErrorRedacted = QStringLiteral("incoming transfer expired");
                m_impl->updateRecordAndInbox(
                    record, SstvInboxDisposition::Expired);
                continue;
            }
        }
        bool operationStarted = false;
        if (record.state == SstvManagedTransferState::DownloadQueued
            || record.state == SstvManagedTransferState::Downloading
            || record.state == SstvManagedTransferState::RetryScheduled) {
            operationStarted = m_impl->dispatchDownload(record, shareProvider);
        } else if (record.state == SstvManagedTransferState::Acknowledging
                   && shareProvider->capabilities().acknowledgement) {
            operationStarted = m_impl->dispatchAcknowledge(record, shareProvider);
        } else if (record.state == SstvManagedTransferState::Rejecting
                   && shareProvider->capabilities().rejection) {
            operationStarted = m_impl->dispatchReject(record, shareProvider);
        }
        if (operationStarted) {
            ++started;
        }
    }
    m_impl->pumping = false;
    return started;
}

qsizetype SstvShareQueueManager::activeOperationCount() const noexcept
{
    return m_impl->active.size();
}

QVector<SstvManagedTransferRecord> SstvShareQueueManager::activeTransfers(
    qsizetype maximumItems,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized
        || maximumItems <= 0
        || maximumItems > m_impl->store.config().limits.maximumQueryItems) {
        fail(error, QStringLiteral("invalid active transfer query"));
        return {};
    }
    return m_impl->store.queryTransfers(
        SstvShareTransferView::Active, maximumItems, error);
}

QVector<SstvManagedTransferRecord> SstvShareQueueManager::transferHistory(
    qsizetype maximumItems,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized
        || maximumItems <= 0
        || maximumItems > m_impl->store.config().limits.maximumQueryItems) {
        fail(error, QStringLiteral("invalid transfer history query"));
        return {};
    }
    return m_impl->store.queryTransfers(
        SstvShareTransferView::History, maximumItems, error);
}

QVector<SstvPersistentInboxItem> SstvShareQueueManager::inbox(
    qsizetype maximumItems,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized
        || maximumItems <= 0
        || maximumItems > m_impl->store.config().limits.maximumQueryItems) {
        fail(error, QStringLiteral("invalid inbox query"));
        return {};
    }
    return m_impl->store.queryInbox(maximumItems, error);
}

std::optional<SstvValidatedIncomingHandoff>
SstvShareQueueManager::validatedIncomingHandoff(
    const QString& transferId,
    QString* error) const
{
    if (!m_impl->ownerThread(error) || !m_impl->initialized
        || !isCanonicalUuid(transferId)) {
        fail(error, QStringLiteral("invalid validated handoff query"));
        return {};
    }
    const auto record = m_impl->store.transfer(transferId, error);
    if (!record
        || record->direction != SstvManagedTransferDirection::Download
        || (record->state != SstvManagedTransferState::AwaitingAcceptance
            && record->state != SstvManagedTransferState::Accepted
            && record->state != SstvManagedTransferState::Acknowledging
            && record->state != SstvManagedTransferState::Acknowledged)) {
        fail(error, QStringLiteral("download has no validated handoff"));
        return {};
    }
    const auto manifest = parseSstvShareManifestV1(
        record->canonicalManifestJson);
    const auto item = m_impl->store.inboxItem(
        record->providerId, record->incomingId, error);
    if (!manifest.ok() || !item) {
        fail(error, QStringLiteral("validated handoff metadata is unavailable"));
        return {};
    }
    const QString validatedRoot = QDir(m_impl->store.config().downloadRoot)
        .absoluteFilePath(QStringLiteral("validated"));
    const auto validation = inspectStagedIncomingMedia(
        record->destinationPath, validatedRoot, record->transferId,
        record->incomingId, *manifest.manifest, item->receivedUtc,
        item->expiresUtc);
    if (!validation.ok()) {
        fail(error, validation.redactedDiagnostic);
        return {};
    }
    return validation.handoff;
}

SstvShareQueueDiagnostics SstvShareQueueManager::diagnostics(
    QString* error) const
{
    SstvShareQueueDiagnostics output;
    if (!m_impl->ownerThread(error) || !m_impl->initialized) {
        fail(error, QStringLiteral("share queue manager is not initialized"));
        return output;
    }
    QString localError;
    QString* queryError = error ? error : &localError;
    const auto active = m_impl->store.queryTransfers(
        SstvShareTransferView::Active,
        m_impl->store.config().limits.maximumRecords, queryError);
    if (!queryError->isEmpty()) {
        return {};
    }
    output.activeQueueDepth = active.size();
    for (const auto& record : active) {
        if (record.direction == SstvManagedTransferDirection::Upload) {
            ++output.uploadQueueDepth;
        } else {
            ++output.downloadQueueDepth;
        }
    }
    output.uploadedBytes = m_impl->uploadedBytes;
    output.downloadedBytes = m_impl->downloadedBytes;
    output.reclaimedRows = m_impl->store.reclaimedRows();
    output.resetUtc = m_impl->diagnosticsResetUtc;
    const qint64 elapsedMs = std::max<qint64>(
        1, m_impl->diagnosticsResetUtc.msecsTo(m_impl->now()));
    const auto rate = [elapsedMs](quint64 bytes) {
        const long double perSecond = static_cast<long double>(bytes)
            * 1000.0L / static_cast<long double>(elapsedMs);
        return static_cast<quint64>(std::min<long double>(
            static_cast<long double>(kMaximumDiagnosticsCounter),
            std::max<long double>(0.0L, perSecond)));
    };
    output.uploadBytesPerSecond = rate(output.uploadedBytes);
    output.downloadBytesPerSecond = rate(output.downloadedBytes);
    return output;
}

void SstvShareQueueManager::resetDiagnostics()
{
    if (!m_impl->ownerThread(nullptr)) {
        return;
    }
    m_impl->uploadedBytes = 0U;
    m_impl->downloadedBytes = 0U;
    m_impl->store.resetReclaimedRows();
    m_impl->diagnosticsResetUtc = m_impl->now();
}

const SstvShareQueueStore& SstvShareQueueManager::store() const noexcept
{
    return m_impl->store;
}

} // namespace decodium::sstv::sharing
