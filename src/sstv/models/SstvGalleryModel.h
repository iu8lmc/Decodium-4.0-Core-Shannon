// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../storage/SstvStorageWorker.h"

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

namespace decodium::sstv {

class SstvThumbnailProvider;

class SstvGalleryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY filtersChanged)
    Q_PROPERTY(int categoryMask READ categoryMask WRITE setCategoryMask NOTIFY filtersChanged)
    Q_PROPERTY(int remoteFilter READ remoteFilter WRITE setRemoteFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString modeFilter READ modeFilter WRITE setModeFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString callsignFilter READ callsignFilter WRITE setCallsignFilter NOTIFY filtersChanged)
    Q_PROPERTY(QDateTime capturedFromUtc READ capturedFromUtc WRITE setCapturedFromUtc NOTIFY filtersChanged)
    Q_PROPERTY(QDateTime capturedToUtc READ capturedToUtc WRITE setCapturedToUtc NOTIFY filtersChanged)
    Q_PROPERTY(qint64 minimumFrequencyHz READ minimumFrequencyHz WRITE setMinimumFrequencyHz NOTIFY filtersChanged)
    Q_PROPERTY(qint64 maximumFrequencyHz READ maximumFrequencyHz WRITE setMaximumFrequencyHz NOTIFY filtersChanged)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags NOTIFY filtersChanged)
    Q_PROPERTY(bool requireAllTags READ requireAllTags WRITE setRequireAllTags NOTIFY filtersChanged)
    Q_PROPERTY(int partialFilter READ partialFilter WRITE setPartialFilter NOTIFY filtersChanged)
    Q_PROPERTY(int uploadStateFilter READ uploadStateFilter WRITE setUploadStateFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filtersChanged)
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY filtersChanged)
    Q_PROPERTY(QVariantMap retentionSettings READ retentionSettings
               NOTIFY retentionSettingsChanged)
    Q_PROPERTY(QVariantMap quotaSummary READ quotaSummary
               NOTIFY quotaSummaryChanged)
    Q_PROPERTY(QVariantMap retentionPreview READ retentionPreview
               NOTIFY retentionPreviewChanged)
    Q_PROPERTY(bool retentionBusy READ retentionBusy
               NOTIFY retentionBusyChanged)

public:
    enum Role : int
    {
        IdRole = Qt::UserRole + 1,
        CategoryRole,
        CategoryNameRole,
        CapturedAtRole,
        UpdatedAtRole,
        ModeRole,
        VisCodeRole,
        RemoteCallsignRole,
        LocalCallsignRole,
        SourceRole,
        FrequencyHzRole,
        CompleteRole,
        PartialRole,
        RemoteRole,
        UploadStateRole,
        UploadStateNameRole,
        TagsRole,
        NoteRole,
        ImagePathRole,
        MetadataPathRole,
        WidthRole,
        HeightRole,
        SelectedRole,
        EventAtRole,
        CreatedAtRole,
        VisValidRole,
        FskIdRole,
        RemoteGridRole,
        LocalGridRole,
        AudioFrequencyHzRole,
        SourceSampleRateHzRole,
        DigitalRole,
        CompletionPercentRole,
        QualityMetricsRole,
        SlantCorrectionPpmRole,
        RawAudioPathRole,
        RelatedQsoIdRole,
        RemoteProviderRole,
        RemoteObjectIdRole,
        ExpiresAtRole,
        PrivacyFlagsRole,
        ThumbnailPathRole,
        MimeTypeRole,
        FileSizeBytesRole,
        OriginalWidthRole,
        OriginalHeightRole,
        Sha256HexRole,
        FavoriteRole
    };
    Q_ENUM(Role)

    explicit SstvGalleryModel(QObject* parent = nullptr);
    ~SstvGalleryModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

    SstvStorageWorker* storageWorker() const { return m_worker.data(); }
    void setStorageWorker(SstvStorageWorker* worker);
    void setUnavailableError(const QString& error);
    SstvThumbnailProvider* thumbnailProvider() const
    {
        return m_thumbnailProvider.data();
    }
    void setThumbnailProvider(SstvThumbnailProvider* provider);

    bool loading() const noexcept { return m_loading; }
    bool hasMore() const noexcept { return m_hasMore; }
    QString errorString() const { return m_errorString; }
    int selectedCount() const { return static_cast<int>(m_selected.size()); }
    int pageSize() const noexcept { return m_query.limit; }
    int categoryMask() const noexcept { return m_query.categoryMask; }
    int remoteFilter() const noexcept { return static_cast<int>(m_query.remote); }
    QString modeFilter() const { return m_query.mode; }
    QString callsignFilter() const { return m_query.callsign; }
    QDateTime capturedFromUtc() const { return m_query.capturedFromUtc; }
    QDateTime capturedToUtc() const { return m_query.capturedToUtc; }
    qint64 minimumFrequencyHz() const noexcept
    {
        return m_query.minimumFrequencyHz;
    }
    qint64 maximumFrequencyHz() const noexcept
    {
        return m_query.maximumFrequencyHz;
    }
    QStringList tags() const { return m_query.tags; }
    bool requireAllTags() const noexcept { return m_query.requireAllTags; }
    int partialFilter() const noexcept
    {
        return static_cast<int>(m_query.partial);
    }
    int uploadStateFilter() const noexcept { return m_query.uploadState; }
    QString search() const { return m_query.search; }
    int sortOrder() const noexcept { return static_cast<int>(m_query.sort); }
    SstvGalleryQuery query() const { return m_query; }
    QVariantMap retentionSettings() const { return m_retentionSettings; }
    QVariantMap quotaSummary() const { return m_quotaSummary; }
    QVariantMap retentionPreview() const { return m_retentionPreview; }
    bool retentionBusy() const noexcept { return m_retentionBusy; }

    bool setQuery(const SstvGalleryQuery& query, QString* error = nullptr);
    void setPageSize(int value);
    void setCategoryMask(int value);
    void setRemoteFilter(int value);
    void setModeFilter(const QString& value);
    void setCallsignFilter(const QString& value);
    void setCapturedFromUtc(QDateTime value);
    void setCapturedToUtc(QDateTime value);
    void setMinimumFrequencyHz(qint64 value);
    void setMaximumFrequencyHz(qint64 value);
    void setTags(const QStringList& value);
    void setRequireAllTags(bool value);
    void setPartialFilter(int value);
    void setUploadStateFilter(int value);
    void setSearch(const QString& value);
    void setSortOrder(int value);

    Q_INVOKABLE void reload();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE bool applyFilters(const QVariantMap& filters);
    Q_INVOKABLE QVariantMap filters() const;
    // QML must never assemble file URLs by concatenating a platform path.
    // QUrl owns Windows drive, UNC, escaping and fragment semantics.
    Q_INVOKABLE QUrl localFileUrl(const QString& path) const
    {
        return path.isEmpty() ? QUrl {} : QUrl::fromLocalFile(path);
    }
    Q_INVOKABLE void cancelPending();
    Q_INVOKABLE bool isSelected(const QString& id) const;
    Q_INVOKABLE void setSelected(const QString& id, bool selected);
    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE quint64 setFavorite(const QString& id, bool favorite);
    Q_INVOKABLE quint64 toggleFavorite(int row);
    // Bounded, local-only edit path for free-form operator metadata.  It
    // accepts neither paths nor a complete record; the storage worker fetches
    // the authoritative record before changing only note and tags.
    Q_INVOKABLE quint64 updateUserMetadata(const QString& imageId,
                                           const QString& note,
                                           const QStringList& tags);
    // Associates only by indexed image UUID and opaque QSO identifier. The
    // empty identifier explicitly disassociates; paths are never accepted.
    Q_INVOKABLE quint64 associateWithQso(const QString& imageId,
                                         const QString& qsoId);
    Q_INVOKABLE quint64 requestDeleteSelectedFromIndex();
    Q_INVOKABLE quint64 requestDeleteFromIndex(const QStringList& ids);
    Q_INVOKABLE quint64 requestDeleteSelectedWithFiles();
    Q_INVOKABLE quint64 requestDeleteWithFiles(const QStringList& ids);
    Q_INVOKABLE quint64 requestExportRecord(const QString& id,
                                            const QUrl& destination,
                                            bool replaceExisting = false);
    Q_INVOKABLE quint64 refreshQuota();
    Q_INVOKABLE quint64 requestRetentionPreview();
    Q_INVOKABLE quint64 applyRetentionPreview(
        const QString& token,
        const QString& confirmationPhrase);
    Q_INVOKABLE quint64 updateRetentionSettings(
        const QVariantMap& settings);
    Q_INVOKABLE quint64 requestAutomaticRetention();
    Q_INVOKABLE void shutdown();

signals:
    void loadingChanged();
    void hasMoreChanged();
    void errorStringChanged();
    void selectedCountChanged();
    void filtersChanged();
    void retentionSettingsChanged();
    void quotaSummaryChanged();
    void retentionPreviewChanged();
    void retentionBusyChanged();
    void queryRejected(QString error);
    void deleteRequested(quint64 requestId, QStringList ids);
    void deleteFinished(quint64 requestId, bool ok, QString error);
    void deleteFilesFinished(quint64 requestId,
                             bool ok,
                             QString warningOrError);
    void exportFinished(quint64 requestId,
                        bool ok,
                        QUrl destination,
                        QString error);
    void favoriteFinished(quint64 requestId,
                          QString id,
                          bool ok,
                          QString error);
    void qsoAssociationFinished(quint64 requestId,
                                QString imageId,
                                QString qsoId,
                                bool ok,
                                QString error);
    void userMetadataUpdateFinished(quint64 requestId,
                                    QString imageId,
                                    QString note,
                                    QStringList tags,
                                    bool ok,
                                    QString error);
    void retentionSettingsFinished(quint64 requestId,
                                   bool ok,
                                   QString error);
    void quotaRefreshFinished(quint64 requestId,
                              bool ok,
                              QString error);
    void retentionPreviewFinished(quint64 requestId,
                                  bool ok,
                                  QString error);
    void retentionApplyFinished(quint64 requestId,
                                bool automatic,
                                bool ok,
                                QString warningOrError);

private slots:
    void handleGalleryPage(quint64 requestId,
                           decodium::sstv::SstvGalleryPage page,
                           const QString& error);
    void handleRecordChanged(const decodium::sstv::SstvImageRecord& record);
    void handleRecordsRemoved(const QStringList& ids, quint64 requestId);
    void handleRecordsDeletedWithFiles(const QStringList& ids,
                                       quint64 requestId,
                                       const QString& cleanupWarning);
    void handleOperationFinished(quint64 requestId,
                                 decodium::sstv::SstvStorageOperation operation,
                                 bool ok,
                                 const QString& error);
    void handleRecordExported(quint64 requestId,
                              bool ok,
                              const QString& destinationPath,
                              const QString& error);
    void handleRetentionSettingsLoaded(
        const decodium::sstv::SstvRetentionSettings& settings,
        const QString& error);
    void handleRetentionSettingsUpdated(
        quint64 requestId,
        bool ok,
        const decodium::sstv::SstvRetentionSettings& settings,
        const QString& error);
    void handleQuotaCalculated(
        quint64 requestId,
        const decodium::sstv::SstvQuotaSummary& summary,
        const QString& error);
    void handleRetentionPreviewReady(
        quint64 requestId,
        const decodium::sstv::SstvRetentionPlan& plan,
        const QString& error);

private:
    struct PendingQsoAssociation final
    {
        QString imageId;
        QString qsoId;
    };

    struct PendingUserMetadata final
    {
        QString imageId;
        QString note;
        QStringList tags;
    };

    void requestNextPage();
    void setLoading(bool value);
    void setHasMore(bool value);
    void setErrorString(const QString& value);
    void clearRowsIncrementally();
    void setRetentionBusy(bool value);
    void clearRetentionState(const QString& reason);
    int indexOfId(const QString& id) const;
    int insertionIndex(const SstvImageRecord& record) const;
    bool recordMatches(const SstvImageRecord& record) const;
    bool comesBefore(const SstvImageRecord& left,
                     const SstvImageRecord& right) const;
    void insertRecordIncrementally(const SstvImageRecord& record,
                                   bool adjustNextOffset);
    void removeRowIncrementally(int row, bool adjustNextOffset);
    bool applyCandidateQuery(SstvGalleryQuery candidate);
    quint64 nextRequestId();

    QPointer<SstvStorageWorker> m_worker;
    QPointer<SstvThumbnailProvider> m_thumbnailProvider;
    QVector<QMetaObject::Connection> m_connections;
    QVector<SstvImageRecord> m_records;
    QSet<QString> m_selected;
    QHash<quint64, QStringList> m_deleteRequests;
    QHash<quint64, QStringList> m_fileDeleteRequests;
    QHash<quint64, QString> m_exportRequests;
    QHash<quint64, QString> m_favoriteRequests;
    QHash<quint64, PendingQsoAssociation> m_qsoAssociationRequests;
    QHash<quint64, PendingUserMetadata> m_userMetadataRequests;
    QSet<quint64> m_retentionSettingsRequests;
    QSet<quint64> m_quotaRequests;
    QSet<quint64> m_retentionPreviewRequests;
    QHash<quint64, bool> m_retentionApplyRequests;
    SstvGalleryQuery m_query;
    QVariantMap m_retentionSettings;
    QVariantMap m_quotaSummary;
    QVariantMap m_retentionPreview;
    quint64 m_nextRequestId {1};
    quint64 m_pendingPageRequest {0};
    int m_nextOffset {0};
    bool m_loading {false};
    bool m_hasMore {true};
    bool m_acceptingResults {true};
    bool m_retentionBusy {false};
    QString m_errorString;
};

} // namespace decodium::sstv
