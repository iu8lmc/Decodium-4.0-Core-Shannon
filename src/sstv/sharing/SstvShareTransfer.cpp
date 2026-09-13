// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvShareTransfer.h"

#include <QCryptographicHash>
#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace decodium::sstv::sharing {
namespace {

inline constexpr int kShareTransferPersistenceVersion = 1;

QString utcTimestamp(const QDateTime& value)
{
    return value.isValid()
        ? value.toUTC().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"))
        : QString {};
}

std::optional<QDateTime> parseOptionalUtcTimestamp(const QString& text)
{
    if (text.isEmpty()) {
        return QDateTime {};
    }
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid() || parsed.offsetFromUtc() != 0) {
        return std::nullopt;
    }
    parsed = parsed.toUTC();
    if (utcTimestamp(parsed) != text) {
        return std::nullopt;
    }
    return parsed;
}

SstvShareValidationError exactKeys(const QJsonObject& object,
                                   const QSet<QString>& expected,
                                   const QString& prefix = {})
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!expected.contains(it.key())) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::UnknownField, prefix + it.key());
        }
    }
    for (const QString& key : expected) {
        if (!object.contains(key)) {
            return SstvShareValidationError::failure(
                SstvShareValidationCode::MissingField, prefix + key);
        }
    }
    return {};
}

std::optional<quint64> unsignedInteger(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    constexpr double maximumExactJsonInteger = 9'007'199'254'740'991.0;
    if (!std::isfinite(number) || number < 0.0 || std::trunc(number) != number
        || number > maximumExactJsonInteger) {
        return std::nullopt;
    }
    return static_cast<quint64>(number);
}

bool parseTransferState(const QString& text, SstvShareTransferState& output)
{
    for (SstvShareTransferState state : {
             SstvShareTransferState::Draft,
             SstvShareTransferState::Queued,
             SstvShareTransferState::Preparing,
             SstvShareTransferState::Encrypting,
             SstvShareTransferState::Uploading,
             SstvShareTransferState::WaitingForAcknowledgement,
             SstvShareTransferState::Completed,
             SstvShareTransferState::RetryScheduled,
             SstvShareTransferState::Paused,
             SstvShareTransferState::Cancelled,
             SstvShareTransferState::Rejected,
             SstvShareTransferState::Expired,
             SstvShareTransferState::Failed}) {
        if (sstvShareTransferStateName(state) == text) {
            output = state;
            return true;
        }
    }
    return false;
}

bool parseProviderFailure(const QString& text, SstvShareProviderFailure& output)
{
    for (SstvShareProviderFailure failure : {
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
        if (sstvShareProviderFailureName(failure) == text) {
            output = failure;
            return true;
        }
    }
    return false;
}

bool resumableOperationalState(SstvShareTransferState state) noexcept
{
    return state == SstvShareTransferState::Queued
        || state == SstvShareTransferState::Preparing
        || state == SstvShareTransferState::Encrypting
        || state == SstvShareTransferState::Uploading
        || state == SstvShareTransferState::WaitingForAcknowledgement;
}

} // namespace

QString sstvShareTransferStateName(SstvShareTransferState state)
{
    switch (state) {
    case SstvShareTransferState::Draft: return QStringLiteral("Draft");
    case SstvShareTransferState::Queued: return QStringLiteral("Queued");
    case SstvShareTransferState::Preparing: return QStringLiteral("Preparing");
    case SstvShareTransferState::Encrypting: return QStringLiteral("Encrypting");
    case SstvShareTransferState::Uploading: return QStringLiteral("Uploading");
    case SstvShareTransferState::WaitingForAcknowledgement:
        return QStringLiteral("WaitingForAcknowledgement");
    case SstvShareTransferState::Completed: return QStringLiteral("Completed");
    case SstvShareTransferState::RetryScheduled: return QStringLiteral("RetryScheduled");
    case SstvShareTransferState::Paused: return QStringLiteral("Paused");
    case SstvShareTransferState::Cancelled: return QStringLiteral("Cancelled");
    case SstvShareTransferState::Rejected: return QStringLiteral("Rejected");
    case SstvShareTransferState::Expired: return QStringLiteral("Expired");
    case SstvShareTransferState::Failed: return QStringLiteral("Failed");
    }
    return {};
}

bool isTerminalShareTransferState(SstvShareTransferState state) noexcept
{
    return state == SstvShareTransferState::Completed
        || state == SstvShareTransferState::Cancelled
        || state == SstvShareTransferState::Rejected
        || state == SstvShareTransferState::Expired
        || state == SstvShareTransferState::Failed;
}

SstvShareValidationError SstvShareRetryPolicy::validate() const
{
    if (baseDelayMs <= 0 || maximumDelayMs < baseDelayMs
        || maximumProviderRetryAfterMs < maximumDelayMs
        || maximumProviderRetryAfterMs > 7LL * 24LL * 60LL * 60LL * 1'000LL
        || maximumRetries > 20U || jitterPermille > 500U) {
        return SstvShareValidationError::failure(
            SstvShareValidationCode::InvalidRetryPolicy,
            QStringLiteral("retryPolicy"));
    }
    return {};
}

SstvShareTransfer::SstvShareTransfer(SstvShareManifestV1 manifest,
                                     SstvShareRetryPolicy retryPolicy)
    : m_manifest(std::move(manifest))
    , m_retryPolicy(retryPolicy)
{
    m_validationError = m_manifest.validate(false);
    if (m_validationError.ok()) {
        m_validationError = m_retryPolicy.validate();
    }
    if (m_validationError.ok()) {
        m_snapshot.idempotencyKey = deriveIdempotencyKey(m_manifest);
        if (!isLowercaseSha256(m_snapshot.idempotencyKey)) {
            m_validationError = SstvShareValidationError::failure(
                SstvShareValidationCode::InvalidIdempotencyKey,
                QStringLiteral("idempotencyKey"));
        }
    }
}

bool SstvShareTransfer::canTransition(SstvShareTransferState from,
                                      SstvShareTransferState to) noexcept
{
    if (from == to) {
        return true;
    }
    if (isTerminalShareTransferState(from)) {
        return false;
    }
    if (to == SstvShareTransferState::Cancelled
        || to == SstvShareTransferState::Expired
        || to == SstvShareTransferState::Failed) {
        return true;
    }
    switch (from) {
    case SstvShareTransferState::Draft:
        return to == SstvShareTransferState::Queued;
    case SstvShareTransferState::Queued:
        return to == SstvShareTransferState::Preparing
            || to == SstvShareTransferState::RetryScheduled
            || to == SstvShareTransferState::Paused
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::Preparing:
        return to == SstvShareTransferState::Encrypting
            || to == SstvShareTransferState::Uploading
            || to == SstvShareTransferState::RetryScheduled
            || to == SstvShareTransferState::Paused
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::Encrypting:
        return to == SstvShareTransferState::Uploading
            || to == SstvShareTransferState::RetryScheduled
            || to == SstvShareTransferState::Paused
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::Uploading:
        return to == SstvShareTransferState::WaitingForAcknowledgement
            || to == SstvShareTransferState::RetryScheduled
            || to == SstvShareTransferState::Paused
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::WaitingForAcknowledgement:
        return to == SstvShareTransferState::Completed
            || to == SstvShareTransferState::RetryScheduled
            || to == SstvShareTransferState::Paused
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::RetryScheduled:
        return resumableOperationalState(to)
            || to == SstvShareTransferState::Paused
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::Paused:
        return resumableOperationalState(to)
            || to == SstvShareTransferState::RetryScheduled
            || to == SstvShareTransferState::Rejected;
    case SstvShareTransferState::Completed:
    case SstvShareTransferState::Cancelled:
    case SstvShareTransferState::Rejected:
    case SstvShareTransferState::Expired:
    case SstvShareTransferState::Failed:
        return false;
    }
    return false;
}

QString SstvShareTransfer::deriveIdempotencyKey(
    const SstvShareManifestV1& manifest)
{
    SstvShareValidationError error;
    const QByteArray canonical = manifest.toCanonicalJson(&error);
    if (!error.ok() || canonical.isEmpty()) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    static constexpr char prefix[] =
        "decodium-sstv-share-idempotency-v1";
    hash.addData(QByteArrayView(prefix, sizeof(prefix)));
    hash.addData(canonical);
    return QString::fromLatin1(hash.result().toHex());
}

bool SstvShareTransfer::transitionTo(SstvShareTransferState next)
{
    if (!isValid() || !canTransition(m_snapshot.state, next)) {
        return false;
    }
    m_snapshot.state = next;
    if (isTerminalShareTransferState(next)) {
        m_snapshot.retryAtUtc = {};
    }
    return true;
}

bool SstvShareTransfer::expireIfNeeded(QDateTime nowUtc)
{
    if (!isValid() || isTerminalShareTransferState(m_snapshot.state)
        || !nowUtc.isValid()) {
        return false;
    }
    if (m_manifest.expiresUtc <= nowUtc.toUTC()) {
        return transitionTo(SstvShareTransferState::Expired);
    }
    return false;
}

bool SstvShareTransfer::activeAndUnexpired(QDateTime nowUtc)
{
    if (!isValid() || !nowUtc.isValid()
        || isTerminalShareTransferState(m_snapshot.state)) {
        return false;
    }
    if (expireIfNeeded(nowUtc)) {
        return false;
    }
    return true;
}

bool SstvShareTransfer::enqueue(QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)
        || m_snapshot.state != SstvShareTransferState::Draft
        || !m_manifest.validate(true, nowUtc).ok()) {
        return false;
    }
    return transitionTo(SstvShareTransferState::Queued);
}

bool SstvShareTransfer::beginPreparing(QDateTime nowUtc)
{
    return activeAndUnexpired(nowUtc)
        && m_snapshot.state == SstvShareTransferState::Queued
        && transitionTo(SstvShareTransferState::Preparing);
}

bool SstvShareTransfer::beginEncrypting(QDateTime nowUtc)
{
    return activeAndUnexpired(nowUtc)
        && m_snapshot.state == SstvShareTransferState::Preparing
        && m_manifest.encryption.mode == SstvShareEncryptionMode::EndToEnd
        && transitionTo(SstvShareTransferState::Encrypting);
}

bool SstvShareTransfer::bindProviderUpload(
    const QString& idempotencyKey,
    const QString& opaqueProviderUploadId)
{
    if (!isValid() || idempotencyKey != m_snapshot.idempotencyKey
        || !isSafeShareIdentifier(opaqueProviderUploadId)
        || (m_snapshot.state != SstvShareTransferState::Preparing
            && m_snapshot.state != SstvShareTransferState::Encrypting
            && m_snapshot.state != SstvShareTransferState::Uploading)) {
        return false;
    }
    if (!m_snapshot.providerUploadId.isEmpty()) {
        return m_snapshot.providerUploadId == opaqueProviderUploadId;
    }
    m_snapshot.providerUploadId = opaqueProviderUploadId;
    return true;
}

bool SstvShareTransfer::beginUploading(QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)) {
        return false;
    }
    const SstvShareTransferState required =
        m_manifest.encryption.mode == SstvShareEncryptionMode::EndToEnd
        ? SstvShareTransferState::Encrypting
        : SstvShareTransferState::Preparing;
    return m_snapshot.state == required
        && !m_snapshot.providerUploadId.isEmpty()
        && transitionTo(SstvShareTransferState::Uploading);
}

bool SstvShareTransfer::recordProgress(quint64 committedBytes,
                                       QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)
        || m_snapshot.state != SstvShareTransferState::Uploading
        || committedBytes < m_snapshot.bytesTransferred
        || committedBytes > m_manifest.byteSize) {
        return false;
    }
    m_snapshot.bytesTransferred = committedBytes;
    return true;
}

bool SstvShareTransfer::waitForAcknowledgement(QDateTime nowUtc)
{
    return activeAndUnexpired(nowUtc)
        && m_snapshot.state == SstvShareTransferState::Uploading
        && m_snapshot.bytesTransferred == m_manifest.byteSize
        && transitionTo(SstvShareTransferState::WaitingForAcknowledgement);
}

bool SstvShareTransfer::markCompleted(const QString& idempotencyKey,
                                      const QString& remoteObjectId,
                                      QDateTime nowUtc)
{
    if (!isValid() || idempotencyKey != m_snapshot.idempotencyKey
        || !isSafeShareIdentifier(remoteObjectId)) {
        return false;
    }
    if (m_snapshot.state == SstvShareTransferState::Completed) {
        return m_snapshot.remoteObjectId == remoteObjectId;
    }
    if (!activeAndUnexpired(nowUtc)
        || m_snapshot.state
               != SstvShareTransferState::WaitingForAcknowledgement
        || m_snapshot.bytesTransferred != m_manifest.byteSize
        || !transitionTo(SstvShareTransferState::Completed)) {
        return false;
    }
    m_snapshot.remoteObjectId = remoteObjectId;
    m_snapshot.lastFailure = SstvShareProviderFailure::None;
    return true;
}

bool SstvShareTransfer::pause(QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)
        || m_snapshot.state == SstvShareTransferState::Draft
        || m_snapshot.state == SstvShareTransferState::Paused) {
        return false;
    }
    m_snapshot.pausedResumeState = m_snapshot.state;
    return transitionTo(SstvShareTransferState::Paused);
}

bool SstvShareTransfer::resume(QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)
        || m_snapshot.state != SstvShareTransferState::Paused
        || !canTransition(m_snapshot.state, m_snapshot.pausedResumeState)) {
        return false;
    }
    return transitionTo(m_snapshot.pausedResumeState);
}

bool SstvShareTransfer::cancel()
{
    return isValid() && !isTerminalShareTransferState(m_snapshot.state)
        && transitionTo(SstvShareTransferState::Cancelled);
}

SstvShareTransferState SstvShareTransfer::retryTargetForCurrentState() const noexcept
{
    if (m_snapshot.state == SstvShareTransferState::RetryScheduled) {
        return m_snapshot.retryResumeState;
    }
    return resumableOperationalState(m_snapshot.state)
        ? m_snapshot.state
        : SstvShareTransferState::Queued;
}

qint64 SstvShareTransfer::deterministicRetryDelayMs(
    quint32 attempt,
    qint64 providerRetryAfterMs) const
{
    if (!isValid() || attempt == 0U) {
        return 0;
    }
    qint64 exponential = m_retryPolicy.baseDelayMs;
    for (quint32 index = 1U; index < attempt; ++index) {
        if (exponential >= m_retryPolicy.maximumDelayMs / 2) {
            exponential = m_retryPolicy.maximumDelayMs;
            break;
        }
        exponential *= 2;
    }
    exponential = std::min(exponential, m_retryPolicy.maximumDelayMs);
    const qint64 jitterMagnitude = static_cast<qint64>(
        (static_cast<quint64>(exponential)
         * m_retryPolicy.jitterPermille) / 1'000U);

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(m_snapshot.idempotencyKey.toLatin1());
    static constexpr char retryMarker[] = "\0retry\0";
    hash.addData(QByteArrayView(retryMarker, sizeof(retryMarker) - 1U));
    hash.addData(QByteArray::number(attempt));
    const QByteArray digest = hash.result();
    quint64 sample = 0U;
    for (int i = 0; i < 8; ++i) {
        sample = (sample << 8U) | static_cast<quint8>(digest.at(i));
    }
    const quint64 span = static_cast<quint64>(jitterMagnitude) * 2U + 1U;
    const qint64 jitter = span > 0U
        ? static_cast<qint64>(sample % span) - jitterMagnitude
        : 0;
    qint64 delay = std::clamp(exponential + jitter,
                              qint64 {0}, m_retryPolicy.maximumDelayMs);
    const qint64 boundedRetryAfter = std::clamp(
        providerRetryAfterMs, qint64 {0},
        m_retryPolicy.maximumProviderRetryAfterMs);
    delay = std::max(delay, boundedRetryAfter);
    return delay;
}

bool SstvShareTransfer::handleFailure(SstvShareProviderFailure failure,
                                      QDateTime nowUtc,
                                      qint64 providerRetryAfterMs)
{
    if (!activeAndUnexpired(nowUtc)
        || failure == SstvShareProviderFailure::None
        || m_snapshot.state == SstvShareTransferState::Draft
        || m_snapshot.state == SstvShareTransferState::Paused) {
        return false;
    }
    m_snapshot.lastFailure = failure;
    if (failure == SstvShareProviderFailure::Cancelled) {
        return transitionTo(SstvShareTransferState::Cancelled);
    }
    if (failure == SstvShareProviderFailure::RejectedRecipient) {
        return transitionTo(SstvShareTransferState::Rejected);
    }
    if (!isRetryableShareProviderFailure(failure)
        || m_snapshot.retryCount >= m_retryPolicy.maximumRetries) {
        return transitionTo(SstvShareTransferState::Failed);
    }

    m_snapshot.retryResumeState = retryTargetForCurrentState();
    ++m_snapshot.retryCount;
    const qint64 delay = deterministicRetryDelayMs(
        m_snapshot.retryCount, providerRetryAfterMs);
    m_snapshot.retryAtUtc = nowUtc.toUTC().addMSecs(delay);
    return transitionTo(SstvShareTransferState::RetryScheduled);
}

bool SstvShareTransfer::activateScheduledRetry(QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)
        || m_snapshot.state != SstvShareTransferState::RetryScheduled
        || !m_snapshot.retryAtUtc.isValid()
        || nowUtc.toUTC() < m_snapshot.retryAtUtc
        || !resumableOperationalState(m_snapshot.retryResumeState)) {
        return false;
    }
    const SstvShareTransferState resumeState = m_snapshot.retryResumeState;
    if (!transitionTo(resumeState)) {
        return false;
    }
    m_snapshot.retryAtUtc = {};
    return true;
}

bool SstvShareTransfer::recoverAfterRestart(QDateTime nowUtc)
{
    if (!activeAndUnexpired(nowUtc)) {
        return false;
    }
    if (m_snapshot.state != SstvShareTransferState::Preparing
        && m_snapshot.state != SstvShareTransferState::Encrypting
        && m_snapshot.state != SstvShareTransferState::Uploading
        && m_snapshot.state
               != SstvShareTransferState::WaitingForAcknowledgement) {
        return true;
    }
    m_snapshot.retryResumeState = m_snapshot.state;
    m_snapshot.retryAtUtc = nowUtc.toUTC();
    ++m_snapshot.restartRecoveries;
    return transitionTo(SstvShareTransferState::RetryScheduled);
}

QByteArray SstvShareTransfer::toPersistenceJson(
    SstvShareValidationError* error) const
{
    if (error) {
        *error = {};
    }
    if (!isValid()) {
        if (error) {
            *error = m_validationError;
        }
        return {};
    }
    const QJsonObject retryObject {
        {QStringLiteral("baseDelayMs"), m_retryPolicy.baseDelayMs},
        {QStringLiteral("jitterPermille"),
         static_cast<qint64>(m_retryPolicy.jitterPermille)},
        {QStringLiteral("maximumDelayMs"), m_retryPolicy.maximumDelayMs},
        {QStringLiteral("maximumProviderRetryAfterMs"),
         m_retryPolicy.maximumProviderRetryAfterMs},
        {QStringLiteral("maximumRetries"),
         static_cast<qint64>(m_retryPolicy.maximumRetries)},
    };
    const QJsonObject object {
        {QStringLiteral("bytesTransferred"),
         static_cast<qint64>(m_snapshot.bytesTransferred)},
        {QStringLiteral("idempotencyKey"), m_snapshot.idempotencyKey},
        {QStringLiteral("lastFailure"),
         sstvShareProviderFailureName(m_snapshot.lastFailure)},
        {QStringLiteral("manifest"), m_manifest.toJsonObject()},
        {QStringLiteral("pausedResumeState"),
         sstvShareTransferStateName(m_snapshot.pausedResumeState)},
        {QStringLiteral("persistenceVersion"),
         kShareTransferPersistenceVersion},
        {QStringLiteral("providerUploadId"), m_snapshot.providerUploadId},
        {QStringLiteral("remoteObjectId"), m_snapshot.remoteObjectId},
        {QStringLiteral("restartRecoveries"),
         static_cast<qint64>(m_snapshot.restartRecoveries)},
        {QStringLiteral("retryAtUtc"), utcTimestamp(m_snapshot.retryAtUtc)},
        {QStringLiteral("retryCount"),
         static_cast<qint64>(m_snapshot.retryCount)},
        {QStringLiteral("retryPolicy"), retryObject},
        {QStringLiteral("retryResumeState"),
         sstvShareTransferStateName(m_snapshot.retryResumeState)},
        {QStringLiteral("state"), sstvShareTransferStateName(m_snapshot.state)},
    };
    QByteArray encoded = canonicalJson(object, error);
    if (encoded.size() > kMaximumPersistenceJsonBytes) {
        if (error) {
            *error = SstvShareValidationError::failure(
                SstvShareValidationCode::JsonTooLarge);
        }
        return {};
    }
    return encoded;
}

SstvShareTransferRestoreResult restoreSstvShareTransfer(
    const QByteArray& json,
    QDateTime nowUtc,
    bool recoverInFlight)
{
    const auto bounded = parseBoundedJsonObject(
        json, kMaximumPersistenceJsonBytes, kMaximumJsonDepth,
        kMaximumJsonNodes);
    if (!bounded.ok()) {
        return {std::nullopt, bounded.error};
    }
    static const QSet<QString> keys {
        QStringLiteral("bytesTransferred"), QStringLiteral("idempotencyKey"),
        QStringLiteral("lastFailure"), QStringLiteral("manifest"),
        QStringLiteral("pausedResumeState"), QStringLiteral("persistenceVersion"),
        QStringLiteral("providerUploadId"), QStringLiteral("remoteObjectId"),
        QStringLiteral("restartRecoveries"),
        QStringLiteral("retryAtUtc"), QStringLiteral("retryCount"),
        QStringLiteral("retryPolicy"), QStringLiteral("retryResumeState"),
        QStringLiteral("state"),
    };
    if (const auto keyError = exactKeys(bounded.object, keys); !keyError.ok()) {
        return {std::nullopt, keyError};
    }
    const auto persistenceVersion = unsignedInteger(
        bounded.object.value(QStringLiteral("persistenceVersion")));
    if (!persistenceVersion
        || *persistenceVersion != kShareTransferPersistenceVersion) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::UnknownProtocolVersion,
                                  QStringLiteral("persistenceVersion"))};
    }
    if (!bounded.object.value(QStringLiteral("manifest")).isObject()
        || !bounded.object.value(QStringLiteral("retryPolicy")).isObject()) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::WrongType,
                                  QStringLiteral("persistence-object"))};
    }
    SstvShareValidationError canonicalError;
    const QByteArray manifestJson = canonicalJson(
        bounded.object.value(QStringLiteral("manifest")), &canonicalError);
    if (!canonicalError.ok()) {
        return {std::nullopt, canonicalError};
    }
    const auto parsedManifest = parseSstvShareManifestV1(manifestJson);
    if (!parsedManifest.ok()) {
        return {std::nullopt, parsedManifest.error};
    }

    const QJsonObject retryObject =
        bounded.object.value(QStringLiteral("retryPolicy")).toObject();
    static const QSet<QString> retryKeys {
        QStringLiteral("baseDelayMs"), QStringLiteral("jitterPermille"),
        QStringLiteral("maximumDelayMs"),
        QStringLiteral("maximumProviderRetryAfterMs"),
        QStringLiteral("maximumRetries"),
    };
    if (const auto keyError = exactKeys(retryObject, retryKeys,
                                        QStringLiteral("retryPolicy."));
        !keyError.ok()) {
        return {std::nullopt, keyError};
    }
    const auto baseDelay = unsignedInteger(retryObject.value(QStringLiteral("baseDelayMs")));
    const auto jitter = unsignedInteger(retryObject.value(QStringLiteral("jitterPermille")));
    const auto maximumDelay = unsignedInteger(retryObject.value(QStringLiteral("maximumDelayMs")));
    const auto maximumRetryAfter = unsignedInteger(
        retryObject.value(QStringLiteral("maximumProviderRetryAfterMs")));
    const auto maximumRetries = unsignedInteger(
        retryObject.value(QStringLiteral("maximumRetries")));
    if (!baseDelay || !jitter || !maximumDelay || !maximumRetryAfter
        || !maximumRetries
        || *baseDelay > static_cast<quint64>(std::numeric_limits<qint64>::max())
        || *maximumDelay > static_cast<quint64>(std::numeric_limits<qint64>::max())
        || *maximumRetryAfter > static_cast<quint64>(std::numeric_limits<qint64>::max())
        || *jitter > std::numeric_limits<quint32>::max()
        || *maximumRetries > std::numeric_limits<quint32>::max()) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidRetryPolicy,
                                  QStringLiteral("retryPolicy"))};
    }
    SstvShareRetryPolicy retryPolicy;
    retryPolicy.baseDelayMs = static_cast<qint64>(*baseDelay);
    retryPolicy.maximumDelayMs = static_cast<qint64>(*maximumDelay);
    retryPolicy.maximumProviderRetryAfterMs =
        static_cast<qint64>(*maximumRetryAfter);
    retryPolicy.maximumRetries = static_cast<quint32>(*maximumRetries);
    retryPolicy.jitterPermille = static_cast<quint32>(*jitter);
    if (const auto policyError = retryPolicy.validate(); !policyError.ok()) {
        return {std::nullopt, policyError};
    }

    SstvShareTransfer transfer(*parsedManifest.manifest, retryPolicy);
    if (!transfer.isValid()) {
        return {std::nullopt, transfer.validationError()};
    }
    const auto bytes = unsignedInteger(
        bounded.object.value(QStringLiteral("bytesTransferred")));
    const auto retryCount = unsignedInteger(
        bounded.object.value(QStringLiteral("retryCount")));
    const auto restartRecoveries = unsignedInteger(
        bounded.object.value(QStringLiteral("restartRecoveries")));
    if (!bytes || !retryCount || !restartRecoveries
        || *bytes > transfer.m_manifest.byteSize
        || *retryCount > transfer.m_retryPolicy.maximumRetries
        || *restartRecoveries > std::numeric_limits<quint32>::max()) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidProgress,
                                  QStringLiteral("persistence-counters"))};
    }
    if (!bounded.object.value(QStringLiteral("idempotencyKey")).isString()
        || !bounded.object.value(QStringLiteral("providerUploadId")).isString()
        || !bounded.object.value(QStringLiteral("remoteObjectId")).isString()
        || !bounded.object.value(QStringLiteral("retryAtUtc")).isString()
        || !bounded.object.value(QStringLiteral("lastFailure")).isString()
        || !bounded.object.value(QStringLiteral("state")).isString()
        || !bounded.object.value(QStringLiteral("retryResumeState")).isString()
        || !bounded.object.value(QStringLiteral("pausedResumeState")).isString()) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::WrongType,
                                  QStringLiteral("persistence-scalars"))};
    }
    const QString persistedIdempotency =
        bounded.object.value(QStringLiteral("idempotencyKey")).toString();
    if (persistedIdempotency != transfer.m_snapshot.idempotencyKey) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidIdempotencyKey,
                                  QStringLiteral("idempotencyKey"))};
    }
    SstvShareTransferState state;
    SstvShareTransferState retryResume;
    SstvShareTransferState pausedResume;
    SstvShareProviderFailure lastFailure {SstvShareProviderFailure::None};
    if (!parseTransferState(bounded.object.value(QStringLiteral("state")).toString(), state)
        || !parseTransferState(
            bounded.object.value(QStringLiteral("retryResumeState")).toString(),
            retryResume)
        || !parseTransferState(
            bounded.object.value(QStringLiteral("pausedResumeState")).toString(),
            pausedResume)
        || !parseProviderFailure(
            bounded.object.value(QStringLiteral("lastFailure")).toString(),
            lastFailure)) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidState,
                                  QStringLiteral("state"))};
    }
    const auto retryAt = parseOptionalUtcTimestamp(
        bounded.object.value(QStringLiteral("retryAtUtc")).toString());
    if (!retryAt) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidTimestamp,
                                  QStringLiteral("retryAtUtc"))};
    }
    const QString remoteObjectId =
        bounded.object.value(QStringLiteral("remoteObjectId")).toString();
    const QString providerUploadId =
        bounded.object.value(QStringLiteral("providerUploadId")).toString();
    const auto effectiveProgressState = [&]() {
        if (state == SstvShareTransferState::RetryScheduled) {
            return retryResume;
        }
        if (state == SstvShareTransferState::Paused) {
            return pausedResume == SstvShareTransferState::RetryScheduled
                ? retryResume : pausedResume;
        }
        return state;
    }();
    const bool progressMustBeZero =
        effectiveProgressState == SstvShareTransferState::Draft
        || effectiveProgressState == SstvShareTransferState::Queued
        || effectiveProgressState == SstvShareTransferState::Preparing
        || effectiveProgressState == SstvShareTransferState::Encrypting;
    const bool progressMustBeComplete =
        effectiveProgressState
            == SstvShareTransferState::WaitingForAcknowledgement
        || state == SstvShareTransferState::Completed;
    const bool retryTimestampRequired =
        state == SstvShareTransferState::RetryScheduled
        || (state == SstvShareTransferState::Paused
            && pausedResume == SstvShareTransferState::RetryScheduled);
    const bool uploadHandleRequired =
        effectiveProgressState == SstvShareTransferState::Uploading
        || effectiveProgressState
            == SstvShareTransferState::WaitingForAcknowledgement
        || state == SstvShareTransferState::Completed;
    if ((!remoteObjectId.isEmpty() && !isSafeShareIdentifier(remoteObjectId))
        || (!providerUploadId.isEmpty()
            && !isSafeShareIdentifier(providerUploadId))
        || (uploadHandleRequired && providerUploadId.isEmpty())
        || (state == SstvShareTransferState::Completed
            && (remoteObjectId.isEmpty() || *bytes != transfer.m_manifest.byteSize))
        || (state != SstvShareTransferState::Completed
            && !remoteObjectId.isEmpty())
        || (state == SstvShareTransferState::RetryScheduled
            && (!retryAt->isValid() || !resumableOperationalState(retryResume)))
        || (state == SstvShareTransferState::Paused
            && !SstvShareTransfer::canTransition(
                SstvShareTransferState::Paused, pausedResume))
        || !resumableOperationalState(retryResume)
        || (progressMustBeZero && *bytes != 0U)
        || (progressMustBeComplete && *bytes != transfer.m_manifest.byteSize)
        || (retryTimestampRequired != retryAt->isValid())) {
        return {std::nullopt, SstvShareValidationError::failure(
                                  SstvShareValidationCode::InvalidState,
                                  QStringLiteral("state-invariants"))};
    }
    transfer.m_snapshot.state = state;
    transfer.m_snapshot.retryResumeState = retryResume;
    transfer.m_snapshot.pausedResumeState = pausedResume;
    transfer.m_snapshot.retryCount = static_cast<quint32>(*retryCount);
    transfer.m_snapshot.restartRecoveries = static_cast<quint32>(*restartRecoveries);
    transfer.m_snapshot.bytesTransferred = *bytes;
    transfer.m_snapshot.retryAtUtc = *retryAt;
    transfer.m_snapshot.providerUploadId = providerUploadId;
    transfer.m_snapshot.remoteObjectId = remoteObjectId;
    transfer.m_snapshot.lastFailure = lastFailure;

    if (recoverInFlight && !transfer.recoverAfterRestart(nowUtc)) {
        if (transfer.m_snapshot.state != SstvShareTransferState::Draft
            && transfer.m_snapshot.state != SstvShareTransferState::Queued
            && transfer.m_snapshot.state != SstvShareTransferState::RetryScheduled
            && transfer.m_snapshot.state != SstvShareTransferState::Paused
            && !isTerminalShareTransferState(transfer.m_snapshot.state)) {
            return {std::nullopt, SstvShareValidationError::failure(
                                      SstvShareValidationCode::InvalidState,
                                      QStringLiteral("restart-recovery"))};
        }
    }
    return {std::move(transfer), {}};
}

} // namespace decodium::sstv::sharing
