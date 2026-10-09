// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/security/SecureSettings.hpp"
#include "src/sstv/models/SstvShareController.h"

#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QSignalSpy>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

namespace {

quintptr threadToken()
{
    return reinterpret_cast<quintptr>(QThread::currentThreadId());
}

class FakeSecureBackend final : public secure_settings::Backend
{
public:
    bool available() const override { return true; }

    secure_settings::LookupResult lookup(
        const QString& service, const QString& account) const override
    {
        QMutexLocker lock(&mutex);
        lookupThread = threadToken();
        secure_settings::LookupResult result;
        result.backend_available = true;
        const QString key = service + QLatin1Char('/') + account;
        const auto value = secrets.constFind(key);
        result.found = value != secrets.constEnd();
        if (result.found) {
            result.value = *value;
        }
        return result;
    }

    bool store(const QString& service,
               const QString& account,
               const QString& value,
               QString*) const override
    {
        QMutexLocker lock(&mutex);
        storeThread = threadToken();
        secrets.insert(service + QLatin1Char('/') + account, value);
        return true;
    }

    bool remove(const QString& service,
                const QString& account,
                QString*) const override
    {
        QMutexLocker lock(&mutex);
        removeThread = threadToken();
        secrets.remove(service + QLatin1Char('/') + account);
        return true;
    }

    bool containsValue(const QString& expected) const
    {
        QMutexLocker lock(&mutex);
        return secrets.values().contains(expected);
    }

    mutable QMutex mutex;
    mutable QHash<QString, QString> secrets;
    mutable quintptr lookupThread {0};
    mutable quintptr storeThread {0};
    mutable quintptr removeThread {0};
};

} // namespace

class TestSstvShareController final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsRoot.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("DecodiumTests"));
        QCoreApplication::setApplicationName(QStringLiteral("SstvShareController"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsRoot.path());
    }

    void privacyThreadingAndSecureCredentialPersistence()
    {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        const quintptr guiThread = threadToken();
        FakeSecureBackend backend;
        decodium::sstv::SstvShareController controller(&backend);
        QSignalSpy operationSpy(&controller,
                                &decodium::sstv::SstvShareController::operationFinished);

        controller.setStorageRoot(storage.path(), QStringLiteral("9H1TEST"));
        QTRY_VERIFY_WITH_TIMEOUT(controller.ready(), 5'000);
        QCOMPARE(controller.enabled(), false);
        QCOMPARE(controller.activeTransfers()->rowCount(), 0);
        QVERIFY(controller.workerThreadToken() != 0U);
        QVERIFY(controller.workerThreadToken() != guiThread);

        const QString secret = QStringLiteral("TOP_SECRET_TOKEN_8c2b");
        QVariantMap configuration {
            {QStringLiteral("type"), QStringLiteral("rest")},
            {QStringLiteral("providerId"), QStringLiteral("test-provider")},
            {QStringLiteral("endpoint"), QStringLiteral("https://share.example")},
            {QStringLiteral("createPath"), QStringLiteral("/create")},
            {QStringLiteral("chunkPath"), QStringLiteral("/upload/{uploadId}")},
            {QStringLiteral("statusPath"), QStringLiteral("/status/{uploadId}")},
            {QStringLiteral("completePath"), QStringLiteral("/complete/{uploadId}")},
            {QStringLiteral("cancelPath"), QStringLiteral("/cancel/{uploadId}")},
            {QStringLiteral("credentialsRequired"), true},
            {QStringLiteral("authType"), QStringLiteral("bearer")},
            {QStringLiteral("secret"), secret},
        };
        QVERIFY(controller.configureProvider(configuration));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("configure"));
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(controller.configured(), 5'000);
        QVERIFY(controller.configuration().value(
            QStringLiteral("credentialStored")).toBool());
        QVERIFY(backend.containsValue(secret));
        QVERIFY(backend.storeThread != 0U);
        QCOMPARE(backend.storeThread, controller.workerThreadToken());
        QVERIFY(backend.storeThread != guiThread);

        QSettings persisted;
        persisted.sync();
        for (const QString& key : persisted.allKeys()) {
            QVERIFY2(!persisted.value(key).toString().contains(secret),
                     qPrintable(key));
        }
        QFile settingsFile(persisted.fileName());
        QVERIFY(settingsFile.open(QIODevice::ReadOnly));
        QVERIFY(!settingsFile.readAll().contains(secret.toUtf8()));

        operationSpy.clear();
        QVERIFY(controller.setEnabled(true));
        QTRY_VERIFY_WITH_TIMEOUT(controller.enabled(), 5'000);
        QCOMPARE(controller.providerSupportsInbox(), false);

        QImage source(32, 24, QImage::Format_RGB32);
        source.fill(QColor(QStringLiteral("#2c98d4")));
        source.setText(QStringLiteral("Comment"),
                       QStringLiteral("private-location-metadata"));
        const QString sourcePath = storage.filePath(
            QStringLiteral("source-with-metadata.png"));
        QVERIFY(source.save(sourcePath, "PNG"));
        QImageReader sourceReader(sourcePath);
        QVERIFY(sourceReader.textKeys().contains(QStringLiteral("Comment")));
        operationSpy.clear();
        QVERIFY(controller.upload(QUrl::fromLocalFile(sourcePath),
                                  QStringLiteral("recipient-1"),
                                  QStringLiteral("Martin M1"), {}, true));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("upload"));
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(controller.activeTransfers()->rowCount() >= 1,
                                 5'000);
        QAbstractItemModel* activeModel = controller.activeTransfers();
        int transferIdRole = -1;
        int stateRole = -1;
        int canPauseRole = -1;
        int canResumeRole = -1;
        int canRemoveRemoteCopyRole = -1;
        int remoteCopyActionRole = -1;
        int expiresUtcRole = -1;
        int messageRole = -1;
        int sha256Role = -1;
        int privacySummaryRole = -1;
        const QHash<int, QByteArray> roles = activeModel->roleNames();
        for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
            if (it.value() == QByteArrayLiteral("transferId")) {
                transferIdRole = it.key();
            } else if (it.value() == QByteArrayLiteral("transferState")) {
                stateRole = it.key();
            } else if (it.value() == QByteArrayLiteral("canPause")) {
                canPauseRole = it.key();
            } else if (it.value() == QByteArrayLiteral("canResume")) {
                canResumeRole = it.key();
            } else if (it.value()
                       == QByteArrayLiteral("canRemoveRemoteCopy")) {
                canRemoveRemoteCopyRole = it.key();
            } else if (it.value()
                       == QByteArrayLiteral("remoteCopyAction")) {
                remoteCopyActionRole = it.key();
            } else if (it.value() == QByteArrayLiteral("expiresUtc")) {
                expiresUtcRole = it.key();
            } else if (it.value() == QByteArrayLiteral("message")) {
                messageRole = it.key();
            } else if (it.value() == QByteArrayLiteral("sha256")) {
                sha256Role = it.key();
            } else if (it.value() == QByteArrayLiteral("privacySummary")) {
                privacySummaryRole = it.key();
            }
        }
        QVERIFY(transferIdRole > 0);
        QVERIFY(stateRole > 0);
        QVERIFY(canPauseRole > 0);
        QVERIFY(canResumeRole > 0);
        QVERIFY(canRemoveRemoteCopyRole > 0);
        QVERIFY(remoteCopyActionRole > 0);
        QVERIFY(expiresUtcRole > 0);
        QVERIFY(messageRole > 0);
        QVERIFY(sha256Role > 0);
        QVERIFY(privacySummaryRole > 0);
        const QString transferId = activeModel->data(
            activeModel->index(0, 0), transferIdRole).toString();
        QVERIFY(!transferId.isEmpty());
        QVERIFY(!activeModel->data(activeModel->index(0, 0),
                                   canRemoveRemoteCopyRole).toBool());
        QCOMPARE(activeModel->data(activeModel->index(0, 0),
                                   remoteCopyActionRole).toString(),
                 QStringLiteral("unavailable"));
        const QDateTime defaultExpiry = activeModel->data(
            activeModel->index(0, 0), expiresUtcRole).toDateTime();
        QVERIFY(defaultExpiry.isValid());
        QVERIFY(defaultExpiry > QDateTime::currentDateTimeUtc().addDays(6));
        const QVariantMap defaultPrivacy = activeModel->data(
            activeModel->index(0, 0), privacySummaryRole).toMap();
        QCOMPARE(defaultPrivacy.value(QStringLiteral("publicShare")).toBool(),
                 false);
        QCOMPARE(defaultPrivacy.value(
            QStringLiteral("callsignIncluded")).toBool(), false);
        QCOMPARE(defaultPrivacy.value(QStringLiteral("gridIncluded")).toBool(),
                 false);
        QCOMPARE(defaultPrivacy.value(
            QStringLiteral("meteredNetworkAllowed")).toBool(), false);
        QCOMPARE(defaultPrivacy.value(
            QStringLiteral("endToEndEncrypted")).toBool(), false);
        QCOMPARE(defaultPrivacy.value(
            QStringLiteral("providerCanReadContent")).toBool(), true);
        QVERIFY(controller.metaObject()->indexOfMethod(
            "removeRemoteCopy(QString)") >= 0);
        QVERIFY(controller.metaObject()->indexOfMethod(
            "uploadWithOptions(QUrl,QString,QString,QString,bool,QVariantMap)")
                >= 0);
        QVERIFY(controller.metaObject()->indexOfMethod(
            "saveAs(QString,QUrl)") >= 0);
        QVERIFY(controller.metaObject()->indexOfMethod(
            "deleteLocalCopy(QString)") >= 0);
        QVERIFY(controller.metaObject()->indexOfMethod(
            "requestProviderDeletion(QString)") >= 0);
        QVERIFY(controller.metaObject()->indexOfMethod(
            "blockSender(QString,bool)") >= 0);
        QVERIFY(!controller.preSignedAvailable());
        QVERIFY(!controller.peerRelayAvailable());
        QVERIFY(controller.preSignedUnavailableReason().contains(
            QStringLiteral("trusted broker"), Qt::CaseInsensitive));
        QVERIFY(controller.peerRelayUnavailableReason().contains(
            QStringLiteral("backend"), Qt::CaseInsensitive));
        operationSpy.clear();
        QVERIFY(controller.removeRemoteCopy(transferId));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(),
                 QStringLiteral("remove-remote-copy"));
        QVERIFY(!operationSpy.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(activeModel->data(
            activeModel->index(0, 0), canPauseRole).toBool(), 5'000);

        operationSpy.clear();
        QVERIFY(controller.pause(transferId));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("pause"));
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(activeModel->data(
            activeModel->index(0, 0), stateRole).toString(),
            QStringLiteral("Paused"), 5'000);
        QVERIFY(activeModel->data(activeModel->index(0, 0),
                                  canResumeRole).toBool());

        operationSpy.clear();
        QVERIFY(controller.resume(transferId));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("resume"));
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(activeModel->data(
            activeModel->index(0, 0), stateRole).toString()
            != QStringLiteral("Paused"), 5'000);
        const QDir outgoing(storage.filePath(
            QStringLiteral("sharing/outgoing")));
        const QStringList staged = outgoing.entryList(
            {QStringLiteral("*.png")}, QDir::Files | QDir::NoSymLinks);
        QCOMPARE(staged.size(), 1);
        QImageReader sanitized(outgoing.filePath(staged.constFirst()));
        QVERIFY(sanitized.canRead());
        QVERIFY2(sanitized.textKeys().isEmpty(),
                 "Sanitized upload retained source text/EXIF metadata");

        operationSpy.clear();
        QVariantMap explicitPrivacy {
            {QStringLiteral("expiryHours"), 24},
            {QStringLiteral("includeCallsign"), true},
            {QStringLiteral("callsign"), QStringLiteral("9H1TEST")},
            {QStringLiteral("includeGrid"), true},
            {QStringLiteral("grid"), QStringLiteral("JM75FV")},
            {QStringLiteral("meteredNetworkAllowed"), true},
        };
        QVERIFY(controller.uploadWithOptions(
            QUrl::fromLocalFile(sourcePath), QStringLiteral("recipient-2"),
            QStringLiteral("Scottie S1"), QStringLiteral("privacy-check"),
            true, explicitPrivacy));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("upload"));
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(activeModel->rowCount() >= 2, 5'000);
        int explicitRow = -1;
        for (int row = 0; row < activeModel->rowCount(); ++row) {
            if (activeModel->data(activeModel->index(row, 0), messageRole)
                    .toString() == QStringLiteral("privacy-check")) {
                explicitRow = row;
                break;
            }
        }
        QVERIFY(explicitRow >= 0);
        const QVariantMap explicitSummary = activeModel->data(
            activeModel->index(explicitRow, 0), privacySummaryRole).toMap();
        QCOMPARE(explicitSummary.value(
            QStringLiteral("callsignIncluded")).toBool(), true);
        QCOMPARE(explicitSummary.value(
            QStringLiteral("gridIncluded")).toBool(), true);
        QCOMPARE(explicitSummary.value(
            QStringLiteral("meteredNetworkAllowed")).toBool(), true);
        QCOMPARE(explicitSummary.value(
            QStringLiteral("publicShare")).toBool(), false);
        const QDateTime explicitExpiry = activeModel->data(
            activeModel->index(explicitRow, 0), expiresUtcRole).toDateTime();
        QVERIFY(explicitExpiry > QDateTime::currentDateTimeUtc().addSecs(
            23 * 60 * 60));
        QVERIFY(explicitExpiry < QDateTime::currentDateTimeUtc().addSecs(
            25 * 60 * 60));
        QVERIFY(activeModel->data(activeModel->index(explicitRow, 0),
                                  sha256Role).toString().size() == 64);

        operationSpy.clear();
        QVERIFY(controller.uploadWithOptions(
            QUrl::fromLocalFile(sourcePath), QStringLiteral("recipient-3"),
            QStringLiteral("Martin M1"), {}, true,
            {{QStringLiteral("expiryHours"), 24}}));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("upload"));
        QVERIFY(!operationSpy.last().at(1).toBool());

        const QVariantMap diagnostics = controller.diagnostics();
        const QSet<QString> expectedDiagnosticKeys {
            QStringLiteral("schemaVersion"),
            QStringLiteral("uploadedBytes"),
            QStringLiteral("downloadedBytes"),
            QStringLiteral("reclaimedRows"),
            QStringLiteral("uploadBytesPerSecond"),
            QStringLiteral("downloadBytesPerSecond"),
            QStringLiteral("activeQueueDepth"),
            QStringLiteral("uploadQueueDepth"),
            QStringLiteral("downloadQueueDepth"),
            QStringLiteral("resetUtc"),
        };
        const QStringList diagnosticKeyList = diagnostics.keys();
        QCOMPARE(QSet<QString>(diagnosticKeyList.cbegin(),
                               diagnosticKeyList.cend()),
                 expectedDiagnosticKeys);
        const QByteArray diagnosticBytes = QJsonDocument::fromVariant(
            diagnostics).toJson(QJsonDocument::Compact);
        QVERIFY(!diagnosticBytes.contains(secret.toUtf8()));
        QVERIFY(!diagnosticBytes.contains(sourcePath.toUtf8()));
        QVERIFY(!diagnosticBytes.contains("share.example"));
        operationSpy.clear();
        QVERIFY(controller.resetDiagnostics());
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(),
                 QStringLiteral("reset-diagnostics"));
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(controller.diagnostics().value(
            QStringLiteral("uploadedBytes")).toULongLong(), 0U, 5'000);

        operationSpy.clear();
        QVERIFY(controller.refreshInbox());
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(operationSpy.last().at(0).toString(),
                 QStringLiteral("refresh-inbox"));
        QVERIFY(!operationSpy.last().at(1).toBool());

        QVERIFY(controller.setEnabled(false));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.enabled(), 5'000);
        operationSpy.clear();
        QVERIFY(controller.cancel(transferId));
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QVERIFY(operationSpy.last().at(1).toBool());
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            for (int row = 0; row < activeModel->rowCount(); ++row) {
                const QModelIndex index = activeModel->index(row, 0);
                if (activeModel->data(index, transferIdRole).toString()
                        == transferId) {
                    return activeModel->data(index, stateRole).toString()
                        == QStringLiteral("CancelPending");
                }
            }
            return false;
        }(), 5'000);
        operationSpy.clear();
        QVERIFY(controller.clearCredentials());
        QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
        QCOMPARE(backend.removeThread, controller.workerThreadToken());
        QVERIFY(backend.removeThread != guiThread);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.configuration().value(
            QStringLiteral("credentialStored")).toBool(), 5'000);
        controller.shutdown();
    }

private:
    QTemporaryDir m_settingsRoot;
};

QTEST_GUILESS_MAIN(TestSstvShareController)

#include "test_sstv_share_controller.moc"
