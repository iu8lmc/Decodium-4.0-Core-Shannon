// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/models/SstvGalleryModel.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#include "src/sstv/storage/SstvImageStorage.h"
#include "src/sstv/storage/SstvStorageWorker.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
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

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace decodium::sstv;

namespace {

QString uniqueConnectionName(const QString& prefix)
{
    return prefix + QLatin1Char('_')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QDateTime utcDateTime(const QDate& date, const QTime& time)
{
    return QDateTime(date, time, QTimeZone(QTimeZone::UTC));
}

QString syntheticId(int index)
{
    return QStringLiteral("00000000-0000-4000-8000-%1")
        .arg(index, 12, 16, QLatin1Char('0'));
}

class WorkerSession final
{
public:
    explicit WorkerSession(const SstvStorageLayout& layout)
        : worker(new SstvStorageWorker(layout.databasePath(),
                                       layout.rootPath()))
    {
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished,
                         worker, &QObject::deleteLater);
    }

    ~WorkerSession() { stop(); }

    bool start(QString* error = nullptr)
    {
        QSignalSpy spy(worker, &SstvStorageWorker::initialized);
        thread.start();
        if (!QMetaObject::invokeMethod(worker, &SstvStorageWorker::initialize,
                                      Qt::QueuedConnection)
            || !spy.wait(5000) || spy.isEmpty()) {
            if (error) {
                *error = QStringLiteral("worker initialization timed out");
            }
            return false;
        }
        const QList<QVariant> arguments = spy.takeFirst();
        if (error) {
            *error = arguments.at(1).toString();
        }
        return arguments.at(0).toBool();
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
    }

    QThread thread;
    SstvStorageWorker* worker {nullptr};
};

bool populateSyntheticDatabase(const SstvStorageLayout& layout,
                               int recordCount,
                               const QDateTime& base,
                               QString* error)
{
    const QString connection = uniqueConnectionName(
        QStringLiteral("sstv_gallery_seed"));
    bool ok = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(layout.databasePath());
        if (!database.open() || !database.transaction()) {
            if (error) {
                *error = database.lastError().text();
            }
        } else {
            QSqlQuery image(database);
            QSqlQuery tag(database);
            const bool prepared = image.prepare(QStringLiteral(
                "INSERT INTO sstv_images("
                "id,category,captured_at_ms,created_at_ms,updated_at_ms,"
                "mode,vis_code,remote_callsign,local_callsign,source,"
                "frequency_hz,complete,image_path,metadata_path,sha256_hex,"
                "file_size_bytes,width,height,note,remote,upload_state,tags_json)"
                " VALUES(:id,:category,:captured,:created,:updated,:mode,:vis,"
                ":remote_call,:local_call,:source,:frequency,:complete,:image,"
                ":metadata,:hash,:bytes,:width,:height,:note,:remote,"
                ":upload_state,:tags_json)"))
                && tag.prepare(QStringLiteral(
                    "INSERT INTO sstv_image_tags(image_id,tag,tag_folded) "
                    "VALUES(:id,:tag,:folded)"));
            ok = prepared;
            for (int index = 1; ok && index <= recordCount; ++index) {
                const QString id = syntheticId(index);
                const int category = index % 4 == 0
                    ? static_cast<int>(SstvImageCategory::Received)
                    : index % 4 == 1
                        ? static_cast<int>(SstvImageCategory::Transmitted)
                        : index % 4 == 2
                            ? static_cast<int>(SstvImageCategory::Imported)
                            : static_cast<int>(SstvImageCategory::Draft);
                const QString mode = index % 3 == 0
                    ? QStringLiteral("Martin M1")
                    : index % 3 == 1
                        ? QStringLiteral("Scottie S1")
                        : QStringLiteral("Robot 36");
                const QString firstTag = index % 2 == 0
                    ? QStringLiteral("portable") : QStringLiteral("field");
                const QString secondTag = QStringLiteral("group-%1")
                    .arg(index % 10);
                QJsonArray tags;
                tags.append(firstTag);
                tags.append(secondTag);
                const QString directory = layout.categoryRoot(
                    static_cast<SstvImageCategory>(category));
                image.bindValue(QStringLiteral(":id"), id);
                image.bindValue(QStringLiteral(":category"), category);
                image.bindValue(QStringLiteral(":captured"),
                                base.addSecs(index).toMSecsSinceEpoch());
                image.bindValue(QStringLiteral(":created"),
                                base.addSecs(index).toMSecsSinceEpoch());
                image.bindValue(QStringLiteral(":updated"),
                                base.addSecs(index).toMSecsSinceEpoch());
                image.bindValue(QStringLiteral(":mode"), mode);
                image.bindValue(QStringLiteral(":vis"), 44);
                image.bindValue(QStringLiteral(":remote_call"),
                                QStringLiteral("CALL%1").arg(index));
                image.bindValue(QStringLiteral(":local_call"),
                                QStringLiteral("IU8LMC"));
                image.bindValue(QStringLiteral(":source"),
                                QStringLiteral("synthetic collection"));
                image.bindValue(QStringLiteral(":frequency"),
                                14'000'000 + index);
                image.bindValue(QStringLiteral(":complete"),
                                index % 5 == 0 ? 0 : 1);
                image.bindValue(QStringLiteral(":image"),
                                QDir(directory).filePath(id + QStringLiteral(".png")));
                image.bindValue(QStringLiteral(":metadata"),
                                QDir(directory).filePath(id + QStringLiteral(".json")));
                image.bindValue(QStringLiteral(":hash"),
                                QString(64, QLatin1Char('0')));
                image.bindValue(QStringLiteral(":bytes"), 100);
                image.bindValue(QStringLiteral(":width"), 320);
                image.bindValue(QStringLiteral(":height"), 256);
                image.bindValue(QStringLiteral(":note"),
                                QStringLiteral("note needle-%1").arg(index));
                image.bindValue(QStringLiteral(":remote"),
                                index % 7 == 0 ? 1 : 0);
                image.bindValue(QStringLiteral(":upload_state"), index % 5);
                image.bindValue(QStringLiteral(":tags_json"),
                                QString::fromUtf8(QJsonDocument(tags).toJson(
                                    QJsonDocument::Compact)));
                ok = image.exec();
                for (const QString& value : {firstTag, secondTag}) {
                    if (!ok) {
                        break;
                    }
                    tag.bindValue(QStringLiteral(":id"), id);
                    tag.bindValue(QStringLiteral(":tag"), value);
                    tag.bindValue(QStringLiteral(":folded"),
                                  value.toCaseFolded());
                    ok = tag.exec();
                }
                if (!ok && error) {
                    *error = image.lastError().isValid()
                        ? image.lastError().text() : tag.lastError().text();
                }
            }
            if (ok) {
                ok = database.commit();
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

SstvImageSaveRequest makeRealRequest(int index, const QDateTime& captured)
{
    SstvImageSaveRequest request;
    request.record.category = index % 2 == 0
        ? SstvImageCategory::Received : SstvImageCategory::Transmitted;
    request.record.capturedAtUtc = captured;
    request.record.mode = QStringLiteral("Martin M1");
    request.record.visCode = 44;
    request.record.remoteCallsign = QStringLiteral("9H1TEST%1").arg(index);
    request.record.localCallsign = QStringLiteral("IU8LMC");
    request.record.source = QStringLiteral("gallery model test");
    request.record.frequencyHz = 14'230'000 + index;
    request.record.complete = true;
    request.record.tags = {QStringLiteral("verified")};
    request.image = QImage(64, 48, QImage::Format_RGB32);
    request.image.fill(QColor::fromRgb(20 * index, 40, 80));
    request.fileNameTemplate = QStringLiteral("{id}");
    return request;
}

void waitForModelIdle(SstvGalleryModel& model)
{
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 10000);
}

} // namespace

class TestSstvGalleryModel final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void qmlFilterMapIsAtomicAndValidated();
    void lazyLargeCollectionFiltersSortAndStaleRequests();
    void incrementalUpdateSelectionAndExplicitIndexDelete();
    void favoriteQuotaRetentionModelApiIsTypedAndFailClosed();
    void qsoAssociationModelApiIsBoundedAndObservable();
    void userMetadataModelApiIsBoundedAndObservable();
    void thumbnailProviderRequestsAreReentrant();
    void thumbnailsAreBoundedCachedInvalidatedAndLifecycleSafe();
};

void TestSstvGalleryModel::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("IU8LMC"));
    QCoreApplication::setApplicationName(QStringLiteral("Decodium SSTV Gallery Test"));
    QStandardPaths::setTestModeEnabled(true);
    qRegisterMetaType<SstvGalleryPage>();
    qRegisterMetaType<SstvGalleryQuery>();
    qRegisterMetaType<SstvImageRecord>();
    qRegisterMetaType<SstvStorageOperation>();
    qRegisterMetaType<SstvRetentionSettings>();
    qRegisterMetaType<SstvQuotaSummary>();
    qRegisterMetaType<SstvRetentionPlan>();
}

void TestSstvGalleryModel::qmlFilterMapIsAtomicAndValidated()
{
    SstvGalleryModel model;
    QSignalSpy rejected(&model, &SstvGalleryModel::queryRejected);
    QVariantMap filters;
    filters.insert(QStringLiteral("categoryMask"),
                   static_cast<int>(SstvGalleryReceived));
    filters.insert(QStringLiteral("remoteFilter"), 1);
    filters.insert(QStringLiteral("mode"), QStringLiteral(" Martin M1 "));
    filters.insert(QStringLiteral("callsign"), QStringLiteral(" 9H1 "));
    filters.insert(QStringLiteral("capturedFromUtc"),
                   QStringLiteral("2026-08-01T00:00:00.000Z"));
    filters.insert(QStringLiteral("capturedToUtc"),
                   QStringLiteral("2026-08-31T23:59:59.999Z"));
    filters.insert(QStringLiteral("minimumFrequencyHz"), 14'000'000);
    filters.insert(QStringLiteral("maximumFrequencyHz"), 14'400'000);
    filters.insert(QStringLiteral("tags"),
                   QVariantList {QStringLiteral(" portable "),
                                 QString::fromUtf8("M\xC3\xA1laga")});
    filters.insert(QStringLiteral("requireAllTags"), true);
    filters.insert(QStringLiteral("partialFilter"), 2);
    filters.insert(QStringLiteral("uploadStateFilter"), 3);
    filters.insert(QStringLiteral("search"), QStringLiteral(" field "));
    filters.insert(QStringLiteral("sortOrder"), 6);
    filters.insert(QStringLiteral("pageSize"), 75);
    QVERIFY(model.applyFilters(filters));
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(model.categoryMask(), static_cast<int>(SstvGalleryReceived));
    QCOMPARE(model.remoteFilter(), 1);
    QCOMPARE(model.modeFilter(), QStringLiteral("Martin M1"));
    QCOMPARE(model.callsignFilter(), QStringLiteral("9H1"));
    QCOMPARE(model.minimumFrequencyHz(), qint64(14'000'000));
    QCOMPARE(model.maximumFrequencyHz(), qint64(14'400'000));
    QCOMPARE(model.tags(),
             QStringList({QStringLiteral("portable"),
                          QString::fromUtf8("M\xC3\xA1laga")}));
    QCOMPARE(model.pageSize(), 75);

    const QVariantMap before = model.filters();
    QVariantMap invalid;
    invalid.insert(QStringLiteral("minimumFrequencyHz"),
                   QStringLiteral("not-a-frequency"));
    QVERIFY(!model.applyFilters(invalid));
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(model.filters(), before);
}

void TestSstvGalleryModel::lazyLargeCollectionFiltersSortAndStaleRequests()
{
    constexpr int recordCount = 5000;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    WorkerSession schemaSession(layout);
    QString error;
    QVERIFY2(schemaSession.start(&error), qPrintable(error));
    schemaSession.stop();
    const QDateTime base = utcDateTime(QDate(2026, 1, 1), QTime(0, 0));
    QElapsedTimer seedTimer;
    seedTimer.start();
    QVERIFY2(populateSyntheticDatabase(layout, recordCount, base, &error),
             qPrintable(error));
    const qint64 seedElapsedMs = seedTimer.elapsed();

    WorkerSession session(layout);
    QVERIFY2(session.start(&error), qPrintable(error));
    SstvGalleryModel model;
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    model.setStorageWorker(session.worker); // Intentionally made stale below.

    SstvGalleryQuery query;
    query.categoryMask = SstvGalleryReceived;
    query.remote = SstvGalleryTruthFilter::OnlyTrue;
    query.mode = QStringLiteral("Martin M1");
    query.callsign = QStringLiteral("CALL");
    query.capturedFromUtc = base.addSecs(1);
    query.capturedToUtc = base.addSecs(recordCount + 1);
    query.minimumFrequencyHz = 14'000'001;
    query.maximumFrequencyHz = 14'000'000 + recordCount;
    query.tags = {QStringLiteral("portable"), QStringLiteral("group-0")};
    query.requireAllTags = true;
    query.partial = SstvGalleryTruthFilter::OnlyTrue;
    query.uploadState = static_cast<int>(SstvUploadState::NotRequested);
    query.search = QStringLiteral("synthetic");
    query.sort = SstvGallerySort::FrequencyAscending;
    query.limit = 4;
    QElapsedTimer firstInteractionTimer;
    firstInteractionTimer.start();
    QVERIFY2(model.setQuery(query, &error), qPrintable(error));
    waitForModelIdle(model);
    while (model.hasMore()) {
        QVERIFY(model.canFetchMore({}));
        model.fetchMore({});
        waitForModelIdle(model);
    }
    const qint64 firstInteractionElapsedMs = firstInteractionTimer.elapsed();
    QVERIFY2(firstInteractionElapsedMs < 5'000,
             qPrintable(QStringLiteral(
                 "5k Gallery lazy filter/fetch exceeded 5000 ms: %1")
                 .arg(firstInteractionElapsedMs)));
    QCOMPARE(model.rowCount(), recordCount / 420);
    QVERIFY(insertedSpy.count() >= 3);
    QCOMPARE(resetSpy.count(), 0);
    qint64 previousFrequency = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row);
        QCOMPARE(model.data(index, SstvGalleryModel::CategoryRole).toInt(),
                 static_cast<int>(SstvImageCategory::Received));
        QVERIFY(model.data(index, SstvGalleryModel::RemoteRole).toBool());
        QVERIFY(model.data(index, SstvGalleryModel::PartialRole).toBool());
        QCOMPARE(model.data(index, SstvGalleryModel::ModeRole).toString(),
                 QStringLiteral("Martin M1"));
        QVERIFY(model.data(index, SstvGalleryModel::TagsRole)
                    .toStringList().contains(QStringLiteral("portable")));
        const qint64 frequency = model.data(
            index, SstvGalleryModel::FrequencyHzRole).toLongLong();
        QVERIFY(frequency > previousFrequency);
        previousFrequency = frequency;
    }

    SstvGalleryQuery searchQuery;
    searchQuery.search = QStringLiteral("needle-4992");
    searchQuery.limit = 50;
    QElapsedTimer searchTimer;
    searchTimer.start();
    QVERIFY2(model.setQuery(searchQuery, &error), qPrintable(error));
    waitForModelIdle(model);
    const qint64 searchElapsedMs = searchTimer.elapsed();
    QVERIFY2(searchElapsedMs < 5'000,
             qPrintable(QStringLiteral(
                 "5k Gallery indexed search exceeded 5000 ms: %1")
                 .arg(searchElapsedMs)));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), SstvGalleryModel::IdRole).toString(),
             syntheticId(4992));
    qInfo().noquote()
        << QStringLiteral(
               "SSTV_GALLERY_5K seed_ms=%1 lazy_filter_fetch_ms=%2 search_ms=%3")
               .arg(seedElapsedMs)
               .arg(firstInteractionElapsedMs)
               .arg(searchElapsedMs);

    SstvGalleryQuery transmitted;
    transmitted.categoryMask = SstvGalleryTransmitted;
    transmitted.limit = 200;
    QVERIFY2(model.setQuery(transmitted, &error), qPrintable(error));
    waitForModelIdle(model);
    QCOMPARE(model.rowCount(), 200);
    QVERIFY(model.hasMore());
    for (int row = 0; row < model.rowCount(); ++row) {
        QCOMPARE(model.data(model.index(row),
                            SstvGalleryModel::CategoryRole).toInt(),
                 static_cast<int>(SstvImageCategory::Transmitted));
    }
    const QString movedId = model.data(model.index(0),
                                       SstvGalleryModel::IdRole).toString();
    const QString updateConnection = uniqueConnectionName(
        QStringLiteral("sstv_gallery_reorder"));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), updateConnection);
        database.setDatabaseName(layout.databasePath());
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));
        QSqlQuery update(database);
        update.prepare(QStringLiteral(
            "UPDATE sstv_images SET captured_at_ms=:captured,"
            "updated_at_ms=:updated WHERE id=:id"));
        update.bindValue(QStringLiteral(":captured"),
                         base.addSecs(-100).toMSecsSinceEpoch());
        update.bindValue(QStringLiteral(":updated"),
                         base.addSecs(recordCount + 100).toMSecsSinceEpoch());
        update.bindValue(QStringLiteral(":id"), movedId);
        QVERIFY2(update.exec(), qPrintable(update.lastError().text()));
        QCOMPARE(update.numRowsAffected(), qint64(1));
        database.close();
        database = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(updateConnection);
    QSignalSpy fetchSpy(session.worker, &SstvStorageWorker::recordFetched);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, movedId]() {
            worker->fetchRecord(movedId, 9000);
        }, Qt::QueuedConnection));
    QVERIFY(fetchSpy.wait(5000));
    const QList<QVariant> fetched = fetchSpy.takeFirst();
    QVERIFY2(fetched.at(1).toBool(), qPrintable(fetched.at(3).toString()));
    const SstvImageRecord moved = fetched.at(2).value<SstvImageRecord>();
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, moved]() { worker->recordChanged(moved); },
        Qt::QueuedConnection));
    // QAbstractItemModelTester may immediately exercise canFetchMore() after
    // the boundary row is removed; either 199 or a refilled/larger prefix is
    // valid, but the stale row must not remain in that loaded prefix.
    const auto loadedContains = [&model](const QString& id) {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.data(model.index(row),
                           SstvGalleryModel::IdRole).toString() == id) {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(!loadedContains(movedId), 5000);
    QVERIFY(model.rowCount() >= 199);
    waitForModelIdle(model);
    QVERIFY(model.data(model.index(0), SstvGalleryModel::IdRole).toString()
            != movedId);
    for (int row = 0; row < model.rowCount(); ++row) {
        QVERIFY(model.data(model.index(row),
                           SstvGalleryModel::IdRole).toString() != movedId);
    }
    QCOMPARE(resetSpy.count(), 0);
    model.shutdown();
    session.stop();
}

void TestSstvGalleryModel::incrementalUpdateSelectionAndExplicitIndexDelete()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(temporary.filePath(QStringLiteral("sstv")));
    const SstvImageStore store(layout);
    const QDateTime base = utcDateTime(QDate(2026, 8, 24), QTime(12, 0));
    QVector<SstvImageRecord> records;
    for (int index = 1; index <= 3; ++index) {
        const SstvImageSaveResult saved = store.save(
            makeRealRequest(index, base.addSecs(index)));
        QVERIFY2(saved.ok, qPrintable(saved.error));
        records.append(saved.record);
    }

    WorkerSession session(layout);
    QString error;
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy operationSpy(session.worker,
                            &SstvStorageWorker::operationFinished);
    for (int index = 0; index < records.size(); ++index) {
        const SstvImageRecord record = records.at(index);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker,
            [worker = session.worker, record, index]() {
                worker->insertRecord(record,
                    static_cast<quint64>(100 + index));
            }, Qt::QueuedConnection));
    }
    QTRY_COMPARE_WITH_TIMEOUT(operationSpy.count(), records.size(), 10000);
    for (const QList<QVariant>& operation : operationSpy) {
        QVERIFY2(operation.at(2).toBool(),
                 qPrintable(operation.at(3).toString()));
    }

    SstvThumbnailProvider thumbnailProvider;
    SstvGalleryModel model;
    model.setThumbnailProvider(&thumbnailProvider);
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setStorageWorker(session.worker);
    waitForModelIdle(model);
    QCOMPARE(model.rowCount(), 3);
    const QHash<int, QByteArray> roles = model.roleNames();
    for (const QByteArray& name : {
             QByteArrayLiteral("eventAtUtc"),
             QByteArrayLiteral("visValid"),
             QByteArrayLiteral("completionPercent"),
             QByteArrayLiteral("qualityMetrics"),
             QByteArrayLiteral("thumbnailPath"),
             QByteArrayLiteral("mimeType"),
             QByteArrayLiteral("originalWidth"),
             QByteArrayLiteral("sha256Hex")}) {
        QVERIFY2(roles.values().contains(name), name.constData());
    }
    int metadataRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), SstvGalleryModel::IdRole).toString()
            == records.constFirst().id) {
            metadataRow = row;
            break;
        }
    }
    QVERIFY(metadataRow >= 0);
    const QModelIndex metadataIndex = model.index(metadataRow);
    QCOMPARE(model.data(metadataIndex, SstvGalleryModel::EventAtRole).toDateTime(),
             records.constFirst().eventAtUtc);
    QCOMPARE(model.data(metadataIndex,
                        SstvGalleryModel::CompletionPercentRole).toInt(), 100);
    QCOMPARE(model.data(metadataIndex,
                        SstvGalleryModel::ThumbnailPathRole).toString(),
             records.constFirst().thumbnailPath);
    QCOMPARE(model.data(metadataIndex, SstvGalleryModel::MimeTypeRole).toString(),
             QStringLiteral("image/png"));
    std::unique_ptr<QQuickImageResponse> registeredThumbnail(
        thumbnailProvider.requestImageResponse(records.constFirst().id,
                                               QSize(32, 32)));
    QSignalSpy registeredThumbnailFinished(
        registeredThumbnail.get(), &QQuickImageResponse::finished);
    QVERIFY(registeredThumbnailFinished.wait(5000));
    QVERIFY2(registeredThumbnail->errorString().isEmpty(),
             qPrintable(registeredThumbnail->errorString()));

    SstvImageRecord updated = records.constFirst();
    updated.capturedAtUtc = base.addSecs(100);
    updated.updatedAtUtc = QDateTime::currentDateTimeUtc().addSecs(1);
    updated.note = QStringLiteral("moved incrementally");
    QVERIFY2(store.updateMetadata(updated, &error), qPrintable(error));
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, updated]() {
            worker->updateRecord(updated, 200);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(model.index(0), SstvGalleryModel::IdRole).toString(),
        updated.id, 10000);
    QCOMPARE(model.data(model.index(0), SstvGalleryModel::NoteRole).toString(),
             QStringLiteral("moved incrementally"));

    const QString exportPath = temporary.filePath(
        QStringLiteral("explicit-export.png"));
    QSignalSpy exportSpy(&model, &SstvGalleryModel::exportFinished);
    const quint64 exportRequest = model.requestExportRecord(
        updated.id, QUrl::fromLocalFile(exportPath));
    QVERIFY(exportRequest != 0U);
    QTRY_COMPARE_WITH_TIMEOUT(exportSpy.count(), 1, 10000);
    const QList<QVariant> exported = exportSpy.takeFirst();
    QCOMPARE(exported.at(0).toULongLong(), exportRequest);
    QVERIFY2(exported.at(1).toBool(),
             qPrintable(exported.at(3).toString()));
    QCOMPARE(QFileInfo(exported.at(2).toUrl().toLocalFile()).canonicalFilePath(),
             QFileInfo(exportPath).canonicalFilePath());
    QFile sourceBytes(updated.imagePath);
    QFile exportedBytes(exportPath);
    QVERIFY(sourceBytes.open(QIODevice::ReadOnly));
    QVERIFY(exportedBytes.open(QIODevice::ReadOnly));
    const QByteArray expectedExport = sourceBytes.readAll();
    QCOMPARE(exportedBytes.readAll(), expectedExport);
    const QFileDevice::Permissions exportedPermissions =
        QFileInfo(exportPath).permissions();
    QVERIFY(exportedPermissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(exportedPermissions.testFlag(QFileDevice::WriteOwner));
    QVERIFY(!exportedPermissions.testFlag(QFileDevice::ReadGroup));
    QVERIFY(!exportedPermissions.testFlag(QFileDevice::ReadOther));

    const quint64 existingDestination = model.requestExportRecord(
        updated.id, QUrl::fromLocalFile(exportPath));
    QVERIFY(existingDestination != 0U);
    QTRY_COMPARE_WITH_TIMEOUT(exportSpy.count(), 1, 10000);
    const QList<QVariant> refusedExisting = exportSpy.takeFirst();
    QVERIFY(!refusedExisting.at(1).toBool());
    QVERIFY(refusedExisting.at(3).toString().contains(
        QStringLiteral("exists"), Qt::CaseInsensitive));
    const quint64 sourceReplacement = model.requestExportRecord(
        updated.id, QUrl::fromLocalFile(updated.imagePath), true);
    QVERIFY(sourceReplacement != 0U);
    QTRY_COMPARE_WITH_TIMEOUT(exportSpy.count(), 1, 10000);
    const QList<QVariant> refusedSource = exportSpy.takeFirst();
    QVERIFY(!refusedSource.at(1).toBool());
    QVERIFY(refusedSource.at(3).toString().contains(
        QStringLiteral("source"), Qt::CaseInsensitive));
    QVERIFY(model.requestExportRecord(
        updated.id, QUrl(QStringLiteral("https://example.invalid/a.png")))
        == 0U);
    exportedBytes.close();
    QVERIFY(exportedBytes.open(QIODevice::ReadOnly));
    QCOMPARE(exportedBytes.readAll(), expectedExport);

    model.setSelected(records.at(0).id, true);
    model.setSelected(records.at(1).id, true);
    QCOMPARE(model.selectedCount(), 2);
    const QString firstImage = records.at(0).imagePath;
    const QString firstMetadata = records.at(0).metadataPath;
    const QString secondImage = records.at(1).imagePath;
    const QString secondMetadata = records.at(1).metadataPath;
    QSignalSpy deleteSpy(&model, &SstvGalleryModel::deleteFinished);
    const quint64 deleteRequest = model.requestDeleteSelectedFromIndex();
    QVERIFY(deleteRequest != 0);
    QTRY_COMPARE_WITH_TIMEOUT(deleteSpy.count(), 1, 10000);
    const QList<QVariant> deletion = deleteSpy.takeFirst();
    QCOMPARE(deletion.at(0).toULongLong(), deleteRequest);
    QVERIFY2(deletion.at(1).toBool(), qPrintable(deletion.at(2).toString()));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.selectedCount(), 0);
    QVERIFY(QFileInfo::exists(firstImage));
    QVERIFY(QFileInfo::exists(firstMetadata));
    QVERIFY(QFileInfo::exists(secondImage));
    QVERIFY(QFileInfo::exists(secondMetadata));
    std::unique_ptr<QQuickImageResponse> unregisteredThumbnail(
        thumbnailProvider.requestImageResponse(records.at(0).id,
                                               QSize(32, 32)));
    QSignalSpy unregisteredThumbnailFinished(
        unregisteredThumbnail.get(), &QQuickImageResponse::finished);
    QTRY_COMPARE_WITH_TIMEOUT(unregisteredThumbnailFinished.count(), 1, 5000);
    QVERIFY(unregisteredThumbnail->errorString().contains(
        QStringLiteral("not registered"), Qt::CaseInsensitive));

    SstvImageRecord deletable = records.at(2);
    const QString rawDirectory = QDir(layout.rootPath()).absoluteFilePath(
        QStringLiteral("raw"));
    QVERIFY(QDir().mkpath(rawDirectory));
    deletable.rawAudioPath = QDir(rawDirectory).absoluteFilePath(
        QStringLiteral("retained.wav"));
    QFile rawAudio(deletable.rawAudioPath);
    QVERIFY(rawAudio.open(QIODevice::WriteOnly));
    QVERIFY(rawAudio.write("RIFF\x04\x00\x00\x00WAVE", 12) == 12);
    rawAudio.close();
    QImage thumbnail(24, 16, QImage::Format_RGB32);
    thumbnail.fill(QColor(QStringLiteral("#284866")));
    QVERIFY(thumbnail.save(deletable.thumbnailPath, "PNG"));
    deletable.updatedAtUtc = QDateTime::currentDateTimeUtc().addSecs(2);
    QVERIFY2(store.updateMetadata(deletable, &error), qPrintable(error));
    QSignalSpy updateForDelete(session.worker,
                               &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, deletable]() {
            worker->updateRecord(deletable, 301);
        }, Qt::QueuedConnection));
    QTRY_VERIFY_WITH_TIMEOUT(updateForDelete.count() >= 1, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(model.index(0), SstvGalleryModel::RawAudioPathRole)
            .toString(),
        deletable.rawAudioPath, 10000);
    model.setSelected(deletable.id, true);
    QCOMPARE(model.selectedCount(), 1);
    QSignalSpy deleteFilesSpy(&model,
                              &SstvGalleryModel::deleteFilesFinished);
    const quint64 deleteFilesRequest =
        model.requestDeleteSelectedWithFiles();
    QVERIFY(deleteFilesRequest != 0U);
    QTRY_COMPARE_WITH_TIMEOUT(deleteFilesSpy.count(), 1, 10000);
    const QList<QVariant> fileDeletion = deleteFilesSpy.takeFirst();
    QCOMPARE(fileDeletion.at(0).toULongLong(), deleteFilesRequest);
    QVERIFY2(fileDeletion.at(1).toBool(),
             qPrintable(fileDeletion.at(2).toString()));
    QVERIFY(fileDeletion.at(2).toString().isEmpty());
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.selectedCount(), 0);
    QVERIFY(!QFileInfo::exists(deletable.imagePath));
    QVERIFY(!QFileInfo::exists(deletable.metadataPath));
    QVERIFY(!QFileInfo::exists(deletable.thumbnailPath));
    QVERIFY(!QFileInfo::exists(deletable.rawAudioPath));
    const QDir deletionStage(QDir(layout.rootPath()).absoluteFilePath(
        QStringLiteral(".delete-staging")));
    QVERIFY(deletionStage.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    QVERIFY(QFileInfo::exists(firstImage));
    QVERIFY(QFileInfo::exists(firstMetadata));
    QVERIFY(QFileInfo::exists(secondImage));
    QVERIFY(QFileInfo::exists(secondMetadata));
    QCOMPARE(resetSpy.count(), 0);
    model.shutdown();
    thumbnailProvider.shutdown();
    session.stop();
}

void TestSstvGalleryModel::favoriteQuotaRetentionModelApiIsTypedAndFailClosed()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY(layout.ensure(&error));
    SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRealRequest(
        7, utcDateTime(QDate(2020, 1, 2), QTime(3, 4, 5)));
    request.record.relatedQsoId.clear();
    request.record.uploadState = SstvUploadState::NotRequested;
    request.record.remoteProvider.clear();
    request.record.remoteObjectId.clear();
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    WorkerSession session(layout);
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy inserted(session.worker,
                        &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 900);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(inserted.count(), 1, 5'000);
    QVERIFY(inserted.takeFirst().at(2).toBool());

    SstvGalleryModel model;
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setStorageWorker(session.worker);
    waitForModelIdle(model);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.roleNames().value(SstvGalleryModel::FavoriteRole),
             QByteArrayLiteral("favorite"));
    QVERIFY(!model.data(model.index(0),
                        SstvGalleryModel::FavoriteRole).toBool());

    QSignalSpy favoriteFinished(&model,
                                &SstvGalleryModel::favoriteFinished);
    QVERIFY(model.toggleFavorite(0) != 0);
    QTRY_COMPARE_WITH_TIMEOUT(favoriteFinished.count(), 1, 5'000);
    QVERIFY(favoriteFinished.takeFirst().at(2).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(model.data(
        model.index(0), SstvGalleryModel::FavoriteRole).toBool(), 5'000);
    SstvImageRecord sidecar;
    QVERIFY2(SstvImageStore::loadMetadata(
                 saved.record.metadataPath, &sidecar, &error),
             qPrintable(error));
    QVERIFY(sidecar.favorite);

    QVERIFY(model.setFavorite(saved.record.id, false) != 0);
    QTRY_COMPARE_WITH_TIMEOUT(favoriteFinished.count(), 1, 5'000);
    QVERIFY(favoriteFinished.takeFirst().at(2).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!model.data(
        model.index(0), SstvGalleryModel::FavoriteRole).toBool(), 5'000);

    QTRY_VERIFY_WITH_TIMEOUT(
        model.retentionSettings().contains(
            QStringLiteral("automaticEnabled")), 5'000);
    QVERIFY(!model.retentionSettings().value(
        QStringLiteral("automaticEnabled")).toBool());
    QCOMPARE(model.requestAutomaticRetention(), quint64(0));
    QVariantMap settings = model.retentionSettings();
    settings.insert(QStringLiteral("maximumAgeDays"), 1);
    settings.insert(QStringLiteral("maximumDeletesPerRun"), 10);
    QSignalSpy settingsFinished(
        &model, &SstvGalleryModel::retentionSettingsFinished);
    QVERIFY(model.updateRetentionSettings(settings) != 0);
    QTRY_COMPARE_WITH_TIMEOUT(settingsFinished.count(), 1, 5'000);
    QVERIFY(settingsFinished.takeFirst().at(1).toBool());

    QSignalSpy quotaFinished(&model,
                             &SstvGalleryModel::quotaRefreshFinished);
    QVERIFY(model.refreshQuota() != 0);
    QTRY_VERIFY_WITH_TIMEOUT(quotaFinished.count() >= 1, 5'000);
    QVERIFY(model.quotaSummary().value(
        QStringLiteral("imageBytes")).toLongLong() > 0);
    QCOMPARE(model.quotaSummary().value(
                 QStringLiteral("recordCount")).toInt(), 1);

    QSignalSpy previewFinished(
        &model, &SstvGalleryModel::retentionPreviewFinished);
    QVERIFY(model.requestRetentionPreview() != 0);
    QTRY_COMPARE_WITH_TIMEOUT(previewFinished.count(), 1, 5'000);
    QVERIFY(previewFinished.takeFirst().at(1).toBool());
    QCOMPARE(model.retentionPreview().value(
                 QStringLiteral("recordCount")).toInt(), 1);
    const QString firstToken = model.retentionPreview().value(
        QStringLiteral("token")).toString();
    QSignalSpy applyFinished(&model,
                             &SstvGalleryModel::retentionApplyFinished);
    QVERIFY(model.applyRetentionPreview(firstToken,
                                        QStringLiteral("wrong")) != 0);
    QTRY_COMPARE_WITH_TIMEOUT(applyFinished.count(), 1, 5'000);
    QVERIFY(!applyFinished.takeFirst().at(2).toBool());
    QVERIFY(QFileInfo::exists(saved.record.imagePath));

    QVERIFY(model.requestRetentionPreview() != 0);
    QTRY_COMPARE_WITH_TIMEOUT(previewFinished.count(), 1, 5'000);
    QVERIFY(previewFinished.takeFirst().at(1).toBool());
    const QVariantMap plan = model.retentionPreview();
    QVERIFY(model.applyRetentionPreview(
        plan.value(QStringLiteral("token")).toString(),
        plan.value(QStringLiteral("confirmationPhrase")).toString()) != 0);
    QTRY_COMPARE_WITH_TIMEOUT(applyFinished.count(), 1, 5'000);
    QVERIFY(applyFinished.takeFirst().at(2).toBool());
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 5'000);
    QVERIFY(!QFileInfo::exists(saved.record.imagePath));
    QVERIFY(!QFileInfo::exists(saved.record.metadataPath));
    model.shutdown();
}

void TestSstvGalleryModel::qsoAssociationModelApiIsBoundedAndObservable()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY2(layout.ensure(&error), qPrintable(error));
    SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRealRequest(
        8, utcDateTime(QDate(2026, 8, 24), QTime(10, 11, 12)));
    request.record.relatedQsoId.clear();
    request.record.uploadState = SstvUploadState::NotRequested;
    request.record.remoteProvider.clear();
    request.record.remoteObjectId.clear();
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    WorkerSession session(layout);
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy inserted(session.worker,
                        &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 950);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(inserted.count(), 1, 5'000);
    QVERIFY2(inserted.takeFirst().at(2).toBool(),
             "fixture insert failed");

    SstvGalleryModel model;
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setStorageWorker(session.worker);
    waitForModelIdle(model);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.roleNames().value(SstvGalleryModel::RelatedQsoIdRole),
             QByteArrayLiteral("relatedQsoId"));
    QVERIFY(model.data(model.index(0),
                       SstvGalleryModel::RelatedQsoIdRole).toString().isEmpty());

    QSignalSpy finished(&model,
                        &SstvGalleryModel::qsoAssociationFinished);
    QCOMPARE(model.associateWithQso(
                 saved.record.id, QStringLiteral("../qso")), quint64(0));
    QCOMPARE(model.associateWithQso(
                 saved.record.id, QStringLiteral("qso\ncontrol")), quint64(0));
    QCOMPARE(model.associateWithQso(
                 saved.record.id, QString(257, QLatin1Char('x'))), quint64(0));
    QCOMPARE(model.associateWithQso(
                 QUuid::createUuid().toString(QUuid::WithoutBraces),
                 QStringLiteral("qso-id")), quint64(0));
    QCOMPARE(finished.count(), 0);

    const QString maximumLengthQsoId(256, QLatin1Char('q'));
    const quint64 associateRequest = model.associateWithQso(
        saved.record.id, maximumLengthQsoId);
    QVERIFY(associateRequest != 0);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5'000);
    const QList<QVariant> associated = finished.takeFirst();
    QCOMPARE(associated.at(0).toULongLong(), associateRequest);
    QCOMPARE(associated.at(1).toString(), saved.record.id);
    QCOMPARE(associated.at(2).toString(), maximumLengthQsoId);
    QVERIFY2(associated.at(3).toBool(),
             qPrintable(associated.at(4).toString()));
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(model.index(0),
                   SstvGalleryModel::RelatedQsoIdRole).toString(),
        maximumLengthQsoId, 5'000);

    const quint64 disassociateRequest = model.associateWithQso(
        saved.record.id, {});
    QVERIFY(disassociateRequest != 0);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5'000);
    const QList<QVariant> disassociated = finished.takeFirst();
    QCOMPARE(disassociated.at(0).toULongLong(), disassociateRequest);
    QCOMPARE(disassociated.at(1).toString(), saved.record.id);
    QVERIFY(disassociated.at(2).toString().isEmpty());
    QVERIFY2(disassociated.at(3).toBool(),
             qPrintable(disassociated.at(4).toString()));
    QTRY_VERIFY_WITH_TIMEOUT(
        model.data(model.index(0),
                   SstvGalleryModel::RelatedQsoIdRole).toString().isEmpty(),
        5'000);
    model.shutdown();
}

void TestSstvGalleryModel::userMetadataModelApiIsBoundedAndObservable()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const SstvStorageLayout layout(
        QDir(temporary.path()).absoluteFilePath(QStringLiteral("gallery")));
    QString error;
    QVERIFY2(layout.ensure(&error), qPrintable(error));
    SstvImageStore store(layout);
    SstvImageSaveRequest request = makeRealRequest(
        9, utcDateTime(QDate(2026, 8, 24), QTime(10, 12, 13)));
    request.record.note = QStringLiteral("initial Gallery note");
    request.record.tags = {QStringLiteral("initial")};
    const SstvImageSaveResult saved = store.save(request);
    QVERIFY2(saved.ok, qPrintable(saved.error));

    WorkerSession session(layout);
    QVERIFY2(session.start(&error), qPrintable(error));
    QSignalSpy inserted(session.worker,
                        &SstvStorageWorker::operationFinished);
    QVERIFY(QMetaObject::invokeMethod(
        session.worker,
        [worker = session.worker, record = saved.record]() {
            worker->insertRecord(record, 960);
        }, Qt::QueuedConnection));
    QTRY_COMPARE_WITH_TIMEOUT(inserted.count(), 1, 5'000);
    QVERIFY2(inserted.takeFirst().at(2).toBool(), "fixture insert failed");

    SstvGalleryModel model;
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setStorageWorker(session.worker);
    waitForModelIdle(model);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.roleNames().value(SstvGalleryModel::TagsRole),
             QByteArrayLiteral("tags"));
    QCOMPARE(model.roleNames().value(SstvGalleryModel::NoteRole),
             QByteArrayLiteral("note"));
    QCOMPARE(model.data(model.index(0), SstvGalleryModel::NoteRole).toString(),
             request.record.note);

    QSignalSpy finished(&model,
                        &SstvGalleryModel::userMetadataUpdateFinished);
    QCOMPARE(model.updateUserMetadata(
                 saved.record.id, QString(4'097, QLatin1Char('n')), {}),
             quint64(0));
    QCOMPARE(model.updateUserMetadata(
                 saved.record.id, QStringLiteral("too many tags"),
                 QStringList(33, QStringLiteral("tag"))), quint64(0));
    QCOMPARE(model.updateUserMetadata(
                 saved.record.id, QStringLiteral("duplicate tags"),
                 {QStringLiteral("Field"), QStringLiteral("field")}),
             quint64(0));
    QCOMPARE(model.updateUserMetadata(
                 saved.record.id, QStringLiteral("invalid tag"),
                 {QStringLiteral("line\nbreak")}), quint64(0));
    QCOMPARE(finished.count(), 0);
    QVERIFY(!model.errorString().isEmpty());

    const QString expectedNote = QStringLiteral("Field day operator note\n"
                                                "kept locally");
    const QString expectedAccent = QString::fromUtf8("M\xC3\xA1laga");
    const quint64 requestId = model.updateUserMetadata(
        saved.record.id, expectedNote,
        {QStringLiteral(" portable "),
         QString::fromUtf8("Ma\xCC\x81laga")});
    QVERIFY(requestId != 0);
    QCOMPARE(model.updateUserMetadata(
                 saved.record.id, QStringLiteral("second request"),
                 {QStringLiteral("second")}), quint64(0));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5'000);
    const QList<QVariant> result = finished.takeFirst();
    QCOMPARE(result.at(0).toULongLong(), requestId);
    QCOMPARE(result.at(1).toString(), saved.record.id);
    QCOMPARE(result.at(2).toString(), expectedNote);
    QCOMPARE(result.at(3).toStringList(),
             QStringList({QStringLiteral("portable"), expectedAccent}));
    QVERIFY2(result.at(4).toBool(), qPrintable(result.at(5).toString()));
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(model.index(0), SstvGalleryModel::NoteRole).toString(),
        expectedNote, 5'000);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(model.index(0), SstvGalleryModel::TagsRole).toStringList(),
        QStringList({QStringLiteral("portable"), expectedAccent}), 5'000);
    model.shutdown();
}

void TestSstvGalleryModel::thumbnailsAreBoundedCachedInvalidatedAndLifecycleSafe()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString imagePath = temporary.filePath(QStringLiteral("source.png"));
    QImage source(1024, 768, QImage::Format_RGB32);
    source.fill(QColor::fromRgb(30, 60, 90));
    QVERIFY(source.save(imagePath, "PNG"));

    SstvThumbnailLimits limits;
    limits.maximumPendingRequests = 1;
    limits.maximumRegisteredSources = 2;
    limits.maximumCacheBytes = 128 * 1024;
    limits.maximumCacheEntries = 1;
    limits.maximumEdge = 256;
    SstvThumbnailProvider provider(limits);
    const QString id = QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
    QVERIFY(provider.registerSource(id, imagePath));

    std::unique_ptr<QQuickImageResponse> first(
        provider.requestImageResponse(id, QSize(64, 64)));
    QSignalSpy firstFinished(first.get(), &QQuickImageResponse::finished);
    std::unique_ptr<QQuickImageResponse> queueFull(
        provider.requestImageResponse(id, QSize(64, 64)));
    QSignalSpy queueFullFinished(queueFull.get(), &QQuickImageResponse::finished);
    QVERIFY(firstFinished.wait(10000));
    QTRY_COMPARE_WITH_TIMEOUT(queueFullFinished.count(), 1, 5000);
    QVERIFY2(first->errorString().isEmpty(), qPrintable(first->errorString()));
    QVERIFY(queueFull->errorString().contains(QStringLiteral("full"),
                                              Qt::CaseInsensitive));
    std::unique_ptr<QQuickTextureFactory> texture(first->textureFactory());
    QVERIFY(texture != nullptr);
    QVERIFY(texture->textureSize().width() <= 64);
    QVERIFY(texture->textureSize().height() <= 64);
    QVERIFY(provider.lastWorkerThreadToken() != 0);
    QVERIFY(provider.lastWorkerThreadToken()
            != reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QTRY_VERIFY_WITH_TIMEOUT(provider.cacheEntries() == 1, 5000);
    QVERIFY(provider.cacheBytes() <= limits.maximumCacheBytes);

    std::unique_ptr<QQuickImageResponse> cached(
        provider.requestImageResponse(id, QSize(64, 64)));
    QSignalSpy cachedFinished(cached.get(), &QQuickImageResponse::finished);
    QVERIFY(cachedFinished.wait(5000));
    QVERIFY(cached->errorString().isEmpty());
    QVERIFY(cached->property("sstvFromCache").toBool());

    const QString secondPath = temporary.filePath(QStringLiteral("second.png"));
    source.fill(QColor::fromRgb(90, 20, 30));
    QVERIFY(source.save(secondPath, "PNG"));
    const QString secondId = QStringLiteral(
        "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff");
    QVERIFY(provider.registerSource(secondId, secondPath));
    std::unique_ptr<QQuickImageResponse> second(
        provider.requestImageResponse(secondId, QSize(64, 64)));
    QSignalSpy secondFinished(second.get(), &QQuickImageResponse::finished);
    QVERIFY(secondFinished.wait(5000));
    QVERIFY(second->errorString().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(provider.cacheEntries(), 1, 5000);
    std::unique_ptr<QQuickImageResponse> evictedFirst(
        provider.requestImageResponse(id, QSize(64, 64)));
    QSignalSpy evictedFirstFinished(evictedFirst.get(),
                                    &QQuickImageResponse::finished);
    QVERIFY(evictedFirstFinished.wait(5000));
    QVERIFY(!evictedFirst->property("sstvFromCache").toBool());
    QTRY_COMPARE_WITH_TIMEOUT(provider.cacheEntries(), 1, 5000);

    QSignalSpy invalidatedSpy(&provider,
                              &SstvThumbnailProvider::cacheInvalidated);
    provider.invalidateThumbnail(id);
    QVERIFY(invalidatedSpy.wait(5000));
    QTRY_COMPARE_WITH_TIMEOUT(provider.cacheEntries(), 0, 5000);
    std::unique_ptr<QQuickImageResponse> afterInvalidation(
        provider.requestImageResponse(id, QSize(64, 64)));
    QSignalSpy afterInvalidationFinished(
        afterInvalidation.get(), &QQuickImageResponse::finished);
    QVERIFY(afterInvalidationFinished.wait(5000));
    QVERIFY(!afterInvalidation->property("sstvFromCache").toBool());

    const QString missingId = QStringLiteral(
        "11111111-2222-4333-8444-555555555555");
    const QString missingPath = temporary.filePath(QStringLiteral("missing.png"));
    QVERIFY(provider.registerSource(missingId, missingPath));
    QCOMPARE(provider.registeredSourceCount(), 2);
    std::unique_ptr<QQuickImageResponse> sourceRegistryEvicted(
        provider.requestImageResponse(secondId, QSize(64, 64)));
    QSignalSpy sourceRegistryEvictedFinished(
        sourceRegistryEvicted.get(), &QQuickImageResponse::finished);
    QTRY_COMPARE_WITH_TIMEOUT(sourceRegistryEvictedFinished.count(), 1, 5000);
    QVERIFY(sourceRegistryEvicted->errorString().contains(
        QStringLiteral("not registered"), Qt::CaseInsensitive));
    std::unique_ptr<QQuickImageResponse> missing(
        provider.requestImageResponse(missingId, QSize(64, 64)));
    QSignalSpy missingFinished(missing.get(), &QQuickImageResponse::finished);
    QVERIFY(missingFinished.wait(5000));
    QVERIFY(missing->errorString().contains(QStringLiteral("missing"),
                                           Qt::CaseInsensitive));

    provider.invalidateThumbnail(id);
    QVERIFY(invalidatedSpy.wait(5000));
    std::unique_ptr<QQuickImageResponse> cancelled(
        provider.requestImageResponse(id, QSize(256, 256)));
    QSignalSpy cancelledFinished(cancelled.get(), &QQuickImageResponse::finished);
    cancelled->cancel();
    QTRY_COMPARE_WITH_TIMEOUT(cancelledFinished.count(), 1, 5000);
    QVERIFY(cancelled->errorString().contains(QStringLiteral("cancelled"),
                                             Qt::CaseInsensitive));
    QTRY_COMPARE_WITH_TIMEOUT(provider.pendingCount(), 0, 10000);

    std::unique_ptr<QQuickImageResponse> duringShutdown(
        provider.requestImageResponse(id, QSize(256, 256)));
    QSignalSpy shutdownResponseFinished(
        duringShutdown.get(), &QQuickImageResponse::finished);
    provider.shutdown();
    QTRY_COMPARE_WITH_TIMEOUT(shutdownResponseFinished.count(), 1, 5000);
    QCOMPARE(provider.pendingCount(), 0);
    QCOMPARE(provider.cacheEntries(), 0);
    QVERIFY(provider.shuttingDown());

    std::unique_ptr<QQuickImageResponse> afterShutdown(
        provider.requestImageResponse(id, QSize(64, 64)));
    QSignalSpy afterShutdownFinished(afterShutdown.get(),
                                     &QQuickImageResponse::finished);
    QTRY_COMPARE_WITH_TIMEOUT(afterShutdownFinished.count(), 1, 5000);
    QVERIFY(afterShutdown->errorString().contains(QStringLiteral("shut down"),
                                                 Qt::CaseInsensitive));
}

void TestSstvGalleryModel::thumbnailProviderRequestsAreReentrant()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString imagePath = temporary.filePath(
        QStringLiteral("concurrent-source.png"));
    QImage source(256, 192, QImage::Format_RGB32);
    source.fill(QColor::fromRgb(25, 100, 180));
    QVERIFY(source.save(imagePath, "PNG"));

    SstvThumbnailLimits limits;
    limits.maximumPendingRequests = 32;
    limits.maximumRegisteredSources = 4;
    limits.maximumCacheEntries = 16;
    SstvThumbnailProvider provider(limits);
    const QString id = QStringLiteral(
        "12345678-1234-4abc-8def-123456789abc");
    QVERIFY(provider.registerSource(id, imagePath));

    constexpr std::size_t requestCount = 16;
    std::array<QQuickImageResponse*, requestCount> responses {};
    QThread* const guiThread = QThread::currentThread();
    std::vector<std::thread> callers;
    callers.reserve(requestCount);
    for (std::size_t index = 0; index < requestCount; ++index) {
        callers.emplace_back([&, index]() {
            QQuickImageResponse* const response = provider.requestImageResponse(
                id, QSize(80 + static_cast<int>(index % 3U), 60));
            response->moveToThread(guiThread);
            responses.at(index) = response;
        });
    }
    for (std::thread& caller : callers) {
        caller.join();
    }

    std::vector<std::unique_ptr<QQuickImageResponse>> owners;
    std::vector<std::unique_ptr<QSignalSpy>> finished;
    owners.reserve(requestCount);
    finished.reserve(requestCount);
    for (QQuickImageResponse* response : responses) {
        QVERIFY(response != nullptr);
        owners.emplace_back(response);
        finished.push_back(std::make_unique<QSignalSpy>(
            response, &QQuickImageResponse::finished));
    }
    QTRY_COMPARE_WITH_TIMEOUT(provider.pendingCount(), 0, 10'000);
    for (std::size_t index = 0; index < requestCount; ++index) {
        QCOMPARE(finished.at(index)->count(), 1);
        QVERIFY2(owners.at(index)->errorString().isEmpty(),
                 qPrintable(owners.at(index)->errorString()));
    }
    QCOMPARE(provider.registeredSourceCount(), 1);
    provider.shutdown();

    SstvThumbnailProvider racingProvider(limits);
    QVERIFY(racingProvider.registerSource(id, imagePath));
    constexpr std::size_t racingCount = 8;
    std::array<QQuickImageResponse*, racingCount> racingResponses {};
    std::atomic_int ready {0};
    std::atomic_bool go {false};
    callers.clear();
    callers.reserve(racingCount);
    for (std::size_t index = 0; index < racingCount; ++index) {
        callers.emplace_back([&, index]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            QQuickImageResponse* const response
                = racingProvider.requestImageResponse(id, QSize(96, 72));
            response->moveToThread(guiThread);
            racingResponses.at(index) = response;
        });
    }
    QTRY_COMPARE_WITH_TIMEOUT(ready.load(std::memory_order_acquire),
                              static_cast<int>(racingCount), 5'000);
    go.store(true, std::memory_order_release);
    racingProvider.shutdown();
    for (std::thread& caller : callers) {
        caller.join();
    }
    std::vector<std::unique_ptr<QQuickImageResponse>> racingOwners;
    racingOwners.reserve(racingCount);
    for (QQuickImageResponse* response : racingResponses) {
        QVERIFY(response != nullptr);
        racingOwners.emplace_back(response);
    }
    const auto allRacingResponsesResolved = [&racingOwners]() {
        for (const auto& response : racingOwners) {
            if (!response->errorString().isEmpty()) {
                continue;
            }
            std::unique_ptr<QQuickTextureFactory> texture(
                response->textureFactory());
            if (!texture) {
                return false;
            }
        }
        return true;
    };
    QTRY_VERIFY_WITH_TIMEOUT(allRacingResponsesResolved(), 5'000);
    QCOMPARE(racingProvider.pendingCount(), 0);
}

QTEST_MAIN(TestSstvGalleryModel)

#include "test_sstv_gallery_model.moc"
