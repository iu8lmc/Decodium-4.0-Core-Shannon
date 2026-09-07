// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvGalleryQuery.h"
#include "SstvImageStorage.h"
#include "src/sstv/sharing/SstvIncomingMediaValidator.h"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <atomic>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>

class QSqlDatabase;
class QSqlQuery;

namespace decodium::sstv {

enum class SstvStorageOperation : quint8
{
    Insert = 1,
    Update = 2,
    Remove = 3,
    RemoveMany = 4,
    StoreAndInsert = 5,
    Export = 6,
    DeleteManyFiles = 7,
    SetFavorite = 8,
    UpdateRetentionSettings = 9,
    CalculateQuota = 10,
    PreviewRetention = 11,
    AssociateQso = 12,
    // Deliberately narrower than Update: this is the only Gallery-facing
    // mutation that accepts operator-authored free text.  It can never alter
    // paths, integrity fields, radio metadata or sharing state.
    UpdateUserMetadata = 13
};

enum class SstvSharedRetentionPolicy : quint8
{
    Protect = 0,
    AllowUploaded = 1
};

// Persisted in SQLite.  Zero disables an individual age/quota constraint.
// Automatic deletion is deliberately disabled by default and is never
// inferred from non-zero limits.
struct SstvRetentionSettings final
{
    static constexpr qint64 kMaximumQuotaBytes =
        16LL * 1024LL * 1024LL * 1024LL * 1024LL;

    bool automaticEnabled {false};
    int maximumAgeDays {0};
    qint64 imageQuotaBytes {0};
    qint64 thumbnailQuotaBytes {0};
    qint64 rawAudioQuotaBytes {0};
    SstvSharedRetentionPolicy sharedPolicy {
        SstvSharedRetentionPolicy::Protect};
    int maximumDeletesPerRun {100};

    bool validate(QString* error = nullptr) const;
    QVariantMap toVariantMap() const;
    static bool fromVariantMap(const QVariantMap& values,
                               SstvRetentionSettings* settings,
                               QString* error = nullptr);
};

struct SstvQuotaSummary final
{
    qint64 imageBytes {0};
    qint64 thumbnailBytes {0};
    qint64 rawAudioBytes {0};
    qint64 metadataBytes {0};
    int recordCount {0};
    int missingFileCount {0};
    int unsafePathCount {0};
    bool complete {false};

    QVariantMap toVariantMap() const;
};

struct SstvRetentionPlan final
{
    QString token;
    QDateTime createdAtUtc;
    SstvRetentionSettings settings;
    QStringList recordIds;
    qint64 imageBytes {0};
    qint64 thumbnailBytes {0};
    qint64 rawAudioBytes {0};
    int protectedFavoriteCount {0};
    int protectedQsoCount {0};
    int protectedSharedCount {0};
    int protectedUnsafeCount {0};
    bool targetsSatisfied {true};
    QString confirmationPhrase;
    QString warning;

    QVariantMap toVariantMap() const;
};

struct SstvImagePageRequest final
{
    // Zero means all categories; otherwise the integer representation of
    // SstvImageCategory.  Keyset pagination remains stable when newer rows are
    // inserted while an existing gallery traversal is in progress.
    int categoryFilter {0};
    QString modeFilter;
    int limit {50};
    bool hasCursor {false};
    qint64 beforeCapturedAtMs {std::numeric_limits<qint64>::max()};
    QString beforeId;

    bool validate(QString* error = nullptr) const;
};

struct SstvImagePage final
{
    QVector<SstvImageRecord> records;
    bool hasMore {false};
    qint64 nextBeforeCapturedAtMs {0};
    QString nextBeforeId;
};

enum class SstvIncomingImportFailure : quint8
{
    None = 0,
    InvalidHandoff,
    UnsafeStagingPath,
    IntegrityFailure,
    Conflict,
    StorageUnavailable,
    StorageFailure,
    CleanupPending
};

// Exact completion contract for the accepted-sharing -> Gallery boundary.
// A retryable failure never transfers ownership of the staged PNG. On ok,
// the record is committed and the staged file is gone (or was already gone
// for an idempotent replay).
struct SstvIncomingImportResult final
{
    bool ok {false};
    bool retryable {false};
    bool idempotent {false};
    QString transferId;
    SstvIncomingImportFailure failure {
        SstvIncomingImportFailure::InvalidHandoff};
    SstvImageRecord record;
    QString error;
};

// Fixed-size, redacted storage telemetry.  Durations use the monotonic clock
// and no request identifiers, paths, filenames or metadata are retained.
struct SstvStoragePerformanceSnapshot final
{
    bool acceptingDatabaseOperations {false};
    quint64 databaseQueueDepth {0U};
    quint64 peakDatabaseQueueDepth {0U};
    quint64 databaseOperationsQueued {0U};
    quint64 databaseOperationsDispatched {0U};
    quint64 databaseOperationsCompleted {0U};
    quint64 databaseOperationsRejected {0U};
    quint64 databaseOperationsCancelled {0U};
    quint64 databaseQueueFailures {0U};
    quint64 imageSaveAttempts {0U};
    quint64 imageSaveSuccesses {0U};
    quint64 imageSaveFailures {0U};
    quint64 lastImageSaveNanoseconds {0U};
    quint64 averageImageSaveNanoseconds {0U};
    quint64 maximumImageSaveNanoseconds {0U};

    QVariantMap toVariantMap() const;
};

// Short mutex-protected updates are negligible relative to SQLite and PNG
// I/O, provide a coherent cross-thread snapshot, and avoid allocating per
// operation. Lifecycle-generation tickets prevent a cancelled callable from
// executing after a later reinitialization. Every addition saturates rather
// than wrapping.
class SstvStoragePerformanceCounters final
{
public:
    static constexpr quint64 kMaximumDatabaseQueueDepth = 1'024U;

    void beginLifecycle();
    void endLifecycle();
    std::optional<quint64> tryQueueDatabaseOperation();
    bool beginQueuedDatabaseOperation(quint64 lifecycleGeneration);
    void finishQueuedDatabaseOperation(bool dispatched = true);
    void cancelQueuedDatabaseOperation(quint64 lifecycleGeneration);
    void recordImageSave(quint64 elapsedNanoseconds, bool success);
    SstvStoragePerformanceSnapshot snapshot() const;

private:
    static void saturatingAdd(quint64& value,
                              quint64 increment = 1U) noexcept;

    mutable std::mutex m_mutex;
    SstvStoragePerformanceSnapshot m_snapshot;
    quint64 m_lifecycleGeneration {0U};
    quint64 m_totalImageSaveNanoseconds {0U};
};

// Move this object to its dedicated QThread before initialize().  Every public
// slot rejects a call from any other thread.  Its unique named QSQLITE
// connection is created, used, closed and removed exclusively on that owner
// thread; the process-wide default connection is never touched.
class SstvStorageWorker final : public QObject
{
    Q_OBJECT

public:
    static constexpr int kCurrentSchemaVersion = 5;
    static constexpr int kMaximumPageSize = 200;
    static constexpr quint64 kMaximumDatabaseQueueDepth =
        SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth;
    using DatabaseOperation = std::function<void(SstvStorageWorker&)>;

    explicit SstvStorageWorker(QString databasePath,
                               QString storageRoot,
                               SstvStorageLimits limits = {},
                               QObject* parent = nullptr);
    ~SstvStorageWorker() override;

    QString databasePath() const { return m_databasePath; }
    QString storageRoot() const { return m_layout.rootPath(); }
    QString connectionName() const { return m_connectionName; }
    bool isInitialized() const noexcept
    {
        return m_initialized.load(std::memory_order_acquire);
    }
    int schemaVersion() const noexcept
    {
        return m_schemaVersion.load(std::memory_order_acquire);
    }
    // The only queue-depth-aware dispatch path. It is safe to call from any
    // thread, rejects work once shutdown begins or the fixed bound is full,
    // and never records captured request data in diagnostics.
    bool enqueueDatabaseOperation(DatabaseOperation operation);
    SstvStoragePerformanceSnapshot performanceSnapshot() const;

public slots:
    void initialize();
    void shutdown();

    void insertRecord(decodium::sstv::SstvImageRecord record,
                      quint64 requestId);
    // Performs PNG/sidecar encoding and the matching SQLite insert on this
    // worker thread.  Callers therefore never need to perform file I/O on the
    // GUI or audio thread, and Gallery observes the same recordChanged signal
    // as it does for an already materialised record.
    void storeAndInsertImage(decodium::sstv::SstvImageSaveRequest request,
                             quint64 requestId);
    // Direct queued-connect target for
    // SstvShareController::incomingHandoffReady(QVariantMap). Parsing,
    // filesystem I/O, image decoding and SQLite work all stay on this
    // worker's owner thread.
    void importValidatedIncomingHandoff(QVariantMap handoff);
    // Typed native entry point for non-QML producers and tests.
    void importValidatedIncomingHandoffTyped(
        decodium::sstv::sharing::SstvValidatedIncomingHandoff handoff);
    void updateRecord(decodium::sstv::SstvImageRecord record,
                      quint64 requestId);
    // Updates the SQLite projection and the exact sidecar on the storage
    // thread.  This is the only UI-facing favourite mutation path.
    void setFavorite(QString id, bool favorite, quint64 requestId);
    // Associates an opaque local QSO identifier with an indexed image. An
    // empty qsoId explicitly removes the association. No path is accepted.
    void associateWithQso(QString imageId,
                          QString qsoId,
                          quint64 requestId);
    // Updates only operator-authored note and tags. The worker reloads and
    // revalidates the indexed record itself, atomically publishes the exact
    // sidecar, then commits a guarded SQLite projection and normalized tags.
    // QML must not construct or submit a whole SstvImageRecord.
    void updateUserMetadata(QString imageId,
                            QString note,
                            QStringList tags,
                            quint64 requestId);
    void removeRecord(QString id, quint64 requestId);
    // Explicit, index-only bulk removal.  Image and sidecar files are never
    // unlinked here and there is deliberately no automatic orphan cleanup.
    void removeRecords(QStringList ids, quint64 requestId);
    // Explicit destructive path used only after a separate UI confirmation.
    // Verified image/sidecar/thumbnail and unshared raw-audio files are first
    // renamed into private in-root staging, then rows are deleted atomically.
    void deleteRecordsWithFiles(QStringList ids, quint64 requestId);
    void fetchRecord(QString id, quint64 requestId);
    // Copies one verified Gallery PNG to an explicitly selected local path.
    // The copy is streamed and atomically committed on this worker thread;
    // it never mutates the indexed source or its metadata sidecar.
    void exportRecord(QString id,
                      QString destinationPath,
                      bool replaceExisting,
                      quint64 requestId);
    void listRecords(decodium::sstv::SstvImagePageRequest request,
                     quint64 requestId);
    void queryGallery(decodium::sstv::SstvGalleryQuery request,
                      quint64 requestId);
    void loadRetentionSettings();
    void updateRetentionSettings(
        decodium::sstv::SstvRetentionSettings settings,
        quint64 requestId);
    void calculateQuota(quint64 requestId);
    void previewRetention(decodium::sstv::SstvRetentionSettings settings,
                          quint64 requestId);
    // Applies only the most recently previewed, unexpired token.  The exact
    // confirmationPhrase from that preview is required and every protection
    // and path invariant is revalidated before DeleteManyFiles is entered.
    void applyRetentionPlan(QString token,
                            QString confirmationPhrase,
                            quint64 requestId);
    // Uses persisted settings and refuses unless automaticEnabled is true.
    void runAutomaticRetention(quint64 requestId);

signals:
    void initialized(bool ok,
                     QString error,
                     int schemaVersion,
                     quintptr workerThreadToken);
    void shutdownFinished(quintptr workerThreadToken);
    void operationFinished(quint64 requestId,
                           decodium::sstv::SstvStorageOperation operation,
                           bool ok,
                           QString error);
    void imageStoreFinished(quint64 requestId,
                            bool ok,
                            decodium::sstv::SstvImageRecord record,
                            QString error);
    void incomingImportFinished(
        decodium::sstv::SstvIncomingImportResult result);
    void recordFetched(quint64 requestId,
                       bool found,
                       decodium::sstv::SstvImageRecord record,
                       QString error);
    void recordExported(quint64 requestId,
                        bool ok,
                        QString destinationPath,
                        QString error);
    void pageFetched(quint64 requestId,
                     decodium::sstv::SstvImagePage page,
                     QString error);
    void galleryPageFetched(quint64 requestId,
                            decodium::sstv::SstvGalleryPage page,
                            QString error);
    void recordChanged(decodium::sstv::SstvImageRecord record);
    void recordsRemoved(QStringList ids, quint64 requestId);
    void recordsDeletedWithFiles(QStringList ids,
                                 quint64 requestId,
                                 QString cleanupWarning);
    void retentionSettingsLoaded(
        decodium::sstv::SstvRetentionSettings settings,
        QString error);
    void retentionSettingsUpdated(
        quint64 requestId,
        bool ok,
        decodium::sstv::SstvRetentionSettings settings,
        QString error);
    void quotaCalculated(quint64 requestId,
                         decodium::sstv::SstvQuotaSummary summary,
                         QString error);
    void retentionPreviewReady(quint64 requestId,
                               decodium::sstv::SstvRetentionPlan plan,
                               QString error);
    void threadOwnershipViolation(QString operation);

private:
    bool requireOwnerThread(const QString& operation);
    bool migrateSchema(QSqlDatabase& database,
                       int* resultingVersion,
                       QString* error);
    bool recoverDeletionStaging(QSqlDatabase& database,
                                QString* error);
    bool validateSchema(QSqlDatabase& database, QString* error) const;
    bool validateRecordForWrite(const SstvImageRecord& record,
                                QString* error) const;
    bool readRetentionSettings(QSqlDatabase& database,
                               SstvRetentionSettings* settings,
                               QString* error) const;
    bool buildRetentionPlan(QSqlDatabase& database,
                            const SstvRetentionSettings& settings,
                            SstvRetentionPlan* plan,
                            SstvQuotaSummary* quota,
                            QString* error) const;
    bool retentionRecordIsProtected(const SstvImageRecord& record,
                                    const SstvRetentionSettings& settings,
                                    QString* reason,
                                    QString* error) const;
    bool insertRecordTransaction(const SstvImageRecord& record,
                                 QString* error);
    void importValidatedIncoming(
        const sharing::SstvValidatedIncomingHandoff& handoff);
    void closeConnection();
    void emitOperationFailure(quint64 requestId,
                              SstvStorageOperation operation,
                              const QString& error);

    static bool bindRecord(QSqlQuery& query,
                           const SstvImageRecord& record,
                           QString* error);
    static bool readRecord(QSqlQuery& query,
                           const SstvStorageLimits& limits,
                           SstvImageRecord* record,
                           QString* error);
    static bool replaceTags(QSqlDatabase& database,
                            const SstvImageRecord& record,
                            QString* error);
    static quintptr currentThreadToken() noexcept;

    QString m_databasePath;
    SstvStorageLayout m_layout;
    SstvStorageLimits m_limits;
    QString m_connectionName;
    std::atomic_bool m_initialized {false};
    std::atomic_int m_schemaVersion {0};
    SstvStoragePerformanceCounters m_performance;
    std::optional<SstvRetentionPlan> m_retentionPlan;
};

} // namespace decodium::sstv

Q_DECLARE_METATYPE(decodium::sstv::SstvStorageOperation)
Q_DECLARE_METATYPE(decodium::sstv::SstvImagePageRequest)
Q_DECLARE_METATYPE(decodium::sstv::SstvImagePage)
Q_DECLARE_METATYPE(decodium::sstv::SstvIncomingImportFailure)
Q_DECLARE_METATYPE(decodium::sstv::SstvIncomingImportResult)
Q_DECLARE_METATYPE(decodium::sstv::SstvStoragePerformanceSnapshot)
Q_DECLARE_METATYPE(decodium::sstv::SstvSharedRetentionPolicy)
Q_DECLARE_METATYPE(decodium::sstv::SstvRetentionSettings)
Q_DECLARE_METATYPE(decodium::sstv::SstvQuotaSummary)
Q_DECLARE_METATYPE(decodium::sstv::SstvRetentionPlan)
