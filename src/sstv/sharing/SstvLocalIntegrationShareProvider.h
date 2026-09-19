// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvShareProvider.h"

#include <QObject>
#include <QSet>

#include <functional>
#include <memory>

namespace decodium::sstv::sharing {

// Deterministic, bounded, process-local contract adapter. It exists for
// integration tests and developer harnesses when no peer/relay backend is
// available. It opens no socket, performs no filesystem I/O, stores no
// credentials, and is never selected by the production controller/UI.
class SstvLocalIntegrationShareProvider final : public QObject,
                                                public SstvShareProvider
{
public:
    struct Config final
    {
        QString localRecipientId {QStringLiteral("station:local")};
        QSet<QString> participantIds {
            QStringLiteral("station:local"),
            QStringLiteral("station:remote")};
        quint64 maximumChunkBytes {64U * 1024U};
        quint64 maximumTotalBytes {128U * 1024U * 1024U};
        qsizetype maximumSessions {128};
        qsizetype maximumObjects {128};
        qsizetype maximumPendingOperations {256};
        // Deterministic exhaustion seam for this developer/test-only adapter.
        SstvShareOperationId firstOperationId {1U};
        int completionDelayMs {0};
        std::function<QDateTime()> clock;
    };

    explicit SstvLocalIntegrationShareProvider(QObject* parent = nullptr);
    explicit SstvLocalIntegrationShareProvider(
        Config config, QObject* parent = nullptr);
    ~SstvLocalIntegrationShareProvider() override;

    bool isConfigurationValid() const noexcept;
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

private:
    struct State;
    SstvShareOperationId schedule(std::function<void()> callback);
    void reclaimExpired();
    QDateTime nowUtc() const;
    static SstvShareProviderResult invalid(const QString& reasonCode);

    Config m_config;
    std::unique_ptr<State> m_state;
    bool m_valid {false};
};

} // namespace decodium::sstv::sharing
