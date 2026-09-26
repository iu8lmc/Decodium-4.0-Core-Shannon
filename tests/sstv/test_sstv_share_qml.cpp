// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/models/SstvShareController.h"

#include <QtTest/QtTest>

#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>

namespace {

class ShareEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* sstvShare READ sstvShare CONSTANT)

public:
    explicit ShareEngineFixture(decodium::sstv::SstvShareController* share,
                                QObject* parent = nullptr)
        : QObject(parent)
        , m_share(share)
    {
    }

    QObject* sstvShare() const noexcept { return m_share; }

private:
    decodium::sstv::SstvShareController* const m_share;
};

QString qmlErrors(const QList<QQmlError>& errors)
{
    QStringList lines;
    for (const QQmlError& error : errors) {
        lines.push_back(error.toString());
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

class TestSstvShareQml final : public QObject
{
    Q_OBJECT

private slots:
    void pageCreatesWithPrivacyOffAndRenders()
    {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        decodium::sstv::SstvShareController controller(nullptr);
        controller.setStorageRoot(storage.path(), QStringLiteral("9H1TEST"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready(), 5'000);
        QVERIFY(!controller.enabled());
        const QString specialPath = storage.filePath(
            QStringLiteral("space # 100% qualità 日本.png"));
        const QUrl specialUrl = controller.localFileUrl(specialPath);
        QCOMPARE(specialUrl.toLocalFile(), specialPath);
        QVERIFY(specialUrl.toEncoded().contains("%23"));
        QVERIFY(specialUrl.toEncoded().contains("%25"));
        ShareEngineFixture fixture(&controller);

        QQmlEngine engine;
        QStringList runtimeWarnings;
        connect(&engine, &QQmlEngine::warnings, this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });
        QQmlComponent component(
            &engine,
            QUrl::fromLocalFile(QString::fromUtf8(
                DECODIUM_SSTV_SHARE_QML_SOURCE)),
            QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(),
                 qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {
            {QStringLiteral("engine"),
             QVariant::fromValue(static_cast<QObject*>(&fixture))},
        };
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* page = qobject_cast<QQuickItem*>(object.data());
        QVERIFY(page);

        QObject* optIn = page->findChild<QObject*>(
            QStringLiteral("sstvSharingOptIn"));
        QObject* credential = page->findChild<QObject*>(
            QStringLiteral("sstvShareCredential"));
        QObject* upload = page->findChild<QObject*>(
            QStringLiteral("sstvShareUpload"));
        QObject* remoteDialog = page->findChild<QObject*>(
            QStringLiteral("sstvShareRemoteRemovalDialog"));
        QObject* remoteAcknowledgement = page->findChild<QObject*>(
            QStringLiteral("sstvShareRemoteRemovalAcknowledgement"));
        QObject* remoteConfirm = page->findChild<QObject*>(
            QStringLiteral("sstvShareConfirmRemoteRemoval"));
        QObject* remoteScopeWarning = page->findChild<QObject*>(
            QStringLiteral("sstvShareRemoteRemovalScopeWarning"));
        QObject* providerType = page->findChild<QObject*>(
            QStringLiteral("sstvShareProviderType"));
        QObject* saveProvider = page->findChild<QObject*>(
            QStringLiteral("sstvShareSaveProvider"));
        QObject* unavailableProviderReason = page->findChild<QObject*>(
            QStringLiteral("sstvShareUnavailableProviderReason"));
        QObject* uploadExpiry = page->findChild<QObject*>(
            QStringLiteral("sstvShareUploadExpiry"));
        QObject* allowMetered = page->findChild<QObject*>(
            QStringLiteral("sstvShareAllowMetered"));
        QObject* diagnostics = page->findChild<QObject*>(
            QStringLiteral("sstvShareDiagnostics"));
        QObject* resetDiagnostics = page->findChild<QObject*>(
            QStringLiteral("sstvShareResetDiagnostics"));
        QObject* saveAsPicker = page->findChild<QObject*>(
            QStringLiteral("sstvShareSaveAsPicker"));
        QObject* localDeleteDialog = page->findChild<QObject*>(
            QStringLiteral("sstvShareLocalDeleteDialog"));
        QObject* providerDeleteDialog = page->findChild<QObject*>(
            QStringLiteral("sstvShareProviderIncomingDeleteDialog"));
        QObject* blockDialog = page->findChild<QObject*>(
            QStringLiteral("sstvShareBlockSenderDialog"));
        QVERIFY(optIn);
        QVERIFY(credential);
        QVERIFY(upload);
        QVERIFY(remoteDialog);
        QVERIFY(remoteAcknowledgement);
        QVERIFY(remoteConfirm);
        QVERIFY(remoteScopeWarning);
        QVERIFY(providerType);
        QVERIFY(saveProvider);
        QVERIFY(unavailableProviderReason);
        QVERIFY(uploadExpiry);
        QVERIFY(allowMetered);
        QVERIFY(diagnostics);
        QVERIFY(resetDiagnostics);
        QVERIFY(saveAsPicker);
        QVERIFY(localDeleteDialog);
        QVERIFY(providerDeleteDialog);
        QVERIFY(blockDialog);
        QCOMPARE(optIn->property("checked").toBool(), false);
        QCOMPARE(upload->property("enabled").toBool(), false);
        QCOMPARE(remoteDialog->property("visible").toBool(), false);
        QCOMPARE(remoteAcknowledgement->property("checked").toBool(), false);
        QCOMPARE(remoteConfirm->property("enabled").toBool(), false);
        QVERIFY(remoteScopeWarning->property("text").toString().contains(
            QStringLiteral("Gallery")));
        QVERIFY(remoteScopeWarning->property("text").toString().contains(
            QStringLiteral("RF/TX")));
        QCOMPARE(controller.preSignedAvailable(), false);
        QCOMPARE(controller.peerRelayAvailable(), false);
        QCOMPARE(uploadExpiry->property("currentIndex").toInt(), 1);
        QCOMPARE(allowMetered->property("checked").toBool(), false);
        QVERIFY(resetDiagnostics->property("enabled").toBool());
        QCOMPARE(saveAsPicker->property("visible").toBool(), false);
        QCOMPARE(localDeleteDialog->property("visible").toBool(), false);
        QCOMPARE(providerDeleteDialog->property("visible").toBool(), false);
        QCOMPARE(blockDialog->property("visible").toBool(), false);
        providerType->setProperty("currentIndex", 2);
        QTest::qWait(10);
        QCOMPARE(saveProvider->property("enabled").toBool(), false);
        QVERIFY(unavailableProviderReason->property("text").toString()
                    .contains(QStringLiteral("trusted broker"),
                              Qt::CaseInsensitive));
        QVERIFY(!unavailableProviderReason->property("text").toString()
                     .contains(QStringLiteral("https://")));
        providerType->setProperty("currentIndex", 3);
        QTest::qWait(10);
        QCOMPARE(saveProvider->property("enabled").toBool(), false);
        QVERIFY(unavailableProviderReason->property("text").toString()
                    .contains(QStringLiteral("backend"),
                              Qt::CaseInsensitive));
        providerType->setProperty("currentIndex", 0);
        page->setProperty("remoteRemovalTransferId",
                          QStringLiteral("00000000-0000-4000-8000-000000000001"));
        page->setProperty("remoteRemovalAction", QStringLiteral("delete"));
        remoteAcknowledgement->setProperty("checked", true);
        QTest::qWait(10);
        QCOMPARE(remoteConfirm->property("enabled").toBool(), false);

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#111c25")));
        window.resize(1040, 700);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(1040.0, 700.0));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTest::qWait(250);
        const QImage rendered = window.grabWindow();
        QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.size(), QSize(1040, 700));
        const QString screenshotPath = qEnvironmentVariable(
            "DECODIUM_SSTV_SHARE_TEST_SCREENSHOT");
        if (!screenshotPath.isEmpty()) {
            QVERIFY(rendered.save(screenshotPath));
        }
        QSet<QRgb> colours;
        for (int y = 0; y < rendered.height(); y += 17) {
            for (int x = 0; x < rendered.width(); x += 17) {
                colours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY(colours.size() > 10);
        page->setProperty("configurationExpanded", true);
        QTest::qWait(150);
        const QImage expanded = window.grabWindow();
        QVERIFY(!expanded.isNull());
        const QString expandedPath = qEnvironmentVariable(
            "DECODIUM_SSTV_SHARE_CONFIG_TEST_SCREENSHOT");
        if (!expandedPath.isEmpty()) {
            QVERIFY(expanded.save(expandedPath));
        }
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
        QFile qmlSource(QString::fromUtf8(DECODIUM_SSTV_SHARE_QML_SOURCE));
        QVERIFY(qmlSource.open(QIODevice::ReadOnly));
        const QByteArray qmlBytes = qmlSource.readAll();
        QVERIFY(!qmlBytes.contains("\"file://\" +"));
        controller.shutdown();
    }
};

QTEST_MAIN(TestSstvShareQml)

#include "test_sstv_share_qml.moc"
