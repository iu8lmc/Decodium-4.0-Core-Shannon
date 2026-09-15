// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include "../../src/sstv/integration/SstvStudioController.h"

#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QSet>
#include <QSignalSpy>
#include <QUrl>

#include <memory>

using namespace decodium::sstv;

namespace {

class StudioEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* sstvStudio READ sstvStudio CONSTANT)
    Q_PROPERTY(QString callsign READ callsign CONSTANT)
    Q_PROPERTY(QString grid READ grid CONSTANT)
    Q_PROPERTY(qulonglong frequency READ frequency CONSTANT)
    Q_PROPERTY(bool sstvTxCanStart READ sstvTxCanStart CONSTANT)
    Q_PROPERTY(bool sstvTxActive READ sstvTxActive CONSTANT)
    Q_PROPERTY(QString sstvTxState READ sstvTxState CONSTANT)
    Q_PROPERTY(double sstvTxProgress READ sstvTxProgress CONSTANT)
    Q_PROPERTY(QString sstvTxError READ sstvTxError CONSTANT)
    Q_PROPERTY(int txOutputLevel READ txOutputLevel CONSTANT)
    Q_PROPERTY(QString audioOutputDevice READ audioOutputDevice CONSTANT)
    Q_PROPERTY(QString catRigName READ catRigName CONSTANT)
    Q_PROPERTY(bool pttPending READ pttPending CONSTANT)
    Q_PROPERTY(bool pttConfirmed READ pttConfirmed CONSTANT)
    Q_PROPERTY(QVariantMap sstvTxDiagnostics READ sstvTxDiagnostics CONSTANT)

public:
    explicit StudioEngineFixture(SstvStudioController* studio,
                                 QObject* parent = nullptr)
        : QObject(parent)
        , m_studio(studio)
    {
    }

    QObject* sstvStudio() const noexcept { return m_studio; }
    QString callsign() const { return QStringLiteral("9H1TEST"); }
    QString grid() const { return QStringLiteral("JM75FV"); }
    qulonglong frequency() const noexcept { return 14'230'000ULL; }
    bool sstvTxCanStart() const noexcept { return true; }
    bool sstvTxActive() const noexcept { return false; }
    QString sstvTxState() const { return QStringLiteral("Idle"); }
    double sstvTxProgress() const noexcept { return 0.0; }
    QString sstvTxError() const { return {}; }
    int txOutputLevel() const noexcept { return 35; }
    QString audioOutputDevice() const { return QStringLiteral("Test output"); }
    QString catRigName() const { return QStringLiteral("Mock CAT"); }
    bool pttPending() const noexcept { return false; }
    bool pttConfirmed() const noexcept { return false; }
    QVariantMap sstvTxDiagnostics() const
    {
        return {{QStringLiteral("pcmPeak"), 0.5},
                {QStringLiteral("headroomDb"), -1.0},
                {QStringLiteral("clippedFrames"), qulonglong {0U}}};
    }

    Q_INVOKABLE bool startSstvTx(const QString&) { return true; }
    Q_INVOKABLE bool startSstvCalibrationTone(const QString& id)
    {
        m_lastCalibrationTone = id;
        return true;
    }
    Q_INVOKABLE void cancelSstvTx() {}
    QString lastCalibrationTone() const { return m_lastCalibrationTone; }

private:
    SstvStudioController* const m_studio;
    QString m_lastCalibrationTone;
};

class StudioImageProvider final : public QQuickImageProvider
{
public:
    explicit StudioImageProvider(const SstvStudioController* studio)
        : QQuickImageProvider(QQuickImageProvider::Image,
                              QQuickImageProvider::ForceAsynchronousImageLoading)
        , m_studio(studio)
    {
    }

    QImage requestImage(const QString& id,
                        QSize* size,
                        const QSize&) override
    {
        const bool prepared = id.startsWith(QStringLiteral("tx-prepared/"));
        const bool loopback = id.startsWith(QStringLiteral("tx-loopback/"));
        const auto snapshot = loopback ? m_studio->loopbackSnapshot()
            : (prepared ? m_studio->preparedSnapshot()
                        : m_studio->sourceSnapshot());
        if (!snapshot) {
            return {};
        }
        if (size) {
            *size = snapshot->size();
        }
        return *snapshot;
    }

private:
    const SstvStudioController* const m_studio;
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

} // namespace

class TestSstvStudioQml final : public QObject
{
    Q_OBJECT

private slots:
    void componentCreatesPreparesAndRendersOffscreen()
    {
        SstvStudioController studio;
        QVERIFY(studio.generateCalibrationPattern());
        StudioEngineFixture fixture(&studio);

        QQmlEngine engine;
        engine.addImageProvider(QStringLiteral("decodium-sstv"),
                                new StudioImageProvider(&studio));
        QStringList runtimeWarnings;
        connect(&engine,
                &QQmlEngine::warnings,
                this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });

        const QString sourcePath = QString::fromUtf8(DECODIUM_SSTV_QML_SOURCE);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial;
        initial.insert(QStringLiteral("engine"),
                       QVariant::fromValue(static_cast<QObject*>(&fixture)));
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* page = qobject_cast<QQuickItem*>(object.data());
        QVERIFY(page);

        QObject* modeSelector = page->findChild<QObject*>(
            QStringLiteral("sstvModeSelector"));
        QVERIFY2(modeSelector, "Studio mode selector was not instantiated");
        const QVariantList modes = studio.modes();
        int avt90Index = -1;
        for (qsizetype index = 0; index < modes.size(); ++index) {
            if (modes.at(index).toMap().value(QStringLiteral("id"))
                == QStringLiteral("avt-90")) {
                avt90Index = static_cast<int>(index);
                break;
            }
        }
        QVERIFY2(avt90Index >= 0, "AVT90 is absent from the Studio QML model");
        QVERIFY(modeSelector->setProperty("currentIndex", avt90Index));
        QVERIFY(QMetaObject::invokeMethod(modeSelector,
                                          "activated",
                                          Q_ARG(int, avt90Index)));
        QTRY_COMPARE(studio.modeId(), QStringLiteral("avt-90"));
        QCOMPARE(studio.outputSize(), QSize(320, 240));

        const QStringList requiredControls {
            QStringLiteral("sstvCropX"),
            QStringLiteral("sstvCropY"),
            QStringLiteral("sstvCropWidth"),
            QStringLiteral("sstvCropHeight"),
            QStringLiteral("sstvExposure"),
            QStringLiteral("sstvWhiteBalanceRed"),
            QStringLiteral("sstvWhiteBalanceGreen"),
            QStringLiteral("sstvWhiteBalanceBlue"),
            QStringLiteral("sstvSharpness"),
            QStringLiteral("sstvOverlayCallsign"),
            QStringLiteral("sstvOverlayGrid"),
            QStringLiteral("sstvOverlayUtc"),
            QStringLiteral("sstvOverlayFrequency"),
            QStringLiteral("sstvOverlayMode"),
            QStringLiteral("sstvOverlayCustom"),
            QStringLiteral("sstvOverlayReport"),
            QStringLiteral("sstvOverlayWatermark"),
            QStringLiteral("sstvTemplateSelector"),
            QStringLiteral("sstvTemplateSave"),
            QStringLiteral("sstvLoopbackButton"),
            QStringLiteral("sstvCalibrationTone"),
            QStringLiteral("sstvCalibrationButton")};
        for (const QString& name : requiredControls) {
            QVERIFY2(page->findChild<QObject*>(name), qPrintable(name));
        }
        QVERIFY(page->findChild<QObject*>(QStringLiteral("sstvCropX"))
                    ->setProperty("value", 10));
        QVERIFY(page->findChild<QObject*>(QStringLiteral("sstvCropWidth"))
                    ->setProperty("value", 80));
        QVERIFY(page->findChild<QObject*>(QStringLiteral("sstvExposure"))
                    ->setProperty("value", 0.5));
        QVERIFY(page->findChild<QObject*>(QStringLiteral("sstvOverlayCustom"))
                    ->setProperty("checked", true));
        QVERIFY(page->findChild<QObject*>(QStringLiteral("sstvCustomOverlayText"))
                    ->setProperty("text", QStringLiteral("CQ TEST")));
        QVariant collected;
        QVERIFY(QMetaObject::invokeMethod(page,
                                          "collectControls",
                                          Q_RETURN_ARG(QVariant, collected)));
        const QVariantMap controls = collected.toMap();
        QCOMPARE(controls.value(QStringLiteral("cropX")).toDouble(), 0.1);
        QCOMPARE(controls.value(QStringLiteral("cropWidth")).toDouble(), 0.8);
        QCOMPARE(controls.value(QStringLiteral("exposure")).toDouble(), 0.5);
        QCOMPARE(controls.value(QStringLiteral("overlays")).toList().size(), 1);

        QObject* calibration = page->findChild<QObject*>(
            QStringLiteral("sstvCalibrationTone"));
        QVERIFY(calibration->setProperty("currentIndex", 3));
        QObject* calibrationButton = page->findChild<QObject*>(
            QStringLiteral("sstvCalibrationButton"));
        QVERIFY(QMetaObject::invokeMethod(calibrationButton, "clicked"));
        QCOMPARE(fixture.lastCalibrationTone(), QStringLiteral("white-2300"));

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#111c25")));
        window.resize(800, 580);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(800.0, 580.0));
        window.show();

        QVERIFY(QMetaObject::invokeMethod(page, "preparePreview"));
        QTRY_VERIFY_WITH_TIMEOUT(!studio.busy(), 5'000);
        QVERIFY2(studio.preparedReady(), qPrintable(studio.error()));
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTest::qWait(250);
        const QImage rendered = window.grabWindow();
        QVERIFY2(!rendered.isNull(), "Offscreen QQuickWindow produced no frame");
        QCOMPARE(rendered.size(), QSize(800, 580));
        const QString screenshotPath
            = qEnvironmentVariable("DECODIUM_SSTV_TEST_SCREENSHOT");
        if (!screenshotPath.isEmpty()) {
            QVERIFY2(rendered.save(screenshotPath),
                     "Could not save requested SSTV studio test screenshot");
        }

        QSet<QRgb> sampledColours;
        for (int y = 0; y < rendered.height(); y += 17) {
            for (int x = 0; x < rendered.width(); x += 17) {
                sampledColours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 8,
                 "Rendered studio page did not contain meaningful visual content");
        QVERIFY2(runtimeWarnings.isEmpty(), qPrintable(runtimeWarnings.join('\n')));
    }
};

QTEST_MAIN(TestSstvStudioQml)
#include "test_sstv_studio_qml.moc"
