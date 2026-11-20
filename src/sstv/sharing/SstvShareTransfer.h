// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvShareProvider.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <cstdint>
#include <optional>

namespace decodium::sstv::sharing {

enum class SstvShareTransferState
{
    Draft,
    Queued,
    Preparing,
    Encrypting,
    Uploading,
    WaitingForAcknowledgement,
    Completed,
    RetryScheduled,
    Paused,
    Cancelled,
    Rejected,
    Expired,
    Failed,
};

QString sstvShareTransferStateName(SstvShareTransferState state);
bool isTerminalShareTransferState(SstvShareTransferState state) noexcept;

struct SstvShareRetryPolicy final
{
    qint64 baseDelayMs {1'000};
    qint64 maximumDelayMs {300'000};
    qint64 maximumProviderRetryAfterMs {86'400'000};
    quint32 maximumRetries {5U};
    // 200 means deterministic jitter in [-20%, +20%].
    quint32 jitterPermille {200U};

    SstvShareValidationError validate() const;
};

struct SstvShareTransferSnapshot final
{
    SstvShareTransferState state {SstvShareTransferState::Draft};
    SstvShareTransferState retryResumeState {SstvShareTransferState::Queued};
    SstvShareTransferState pausedResumeState {SstvShareTransferState::Queued};
    quint32 retryCount {0U};
    quint32 restartRecoveries {0U};
    quint64 bytesTransferred {0U};
    QDateTime retryAtUtc;
    QString idempotencyKey;
    QString providerUploadId;
    QString remoteObjectId;
    SstvShareProviderFailure lastFailure {SstvShareProviderFailure::None};
};

class SstvShareTransfer;
struct SstvShareTransferRestoreResult;
SstvShareTransferRestoreResult restoreSstvShareTransfer(
    const QByteArray& json,
    QDateTime nowUtc,
    bool recoverInFlight);

class SstvShareTransfer final
{
public:
    explicit SstvShareTransfer(SstvShareManifestV1 manifest,
                               SstvShareRetryPolicy retryPolicy = {});

    bool isValid() const noexcept { return m_validationError.ok(); }
    const SstvShareValidationError& validationError() const noexcept
    {
        return m_validationError;
    }
    const SstvShareManifestV1& manifest() const noexcept { return m_manifest; }
    const SstvShareRetryPolicy& retryPolicy() const noexcept { return m_retryPolicy; }
    const SstvShareTransferSnapshot& snapshot() const noexcept { return m_snapshot; }

    static bool canTransition(SstvShareTransferState from,
                              SstvShareTransferState to) noexcept;
    static QString deriveIdempotencyKey(const SstvShareManifestV1& manifest);

    bool enqueue(QDateTime nowUtc);
    bool beginPreparing(QDateTime nowUtc);
    bool beginEncrypting(QDateTime nowUtc);
    bool bindProviderUpload(const QString& idempotencyKey,
                            const QString& opaqueProviderUploadId);
    bool beginUploading(QDateTime nowUtc);
    bool recordProgress(quint64 committedBytes, QDateTime nowUtc);
    bool waitForAcknowledgement(QDateTime nowUtc);
    bool markCompleted(const QString& idempotencyKey,
                       const QString& remoteObjectId,
                       QDateTime nowUtc);
    bool pause(QDateTime nowUtc);
    bool resume(QDateTime nowUtc);
    bool cancel();
    bool expireIfNeeded(QDateTime nowUtc);

    // Returns true when the failure was consumed. Retryable failures enter
    // RetryScheduled until the bounded attempt limit; permanent categories
    // move directly to Failed/Rejected/Cancelled and are never auto-retried.
    bool handleFailure(SstvShareProviderFailure failure,
                       QDateTime nowUtc,
                       qint64 providerRetryAfterMs = 0);
    bool activateScheduledRetry(QDateTime nowUtc);
    qint64 deterministicRetryDelayMs(quint32 attempt,
                                     qint64 providerRetryAfterMs = 0) const;

    // Preparing/Encrypting/Uploading/Waiting states cannot safely keep running
    // after process loss. Recovery preserves progress and schedules an
    // immediate, idempotent resume without consuming a retry attempt.
    bool recoverAfterRestart(QDateTime nowUtc);

    QByteArray toPersistenceJson(SstvShareValidationError* error = nullptr) const;

private:
    bool transitionTo(SstvShareTransferState next);
    bool activeAndUnexpired(QDateTime nowUtc);
    SstvShareTransferState retryTargetForCurrentState() const noexcept;

    SstvShareManifestV1 m_manifest;
    SstvShareRetryPolicy m_retryPolicy;
    SstvShareTransferSnapshot m_snapshot;
    SstvShareValidationError m_validationError;

    friend SstvShareTransferRestoreResult restoreSstvShareTransfer(
        const QByteArray& json,
        QDateTime nowUtc,
        bool recoverInFlight);
};

struct SstvShareTransferRestoreResult final
{
    std::optional<SstvShareTransfer> transfer;
    SstvShareValidationError error;

    bool ok() const noexcept { return transfer.has_value() && error.ok(); }
};

SstvShareTransferRestoreResult restoreSstvShareTransfer(
    const QByteArray& json,
    QDateTime nowUtc,
    bool recoverInFlight = true);

} // namespace decodium::sstv::sharing
