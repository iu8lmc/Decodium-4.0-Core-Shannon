// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/models/SstvShareController.h"
#ifdef DECODIUM_TEST_MAC_E2EE
#include "src/security/MacKeychainBackend.h"
#include "src/sstv/sharing/SstvShareSecureEnvelope.h"
#include "src/sstv/sharing/SstvLocalIntegrationShareProvider.h"
#include <Security/Security.h>
#include <QSettings>
#endif

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
#ifdef DECODIUM_TEST_MAC_E2EE
    void uiQueuesEncryptedAndPlainUploads()
    {
        using namespace decodium::sstv::sharing;
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("DecodiumUiE2eeTests"));
        QCoreApplication::setApplicationName(QStringLiteral("UiSharing"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, storage.path());
        QSettings settings; settings.clear(); settings.sync();
        const QByteArray path = storage.filePath(QStringLiteral("test.keychain-db")).toUtf8();
        const QByteArray password("temporary-ui-test-keychain");
        SecKeychainRef raw = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        SecKeychainSetUserInteractionAllowed(false);
        const auto status = SecKeychainCreate(path.constData(), UInt32(password.size()), password.constData(), false, nullptr, &raw);
#pragma clang diagnostic pop
        QVERIFY(status == errSecSuccess && raw);
        struct Cleanup { SecKeychainRef keychain; ~Cleanup() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            SecKeychainDelete(keychain);
#pragma clang diagnostic pop
            CFRelease(keychain);
        }} cleanup {raw};
        secure_settings::MacKeychainBackend backend(QString::fromUtf8(path));
        SstvLocalIntegrationShareProvider::Config config;
        config.localRecipientId = QStringLiteral("station-local");
        config.participantIds = {QStringLiteral("station-local")};
        config.encryptedEnvelopes = true;
        config.maximumTotalBytes = 4U * 1024U * 1024U;
        config.maximumPendingOperations = 64;
        auto provider = std::make_shared<SstvLocalIntegrationShareProvider>(config);
        QVERIFY(provider->isConfigurationValid());
        decodium::sstv::SstvShareController controller(&backend, provider);
        QSignalSpy operations(&controller, &decodium::sstv::SstvShareController::operationFinished);
        controller.setStorageRoot(storage.path(), QStringLiteral("station-local"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready(), 5000);
        QVERIFY(controller.setEnabled(true));
        QTRY_VERIFY_WITH_TIMEOUT(controller.enabled(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(controller.encryptedSharingAvailable(), 5000);
        ShareEngineFixture fixture(&controller);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromUtf8(DECODIUM_SSTV_SHARE_QML_SOURCE)), QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QScopedPointer<QObject> page(component.createWithInitialProperties({{"engine", QVariant::fromValue(static_cast<QObject*>(&fixture))}}));
        QVERIFY2(page, qPrintable(qmlErrors(component.errors())));
        auto child = [&](const char* name) { return page->findChild<QObject*>(QString::fromLatin1(name)); };
        QObject* create = child("sstvShareCreateIdentity");
        QObject* recipient = child("sstvShareRecipient");
        QObject* key = child("sstvSharePeerPublicKey");
        QObject* fingerprint = child("sstvSharePeerFingerprint");
        QObject* verified = child("sstvShareFingerprintConfirmed");
        QObject* pin = child("sstvSharePinRecipient");
        QObject* encrypt = child("sstvShareEncryptUpload");
        QObject* uploadPrivacy = child("sstvShareUploadPrivacy");
        QObject* confirmed = child("sstvShareRecipientConfirmed");
        QObject* metered = child("sstvShareAllowMetered");
        QObject* upload = child("sstvShareUpload");
        QObject* policy = child("sstvShareRequireIncomingEncryption");
        QVERIFY(create && recipient && key && fingerprint && verified && pin && encrypt && uploadPrivacy && confirmed && metered && upload);
        QVERIFY(policy);
        QVERIFY(QMetaObject::invokeMethod(create, "clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(!page->property("sharingPublicKey").toString().isEmpty(), 5000);
        recipient->setProperty("text", QStringLiteral("station-local"));
        key->setProperty("text", page->property("sharingPublicKey"));
        fingerprint->setProperty("text", page->property("sharingFingerprint"));
        verified->setProperty("checked", true);
        operations.clear();
        QVERIFY(QMetaObject::invokeMethod(pin, "clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(operations.count() >= 1, 5000);
        QCOMPARE(operations.last().at(0).toString(), QStringLiteral("sharing-pin"));
        QVERIFY(operations.last().at(1).toBool());
        const QString imagePath = storage.filePath(QStringLiteral("image.png"));
        QImage image(32, 24, QImage::Format_RGB32); image.fill(Qt::red);
        QVERIFY(image.save(imagePath));
        page->setProperty("selectedUpload", QUrl::fromLocalFile(imagePath));
        confirmed->setProperty("checked", true); metered->setProperty("checked", true);
        encrypt->setProperty("checked", true);
        QVERIFY(uploadPrivacy->property("text").toString().contains("Sender identity is not authenticated"));
        QVERIFY(!uploadPrivacy->property("text").toString().contains("provider can read content"));
        QVERIFY(upload->property("enabled").toBool());
        QVERIFY(controller.setEnabled(false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.enabled(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!encrypt->property("enabled").toBool(), 5000);
        QVERIFY(encrypt->property("checked").toBool());
        QVERIFY(!upload->property("enabled").toBool());
        QVERIFY(uploadPrivacy->property("text").toString().contains("Upload is blocked"));
        QVERIFY(controller.setEnabled(true));
        QTRY_VERIFY_WITH_TIMEOUT(controller.encryptedSharingAvailable(), 5000);
        QVERIFY(encrypt->property("checked").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(upload->property("enabled").toBool(), 5000);
        recipient->setProperty("text", QStringLiteral("station-other"));
        QVERIFY(!confirmed->property("checked").toBool());
        QVERIFY(!upload->property("enabled").toBool());
        recipient->setProperty("text", QStringLiteral("station-local"));
        QVERIFY(!confirmed->property("checked").toBool());
        confirmed->setProperty("checked", true);
        QVERIFY(upload->property("enabled").toBool());
        operations.clear();
        QVERIFY(QMetaObject::invokeMethod(upload, "clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(operations.count() >= 1, 5000);
        QCOMPARE(operations.last().at(0).toString(), QStringLiteral("upload"));
        QVERIFY(operations.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(controller.diagnostics().value("uploadedBytes").toULongLong() > 0, 8000);
        image.fill(Qt::blue); QVERIFY(image.save(imagePath));
        encrypt->setProperty("checked", false);
        QVERIFY(uploadPrivacy->property("text").toString().contains("provider can read content"));
        recipient->setProperty("text", QStringLiteral("station-other"));
        QVERIFY(!confirmed->property("checked").toBool());
        QVERIFY(!upload->property("enabled").toBool());
        recipient->setProperty("text", QStringLiteral("station-local"));
        confirmed->setProperty("checked", true);
        QVERIFY(upload->property("enabled").toBool());
        operations.clear();
        QVERIFY(QMetaObject::invokeMethod(upload, "clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(operations.count() >= 1, 5000);
        QCOMPARE(operations.last().at(0).toString(), QStringLiteral("upload"));
        QVERIFY(operations.last().at(1).toBool());
        QVector<SstvShareIncomingItem> items;
        bool listed = false;
        for (int attempt = 0; attempt < 30 && items.size() < 2; ++attempt) {
            listed = false;
            provider->listIncomingAsync(10, [&](SstvShareProviderResult result, QVector<SstvShareIncomingItem> rows) {
                if (result.ok()) items = std::move(rows); listed = true;
            });
            QTRY_VERIFY_WITH_TIMEOUT(listed, 2000);
            if (items.size() < 2) QTest::qWait(100);
        }
        QCOMPARE(items.size(), 2);
        QSet<SstvShareEncryptionMode> modes;
        QString encryptedId;
        QString tlsId;
        for (const auto& item : items) {
            const auto parsed = parseSstvShareManifestV1(item.canonicalManifestJson);
            QVERIFY(parsed.ok());
            modes.insert(parsed.manifest->encryption.mode);
            QByteArray bytes; bool downloaded = false; bool downloadOk = false;
            provider->downloadAsync(item.opaqueId, 0, 64U * 1024U, {}, [&](SstvShareProviderResult result) {
                downloadOk = result.ok(); bytes = result.boundedPayload(); downloaded = true;
            });
            QTRY_VERIFY_WITH_TIMEOUT(downloaded, 2000);
            QVERIFY(downloadOk);
            if (parsed.manifest->encryption.mode == SstvShareEncryptionMode::EndToEnd) {
                encryptedId = item.opaqueId;
                SstvShareKeyStore receiver(backend, QStringLiteral("STATION-LOCAL"));
                const auto plaintext = sstvShareOpenStoredEnvelope(receiver, "local-integration", "station-local", *parsed.manifest, bytes);
                QVERIFY(plaintext.has_value());
                QCOMPARE(QImage::fromData(*plaintext).pixelColor(0, 0), QColor(Qt::red));
            } else {
                tlsId = item.opaqueId;
                QCOMPARE(QImage::fromData(bytes).pixelColor(0, 0), QColor(Qt::blue));
            }
        }
        QVERIFY(modes.contains(SstvShareEncryptionMode::EndToEnd));
        QVERIFY(modes.contains(SstvShareEncryptionMode::TransportTls));
        QVERIFY(controller.refreshInbox());
        QTRY_COMPARE_WITH_TIMEOUT(controller.inbox()->rowCount(), 2, 5000);
        policy->setProperty("checked", true);
        QVERIFY(QMetaObject::invokeMethod(policy, "clicked"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.requireIncomingEncryption(), 5000);
        operations.clear();
        QVERIFY(controller.download(tlsId, QStringLiteral("plain.png")));
        QTRY_VERIFY_WITH_TIMEOUT(operations.count() >= 1, 5000);
        QCOMPARE(operations.last().at(0).toString(), QStringLiteral("download"));
        QVERIFY(!operations.last().at(1).toBool());
        operations.clear();
        QVERIFY(controller.download(encryptedId, QStringLiteral("encrypted.png")));
        QTRY_VERIFY_WITH_TIMEOUT(operations.count() >= 1, 5000);
        QCOMPARE(operations.last().at(0).toString(), QStringLiteral("download"));
        QVERIFY(operations.last().at(1).toBool());
        QVERIFY(controller.setRequireIncomingEncryption(false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.requireIncomingEncryption(), 5000);
        operations.clear();
        QVERIFY(controller.download(tlsId, QStringLiteral("plain-allowed.png")));
        QTRY_VERIFY_WITH_TIMEOUT(operations.count() >= 1, 5000);
        QCOMPARE(operations.last().at(0).toString(), QStringLiteral("download"));
        QVERIFY(operations.last().at(1).toBool());
        QVERIFY(controller.setRequireIncomingEncryption(true));
        QTRY_VERIFY_WITH_TIMEOUT(controller.requireIncomingEncryption(), 5000);
        controller.shutdown();
        decodium::sstv::SstvShareController restored(&backend, provider);
        restored.setStorageRoot(storage.path(), QStringLiteral("station-local"));
        QTRY_VERIFY_WITH_TIMEOUT(restored.ready(), 5000);
        QVERIFY(restored.requireIncomingEncryption());
        QVERIFY(restored.setRequireIncomingEncryption(false));
        QTRY_VERIFY_WITH_TIMEOUT(!restored.requireIncomingEncryption(), 5000);
        restored.shutdown();
    }
#endif

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
        QObject* encrypt = page->findChild<QObject*>(QStringLiteral("sstvShareEncryptUpload"));
        QObject* peerKey = page->findChild<QObject*>(QStringLiteral("sstvSharePeerPublicKey"));
        QObject* peerFingerprint = page->findChild<QObject*>(QStringLiteral("sstvSharePeerFingerprint"));
        QObject* verified = page->findChild<QObject*>(QStringLiteral("sstvShareFingerprintConfirmed"));
        QObject* recipientInput = page->findChild<QObject*>(QStringLiteral("sstvShareRecipient"));
        QVERIFY(encrypt && peerKey && peerFingerprint && verified && recipientInput);
        QVERIFY(!controller.encryptedSharingAvailable());
        QVERIFY(!encrypt->property("enabled").toBool());
        QVERIFY(!encrypt->property("checked").toBool());
        QObject* uploadLayout = page->findChild<QObject*>(QStringLiteral("sstvShareUploadLayout"));
        QVERIFY(uploadLayout);
        QVERIFY(uploadLayout->findChild<QObject*>(QStringLiteral("sstvShareEncryptUpload")) == encrypt);
        verified->setProperty("checked", true);
        peerKey->setProperty("text", QStringLiteral("abc"));
        QVERIFY(!verified->property("checked").toBool());
        verified->setProperty("checked", true);
        peerFingerprint->setProperty("text", QStringLiteral("def"));
        QVERIFY(!verified->property("checked").toBool());
        verified->setProperty("checked", true);
        recipientInput->setProperty("text", QStringLiteral("peer"));
        QVERIFY(!verified->property("checked").toBool());

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
