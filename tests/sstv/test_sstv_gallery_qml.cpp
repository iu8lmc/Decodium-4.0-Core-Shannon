// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/models/SstvGalleryModel.h"
#include "src/sstv/models/SstvThumbnailProvider.h"
#include "src/sstv/storage/SstvImageStorage.h"
#include "src/sstv/storage/SstvStorageWorker.h"

#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimeZone>
#include <QUrl>
#include <QVariantMap>

using namespace decodium::sstv;

namespace {

class GalleryEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* sstvGallery READ sstvGallery CONSTANT)

public:
    explicit GalleryEngineFixture(SstvGalleryModel* gallery,
                                  QObject* parent = nullptr)
        : QObject(parent)
        , m_gallery(gallery)
    {
    }

    QObject* sstvGallery() const noexcept { return m_gallery; }

private:
    SstvGalleryModel* const m_gallery;
};

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

    bool start(QString* error)
    {
        QSignalSpy initialized(worker, &SstvStorageWorker::initialized);
        thread.start();
        if (!QMetaObject::invokeMethod(worker, &SstvStorageWorker::initialize,
                                      Qt::QueuedConnection)
            || !initialized.wait(5'000) || initialized.isEmpty()) {
            if (error) {
                *error = QStringLiteral("storage initialization timed out");
            }
            return false;
        }
        const QList<QVariant> arguments = initialized.takeFirst();
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
        QMetaObject::invokeMethod(worker, &SstvStorageWorker::shutdown,
                                  Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait();
        worker = nullptr;
    }

    QThread thread;
    SstvStorageWorker* worker {nullptr};
};

QString qmlErrors(const QList<QQmlError>& errors)
{
    QStringList lines;
    lines.reserve(errors.size());
    for (const QQmlError& error : errors) {
        lines.push_back(error.toString());
    }
    return lines.join(QLatin1Char('\n'));
}

QQuickItem* findQuickItem(QQuickItem* root, const QString& objectName)
{
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == objectName) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = findQuickItem(child, objectName)) {
            return found;
        }
    }
    return nullptr;
}

SstvImageSaveRequest makeImageRequest()
{
    SstvImageSaveRequest request;
    request.record.category = SstvImageCategory::Received;
    request.record.capturedAtUtc = QDateTime(
        QDate(2026, 8, 24), QTime(18, 30), QTimeZone(QTimeZone::UTC));
    request.record.mode = QStringLiteral("Martin M1");
    request.record.visCode = 44;
    request.record.visValid = true;
    request.record.remoteCallsign = QStringLiteral("9H1TEST");
    request.record.localCallsign = QStringLiteral("IU8LMC");
    request.record.source = QStringLiteral("offscreen render test");
    request.record.frequencyHz = 14'230'000;
    request.record.audioFrequencyHz = -18;
    request.record.completionPercent = 100;
    request.record.complete = true;
    request.record.remote = true;
    request.record.qualityMetrics = {
        {QStringLiteral("snrDb"), 18.25},
        {QStringLiteral("syncConfidence"), 0.97},
        {QStringLiteral("lineDropRate"), 0.001}
    };
    request.record.slantCorrectionPpm = 12.5;
    request.record.tags = {QStringLiteral("rendered")};
    request.record.note = QStringLiteral("Initial Gallery UI note");
    request.image = QImage(320, 256, QImage::Format_RGB32);
    for (int y = 0; y < request.image.height(); ++y) {
        for (int x = 0; x < request.image.width(); ++x) {
            request.image.setPixelColor(
                x, y, QColor::fromHsv((x + y) % 360, 190,
                                      100 + ((x * 155) / 319)));
        }
    }
    request.fileNameTemplate = QStringLiteral("{id}");
    return request;
}

} // namespace

class TestSstvGalleryQml final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("IU8LMC"));
        QCoreApplication::setApplicationName(
            QStringLiteral("Decodium SSTV Gallery QML Test"));
        QStandardPaths::setTestModeEnabled(true);
        qRegisterMetaType<SstvImageRecord>();
        qRegisterMetaType<SstvGalleryPage>();
        qRegisterMetaType<SstvStorageOperation>();
        qRegisterMetaType<SstvRetentionSettings>();
        qRegisterMetaType<SstvQuotaSummary>();
        qRegisterMetaType<SstvRetentionPlan>();
    }

    void pageCreatesFiltersAndRendersThumbnailOffscreen()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const SstvStorageLayout layout(
            temporary.filePath(QStringLiteral("sstv")));
        const SstvImageStore store(layout);
        const SstvImageSaveResult saved = store.save(makeImageRequest());
        QVERIFY2(saved.ok, qPrintable(saved.error));

        WorkerSession session(layout);
        QString error;
        QVERIFY2(session.start(&error), qPrintable(error));
        QSignalSpy operation(session.worker,
                             &SstvStorageWorker::operationFinished);
        QVERIFY(QMetaObject::invokeMethod(
            session.worker,
            [worker = session.worker, record = saved.record]() mutable {
                worker->insertRecord(std::move(record), 1);
            }, Qt::QueuedConnection));
        QTRY_COMPARE_WITH_TIMEOUT(operation.count(), 1, 5'000);
        QVERIFY2(operation.at(0).at(2).toBool(),
                 qPrintable(operation.at(0).at(3).toString()));

        SstvGalleryModel model;
        model.setStorageWorker(session.worker);
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 5'000);
        QCOMPARE(model.rowCount(), 1);
        const QString specialPath = temporary.filePath(
            QStringLiteral("space # 100% qualità 日本.png"));
        const QUrl specialUrl = model.localFileUrl(specialPath);
        QCOMPARE(specialUrl.toLocalFile(), specialPath);
        QVERIFY(specialUrl.toEncoded().contains("%23"));
        QVERIFY(specialUrl.toEncoded().contains("%25"));
#if defined(Q_OS_WIN)
        for (const QString& nativePath : {
                 QStringLiteral("C:/SSTV/space # 100% qualità 日本.png"),
                 QStringLiteral("//server/share/space # 100% qualità 日本.png")}) {
            QCOMPARE(model.localFileUrl(nativePath).toLocalFile(), nativePath);
        }
#endif
        GalleryEngineFixture fixture(&model);

        QQmlEngine engine;
        auto* provider = new SstvThumbnailProvider;
        model.setThumbnailProvider(provider);
        QSignalSpy thumbnailCompleted(
            provider, &SstvThumbnailProvider::requestCompleted);
        engine.addImageProvider(QStringLiteral("decodium-sstv-gallery"),
                                provider);
        QStringList runtimeWarnings;
        connect(&engine, &QQmlEngine::warnings, this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });

        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_GALLERY_QML_SOURCE);
        QQmlComponent component(&engine, QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(),
                 qPrintable(qmlErrors(component.errors())));
        QVariantMap initial;
        initial.insert(QStringLiteral("engine"),
                       QVariant::fromValue(static_cast<QObject*>(&fixture)));
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* page = qobject_cast<QQuickItem*>(object.data());
        QVERIFY(page);

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#111c25")));
        window.resize(940, 650);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(940.0, 650.0));
        window.show();

        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(thumbnailCompleted.count() > 0, 5'000);
        QVERIFY(provider->lastWorkerThreadToken() != 0);
        QVERIFY(provider->lastWorkerThreadToken()
                != reinterpret_cast<quintptr>(QThread::currentThreadId()));

        QObject* search = page->findChild<QObject*>(
            QStringLiteral("sstvGallerySearch"));
        QObject* removeButton = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryRemoveIndex"));
        QObject* deleteFilesButton = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryDeleteFiles"));
        QObject* deleteFilesDialog = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryDeleteFilesDialog"));
        QObject* exportDialog = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryExportDialog"));
        QObject* retentionDialog = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryRetentionDialog"));
        QObject* previewRetention = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryPreviewRetention"));
        QObject* refreshQuota = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryRefreshQuota"));
        QObject* confirmation = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryRetentionConfirmation"));
        QObject* applyRetention = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryApplyRetention"));
        QObject* gridMode = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryGridMode"));
        QObject* listMode = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryListMode"));
        QObject* metadataPanel = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryMetadataPanel"));
        QObject* metadataEditButton = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryEditSelectedMetadata"));
        QObject* metadataDialog = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryEditMetadataDialog"));
        QObject* metadataTags = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryMetadataTags"));
        QObject* metadataNote = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryMetadataNote"));
        QObject* saveMetadata = page->findChild<QObject*>(
            QStringLiteral("sstvGallerySaveMetadata"));
        QObject* gridView = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryView"));
        QObject* listView = page->findChild<QObject*>(
            QStringLiteral("sstvGalleryListView"));
        QVERIFY(search);
        QVERIFY(removeButton);
        QVERIFY(deleteFilesButton);
        QVERIFY(deleteFilesDialog);
        QVERIFY(exportDialog);
        QVERIFY(retentionDialog);
        QVERIFY(previewRetention);
        QVERIFY(refreshQuota);
        QVERIFY(confirmation);
        QVERIFY(applyRetention);
        QVERIFY(gridMode);
        QVERIFY(listMode);
        QVERIFY(metadataPanel);
        QVERIFY(metadataEditButton);
        QVERIFY(metadataDialog);
        QVERIFY(metadataTags);
        QVERIFY(metadataNote);
        QVERIFY(saveMetadata);
        QVERIFY(gridView);
        QVERIFY(listView);
        QVERIFY(page->property("gridViewMode").toBool());
        QVERIFY(gridView->property("visible").toBool());
        QVERIFY(!listView->property("visible").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(
            metadataPanel->property("visible").toBool(), 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(findQuickItem(
            page, QStringLiteral("sstvGalleryRecordActions")) != nullptr,
            2'000);
        QQuickItem* favoriteToggle = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(
            (favoriteToggle = findQuickItem(
                 page, QStringLiteral("sstvGalleryFavoriteToggle"))) != nullptr,
            2'000);
        QVERIFY(QMetaObject::invokeMethod(favoriteToggle, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(model.data(
            model.index(0), SstvGalleryModel::FavoriteRole).toBool(), 5'000);
        QVERIFY(QMetaObject::invokeMethod(favoriteToggle, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.data(
            model.index(0), SstvGalleryModel::FavoriteRole).toBool(), 5'000);
        QVERIFY(QMetaObject::invokeMethod(metadataEditButton, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(metadataDialog->property("visible").toBool(),
                                 2'000);
        QCOMPARE(metadataTags->property("text").toString(),
                 QStringLiteral("rendered"));
        QCOMPARE(metadataNote->property("text").toString(),
                 QStringLiteral("Initial Gallery UI note"));
        metadataTags->setProperty("text", QStringLiteral(" portable, field-day "));
        metadataNote->setProperty("text", QStringLiteral("Operator note"));
        QVERIFY(QMetaObject::invokeMethod(saveMetadata, "click"));
        QTRY_COMPARE_WITH_TIMEOUT(
            model.data(model.index(0), SstvGalleryModel::NoteRole).toString(),
            QStringLiteral("Operator note"), 5'000);
        QTRY_COMPARE_WITH_TIMEOUT(
            model.data(model.index(0), SstvGalleryModel::TagsRole).toStringList(),
            QStringList({QStringLiteral("portable"),
                         QStringLiteral("field-day")}), 5'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !metadataDialog->property("visible").toBool(), 5'000);
        QVERIFY(!page->property("metadataUpdatePending").toBool());
        const QVariantMap selectedMetadata =
            page->property("selectedMetadata").toMap();
        QCOMPARE(selectedMetadata.value(QStringLiteral("note")).toString(),
                 QStringLiteral("Operator note"));
        QCOMPARE(selectedMetadata.value(QStringLiteral("tags")).toStringList(),
                 QStringList({QStringLiteral("portable"),
                              QStringLiteral("field-day")}));
        search->setProperty("text", QStringLiteral("Martin"));
        QVERIFY(QMetaObject::invokeMethod(page, "applyFilterControls"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 5'000);
        QCOMPARE(model.rowCount(), 1);

        model.setSelected(saved.record.id, true);
        QTRY_VERIFY_WITH_TIMEOUT(removeButton->property("enabled").toBool(),
                                 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            deleteFilesButton->property("enabled").toBool(), 2'000);
        QVERIFY(QFileInfo::exists(saved.record.imagePath));
        QVERIFY(QFileInfo::exists(saved.record.metadataPath));

        QVariantMap retentionSettings = model.retentionSettings();
        retentionSettings.insert(QStringLiteral("imageQuotaBytes"), 1);
        QSignalSpy retentionSettingsFinished(
            &model, &SstvGalleryModel::retentionSettingsFinished);
        QVERIFY(model.updateRetentionSettings(retentionSettings) != 0);
        QTRY_COMPARE_WITH_TIMEOUT(retentionSettingsFinished.count(), 1,
                                  5'000);
        QVERIFY(retentionSettingsFinished.takeFirst().at(1).toBool());
        QVERIFY(QMetaObject::invokeMethod(previewRetention, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(
            retentionDialog->property("visible").toBool(), 5'000);
        QCOMPARE(model.retentionPreview().value(
                     QStringLiteral("recordCount")).toInt(), 1);
        QVERIFY(!applyRetention->property("enabled").toBool());
        confirmation->setProperty(
            "text", model.retentionPreview().value(
                        QStringLiteral("confirmationPhrase")));
        QTRY_VERIFY_WITH_TIMEOUT(
            applyRetention->property("enabled").toBool(), 2'000);
        QVERIFY(QMetaObject::invokeMethod(retentionDialog, "close"));

        QTest::qWait(200);
        const QImage gridRendered = window.grabWindow();
        QVERIFY2(!gridRendered.isNull(),
                 "Offscreen QQuickWindow produced no gallery frame");
        QCOMPARE(gridRendered.size(), QSize(940, 650));
        QSet<QRgb> sampledColours;
        for (int y = 0; y < gridRendered.height(); y += 13) {
            for (int x = 0; x < gridRendered.width(); x += 13) {
                sampledColours.insert(gridRendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 20,
                 "Rendered gallery did not contain meaningful visual content");

        QVERIFY(QMetaObject::invokeMethod(listMode, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(
            !page->property("gridViewMode").toBool(), 2'000);
        QVERIFY(!gridView->property("visible").toBool());
        QVERIFY(listView->property("visible").toBool());
        QQuickItem* listActions = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(
            (listActions = findQuickItem(
                 page, QStringLiteral("sstvGalleryListRecordActions")))
                != nullptr,
            2'000);
        QTest::qWait(100);
        const QImage listRendered = window.grabWindow();
        QVERIFY2(!listRendered.isNull(),
                 "Offscreen QQuickWindow produced no list gallery frame");
        QCOMPARE(listRendered.size(), QSize(940, 650));
        QVERIFY(gridRendered != listRendered);
        QVERIFY(QMetaObject::invokeMethod(gridMode, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(page->property("gridViewMode").toBool(),
                                 2'000);
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
        QFile qmlSource(sourcePath);
        QVERIFY(qmlSource.open(QIODevice::ReadOnly));
        const QByteArray qmlBytes = qmlSource.readAll();
        QVERIFY(!qmlBytes.contains("encodeURI("));
        QVERIFY(!qmlBytes.contains("\"file://\" +"));
    }
};

QTEST_MAIN(TestSstvGalleryQml)
#include "test_sstv_gallery_qml.moc"
