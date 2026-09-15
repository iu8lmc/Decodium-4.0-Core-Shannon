// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/storage/SstvImageStorage.h"
#include "src/sstv/storage/SstvStorageWorker.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSemaphore>
#include <QSet>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimeZone>
#include <QUuid>

#include <atomic>
#include <array>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

SstvImageSaveRequest makeRequest(SstvImageCategory category,
                                 const QDateTime& captured,
                                 const QString& id = {})
{
    SstvImageSaveRequest request;
    request.record.id = id;
    request.record.category = category;
    request.record.capturedAtUtc = captured.toUTC();
    request.record.eventAtUtc = request.record.capturedAtUtc;
    request.record.mode = QStringLiteral("Martin M1");
    request.record.visCode = 44;
    request.record.visValid = true;
    request.record.fskId = QStringLiteral("DE IU8LMC");
    request.record.remoteCallsign = QStringLiteral("9H1TEST/P");
    request.record.remoteGrid = QStringLiteral("JM75FV");
    request.record.localCallsign = QStringLiteral("IU8LMC");
    request.record.localGrid = QStringLiteral("JM89AE");
    request.record.source = QStringLiteral("Decodium monitor");
    request.record.frequencyHz = 14'230'000;
    request.record.audioFrequencyHz = 1'900;
    request.record.sourceSampleRateHz = 48'000;
    request.record.digital = category == SstvImageCategory::Imported;
    request.record.completionPercent = 100;
    request.record.complete = true;
    request.record.qualityMetrics.insert(QStringLiteral("snrDb"), 18.25);
    request.record.qualityMetrics.insert(QStringLiteral("syncConfidence"), 0.97);
    request.record.slantCorrectionPpm = -12.5;
    request.record.relatedQsoId = QStringLiteral("qso-20260824-001");
    request.record.remote = category == SstvImageCategory::Imported;
    request.record.uploadState = SstvUploadState::Pending;
    request.record.remoteProvider = QStringLiteral("configured-webdav");
    request.record.remoteObjectId = QStringLiteral("sstv/object-001");
    request.record.expiresAtUtc = request.record.capturedAtUtc.addDays(7);
    request.record.privacyFlags = 0x5U;
    request.record.tags = {QStringLiteral("portable"),
                           QStringLiteral("Málaga")};
    request.record.note = QStringLiteral("native SSTV test");
    request.record.originalWidth = 40;
    request.record.originalHeight = 30;
    request.image = QImage(32, 24, QImage::Format_RGB32);
    for (int y = 0; y < request.image.height(); ++y) {
        for (int x = 0; x < request.image.width(); ++x) {
            request.image.setPixelColor(
                x, y, QColor::fromRgb((x * 7) % 256,
                                      (y * 11) % 256,
                                      ((x + y) * 5) % 256));
        }
    }
    return request;
}

QString uniqueConnectionName(const QString& prefix)
{
    return prefix + QLatin1Char('_')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QDateTime utcDateTime(const QDate& date, const QTime& time)
{
    return QDateTime(date, time, QTimeZone(QTimeZone::UTC));
}

bool execSql(QSqlDatabase& database, const QString& sql, QString* error)
{
    QSqlQuery query(database);
    if (query.exec(sql)) {
        return true;
    }
    if (error) {
        *error = query.lastError().text();
    }
    return false;
}

bool createVersionOneDatabase(const QString& databasePath,
                              const SstvImageRecord& record,
                              QString* error)
{
    const QString connection = uniqueConnectionName(QStringLiteral("sstv_v1"));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(databasePath);
        if (!database.open()) {
            if (error) {
                *error = database.lastError().text();
            }
        } else {
            const QString schema = QStringLiteral(
                "CREATE TABLE sstv_images ("
                "id TEXT PRIMARY KEY NOT NULL CHECK(length(id)=36),"
                "category INTEGER NOT NULL CHECK(category BETWEEN 1 AND 4),"
                "captured_at_ms INTEGER NOT NULL CHECK(captured_at_ms>0),"
                "created_at_ms INTEGER NOT NULL CHECK(created_at_ms>0),"
                "mode TEXT NOT NULL CHECK(length(mode) BETWEEN 1 AND 64),"
                "vis_code INTEGER NOT NULL DEFAULT -1 CHECK(vis_code BETWEEN -1 AND 255),"
                "remote_callsign TEXT NOT NULL DEFAULT '',"
                "local_callsign TEXT NOT NULL DEFAULT '',"
                "source TEXT NOT NULL,frequency_hz INTEGER NOT NULL DEFAULT 0,"
                "complete INTEGER NOT NULL CHECK(complete IN (0,1)),"
                "image_path TEXT NOT NULL UNIQUE,metadata_path TEXT NOT NULL UNIQUE,"
                "sha256_hex TEXT NOT NULL CHECK(length(sha256_hex)=64),"
                "file_size_bytes INTEGER NOT NULL CHECK(file_size_bytes>0),"
                "width INTEGER NOT NULL CHECK(width>0),"
                "height INTEGER NOT NULL CHECK(height>0))");
            if (execSql(database, schema, error)
                && execSql(database, QStringLiteral("PRAGMA user_version=1"),
                           error)) {
                QSqlQuery insert(database);
                insert.prepare(QStringLiteral(
                    "INSERT INTO sstv_images VALUES("
                    ":id,:category,:captured,:created,:mode,:vis,:remote,:local,"
                    ":source,:frequency,:complete,:image,:metadata,:hash,:bytes,"
                    ":width,:height)"));
                insert.bindValue(QStringLiteral(":id"), record.id);
                insert.bindValue(QStringLiteral(":category"),
                                 static_cast<int>(record.category));
                insert.bindValue(QStringLiteral(":captured"),
                                 record.capturedAtUtc.toMSecsSinceEpoch());
                insert.bindValue(QStringLiteral(":created"),
                                 record.createdAtUtc.toMSecsSinceEpoch());
                insert.bindValue(QStringLiteral(":mode"), record.mode);
                insert.bindValue(QStringLiteral(":vis"), record.visCode);
                insert.bindValue(QStringLiteral(":remote"), record.remoteCallsign);
                insert.bindValue(QStringLiteral(":local"), record.localCallsign);
                insert.bindValue(QStringLiteral(":source"), record.source);
                insert.bindValue(QStringLiteral(":frequency"), record.frequencyHz);
                insert.bindValue(QStringLiteral(":complete"), record.complete ? 1 : 0);
                insert.bindValue(QStringLiteral(":image"), record.imagePath);
                insert.bindValue(QStringLiteral(":metadata"), record.metadataPath);
                insert.bindValue(QStringLiteral(":hash"),
                                 QString::fromLatin1(record.sha256.toHex()));
                insert.bindValue(QStringLiteral(":bytes"), record.fileSizeBytes);
                insert.bindValue(QStringLiteral(":width"), record.width);
                insert.bindValue(QStringLiteral(":height"), record.height);
                ok = insert.exec();
                if (!ok && error) {
                    *error = insert.lastError().text();
                }
            }
            database.close();
        }
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

bool createVersionThreeDatabase(const QString& databasePath,
                                const SstvImageRecord& record,
                                bool addPartialV4Column,
                                QString* error)
{
    const QString connection = uniqueConnectionName(QStringLiteral("sstv_v3"));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(databasePath);
        if (!database.open() || !database.transaction()) {
            if (error) {
                *error = database.lastError().text();
            }
        } else {
            const QString imageSchema = QStringLiteral(
                "CREATE TABLE sstv_images ("
                "id TEXT PRIMARY KEY NOT NULL CHECK(length(id)=36),"
                "category INTEGER NOT NULL CHECK(category BETWEEN 1 AND 4),"
                "captured_at_ms INTEGER NOT NULL CHECK(captured_at_ms>0),"
                "created_at_ms INTEGER NOT NULL CHECK(created_at_ms>0),"
                "mode TEXT NOT NULL CHECK(length(mode) BETWEEN 1 AND 64),"
                "vis_code INTEGER NOT NULL DEFAULT -1 CHECK(vis_code BETWEEN -1 AND 255),"
                "remote_callsign TEXT NOT NULL DEFAULT '' CHECK(length(remote_callsign)<=64),"
                "local_callsign TEXT NOT NULL DEFAULT '' CHECK(length(local_callsign)<=64),"
                "source TEXT NOT NULL CHECK(length(source) BETWEEN 1 AND 128),"
                "frequency_hz INTEGER NOT NULL DEFAULT 0 CHECK(frequency_hz>=0),"
                "complete INTEGER NOT NULL CHECK(complete IN (0,1)),"
                "image_path TEXT NOT NULL UNIQUE,metadata_path TEXT NOT NULL UNIQUE,"
                "sha256_hex TEXT NOT NULL CHECK(length(sha256_hex)=64),"
                "file_size_bytes INTEGER NOT NULL CHECK(file_size_bytes>0),"
                "width INTEGER NOT NULL CHECK(width>0),"
                "height INTEGER NOT NULL CHECK(height>0),"
                "updated_at_ms INTEGER NOT NULL DEFAULT 0,"
                "note TEXT NOT NULL DEFAULT '',"
                "remote INTEGER NOT NULL DEFAULT 0 CHECK(remote IN (0,1)),"
                "upload_state INTEGER NOT NULL DEFAULT 0 "
                "CHECK(upload_state BETWEEN 0 AND 4),"
                "tags_json TEXT NOT NULL DEFAULT '[]')");
            const QString tagSchema = QStringLiteral(
                "CREATE TABLE sstv_image_tags ("
                "image_id TEXT NOT NULL REFERENCES sstv_images(id) ON DELETE CASCADE,"
                "tag TEXT NOT NULL CHECK(length(tag) BETWEEN 1 AND 64),"
                "tag_folded TEXT NOT NULL CHECK(length(tag_folded) BETWEEN 1 AND 64),"
                "PRIMARY KEY(image_id,tag_folded))");
            ok = execSql(database, QStringLiteral("PRAGMA foreign_keys=ON"), error)
                && execSql(database, imageSchema, error)
                && execSql(database, tagSchema, error)
                && execSql(database, QStringLiteral(
                    "CREATE INDEX idx_sstv_images_page "
                    "ON sstv_images(captured_at_ms DESC,id DESC)"), error)
                && execSql(database, QStringLiteral(
                    "CREATE INDEX idx_sstv_images_category_page "
                    "ON sstv_images(category,captured_at_ms DESC,id DESC)"), error)
                && execSql(database, QStringLiteral(
                    "CREATE INDEX idx_sstv_images_updated "
                    "ON sstv_images(updated_at_ms DESC,id DESC)"), error)
                && execSql(database, QStringLiteral(
                    "CREATE INDEX idx_sstv_images_filters "
                    "ON sstv_images(remote,upload_state,complete,category)"), error)
                && execSql(database, QStringLiteral(
                    "CREATE INDEX idx_sstv_tags_lookup "
                    "ON sstv_image_tags(tag_folded,image_id)"), error);
            QJsonArray tagArray;
            for (const QString& tag : record.tags) {
                tagArray.append(tag);
            }
            if (ok) {
                QSqlQuery insert(database);
                insert.prepare(QStringLiteral(
                    "INSERT INTO sstv_images("
                    "id,category,captured_at_ms,created_at_ms,mode,vis_code,"
                    "remote_callsign,local_callsign,source,frequency_hz,complete,"
                    "image_path,metadata_path,sha256_hex,file_size_bytes,width,height,"
                    "updated_at_ms,note,remote,upload_state,tags_json) VALUES("
                    ":id,:category,:captured,:created,:mode,:vis,:remote_call,"
                    ":local_call,:source,:frequency,:complete,:image,:metadata,"
                    ":hash,:bytes,:width,:height,:updated,:note,:remote,"
                    ":upload_state,:tags_json)"));
                insert.bindValue(QStringLiteral(":id"), record.id);
                insert.bindValue(QStringLiteral(":category"),
                                 static_cast<int>(record.category));
                insert.bindValue(QStringLiteral(":captured"),
                                 record.capturedAtUtc.toMSecsSinceEpoch());
                insert.bindValue(QStringLiteral(":created"),
                                 record.createdAtUtc.toMSecsSinceEpoch());
                insert.bindValue(QStringLiteral(":mode"), record.mode);
                insert.bindValue(QStringLiteral(":vis"), record.visCode);
                insert.bindValue(QStringLiteral(":remote_call"),
                                 record.remoteCallsign);
                insert.bindValue(QStringLiteral(":local_call"),
                                 record.localCallsign);
                insert.bindValue(QStringLiteral(":source"), record.source);
                insert.bindValue(QStringLiteral(":frequency"), record.frequencyHz);
                insert.bindValue(QStringLiteral(":complete"), record.complete ? 1 : 0);
                insert.bindValue(QStringLiteral(":image"), record.imagePath);
                insert.bindValue(QStringLiteral(":metadata"), record.metadataPath);
                insert.bindValue(QStringLiteral(":hash"),
                                 QString::fromLatin1(record.sha256.toHex()));
                insert.bindValue(QStringLiteral(":bytes"), record.fileSizeBytes);
                insert.bindValue(QStringLiteral(":width"), record.width);
                insert.bindValue(QStringLiteral(":height"), record.height);
                insert.bindValue(QStringLiteral(":updated"),
                                 record.updatedAtUtc.toMSecsSinceEpoch());
                insert.bindValue(QStringLiteral(":note"), record.note);
                insert.bindValue(QStringLiteral(":remote"), record.remote ? 1 : 0);
                insert.bindValue(QStringLiteral(":upload_state"),
                                 static_cast<int>(record.uploadState));
                insert.bindValue(QStringLiteral(":tags_json"),
                    QString::fromUtf8(QJsonDocument(tagArray).toJson(
                        QJsonDocument::Compact)));
                ok = insert.exec();
                if (!ok && error) {
                    *error = insert.lastError().text();
                }
            }
            for (const QString& tag : record.tags) {
                if (!ok) {
                    break;
                }
                QSqlQuery insertTag(database);
                insertTag.prepare(QStringLiteral(
                    "INSERT INTO sstv_image_tags(image_id,tag,tag_folded) "
                    "VALUES(:id,:tag,:folded)"));
                insertTag.bindValue(QStringLiteral(":id"), record.id);
                insertTag.bindValue(QStringLiteral(":tag"), tag);
                insertTag.bindValue(QStringLiteral(":folded"),
                                    tag.toCaseFolded());
                ok = insertTag.exec();
                if (!ok && error) {
                    *error = insertTag.lastError().text();
                }
            }
            if (ok && addPartialV4Column) {
                ok = execSql(database, QStringLiteral(
                    "ALTER TABLE sstv_images ADD COLUMN thumbnail_path "
                    "TEXT NOT NULL DEFAULT '' CHECK(length(thumbnail_path)<=4096)"),
                    error);
            }
            if (ok) {
                ok = execSql(database, QStringLiteral("PRAGMA user_version=3"),
                             error) && database.commit();
            } else {
                database.rollback();
            }
            if (!ok && error && error->isEmpty()) {
                *error = database.lastError().text();
            }
            database.close();
        }
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

class WorkerSession final
{
public:
    WorkerSession(const QString& databasePath,
                  const QString& storageRoot,
                  const SstvStorageLimits& limits = {})
        : worker(new SstvStorageWorker(databasePath, storageRoot, limits))
    {
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished,
                         worker, &QObject::deleteLater);
    }

    ~WorkerSession()
    {
        stop();
    }

    bool start(QString* error = nullptr)
    {
        if (thread.isRunning()) {
            return initialized;
        }
        QSignalSpy spy(worker, &SstvStorageWorker::initialized);
        thread.start();
        if (!QMetaObject::invokeMethod(worker, &SstvStorageWorker::initialize,
                                      Qt::QueuedConnection)
            || !spy.wait(5000) || spy.count() != 1) {
            if (error) {
                *error = QStringLiteral("worker initialization timed out");
            }
            return false;
        }
        const QList<QVariant> arguments = spy.takeFirst();
        initialized = arguments.at(0).toBool();
        schemaVersion = arguments.at(2).toInt();
        workerThreadToken = arguments.at(3).value<quintptr>();
        if (!initialized && error) {
            *error = arguments.at(1).toString();
        }
        return initialized;
    }

    void stop()
    {
        if (!worker || !thread.isRunning()) {
            return;
        }
        QSignalSpy spy(worker, &SstvStorageWorker::shutdownFinished);
        QMetaObject::invokeMethod(worker, &SstvStorageWorker::shutdown,
                                  Qt::QueuedConnection);
        spy.wait(5000);
        thread.quit();
        thread.wait(5000);
        worker = nullptr;
        initialized = false;
    }

    QThread thread;
    SstvStorageWorker* worker {nullptr};
    bool initialized {false};
    int schemaVersion {0};
    quintptr workerThreadToken {0};
};

QList<QVariant> associateWithQsoAndWait(SstvStorageWorker* worker,
                                        const QString& imageId,
                                        const QString& qsoId,
                                        quint64 requestId)
{
    QSignalSpy spy(worker, &SstvStorageWorker::operationFinished);
    const bool queued = QMetaObject::invokeMethod(
        worker,
        [worker, imageId, qsoId, requestId]() {
            worker->associateWithQso(imageId, qsoId, requestId);
        }, Qt::QueuedConnection);
    if (!queued || (spy.isEmpty() && !spy.wait(5'000))) {
        return {};
    }
    for (const QList<QVariant>& result : spy) {
        if (result.at(0).toULongLong() == requestId
            && result.at(1).value<SstvStorageOperation>()
                == SstvStorageOperation::AssociateQso) {
            return result;
        }
    }
    return {};
}

QList<QVariant> updateUserMetadataAndWait(SstvStorageWorker* worker,
                                          const QString& imageId,
                                          const QString& note,
                                          const QStringList& tags,
                                          quint64 requestId)
{
    QSignalSpy spy(worker, &SstvStorageWorker::operationFinished);
    const bool queued = QMetaObject::invokeMethod(
        worker,
        [worker, imageId, note, tags, requestId]() {
            worker->updateUserMetadata(imageId, note, tags, requestId);
        }, Qt::QueuedConnection);
    if (!queued || (spy.isEmpty() && !spy.wait(5'000))) {
        return {};
    }
    for (const QList<QVariant>& result : spy) {
        if (result.at(0).toULongLong() == requestId
            && result.at(1).value<SstvStorageOperation>()
                == SstvStorageOperation::UpdateUserMetadata) {
            return result;
        }
    }
    return {};
}

QList<QVariant> fetchRecordAndWait(SstvStorageWorker* worker,
                                   const QString& imageId,
                                   quint64 requestId)
{
    QSignalSpy spy(worker, &SstvStorageWorker::recordFetched);
    const bool queued = QMetaObject::invokeMethod(
        worker,
        [worker, imageId, requestId]() {
            worker->fetchRecord(imageId, requestId);
        }, Qt::QueuedConnection);
    if (!queued || (spy.isEmpty() && !spy.wait(5'000))) {
        return {};
    }
    for (const QList<QVariant>& result : spy) {
        if (result.at(0).toULongLong() == requestId) {
            return result;
        }
    }
    return {};
}

bool executeStandaloneSql(const QString& databasePath,
                          const QString& sql,
                          QString* error)
{
    const QString connectionName = uniqueConnectionName(
        QStringLiteral("sstv_test_external_sql"));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!database.open()) {
            if (error) {
                *error = database.lastError().text();
            }
        } else {
            QSqlQuery query(database);
            query.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
            ok = query.exec(sql);
            if (!ok && error) {
                *error = query.lastError().text();
            }
            database.close();
        }
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

bool readStoredTags(const QString& databasePath,
                    const QString& imageId,
                    QStringList* tags,
                    QString* error)
{
    if (!tags) {
        if (error) {
            *error = QStringLiteral("tag output is required");
        }
        return false;
    }
    const QString connectionName = uniqueConnectionName(
        QStringLiteral("sstv_test_read_tags"));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!database.open()) {
            if (error) {
                *error = database.lastError().text();
            }
        } else {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT tag FROM sstv_image_tags WHERE image_id=:id "
                "ORDER BY tag_folded"));
            query.bindValue(QStringLiteral(":id"), imageId);
            ok = query.exec();
            if (ok) {
                tags->clear();
                while (query.next()) {
                    tags->append(query.value(0).toString());
                }
            } else if (error) {
                *error = query.lastError().text();
            }
            database.close();
        }
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

} // namespace

class TestSstvStorage final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void performanceCountersAreBoundedThreadSafeAndRedacted();
    void workerQueueDepthAndShutdownCancellationAreMeasured();
    void standardLayoutSeparatesCategories();
    void filenameSanitizationBlocksTraversalAndReservedNames();
    void atomicPngMetadataRoundTripAndCollision();
    void concurrentAtomicSavesNeverOverwrite();
    void enforcesImageLimitsAndPathContainment();
    void workerStoresAndIndexesImageAtomicallyOffOwnerThread();
    void fullMetadataSqliteRoundTrip();
    void indexedRecordSurvivesMissingImageAndThumbnail();
    void workerCrudKeysetPaginationAndConcurrentRead();
    void deletionJournalRecoversBeforeAndAfterDatabaseCommit();
    void favoritePersistsInSchemaSidecarAndRestart();
    void qsoAssociationPersistsAndProtectsRetention();
    void userMetadataPersistsAndRestoresOnDatabaseFailure();
    void quotaRetentionProtectionManualApplyAndAutoOptIn();
    void migratesVersionOneNonDestructively();
    void migratesVersionThreeAndResumesPartialSchema();
    void rejectsFutureSchemaWithoutMutation();
};

void TestSstvStorage::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IU8LMC"));
    QCoreApplication::setApplicationName(QStringLiteral("Decodium SSTV Storage Test"));
    QStandardPaths::setTestModeEnabled(true);
    qRegisterMetaType<SstvImageRecord>();
    qRegisterMetaType<SstvImageSaveRequest>();
    qRegisterMetaType<SstvImagePageRequest>();
    qRegisterMetaType<SstvImagePage>();
    qRegisterMetaType<SstvStorageOperation>();
    qRegisterMetaType<SstvGalleryQuery>();
    qRegisterMetaType<SstvGalleryPage>();
    qRegisterMetaType<SstvRetentionSettings>();
    qRegisterMetaType<SstvQuotaSummary>();
    qRegisterMetaType<SstvRetentionPlan>();
    qRegisterMetaType<SstvStoragePerformanceSnapshot>();
}

void TestSstvStorage::performanceCountersAreBoundedThreadSafeAndRedacted()
{
    SstvStoragePerformanceCounters queue;
    QVERIFY(!queue.snapshot().acceptingDatabaseOperations);
    QVERIFY(!queue.tryQueueDatabaseOperation().has_value());
    queue.beginLifecycle();
    quint64 queueGeneration = 0U;
    for (quint64 index = 0U;
         index < SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth;
         ++index) {
        const std::optional<quint64> ticket =
            queue.tryQueueDatabaseOperation();
        QVERIFY(ticket.has_value());
        queueGeneration = *ticket;
    }
    QVERIFY(!queue.tryQueueDatabaseOperation().has_value());
    SstvStoragePerformanceSnapshot snapshot = queue.snapshot();
    QVERIFY(snapshot.acceptingDatabaseOperations);
    QCOMPARE(snapshot.databaseQueueDepth,
             SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth);
    QCOMPARE(snapshot.peakDatabaseQueueDepth,
             SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth);
    QCOMPARE(snapshot.databaseOperationsQueued,
             SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth);
    QCOMPARE(snapshot.databaseOperationsRejected, quint64 {2U});
    QCOMPARE(snapshot.databaseQueueFailures, quint64 {2U});

    for (quint64 index = 0U;
         index < SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth;
         ++index) {
        QVERIFY(queue.beginQueuedDatabaseOperation(queueGeneration));
        queue.finishQueuedDatabaseOperation();
    }
    snapshot = queue.snapshot();
    QCOMPARE(snapshot.databaseQueueDepth, quint64 {0U});
    QCOMPARE(snapshot.databaseOperationsDispatched,
             SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth);
    QCOMPARE(snapshot.databaseOperationsCompleted,
             SstvStoragePerformanceCounters::kMaximumDatabaseQueueDepth);

    for (int index = 0; index < 3; ++index) {
        const std::optional<quint64> ticket =
            queue.tryQueueDatabaseOperation();
        QVERIFY(ticket.has_value());
        QCOMPARE(*ticket, queueGeneration);
    }
    queue.endLifecycle();
    snapshot = queue.snapshot();
    QVERIFY(!snapshot.acceptingDatabaseOperations);
    QCOMPARE(snapshot.databaseQueueDepth, quint64 {0U});
    QCOMPARE(snapshot.databaseOperationsCancelled, quint64 {3U});
    QCOMPARE(snapshot.databaseQueueFailures, quint64 {5U});
    queue.cancelQueuedDatabaseOperation(queueGeneration);
    QCOMPARE(queue.snapshot().databaseOperationsCancelled, quint64 {3U});
    QCOMPARE(queue.snapshot().databaseQueueFailures, quint64 {5U});
    QVERIFY(!queue.tryQueueDatabaseOperation().has_value());
    QCOMPARE(queue.snapshot().databaseOperationsRejected, quint64 {3U});
    QCOMPARE(queue.snapshot().databaseQueueFailures, quint64 {6U});

    queue.beginLifecycle();
    const std::optional<quint64> nextLifecycleTicket =
        queue.tryQueueDatabaseOperation();
    QVERIFY(nextLifecycleTicket.has_value());
    QVERIFY(*nextLifecycleTicket != queueGeneration);
    QVERIFY(!queue.beginQueuedDatabaseOperation(queueGeneration));
    queue.cancelQueuedDatabaseOperation(queueGeneration);
    QCOMPARE(queue.snapshot().databaseQueueDepth, quint64 {1U});
    QCOMPARE(queue.snapshot().databaseOperationsCancelled, quint64 {3U});
    QVERIFY(queue.beginQueuedDatabaseOperation(*nextLifecycleTicket));
    queue.finishQueuedDatabaseOperation();

    SstvStoragePerformanceCounters timing;
    timing.recordImageSave(10U, true);
    timing.recordImageSave(30U, false);
    timing.recordImageSave(20U, true);
    snapshot = timing.snapshot();
    QCOMPARE(snapshot.imageSaveAttempts, quint64 {3U});
    QCOMPARE(snapshot.imageSaveSuccesses, quint64 {2U});
    QCOMPARE(snapshot.imageSaveFailures, quint64 {1U});
    QCOMPARE(snapshot.lastImageSaveNanoseconds, quint64 {20U});
    QCOMPARE(snapshot.averageImageSaveNanoseconds, quint64 {20U});
    QCOMPARE(snapshot.maximumImageSaveNanoseconds, quint64 {30U});

    SstvStoragePerformanceCounters saturated;
    saturated.recordImageSave(std::numeric_limits<quint64>::max(), true);
    saturated.recordImageSave(1U, false);
    snapshot = saturated.snapshot();
    QCOMPARE(snapshot.imageSaveAttempts, quint64 {2U});
    QCOMPARE(snapshot.averageImageSaveNanoseconds,
             std::numeric_limits<quint64>::max() / 2U);
    QCOMPARE(snapshot.maximumImageSaveNanoseconds,
             std::numeric_limits<quint64>::max());

    SstvStoragePerformanceCounters concurrent;
    constexpr std::size_t kThreads = 4U;
    constexpr int kRecordsPerThread = 1'000;
    std::array<std::thread, kThreads> threads;
    for (std::size_t threadIndex = 0U;
         threadIndex < threads.size(); ++threadIndex) {
        threads[threadIndex] = std::thread([&concurrent]() {
            for (int index = 0; index < kRecordsPerThread; ++index) {
                concurrent.recordImageSave(5U, index % 4 != 0);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    snapshot = concurrent.snapshot();
    QCOMPARE(snapshot.imageSaveAttempts, quint64 {4'000U});
    QCOMPARE(snapshot.imageSaveSuccesses, quint64 {3'000U});
    QCOMPARE(snapshot.imageSaveFailures, quint64 {1'000U});
    QCOMPARE(snapshot.lastImageSaveNanoseconds, quint64 {5U});
    QCOMPARE(snapshot.averageImageSaveNanoseconds, quint64 {5U});
    QCOMPARE(snapshot.maximumImageSaveNanoseconds, quint64 {5U});

    const QVariantMap diagnostic = snapshot.toVariantMap();
    QCOMPARE(diagnostic.size(), 15);
    for (auto iterator = diagnostic.cbegin();
         iterator != diagnostic.cend(); ++iterator) {
        const int type = iterator.value().metaType().id();
        QVERIFY2(type == QMetaType::Bool || type == QMetaType::ULongLong,
                 qPrintable(iterator.key()));
    }
}

void TestSstvStorage::workerQueueDepthAndShutdownCancellationAreMeasured()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    WorkerSession session(layout.databasePath(), layout.rootPath());
    QString error;
    QVERIFY2(session.start(&error), qPrintable(error));

    QSemaphore blockerEntered;
    QSemaphore releaseBlocker;
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [&blockerEntered, &releaseBlocker]() {
            blockerEntered.release();
            releaseBlocker.acquire();
        },
        Qt::QueuedConnection));
    QVERIFY(blockerEntered.tryAcquire(1, 5'000));

    std::atomic_int cancelledOperationsRun {0};
    QSignalSpy shutdownSpy(session.worker,
                           &SstvStorageWorker::shutdownFinished);
    QVERIFY(session.worker->enqueueDatabaseOperation(
        [](SstvStorageWorker& storage) {
            storage.shutdown();
        }));
    for (int index = 0; index < 2; ++index) {
        QVERIFY(session.worker->enqueueDatabaseOperation(
            [&cancelledOperationsRun](SstvStorageWorker&) {
                cancelledOperationsRun.fetch_add(1,
                                                   std::memory_order_relaxed);
            }));
    }

    SstvStoragePerformanceSnapshot snapshot =
        session.worker->performanceSnapshot();
    QCOMPARE(snapshot.databaseQueueDepth, quint64 {3U});
    QCOMPARE(snapshot.peakDatabaseQueueDepth, quint64 {3U});
    QCOMPARE(snapshot.databaseOperationsQueued, quint64 {3U});
    QCOMPARE(snapshot.databaseOperationsDispatched, quint64 {0U});

    releaseBlocker.release();
    QVERIFY(shutdownSpy.wait(5'000));
    QTRY_COMPARE_WITH_TIMEOUT(
        session.worker->performanceSnapshot().databaseOperationsCompleted,
        quint64 {1U}, 5'000);
    snapshot = session.worker->performanceSnapshot();
    QVERIFY(!snapshot.acceptingDatabaseOperations);
    QCOMPARE(snapshot.databaseQueueDepth, quint64 {0U});
    QCOMPARE(snapshot.databaseOperationsDispatched, quint64 {1U});
    QCOMPARE(snapshot.databaseOperationsCompleted, quint64 {1U});
    QCOMPARE(snapshot.databaseOperationsCancelled, quint64 {2U});
    QCOMPARE(snapshot.databaseQueueFailures, quint64 {2U});
    QCOMPARE(cancelledOperationsRun.load(std::memory_order_relaxed), 0);

    QVERIFY(!session.worker->enqueueDatabaseOperation(
        [](SstvStorageWorker&) {}));
    snapshot = session.worker->performanceSnapshot();
    QCOMPARE(snapshot.databaseOperationsRejected, quint64 {1U});
    QCOMPARE(snapshot.databaseQueueFailures, quint64 {3U});
}

void TestSstvStorage::standardLayoutSeparatesCategories()
{
    const SstvStorageLayout layout = SstvStorageLayout::fromStandardPaths();
    QVERIFY(!layout.rootPath().isEmpty());
    QVERIFY(QFileInfo(layout.rootPath()).isAbsolute());
    const QString appData = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QVERIFY(!appData.isEmpty());
    QCOMPARE(layout.rootPath(),
             QDir(appData).absoluteFilePath(QStringLiteral("sstv")));
    const QString received = layout.categoryRoot(SstvImageCategory::Received);
    const QString transmitted = layout.categoryRoot(SstvImageCategory::Transmitted);
    const QString imported = layout.categoryRoot(SstvImageCategory::Imported);
    const QString drafts = layout.categoryRoot(SstvImageCategory::Draft);
    const QString wavExports = layout.wavExportRoot();
    QVERIFY(received != transmitted);
    QVERIFY(received != imported);
    QVERIFY(imported != drafts);
    QVERIFY(received.startsWith(layout.rootPath()));
    QVERIFY(wavExports.startsWith(layout.rootPath()));
    QCOMPARE(QDir(layout.rootPath()).relativeFilePath(wavExports),
             QStringLiteral("exports/wav"));
    QVERIFY(layout.databasePath().startsWith(layout.rootPath()));
    QCOMPARE(QFileInfo(layout.databasePath()).fileName(),
             QStringLiteral("sstv-index.sqlite3"));
}

void TestSstvStorage::filenameSanitizationBlocksTraversalAndReservedNames()
{
    const QString hostile = SstvImageStore::sanitizeFileComponent(
        QStringLiteral(" ../../CON:<bad>|?*\\NUL. "));
    QVERIFY(!hostile.contains(QLatin1Char('/')));
    QVERIFY(!hostile.contains(QLatin1Char('\\')));
    QVERIFY(!hostile.contains(QStringLiteral("..")));
    QVERIFY(!hostile.endsWith(QLatin1Char('.')));
    QVERIFY(SstvImageStore::sanitizeFileComponent(QStringLiteral("CON"))
                .startsWith(QLatin1Char('_')));
    QVERIFY(SstvImageStore::sanitizeFileComponent(QStringLiteral("LPT9.txt"))
                .startsWith(QLatin1Char('_')));

    const QString unicode = SstvImageStore::sanitizeFileComponent(
        QStringLiteral("Málaga_日本_📻"));
    QVERIFY(unicode.contains(QStringLiteral("Málaga")));
    QVERIFY(unicode.contains(QStringLiteral("日本")));

    SstvImageRecord record;
    record.id = QStringLiteral("11111111-2222-4333-8444-555555555555");
    record.category = SstvImageCategory::Received;
    record.capturedAtUtc = utcDateTime(QDate(2026, 8, 24),
                                       QTime(12, 34, 56, 789));
    record.mode = QStringLiteral("Martin M1");
    record.remoteCallsign = QStringLiteral("../../CON");
    record.localCallsign = QStringLiteral("IU8LMC");
    record.source = QStringLiteral("TCI");
    QString rendered;
    QString error;
    QVERIFY(SstvImageStore::renderFileBase(
        QStringLiteral("{remoteCall}_{mode}_{id}"), record, 180,
        &rendered, &error));
    QVERIFY2(!rendered.contains(QLatin1Char('/')), qPrintable(rendered));
    QVERIFY(!rendered.contains(QStringLiteral("..")));
    QVERIFY(!SstvImageStore::renderFileBase(
        QStringLiteral("{unknown}"), record, 180, &rendered, &error));
    QVERIFY(!SstvImageStore::renderFileBase(
        QStringLiteral("{mode"), record, 180, &rendered, &error));
}

void TestSstvStorage::atomicPngMetadataRoundTripAndCollision()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    const QString fixedId = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received,
        utcDateTime(QDate(2026, 8, 24), QTime(10, 20, 30, 456)),
        fixedId);
    request.record.favorite = true;
    request.record.remoteCallsign = QStringLiteral("../../CON_日本");
    request.fileNameTemplate = QStringLiteral("{date}_{remoteCall}_{id}");

    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));
    QCOMPARE(saved.code, SstvStoreError::None);
    QString validationError;
    QVERIFY2(saved.record.validate({}, &validationError),
             qPrintable(validationError));
    QVERIFY(layout.containsPath(saved.record.imagePath));
    QVERIFY(layout.containsPath(saved.record.metadataPath));
    QVERIFY(QFileInfo::exists(saved.record.imagePath));
    QVERIFY(QFileInfo::exists(saved.record.metadataPath));
    QVERIFY(!QFileInfo(saved.record.imagePath).fileName().contains(QLatin1Char('/')));
    QCOMPARE(QImage(saved.record.imagePath), request.image);

    SstvImageRecord sidecar;
    QVERIFY2(SstvImageStore::loadMetadata(saved.record.metadataPath,
                                          &sidecar, &validationError),
             qPrintable(validationError));
    QCOMPARE(sidecar, saved.record);
    QVERIFY(store.verify(saved.record, true, &validationError));

    QJsonObject json = saved.record.toJson();
    SstvImageRecord roundTrip;
    QVERIFY(SstvImageRecord::fromJson(json, &roundTrip, &validationError));
    QCOMPARE(roundTrip, saved.record);
    QJsonObject legacyV3 = json;
    legacyV3.insert(QStringLiteral("schemaVersion"), 3);
    legacyV3.remove(QStringLiteral("favorite"));
    SstvImageRecord legacyV3RoundTrip;
    QVERIFY(SstvImageRecord::fromJson(legacyV3, &legacyV3RoundTrip,
                                      &validationError));
    QVERIFY(!legacyV3RoundTrip.favorite);
    QJsonObject legacyV2 = json;
    legacyV2.insert(QStringLiteral("schemaVersion"), 2);
    for (const QString& key : {
             QStringLiteral("eventAtUtc"), QStringLiteral("visValid"),
             QStringLiteral("fskId"), QStringLiteral("remoteGrid"),
             QStringLiteral("localGrid"), QStringLiteral("audioFrequencyHz"),
             QStringLiteral("sourceSampleRateHz"), QStringLiteral("digital"),
             QStringLiteral("completionPercent"),
             QStringLiteral("qualityMetrics"),
             QStringLiteral("slantCorrectionPpm"),
             QStringLiteral("rawAudioPath"), QStringLiteral("relatedQsoId"),
             QStringLiteral("remoteProvider"),
             QStringLiteral("remoteObjectId"),
             QStringLiteral("expiresAtUtc"), QStringLiteral("privacyFlags"),
             QStringLiteral("favorite"),
             QStringLiteral("thumbnailPath"), QStringLiteral("mimeType"),
             QStringLiteral("originalWidth"),
             QStringLiteral("originalHeight")}) {
        legacyV2.remove(key);
    }
    SstvImageRecord legacyRoundTrip;
    QVERIFY(SstvImageRecord::fromJson(legacyV2, &legacyRoundTrip,
                                      &validationError));
    QCOMPARE(legacyRoundTrip.eventAtUtc, legacyRoundTrip.capturedAtUtc);
    QCOMPARE(legacyRoundTrip.thumbnailPath,
             legacyRoundTrip.imagePath.left(legacyRoundTrip.imagePath.size() - 4)
                 + QStringLiteral(".thumb.png"));
    QCOMPARE(legacyRoundTrip.mimeType, QStringLiteral("image/png"));
    QCOMPARE(legacyRoundTrip.originalWidth, legacyRoundTrip.width);
    QCOMPARE(legacyRoundTrip.originalHeight, legacyRoundTrip.height);
    QCOMPARE(legacyRoundTrip.completionPercent, 100);
    QVERIFY(legacyRoundTrip.visValid);
    QVERIFY(legacyRoundTrip.qualityMetrics.isEmpty());
    QVERIFY(!legacyRoundTrip.favorite);
    json.insert(QStringLiteral("schemaVersion"), 99);
    QVERIFY(!SstvImageRecord::fromJson(json, &roundTrip, &validationError));

    QFile original(saved.record.imagePath);
    QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray originalPng = original.readAll();
    original.close();
    const SstvImageSaveResult collision = store.save(request);
    QVERIFY(!collision.ok);
    QCOMPARE(collision.code, SstvStoreError::Collision);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), originalPng);

    const QDir directory(QFileInfo(saved.record.imagePath).absolutePath());
    QVERIFY(directory.entryList(QStringList() << QStringLiteral("*.stage-*"),
                                QDir::Files | QDir::Hidden).isEmpty());
}

void TestSstvStorage::concurrentAtomicSavesNeverOverwrite()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received, QDateTime::currentDateTimeUtc(),
        QStringLiteral("12345678-1234-4abc-8def-123456789abc"));
    request.fileNameTemplate = QStringLiteral("{id}");

    SstvImageSaveResult results[2];
    std::thread first([&]() { results[0] = store.save(request); });
    std::thread second([&]() { results[1] = store.save(request); });
    first.join();
    second.join();

    const int successCount = (results[0].ok ? 1 : 0)
        + (results[1].ok ? 1 : 0);
    QCOMPARE(successCount, 1);
    const SstvImageSaveResult& winner = results[0].ok ? results[0] : results[1];
    const SstvImageSaveResult& loser = results[0].ok ? results[1] : results[0];
    QCOMPARE(loser.code, SstvStoreError::Collision);
    QString error;
    QVERIFY2(store.verify(winner.record, true, &error), qPrintable(error));
    const QDir directory(QFileInfo(winner.record.imagePath).absolutePath());
    QCOMPARE(directory.entryList(QStringList() << QStringLiteral("*.png"),
                                 QDir::Files).size(), 1);
    QCOMPARE(directory.entryList(QStringList() << QStringLiteral("*.json"),
                                 QDir::Files).size(), 1);
    QVERIFY(directory.entryList(QStringList() << QStringLiteral("*.stage-*"),
                                QDir::Files | QDir::Hidden).isEmpty());
}

void TestSstvStorage::enforcesImageLimitsAndPathContainment()
{
    QTemporaryDir temporary;
    QTemporaryDir outside;
    QVERIFY(temporary.isValid());
    QVERIFY(outside.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    QString error;
    QVERIFY(layout.ensure(&error));
    QVERIFY(QFileInfo(layout.wavExportRoot()).isDir());
    QVERIFY(layout.containsPath(layout.wavExportRoot(), true, &error));
    QVERIFY(!layout.containsPath(outside.filePath(QStringLiteral("escape.png")),
                                 false, &error));
#ifdef Q_OS_UNIX
    const QString outsideFilePath = outside.filePath(QStringLiteral("target.png"));
    QFile outsideFile(outsideFilePath);
    QVERIFY(outsideFile.open(QIODevice::WriteOnly));
    QCOMPARE(outsideFile.write("not an image"), qint64(12));
    outsideFile.close();
    const QString linkedPath = QDir(layout.categoryRoot(
        SstvImageCategory::Received)).absoluteFilePath(
            QStringLiteral("linked-outside.png"));
    QVERIFY(QFile::link(outsideFilePath, linkedPath));
    QVERIFY(!layout.containsPath(linkedPath, true, &error));
#endif

    SstvStorageLimits limits;
    limits.maximumWidth = 16;
    limits.maximumHeight = 16;
    limits.maximumPixels = 256;
    limits.maximumDecodedBytes = 2048;
    limits.maximumPngBytes = 1024;
    const SstvImageStore store(layout, limits);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Imported, QDateTime::currentDateTimeUtc());
    const SstvImageSaveResult result = store.save(request);
    QVERIFY(!result.ok);
    QCOMPARE(result.code, SstvStoreError::LimitExceeded);

    SstvImageRecord invalid;
    invalid.id = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
    invalid.category = SstvImageCategory::Received;
    invalid.capturedAtUtc = QDateTime::currentDateTimeUtc();
    invalid.createdAtUtc = invalid.capturedAtUtc;
    invalid.updatedAtUtc = invalid.capturedAtUtc;
    invalid.mode = QStringLiteral("M1");
    invalid.source = QStringLiteral("test");
    invalid.imagePath = QStringLiteral("../escape.png");
    invalid.metadataPath = QStringLiteral("../escape.json");
    invalid.sha256 = QByteArray(32, 'x');
    invalid.fileSizeBytes = 10;
    invalid.width = 1;
    invalid.height = 1;
    QVERIFY(!invalid.validate({}, &error));
}

void TestSstvStorage::workerStoresAndIndexesImageAtomicallyOffOwnerThread()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    WorkerSession session(layout.databasePath(), layout.rootPath());
    QString error;
    QVERIFY2(session.start(&error), qPrintable(error));

    QSignalSpy operationSpy(session.worker,
                            &SstvStorageWorker::operationFinished);
    QSignalSpy storeSpy(session.worker,
                        &SstvStorageWorker::imageStoreFinished);
    QSignalSpy changedSpy(session.worker,
                          &SstvStorageWorker::recordChanged);

    const QString id = QStringLiteral(
        "947cbba7-83b1-40b0-8bdb-680a612a303b");
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received,
        utcDateTime(QDate(2026, 8, 24), QTime(20, 1, 2)), id);
    request.fileNameTemplate = QStringLiteral("{date}_{time}_{id}");
    const quint64 requestId = 701;
    QVERIFY(session.worker->enqueueDatabaseOperation(
        [request](SstvStorageWorker& storage) mutable {
            storage.storeAndInsertImage(std::move(request), requestId);
        }));
    QVERIFY(storeSpy.wait(5000));
    QTRY_COMPARE_WITH_TIMEOUT(operationSpy.count(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(changedSpy.count(), 1, 5000);

    const QList<QVariant> operation = operationSpy.at(0);
    QCOMPARE(operation.at(0).toULongLong(), requestId);
    QCOMPARE(operation.at(1).value<SstvStorageOperation>(),
             SstvStorageOperation::StoreAndInsert);
    QVERIFY2(operation.at(2).toBool(),
             qPrintable(operation.at(3).toString()));
    const QList<QVariant> stored = storeSpy.takeFirst();
    QCOMPARE(stored.at(0).toULongLong(), requestId);
    QVERIFY2(stored.at(1).toBool(), qPrintable(stored.at(3).toString()));
    const SstvImageRecord storedRecord = stored.at(2).value<SstvImageRecord>();
    QCOMPARE(storedRecord.id, id);
    QCOMPARE(storedRecord, changedSpy.at(0).at(0).value<SstvImageRecord>());
    QVERIFY(QFileInfo::exists(storedRecord.imagePath));
    QVERIFY(QFileInfo::exists(storedRecord.metadataPath));
    QVERIFY(layout.containsPath(storedRecord.imagePath));
    QVERIFY(layout.containsPath(storedRecord.metadataPath));

    // A second image with the same UUID but a distinct generated path reaches
    // the SQLite uniqueness check.  Its freshly published files must be
    // removed while the first indexed image remains intact.
    SstvImageSaveRequest duplicate = request;
    duplicate.record.capturedAtUtc = duplicate.record.capturedAtUtc.addSecs(1);
    duplicate.record.createdAtUtc = duplicate.record.capturedAtUtc;
    duplicate.record.updatedAtUtc = duplicate.record.capturedAtUtc;
    const quint64 duplicateRequestId = 702;
    QVERIFY(session.worker->enqueueDatabaseOperation(
        [duplicate](SstvStorageWorker& storage) mutable {
            storage.storeAndInsertImage(std::move(duplicate),
                                        duplicateRequestId);
        }));
    QVERIFY(storeSpy.wait(5000));
    QTRY_COMPARE_WITH_TIMEOUT(operationSpy.count(), 2, 5000);
    const QList<QVariant> duplicateResult = storeSpy.takeFirst();
    QCOMPARE(duplicateResult.at(0).toULongLong(), duplicateRequestId);
    QVERIFY(!duplicateResult.at(1).toBool());
    QVERIFY(!duplicateResult.at(3).toString().isEmpty());
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(QFileInfo::exists(storedRecord.imagePath));
    QVERIFY(QFileInfo::exists(storedRecord.metadataPath));
    const QDir receivedDirectory(QFileInfo(storedRecord.imagePath).absolutePath());
    QCOMPARE(receivedDirectory.entryList(
                 QStringList {QStringLiteral("*.png")}, QDir::Files).size(),
             1);
    QCOMPARE(receivedDirectory.entryList(
                 QStringList {QStringLiteral("*.json")}, QDir::Files).size(),
             1);

    const quint64 invalidRequestId = 703;
    QVERIFY(session.worker->enqueueDatabaseOperation(
        [](SstvStorageWorker& storage) {
            storage.storeAndInsertImage({}, invalidRequestId);
        }));
    QVERIFY(storeSpy.wait(5000));
    QTRY_COMPARE_WITH_TIMEOUT(operationSpy.count(), 3, 5000);
    const QList<QVariant> invalidResult = storeSpy.takeFirst();
    QCOMPARE(invalidResult.at(0).toULongLong(), invalidRequestId);
    QVERIFY(!invalidResult.at(1).toBool());

    QSignalSpy fetchSpy(session.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(session.worker->enqueueDatabaseOperation(
        [id](SstvStorageWorker& storage) {
            storage.fetchRecord(id, 704);
        }));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), storedRecord);

    QTRY_COMPARE_WITH_TIMEOUT(
        session.worker->performanceSnapshot().databaseOperationsCompleted,
        quint64 {4U}, 5000);
    const SstvStoragePerformanceSnapshot performance =
        session.worker->performanceSnapshot();
    QCOMPARE(performance.databaseQueueDepth, quint64 {0U});
    QCOMPARE(performance.peakDatabaseQueueDepth, quint64 {1U});
    QCOMPARE(performance.databaseOperationsQueued, quint64 {4U});
    QCOMPARE(performance.databaseOperationsDispatched, quint64 {4U});
    QCOMPARE(performance.databaseQueueFailures, quint64 {0U});
    QCOMPARE(performance.imageSaveAttempts, quint64 {3U});
    QCOMPARE(performance.imageSaveSuccesses, quint64 {2U});
    QCOMPARE(performance.imageSaveFailures, quint64 {1U});
    QVERIFY(performance.averageImageSaveNanoseconds
            <= performance.maximumImageSaveNanoseconds);
    QVERIFY(performance.lastImageSaveNanoseconds
            <= performance.maximumImageSaveNanoseconds);
}

void TestSstvStorage::fullMetadataSqliteRoundTrip()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    const QDateTime event = utcDateTime(QDate(2026, 8, 23),
                                        QTime(22, 17, 45, 123));
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Imported, event,
        QStringLiteral("46a5a75d-24cb-4218-8b89-ad2b0ae01edf"));
    request.record.visValid = false;
    request.record.digital = true;
    request.record.complete = false;
    request.record.completionPercent = 73;
    request.record.audioFrequencyHz = -1'250;
    request.record.sourceSampleRateHz = 96'000;
    request.record.originalWidth = 64;
    request.record.originalHeight = 48;
    request.record.qualityMetrics = {
        {QStringLiteral("snrDb"), 7.75},
        {QStringLiteral("syncConfidence"), 0.625},
        {QStringLiteral("lineDropRate"), 0.03125}
    };
    request.record.slantCorrectionPpm = 42.125;
    request.record.relatedQsoId = QStringLiteral("qso-native-73");
    request.record.remoteProvider = QStringLiteral("operator-webdav");
    request.record.remoteObjectId = QStringLiteral("objects/46a5a75d.png");
    request.record.expiresAtUtc = event.addDays(30);
    request.record.privacyFlags = 0xa5U;
    request.record.rawAudioPath = QDir(layout.datedCategoryDirectory(
        request.record.category, event.date())).absoluteFilePath(
            QStringLiteral("46a5a75d-source.wav"));
    request.fileNameTemplate = QStringLiteral("{id}");

    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));
    QVERIFY(!QFileInfo::exists(saved.record.thumbnailPath));
    QVERIFY(!QFileInfo::exists(saved.record.rawAudioPath));
    QCOMPARE(saved.record.mimeType, QStringLiteral("image/png"));
    QCOMPARE(saved.record.eventAtUtc, event);
    QCOMPARE(saved.record.completionPercent, 73);
    QCOMPARE(saved.record.originalWidth, 64);
    QCOMPARE(saved.record.originalHeight, 48);

    WorkerSession session(layout.databasePath(), layout.rootPath());
    QString error;
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy operationSpy(session.worker,
                            &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 801);
        }, Qt::QueuedConnection));
    QVERIFY(operationSpy.wait(5000));
    QVERIFY2(operationSpy.constFirst().at(2).toBool(),
             qPrintable(operationSpy.constFirst().at(3).toString()));

    QSignalSpy fetchSpy(session.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, id = saved.record.id]() {
            worker->fetchRecord(id, 802);
        }, Qt::QueuedConnection));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), saved.record);
}

void TestSstvStorage::indexedRecordSurvivesMissingImageAndThumbnail()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received,
        utcDateTime(QDate(2026, 8, 24), QTime(23, 59, 1)));
    request.fileNameTemplate = QStringLiteral("{id}");
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));
    QImage thumbnail(8, 8, QImage::Format_RGB32);
    thumbnail.fill(Qt::darkBlue);
    QVERIFY(thumbnail.save(saved.record.thumbnailPath, "PNG"));
    QVERIFY(QFileInfo::exists(saved.record.thumbnailPath));

    WorkerSession session(layout.databasePath(), layout.rootPath());
    QString error;
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy operationSpy(session.worker,
                            &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 811);
        }, Qt::QueuedConnection));
    QVERIFY(operationSpy.wait(5000));
    QVERIFY2(operationSpy.constFirst().at(2).toBool(),
             qPrintable(operationSpy.constFirst().at(3).toString()));

    QVERIFY(QFile::remove(saved.record.imagePath));
    QVERIFY(QFile::remove(saved.record.thumbnailPath));
    QVERIFY(!QFileInfo::exists(saved.record.imagePath));
    QVERIFY(!QFileInfo::exists(saved.record.thumbnailPath));

    QSignalSpy fetchSpy(session.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, id = saved.record.id]() {
            worker->fetchRecord(id, 812);
        }, Qt::QueuedConnection));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), saved.record);

    QSignalSpy pageSpy(session.worker, &SstvStorageWorker::pageFetched);
    SstvImagePageRequest pageRequest;
    pageRequest.limit = 10;
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, pageRequest]() {
            worker->listRecords(pageRequest, 813);
        }, Qt::QueuedConnection));
    QVERIFY(pageSpy.wait(5000));
    QVERIFY2(pageSpy.constFirst().at(2).toString().isEmpty(),
             qPrintable(pageSpy.constFirst().at(2).toString()));
    const SstvImagePage page = pageSpy.constFirst().at(1).value<SstvImagePage>();
    QCOMPARE(page.records.size(), 1);
    QCOMPARE(page.records.constFirst(), saved.record);
}

void TestSstvStorage::workerCrudKeysetPaginationAndConcurrentRead()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    std::vector<SstvImageRecord> records;
    for (int index = 0; index < 8; ++index) {
        SstvImageSaveRequest request = makeRequest(
            index % 2 == 0 ? SstvImageCategory::Received
                           : SstvImageCategory::Transmitted,
            utcDateTime(QDate(2026, 8, 24), QTime(12, 0))
                .addSecs(index));
        request.fileNameTemplate = QStringLiteral("{id}");
        const SstvImageSaveResult saved = store.save(request);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        records.push_back(saved.record);
    }

    WorkerSession session(layout.databasePath(), layout.rootPath());
    QString error;
    QVERIFY2(session.start(&error), qPrintable(error));
    QCOMPARE(session.schemaVersion, SstvStorageWorker::kCurrentSchemaVersion);
    QVERIFY(session.workerThreadToken != 0);
    QVERIFY(session.workerThreadToken
            != reinterpret_cast<quintptr>(QThread::currentThreadId()));

    QSignalSpy violationSpy(session.worker,
                            &SstvStorageWorker::threadOwnershipViolation);
    session.worker->fetchRecord(records.front().id, 9999);
    QCOMPARE(violationSpy.count(), 1);

    QSignalSpy operations(session.worker,
                          &SstvStorageWorker::operationFinished);
    const QString concurrentReadConnection = uniqueConnectionName(
        QStringLiteral("sstv_live_reader"));
    std::atomic<bool> readerStarted {false};
    std::atomic<bool> stopReader {false};
    std::atomic<bool> readerOk {true};
    std::atomic<int> readerIterations {0};
    QString readerError;
    std::thread reader([&]() {
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), concurrentReadConnection);
            database.setDatabaseName(layout.databasePath());
            if (!database.open()) {
                readerOk.store(false);
                readerError = database.lastError().text();
                readerStarted.store(true);
            } else {
                QSqlQuery timeout(database);
                if (!timeout.exec(QStringLiteral("PRAGMA busy_timeout=5000"))) {
                    readerOk.store(false);
                    readerError = timeout.lastError().text();
                }
                readerStarted.store(true);
                while (readerOk.load() && !stopReader.load()) {
                    QSqlQuery count(database);
                    if (!count.exec(QStringLiteral(
                            "SELECT COUNT(*) FROM sstv_images"))
                        || !count.next()) {
                        readerOk.store(false);
                        readerError = count.lastError().text();
                        break;
                    }
                    const int rows = count.value(0).toInt();
                    if (rows < 0
                        || rows > static_cast<int>(records.size())) {
                        readerOk.store(false);
                        readerError = QStringLiteral(
                            "concurrent reader observed an invalid row count");
                        break;
                    }
                    readerIterations.fetch_add(1);
                    QThread::yieldCurrentThread();
                }
                database.close();
            }
            database = QSqlDatabase();
        }
        QSqlDatabase::removeDatabase(concurrentReadConnection);
    });
    QElapsedTimer readerStartTimer;
    readerStartTimer.start();
    while (!readerStarted.load() && readerStartTimer.elapsed() < 5000) {
        QTest::qWait(1);
    }
    std::atomic<bool> allQueued {true};
    std::vector<std::thread> producers;
    for (std::size_t index = 0; index < records.size(); ++index) {
        producers.emplace_back([&, index]() {
            const bool queued = QMetaObject::invokeMethod(
                session.worker,
                [worker = session.worker, record = records.at(index), index]() {
                    worker->insertRecord(record,
                        static_cast<quint64>(index + 1U));
                }, Qt::QueuedConnection);
            if (!queued) {
                allQueued.store(false);
            }
        });
    }
    for (std::thread& producer : producers) {
        producer.join();
    }
    QElapsedTimer writeTimer;
    writeTimer.start();
    while (operations.count() < static_cast<int>(records.size())
           && writeTimer.elapsed() < 10000) {
        QTest::qWait(1);
    }
    stopReader.store(true);
    reader.join();
    QVERIFY(readerStarted.load());
    QVERIFY(allQueued.load());
    QCOMPARE(operations.count(), static_cast<int>(records.size()));
    QVERIFY2(readerOk.load(), qPrintable(readerError));
    QVERIFY(readerIterations.load() > 0);
    for (const QList<QVariant>& arguments : operations) {
        QVERIFY2(arguments.at(2).toBool(),
                 qPrintable(arguments.at(3).toString()));
    }

    // The worker keeps WAL enabled, so a distinct named connection can read
    // concurrently without sharing the worker's QSqlDatabase object.
    const QString readConnection = uniqueConnectionName(
        QStringLiteral("sstv_concurrent_read_verify"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), readConnection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery count(database);
        QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM sstv_images")));
        QVERIFY(count.next());
        QCOMPARE(count.value(0).toInt(), static_cast<int>(records.size()));
        QSqlQuery schema(database);
        QVERIFY(schema.exec(QStringLiteral(
            "SELECT sql FROM sqlite_master WHERE name='sstv_images'")));
        QVERIFY(schema.next());
        QVERIFY(!schema.value(0).toString().contains(
            QStringLiteral("BLOB"), Qt::CaseInsensitive));
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(readConnection);

    QSignalSpy pageSpy(session.worker, &SstvStorageWorker::pageFetched);
    SstvImagePageRequest firstPage;
    firstPage.limit = 3;
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, firstPage]() {
            worker->listRecords(firstPage, 101);
        }, Qt::QueuedConnection));
    QVERIFY(pageSpy.wait(5000));
    QList<QVariant> pageArguments = pageSpy.takeFirst();
    QVERIFY2(pageArguments.at(2).toString().isEmpty(),
             qPrintable(pageArguments.at(2).toString()));
    const SstvImagePage pageOne = pageArguments.at(1).value<SstvImagePage>();
    QCOMPARE(pageOne.records.size(), 3);
    QVERIFY(pageOne.hasMore);
    QVERIFY(pageOne.records.at(0).capturedAtUtc
            > pageOne.records.at(1).capturedAtUtc);

    SstvImagePageRequest secondPage;
    secondPage.limit = 3;
    secondPage.hasCursor = true;
    secondPage.beforeCapturedAtMs = pageOne.nextBeforeCapturedAtMs;
    secondPage.beforeId = pageOne.nextBeforeId;
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, secondPage]() {
            worker->listRecords(secondPage, 102);
        }, Qt::QueuedConnection));
    QVERIFY(pageSpy.wait(5000));
    pageArguments = pageSpy.takeFirst();
    const SstvImagePage pageTwo = pageArguments.at(1).value<SstvImagePage>();
    QCOMPARE(pageTwo.records.size(), 3);
    QSet<QString> ids;
    for (const SstvImageRecord& record : pageOne.records) {
        ids.insert(record.id);
    }
    for (const SstvImageRecord& record : pageTwo.records) {
        QVERIFY(!ids.contains(record.id));
    }

    SstvImageRecord updated = records.back();
    updated.note = QStringLiteral("updated atomically");
    updated.updatedAtUtc = QDateTime::currentDateTimeUtc().addSecs(1);
    QVERIFY2(store.updateMetadata(updated, &error), qPrintable(error));
    const qsizetype operationBeforeUpdate = operations.count();
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, updated]() {
            worker->updateRecord(updated, 201);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(operations.count(), operationBeforeUpdate + 1,
                              5000);
    QVERIFY(operations.constLast().at(2).toBool());

    QSignalSpy fetchSpy(session.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, id = updated.id]() {
            worker->fetchRecord(id, 202);
        }, Qt::QueuedConnection));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY(fetched.at(1).toBool());
    const SstvImageRecord fetchedRecord =
        fetched.at(2).value<SstvImageRecord>();
    QCOMPARE(fetchedRecord, updated);

    const qsizetype operationBeforeRemove = operations.count();
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, id = updated.id]() {
            worker->removeRecord(id, 203);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(operations.count(), operationBeforeRemove + 1,
                              5000);
    QVERIFY(operations.constLast().at(2).toBool());
    const QString workerConnection = session.worker->connectionName();
    session.stop();
    QVERIFY(!QSqlDatabase::contains(workerConnection));
}

void TestSstvStorage::deletionJournalRecoversBeforeAndAfterDatabaseCommit()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received,
        utcDateTime(QDate(2026, 8, 24), QTime(20, 10, 0)));
    request.fileNameTemplate = QStringLiteral("{id}");
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    QString error;
    {
        WorkerSession session(layout.databasePath(), layout.rootPath());
        QVERIFY2(session.start(&error), qPrintable(error));
        QSignalSpy operation(session.worker,
                             &SstvStorageWorker::operationFinished);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker,
            [worker = session.worker, record = saved.record]() mutable {
                worker->insertRecord(std::move(record), 700);
        }, Qt::QueuedConnection));
        QVERIFY(operation.wait(5'000));
        const QList<QVariant> inserted = operation.takeFirst();
        QVERIFY2(inserted.at(2).toBool(),
                 qPrintable(inserted.value(3).toString()));
        session.stop();
    }

    const QString stagingRoot = QDir(layout.rootPath()).absoluteFilePath(
        QStringLiteral(".delete-staging"));
    const auto stageFiles = [&](const QString& stageName) {
        const QString stagePath = QDir(stagingRoot).absoluteFilePath(stageName);
        if (!QDir().mkpath(stagePath)) {
            return false;
        }
        const QString stagedImage = QDir(stagePath).absoluteFilePath(
            QStringLiteral("0000-image.png"));
        const QString stagedMetadata = QDir(stagePath).absoluteFilePath(
            QStringLiteral("0001-metadata.json"));
        QJsonObject journal {
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("recordIds"), QJsonArray {saved.record.id}},
            {QStringLiteral("files"), QJsonArray {
                QJsonObject {
                    {QStringLiteral("original"), saved.record.imagePath},
                    {QStringLiteral("staged"),
                     QFileInfo(stagedImage).fileName()},
                },
                QJsonObject {
                    {QStringLiteral("original"), saved.record.metadataPath},
                    {QStringLiteral("staged"),
                     QFileInfo(stagedMetadata).fileName()},
                },
            }},
        };
        QFile file(QDir(stagePath).absoluteFilePath(
            QStringLiteral("journal.json")));
        const QByteArray encoded = QJsonDocument(journal).toJson(
            QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly)
            || file.write(encoded) != encoded.size()) {
            return false;
        }
        file.close();
        return QFile::rename(saved.record.imagePath, stagedImage)
            && QFile::rename(saved.record.metadataPath, stagedMetadata);
    };

    const QString beforeCommitStage = QStringLiteral(
        "701-11111111-2222-4333-8444-555555555555");
    QVERIFY(stageFiles(beforeCommitStage));
    QVERIFY(!QFileInfo::exists(saved.record.imagePath));
    QVERIFY(!QFileInfo::exists(saved.record.metadataPath));
    {
        WorkerSession recovered(layout.databasePath(), layout.rootPath());
        QVERIFY2(recovered.start(&error), qPrintable(error));
        QVERIFY(QFileInfo::exists(saved.record.imagePath));
        QVERIFY(QFileInfo::exists(saved.record.metadataPath));
        QVERIFY(!QFileInfo::exists(
            QDir(stagingRoot).absoluteFilePath(beforeCommitStage)));
        QSignalSpy fetched(recovered.worker,
                           &SstvStorageWorker::recordFetched);
        QVERIFY(QMetaObject::invokeMethod(
            recovered.worker,
            [worker = recovered.worker, id = saved.record.id]() {
                worker->fetchRecord(id, 702);
            }, Qt::QueuedConnection));
        QVERIFY(fetched.wait(5'000));
        QVERIFY(fetched.takeFirst().at(1).toBool());
        recovered.stop();
    }

    const QString afterCommitStage = QStringLiteral(
        "703-aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
    QVERIFY(stageFiles(afterCommitStage));
    const QString connection = uniqueConnectionName(
        QStringLiteral("delete_recovery_commit"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery remove(database);
        remove.prepare(QStringLiteral("DELETE FROM sstv_images WHERE id=:id"));
        remove.bindValue(QStringLiteral(":id"), saved.record.id);
        QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));
        QCOMPARE(remove.numRowsAffected(), 1);
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connection);
    {
        WorkerSession recovered(layout.databasePath(), layout.rootPath());
        QVERIFY2(recovered.start(&error), qPrintable(error));
        QVERIFY(!QFileInfo::exists(saved.record.imagePath));
        QVERIFY(!QFileInfo::exists(saved.record.metadataPath));
        QVERIFY(!QFileInfo::exists(
            QDir(stagingRoot).absoluteFilePath(afterCommitStage)));
        recovered.stop();
    }
    QVERIFY(QDir(stagingRoot).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
}

void TestSstvStorage::favoritePersistsInSchemaSidecarAndRestart()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY2(layout.ensure(&error), qPrintable(error));
    SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received,
        QDateTime::currentDateTimeUtc().addDays(-3));
    request.record.relatedQsoId.clear();
    request.record.uploadState = SstvUploadState::NotRequested;
    request.record.remoteProvider.clear();
    request.record.remoteObjectId.clear();
    request.record.expiresAtUtc = {};
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));
    QVERIFY(!saved.record.favorite);

    SstvImageRecord updated;
    {
        WorkerSession session(layout.databasePath(), layout.rootPath());
        QVERIFY2(session.start(&error), qPrintable(error));
        QCOMPARE(session.schemaVersion, 5);
        QSignalSpy operation(session.worker,
                             &SstvStorageWorker::operationFinished);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker,
            [worker = session.worker, record = saved.record]() {
                worker->insertRecord(record, 501);
            }, Qt::QueuedConnection));
        QTRY_VERIFY_WITH_TIMEOUT(operation.count() >= 1, 5'000);
        QCOMPARE(operation.takeFirst().at(2).toBool(), true);

        QSignalSpy changed(session.worker, &SstvStorageWorker::recordChanged);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker,
            [worker = session.worker, id = saved.record.id]() {
                worker->setFavorite(id, true, 502);
            }, Qt::QueuedConnection));
        QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 5'000);
        updated = changed.takeLast().at(0).value<SstvImageRecord>();
        QVERIFY(updated.favorite);
        QVERIFY(updated.updatedAtUtc > saved.record.updatedAtUtc);

        QSignalSpy settingsSpy(
            session.worker, &SstvStorageWorker::retentionSettingsLoaded);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker, &SstvStorageWorker::loadRetentionSettings,
            Qt::QueuedConnection));
        QTRY_COMPARE_WITH_TIMEOUT(settingsSpy.count(), 1, 5'000);
        const SstvRetentionSettings settings =
            settingsSpy.takeFirst().at(0).value<SstvRetentionSettings>();
        QVERIFY(!settings.automaticEnabled);
        QCOMPARE(settings.sharedPolicy,
                 SstvSharedRetentionPolicy::Protect);
    }

    SstvImageRecord sidecar;
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QVERIFY(sidecar.favorite);
    QCOMPARE(sidecar, updated);

    {
        WorkerSession restarted(layout.databasePath(), layout.rootPath());
        QVERIFY2(restarted.start(&error), qPrintable(error));
        QSignalSpy fetched(restarted.worker,
                           &SstvStorageWorker::recordFetched);
        QVERIFY(QMetaObject::invokeMethod(
            restarted.worker,
            [worker = restarted.worker, id = saved.record.id]() {
                worker->fetchRecord(id, 503);
            }, Qt::QueuedConnection));
        QTRY_COMPARE_WITH_TIMEOUT(fetched.count(), 1, 5'000);
        const QList<QVariant> result = fetched.takeFirst();
        QVERIFY(result.at(1).toBool());
        QVERIFY(result.at(2).value<SstvImageRecord>().favorite);
    }
}

void TestSstvStorage::qsoAssociationPersistsAndProtectsRetention()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY2(layout.ensure(&error), qPrintable(error));
    SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received,
        QDateTime::currentDateTimeUtc().addDays(-30));
    request.record.relatedQsoId.clear();
    request.record.favorite = false;
    request.record.uploadState = SstvUploadState::NotRequested;
    request.record.remoteProvider.clear();
    request.record.remoteObjectId.clear();
    request.record.expiresAtUtc = {};
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    WorkerSession session(layout.databasePath(), layout.rootPath());
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy insertSpy(session.worker,
                         &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 601);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(insertSpy.count(), 1, 5'000);
    QVERIFY2(insertSpy.takeFirst().at(2).toBool(),
             "fixture insert failed");

    const QString qsoId = QStringLiteral("adif-sha256:")
        + QString(64, QLatin1Char('a'));
    QList<QVariant> association = associateWithQsoAndWait(
        session.worker, saved.record.id, qsoId, 602);
    QVERIFY(!association.isEmpty());
    QCOMPARE(association.at(1).value<SstvStorageOperation>(),
             SstvStorageOperation::AssociateQso);
    QVERIFY2(association.at(2).toBool(),
             qPrintable(association.at(3).toString()));

    SstvImageRecord sidecar;
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar.relatedQsoId, qsoId);
    QVERIFY(sidecar.updatedAtUtc > saved.record.updatedAtUtc);

    QList<QVariant> fetched = fetchRecordAndWait(
        session.worker, saved.record.id, 603);
    QVERIFY(!fetched.isEmpty());
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    QCOMPARE(fetched.at(2).value<SstvImageRecord>().relatedQsoId, qsoId);
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), sidecar);

    SstvRetentionSettings policy;
    policy.maximumAgeDays = 1;
    policy.maximumDeletesPerRun = 10;
    QSignalSpy protectedPreview(
        session.worker, &SstvStorageWorker::retentionPreviewReady);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, policy]() {
            worker->previewRetention(policy, 604);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(protectedPreview.count(), 1, 5'000);
    const QList<QVariant> protectedResult = protectedPreview.takeFirst();
    QVERIFY2(protectedResult.at(2).toString().isEmpty(),
             qPrintable(protectedResult.at(2).toString()));
    const SstvRetentionPlan protectedPlan =
        protectedResult.at(1).value<SstvRetentionPlan>();
    QVERIFY(protectedPlan.recordIds.isEmpty());
    QCOMPARE(protectedPlan.protectedQsoCount, 1);

    const QList<QPair<QString, QString>> invalidInputs {
        {saved.record.id, QStringLiteral("../qso")},
        {saved.record.id, QStringLiteral("qso\ncontrol")},
        {saved.record.id, QString(257, QLatin1Char('x'))},
        {QStringLiteral("not-a-canonical-uuid"), qsoId}
    };
    quint64 invalidRequestId = 605;
    for (const auto& invalid : invalidInputs) {
        association = associateWithQsoAndWait(
            session.worker, invalid.first, invalid.second,
            invalidRequestId++);
        QVERIFY(!association.isEmpty());
        QVERIFY(!association.at(2).toBool());
        QVERIFY(!association.at(3).toString().isEmpty());
    }
    const QString missingId = QUuid::createUuid().toString(
        QUuid::WithoutBraces);
    association = associateWithQsoAndWait(
        session.worker, missingId, qsoId, invalidRequestId++);
    QVERIFY(!association.isEmpty());
    QVERIFY(!association.at(2).toBool());
    QVERIFY(association.at(3).toString().contains(
        QStringLiteral("not found"), Qt::CaseInsensitive));

    fetched = fetchRecordAndWait(session.worker, saved.record.id,
                                 invalidRequestId++);
    QVERIFY(!fetched.isEmpty());
    QVERIFY(fetched.at(1).toBool());
    QCOMPARE(fetched.at(2).value<SstvImageRecord>().relatedQsoId, qsoId);
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar.relatedQsoId, qsoId);

    QVERIFY2(executeStandaloneSql(
                 layout.databasePath(),
                 QStringLiteral(
                     "CREATE TRIGGER sstv_test_fail_qso_update "
                     "BEFORE UPDATE OF related_qso_id ON sstv_images "
                     "BEGIN SELECT RAISE(ABORT,'forced QSO update failure'); "
                     "END"),
                 &error),
             qPrintable(error));
    const QString rejectedQsoId = QStringLiteral("adif-sha256:")
        + QString(64, QLatin1Char('b'));
    association = associateWithQsoAndWait(
        session.worker, saved.record.id, rejectedQsoId,
        invalidRequestId++);
    QVERIFY(!association.isEmpty());
    QVERIFY(!association.at(2).toBool());
    QVERIFY2(executeStandaloneSql(
                 layout.databasePath(),
                 QStringLiteral("DROP TRIGGER sstv_test_fail_qso_update"),
                 &error),
             qPrintable(error));
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar.relatedQsoId, qsoId);
    fetched = fetchRecordAndWait(session.worker, saved.record.id,
                                 invalidRequestId++);
    QVERIFY(!fetched.isEmpty());
    QVERIFY(fetched.at(1).toBool());
    QCOMPARE(fetched.at(2).value<SstvImageRecord>().relatedQsoId, qsoId);

    association = associateWithQsoAndWait(
        session.worker, saved.record.id, {}, invalidRequestId++);
    QVERIFY(!association.isEmpty());
    QVERIFY2(association.at(2).toBool(),
             qPrintable(association.at(3).toString()));
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QVERIFY(sidecar.relatedQsoId.isEmpty());
    fetched = fetchRecordAndWait(session.worker, saved.record.id,
                                 invalidRequestId++);
    QVERIFY(!fetched.isEmpty());
    QVERIFY(fetched.at(1).toBool());
    QVERIFY(fetched.at(2).value<SstvImageRecord>().relatedQsoId.isEmpty());
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), sidecar);

    QSignalSpy unprotectedPreview(
        session.worker, &SstvStorageWorker::retentionPreviewReady);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, policy, invalidRequestId]() {
            worker->previewRetention(policy, invalidRequestId);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(unprotectedPreview.count(), 1, 5'000);
    const QList<QVariant> unprotectedResult = unprotectedPreview.takeFirst();
    QVERIFY2(unprotectedResult.at(2).toString().isEmpty(),
             qPrintable(unprotectedResult.at(2).toString()));
    const SstvRetentionPlan unprotectedPlan =
        unprotectedResult.at(1).value<SstvRetentionPlan>();
    QCOMPARE(unprotectedPlan.recordIds, QStringList {saved.record.id});
    QCOMPARE(unprotectedPlan.protectedQsoCount, 0);
}

void TestSstvStorage::userMetadataPersistsAndRestoresOnDatabaseFailure()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY2(layout.ensure(&error), qPrintable(error));
    SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Received, QDateTime::currentDateTimeUtc());
    request.record.note = QStringLiteral("initial Gallery note");
    request.record.tags = {QStringLiteral("initial"),
                           QStringLiteral("remove")};
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    WorkerSession session(layout.databasePath(), layout.rootPath());
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy inserted(session.worker,
                        &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 670);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(inserted.count(), 1, 5'000);
    QVERIFY2(inserted.takeFirst().at(2).toBool(), "fixture insert failed");

    const QString expectedNote = QStringLiteral("Portable operator note\n"
                                                "with a line break");
    const QString expectedAccent = QString::fromUtf8("M\xC3\xA1laga");
    const QList<QVariant> updated = updateUserMetadataAndWait(
        session.worker, saved.record.id, expectedNote,
        {QStringLiteral(" portable "),
         QString::fromUtf8("Ma\xCC\x81laga")}, 671);
    QVERIFY(!updated.isEmpty());
    QCOMPARE(updated.at(1).value<SstvStorageOperation>(),
             SstvStorageOperation::UpdateUserMetadata);
    QVERIFY2(updated.at(2).toBool(), qPrintable(updated.at(3).toString()));

    SstvImageRecord sidecar;
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar.note, expectedNote);
    QCOMPARE(sidecar.tags,
             QStringList({QStringLiteral("portable"), expectedAccent}));
    QVERIFY(sidecar.updatedAtUtc > saved.record.updatedAtUtc);

    QList<QVariant> fetched = fetchRecordAndWait(
        session.worker, saved.record.id, 672);
    QVERIFY(!fetched.isEmpty());
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), sidecar);

    QStringList indexedTags;
    QVERIFY2(readStoredTags(layout.databasePath(), saved.record.id,
                             &indexedTags, &error),
             qPrintable(error));
    QCOMPARE(indexedTags,
             QStringList({expectedAccent, QStringLiteral("portable")}));

    const QList<QPair<QString, QStringList>> invalidInputs {
        {QString(4'097, QLatin1Char('n')), {}},
        {expectedNote, {QStringLiteral("Field"), QStringLiteral("field")}}
    };
    quint64 invalidRequestId = 673;
    for (const auto& invalid : invalidInputs) {
        const QList<QVariant> rejected = updateUserMetadataAndWait(
            session.worker, saved.record.id, invalid.first, invalid.second,
            invalidRequestId++);
        QVERIFY(!rejected.isEmpty());
        QVERIFY(!rejected.at(2).toBool());
        QVERIFY(!rejected.at(3).toString().isEmpty());
    }
    const QList<QVariant> invalidId = updateUserMetadataAndWait(
        session.worker, QStringLiteral("not-a-canonical-uuid"), expectedNote,
        {QStringLiteral("portable")}, invalidRequestId++);
    QVERIFY(!invalidId.isEmpty());
    QVERIFY(!invalidId.at(2).toBool());

    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar.note, expectedNote);
    QCOMPARE(sidecar.tags,
             QStringList({QStringLiteral("portable"), expectedAccent}));
    fetched = fetchRecordAndWait(session.worker, saved.record.id,
                                 invalidRequestId++);
    QVERIFY(!fetched.isEmpty());
    QVERIFY(fetched.at(1).toBool());
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), sidecar);

    QVERIFY2(executeStandaloneSql(
                 layout.databasePath(),
                 QStringLiteral(
                     "CREATE TRIGGER sstv_test_fail_user_metadata_update "
                     "BEFORE UPDATE OF note ON sstv_images "
                     "BEGIN SELECT RAISE(ABORT,'forced metadata update failure'); "
                     "END"),
                 &error),
             qPrintable(error));
    const QList<QVariant> databaseRejected = updateUserMetadataAndWait(
        session.worker, saved.record.id, QStringLiteral("must not persist"),
        {QStringLiteral("rejected")}, invalidRequestId++);
    QVERIFY(!databaseRejected.isEmpty());
    QVERIFY(!databaseRejected.at(2).toBool());
    QVERIFY2(executeStandaloneSql(
                 layout.databasePath(),
                 QStringLiteral("DROP TRIGGER sstv_test_fail_user_metadata_update"),
                 &error),
             qPrintable(error));

    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar.note, expectedNote);
    QCOMPARE(sidecar.tags,
             QStringList({QStringLiteral("portable"), expectedAccent}));
    fetched = fetchRecordAndWait(session.worker, saved.record.id,
                                 invalidRequestId++);
    QVERIFY(!fetched.isEmpty());
    QVERIFY(fetched.at(1).toBool());
    QCOMPARE(fetched.at(2).value<SstvImageRecord>(), sidecar);
    QVERIFY2(readStoredTags(layout.databasePath(), saved.record.id,
                             &indexedTags, &error),
             qPrintable(error));
    QCOMPARE(indexedTags,
             QStringList({expectedAccent, QStringLiteral("portable")}));
}

void TestSstvStorage::quotaRetentionProtectionManualApplyAndAutoOptIn()
{
    SstvRetentionSettings invalidPolicy;
    invalidPolicy.imageQuotaBytes =
        SstvRetentionSettings::kMaximumQuotaBytes + 1;
    QString boundsError;
    QVERIFY(!invalidPolicy.validate(&boundsError));
    invalidPolicy = {};
    invalidPolicy.maximumDeletesPerRun = 501;
    QVERIFY(!invalidPolicy.validate(&boundsError));
    QVariantMap hostileSettings = SstvRetentionSettings {}.toVariantMap();
    hostileSettings.insert(QStringLiteral("unknown"), 1);
    QVERIFY(!SstvRetentionSettings::fromVariantMap(
        hostileSettings, &invalidPolicy, &boundsError));

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY2(layout.ensure(&error), qPrintable(error));
    SstvImageStore store(layout);
    WorkerSession session(layout.databasePath(), layout.rootPath());
    QVERIFY2(session.start(&error), qPrintable(error));

    struct Materialized final
    {
        SstvImageRecord record;
        QStringList ownedPaths;
    };
    const auto materialize = [&](int index,
                                 bool favorite,
                                 bool qso,
                                 bool shared,
                                 Materialized* output) {
        QVERIFY(output != nullptr);
        const QString id = QUuid::createUuid().toString(
            QUuid::WithoutBraces);
        SstvImageSaveRequest request = makeRequest(
            SstvImageCategory::Received,
            QDateTime::currentDateTimeUtc().addDays(-30 - index), id);
        request.record.favorite = favorite;
        request.record.relatedQsoId = qso
            ? QStringLiteral("qso-protected-%1").arg(index) : QString {};
        request.record.uploadState = shared ? SstvUploadState::Uploaded
                                            : SstvUploadState::NotRequested;
        request.record.remoteProvider = shared
            ? QStringLiteral("provider") : QString {};
        request.record.remoteObjectId = shared
            ? QStringLiteral("remote-%1").arg(index) : QString {};
        request.record.expiresAtUtc = {};
        QString directory;
        QVERIFY(layout.ensureDatedCategoryDirectory(
            request.record.category, request.record.capturedAtUtc.date(),
            &directory, &error));
        request.record.rawAudioPath = QDir(directory).absoluteFilePath(
            id + QStringLiteral(".wav"));
        const SstvImageSaveResult saved = store.save(request);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        QImage thumbnail(8, 6, QImage::Format_RGB32);
        thumbnail.fill(Qt::darkCyan);
        QVERIFY(thumbnail.save(saved.record.thumbnailPath, "PNG"));
        QFile raw(saved.record.rawAudioPath);
        QVERIFY(raw.open(QIODevice::WriteOnly));
        QCOMPARE(raw.write("RIFF\x04\x00\x00\x00WAVE", 12), qint64(12));
        raw.close();
        QSignalSpy inserted(session.worker,
                            &SstvStorageWorker::operationFinished);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker,
            [worker = session.worker, record = saved.record,
             requestId = static_cast<quint64>(600 + index)]() {
                worker->insertRecord(record, requestId);
            }, Qt::QueuedConnection));
        QTRY_COMPARE_WITH_TIMEOUT(inserted.count(), 1, 5'000);
        QVERIFY(inserted.takeFirst().at(2).toBool());
        *output = Materialized {
            saved.record,
            {saved.record.imagePath, saved.record.metadataPath,
             saved.record.thumbnailPath, saved.record.rawAudioPath}};
    };

    Materialized favorite;
    Materialized qso;
    Materialized shared;
    Materialized unsafe;
    Materialized deletableNewer;
    Materialized deletableOlder;
    materialize(0, true, false, false, &favorite);
    materialize(1, false, true, false, &qso);
    materialize(2, false, false, true, &shared);
    materialize(3, false, false, false, &unsafe);
    materialize(4, false, false, false, &deletableNewer);
    materialize(5, false, false, false, &deletableOlder);

    const QString externalPath = QDir(temporary.path()).absoluteFilePath(
        QStringLiteral("outside-thumbnail.png"));
    QFile external(externalPath);
    QVERIFY(external.open(QIODevice::WriteOnly));
    QCOMPARE(external.write("outside"), qint64(7));
    external.close();
    QVERIFY(QFile::remove(unsafe.record.thumbnailPath));
    QVERIFY(QFile::link(externalPath, unsafe.record.thumbnailPath));
    QVERIFY(QFileInfo(unsafe.record.thumbnailPath).isSymLink());

    QSignalSpy quotaSpy(session.worker,
                        &SstvStorageWorker::quotaCalculated);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker]() { worker->calculateQuota(700); },
        Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(quotaSpy.count(), 1, 5'000);
    const QList<QVariant> quotaResult = quotaSpy.takeFirst();
    QVERIFY2(quotaResult.at(2).toString().isEmpty(),
             qPrintable(quotaResult.at(2).toString()));
    const SstvQuotaSummary quota =
        quotaResult.at(1).value<SstvQuotaSummary>();
    QCOMPARE(quota.recordCount, 6);
    QVERIFY(quota.imageBytes > 0);
    QVERIFY(quota.thumbnailBytes > 0);
    QCOMPARE(quota.rawAudioBytes, qint64(72));
    QCOMPARE(quota.unsafePathCount, 1);
    QVERIFY(!quota.complete);

    SstvRetentionSettings policy;
    policy.maximumAgeDays = 1;
    policy.maximumDeletesPerRun = 500;
    QSignalSpy previewSpy(session.worker,
                          &SstvStorageWorker::retentionPreviewReady);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, policy]() {
            worker->previewRetention(policy, 701);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(previewSpy.count(), 1, 5'000);
    QList<QVariant> previewResult = previewSpy.takeFirst();
    QVERIFY2(previewResult.at(2).toString().isEmpty(),
             qPrintable(previewResult.at(2).toString()));
    SstvRetentionPlan plan = previewResult.at(1).value<SstvRetentionPlan>();
    QCOMPARE(plan.recordIds,
             QStringList({deletableOlder.record.id,
                          deletableNewer.record.id}));
    QCOMPARE(plan.protectedFavoriteCount, 1);
    QCOMPARE(plan.protectedQsoCount, 1);
    QCOMPARE(plan.protectedSharedCount, 1);
    QCOMPARE(plan.protectedUnsafeCount, 1);
    QVERIFY(!plan.targetsSatisfied);
    QVERIFY(!plan.confirmationPhrase.isEmpty());

    SstvRetentionSettings allowUploaded = policy;
    allowUploaded.sharedPolicy = SstvSharedRetentionPolicy::AllowUploaded;
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, allowUploaded]() {
            worker->previewRetention(allowUploaded, 708);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(previewSpy.count(), 1, 5'000);
    const SstvRetentionPlan allowPlan =
        previewSpy.takeFirst().at(1).value<SstvRetentionPlan>();
    QVERIFY(allowPlan.recordIds.contains(shared.record.id));
    QCOMPARE(allowPlan.protectedSharedCount, 0);
    QCOMPARE(allowPlan.protectedFavoriteCount, 1);
    QCOMPARE(allowPlan.protectedQsoCount, 1);

    QSignalSpy wrongApply(session.worker,
                          &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, token = plan.token]() {
            worker->applyRetentionPlan(token, QStringLiteral("DELETE"), 702);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(wrongApply.count(), 1, 5'000);
    QVERIFY(!wrongApply.takeFirst().at(2).toBool());
    for (const QString& path : deletableOlder.ownedPaths) {
        QVERIFY(QFileInfo::exists(path));
    }

    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, policy]() {
            worker->previewRetention(policy, 703);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(previewSpy.count(), 1, 5'000);
    plan = previewSpy.takeFirst().at(1).value<SstvRetentionPlan>();
    QSignalSpy deleted(session.worker,
                       &SstvStorageWorker::recordsDeletedWithFiles);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, plan]() {
            worker->applyRetentionPlan(plan.token,
                                       plan.confirmationPhrase, 704);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(deleted.count(), 1, 5'000);
    QStringList deletedIds = deleted.takeFirst().at(0).toStringList();
    deletedIds.sort();
    QStringList expectedDeleted {deletableNewer.record.id,
                                 deletableOlder.record.id};
    expectedDeleted.sort();
    QCOMPARE(deletedIds, expectedDeleted);
    for (const QString& path : deletableNewer.ownedPaths
                                   + deletableOlder.ownedPaths) {
        QVERIFY(!QFileInfo::exists(path));
    }
    for (const Materialized* protectedRecord :
         {&favorite, &qso, &shared, &unsafe}) {
        QVERIFY(QFileInfo::exists(protectedRecord->record.imagePath));
        QVERIFY(QFileInfo::exists(protectedRecord->record.metadataPath));
    }

    Materialized automatic;
    materialize(6, false, false, false, &automatic);
    QSignalSpy automaticDisabled(session.worker,
                                 &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker]() {
            worker->runAutomaticRetention(705);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(automaticDisabled.count(), 1, 5'000);
    QVERIFY(!automaticDisabled.takeFirst().at(2).toBool());
    QVERIFY(QFileInfo::exists(automatic.record.imagePath));

    policy.automaticEnabled = true;
    QSignalSpy settingsUpdated(
        session.worker, &SstvStorageWorker::retentionSettingsUpdated);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, policy]() {
            worker->updateRetentionSettings(policy, 706);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(settingsUpdated.count(), 1, 5'000);
    QVERIFY(settingsUpdated.takeFirst().at(1).toBool());
    session.stop();

    WorkerSession restarted(layout.databasePath(), layout.rootPath());
    QVERIFY2(restarted.start(&error), qPrintable(error));
    QSignalSpy loaded(restarted.worker,
                      &SstvStorageWorker::retentionSettingsLoaded);
    QVERIFY(QMetaObject::invokeMethod(
        restarted.worker, &SstvStorageWorker::loadRetentionSettings,
        Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5'000);
    QVERIFY(loaded.takeFirst().at(0)
                .value<SstvRetentionSettings>().automaticEnabled);
    QSignalSpy automaticDeleted(
        restarted.worker, &SstvStorageWorker::recordsDeletedWithFiles);
    QVERIFY(QMetaObject::invokeMethod(
        restarted.worker,
        [worker = restarted.worker]() {
            worker->runAutomaticRetention(707);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(automaticDeleted.count(), 1, 5'000);
    QCOMPARE(automaticDeleted.takeFirst().at(0).toStringList(),
             QStringList {automatic.record.id});
    for (const QString& path : automatic.ownedPaths) {
        QVERIFY(!QFileInfo::exists(path));
    }
    QVERIFY(QFileInfo::exists(favorite.record.imagePath));
    QVERIFY(QFileInfo::exists(qso.record.imagePath));
    QVERIFY(QFileInfo::exists(shared.record.imagePath));
    QVERIFY(QFileInfo::exists(unsafe.record.imagePath));
}

void TestSstvStorage::migratesVersionOneNonDestructively()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Imported,
        utcDateTime(QDate(2025, 1, 2), QTime(3, 4, 5)));
    request.fileNameTemplate = QStringLiteral("{id}");
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    QString error;
    QVERIFY2(createVersionOneDatabase(layout.databasePath(), saved.record, &error),
             qPrintable(error));
    WorkerSession session(layout.databasePath(), layout.rootPath());
    QVERIFY2(session.start(&error), qPrintable(error));
    QCOMPARE(session.schemaVersion, SstvStorageWorker::kCurrentSchemaVersion);
    QSignalSpy fetchSpy(session.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, id = saved.record.id]() {
            worker->fetchRecord(id, 301);
        }, Qt::QueuedConnection));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    const SstvImageRecord migrated = fetched.at(2).value<SstvImageRecord>();
    QCOMPARE(migrated.id, saved.record.id);
    QCOMPARE(migrated.updatedAtUtc, migrated.createdAtUtc);
    QCOMPARE(migrated.note, QString());
    QVERIFY(!migrated.remote);
    QCOMPARE(migrated.uploadState, SstvUploadState::NotRequested);
    QVERIFY(migrated.tags.isEmpty());
    QCOMPARE(migrated.eventAtUtc, migrated.capturedAtUtc);
    QCOMPARE(migrated.thumbnailPath,
             migrated.imagePath.left(migrated.imagePath.size() - 4)
                 + QStringLiteral(".thumb.png"));
    QCOMPARE(migrated.mimeType, QStringLiteral("image/png"));
    QCOMPARE(migrated.originalWidth, migrated.width);
    QCOMPARE(migrated.originalHeight, migrated.height);
    QCOMPARE(migrated.visValid, migrated.visCode >= 0);
    QCOMPARE(migrated.completionPercent, migrated.complete ? 100 : 0);
    QVERIFY(migrated.qualityMetrics.isEmpty());
    QCOMPARE(migrated.privacyFlags, quint32(0));
    QVERIFY(!migrated.favorite);
    session.stop();

    const QString connection = uniqueConnectionName(QStringLiteral("verify_v5"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY(database.open());
        QSqlQuery version(database);
        QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(version.next());
        QCOMPARE(version.value(0).toInt(), 5);
        QSqlQuery count(database);
        QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM sstv_images")));
        QVERIFY(count.next());
        QCOMPARE(count.value(0).toInt(), 1);
        QSqlQuery settings(database);
        QVERIFY(settings.exec(QStringLiteral(
            "SELECT automatic_enabled,shared_policy "
            "FROM sstv_retention_settings WHERE id=1")));
        QVERIFY(settings.next());
        QCOMPARE(settings.value(0).toInt(), 0);
        QCOMPARE(settings.value(1).toInt(), 0);
        QVERIFY(!settings.next());
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connection);
}

void TestSstvStorage::migratesVersionThreeAndResumesPartialSchema()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRequest(
        SstvImageCategory::Imported,
        utcDateTime(QDate(2025, 12, 31), QTime(23, 59, 58)));
    // Keep every post-v3 field at the deterministic migration default so the
    // migrated database record can still be verified against its sidecar.
    request.record.fskId.clear();
    request.record.localGrid.clear();
    request.record.remoteGrid.clear();
    request.record.audioFrequencyHz = 0;
    request.record.sourceSampleRateHz = 0;
    request.record.digital = false;
    request.record.qualityMetrics = {};
    request.record.slantCorrectionPpm = 0.0;
    request.record.relatedQsoId.clear();
    request.record.remoteProvider.clear();
    request.record.remoteObjectId.clear();
    request.record.expiresAtUtc = {};
    request.record.privacyFlags = 0;
    request.record.originalWidth = 0;
    request.record.originalHeight = 0;
    request.fileNameTemplate = QStringLiteral("{id}");
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    QString error;
    QVERIFY2(createVersionThreeDatabase(layout.databasePath(), saved.record,
                                        true, &error),
             qPrintable(error));
    WorkerSession first(layout.databasePath(), layout.rootPath());
    QVERIFY2(first.start(&error), qPrintable(error));
    QCOMPARE(first.schemaVersion, SstvStorageWorker::kCurrentSchemaVersion);

    QSignalSpy fetchSpy(first.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(QMetaObject::invokeMethod(
        first.worker,
        [worker = first.worker, id = saved.record.id]() {
            worker->fetchRecord(id, 901);
        }, Qt::QueuedConnection));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    const SstvImageRecord migrated = fetched.at(2).value<SstvImageRecord>();
    QCOMPARE(migrated.id, saved.record.id);
    QCOMPARE(migrated.category, saved.record.category);
    QCOMPARE(migrated.capturedAtUtc, saved.record.capturedAtUtc);
    QCOMPARE(migrated.createdAtUtc, saved.record.createdAtUtc);
    QCOMPARE(migrated.updatedAtUtc, saved.record.updatedAtUtc);
    QCOMPARE(migrated.note, saved.record.note);
    QCOMPARE(migrated.remote, saved.record.remote);
    QCOMPARE(migrated.uploadState, saved.record.uploadState);
    QCOMPARE(migrated.tags, saved.record.tags);
    QCOMPARE(migrated.eventAtUtc, saved.record.capturedAtUtc);
    QCOMPARE(migrated.thumbnailPath,
             saved.record.imagePath.left(saved.record.imagePath.size() - 4)
                 + QStringLiteral(".thumb.png"));
    QCOMPARE(migrated.mimeType, QStringLiteral("image/png"));
    QCOMPARE(migrated.originalWidth, saved.record.width);
    QCOMPARE(migrated.originalHeight, saved.record.height);
    QVERIFY(migrated.visValid);
    QCOMPARE(migrated.completionPercent, 100);
    QVERIFY(!migrated.digital);
    QVERIFY(migrated.fskId.isEmpty());
    QVERIFY(migrated.localGrid.isEmpty());
    QVERIFY(migrated.remoteGrid.isEmpty());
    QCOMPARE(migrated.audioFrequencyHz, qint64(0));
    QCOMPARE(migrated.sourceSampleRateHz, 0);
    QVERIFY(migrated.qualityMetrics.isEmpty());
    QCOMPARE(migrated.slantCorrectionPpm, 0.0);
    QVERIFY(migrated.rawAudioPath.isEmpty());
    QVERIFY(migrated.relatedQsoId.isEmpty());
    QVERIFY(migrated.remoteProvider.isEmpty());
    QVERIFY(migrated.remoteObjectId.isEmpty());
    QVERIFY(!migrated.expiresAtUtc.isValid());
    QCOMPARE(migrated.privacyFlags, quint32(0));
    QCOMPARE(migrated, saved.record);
    QVERIFY2(store.verify(migrated, true, &error), qPrintable(error));
    first.stop();

    // Reopening a fully migrated database is an idempotent validation path.
    WorkerSession second(layout.databasePath(), layout.rootPath());
    QVERIFY2(second.start(&error), qPrintable(error));
    QCOMPARE(second.schemaVersion, SstvStorageWorker::kCurrentSchemaVersion);
    second.stop();

    const QString connection = uniqueConnectionName(
        QStringLiteral("verify_resumed_v4"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery version(database);
        QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(version.next());
        QCOMPARE(version.value(0).toInt(), 5);
        QSqlQuery rows(database);
        QVERIFY(rows.exec(QStringLiteral("SELECT COUNT(*) FROM sstv_images")));
        QVERIFY(rows.next());
        QCOMPARE(rows.value(0).toInt(), 1);
        QSqlQuery tags(database);
        QVERIFY(tags.exec(QStringLiteral("SELECT COUNT(*) FROM sstv_image_tags")));
        QVERIFY(tags.next());
        QCOMPARE(tags.value(0).toInt(), saved.record.tags.size());
        QSqlQuery trigger(database);
        QVERIFY(trigger.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' "
            "AND name='trg_sstv_images_v4_compat_insert'")));
        QVERIFY(trigger.next());
        QCOMPARE(trigger.value(0).toInt(), 1);
        QSqlQuery integrity(database);
        QVERIFY(integrity.exec(QStringLiteral("PRAGMA integrity_check")));
        QVERIFY(integrity.next());
        QCOMPARE(integrity.value(0).toString(), QStringLiteral("ok"));
        QSqlQuery foreignKeys(database);
        QVERIFY(foreignKeys.exec(QStringLiteral("PRAGMA foreign_key_check")));
        QVERIFY(!foreignKeys.next());
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connection);
}

void TestSstvStorage::rejectsFutureSchemaWithoutMutation()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    QString error;
    QVERIFY(layout.ensure(&error));
    const QString connection = uniqueConnectionName(QStringLiteral("future_v6"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY(database.open());
        QVERIFY(execSql(database,
                        QStringLiteral("CREATE TABLE future_sentinel(value TEXT)"),
                        &error));
        QVERIFY(execSql(database,
                        QStringLiteral("INSERT INTO future_sentinel VALUES('keep')"),
                        &error));
        QVERIFY(execSql(database, QStringLiteral("PRAGMA user_version=6"),
                        &error));
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connection);

    WorkerSession session(layout.databasePath(), layout.rootPath());
    QVERIFY(!session.start(&error));
    QCOMPARE(session.schemaVersion, 6);
    QVERIFY(error.contains(QStringLiteral("newer"), Qt::CaseInsensitive));
    session.stop();

    const QString verifyConnection = uniqueConnectionName(
        QStringLiteral("future_verify"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), verifyConnection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY(database.open());
        QSqlQuery version(database);
        QVERIFY(version.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(version.next());
        QCOMPARE(version.value(0).toInt(), 6);
        QSqlQuery sentinel(database);
        QVERIFY(sentinel.exec(QStringLiteral("SELECT value FROM future_sentinel")));
        QVERIFY(sentinel.next());
        QCOMPARE(sentinel.value(0).toString(), QStringLiteral("keep"));
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(verifyConnection);
}

QTEST_MAIN(TestSstvStorage)

#include "test_sstv_storage.moc"
