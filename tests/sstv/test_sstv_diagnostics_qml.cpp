// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/diagnostics/SstvDiagnosticsController.h"

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
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>

using decodium::sstv::SstvDiagnosticsController;

namespace {

class DiagnosticsEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* sstvDiagnostics READ sstvDiagnostics CONSTANT)

public:
    explicit DiagnosticsEngineFixture(SstvDiagnosticsController* controller,
                                      QObject* parent = nullptr)
        : QObject(parent)
        , m_controller(controller)
    {
    }

    QObject* sstvDiagnostics() const noexcept { return m_controller; }

private:
    SstvDiagnosticsController* const m_controller;
};

QString qmlErrors(const QList<QQmlError>& errors)
{
    QStringList lines;
    for (const QQmlError& error : errors) {
        lines.push_back(error.toString());
    }
    return lines.join(QLatin1Char('\n'));
}

QVariantMap inputSnapshot()
{
    return {
        {QStringLiteral("capabilities"), QVariantMap {
             {QStringLiteral("analogRx"), true},
             {QStringLiteral("analogTx"), true},
             {QStringLiteral("gallery"), true},
             {QStringLiteral("hamdrm"), true}}},
        {QStringLiteral("rx"), QVariantMap {
             {QStringLiteral("state"), QStringLiteral("receiving")},
             {QStringLiteral("modeId"), QStringLiteral("martin-m1")},
             {QStringLiteral("queuedChunks"), 2},
             {QStringLiteral("queuedSamples"), 2048}}},
        {QStringLiteral("tx"), QVariantMap {
             {QStringLiteral("state"), QStringLiteral("idle")},
             {QStringLiteral("samplesProduced"), 12345}}},
        {QStringLiteral("storage"), QVariantMap {
             {QStringLiteral("recordCount"), 5000},
             {QStringLiteral("imageBytes"), 1024}}},
        {QStringLiteral("share"), QVariantMap {
             {QStringLiteral("activeQueueDepth"), 3},
             {QStringLiteral("uploadedBytes"), 4096},
             {QStringLiteral("uploadBytesPerSecond"), 512}}},
        {QStringLiteral("hamdrm"), QVariantMap {
             {QStringLiteral("available"), true},
             {QStringLiteral("state"), QStringLiteral("idle")}}},
        {QStringLiteral("calibration"), QVariantMap {
             {QStringLiteral("completed"), true},
             {QStringLiteral("success"), true},
             {QStringLiteral("frequencyHz"), 1900}}},
        {QStringLiteral("testTone"), QVariantMap {
             {QStringLiteral("available"), true},
             {QStringLiteral("running"), false},
             {QStringLiteral("frequencyHz"), 1500},
             {QStringLiteral("durationMs"), 1000}}},
    };
}

} // namespace

class TestSstvDiagnosticsQml final : public QObject
{
    Q_OBJECT

private slots:
    void pageRendersAndReportsExportOutcomes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SstvDiagnosticsController controller;
        QString error;
        QVERIFY2(controller.setInputSnapshot(inputSnapshot(), &error),
                 qPrintable(error));
        DiagnosticsEngineFixture fixture(&controller);

        QQmlEngine engine;
        QStringList runtimeWarnings;
        connect(&engine, &QQmlEngine::warnings, this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });
        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_DIAGNOSTICS_QML_SOURCE);
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
        window.resize(980, 720);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(980.0, 720.0));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2000);

        QObject* refresh = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsRefresh"));
        QObject* testTone = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsTestTone"));
        QObject* exportButton = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsExport"));
        QObject* exportDialog = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsExportDialog"));
        QObject* status = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsStatus"));
        QObject* clearEvents = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsClearEvents"));
        QObject* hamDrmBytes = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsHamDrmBytes"));
        QObject* hamDrmRepair = page->findChild<QObject*>(
            QStringLiteral("sstvDiagnosticsHamDrmRepair"));
        QVERIFY(refresh);
        QVERIFY(testTone);
        QVERIFY(exportButton);
        QVERIFY(exportDialog);
        QVERIFY(status);
        QVERIFY(clearEvents);
        QVERIFY(hamDrmBytes);
        QVERIFY(hamDrmRepair);
        QVERIFY(refresh->property("enabled").toBool());
        QVERIFY(testTone->property("enabled").toBool());
        QVERIFY(exportButton->property("enabled").toBool());
        QCOMPARE(testTone->property("text").toString(),
                 QStringLiteral("Transmit 1500 Hz (2 s)"));
        QCOMPARE(hamDrmBytes->property("value").toString(),
                 QStringLiteral("Unavailable / Unavailable"));
        QCOMPARE(hamDrmRepair->property("value").toString(),
                 QStringLiteral("Unavailable / Unavailable"));

        QSignalSpy toneRequested(&controller,
                                 &SstvDiagnosticsController::testToneRequested);
        QVERIFY(QMetaObject::invokeMethod(testTone, "click"));
        QCOMPARE(toneRequested.size(), 1);

        QSignalSpy failed(&controller,
                          &SstvDiagnosticsController::exportFinished);
        controller.exportReport(QUrl(QStringLiteral("https://invalid/report.json")));
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.constFirst().at(0).toBool(), false);
        QTRY_VERIFY_WITH_TIMEOUT(page->property("statusIsError").toBool(), 1000);

        const QString reportPath = directory.filePath(
            QStringLiteral("qml-report.json"));
        QSignalSpy completed(&controller,
                             &SstvDiagnosticsController::exportFinished);
        controller.exportReport(QUrl::fromLocalFile(reportPath));
        QVERIFY(completed.wait(5000));
        QCOMPARE(completed.constLast().at(0).toBool(), true);
        QVERIFY(QFileInfo::exists(reportPath));
        QTRY_VERIFY_WITH_TIMEOUT(
            !page->property("statusIsError").toBool(), 1000);

        QTest::qWait(100);
        const QImage rendered = window.grabWindow();
        QVERIFY2(!rendered.isNull(),
                 "Offscreen QQuickWindow produced no diagnostics frame");
        QCOMPARE(rendered.size(), QSize(980, 720));
        QSet<QRgb> sampledColours;
        for (int y = 0; y < rendered.height(); y += 17) {
            for (int x = 0; x < rendered.width(); x += 17) {
                sampledColours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 12,
                 "Rendered diagnostics page lacks meaningful content");
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
        controller.shutdown();
    }
};

QTEST_MAIN(TestSstvDiagnosticsQml)

#include "test_sstv_diagnostics_qml.moc"
