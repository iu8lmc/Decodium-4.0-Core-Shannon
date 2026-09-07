// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/storage/SstvImageStorage.h"
#include "src/sstv/storage/SstvTxGalleryArchive.h"

#include <QtTest/QtTest>

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QTime>
#include <QTimeZone>
#include <QTemporaryDir>

#include <limits>

using namespace decodium::sstv;

namespace {

QDateTime fixtureTime(int minute = 34)
{
    return QDateTime(QDate(2026, 8, 24), QTime(12, minute, 56, 789),
                     QTimeZone(QTimeZone::UTC));
}

QImage fixtureImage()
{
    QImage image(32, 24, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y,
                                QColor::fromRgb((x * 17) % 256,
                                                (y * 29) % 256,
                                                ((x + y) * 11) % 256,
                                                255));
        }
    }
    return image;
}

SstvTxGalleryArchiveContext analogContext(const QString& id,
                                          const QDateTime& eventAtUtc)
{
    SstvTxGalleryArchiveContext context;
    context.id = id;
    context.eventAtUtc = eventAtUtc;
    context.mode = QStringLiteral("martin-m1");
    context.localCallsign = QStringLiteral("9h1test");
    context.localGrid = QStringLiteral("jm75fv");
    context.source = QStringLiteral("sstv-studio");
    context.digital = false;
    context.frequencyHz = 14'230'000;
    context.audioFrequencyHz = 1'900;
    context.sourceSampleRateHz = 48'000;
    context.qualityMetrics = {
        {QStringLiteral("audioToneLowHz"), 1'200.0},
        {QStringLiteral("audioToneCentreHz"), 1'900.0},
        {QStringLiteral("audioToneHighHz"), 2'300.0},
    };
    context.fileNameTemplate = QStringLiteral("{category}_{mode}_{id}");
    return context;
}

void verifyStoredRecord(const SstvImageStore& store,
                        const SstvImageSaveResult& saved,
                        SstvImageCategory category,
                        const QString& source,
                        const QString& mode,
                        bool digital)
{
    QVERIFY2(saved.ok, qPrintable(saved.error));
    QCOMPARE(saved.record.category, category);
    QCOMPARE(saved.record.source, source);
    QCOMPARE(saved.record.mode, mode);
    QCOMPARE(saved.record.digital, digital);
    QCOMPARE(saved.record.completionPercent, 100);
    QVERIFY(saved.record.complete);
    QVERIFY(!saved.record.remote);
    QVERIFY(QFileInfo::exists(saved.record.imagePath));
    QVERIFY(QFileInfo::exists(saved.record.metadataPath));
    QVERIFY(saved.record.imagePath.startsWith(
        store.layout().categoryRoot(category) + QLatin1Char('/')));

    QString error;
    SstvImageRecord sidecar;
    QVERIFY2(SstvImageStore::loadMetadata(saved.record.metadataPath,
                                          &sidecar, &error),
             qPrintable(error));
    QCOMPARE(sidecar, saved.record);
    QVERIFY2(store.verify(saved.record, true, &error), qPrintable(error));
}

} // namespace

class TestSstvTxGalleryArchive final : public QObject
{
    Q_OBJECT

private slots:
    void persistsDraftAndAnalogTransmittedMetadata()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const SstvImageStore store(SstvStorageLayout(
            temporary.filePath(QStringLiteral("sstv"))));
        const QImage image = fixtureImage();

        const QString draftId =
            QStringLiteral("11111111-2222-4333-8444-555555555555");
        SstvTxGalleryArchiveContext draft = analogContext(draftId,
                                                           fixtureTime());
        draft.qualityMetrics.insert(QStringLiteral("txAccepted"), 0.0);
        const auto draftRequest = makeSstvTxGalleryArchiveRequest(
            image, SstvImageCategory::Draft, draft);
        QVERIFY(draftRequest.has_value());
        QCOMPARE(draftRequest->record.id, draftId);
        QCOMPARE(draftRequest->record.localCallsign,
                 QStringLiteral("9H1TEST"));
        QCOMPARE(draftRequest->record.localGrid, QStringLiteral("JM75FV"));
        QCOMPARE(draftRequest->record.eventAtUtc, fixtureTime());
        QCOMPARE(draftRequest->record.capturedAtUtc, fixtureTime());
        QCOMPARE(draftRequest->record.note,
                 QStringLiteral("Native SSTV Studio image prepared"));
        QVERIFY(!draftRequest->record.digital);
        const SstvImageSaveResult savedDraft = store.save(*draftRequest);
        verifyStoredRecord(store, savedDraft, SstvImageCategory::Draft,
                           QStringLiteral("sstv-studio"),
                           QStringLiteral("martin-m1"), false);
        QCOMPARE(savedDraft.record.qualityMetrics.value(
                     QStringLiteral("txAccepted")).toDouble(), 0.0);

        const QString txId =
            QStringLiteral("66666666-7777-4888-8999-aaaaaaaaaaaa");
        SstvTxGalleryArchiveContext transmitted = analogContext(
            txId, fixtureTime(35));
        transmitted.fskId = QStringLiteral("de 9h1test");
        transmitted.qualityMetrics.insert(QStringLiteral("txAccepted"), 1.0);
        transmitted.qualityMetrics.insert(QStringLiteral("txSessionId"),
                                          41.0);
        const auto transmittedRequest = makeSstvTxGalleryArchiveRequest(
            image, SstvImageCategory::Transmitted, transmitted);
        QVERIFY(transmittedRequest.has_value());
        QCOMPARE(transmittedRequest->record.fskId,
                 QStringLiteral("DE 9H1TEST"));
        QCOMPARE(transmittedRequest->record.note,
                 QStringLiteral("Native SSTV Studio transmission accepted"));
        const SstvImageSaveResult savedTransmitted = store.save(
            *transmittedRequest);
        verifyStoredRecord(store, savedTransmitted,
                           SstvImageCategory::Transmitted,
                           QStringLiteral("sstv-studio"),
                           QStringLiteral("martin-m1"), false);
        QCOMPARE(savedTransmitted.record.qualityMetrics.value(
                     QStringLiteral("txAccepted")).toDouble(), 1.0);
        QCOMPARE(savedTransmitted.record.fskId, QStringLiteral("DE 9H1TEST"));
    }

    void persistsDigitalHamDrmTransmittedMetadata()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const SstvImageStore store(SstvStorageLayout(
            temporary.filePath(QStringLiteral("sstv"))));

        SstvTxGalleryArchiveContext context;
        context.id = QStringLiteral("bbbbbbbb-cccc-4ddd-8eee-ffffffffffff");
        context.eventAtUtc = fixtureTime(36);
        context.mode = QStringLiteral("hamdrm-e-2300-qam16");
        context.localCallsign = QStringLiteral("9h1test");
        context.localGrid = QStringLiteral("jm75fv");
        context.source = QStringLiteral("hamdrm");
        context.digital = true;
        context.note = QStringLiteral("HAMDRM transmission accepted");
        context.frequencyHz = 14'236'000;
        context.audioFrequencyHz = 0;
        context.sourceSampleRateHz = 48'000;
        context.qualityMetrics = {
            {QStringLiteral("occupiedBandwidthHz"), 2'300.0},
            {QStringLiteral("txAccepted"), 1.0},
        };
        context.fileNameTemplate = QStringLiteral("{category}_{mode}_{id}");

        const auto request = makeSstvTxGalleryArchiveRequest(
            fixtureImage(), SstvImageCategory::Transmitted, context);
        QVERIFY(request.has_value());
        QVERIFY(request->record.digital);
        QCOMPARE(request->record.source, QStringLiteral("hamdrm"));
        QCOMPARE(request->record.mode, QStringLiteral("hamdrm-e-2300-qam16"));
        QCOMPARE(request->record.audioFrequencyHz, qint64 {0});
        QCOMPARE(request->record.note,
                 QStringLiteral("HAMDRM transmission accepted"));
        const SstvImageSaveResult saved = store.save(*request);
        verifyStoredRecord(store, saved, SstvImageCategory::Transmitted,
                           QStringLiteral("hamdrm"),
                           QStringLiteral("hamdrm-e-2300-qam16"), true);
        QCOMPARE(saved.record.qualityMetrics.value(
                     QStringLiteral("occupiedBandwidthHz")).toDouble(),
                 2'300.0);
    }

    void draftGenerationDeduplicatesWorkerReadyAndRepeatedPreparation()
    {
        quint64 generation = 0U;
        quint64 queuedGeneration = 0U;

        // A prepared image that predates async storage readiness is queued
        // once, and a duplicate readiness pass cannot create a second Draft.
        QVERIFY(sstvTxDraftNeedsArchive(generation, queuedGeneration));
        QCOMPARE(generation, quint64 {1U});
        queuedGeneration = generation;
        QVERIFY(!sstvTxDraftNeedsArchive(generation, queuedGeneration));

        advanceSstvTxDraftGeneration(generation, queuedGeneration);
        QCOMPARE(generation, quint64 {2U});
        QVERIFY(sstvTxDraftNeedsArchive(generation, queuedGeneration));
        queuedGeneration = generation;
        QVERIFY(!sstvTxDraftNeedsArchive(generation, queuedGeneration));

        generation = std::numeric_limits<quint64>::max();
        queuedGeneration = generation;
        advanceSstvTxDraftGeneration(generation, queuedGeneration);
        QCOMPARE(generation, quint64 {1U});
        QCOMPARE(queuedGeneration, quint64 {0U});
        QVERIFY(sstvTxDraftNeedsArchive(generation, queuedGeneration));
    }

    void rejectsInvalidLifecycleRequests()
    {
        SstvTxGalleryArchiveContext valid = analogContext(
            QStringLiteral("99999999-aaaa-4bbb-8ccc-dddddddddddd"),
            fixtureTime());
        QVERIFY(!makeSstvTxGalleryArchiveRequest(
                     QImage(), SstvImageCategory::Draft, valid).has_value());
        QVERIFY(!makeSstvTxGalleryArchiveRequest(
                     fixtureImage(), SstvImageCategory::Received, valid)
                     .has_value());

        valid.eventAtUtc = {};
        QVERIFY(!makeSstvTxGalleryArchiveRequest(
                     fixtureImage(), SstvImageCategory::Draft, valid)
                     .has_value());
        valid = analogContext(QStringLiteral(
            "99999999-aaaa-4bbb-8ccc-dddddddddddd"), fixtureTime());
        valid.mode.clear();
        QVERIFY(!makeSstvTxGalleryArchiveRequest(
                     fixtureImage(), SstvImageCategory::Draft, valid)
                     .has_value());
    }
};

QTEST_GUILESS_MAIN(TestSstvTxGalleryArchive)
#include "test_sstv_tx_gallery_archive.moc"
