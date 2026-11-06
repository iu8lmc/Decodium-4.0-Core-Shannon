#include "LinuxGraphicsSafety.hpp"
#include "GraphicsPipelineCache.hpp"

#include <QCoreApplication>
#include <QMap>
#include <QTemporaryDir>
#include <QTest>
#include <thread>

using namespace decodium::linux_graphics;

namespace {
struct Candidate {
    quint32 vendorId;
    QByteArray name;
    quint64 localMemoryBytes;
    bool integratedGpu;
    bool usable;
    bool integrated() const { return integratedGpu; }
    bool eligible() const { return usable; }
};

std::vector<Candidate> reportedHardware()
{
    return {{0x8086, "Intel(R) Graphics (RPL-U)", 11760, true, true},
            {0x10de, "NVIDIA GeForce RTX 2050 (NVK GA107)", 4096, false, true},
            {0x10005, "llvmpipe (LLVM 21.1.8, 256 bits)", 15681, false, false}};
}

const QString lostDevice = QStringLiteral(
    "vkDebug: DRM_NOUVEAU_EXEC failed: No such device (VK_ERROR_DEVICE_LOST)");

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}
}

class TestLinuxGraphicsSafety : public QObject {
    Q_OBJECT
    QMap<QByteArray, QByteArray> m_environment;
private slots:
    void init()
    {
        for (const char* name : {"QSG_RHI_BACKEND", "QT_QUICK_BACKEND", "QMLSCENE_DEVICE",
                                 "DECODIUM_GRAPHICS_BACKEND", "QT_VK_PHYSICAL_DEVICE_INDEX",
                                 "DECODIUM_DISABLE_LINUX_GPU_AUTOSELECT", "QT_OPENGL",
                                 "DECODIUM_DISABLE_GPU_PANADAPTER_FFT"}) {
            m_environment[name] = qgetenv(name);
            qunsetenv(name);
        }
    }

    void cleanup()
    {
        for (auto it = m_environment.cbegin(); it != m_environment.cend(); ++it) {
            if (it.value().isNull()) qunsetenv(it.key().constData());
            else qputenv(it.key().constData(), it.value());
        }
        QCoreApplication::instance()->setProperty("decodiumVulkanCacheDeviceKey", QVariant {});
    }

    void reportedNvkIntelLaptopUsesIntel()
    {
        const auto devices = reportedHardware();
        QCOMPARE(automaticAlternativeForNvk(devices, &devices[1], false), &devices[0]);
    }

    void selectionDoesNotDependOnEnumerationOrder()
    {
        auto devices = reportedHardware();
        std::swap(devices[0], devices[1]);
        QCOMPARE(automaticAlternativeForNvk(devices, &devices[0], false), &devices[1]);
    }

    void unusableIntelCannotBeSelected()
    {
        auto devices = reportedHardware();
        devices[0].usable = false; // No presentation/compute queue or swapchain.
        QVERIFY(!automaticAlternativeForNvk(devices, &devices[1], false));
        devices[0].usable = true;
        devices[0].integratedGpu = false;
        QVERIFY(!automaticAlternativeForNvk(devices, &devices[1], false));
    }

    void nvkOnlyUsesSoftwareInsteadOfImplicitOpenGl()
    {
        const std::vector<Candidate> devices {reportedHardware()[1]};
        QVERIFY(!automaticAlternativeForNvk(devices, &devices[0], false));
    }

    void healthyDriversAndExplicitVulkanRemainUnchanged()
    {
        auto devices = reportedHardware();
        QCOMPARE(automaticAlternativeForNvk(devices, &devices[1], true), &devices[1]);
        devices[1].name = "NVIDIA GeForce RTX 2050"; // Proprietary, not NVK.
        QCOMPARE(automaticAlternativeForNvk(devices, &devices[1], false), &devices[1]);
        devices[1].vendorId = 0x1002;
        devices[1].name = "AMD Radeon RX 6600";
        QCOMPARE(automaticAlternativeForNvk(devices, &devices[1], false), &devices[1]);
        QCOMPARE(automaticAlternativeForNvk(devices, &devices[0], false), &devices[0]);
        QVERIFY(!automaticAlternativeForNvk(devices, static_cast<const Candidate*>(nullptr), false));
    }

    void softwareFallbackDisablesVulkanAndCompute()
    {
        qputenv("QSG_RHI_BACKEND", "vulkan");
        qputenv("QT_VK_PHYSICAL_DEVICE_INDEX", "1");
        useSoftwareRendering();
        QVERIFY(!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"));
        QVERIFY(!qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX"));
        QCOMPARE(qgetenv("QT_QUICK_BACKEND"), QByteArray("software"));
        QCOMPARE(qgetenv("QT_OPENGL"), QByteArray("software"));
        QCOMPARE(qgetenv("DECODIUM_DISABLE_GPU_PANADAPTER_FFT"), QByteArray("1"));
    }

    void manualGraphicsChoicesAreRecognised_data()
    {
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<QByteArray>("value");
        QTest::newRow("backend") << QByteArray("QSG_RHI_BACKEND") << QByteArray("opengl");
        QTest::newRow("quick") << QByteArray("QT_QUICK_BACKEND") << QByteArray("software");
        QTest::newRow("legacy-quick") << QByteArray("QMLSCENE_DEVICE") << QByteArray("software");
        QTest::newRow("decodium") << QByteArray("DECODIUM_GRAPHICS_BACKEND") << QByteArray("vulkan");
        QTest::newRow("index-zero") << QByteArray("QT_VK_PHYSICAL_DEVICE_INDEX") << QByteArray("0");
        QTest::newRow("no-autoselect") << QByteArray("DECODIUM_DISABLE_LINUX_GPU_AUTOSELECT") << QByteArray("1");
    }

    void manualGraphicsChoicesAreRecognised()
    {
        QFETCH(QByteArray, name);
        QFETCH(QByteArray, value);
        QVERIFY(!hasExplicitGraphicsChoice());
        qputenv(name.constData(), value);
        QVERIFY(hasExplicitGraphicsChoice());
        qputenv(name.constData(), "");
        QVERIFY(!hasExplicitGraphicsChoice());
    }

    void deviceLossIsPersistentAndResetIsExplicit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("nested/linux-vulkan-device-loss.json");
        DeviceLossRecovery first(path);
        QVERIFY(!first.active());
        QVERIFY(!first.record("VK_ERROR_OUT_OF_DEVICE_MEMORY"));
        QVERIFY(!first.record("Main window closing - shutting down application"));
        QVERIFY(!first.observed());
        QVERIFY(first.record(lostDevice));
        QVERIFY(first.observed());
        QVERIFY(first.persisted());
        const QByteArray bytes = readFile(path);
        QCOMPARE(QJsonDocument::fromJson(bytes).object().value("fallback").toString(), "software");
        QVERIFY(!first.record("Submit failed (VK_ERROR_DEVICE_LOST)"));
        QCOMPARE(readFile(path), bytes);

        DeviceLossRecovery nextLaunch(path);
        QVERIFY(nextLaunch.active());
        QVERIFY(!nextLaunch.observed());
        QVERIFY(nextLaunch.reset());
        QVERIFY(!nextLaunch.active());
        QVERIFY(nextLaunch.reset());
    }

    void deviceLossNotifications_data()
    {
        QTest::addColumn<QString>("message");
        QTest::addColumn<bool>("deviceLost");
        QTest::newRow("NVK-debug") << lostDevice << true;
        // Qt 6.11 emits these independently of the Vulkan driver's optional
        // debug output. The fence/submit/scenegraph messages also occur in
        // issue #101 comment 6402574508 (v1.0.712, 19:05 and 19:09 sessions).
        for (const char* call : {"vkWaitForFences", "vkQueueSubmit", "vkQueuePresentKHR",
                                 "vkAcquireNextImageKHR", "vkAllocateCommandBuffers",
                                 "vkBeginCommandBuffer", "vkEndCommandBuffer"}) {
            QTest::newRow(call) << QStringLiteral("Device loss detected in %1()")
                                      .arg(QLatin1String(call)) << true;
        }
        QTest::newRow("Qt-threaded-renderloop")
            << QStringLiteral("Graphics device lost, cleaning up scenegraph and releasing RHI") << true;
        QTest::newRow("Qt-basic-renderloop")
            << QStringLiteral("Graphics device lost, cleaning up scenegraph and releasing RHIs") << true;
        QTest::newRow("swapchain-resize") << QStringLiteral("VK_ERROR_OUT_OF_DATE_KHR") << false;
        QTest::newRow("memory") << QStringLiteral("VK_ERROR_OUT_OF_DEVICE_MEMORY") << false;
        QTest::newRow("normal-teardown") << QStringLiteral("sceneGraphInvalidated") << false;
        QTest::newRow("sound-device") << QStringLiteral("Audio device lost") << false;
        QTest::newRow("generic-warning") << QStringLiteral("Device loss detected") << false;
        QTest::newRow("counter-error") << QStringLiteral("drm-process-stale") << false;
    }

    void deviceLossNotifications()
    {
        QFETCH(QString, message);
        QFETCH(bool, deviceLost);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("loss.json");
        DeviceLossRecovery recovery(path);
        QCOMPARE(recovery.record(message), deviceLost);
        QCOMPARE(recovery.observed(), deviceLost);
        QCOMPARE(recovery.persisted(), deviceLost);
        QCOMPARE(DeviceLossRecovery(path).active(), deviceLost);
        if (deviceLost) {
            QCOMPARE(QJsonDocument::fromJson(readFile(path)).object().value("reason").toString(), message);
            QVERIFY(!recovery.record(message)); // No repeated shutdown requests.
        }
    }

    void qtOnlyLossSequenceRequestsRecoveryOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("loss.json");
        DeviceLossRecovery recovery(path);
        int requests = 0;
        // Replay the repeated device-loss portion without vkDebug messages.
        // A build/driver that omits debug output must still arm safe startup.
        for (int attempt = 0; attempt < 4; ++attempt) {
            for (const QString& message : {
                     QStringLiteral("Device loss detected in vkQueueSubmit()"),
                     QStringLiteral("Graphics device lost, cleaning up scenegraph and releasing RHI")}) {
                if (recovery.record(message)) ++requests;
            }
        }
        QCOMPARE(requests, 1);
        DeviceLossRecovery nextLaunch(path);
        QVERIFY(nextLaunch.active());
        QVERIFY(!hasExplicitGraphicsChoice());
        useSoftwareRendering();
        QCOMPARE(qgetenv("QT_QUICK_BACKEND"), QByteArray("software"));
        QVERIFY(!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"));
    }

    void renderThreadFailureIsRecordedOnlyOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DeviceLossRecovery recovery(dir.filePath("loss.json"));
        std::atomic_int firstReports {0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 12; ++i)
            threads.emplace_back([&] { if (recovery.record(lostDevice)) ++firstReports; });
        for (auto& thread : threads) thread.join();
        QCOMPARE(firstReports.load(), 1);
        QVERIFY(recovery.persisted());
        QVERIFY(recovery.active());
    }

    void failedMarkerWriteStillRequestsShutdown()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile notDirectory(dir.filePath("file"));
        QVERIFY(notDirectory.open(QIODevice::WriteOnly));
        notDirectory.close();
        DeviceLossRecovery recovery(dir.filePath("file/loss.json"));
        QVERIFY(recovery.record(lostDevice));
        QVERIFY(recovery.observed());
        QVERIFY(!recovery.persisted());
        QVERIFY(!recovery.record(lostDevice));
    }

    void vulkanCachesAreSeparatedByDeviceAndDriver()
    {
        qputenv("QSG_RHI_BACKEND", "vulkan");
        auto* app = QCoreApplication::instance();
        app->setProperty("decodiumVulkanCacheDeviceKey", QByteArray("10de-ga107-driverA-uuidA"));
        const QString nvidia = decodium::graphics_cache::graphicsBackendToken();
        app->setProperty("decodiumVulkanCacheDeviceKey", QByteArray("8086-rplu-driverA-uuidB"));
        const QString intel = decodium::graphics_cache::graphicsBackendToken();
        QVERIFY(nvidia != intel);
        QVERIFY(intel != "vulkan"); // Do not load the old shared Vulkan cache.
        app->setProperty("decodiumVulkanCacheDeviceKey", QByteArray("8086-rplu-driverB-uuidC"));
        QVERIFY(decodium::graphics_cache::graphicsBackendToken() != intel);
        qputenv("QSG_RHI_BACKEND", "metal");
        QCOMPARE(decodium::graphics_cache::graphicsBackendToken(), QString("metal"));
        qputenv("QSG_RHI_BACKEND", "d3d11");
        QVERIFY(decodium::graphics_cache::graphicsBackendToken().startsWith("d3d11"));
    }

    void deviceLossDuringTeardownDoesNotPromoteCache_data()
    {
        QTest::addColumn<QString>("notification");
        QTest::newRow("driver") << lostDevice;
        QTest::newRow("Qt-only")
            << QStringLiteral("Graphics device lost, cleaning up scenegraph and releasing RHI");
    }

    void deviceLossDuringTeardownDoesNotPromoteCache()
    {
        QFETCH(QString, notification);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DeviceLossRecovery recovery(dir.filePath("loss.json"));
        QString marker, stage, stable;
        {
            decodium::graphics_cache::PipelineCacheSession session(
                dir.path(), false, [&] { return !recovery.observed(); });
            marker = session.markerFile();
            stable = decodium::graphics_cache::stableCacheFile(dir.path(), "main");
            stage = decodium::graphics_cache::stagingCacheFile(stable);
            QFile file(stage);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("failed-driver-cache") > 0);
            file.close();
            session.markCleanShutdown(); // app.exec() returned before teardown.
            QVERIFY(recovery.record(notification));
        }
        QVERIFY(QFileInfo::exists(marker));
        QVERIFY(!QFileInfo::exists(stable));
        QVERIFY(QFileInfo::exists(stage));
        decodium::graphics_cache::PipelineCacheSession next(dir.path());
        QVERIFY(next.recoveredFromInterruptedSession());
        QVERIFY(!QFileInfo::exists(stage));
        next.markCleanShutdown();
    }

    void recoveryWiringRunsBeforeGraphicsAndStopsRadioBeforeDisconnect()
    {
        const QString source = QString::fromUtf8(readFile(
            QStringLiteral(DECODIUM_SOURCE_DIR "/src/app/main_qml.cpp")));
        QVERIFY(!source.isEmpty());
        const int recover = source.indexOf("if (g_linuxGraphicsRecovery->active())");
        const int app = source.indexOf("DecodiumApplication app(argc, argv)");
        const int select = source.indexOf("configureLinuxVulkanGpuSelection();", app);
        QVERIFY(recover >= 0 && recover < app && app < select);
        const int handler = source.indexOf("g_linuxGraphicsRecovery->record(msg)");
        const int queue = source.indexOf("QMetaObject::invokeMethod(application", handler);
        const int exit = source.indexOf("QCoreApplication::exit(1)", queue);
        QVERIFY(handler >= 0 && handler < queue && queue < exit);
        QVERIFY(source.contains("if (!g_linuxGraphicsRecovery->reset())"));
        const int shutdown = source.indexOf("Device-loss shutdown bypasses");
        const int cw = source.indexOf("cwModule.suspend();", shutdown);
        const int ssb = source.indexOf("ssbModule.stop();", cw);
        const int disconnect = source.indexOf("bridge.shutdown();", ssb);
        QVERIFY(shutdown >= 0 && shutdown < cw && cw < ssb && ssb < disconnect);
        QVERIFY(source.contains("return !g_linuxGraphicsRecovery->observed();"));
        QVERIFY(source.contains("properties.pipelineCacheUUID"));
    }
};

QTEST_GUILESS_MAIN(TestLinuxGraphicsSafety)
#include "test_linux_graphics_safety.moc"
