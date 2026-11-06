#include "GraphicsPipelineCache.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool writeFile(const QString& path, const QByteArray& payload)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(payload) == payload.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestGraphicsPipelineCache final : public QObject
{
    Q_OBJECT

private slots:
    void cacheNamesInvalidateTheLegacyFormatAndSeparateRoles();
    void cacheNamesSeparateHardwareD3d11FromWarp();
    void interruptedSessionDiscardsPersistentAndStagedCaches();
    void cleanSessionPromotesTheStageAtomically();
    void softwareRendererRecoverySwitchWorksOutsideWindows();
    void windowsRuntimeDriverCrashRecoveryIsPersistent();
    void windowsD3d11StallFallbackDoesNotRetryGpu();
};

void TestGraphicsPipelineCache::cacheNamesInvalidateTheLegacyFormatAndSeparateRoles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString const bootstrap = decodium::graphics_cache::stableCacheFile(
        directory.path(), QStringLiteral("bootstrap"));
    QString const main = decodium::graphics_cache::stableCacheFile(
        directory.path(), QStringLiteral("main"));

    QVERIFY(bootstrap.contains(QStringLiteral("qsg_pipeline_cache_v2_")));
    QVERIFY(bootstrap.endsWith(QStringLiteral(".bin")));
    QVERIFY(bootstrap != main);
    QVERIFY(decodium::graphics_cache::stagingCacheFile(bootstrap)
                .startsWith(bootstrap + QStringLiteral(".stage.")));
}

void TestGraphicsPipelineCache::cacheNamesSeparateHardwareD3d11FromWarp()
{
    QByteArray const oldRhi = qgetenv("QSG_RHI_BACKEND");
    QByteArray const oldSoftwarePreference =
        qgetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
    bool const hadRhi = qEnvironmentVariableIsSet("QSG_RHI_BACKEND");
    bool const hadSoftwarePreference =
        qEnvironmentVariableIsSet("QSG_RHI_PREFER_SOFTWARE_RENDERER");

    qputenv("QSG_RHI_BACKEND", "d3d11");
    qunsetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
    QString const hardware = decodium::graphics_cache::graphicsBackendToken();
    qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", "1");
    QString const warp = decodium::graphics_cache::graphicsBackendToken();

    QCOMPARE(hardware, QStringLiteral("d3d11"));
    QCOMPARE(warp, QStringLiteral("d3d11_warp"));
    QVERIFY(hardware != warp);

    hadRhi ? qputenv("QSG_RHI_BACKEND", oldRhi)
           : qunsetenv("QSG_RHI_BACKEND");
    hadSoftwarePreference
        ? qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", oldSoftwarePreference)
        : qunsetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
}

void TestGraphicsPipelineCache::interruptedSessionDiscardsPersistentAndStagedCaches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString stable;
    QString staged;

    {
        decodium::graphics_cache::PipelineCacheSession interrupted(directory.path());
        QVERIFY(interrupted.markerArmed());
        stable = decodium::graphics_cache::stableCacheFile(
            directory.path(), QStringLiteral("main"));
        staged = decodium::graphics_cache::stagingCacheFile(stable);
        QVERIFY(writeFile(stable, QByteArrayLiteral("last-known-cache")));
        QVERIFY(writeFile(staged, QByteArrayLiteral("incomplete-cache")));
        // Deliberately do not mark this session clean, as if the compositor or
        // process had frozen during scene-graph teardown.
    }

    QVERIFY(QFileInfo::exists(stable));
    QVERIFY(QFileInfo::exists(staged));
    {
        decodium::graphics_cache::PipelineCacheSession recovered(directory.path());
        QVERIFY(recovered.recoveredFromInterruptedSession());
        QVERIFY(recovered.removedFileCount() >= 2);
        QVERIFY(!decodium::graphics_cache::pipelineCacheLoadAllowed());
        QVERIFY(!QFileInfo::exists(stable));
        QVERIFY(!QFileInfo::exists(staged));
        recovered.markCleanShutdown();
    }
}

void TestGraphicsPipelineCache::cleanSessionPromotesTheStageAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString stable;
    QString staged;

    {
        decodium::graphics_cache::PipelineCacheSession session(directory.path());
        QVERIFY(session.markerArmed());
        stable = decodium::graphics_cache::stableCacheFile(
            directory.path(), QStringLiteral("main"));
        staged = decodium::graphics_cache::stagingCacheFile(stable);
        QVERIFY(writeFile(stable, QByteArrayLiteral("old-cache")));
        QVERIFY(writeFile(staged, QByteArrayLiteral("new-cache")));
        session.markCleanShutdown();
    }

    QCOMPARE(readFile(stable), QByteArrayLiteral("new-cache"));
    QVERIFY(!QFileInfo::exists(staged));
    QDir const directoryView(directory.path());
    QVERIFY(directoryView.entryList(
                QStringList {decodium::graphics_cache::cachePrefix()
                             + QStringLiteral("*session.pending")},
                QDir::Files).isEmpty());
}

void TestGraphicsPipelineCache::softwareRendererRecoverySwitchWorksOutsideWindows()
{
    QString const source = readSource(QStringLiteral("src/app/main_qml.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral(
        "hasCommandLineSwitchAny(argc, argv, \"--disable-gpu\")")));
    QVERIFY(source.contains(QStringLiteral(
        "hasCommandLineSwitchAny(argc, argv, \"--software-renderer\")")));
    QVERIFY(source.contains(QStringLiteral(
        "qputenv(\"QT_QUICK_BACKEND\", \"software\")")));

    int const switchPosition = source.indexOf(QStringLiteral(
        "bool const commandLineSoftwareGraphics"));
    int const applicationPosition = source.indexOf(QStringLiteral(
        "DecodiumApplication app(argc, argv)"));
    QVERIFY(switchPosition >= 0);
    QVERIFY(applicationPosition > switchPosition);
}

void TestGraphicsPipelineCache::windowsRuntimeDriverCrashRecoveryIsPersistent()
{
    QString const startup = readSource(QStringLiteral("src/app/main_qml.cpp"));
    QString const logging = readSource(QStringLiteral("src/bridge/DecodiumLogging.cpp"));
    QVERIFY(!startup.isEmpty());
    QVERIFY(!logging.isEmpty());

    QVERIFY(logging.contains(QStringLiteral(
        "module.startsWith(QStringLiteral(\"igd\"))")));
    QVERIFY(logging.contains(QStringLiteral("decodium-gpu-runtime-crash.flag")));
    QVERIFY(logging.contains(QStringLiteral("writeWindowsRuntimeGraphicsCrashFlag")));
    QVERIFY(startup.contains(QStringLiteral("graphicsRecoveryFromRuntimeCrash")));
    QVERIFY(startup.contains(QStringLiteral("fallbackAfter(crashedBackend)")));
    QVERIFY(startup.contains(QStringLiteral(
        "Automatic recovery after a runtime graphics-driver crash")));
    QVERIFY(startup.contains(QStringLiteral(
        "qputenv(\"DECODIUM_DISABLE_GPU_PANADAPTER_FFT\", \"1\")")));
}

void TestGraphicsPipelineCache::windowsD3d11StallFallbackDoesNotRetryGpu()
{
    QString const bridge = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!bridge.isEmpty());

    int const functionStart = bridge.indexOf(QStringLiteral(
        "void DecodiumBridge::noteMainThreadMicroStall(qint64 deltaMs)"));
    int const functionEnd = bridge.indexOf(QStringLiteral(
        "void DecodiumBridge::noteFt8AdaptivePeriod(qint64 periodId)"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);
    QString const body = bridge.mid(functionStart, functionEnd - functionStart);

    int const d3dGuard = body.indexOf(QStringLiteral("bool const hardwareD3d11"));
    int const disableForce = body.indexOf(
        QStringLiteral("m_forceGpuPanadapterFft.exchange(false)"), d3dGuard);
    int const suppressRetry = body.indexOf(
        QStringLiteral("m_gpuPanadapterRetryNotBeforeMs = 0"), disableForce);
    int const ft8Only = body.indexOf(
        QStringLiteral("if (m_mode != QStringLiteral(\"FT8\"))"), suppressRetry);
    QVERIFY(d3dGuard >= 0);
    QVERIFY(disableForce > d3dGuard);
    QVERIFY(suppressRetry > disableForce);
    QVERIFY(ft8Only > suppressRetry);
}

QTEST_MAIN(TestGraphicsPipelineCache)
#include "test_graphics_pipeline_cache.moc"
