// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/diagnostics/SstvDiagnosticLogging.h"
#include "src/sstv/diagnostics/SstvDiagnosticsController.h"
#include "src/sstv/core/SstvModeRegistry.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <limits>
#include <thread>
#include <vector>

using decodium::sstv::SstvDiagnosticLogBuffer;
using decodium::sstv::SstvDiagnosticRedactor;
using decodium::sstv::SstvDiagnosticsController;
using decodium::sstv::recordSstvDiagnosticEvent;

class TestSstvDiagnostics final : public QObject
{
    Q_OBJECT

private slots:
    void categoriesAreExact();
    void hostileDataIsAllowlisted();
    void eventRingIsBoundedThreadSafeAndMonotonic();
    void canonicalRegistryEventIsProcessOnce();
    void refreshRequestsOwnerSnapshotWithoutRecursion();
    void shutdownRejectsTestToneRequest();
    void exportIsAtomicBoundedAndPrivacySafe();
    void exportFailureAndRepeatedLifecycle();
};

void TestSstvDiagnostics::refreshRequestsOwnerSnapshotWithoutRecursion()
{
    SstvDiagnosticsController controller;
    QSignalSpy requested(
        &controller, &SstvDiagnosticsController::inputSnapshotRequested);
    connect(&controller,
            &SstvDiagnosticsController::inputSnapshotRequested,
            &controller,
            [&controller]() {
                QString error;
                QVERIFY(controller.setInputSnapshot({
                    {QStringLiteral("capabilities"), QVariantMap {
                         {QStringLiteral("analogRx"), true}}},
                    {QStringLiteral("rx"), QVariantMap {
                         {QStringLiteral("state"),
                          QStringLiteral("receiving")}}},
                }, &error));
                QVERIFY2(error.isEmpty(), qPrintable(error));
            },
            Qt::DirectConnection);

    controller.refresh();
    QCOMPARE(requested.size(), 1);
    QCOMPARE(controller.capabilities().value(
                 QStringLiteral("analogRx")).toBool(), true);
    QCOMPARE(controller.rxMetrics().value(
                 QStringLiteral("state")).toString(),
             QStringLiteral("receiving"));
    controller.shutdown();
}

void TestSstvDiagnostics::shutdownRejectsTestToneRequest()
{
    SstvDiagnosticsController controller;
    QString error;
    QVERIFY2(controller.setInputSnapshot({
        {QStringLiteral("capabilities"), QVariantMap {
             {QStringLiteral("analogTx"), true}}},
        {QStringLiteral("testTone"), QVariantMap {
             {QStringLiteral("available"), true}}},
    }, &error), qPrintable(error));
    QSignalSpy requested(
        &controller, &SstvDiagnosticsController::testToneRequested);

    controller.shutdown();
    controller.requestTestTone();

    QCOMPARE(requested.size(), 0);
    QVERIFY(!controller.errorString().isEmpty());
}

void TestSstvDiagnostics::categoriesAreExact()
{
    QCOMPARE(QString::fromLatin1(sstvCoreLog().categoryName()),
             QStringLiteral("sstv.core"));
    QCOMPARE(QString::fromLatin1(sstvRxLog().categoryName()),
             QStringLiteral("sstv.rx"));
    QCOMPARE(QString::fromLatin1(sstvTxLog().categoryName()),
             QStringLiteral("sstv.tx"));
    QCOMPARE(QString::fromLatin1(sstvVisLog().categoryName()),
             QStringLiteral("sstv.vis"));
    QCOMPARE(QString::fromLatin1(sstvSyncLog().categoryName()),
             QStringLiteral("sstv.sync"));
    QCOMPARE(QString::fromLatin1(sstvStorageLog().categoryName()),
             QStringLiteral("sstv.storage"));
    QCOMPARE(QString::fromLatin1(sstvShareLog().categoryName()),
             QStringLiteral("sstv.share"));
    QCOMPARE(QString::fromLatin1(sstvHamDrmLog().categoryName()),
             QStringLiteral("sstv.hamdrm"));
    QCOMPARE(QString::fromLatin1(sstvSecurityLog().categoryName()),
             QStringLiteral("sstv.security"));
}

void TestSstvDiagnostics::hostileDataIsAllowlisted()
{
    const QVariantMap capabilities = SstvDiagnosticRedactor::capabilities({
        {QStringLiteral("analogRx"), true},
        {QStringLiteral("remoteSharing"), false},
        {QStringLiteral("token"), QStringLiteral("secret")},
        {QStringLiteral("analogTx"), QStringLiteral("yes")},
    });
    QCOMPARE(capabilities.size(), 2);
    QCOMPARE(capabilities.value(QStringLiteral("analogRx")).toBool(), true);

    const QVariantMap settings = SstvDiagnosticRedactor::settings({
        {QStringLiteral("autoSave"), true},
        {QStringLiteral("rxSampleRateHz"), 48000},
        {QStringLiteral("password"), QStringLiteral("hunter2")},
        {QStringLiteral("retainRawAudio"), QVariantList {true}},
    });
    QCOMPARE(settings.size(), 2);
    QVERIFY(!settings.contains(QStringLiteral("password")));

    const QVariantMap stableSource = SstvDiagnosticRedactor::metrics(
        QStringLiteral("rx"),
        {{QStringLiteral("sourceKind"), QStringLiteral("replay-wav")}});
    QCOMPARE(stableSource.value(QStringLiteral("sourceKind")).toString(),
             QStringLiteral("replay-wav"));
    const QVariantMap translatedSource = SstvDiagnosticRedactor::metrics(
        QStringLiteral("rx"),
        {{QStringLiteral("sourceKind"), QStringLiteral("Replay/WAV")}});
    QVERIFY(!translatedSource.contains(QStringLiteral("sourceKind")));

    const QVariantMap fields = SstvDiagnosticRedactor::eventFields({
        {QStringLiteral("operation"), QStringLiteral("upload")},
        {QStringLiteral("reasonCode"),
         QStringLiteral("https://host/?x-amz-signature=secret")},
        {QStringLiteral("bytes"), std::numeric_limits<double>::infinity()},
        {QStringLiteral("path"), QStringLiteral("/Users/alice/private")},
    });
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.value(QStringLiteral("operation")).toString(),
             QStringLiteral("upload"));

    SstvDiagnosticsController controller;
    QString error;
    QVERIFY(!controller.setInputSnapshot({
        {QStringLiteral("credentials"), QVariantMap {
             {QStringLiteral("token"), QStringLiteral("secret")}}}},
        &error));
    QVERIFY(!error.isEmpty());
    controller.shutdown();
}

void TestSstvDiagnostics::eventRingIsBoundedThreadSafeAndMonotonic()
{
    SstvDiagnosticLogBuffer& buffer = SstvDiagnosticLogBuffer::instance();
    buffer.clear();
    constexpr int threadCount = 8;
    constexpr int eventsPerThread = 100;
    std::vector<std::thread> writers;
    writers.reserve(threadCount);
    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        writers.emplace_back([threadIndex]() {
            for (int eventIndex = 0; eventIndex < eventsPerThread;
                 ++eventIndex) {
                recordSstvDiagnosticEvent(
                    sstvCoreLog(), QtInfoMsg,
                    QStringLiteral("test.concurrent-event"),
                    {{QStringLiteral("attempt"), eventIndex},
                     {QStringLiteral("component"),
                      QStringLiteral("worker%1").arg(threadIndex)}});
            }
        });
    }
    for (std::thread& writer : writers) {
        writer.join();
    }
    QCOMPARE(buffer.size(), SstvDiagnosticLogBuffer::kMaximumEvents);
    const QVariantList snapshot = buffer.snapshot();
    QCOMPARE(snapshot.size(),
             SstvDiagnosticLogBuffer::kMaximumExportedEvents);
    quint64 previous = 0;
    for (const QVariant& item : snapshot) {
        const quint64 sequence = item.toMap()
            .value(QStringLiteral("sequence")).toULongLong();
        QVERIFY(sequence > previous);
        previous = sequence;
    }
    buffer.clear();
    recordSstvDiagnosticEvent(sstvCoreLog(), QtInfoMsg,
                              QStringLiteral("test.after-clear"));
    const QVariantList afterClear = buffer.snapshot();
    QCOMPARE(afterClear.size(), 1);
    QVERIFY(afterClear.constFirst().toMap()
                .value(QStringLiteral("sequence")).toULongLong() > previous);
}

void TestSstvDiagnostics::canonicalRegistryEventIsProcessOnce()
{
    SstvDiagnosticLogBuffer& buffer = SstvDiagnosticLogBuffer::instance();
    buffer.clear();
    for (int iteration = 0; iteration < 20; ++iteration) {
        const auto registry = decodium::sstv::SstvModeRegistry::canonical();
        QVERIFY(!registry.modes().empty());
    }
    int registryEvents = 0;
    for (const QVariant& item : buffer.snapshot()) {
        if (item.toMap().value(QStringLiteral("event")).toString()
            == QStringLiteral("registry.canonical-loaded")) {
            ++registryEvents;
        }
    }
    QVERIFY(registryEvents <= 1);
}

void TestSstvDiagnostics::exportIsAtomicBoundedAndPrivacySafe()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SstvDiagnosticsController controller;
    QString error;
    const QVariantMap snapshot {
        {QStringLiteral("capabilities"), QVariantMap {
             {QStringLiteral("analogRx"), true},
             {QStringLiteral("analogTx"), true},
             {QStringLiteral("hamdrm"), false},
             {QStringLiteral("token"), QStringLiteral("do-not-export")}}},
        {QStringLiteral("settings"), QVariantMap {
             {QStringLiteral("autoSave"), true},
             {QStringLiteral("password"), QStringLiteral("do-not-export")},
             {QStringLiteral("rxSampleRateHz"), 48000}}},
        {QStringLiteral("rx"), QVariantMap {
             {QStringLiteral("state"), QStringLiteral("receiving")},
             {QStringLiteral("queuedSamples"), 1024},
             {QStringLiteral("lastPath"),
              QStringLiteral("/Users/alice/private.wav")}}},
        {QStringLiteral("tx"), QVariantMap {
             {QStringLiteral("state"), QStringLiteral("idle")}}},
        {QStringLiteral("storage"), QVariantMap {
             {QStringLiteral("recordCount"), 5000}}},
        {QStringLiteral("share"), QVariantMap {
             {QStringLiteral("uploadedBytes"), 42},
             {QStringLiteral("activeQueueDepth"), 3},
             {QStringLiteral("signedUrl"),
              QStringLiteral("https://host/?x-amz-signature=secret")}}},
        {QStringLiteral("hamdrm"), QVariantMap {
             {QStringLiteral("available"), false}}},
        {QStringLiteral("calibration"), QVariantMap {
             {QStringLiteral("completed"), true},
             {QStringLiteral("frequencyHz"), 1900}}},
        {QStringLiteral("testTone"), QVariantMap {
             {QStringLiteral("available"), true},
             {QStringLiteral("frequencyHz"), 1500}}},
    };
    QVERIFY2(controller.setInputSnapshot(snapshot, &error),
             qPrintable(error));
    QCOMPARE(controller.shareMetrics()
                 .value(QStringLiteral("uploadedBytes")).toInt(), 42);
    QVERIFY(controller.setInputSnapshot({
        {QStringLiteral("share"), QVariantMap {
             {QStringLiteral("uploadedBytes"), 0},
             {QStringLiteral("activeQueueDepth"), 0}}}}, &error));
    QCOMPARE(controller.shareMetrics()
                 .value(QStringLiteral("uploadedBytes")).toInt(), 0);
    QVERIFY(controller.setInputSnapshot(snapshot, &error));

    const QString reportPath = directory.filePath(QStringLiteral("report.json"));
    QSignalSpy finished(&controller,
                        &SstvDiagnosticsController::exportFinished);
    controller.exportReport(QUrl::fromLocalFile(reportPath));
    QVERIFY(finished.wait(5000));
    QCOMPARE(finished.constFirst().at(0).toBool(), true);

    QFile file(reportPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QVERIFY(bytes.size() > 0);
    QVERIFY(bytes.size() <= SstvDiagnosticsController::kMaximumReportBytes);
    const QJsonDocument report = QJsonDocument::fromJson(bytes);
    QVERIFY(report.isObject());
    QCOMPARE(report.object().value(QStringLiteral("schemaVersion")).toInt(), 1);
    const QJsonObject registry = report.object()
        .value(QStringLiteral("modeRegistry")).toObject();
    QCOMPARE(registry.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(registry.value(QStringLiteral("sha256")).toString().size(), 64);
    QCOMPARE(report.object().value(QStringLiteral("privacy")).toObject()
                 .value(QStringLiteral("containsImages")).toBool(), false);

    const QByteArray folded = bytes.toLower();
    const QList<QByteArray> forbidden {
        QByteArrayLiteral("do-not-export"), QByteArrayLiteral("password"),
        QByteArrayLiteral("token"), QByteArrayLiteral("signedurl"),
        QByteArrayLiteral("x-amz"), QByteArrayLiteral("/users/"),
        reportPath.toUtf8().toLower(), QByteArrayLiteral("private.wav"),
        QByteArrayLiteral("imagepath"), QByteArrayLiteral("audiopath"),
    };
    for (const QByteArray& marker : forbidden) {
        QVERIFY2(!folded.contains(marker), marker.constData());
    }
    controller.shutdown();
}

void TestSstvDiagnostics::exportFailureAndRepeatedLifecycle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    for (int iteration = 0; iteration < 10; ++iteration) {
        SstvDiagnosticsController controller;
        const QVariantMap hash = controller.modeRegistryInfo();
        QCOMPARE(hash.value(QStringLiteral("sha256")).toString().size(), 64);
        QSignalSpy failed(&controller,
                          &SstvDiagnosticsController::exportFinished);
        controller.exportReport(QUrl(QStringLiteral("https://invalid/report.json")));
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.constFirst().at(0).toBool(), false);

        const QString output = directory.filePath(
            QStringLiteral("repeat-%1.json").arg(iteration));
        QSignalSpy completed(&controller,
                             &SstvDiagnosticsController::exportFinished);
        controller.exportReport(QUrl::fromLocalFile(output));
        QVERIFY(completed.wait(5000));
        QCOMPARE(completed.constLast().at(0).toBool(), true);

        QSignalSpy exists(&controller,
                          &SstvDiagnosticsController::exportFinished);
        controller.exportReport(QUrl::fromLocalFile(output));
        QVERIFY(exists.wait(5000));
        QCOMPARE(exists.constLast().at(0).toBool(), false);
        controller.shutdown();
    }
}

QTEST_MAIN(TestSstvDiagnostics)

#include "test_sstv_diagnostics.moc"
