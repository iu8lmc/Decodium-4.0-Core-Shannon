// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/models/SstvShareController.h"
#include "src/sstv/sharing/SstvLocalIntegrationShareProvider.h"
#include "src/sstv/sharing/SstvShareQueueManager.h"
#include "src/sstv/sharing/SstvShareTransfer.h"

#include <QAbstractItemModel>
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>

#include <limits>
#include <memory>
#include <optional>

using namespace decodium::sstv;
using namespace decodium::sstv::sharing;

namespace {

QString sha256Hex(const QByteArray& bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool spinUntil(const std::function<bool()>& predicate, int timeoutMs = 5'000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1U);
    }
    return predicate();
}

struct Awaited final
{
    SstvShareProviderResult result;
    SstvShareOperationId operationId {0U};
    bool completed {false};
};

Awaited awaitResult(
    const std::function<SstvShareOperationId(
        SstvShareProviderCompletion)>& start,
    int timeoutMs = 2'000)
{
    Awaited awaited;
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    awaited.operationId = start([&](SstvShareProviderResult result) {
        awaited.result = std::move(result);
        awaited.completed = true;
        loop.quit();
    });
    if (awaited.operationId != 0U && !awaited.completed) {
        guard.start(timeoutMs);
        loop.exec();
    }
    return awaited;
}

struct AwaitedIncoming final
{
    SstvShareProviderResult result;
    QVector<SstvShareIncomingItem> items;
    SstvShareOperationId operationId {0U};
    bool completed {false};
};

AwaitedIncoming awaitIncoming(
    const std::function<SstvShareOperationId(
        SstvShareIncomingCompletion)>& start)
{
    AwaitedIncoming awaited;
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    awaited.operationId = start(
        [&](SstvShareProviderResult result,
            QVector<SstvShareIncomingItem> items) {
            awaited.result = std::move(result);
            awaited.items = std::move(items);
            awaited.completed = true;
            loop.quit();
        });
    if (awaited.operationId != 0U && !awaited.completed) {
        guard.start(2'000);
        loop.exec();
    }
    return awaited;
}

SstvShareManifestV1 manifestFor(
    const QByteArray& bytes,
    const QDateTime& now,
    const QString& sender = QStringLiteral("station:remote"),
    const QString& recipient = QStringLiteral("station:local"),
    quint64 chunkBytes = 4U)
{
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid::createUuid();
    manifest.providerId = QStringLiteral("local-integration");
    manifest.senderId = sender;
    manifest.recipientId = recipient;
    manifest.createdUtc = now.addSecs(-5);
    manifest.expiresUtc = now.addDays(1);
    manifest.mediaUtc = now.addSecs(-10);
    manifest.originalFilename = QStringLiteral("sstv-test.png");
    manifest.safeDisplayFilename = QStringLiteral("SSTV test.png");
    manifest.mimeType = QStringLiteral("image/png");
    manifest.byteSize = static_cast<quint64>(bytes.size());
    manifest.sha256 = sha256Hex(bytes);
    manifest.width = 32U;
    manifest.height = 24U;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.chunkCount = static_cast<quint32>(
        std::max<quint64>(1U,
            (manifest.byteSize + chunkBytes - 1U) / chunkBytes));
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

QByteArray pngPayload()
{
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(QColor(22, 90, 150));
    QByteArray bytes;
    QBuffer output(&bytes);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG")) {
        return {};
    }
    return bytes;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size() && file.flush();
}

int roleFor(const QAbstractItemModel* model, const QByteArray& name)
{
    if (!model) {
        return -1;
    }
    const auto roles = model->roleNames();
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        if (role.value() == name) {
            return role.key();
        }
    }
    return -1;
}

} // namespace

class TestSstvLocalIntegrationProvider final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void residentBudgetLifecycleAndAcknowledgementSemantics();
    void pendingPayloadFloodAndOperationIdExhaustionFailClosed();
    void completionUsesOneClockBoundaryAndKeepsBudgetInvariant();
    void queueManagerRoundTripIsDeterministic();
    void controllerInjectionRoundTripIsExplicitAndFunctional();

private:
    QTemporaryDir m_settingsRoot;
};

void TestSstvLocalIntegrationProvider::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("DecodiumSstvTests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("LocalIntegrationProvider"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       m_settingsRoot.path());
    QSettings settings;
    settings.clear();
    settings.sync();
}

void TestSstvLocalIntegrationProvider::
residentBudgetLifecycleAndAcknowledgementSemantics()
{
    const QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-08-24T12:00:00.000Z"),
        Qt::ISODateWithMs).toUTC();
    SstvLocalIntegrationShareProvider::Config config;
    config.maximumChunkBytes = 4U;
    config.maximumTotalBytes = 8U;
    config.maximumPendingOperations = 2;
    config.maximumSessions = 16;
    config.maximumObjects = 16;
    config.clock = [now] { return now; };
    SstvLocalIntegrationShareProvider provider(config);
    QVERIFY(provider.isConfigurationValid());

    const QByteArray firstBytes("12345678", 8);
    const SstvShareManifestV1 firstManifest = manifestFor(firstBytes, now);
    const QString firstKey =
        SstvShareTransfer::deriveIdempotencyKey(firstManifest);
    const Awaited firstCreate = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(firstManifest, firstKey,
                                              std::move(done));
        });
    QVERIFY(firstCreate.completed && firstCreate.result.ok());
    SstvShareUploadHandle firstHandle = firstCreate.result.handle();
    for (quint64 offset = 0U; offset < 8U; offset += 4U) {
        const QByteArray chunk = firstBytes.mid(
            static_cast<qsizetype>(offset), 4);
        const Awaited uploaded = awaitResult(
            [&](SstvShareProviderCompletion done) {
                return provider.uploadChunkAsync(
                    firstHandle, offset, chunk, sha256Hex(chunk), {},
                    std::move(done));
            });
        QVERIFY(uploaded.completed && uploaded.result.ok());
        firstHandle = uploaded.result.handle();
    }

    const QByteArray secondBytes("ABCD", 4);
    const SstvShareManifestV1 secondManifest = manifestFor(secondBytes, now);
    const QString secondKey =
        SstvShareTransfer::deriveIdempotencyKey(secondManifest);
    const Awaited secondCreate = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(secondManifest, secondKey,
                                              std::move(done));
        });
    QVERIFY(secondCreate.result.ok());
    const Awaited budgetRejected = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.uploadChunkAsync(
                secondCreate.result.handle(), 0U, secondBytes,
                sha256Hex(secondBytes), {}, std::move(done));
        });
    QCOMPARE(budgetRejected.result.category(),
             SstvShareProviderFailure::RateLimited);

    const Awaited cancelled = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.cancelUploadAsync(firstHandle, std::move(done));
        });
    QVERIFY(cancelled.result.ok());
    QCOMPARE(cancelled.result.handle().committedBytes, 0U);

    const Awaited secondUpload = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.uploadChunkAsync(
                secondCreate.result.handle(), 0U, secondBytes,
                sha256Hex(secondBytes), {}, std::move(done));
        });
    QVERIFY(secondUpload.result.ok());
    const Awaited secondComplete = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.completeUploadAsync(
                secondUpload.result.handle(), secondKey, std::move(done));
        });
    QVERIFY(secondComplete.result.ok());
    const Awaited repeatedComplete = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.completeUploadAsync(
                secondUpload.result.handle(), secondKey, std::move(done));
        });
    QVERIFY(repeatedComplete.result.ok());
    QCOMPARE(repeatedComplete.result.handle().opaqueId,
             secondComplete.result.handle().opaqueId);
    QCOMPARE(repeatedComplete.result.handle().committedBytes,
             static_cast<quint64>(secondBytes.size()));

    AwaitedIncoming listed = awaitIncoming(
        [&](SstvShareIncomingCompletion done) {
            return provider.listIncomingAsync(16, std::move(done));
        });
    QVERIFY(listed.result.ok());
    QCOMPARE(listed.items.size(), 1);
    const QString objectId = listed.items.constFirst().opaqueId;
    const Awaited acknowledged = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.acknowledgeAsync(objectId, std::move(done));
        });
    QVERIFY(acknowledged.result.ok());
    listed = awaitIncoming([&](SstvShareIncomingCompletion done) {
        return provider.listIncomingAsync(16, std::move(done));
    });
    // Acknowledgement is a durable decision marker, not inbox deletion.
    QCOMPARE(listed.items.size(), 1);

    const Awaited deleted = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.deleteRemoteObjectAsync(objectId,
                                                    std::move(done));
        });
    QVERIFY(deleted.result.ok());

    const QByteArray reclaimedBytes("WXYZ", 4);
    const auto reclaimedManifest = manifestFor(reclaimedBytes, now);
    const QString reclaimedKey =
        SstvShareTransfer::deriveIdempotencyKey(reclaimedManifest);
    const Awaited reclaimedCreate = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(
                reclaimedManifest, reclaimedKey, std::move(done));
        });
    const Awaited reclaimedUpload = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.uploadChunkAsync(
                reclaimedCreate.result.handle(), 0U, reclaimedBytes,
                sha256Hex(reclaimedBytes), {}, std::move(done));
        });
    QVERIFY(reclaimedUpload.result.ok());
}

void TestSstvLocalIntegrationProvider::
pendingPayloadFloodAndOperationIdExhaustionFailClosed()
{
    const QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-08-24T12:00:00.000Z"),
        Qt::ISODateWithMs).toUTC();
    SstvLocalIntegrationShareProvider::Config invalidConfig;
    invalidConfig.maximumChunkBytes = kMaximumSharedImageBytes;
    invalidConfig.maximumTotalBytes = kMaximumSharedImageBytes;
    invalidConfig.maximumPendingOperations = 4'096;
    SstvLocalIntegrationShareProvider invalidProvider(invalidConfig);
    QVERIFY(!invalidProvider.isConfigurationValid());

    SstvLocalIntegrationShareProvider::Config config;
    config.maximumChunkBytes = 4U;
    config.maximumTotalBytes = 16U;
    config.maximumPendingOperations = 4;
    config.maximumSessions = 16;
    config.clock = [now] { return now; };
    SstvLocalIntegrationShareProvider provider(config);
    QVERIFY(provider.isConfigurationValid());

    QVector<SstvShareUploadHandle> sessions;
    QVector<QByteArray> payloads;
    for (int index = 0; index < 5; ++index) {
        const QByteArray bytes(4, static_cast<char>('a' + index));
        payloads.push_back(bytes);
        const auto manifest = manifestFor(bytes, now);
        const Awaited created = awaitResult(
            [&](SstvShareProviderCompletion done) {
                return provider.createUploadAsync(
                    manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
                    std::move(done));
            });
        QVERIFY(created.result.ok());
        sessions.push_back(created.result.handle());
    }

    QVector<SstvShareOperationId> pending;
    int callbacks = 0;
    for (int index = 0; index < 4; ++index) {
        const QByteArray bytes = payloads.at(index);
        const SstvShareOperationId operationId = provider.uploadChunkAsync(
            sessions.at(index), 0U, bytes, sha256Hex(bytes), {},
            [&callbacks](SstvShareProviderResult) { ++callbacks; });
        QVERIFY(operationId != 0U);
        pending.push_back(operationId);
    }
    bool overflowCompleted = false;
    QCOMPARE(provider.uploadChunkAsync(
                 sessions.at(4), 0U, payloads.at(4),
                 sha256Hex(payloads.at(4)), {},
                 [&overflowCompleted](SstvShareProviderResult) {
                     overflowCompleted = true;
                 }), 0U);
    QVERIFY(!overflowCompleted);
    for (const SstvShareOperationId operationId : pending) {
        QVERIFY(provider.cancelOperation(operationId));
    }
    const Awaited reclaimedPending = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.uploadChunkAsync(
                sessions.at(4), 0U, payloads.at(4),
                sha256Hex(payloads.at(4)), {}, std::move(done));
        });
    QVERIFY(reclaimedPending.result.ok());
    QCOMPARE(callbacks, 0);

    SstvLocalIntegrationShareProvider::Config exhaustedConfig;
    exhaustedConfig.maximumChunkBytes = 4U;
    exhaustedConfig.maximumTotalBytes = 4U;
    exhaustedConfig.maximumPendingOperations = 1;
    exhaustedConfig.firstOperationId =
        std::numeric_limits<SstvShareOperationId>::max();
    exhaustedConfig.clock = [now] { return now; };
    SstvLocalIntegrationShareProvider exhausted(exhaustedConfig);
    int completions = 0;
    const SstvShareOperationId last = exhausted.refreshCredentialsAsync(
        [&completions](SstvShareProviderResult) { ++completions; });
    QCOMPARE(last, std::numeric_limits<SstvShareOperationId>::max());
    QCOMPARE(exhausted.refreshCredentialsAsync(
                 [&completions](SstvShareProviderResult) { ++completions; }),
             0U);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QCOMPARE(completions, 1);
    QCOMPARE(exhausted.refreshCredentialsAsync(
                 [&completions](SstvShareProviderResult) { ++completions; }),
             0U);
}

void TestSstvLocalIntegrationProvider::
completionUsesOneClockBoundaryAndKeepsBudgetInvariant()
{
    const QDateTime base = QDateTime::fromString(
        QStringLiteral("2026-08-24T12:00:00.000Z"),
        Qt::ISODateWithMs).toUTC();
    const QDateTime expiry = base.addSecs(60);
    auto current = std::make_shared<QDateTime>(base);
    auto boundaryReads = std::make_shared<int>(-1);
    SstvLocalIntegrationShareProvider::Config config;
    config.maximumChunkBytes = 4U;
    config.maximumTotalBytes = 4U;
    config.maximumPendingOperations = 1;
    config.clock = [current, boundaryReads, expiry] {
        if (*boundaryReads < 0) {
            return *current;
        }
        const int read = (*boundaryReads)++;
        if (read == 0) {
            return expiry.addMSecs(-2);
        }
        if (read == 1) {
            return expiry.addMSecs(-1);
        }
        return expiry.addMSecs(1);
    };
    SstvLocalIntegrationShareProvider provider(config);
    const QByteArray bytes("EDGE", 4);
    SstvShareManifestV1 manifest = manifestFor(bytes, base);
    manifest.expiresUtc = expiry;
    const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
    const Awaited created = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(manifest, key, std::move(done));
        });
    const Awaited uploaded = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.uploadChunkAsync(
                created.result.handle(), 0U, bytes, sha256Hex(bytes), {},
                std::move(done));
        });
    QVERIFY(uploaded.result.ok());

    *boundaryReads = 0;
    const Awaited completed = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.completeUploadAsync(
                uploaded.result.handle(), key, std::move(done));
        });
    QVERIFY(completed.result.ok());
    QCOMPARE(*boundaryReads, 2);

    *boundaryReads = -1;
    *current = expiry.addMSecs(-1);
    const Awaited repeated = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.completeUploadAsync(
                uploaded.result.handle(), key, std::move(done));
        });
    QVERIFY(repeated.result.ok());
    QCOMPARE(repeated.result.handle().opaqueId,
             completed.result.handle().opaqueId);
    QVERIFY(awaitResult([&](SstvShareProviderCompletion done) {
        return provider.deleteRemoteObjectAsync(
            completed.result.handle().opaqueId, std::move(done));
    }).result.ok());

    const QByteArray replacement("NEXT", 4);
    const auto replacementManifest = manifestFor(replacement, *current);
    const QString replacementKey =
        SstvShareTransfer::deriveIdempotencyKey(replacementManifest);
    const Awaited replacementCreate = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(
                replacementManifest, replacementKey, std::move(done));
        });
    const Awaited replacementUpload = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.uploadChunkAsync(
                replacementCreate.result.handle(), 0U, replacement,
                sha256Hex(replacement), {}, std::move(done));
        });
    QVERIFY(replacementUpload.result.ok());
}

void TestSstvLocalIntegrationProvider::queueManagerRoundTripIsDeterministic()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QByteArray bytes = pngPayload();
    QVERIFY(!bytes.isEmpty());
    const QString sourcePath = temporary.filePath(QStringLiteral("source.png"));
    QVERIFY(writeFile(sourcePath, bytes));
    const QDateTime now = QDateTime::fromString(
        QStringLiteral("2026-08-24T12:00:00.000Z"),
        Qt::ISODateWithMs).toUTC();

    SstvLocalIntegrationShareProvider::Config providerConfig;
    providerConfig.maximumChunkBytes = 1'024U;
    providerConfig.maximumTotalBytes = 8U * 1024U * 1024U;
    providerConfig.maximumPendingOperations = 64;
    providerConfig.clock = [now] { return now; };
    auto provider = std::make_shared<SstvLocalIntegrationShareProvider>(
        providerConfig);

    SstvShareQueueConfig queueConfig;
    queueConfig.databasePath = temporary.filePath(
        QStringLiteral("queue.sqlite"));
    queueConfig.allowedUploadRoots = {temporary.path()};
    queueConfig.downloadRoot = temporary.filePath(
        QStringLiteral("downloads"));
    queueConfig.limits.uploadChunkBytes = 1'024U;
    queueConfig.limits.downloadChunkBytes = 1'024U;
    queueConfig.meteredNetworkProbe = [] {
        return std::optional<bool> {false};
    };
    SstvShareQueueManager manager(queueConfig, {}, [now] { return now; });
    QString error;
    QVERIFY(manager.registerProvider(provider, &error));
    QVERIFY2(manager.initialize(&error), qPrintable(error));

    const SstvShareManifestV1 manifest = manifestFor(
        bytes, now, QStringLiteral("station:remote"),
        QStringLiteral("station:local"), 1'024U);
    const QString transferId = manager.queueUpload(
        manifest, sourcePath, &error);
    QVERIFY2(!transferId.isEmpty(), qPrintable(error));
    QVERIFY(manager.processDue(&error) > 0);
    QVERIFY(spinUntil([&] {
        manager.processDue();
        const auto record = manager.store().transfer(transferId);
        return record
            && record->state == SstvManagedTransferState::Completed;
    }));

    bool refreshed = false;
    QString refreshError;
    QVERIFY(manager.refreshInboxAsync(
        provider->providerId(), [&](SstvShareProviderResult result) {
            refreshed = result.ok();
            refreshError = result.redactedDiagnostic();
        }) != 0U);
    QVERIFY2(spinUntil([&] { return refreshed || !refreshError.isEmpty(); }),
             qPrintable(refreshError));
    QVERIFY2(refreshed, qPrintable(refreshError));
    const auto inbox = manager.inbox(10, &error);
    QCOMPARE(inbox.size(), 1);
    const QString incomingId = inbox.constFirst().incomingId;
    const QString downloadId = manager.queueDownload(
        provider->providerId(), incomingId,
        QStringLiteral("received.png"), &error);
    QVERIFY2(!downloadId.isEmpty(), qPrintable(error));
    QVERIFY(manager.processDue(&error) > 0);
    QVERIFY(spinUntil([&] {
        manager.processDue();
        const auto record = manager.store().transfer(downloadId);
        return record && record->state
            == SstvManagedTransferState::AwaitingAcceptance;
    }));
    QVERIFY(manager.acceptDownload(downloadId, &error));
    bool acknowledged = false;
    QVERIFY(manager.acknowledgeDownloadAsync(
        downloadId, [&](SstvShareProviderResult result) {
            acknowledged = result.ok();
        }) != 0U);
    QVERIFY(spinUntil([&] { return acknowledged; }));
    const AwaitedIncoming providerInbox = awaitIncoming(
        [&](SstvShareIncomingCompletion done) {
            return provider->listIncomingAsync(10, std::move(done));
        });
    QVERIFY(providerInbox.result.ok());
    QCOMPARE(providerInbox.items.size(), 1);
    const auto diagnostics = manager.diagnostics(&error);
    QCOMPARE(diagnostics.uploadedBytes, static_cast<quint64>(bytes.size()));
    QCOMPARE(diagnostics.downloadedBytes, static_cast<quint64>(bytes.size()));
}

void TestSstvLocalIntegrationProvider::
controllerInjectionRoundTripIsExplicitAndFunctional()
{
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    const QByteArray bytes = pngPayload();
    const QString sourcePath = storage.filePath(QStringLiteral("source.png"));
    QVERIFY(writeFile(sourcePath, bytes));

    SstvLocalIntegrationShareProvider::Config config;
    config.localRecipientId = QStringLiteral("station:local");
    config.participantIds = {
        QStringLiteral("station:local"),
        QStringLiteral("station-local")};
    config.maximumChunkBytes = 64U * 1024U;
    config.maximumTotalBytes = 4U * 1024U * 1024U;
    config.maximumPendingOperations = 64;
    auto provider = std::make_shared<SstvLocalIntegrationShareProvider>(config);
    QVERIFY(provider->isConfigurationValid());

    SstvShareController controller(nullptr, provider);
    QSignalSpy operationSpy(&controller,
                            &SstvShareController::operationFinished);
    controller.setStorageRoot(storage.path(), QStringLiteral("station-local"));
    QTRY_VERIFY_WITH_TIMEOUT(controller.ready(), 5'000);
    QVERIFY(controller.configured());
    QCOMPARE(controller.configuration().value(
                 QStringLiteral("type")).toString(),
             QStringLiteral("local-integration"));
    QVERIFY(!controller.configuration().contains(QStringLiteral("secret")));
    QVERIFY(controller.setEnabled(true));
    QTRY_VERIFY_WITH_TIMEOUT(controller.enabled(), 5'000);
    QVERIFY(controller.providerSupportsInbox());

    operationSpy.clear();
    const QVariantMap options {
        {QStringLiteral("expiryHours"), 24},
        {QStringLiteral("includeCallsign"), false},
        {QStringLiteral("callsign"), QString {}},
        {QStringLiteral("includeGrid"), false},
        {QStringLiteral("grid"), QString {}},
        {QStringLiteral("meteredNetworkAllowed"), true},
    };
    QVERIFY(controller.uploadWithOptions(
        QUrl::fromLocalFile(sourcePath), QStringLiteral("station:local"),
        QStringLiteral("Martin M1"), QString {}, true, options));
    QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
    QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("upload"));
    QVERIFY(operationSpy.last().at(1).toBool());
    QAbstractItemModel* history = controller.transferHistory();
    QTRY_VERIFY_WITH_TIMEOUT(history->rowCount() >= 1, 8'000);
    QTRY_VERIFY_WITH_TIMEOUT(controller.diagnostics().value(
        QStringLiteral("uploadedBytes")).toULongLong() > 0U, 8'000);

    operationSpy.clear();
    QVERIFY(controller.refreshInbox());
    QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
    QCOMPARE(operationSpy.last().at(0).toString(),
             QStringLiteral("refresh-inbox"));
    QVERIFY(operationSpy.last().at(1).toBool());
    QAbstractItemModel* inbox = controller.inbox();
    QTRY_VERIFY_WITH_TIMEOUT(inbox->rowCount() == 1, 5'000);
    const int incomingRole = roleFor(inbox, QByteArrayLiteral("incomingId"));
    QVERIFY(incomingRole >= 0);
    const QString incomingId = inbox->data(
        inbox->index(0, 0), incomingRole).toString();
    QVERIFY(!incomingId.isEmpty());

    operationSpy.clear();
    QVERIFY(controller.download(incomingId,
                                QStringLiteral("controller-received.png")));
    QTRY_VERIFY_WITH_TIMEOUT(operationSpy.count() >= 1, 5'000);
    QCOMPARE(operationSpy.last().at(0).toString(), QStringLiteral("download"));
    QVERIFY(operationSpy.last().at(1).toBool());
    QTRY_VERIFY_WITH_TIMEOUT(controller.diagnostics().value(
        QStringLiteral("downloadedBytes")).toULongLong() > 0U, 8'000);
    controller.shutdown();
}

QTEST_GUILESS_MAIN(TestSstvLocalIntegrationProvider)

#include "test_sstv_local_integration_provider.moc"
