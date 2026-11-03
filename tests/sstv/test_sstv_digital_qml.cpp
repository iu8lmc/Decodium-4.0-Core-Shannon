// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmController.h"
#include "src/sstv/digital/HamDrmHybridCodec.h"

#include <QtTest/QtTest>

#include <QBuffer>
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
#include <QVariantMap>

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

class TimingRxFixture final : public hamdrm::HamDrmWaveformRxBackend
{
public:
    bool selected {false};
    hamdrm::HamDrmWaveformCapability capability() const override
    { return {true, QStringLiteral("fixture"), QStringLiteral("fixture")}; }
    bool supportsExperimentalTiming() const noexcept override { return true; }
    bool setExperimentalTiming(bool enabled) noexcept override { selected = enabled; return true; }
    hamdrm::HamDrmStatus start(const hamdrm::HamDrmProfile&, std::uint64_t,
                               hamdrm::HamDrmWaveformRxSink&) override
    { return hamdrm::HamDrmStatus::success(); }
    void cancel(std::uint64_t) noexcept override {}
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

QObject* requiredObject(QObject* root, const char* objectName)
{
    QObject* object = root->findChild<QObject*>(
        QString::fromLatin1(objectName));
    if (object == nullptr) {
        qWarning() << "Missing required QML object" << objectName;
    }
    return object;
}

class WorkspaceCloseEngineFixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool sstvRxRequested READ sstvRxRequested CONSTANT)
    Q_PROPERTY(bool sstvTxActive READ sstvTxActive
               NOTIFY sstvTxActiveChanged)
    Q_PROPERTY(bool sstvRxActive READ sstvRxActive CONSTANT)
    Q_PROPERTY(bool sstvAvailable READ sstvAvailable CONSTANT)
    Q_PROPERTY(QObject* sstvDigital READ sstvDigital CONSTANT)

public:
    bool sstvRxRequested() const noexcept { return true; }
    bool sstvTxActive() const noexcept { return m_sstvTxActive; }
    bool sstvRxActive() const noexcept { return false; }
    bool sstvAvailable() const noexcept { return true; }
    QObject* sstvDigital() const noexcept { return nullptr; }
    QStringList calls() const { return m_calls; }

    Q_INVOKABLE void stopSstvRx()
    {
        m_calls.append(QStringLiteral("stop-rx"));
    }

    Q_INVOKABLE void cancelSstvTx()
    {
        m_calls.append(QStringLiteral("cancel-tx"));
        m_sstvTxActive = false;
        emit sstvTxActiveChanged();
    }

    Q_INVOKABLE void leaveSstvWorkspace()
    {
        m_calls.append(QStringLiteral("leave-workspace"));
    }

signals:
    void sstvTxActiveChanged();

private:
    bool m_sstvTxActive {true};
    QStringList m_calls;
};

} // namespace

class TestSstvDigitalQml final : public QObject
{
    Q_OBJECT

private slots:
    void hybridPickerRetainsReferenceToken_data()
    {
        QTest::addColumn<int>("transportId");
        QTest::newRow("A") << 123;
        QTest::newRow("B") << 456;
    }

    void hybridPickerRetainsReferenceToken()
    {
        QFETCH(int, transportId);
        hamdrm::HamDrmController controller({}, {});
        const auto reference = hamdrm::encodeHamDrmHybridServer({"example.invalid", "user", "dummy", "radio"});
        QVERIFY(reference.ok());
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.filename = "de_TEST-1-image.jpg";
        metadata.transportId = static_cast<std::uint16_t>(transportId);
        const auto object = hamdrm::encodeHamDrmObject(metadata, *reference.value, 128);
        QVERIFY(object.ok());
        const auto ingest = [&] {
            for (const auto& group : object.value->headerGroups)
                if (!controller.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok()) return false;
            for (const auto& group : object.value->bodyGroups)
                if (!controller.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok()) return false;
            return true;
        };
        QVERIFY(ingest());
        const auto token = controller.inbox().front().toMap().value("hybridReferenceToken").toString();
        QVERIFY(!token.isEmpty());
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)), QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {{QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))}};
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        auto* item = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QVERIFY(QMetaObject::invokeMethod(page.data(), "prepareHybridSave",
                                          Q_ARG(QVariant, transportId), Q_ARG(QVariant, token)));
        QCOMPARE(page->property("hybridSaveId").toInt(), transportId);
        QCOMPARE(page->property("hybridSaveToken").toString(), token);
        QVERIFY(controller.discardObject(transportId));
        QVERIFY(ingest());
        QVERIFY(controller.inbox().front().toMap().value("hybridReferenceToken").toString() != token);
        QCOMPARE(page->property("hybridSaveToken").toString(), token);
    }

    void completedImageShowsSaveControl_data()
    {
        QTest::addColumn<int>("width");
        QTest::newRow("A") << 24;
        QTest::newRow("B") << 31;
    }

    void completedImageShowsSaveControl()
    {
        QFETCH(int, width);
        hamdrm::HamDrmController controller({}, {});
        QImage image(width, 20, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QByteArray png;
        QBuffer buffer(&png);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.filename = "received.png";
        metadata.transportId = 177;
        const std::vector<std::uint8_t> body(png.begin(), png.end());
        const auto object = hamdrm::encodeHamDrmObject(metadata, body, 128);
        QVERIFY(object.ok());
        const auto ingest = [&controller](const std::vector<std::uint8_t>& group) {
            return controller.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok();
        };
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        QVERIFY(!controller.selectedImageStructuralOnly());
        QCOMPARE(controller.inbox().front().toMap().value(QStringLiteral("state")).toString(), QStringLiteral("complete"));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {{QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))}};
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        auto* item = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->findChild<QObject*>(QStringLiteral("hamdrmSaveCompletedImage")), 5000);
        QObject* button = page->findChild<QObject*>(QStringLiteral("hamdrmSaveCompletedImage"));
        QVERIFY(button->property("enabled").toBool());
        QVERIFY(button->property("visible").toBool());
    }

    void structuralOnlyShowsNotice_data() { completedImageShowsSaveControl_data(); }
    void structuralOnlyShowsNotice()
    {
        QFETCH(int, width);
        hamdrm::HamDrmController controller({}, {});
        std::vector<std::uint8_t> body {
            0xff, 0xd8, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x14,
            0x00, static_cast<std::uint8_t>(width), 0x03,
            0x01, 0x11, 0x00, 0x02, 0x11, 0x00, 0x03, 0x11, 0x00, 0xff, 0xd9
        };
        body.resize(160U, 0);
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.filename = "structural.jpg"; metadata.transportId = 178;
        const auto object = hamdrm::encodeHamDrmObject(metadata, body, 128);
        QVERIFY(object.ok());
        const auto ingest = [&controller](const std::vector<std::uint8_t>& group) {
            return controller.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok();
        };
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {{QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))}};
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        auto* item = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QObject* notice = requiredObject(page.data(), "hamdrmStructuralOnlyNotice");
        QObject* button = requiredObject(page.data(), "hamdrmSaveCompletedImage");
        QVERIFY(notice && button);
        QTRY_VERIFY(notice->property("visible").toBool());
        QVERIFY(button->property("enabled").toBool());
    }

    void hybridModeDoesNotUseOrdinaryTx_data()
    {
        QTest::addColumn<QString>("callsign");
        QTest::addColumn<QString>("host");
        QTest::newRow("A") << QStringLiteral("TEST1") << QStringLiteral("a.example.invalid");
        QTest::newRow("B") << QStringLiteral("TEST2") << QStringLiteral("b.example.invalid");
    }

    void hybridModeDoesNotUseOrdinaryTx()
    {
        QFETCH(QString, callsign);
        QFETCH(QString, host);
        hamdrm::HamDrmController controller({}, {});
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {{QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))}};
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        auto* item = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QObject* toggle = requiredObject(page.data(), "hamdrmHybridTxEnabled");
        QObject* normal = requiredObject(page.data(), "hamdrmStartTx");
        QObject* hybrid = requiredObject(page.data(), "hamdrmStartHybridTx");
        QObject* call = requiredObject(page.data(), "hamdrmHybridCall");
        QObject* server = requiredObject(page.data(), "hamdrmHybridHost");
        QObject* password = requiredObject(page.data(), "hamdrmHybridPassword");
        QObject* recovery = requiredObject(page.data(), "hamdrmRsLevel");
        QVERIFY(toggle && normal && hybrid && call && server && password && recovery);
        QVERIFY(page->setProperty("selectedTxImage", QUrl::fromLocalFile("/tmp/synthetic.jpg")));
        QTRY_VERIFY(normal->property("enabled").toBool());
        QVERIFY(toggle->setProperty("checked", true));
        QTRY_VERIFY(!normal->property("enabled").toBool());
        QTRY_VERIFY(!recovery->property("enabled").toBool());
        QVERIFY(!hybrid->property("enabled").toBool());
        QVERIFY(call->setProperty("text", callsign));
        QVERIFY(server->setProperty("text", host));
        QTRY_VERIFY(hybrid->property("enabled").toBool());
        QCOMPARE(password->property("echoMode").toInt(), 2); // TextInput.Password
        QVERIFY(toggle->setProperty("checked", false));
        QTRY_VERIFY(normal->property("enabled").toBool());
        QTRY_VERIFY(recovery->property("enabled").toBool());
    }

    void unknownBsrNotice_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("A-extended-unknown") << QByteArray("31746\nH_OK\n0\n0\n1\n-99\nrepair-a.jpg\n21010\n");
        QTest::newRow("B-compact-unknown") << QByteArray("29441\nH_OK\n0\n0\n-1\n1\n-99\n");
        QTest::newRow("C-known-size") << QByteArray("29441\nH_OK\n1024\n0\n-1\n1\n-99\n");
    }

    void unknownBsrNotice()
    {
        QFETCH(QByteArray, body);
        hamdrm::HamDrmControllerBackends backends;
        backends.waveformRx = std::make_shared<TimingRxFixture>();
        hamdrm::HamDrmController controller({}, std::move(backends));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)), QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {{QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))}};
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        auto* item = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QObject* notice = requiredObject(page.data(), "hamdrmBsrUnknownSizeNotice");
        QVERIFY(notice);
        QVERIFY(!notice->property("visible").toBool());
        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x7131;
        metadata.filename = "bsr.bin";
        const std::vector<std::uint8_t> input(body.begin(), body.end());
        const auto encoded = hamdrm::encodeHamDrmObject(metadata, input, 128);
        QVERIFY(encoded.ok());
        const auto asBytes = [](const std::vector<std::uint8_t>& group) {
            return QByteArray(reinterpret_cast<const char*>(group.data()), static_cast<qsizetype>(group.size()));
        };
        for (const auto& group : encoded.value->headerGroups) QVERIFY(controller.ingestMotGroup(asBytes(group)).ok());
        for (const auto& group : encoded.value->bodyGroups) QVERIFY(controller.ingestMotGroup(asBytes(group)).ok());
        const bool unknown = body.split('\n')[2] == "0";
        QCOMPARE(controller.receivedBsrSegmentSizeUnknown(), unknown);
        QTRY_COMPARE(notice->property("visible").toBool(), unknown);
        QVERIFY(!controller.canRepairReceivedBsr());
        QVERIFY(controller.startRx());
        QTRY_VERIFY(!notice->property("visible").toBool());
    }

    void bsrCapabilityTracksRealController_data()
    {
        QTest::addColumn<int>("segmentSize");
        QTest::addColumn<int>("dialect");
        QTest::newRow("A-extended") << 64 << 0;
        QTest::newRow("B-compact") << 128 << 1;
    }

    void bsrCapabilityTracksRealController()
    {
        QFETCH(int, segmentSize);
        QFETCH(int, dialect);
        hamdrm::HamDrmController controller({}, {});
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)), QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial {{QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller))}};
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        auto* item = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QObject* build = requiredObject(page.data(), "hamdrmBuildBsr");
        QObject* selector = requiredObject(page.data(), "hamdrmBsrDialect");
        QVERIFY(build && selector);
        QVERIFY(!build->property("enabled").toBool());
        QVERIFY(selector->setProperty("currentIndex", dialect));

        hamdrm::HamDrmMotObjectMetadata metadata;
        metadata.transportId = 0x2451;
        metadata.filename = "test.jpg";
        const std::vector<std::uint8_t> payload(400U, 0x72);
        const auto source = hamdrm::encodeHamDrmObject(metadata, payload, static_cast<std::size_t>(segmentSize));
        QVERIFY(source.ok());
        const auto asBytes = [](const std::vector<std::uint8_t>& group) {
            return QByteArray(reinterpret_cast<const char*>(group.data()), static_cast<qsizetype>(group.size()));
        };
        for (const auto& group : source.value->headerGroups)
            QVERIFY(controller.ingestMotGroup(asBytes(group)).ok());
        QVERIFY(controller.ingestMotGroup(asBytes(source.value->bodyGroups.back())).ok());
        QVERIFY(controller.canBuildBsr());
        QTRY_VERIFY(build->property("enabled").toBool());
        QVERIFY(selector->property("enabled").toBool());

        const auto first = hamdrm::parseHamDrmMotDataGroup(source.value->bodyGroups.front().data(),
                                                           source.value->bodyGroups.front().size());
        QVERIFY(first.ok());
        auto shortened = *first.value;
        shortened.payload.pop_back();
        const auto malformed = hamdrm::encodeHamDrmMotDataGroup(shortened);
        QVERIFY(malformed.ok());
        QVERIFY(controller.ingestMotGroup(asBytes(*malformed.value)).ok());
        QVERIFY(!controller.canBuildBsr());
        QTRY_VERIFY(!build->property("enabled").toBool());
        QVERIFY(!selector->property("enabled").toBool());
    }

    void experimentalTimingToggle()
    {
        auto rx = std::make_shared<TimingRxFixture>();
        hamdrm::HamDrmControllerBackends backends;
        backends.waveformRx = rx;
        hamdrm::HamDrmController controller({}, std::move(backends));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QString::fromUtf8(DECODIUM_SSTV_DIGITAL_QML_SOURCE)), QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial;
        initial.insert(QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(&controller)));
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY(page);
        QObject* toggle = requiredObject(page.data(), "hamdrmExperimentalTiming");
        QObject* guidance = requiredObject(page.data(), "hamdrmAdaptiveGuidance");
        QVERIFY(guidance);
        QVERIFY(guidance->property("visible").toBool());
        QVERIFY(guidance->property("text").toString().contains(QStringLiteral("stop RX")));
        QVERIFY(toggle && toggle->property("enabled").toBool());
        auto* item = qobject_cast<QQuickItem*>(page.data());
        auto* checkbox = qobject_cast<QQuickItem*>(toggle);
        QVERIFY(item && checkbox);
        QQuickWindow window;
        window.resize(1000, 820);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1000, 820));
        window.show();
        QTest::qWait(100);
        const QPoint click = checkbox->mapToScene(QPointF(checkbox->width() / 2,
                                                            checkbox->height() / 2)).toPoint();
        QVERIFY(!toggle->property("checked").toBool());
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, click);
        QVERIFY(controller.experimentalTiming() && rx->selected);
        QVERIFY(toggle->property("checked").toBool());
        QVERIFY(guidance->property("text").toString().contains(QStringLiteral("is enabled")));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, click);
        QVERIFY(!controller.experimentalTiming() && !rx->selected);
        QVERIFY(!toggle->property("checked").toBool());
        QVERIFY(guidance->property("text").toString().contains(QStringLiteral("stop RX")));
        QVERIFY(controller.startRx());
        QVERIFY(!toggle->property("enabled").toBool());
    }
    void pageCreatesExposesNamedControlsAndRendersOffscreen()
    {
        QTemporaryDir partialRoot;
        QVERIFY(partialRoot.isValid());
        hamdrm::HamDrmControllerConfig config;
        config.partialStoreRoot = partialRoot.path();
        hamdrm::HamDrmControllerBackends backends;
        backends.jpeg2000 = hamdrm::makeNativeHamDrmJpeg2000Backend();
        hamdrm::HamDrmController controller(config, std::move(backends));

        QQmlEngine engine;
        QStringList runtimeWarnings;
        connect(&engine,
                &QQmlEngine::warnings,
                this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });

        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_DIGITAL_QML_SOURCE);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial;
        initial.insert(QStringLiteral("controller"),
                       QVariant::fromValue(
                           static_cast<QObject*>(&controller)));
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* page = qobject_cast<QQuickItem*>(object.data());
        QVERIFY(page);
        QCOMPARE(page->objectName(), QStringLiteral("sstvDigitalPage"));

        const QList<const char*> expectedObjects {
            "hamdrmCapabilityMessage",
            "hamdrmProfileSelector",
            "hamdrmStartRx",
            "hamdrmExperimentalTiming",
            "hamdrmCancelRx",
            "hamdrmChooseTxImage",
            "hamdrmValidateTxImage",
            "hamdrmStartTx",
            "hamdrmCancelTx",
            "hamdrmObjectInbox",
            "hamdrmResumeTransportId",
            "hamdrmResumePartial",
            "hamdrmBuildBsr",
            "hamdrmBsrDialect",
            "hamdrmSendBsr",
            "hamdrmDiscardObject",
            "hamdrmBsrText",
            "hamdrmReceivedBsrText",
            "hamdrmRepairBsr",
            "hamdrmError",
        };
        for (const char* name : expectedObjects) {
            QVERIFY2(requiredObject(page, name) != nullptr, name);
        }
        QCOMPARE(requiredObject(page, "hamdrmBsrDialect")->property("count").toInt(), 2);
        QVERIFY(!requiredObject(page, "hamdrmSendBsr")->property("enabled").toBool());
        QVERIFY(!requiredObject(page, "hamdrmRepairBsr")->property("enabled").toBool());

        QObject* selector = requiredObject(page, "hamdrmProfileSelector");
        QVERIFY(selector);
        QCOMPARE(selector->property("count").toInt(), 72);
        QCOMPARE(selector->property("currentValue").toString(),
                 controller.selectedProfileId());
        const QString capability = requiredObject(
            page, "hamdrmCapabilityMessage")->property("text").toString();
        QVERIFY(capability.contains(QStringLiteral("72 named profiles"),
                                    Qt::CaseInsensitive));
        QVERIFY(capability.contains(QStringLiteral("waveform"),
                                    Qt::CaseInsensitive));

        QSignalSpy rejected(&controller,
                            &hamdrm::HamDrmController::operationRejected);
        QVERIFY(!controller.startRx());
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(rejected.front().at(0).toString(), QStringLiteral("rx"));
        QVERIFY(!requiredObject(page, "hamdrmError")
                     ->property("text").toString().isEmpty());

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#03070a")));
        window.resize(1'000, 820);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(1'000.0, 820.0));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTest::qWait(250);
        const QImage rendered = window.grabWindow();
        QVERIFY2(!rendered.isNull(),
                 "Offscreen QQuickWindow produced no HAMDRM frame");
        QCOMPARE(rendered.size(), QSize(1'000, 820));

        QSet<QRgb> sampledColours;
        for (int y = 0; y < rendered.height(); y += 19) {
            for (int x = 0; x < rendered.width(); x += 19) {
                sampledColours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 10,
                 "Rendered HAMDRM page lacks meaningful visual content");
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
    }

    void workspaceLoadsSelectsDigitalPageAndRendersOffscreen()
    {
        QQmlEngine engine;
        QStringList runtimeWarnings;
        connect(&engine,
                &QQmlEngine::warnings,
                this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });

        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_WORKSPACE_QML_SOURCE);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* window = qobject_cast<QQuickWindow*>(object.data());
        QVERIFY(window);

        const QVariantList labels = window->property("pageLabels").toList();
        QCOMPARE(labels.size(), 7);
        QCOMPARE(labels.at(4).toString(), QStringLiteral("Digital HAMDRM"));
        QVERIFY(window->setProperty("pageIndex", 4));
        QCOMPARE(window->property("pageIndex").toInt(), 4);

        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 2'000);
        QTest::qWait(250);
        const QImage rendered = window->grabWindow();
        QVERIFY2(!rendered.isNull(),
                 "Offscreen SSTV workspace produced no frame");
        QCOMPARE(rendered.size(), QSize(1'120, 740));

        QSet<QRgb> sampledColours;
        for (int y = 0; y < rendered.height(); y += 19) {
            for (int x = 0; x < rendered.width(); x += 19) {
                sampledColours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 10,
                 "Rendered SSTV workspace lacks meaningful visual content");
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
    }

    void workspaceCloseCancelsActiveAnalogTxBeforeHiding()
    {
        WorkspaceCloseEngineFixture bridge;
        QQmlEngine engine;
        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_WORKSPACE_QML_SOURCE);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial;
        initial.insert(QStringLiteral("engine"),
                       QVariant::fromValue(static_cast<QObject*>(&bridge)));
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* window = qobject_cast<QQuickWindow*>(object.data());
        QVERIFY(window);

        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 2'000);
        QVERIFY(window->close());
        QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 2'000);
        QCOMPARE(bridge.calls(),
                 QStringList({QStringLiteral("stop-rx"),
                              QStringLiteral("cancel-tx"),
                              QStringLiteral("leave-workspace")}));
        QVERIFY(!bridge.sstvTxActive());
    }
};

QTEST_MAIN(TestSstvDigitalQml)
#include "test_sstv_digital_qml.moc"
