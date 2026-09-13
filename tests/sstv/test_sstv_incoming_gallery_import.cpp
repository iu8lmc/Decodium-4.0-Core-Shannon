// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/models/SstvShareController.h"
#include "src/sstv/storage/SstvStorageWorker.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimeZone>
#include <QUuid>

#include <optional>

using decodium::sstv::SstvImageRecord;
using decodium::sstv::SstvImageSaveRequest;
using decodium::sstv::SstvImageStore;
using decodium::sstv::SstvIncomingImportFailure;
using decodium::sstv::SstvIncomingImportResult;
using decodium::sstv::SstvStorageLayout;
using decodium::sstv::SstvStorageWorker;
using decodium::sstv::SstvShareController;
using decodium::sstv::sharing::SstvValidatedIncomingHandoff;

namespace {

struct IncomingFixture final
{
    QString storageRoot;
    QString validatedRoot;
    SstvValidatedIncomingHandoff handoff;
    QVariantMap map;
    QByteArray pngBytes;
};

QVariantMap handoffMap(const SstvValidatedIncomingHandoff& handoff)
{
    return {
        {QStringLiteral("schemaVersion"), handoff.schemaVersion},
        {QStringLiteral("transferId"), handoff.transferId},
        {QStringLiteral("providerId"), handoff.providerId},
        {QStringLiteral("incomingId"), handoff.incomingId},
        {QStringLiteral("senderId"), handoff.senderId},
        {QStringLiteral("safeDisplayFilename"),
         handoff.safeDisplayFilename},
        {QStringLiteral("sstvMode"), handoff.sstvMode},
        {QStringLiteral("sourceMimeType"), handoff.sourceMimeType},
        {QStringLiteral("sourceSha256"), handoff.sourceSha256},
        {QStringLiteral("sourceByteSize"),
         QVariant::fromValue(handoff.sourceByteSize)},
        {QStringLiteral("stagedCanonicalPath"),
         handoff.stagedCanonicalPath},
        {QStringLiteral("stagedMimeType"), handoff.stagedMimeType},
        {QStringLiteral("stagedSha256"), handoff.stagedSha256},
        {QStringLiteral("stagedByteSize"),
         QVariant::fromValue(handoff.stagedByteSize)},
        {QStringLiteral("width"), QVariant::fromValue(handoff.width)},
        {QStringLiteral("height"), QVariant::fromValue(handoff.height)},
        {QStringLiteral("receivedUtc"), handoff.receivedUtc},
        {QStringLiteral("expiresUtc"), handoff.expiresUtc},
    };
}

bool makeIncomingFixture(const QString& temporaryRoot,
                         const QString& incomingId,
                         IncomingFixture* output)
{
    if (!output) {
        return false;
    }
    IncomingFixture fixture;
    fixture.storageRoot = QDir(temporaryRoot).absoluteFilePath(
        QStringLiteral("native-sstv"));
    QString error;
    const SstvStorageLayout layout(fixture.storageRoot);
    if (!layout.ensure(&error)) {
        return false;
    }
    fixture.validatedRoot = QDir(fixture.storageRoot).absoluteFilePath(
        QStringLiteral("sharing/downloads/validated"));
    if (!QDir().mkpath(fixture.validatedRoot)
        || !QFile::setPermissions(
            fixture.validatedRoot,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ExeOwner)) {
        return false;
    }

    SstvValidatedIncomingHandoff handoff;
    handoff.transferId = QUuid::createUuid().toString(
        QUuid::WithoutBraces).toLower();
    handoff.providerId = QStringLiteral("rest-provider");
    handoff.incomingId = incomingId;
    handoff.senderId = QStringLiteral("sender-0001");
    handoff.safeDisplayFilename = QStringLiteral("remote-test-card.png");
    handoff.sstvMode = QStringLiteral("Martin M1");
    handoff.sourceMimeType = QStringLiteral("image/png");
    handoff.stagedMimeType = QStringLiteral("image/png");
    handoff.width = 48U;
    handoff.height = 32U;
    handoff.receivedUtc = QDateTime(
        QDate(2026, 8, 24), QTime(10, 11, 12, 345),
        QTimeZone(QTimeZone::UTC));
    handoff.expiresUtc = handoff.receivedUtc.addDays(7);

    QImage image(static_cast<int>(handoff.width),
                 static_cast<int>(handoff.height), QImage::Format_RGB888);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor::fromRgb(
                (x * 13 + y * 3) % 256,
                (y * 17 + x * 5) % 256,
                (x * 7 + y * 11) % 256));
        }
    }
    const QString stagedPath = QDir(fixture.validatedRoot).absoluteFilePath(
        handoff.transferId + QStringLiteral(".png"));
    QSaveFile staged(stagedPath);
    staged.setDirectWriteFallback(false);
    if (!staged.open(QIODevice::WriteOnly)
        || !staged.setPermissions(QFileDevice::ReadOwner
                                  | QFileDevice::WriteOwner)
        || !image.save(&staged, "PNG") || !staged.commit()) {
        staged.cancelWriting();
        return false;
    }
    QFile bytesFile(stagedPath);
    if (!bytesFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    fixture.pngBytes = bytesFile.readAll();
    bytesFile.close();
    if (fixture.pngBytes.isEmpty()) {
        return false;
    }
    const QString sha256 = QString::fromLatin1(
        QCryptographicHash::hash(fixture.pngBytes,
                                 QCryptographicHash::Sha256).toHex());
    handoff.sourceSha256 = sha256;
    handoff.sourceByteSize = static_cast<quint64>(fixture.pngBytes.size());
    handoff.stagedSha256 = sha256;
    handoff.stagedByteSize = static_cast<quint64>(fixture.pngBytes.size());
    handoff.stagedCanonicalPath = QFileInfo(stagedPath).canonicalFilePath();
    if (handoff.stagedCanonicalPath.isEmpty()) {
        return false;
    }
    fixture.handoff = handoff;
    fixture.map = handoffMap(handoff);
    *output = std::move(fixture);
    return true;
}

class WorkerHarness final
{
public:
    explicit WorkerHarness(const QString& storageRoot, bool initializeNow)
    {
        const SstvStorageLayout layout(storageRoot);
        worker = new SstvStorageWorker(layout.databasePath(), storageRoot);
        worker->moveToThread(&thread);
        thread.setObjectName(QStringLiteral("incoming Gallery import test"));
        thread.start();
        if (initializeNow) {
            initialize();
        }
    }

    ~WorkerHarness()
    {
        shutdown();
    }

    bool initialize()
    {
        if (!worker || worker->isInitialized()) {
            return worker && worker->isInitialized();
        }
        const bool invoked = QMetaObject::invokeMethod(
            worker, [this]() { worker->initialize(); },
            Qt::BlockingQueuedConnection);
        return invoked && worker->isInitialized();
    }

    void shutdown()
    {
        if (!worker) {
            return;
        }
        if (worker->isInitialized()) {
            QMetaObject::invokeMethod(
                worker, [this]() { worker->shutdown(); },
                Qt::BlockingQueuedConnection);
        }
        thread.quit();
        thread.wait();
        delete worker;
        worker = nullptr;
    }

    QThread thread;
    SstvStorageWorker* worker {nullptr};
};

std::optional<SstvIncomingImportResult> nextResult(QSignalSpy& spy)
{
    if (spy.isEmpty() && !spy.wait(5'000)) {
        return {};
    }
    const QList<QVariant> arguments = spy.takeFirst();
    if (arguments.size() != 1) {
        return {};
    }
    return qvariant_cast<SstvIncomingImportResult>(arguments.constFirst());
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

} // namespace

class TestSstvIncomingGalleryImport final : public QObject
{
    Q_OBJECT

private slots:
    void controllerQueueRetrySuccessDuplicateAndRestart()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        IncomingFixture fixture;
        QVERIFY(makeIncomingFixture(temporary.path(),
                                    QStringLiteral("incoming-success"),
                                    &fixture));

        SstvShareController controller(nullptr);
        WorkerHarness first(fixture.storageRoot, false);
        QVERIFY(QObject::connect(
            &controller, &SstvShareController::incomingHandoffReady,
            first.worker,
            &SstvStorageWorker::importValidatedIncomingHandoff,
            Qt::QueuedConnection));
        QSignalSpy results(first.worker,
                           &SstvStorageWorker::incomingImportFinished);
        QVERIFY(results.isValid());

        emit controller.incomingHandoffReady(fixture.map);
        const auto unavailable = nextResult(results);
        QVERIFY(unavailable.has_value());
        QVERIFY(!unavailable->ok);
        QVERIFY(unavailable->retryable);
        QCOMPARE(unavailable->failure,
                 SstvIncomingImportFailure::StorageUnavailable);
        QVERIFY(QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        QVERIFY(first.initialize());
        emit controller.incomingHandoffReady(fixture.map);
        const auto imported = nextResult(results);
        QVERIFY(imported.has_value());
        QVERIFY2(imported->ok, qPrintable(imported->error));
        QVERIFY(!imported->retryable);
        QVERIFY(!imported->idempotent);
        QCOMPARE(imported->record.id, fixture.handoff.transferId);
        QCOMPARE(imported->record.mode, fixture.handoff.sstvMode);
        QCOMPARE(imported->record.remoteProvider,
                 fixture.handoff.providerId);
        QCOMPARE(imported->record.remoteObjectId,
                 fixture.handoff.incomingId);
        QCOMPARE(imported->record.remoteCallsign, QString());
        QCOMPARE(imported->record.note, QString());
        QCOMPARE(imported->record.sha256,
                 QByteArray::fromHex(
                     fixture.handoff.stagedSha256.toLatin1()));
        QCOMPARE(readAll(imported->record.imagePath), fixture.pngBytes);
        QVERIFY(QFileInfo::exists(imported->record.metadataPath));
        QVERIFY(!QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        emit controller.incomingHandoffReady(fixture.map);
        const auto duplicate = nextResult(results);
        QVERIFY(duplicate.has_value());
        QVERIFY(duplicate->ok);
        QVERIFY(duplicate->idempotent);
        QCOMPARE(duplicate->record, imported->record);

        QVariantMap conflictingReplay = fixture.map;
        conflictingReplay.insert(QStringLiteral("stagedSha256"),
                                 QString(64, QLatin1Char('0')));
        emit controller.incomingHandoffReady(conflictingReplay);
        const auto conflict = nextResult(results);
        QVERIFY(conflict.has_value());
        QVERIFY(!conflict->ok);
        QVERIFY(!conflict->retryable);
        QCOMPARE(conflict->failure, SstvIncomingImportFailure::Conflict);
        QCOMPARE(readAll(imported->record.imagePath), fixture.pngBytes);

        const QString finalImage = imported->record.imagePath;
        first.shutdown();
        WorkerHarness restarted(fixture.storageRoot, true);
        QVERIFY(restarted.worker->isInitialized());
        QVERIFY(QObject::connect(
            &controller, &SstvShareController::incomingHandoffReady,
            restarted.worker,
            &SstvStorageWorker::importValidatedIncomingHandoff,
            Qt::QueuedConnection));
        QSignalSpy restartResults(
            restarted.worker, &SstvStorageWorker::incomingImportFinished);
        emit controller.incomingHandoffReady(fixture.map);
        const auto replayed = nextResult(restartResults);
        QVERIFY(replayed.has_value());
        QVERIFY(replayed->ok);
        QVERIFY(replayed->idempotent);
        QCOMPARE(replayed->record.id, fixture.handoff.transferId);
        QCOMPARE(readAll(finalImage), fixture.pngBytes);
        controller.shutdown();
    }

    void forgedSchemaTamperAndDimensionMismatchFailClosed()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        IncomingFixture fixture;
        QVERIFY(makeIncomingFixture(temporary.path(),
                                    QStringLiteral("incoming-tamper"),
                                    &fixture));
        WorkerHarness harness(fixture.storageRoot, true);
        QSignalSpy results(harness.worker,
                           &SstvStorageWorker::incomingImportFinished);

        QVariantMap forged = fixture.map;
        forged.insert(QStringLiteral("transmit"), true);
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, forged]() {
                worker->importValidatedIncomingHandoff(forged);
            },
            Qt::QueuedConnection);
        const auto forgedResult = nextResult(results);
        QVERIFY(forgedResult.has_value());
        QVERIFY(!forgedResult->ok);
        QVERIFY(!forgedResult->retryable);
        QCOMPARE(forgedResult->failure,
                 SstvIncomingImportFailure::InvalidHandoff);
        QVERIFY(QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        QVariantMap wrongDimensions = fixture.map;
        wrongDimensions.insert(
            QStringLiteral("width"),
            QVariant::fromValue(fixture.handoff.width + 1U));
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, wrongDimensions]() {
                worker->importValidatedIncomingHandoff(wrongDimensions);
            },
            Qt::QueuedConnection);
        const auto dimensionsResult = nextResult(results);
        QVERIFY(dimensionsResult.has_value());
        QVERIFY(!dimensionsResult->ok);
        QCOMPARE(dimensionsResult->failure,
                 SstvIncomingImportFailure::IntegrityFailure);
        QVERIFY(QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        QFile tampered(fixture.handoff.stagedCanonicalPath);
        QVERIFY(tampered.open(QIODevice::Append));
        QCOMPARE(tampered.write("x", 1), qint64(1));
        tampered.close();
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, map = fixture.map]() {
                worker->importValidatedIncomingHandoff(map);
            },
            Qt::QueuedConnection);
        const auto tamperResult = nextResult(results);
        QVERIFY(tamperResult.has_value());
        QVERIFY(!tamperResult->ok);
        QVERIFY(!tamperResult->retryable);
        QCOMPARE(tamperResult->failure,
                 SstvIncomingImportFailure::IntegrityFailure);
        QVERIFY(QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        QVariantMap traversal = fixture.map;
        traversal.insert(
            QStringLiteral("stagedCanonicalPath"),
            QDir(temporary.path()).absoluteFilePath(QStringLiteral("outside.png")));
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, traversal]() {
                worker->importValidatedIncomingHandoff(traversal);
            },
            Qt::QueuedConnection);
        const auto traversalResult = nextResult(results);
        QVERIFY(traversalResult.has_value());
        QCOMPARE(traversalResult->failure,
                 SstvIncomingImportFailure::UnsafeStagingPath);
    }

    void cleanupRaceRetainsBytesAndRetryFinishesOwnershipTransfer()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        IncomingFixture fixture;
        QVERIFY(makeIncomingFixture(temporary.path(),
                                    QStringLiteral("incoming-cleanup"),
                                    &fixture));
        WorkerHarness harness(fixture.storageRoot, true);
        QSignalSpy results(harness.worker,
                           &SstvStorageWorker::incomingImportFinished);
        const QString heldPath = fixture.handoff.stagedCanonicalPath
            + QStringLiteral(".held");
        const QMetaObject::Connection race = QObject::connect(
            harness.worker, &SstvStorageWorker::recordChanged,
            harness.worker,
            [stagedPath = fixture.handoff.stagedCanonicalPath, heldPath]() {
                if (QFile::rename(stagedPath, heldPath)) {
                    QDir().mkdir(stagedPath);
                }
            },
            Qt::DirectConnection);

        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, map = fixture.map]() {
                worker->importValidatedIncomingHandoff(map);
            },
            Qt::QueuedConnection);
        const auto pending = nextResult(results);
        QVERIFY(pending.has_value());
        QVERIFY(!pending->ok);
        QVERIFY2(pending->retryable, qPrintable(pending->error));
        QCOMPARE(pending->failure,
                 SstvIncomingImportFailure::CleanupPending);
        QVERIFY(!pending->record.id.isEmpty());
        QVERIFY(QFileInfo::exists(pending->record.imagePath));
        QCOMPARE(readAll(heldPath), fixture.pngBytes);

        QObject::disconnect(race);
        QVERIFY(QDir().rmdir(fixture.handoff.stagedCanonicalPath));
        QVERIFY(QFile::rename(heldPath,
                              fixture.handoff.stagedCanonicalPath));
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, map = fixture.map]() {
                worker->importValidatedIncomingHandoff(map);
            },
            Qt::QueuedConnection);
        const auto cleaned = nextResult(results);
        QVERIFY(cleaned.has_value());
        QVERIFY(cleaned->ok);
        QVERIFY(cleaned->idempotent);
        QVERIFY(!QFileInfo::exists(fixture.handoff.stagedCanonicalPath));
        QCOMPARE(cleaned->record.id, pending->record.id);
    }

    void symbolicLinkStagingIsRejectedWhenSupported()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        IncomingFixture fixture;
        QVERIFY(makeIncomingFixture(temporary.path(),
                                    QStringLiteral("incoming-symlink"),
                                    &fixture));
        const QString outside = QDir(temporary.path()).absoluteFilePath(
            QStringLiteral("outside.png"));
        QSaveFile outsideFile(outside);
        QVERIFY(outsideFile.open(QIODevice::WriteOnly));
        QCOMPARE(outsideFile.write(fixture.pngBytes),
                 fixture.pngBytes.size());
        QVERIFY(outsideFile.commit());
        QVERIFY(QFile::remove(fixture.handoff.stagedCanonicalPath));
        if (!QFile::link(outside, fixture.handoff.stagedCanonicalPath)
            || !QFileInfo(fixture.handoff.stagedCanonicalPath).isSymLink()) {
            QSKIP("symbolic links are unavailable on this test platform");
        }

        WorkerHarness harness(fixture.storageRoot, true);
        QSignalSpy results(harness.worker,
                           &SstvStorageWorker::incomingImportFinished);
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, map = fixture.map]() {
                worker->importValidatedIncomingHandoff(map);
            },
            Qt::QueuedConnection);
        const auto rejected = nextResult(results);
        QVERIFY(rejected.has_value());
        QVERIFY(!rejected->ok);
        QVERIFY(!rejected->retryable);
        QCOMPARE(rejected->failure,
                 SstvIncomingImportFailure::UnsafeStagingPath);
        QVERIFY(QFileInfo(fixture.handoff.stagedCanonicalPath).isSymLink());
        QCOMPARE(readAll(outside), fixture.pngBytes);
    }

    void embeddedPngTextIsNotImplicitlyImported()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        IncomingFixture fixture;
        QVERIFY(makeIncomingFixture(temporary.path(),
                                    QStringLiteral("incoming-metadata"),
                                    &fixture));
        const QImage image = QImage::fromData(fixture.pngBytes, "PNG");
        QVERIFY(!image.isNull());
        QSaveFile staged(fixture.handoff.stagedCanonicalPath);
        staged.setDirectWriteFallback(false);
        QVERIFY(staged.open(QIODevice::WriteOnly));
        QVERIFY(staged.setPermissions(QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner));
        QImageWriter writer(&staged, QByteArrayLiteral("png"));
        writer.setText(QStringLiteral("Author"),
                       QStringLiteral("remote-personal-metadata"));
        QVERIFY2(writer.write(image), qPrintable(writer.errorString()));
        QVERIFY(staged.commit());
        fixture.pngBytes = readAll(fixture.handoff.stagedCanonicalPath);
        QVERIFY(!fixture.pngBytes.isEmpty());
        fixture.handoff.stagedByteSize = static_cast<quint64>(
            fixture.pngBytes.size());
        fixture.handoff.stagedSha256 = QString::fromLatin1(
            QCryptographicHash::hash(fixture.pngBytes,
                                     QCryptographicHash::Sha256).toHex());
        fixture.map = handoffMap(fixture.handoff);

        WorkerHarness harness(fixture.storageRoot, true);
        QSignalSpy results(harness.worker,
                           &SstvStorageWorker::incomingImportFinished);
        QMetaObject::invokeMethod(
            harness.worker,
            [worker = harness.worker, map = fixture.map]() {
                worker->importValidatedIncomingHandoff(map);
            },
            Qt::QueuedConnection);
        const auto rejected = nextResult(results);
        QVERIFY(rejected.has_value());
        QVERIFY(!rejected->ok);
        QVERIFY(!rejected->retryable);
        QCOMPARE(rejected->failure,
                 SstvIncomingImportFailure::IntegrityFailure);
        QCOMPARE(readAll(fixture.handoff.stagedCanonicalPath),
                 fixture.pngBytes);
    }

    void restartAdoptsOnlyExactPostFilePreDatabaseOrphan()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        IncomingFixture fixture;
        QVERIFY(makeIncomingFixture(temporary.path(),
                                    QStringLiteral("incoming-orphan"),
                                    &fixture));
        SstvImageSaveRequest request;
        request.record.id = fixture.handoff.transferId;
        request.record.category = decodium::sstv::SstvImageCategory::Imported;
        request.record.capturedAtUtc = fixture.handoff.receivedUtc;
        request.record.eventAtUtc = fixture.handoff.receivedUtc;
        request.record.mode = fixture.handoff.sstvMode;
        request.record.source = QStringLiteral("remote-sharing");
        request.record.completionPercent = 100;
        request.record.complete = true;
        request.record.remote = true;
        request.record.remoteProvider = fixture.handoff.providerId;
        request.record.remoteObjectId = fixture.handoff.incomingId;
        request.record.expiresAtUtc = fixture.handoff.expiresUtc;
        request.record.originalWidth = static_cast<int>(fixture.handoff.width);
        request.record.originalHeight = static_cast<int>(fixture.handoff.height);
        request.image = QImage::fromData(fixture.pngBytes, "PNG")
                            .convertToFormat(QImage::Format_RGB888);
        request.fileNameTemplate = QStringLiteral(
            "remote_{date}_{time}_{mode}_{id}");
        QVERIFY(!request.image.isNull());

        const SstvImageStore store(SstvStorageLayout(fixture.storageRoot));
        const auto orphan = store.savePreservingPng(request, fixture.pngBytes);
        QVERIFY2(orphan.ok, qPrintable(orphan.error));
        QVERIFY(QFileInfo::exists(orphan.record.imagePath));
        QVERIFY(QFileInfo::exists(orphan.record.metadataPath));
        QVERIFY(QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        WorkerHarness restarted(fixture.storageRoot, true);
        QSignalSpy results(restarted.worker,
                           &SstvStorageWorker::incomingImportFinished);
        QMetaObject::invokeMethod(
            restarted.worker,
            [worker = restarted.worker, map = fixture.map]() {
                worker->importValidatedIncomingHandoff(map);
            },
            Qt::QueuedConnection);
        const auto recovered = nextResult(results);
        QVERIFY(recovered.has_value());
        QVERIFY2(recovered->ok, qPrintable(recovered->error));
        QCOMPARE(recovered->record.id, fixture.handoff.transferId);
        QCOMPARE(recovered->record.imagePath, orphan.record.imagePath);
        QCOMPARE(readAll(recovered->record.imagePath), fixture.pngBytes);
        QVERIFY(!QFileInfo::exists(fixture.handoff.stagedCanonicalPath));

        QSignalSpy fetched(restarted.worker,
                           &SstvStorageWorker::recordFetched);
        QMetaObject::invokeMethod(
            restarted.worker,
            [worker = restarted.worker, id = fixture.handoff.transferId]() {
                worker->fetchRecord(id, 77U);
            },
            Qt::QueuedConnection);
        QVERIFY(!fetched.isEmpty() || fetched.wait(5'000));
        QCOMPARE(fetched.constFirst().at(0).toULongLong(), quint64(77));
        QVERIFY(fetched.constFirst().at(1).toBool());
        QCOMPARE(qvariant_cast<SstvImageRecord>(fetched.constFirst().at(2)),
                 recovered->record);
    }
};

QTEST_MAIN(TestSstvIncomingGalleryImport)
#include "test_sstv_incoming_gallery_import.moc"
