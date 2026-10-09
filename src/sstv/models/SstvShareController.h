// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <atomic>
#include <memory>

namespace secure_settings {
class Backend;
}

namespace decodium::sstv {

namespace sharing {
class SstvShareProvider;
}

class SstvShareWorker;

class SstvShareListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role : int
    {
        TransferIdRole = Qt::UserRole + 1,
        IncomingIdRole,
        DirectionRole,
        StateRole,
        ProviderIdRole,
        PeerIdRole,
        FileNameRole,
        ModeRole,
        ByteSizeRole,
        ByteOffsetRole,
        ProgressRole,
        ErrorRole,
        CreatedUtcRole,
        UpdatedUtcRole,
        ExpiresUtcRole,
        CanPauseRole,
        CanResumeRole,
        CanCancelRole,
        CanDownloadRole,
        CanAcceptRole,
        CanAcknowledgeRole,
        CanRejectRole,
        CanSaveAsRole,
        CanDeleteLocalCopyRole,
        CanRequestProviderDeletionRole,
        CanBlockSenderRole,
        BlockSenderScopeRole,
        MessageRole,
        Sha256Role,
        PrivacySummaryRole,
        CanRemoveRemoteCopyRole,
        RemoteCopyActionRole,
        PreviewSourceRole,
        ValidatedHandoffRole,
    };
    Q_ENUM(Role)

    explicit SstvShareListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replaceRows(QVariantList rows);

private:
    QVariantList m_rows;
};

// GUI-thread facade for the persistent sharing queue. Every operation that can
// touch SQLite, the keychain, the network or an image file is queued to the
// dedicated SstvShareWorker thread. Only bounded QVariant snapshots cross back
// to the QML-facing list models.
class SstvShareController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool secureStorageAvailable READ secureStorageAvailable
               NOTIFY stateChanged)
    Q_PROPERTY(bool providerSupportsInbox READ providerSupportsInbox
               NOTIFY stateChanged)
    Q_PROPERTY(bool providerSupportsIncomingDelete
               READ providerSupportsIncomingDelete NOTIFY stateChanged)
    Q_PROPERTY(bool providerSupportsSenderBlocking
               READ providerSupportsSenderBlocking NOTIFY stateChanged)
    Q_PROPERTY(bool preSignedAvailable READ preSignedAvailable CONSTANT)
    Q_PROPERTY(QString preSignedUnavailableReason
               READ preSignedUnavailableReason CONSTANT)
    Q_PROPERTY(bool peerRelayAvailable READ peerRelayAvailable CONSTANT)
    Q_PROPERTY(QString peerRelayUnavailableReason
               READ peerRelayUnavailableReason CONSTANT)
    Q_PROPERTY(QString meteredNetworkStatus READ meteredNetworkStatus
               NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap diagnostics READ diagnostics NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    Q_PROPERTY(QString storageFolder READ storageFolder NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap configuration READ configuration
               NOTIFY configurationChanged)
    Q_PROPERTY(QAbstractItemModel* activeTransfers READ activeTransfers CONSTANT)
    Q_PROPERTY(QAbstractItemModel* transferHistory READ transferHistory CONSTANT)
    Q_PROPERTY(QAbstractItemModel* inbox READ inbox CONSTANT)

public:
    explicit SstvShareController(
        const secure_settings::Backend* secureBackend,
        QObject* parent = nullptr);
    // Explicit developer/test injection only. The production constructor,
    // settings schema and QML provider picker cannot activate this provider.
    SstvShareController(
        const secure_settings::Backend* secureBackend,
        std::shared_ptr<sharing::SstvShareProvider> localIntegrationProvider,
        QObject* parent = nullptr);
    ~SstvShareController() override;

    SstvShareController(const SstvShareController&) = delete;
    SstvShareController& operator=(const SstvShareController&) = delete;

    bool ready() const noexcept { return m_ready; }
    bool enabled() const noexcept { return m_enabled; }
    bool configured() const noexcept { return m_configured; }
    bool busy() const noexcept { return m_busy; }
    bool secureStorageAvailable() const noexcept
    {
        return m_secureStorageAvailable;
    }
    bool providerSupportsInbox() const noexcept
    {
        return m_providerSupportsInbox;
    }
    bool providerSupportsIncomingDelete() const noexcept
    {
        return m_providerSupportsIncomingDelete;
    }
    bool providerSupportsSenderBlocking() const noexcept
    {
        return m_providerSupportsSenderBlocking;
    }
    bool preSignedAvailable() const noexcept { return false; }
    QString preSignedUnavailableReason() const;
    bool peerRelayAvailable() const noexcept { return false; }
    QString peerRelayUnavailableReason() const;
    QString meteredNetworkStatus() const { return m_meteredNetworkStatus; }
    QVariantMap diagnostics() const { return m_diagnostics; }
    QString statusText() const { return m_statusText; }
    QString errorString() const { return m_errorString; }
    QString storageFolder() const { return m_storageFolder; }
    Q_INVOKABLE QUrl localFileUrl(const QString& path) const
    {
        return path.isEmpty() ? QUrl {} : QUrl::fromLocalFile(path);
    }
    QVariantMap configuration() const { return m_configuration; }
    QAbstractItemModel* activeTransfers() noexcept { return &m_activeModel; }
    QAbstractItemModel* transferHistory() noexcept { return &m_historyModel; }
    QAbstractItemModel* inbox() noexcept { return &m_inboxModel; }

    // The storage root must be Decodium's native SSTV root. It is resolved by
    // the existing storage worker before this controller starts its own worker.
    void setStorageRoot(const QString& storageRoot,
                        const QString& stationProfile);

    Q_INVOKABLE bool setEnabled(bool enabled);
    Q_INVOKABLE bool configureProvider(const QVariantMap& configuration);
    Q_INVOKABLE bool clearCredentials();
    Q_INVOKABLE bool upload(const QUrl& source,
                            const QString& recipientId,
                            const QString& mode,
                            const QString& message,
                            bool recipientConfirmed);
    Q_INVOKABLE bool uploadWithOptions(const QUrl& source,
                                       const QString& recipientId,
                                       const QString& mode,
                                       const QString& message,
                                       bool recipientConfirmed,
                                       const QVariantMap& options);
    Q_INVOKABLE bool refreshInbox();
    Q_INVOKABLE bool download(const QString& incomingId,
                              const QString& destinationFileName);
    Q_INVOKABLE bool accept(const QString& transferId);
    Q_INVOKABLE bool acknowledge(const QString& transferId);
    Q_INVOKABLE bool reject(const QString& incomingId);
    Q_INVOKABLE bool saveAs(const QString& transferId,
                            const QUrl& destination);
    Q_INVOKABLE bool deleteLocalCopy(const QString& transferId);
    Q_INVOKABLE bool requestProviderDeletion(const QString& incomingId);
    Q_INVOKABLE bool blockSender(const QString& incomingId,
                                 bool localOnly);
    Q_INVOKABLE bool pause(const QString& transferId);
    Q_INVOKABLE bool resume(const QString& transferId);
    Q_INVOKABLE bool cancel(const QString& transferId);
    Q_INVOKABLE bool removeRemoteCopy(const QString& transferId);
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool resetDiagnostics();
    Q_INVOKABLE void shutdown();

    quintptr workerThreadToken() const noexcept
    {
        return m_workerThreadToken;
    }

signals:
    void stateChanged();
    void configurationChanged();
    void operationFinished(QString operation, bool ok, QString message);
    // Emitted after explicit user acceptance and native byte/image validation,
    // and once on restart for a durable accepted handoff that still needs
    // import. The owner-thread Gallery storage slot consumes this map;
    // sharing itself never inserts a Gallery row or triggers TX/PTT.
    void incomingHandoffReady(QVariantMap handoff);

private slots:
    void applySnapshot(QVariantList active,
                       QVariantList history,
                       QVariantList inbox,
                       QVariantMap state);

private:
    bool queueWorkerCall(std::function<void(SstvShareWorker*)> call);

    QThread m_workerThread;
    QPointer<SstvShareWorker> m_worker;
    SstvShareListModel m_activeModel;
    SstvShareListModel m_historyModel;
    SstvShareListModel m_inboxModel;
    QVariantMap m_configuration;
    QVariantMap m_diagnostics;
    QString m_statusText {tr("Remote sharing is off")};
    QString m_errorString;
    QString m_storageFolder;
    QString m_meteredNetworkStatus {QStringLiteral("unknown")};
    std::shared_ptr<std::atomic<int>> m_meteredNetworkState;
    quintptr m_workerThreadToken {0};
    bool m_ready {false};
    bool m_enabled {false};
    bool m_configured {false};
    bool m_busy {false};
    bool m_secureStorageAvailable {false};
    bool m_providerSupportsInbox {false};
    bool m_providerSupportsIncomingDelete {false};
    bool m_providerSupportsSenderBlocking {false};
    bool m_shutdown {false};
};

} // namespace decodium::sstv
