// main_qml.cpp - QML entry point for Decodium3

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QMetaType>
#include <QStyleFactory>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QIODevice>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QLockFile>
#include <QList>
#include <QLocale>
#include <QWindow>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <clocale>
#include <cstring>
#include <mutex>
#include <thread>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "DecodiumBridge.h"
#include "DecodiumDiagnostics.h"
#include "DecodiumDxCluster.h"
#include "DecodiumLogging.hpp"
#include "L10nLoader.hpp"
#include "MetaDataRegistry.hpp"
#include "Radio.hpp"
#include "Configuration.hpp"
#include "WFPalette.hpp"
#include "models/FrequencyList.hpp"
#include "models/StationList.hpp"
#include "models/IARURegions.hpp"
#include "models/DecodeHighlightingModel.hpp"
#include "Transceiver/TransceiverFactory.hpp"
#include "WaterfallItem.hpp"
#include "PanadapterItem.hpp"
#include "WorldMapItem.hpp"
#include "lib/init_random_seed.h"

static void L(const char* msg) {
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/decodium-start.log";
    FILE* f = fopen(logPath.toLocal8Bit().constData(), "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
    std::fprintf(stderr, "%s\n", msg);
    DIAG_INFO(QString::fromLocal8Bit(msg));
}

static std::atomic_bool g_shuttingDown {false};

static void writeStartupLogLine(const QByteArray& logPath, const QByteArray& msg)
{
    FILE* f = fopen(logPath.constData(), "a");
    if (f) {
        fputs(msg.constData(), f);
        fputc('\n', f);
        fclose(f);
    }
    std::fprintf(stderr, "%s\n", msg.constData());
}

static void logEnvVar(const char* name)
{
    QByteArray line(name);
    line += "=";
    if (qEnvironmentVariableIsSet(name)) {
        line += qgetenv(name);
    } else {
        line += "<unset>";
    }
    L(line.constData());
}

#ifdef Q_OS_WIN
static bool hasCommandLineSwitch(int argc, char* argv[], const char* name)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String(name))
            return true;
    }
    return false;
}

static QString slowQmlStartupFlagPath()
{
    return QDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)}
        .absoluteFilePath(QStringLiteral("decodium-slow-qml-startup.flag"));
}

static void writeSlowQmlStartupFlag(const QByteArray& flagPath, const QByteArray& reason)
{
    QFile flagFile {QString::fromLocal8Bit(flagPath)};
    if (!flagFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    flagFile.write(reason);
    flagFile.write("\n");
}

static QString sanitizedCacheComponent(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
    value = value.trimmed();
    return value.isEmpty() ? QStringLiteral("default") : value;
}

static QString windowsQmlDiskCachePath(const QString& configName)
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QByteArray cacheSeed;
    cacheSeed += QDir::cleanPath(QCoreApplication::applicationDirPath()).toUtf8();
    cacheSeed += '\n';
    cacheSeed += QByteArray(FORK_RELEASE_VERSION);
    cacheSeed += '\n';
    cacheSeed += qVersion();
    cacheSeed += '\n';
    cacheSeed += QSysInfo::buildAbi().toUtf8();
    if (!configName.isEmpty()) {
        cacheSeed += '\n';
        cacheSeed += configName.toUtf8();
    }

    QString const installHash = QString::fromLatin1(
        QCryptographicHash::hash(cacheSeed, QCryptographicHash::Sha256).toHex().left(16));
    QString const versionComponent = sanitizedCacheComponent(QStringLiteral(FORK_RELEASE_VERSION));
    return QDir {basePath}.absoluteFilePath(
        QStringLiteral("qmlcache/%1/%2").arg(versionComponent, installHash));
}

static void configureWindowsQmlDiskCache(const QString& configName)
{
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_QML_CACHE")
        || qEnvironmentVariableIsSet("QML_DISABLE_DISK_CACHE")) {
        return;
    }

    if (qEnvironmentVariableIsSet("QML_DISK_CACHE_PATH")) {
        QByteArray msg("QML disk cache path from environment: ");
        msg += qgetenv("QML_DISK_CACHE_PATH");
        L(msg.constData());
        return;
    }

    QString const cachePath = windowsQmlDiskCachePath(configName);
    if (!QDir().mkpath(cachePath)) {
        L(("QML disk cache path could not be created: " + cachePath.toLocal8Bit()).constData());
        return;
    }

    qputenv("QML_DISK_CACHE_PATH", QDir::toNativeSeparators(cachePath).toLocal8Bit());
    L(("QML disk cache path isolated: " + cachePath.toLocal8Bit()).constData());
}
#endif

static QIcon loadDecodiumApplicationIcon()
{
    QIcon icon(QStringLiteral(":/icon_128x128.png"));

#ifdef Q_OS_WIN
    if (icon.isNull()) {
        QStringList const candidates {
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("decodium.ico")),
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("icons/windows-icons/decodium.ico"))
        };
        for (QString const& candidate : candidates) {
            if (!QFileInfo::exists(candidate))
                continue;
            icon = QIcon(candidate);
            if (!icon.isNull())
                break;
        }
    }
#endif

    return icon;
}

#ifdef Q_OS_WIN
static void applyApplicationIconToTopLevelWindows(QIcon const& icon)
{
    if (icon.isNull())
        return;

    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (window)
            window->setIcon(icon);
    }
}

static void setWindowsAppUserModelId()
{
    using SetAppIdFn = HRESULT (WINAPI *)(PCWSTR);
    HMODULE shell32 = LoadLibraryW(L"shell32.dll");
    if (!shell32)
        return;

    FARPROC proc = GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    if (proc) {
        SetAppIdFn setAppId = nullptr;
        static_assert(sizeof(setAppId) == sizeof(proc),
                      "Unexpected function pointer size");
        std::memcpy(&setAppId, &proc, sizeof(setAppId));
        setAppId(L"IU8LMC.Decodium4");
    }

    FreeLibrary(shell32);
}

static void scheduleWindowsTaskbarIconRefresh(QObject *context, QIcon const& icon)
{
    if (!context || icon.isNull())
        return;

    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, context,
                     [icon] (QWindow *) {
                         applyApplicationIconToTopLevelWindows(icon);
                     });

    for (int delayMs : {0, 250, 750, 1500, 3000}) {
        QTimer::singleShot(delayMs, context, [icon] {
            applyApplicationIconToTopLevelWindows(icon);
        });
    }
}
#endif

static bool isIgnorableShutdownQmlMessage(const QString& msg)
{
    return msg.contains(".qml:");
}

static void logQmlWarnings(const QList<QQmlError>& warnings)
{
    for (const QQmlError& warning : warnings) {
        const QString warningText = warning.toString();
        if (g_shuttingDown.load(std::memory_order_relaxed)
            && isIgnorableShutdownQmlMessage(warningText)) {
            continue;
        }
        L(warningText.toLocal8Bit().constData());
    }
}

static void qtMsgHandler(QtMsgType, const QMessageLogContext&, const QString& msg) {
    if (msg.contains("Main window closing - shutting down application")) {
        g_shuttingDown.store(true, std::memory_order_relaxed);
    }
    if (g_shuttingDown.load(std::memory_order_relaxed) && isIgnorableShutdownQmlMessage(msg))
        return;
    L(msg.toLocal8Bit().constData());
}

static void registerLegacySettingsStreamTypes()
{
    qRegisterMetaType<FrequencyList_v2_101::Item>("Item_v2_101");
    qRegisterMetaType<FrequencyList_v2_101::FrequencyItems>("FrequencyItems_v2_101");
    qRegisterMetaType<FrequencyList_v2::Item>("Item_v2");
    qRegisterMetaType<FrequencyList_v2::FrequencyItems>("FrequencyItems_v2");
    qRegisterMetaType<FrequencyList::Item>("Item");
    qRegisterMetaType<FrequencyList::FrequencyItems>("FrequencyItems");
    qRegisterMetaType<Configuration::DataMode>("Configuration::DataMode");
    qRegisterMetaType<Configuration::Type2MsgGen>("Configuration::Type2MsgGen");
    qRegisterMetaType<StationList::Station>("Station");
    qRegisterMetaType<StationList::Stations>("Stations");
    qRegisterMetaType<TransceiverFactory::DataBits>("TransceiverFactory::DataBits");
    qRegisterMetaType<TransceiverFactory::StopBits>("TransceiverFactory::StopBits");
    qRegisterMetaType<TransceiverFactory::Handshake>("TransceiverFactory::Handshake");
    qRegisterMetaType<TransceiverFactory::PTTMethod>("TransceiverFactory::PTTMethod");
    qRegisterMetaType<TransceiverFactory::TXAudioSource>("TransceiverFactory::TXAudioSource");
    qRegisterMetaType<TransceiverFactory::SplitMode>("TransceiverFactory::SplitMode");
    qRegisterMetaType<WFPalette::Colours>("Colours");
    qRegisterMetaType<IARURegions::Region>("IARURegions::Region");
    qRegisterMetaType<DecodeHighlightingModel::HighlightInfo>("HighlightInfo");
    qRegisterMetaType<DecodeHighlightingModel::HighlightItems>("HighlightItems");
}

int main(int argc, char* argv[])
{
    qInstallMessageHandler(qtMsgHandler);
    L("main() START");

    init_random_seed();
    Radio::register_types();
    register_types();
    registerLegacySettingsStreamTypes();
    L("legacy metatypes OK");

    QQuickStyle::setStyle("Material");
    L("QQuickStyle OK");

#ifdef Q_OS_WIN
    QString const slowQmlStartupFlag = slowQmlStartupFlagPath();
    bool const envSafeGraphics = qEnvironmentVariableIsSet("DECODIUM_SAFE_GRAPHICS");
    bool const commandLineSafeGraphics =
        hasCommandLineSwitch(argc, argv, "--safe-graphics")
        || hasCommandLineSwitch(argc, argv, "--disable-gpu")
        || hasCommandLineSwitch(argc, argv, "--software-renderer");
    bool const autoSafeGraphics = QFile::exists(slowQmlStartupFlag);
    bool const safeGraphicsRequested =
        envSafeGraphics || commandLineSafeGraphics || autoSafeGraphics;
    if (safeGraphicsRequested) {
        qputenv("QSG_RHI_BACKEND", "d3d11");
        qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", "1");
        if (autoSafeGraphics && !envSafeGraphics && !commandLineSafeGraphics) {
            L(("Qt Quick safe graphics auto-enabled after previous slow QML startup: "
               + slowQmlStartupFlag.toLocal8Bit()).constData());
        } else {
            L("Qt Quick safe graphics enabled: D3D11 WARP software renderer");
        }
    } else if (qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QByteArray backendMessage("Qt Quick graphics backend from environment: ");
        backendMessage += qgetenv("QSG_RHI_BACKEND");
        L(backendMessage.constData());
    } else {
        qputenv("QSG_RHI_BACKEND", "d3d11");
        L("Qt Quick graphics backend defaulted to D3D11");
    }
    setWindowsAppUserModelId();
#endif

    QApplication app(argc, argv);
    DecodiumLogging::installCrashHandler();
    L("QApplication OK");
    L((QByteArray("Qt version: ") + qVersion()).constData());
    L((QByteArray("OS: ") + QSysInfo::prettyProductName().toLocal8Bit()
       + " ABI=" + QSysInfo::buildAbi().toLocal8Bit()
       + " CPU=" + QSysInfo::currentCpuArchitecture().toLocal8Bit()).constData());
    logEnvVar("QSG_RHI_BACKEND");
    logEnvVar("QSG_RHI_PREFER_SOFTWARE_RENDERER");
    logEnvVar("QT_OPENGL");
    logEnvVar("QT_QUICK_BACKEND");
    logEnvVar("QML_DISABLE_DISK_CACHE");
    logEnvVar("QML_DISK_CACHE_PATH");
    logEnvVar("DECODIUM_SAFE_GRAPHICS");
    logEnvVar("DECODIUM_DISABLE_QML_CACHE");
    QIcon const appIcon = loadDecodiumApplicationIcon();
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
        L("application icon OK");
    } else {
        L("WARNING: application icon is null");
    }

    QString const fixedFontFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    if (!fixedFontFamily.isEmpty()) {
        QFont::insertSubstitution(QStringLiteral("Consolas"), fixedFontFamily);
    }

    // Forza locale C per numeri (punto decimale) — evita problemi con locale FR/DE/IT
    // che usano la virgola e bloccano il parsing di frequenze/configurazioni
    QLocale::setDefault(QLocale::c());
    setlocale(LC_NUMERIC, "C");

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setApplicationName("Decodium");
    app.setApplicationVersion(QStringLiteral(FORK_RELEASE_VERSION));
    app.setOrganizationName("IU8LMC");
    app.setOrganizationDomain("decodium.iu8lmc.it");
#ifdef Q_OS_WIN
    QGuiApplication::setDesktopFileName(QStringLiteral("IU8LMC.Decodium4"));
    scheduleWindowsTaskbarIconRefresh(&app, appIcon);
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("\nDecodium 4.0 Core Shannon: Digital Modes for Weak Signal Communications in Amateur Radio"));
    auto const helpOption = parser.addHelpOption();
    auto const versionOption = parser.addVersionOption();

    QCommandLineOption const rigOption(QStringList {} << "r" << "rig-name",
                                       QStringLiteral("Where <rig-name> is for multi-instance support."),
                                       QStringLiteral("rig-name"));
    QCommandLineOption const configOption(QStringList {} << "c" << "config",
                                          QStringLiteral("Where <configuration> is an existing one."),
                                          QStringLiteral("configuration"));
    QCommandLineOption const languageOption(QStringList {} << "l" << "language",
                                            QStringLiteral("Where <language> is <lang-code>[-<country-code>]."),
                                            QStringLiteral("language"));
    QCommandLineOption const testOption(QStringList {} << "test-mode",
                                        QStringLiteral("Writable files in test location. Use with caution, for testing only."));
    QCommandLineOption const safeGraphicsOption(
        QStringList {} << "safe-graphics" << "disable-gpu" << "software-renderer",
        QStringLiteral("Use the Windows software graphics renderer for Qt Quick startup troubleshooting."));
    parser.addOption(rigOption);
    parser.addOption(configOption);
    parser.addOption(languageOption);
    parser.addOption(testOption);
    parser.addOption(safeGraphicsOption);

    if (!parser.parse(app.arguments())) {
        L(("Command line error: " + parser.errorText()).toLocal8Bit().constData());
        return -1;
    }
    if (parser.isSet(helpOption)) {
        parser.showHelp(0);
    }
    if (parser.isSet(versionOption)) {
        parser.showVersion();
    }

    QStandardPaths::setTestModeEnabled(parser.isSet(testOption));

    QString const rigName = parser.value(rigOption).trimmed();
    if (!rigName.isEmpty()) {
        if (rigName.contains(QRegularExpression(QStringLiteral(R"([\\/,])")))) {
            L("Invalid rig name - \\ & / not allowed");
            return -1;
        }
        app.setApplicationName(app.applicationName() + QStringLiteral(" - ") + rigName);
    }
    if (parser.isSet(testOption)) {
        app.setApplicationName(app.applicationName() + QStringLiteral(" - test"));
    }

    QString configName = parser.value(configOption).trimmed();
    QSettings rootSettings(QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    if (configName.isEmpty()) {
        configName = rootSettings.value(QStringLiteral("CurrentMultiSettingsConfiguration")).toString().trimmed();
    }
    if (configName.isEmpty() && !rigName.isEmpty()) {
        configName = rigName;
    }
    if (!configName.isEmpty()) {
        app.setProperty("decodiumConfigName", configName);
        rootSettings.setValue(QStringLiteral("CurrentMultiSettingsConfiguration"), configName);
        rootSettings.sync();
    }
#ifdef Q_OS_WIN
    configureWindowsQmlDiskCache(configName);
#endif
    QString languageOverride = parser.value(languageOption).trimmed();
    if (languageOverride.isEmpty()) {
        languageOverride = rootSettings.value(QStringLiteral("UILanguage")).toString().trimmed();
    }
    if (languageOverride.startsWith(QStringLiteral("en_"), Qt::CaseInsensitive)
        || languageOverride.startsWith(QStringLiteral("en-"), Qt::CaseInsensitive)) {
        languageOverride = QStringLiteral("en");
        rootSettings.setValue(QStringLiteral("UILanguage"), languageOverride);
        rootSettings.sync();
    }
    if (!languageOverride.isEmpty()) {
        app.setProperty("decodiumLanguageOverride", languageOverride);
    }
    QLocale const uiLocale;
    L10nLoader l10n {&app, uiLocale, languageOverride};

    // Single-instance detection: prevent multiple QML instances from running
    QDir tempDir{QStandardPaths::writableLocation(QStandardPaths::TempLocation)};
    QLockFile instanceLock{tempDir.absoluteFilePath(app.applicationName() + QStringLiteral("_qml.lock"))};
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(500)) {
        L("Another instance is already running - exiting");
        return -1;
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        if (auto *instance = QCoreApplication::instance()) {
            instance->setProperty("decodiumShuttingDown", true);
        }
        g_shuttingDown.store(true, std::memory_order_relaxed);
    });

    DecodiumBridge bridge;
    if (DecodiumLogging::instance())
        DecodiumLogging::instance()->logStartupDiagnostics();
    L("bridge OK");

    // Keep the bridge alive longer than the QML engine. During shutdown QML
    // bindings still reevaluate while root objects are being torn down.
    // If the context object dies first, QML sees bridge == null and floods the
    // terminal with TypeError messages on exit.
    // Keep QML disk cache enabled by default. Disabling it forces every launch
    // to recompile QML/JS and is expensive on Windows when antivirus scans the
    // installed Qt/QML tree. Use DECODIUM_DISABLE_QML_CACHE=1 only for diagnosis.
    if (qEnvironmentVariableIsSet("DECODIUM_DISABLE_QML_CACHE")) {
        qputenv("QML_DISABLE_DISK_CACHE", "1");
        L("QML disk cache disabled by DECODIUM_DISABLE_QML_CACHE");
    } else if (qEnvironmentVariableIsSet("QML_DISABLE_DISK_CACHE")) {
        L("QML disk cache disabled by environment");
    } else {
        L("QML disk cache enabled");
    }
    logEnvVar("QML_DISK_CACHE_PATH");
    L(("Qt CacheLocation: " + QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toLocal8Bit()).constData());
    L(("Qt GenericCacheLocation: " + QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation).toLocal8Bit()).constData());
    L(("Qt AppDataLocation: " + QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit()).constData());
    L(("Qt AppLocalDataLocation: " + QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toLocal8Bit()).constData());
    L(("Qt TempLocation: " + QStandardPaths::writableLocation(QStandardPaths::TempLocation).toLocal8Bit()).constData());

    QQmlApplicationEngine engine;
    L("engine OK");
    engine.setOutputWarningsToStandardError(true);  // show QML errors on stderr for debugging
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [&bridge] (const QList<QQmlError>& warnings) {
        logQmlWarnings(warnings);
        for (auto const& w : warnings) {
            qWarning("QML WARNING: %s", qPrintable(w.toString()));
            // Feed warnings to the in-app diagnostics system
            if (auto *diag = qobject_cast<DecodiumDiagnostics*>(bridge.diagnostics()))
                diag->addQmlWarning(w.toString());
        }
    });
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    for (const QString& importPath : engine.importPathList()) {
        L(("QML import path: " + importPath.toLocal8Bit()).constData());
    }

    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.rootContext()->setContextProperty("appEngine", &bridge);

    // Load BootLoader first so the process shows a real window before the
    // heavy QML tree is compiled. This also lets Windows users recover from
    // driver-specific Qt Quick stalls instead of waiting on a silent startup.
    QDir const qmlDir {QDir(QCoreApplication::applicationDirPath())
                           .absoluteFilePath(QStringLiteral("qml/decodium"))};

    QString qmlPath = qmlDir.absoluteFilePath(QStringLiteral("BootLoader.qml"));

    // Fallback to Main.qml if BootLoader doesn't exist (portable/dev builds)
    if (!QFile::exists(qmlPath)) {
        qmlPath = qmlDir.absoluteFilePath(QStringLiteral("Main.qml"));
    }

    if (!QFile::exists(qmlPath)) {
        L("QML file NOT FOUND");
        return -1;
    }
    L(("qmlPath=" + qmlPath.toLocal8Bit()).constData());
    auto logQmlFileInfo = [] (const QString& label, const QString& path) {
        QFileInfo const info(path);
        QByteArray line("QML file ");
        line += label.toLocal8Bit();
        line += ": exists=";
        line += info.exists() ? "1" : "0";
        line += " size=";
        line += QByteArray::number(info.exists() ? info.size() : -1);
        line += " path=";
        line += QDir::toNativeSeparators(path).toLocal8Bit();
        L(line.constData());
    };
    logQmlFileInfo(QStringLiteral("BootLoader"), qmlDir.absoluteFilePath(QStringLiteral("BootLoader.qml")));
    logQmlFileInfo(QStringLiteral("Main"), qmlDir.absoluteFilePath(QStringLiteral("Main.qml")));
    logQmlFileInfo(QStringLiteral("SettingsDialog"), qmlDir.absoluteFilePath(QStringLiteral("components/SettingsDialog.qml")));

    qmlRegisterType<WaterfallItem>("Decodium", 1, 0, "WaterfallItem");
    qmlRegisterType<PanadapterItem>("Decodium", 1, 0, "PanadapterItem");
    qmlRegisterType<WorldMapItem>("Decodium", 1, 0, "WorldMapItem");
    // Registra DecodiumDxCluster come tipo QML non-creabile (accessibile solo come proprietà di bridge)
    qmlRegisterUncreatableType<DecodiumDxCluster>("Decodium", 1, 0, "DecodiumDxCluster",
        "DecodiumDxCluster is created by DecodiumBridge");

    // Log any QML component creation failures
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [](QObject *obj, const QUrl &url) {
        if (!obj) {
            qCritical("QML FAILED to create object from: %s", qPrintable(url.toString()));
        } else {
            qDebug("QML object created OK from: %s", qPrintable(url.toString()));
        }
    });

    L("loading QML...");
    // Watchdog: log if QML loading takes too long (helps diagnose hangs)
    QElapsedTimer loadTimer;
    loadTimer.start();
    std::atomic_bool qmlLoadDone {false};
    std::mutex qmlLoadWatchdogMutex;
    std::condition_variable qmlLoadWatchdogCv;
    QByteArray const startupLogPath =
        (QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         + QStringLiteral("/decodium-start.log")).toLocal8Bit();
    QByteArray const watchedQmlPath = qmlPath.toLocal8Bit();
    QByteArray slowQmlStartupFlagBytes;
#ifdef Q_OS_WIN
    slowQmlStartupFlagBytes = slowQmlStartupFlag.toLocal8Bit();
#endif
    std::thread qmlLoadWatchdog([&qmlLoadDone, &qmlLoadWatchdogMutex, &qmlLoadWatchdogCv,
                                  startupLogPath, watchedQmlPath,
                                  slowQmlStartupFlagBytes]() {
        std::unique_lock<std::mutex> lock(qmlLoadWatchdogMutex);
        for (int elapsedSeconds = 10; elapsedSeconds <= 900; elapsedSeconds += 10) {
            if (qmlLoadWatchdogCv.wait_for(lock, std::chrono::seconds(10),
                                           [&qmlLoadDone]() {
                                               return qmlLoadDone.load(std::memory_order_relaxed);
                                           })) {
                return;
            }

            QByteArray msg("QML LOAD WATCHDOG: engine.load(");
            msg += watchedQmlPath;
            msg += ") still running after ";
            msg += QByteArray::number(elapsedSeconds);
            msg += " s";
            writeStartupLogLine(startupLogPath, msg);

#ifdef Q_OS_WIN
            if (elapsedSeconds == 60 && !slowQmlStartupFlagBytes.isEmpty()) {
                QByteArray reason("QML load exceeded 60 seconds; use safe graphics on next launch");
                writeSlowQmlStartupFlag(slowQmlStartupFlagBytes, reason);
                QByteArray flagMsg("QML LOAD WATCHDOG: slow startup marker written: ");
                flagMsg += slowQmlStartupFlagBytes;
                writeStartupLogLine(startupLogPath, flagMsg);
            }
#endif
        }
    });

    engine.load(QUrl::fromLocalFile(qmlPath));
#ifdef Q_OS_WIN
    applyApplicationIconToTopLevelWindows(appIcon);
#endif

    {
        std::lock_guard<std::mutex> lock(qmlLoadWatchdogMutex);
        qmlLoadDone.store(true, std::memory_order_relaxed);
    }
    qmlLoadWatchdogCv.notify_all();
    if (qmlLoadWatchdog.joinable())
        qmlLoadWatchdog.join();
    L(("engine.load() returned in " + QByteArray::number(loadTimer.elapsed()) + " ms").constData());

#ifdef Q_OS_WIN
    if (loadTimer.elapsed() > 60000) {
        QByteArray reason("Last QML load took ");
        reason += QByteArray::number(loadTimer.elapsed());
        reason += " ms; use safe graphics on next launch";
        writeSlowQmlStartupFlag(slowQmlStartupFlag.toLocal8Bit(), reason);
        L(("Slow QML startup marker kept for next launch: "
           + slowQmlStartupFlag.toLocal8Bit()).constData());
    }
#endif


    if (engine.rootObjects().isEmpty()) {
        L("ERROR: rootObjects empty — QML failed to load. Check console for QML errors.");
        // Show a native error dialog so user knows what happened
        QMessageBox::critical(nullptr, QStringLiteral("Decodium — QML Error"),
            QStringLiteral("The user interface failed to load.\n\n"
                           "This is usually caused by a missing Qt plugin or a corrupted installation.\n\n"
                           "Try reinstalling Decodium or deleting the qmlcache folder in the install directory.\n\n"
                           "QML path: %1").arg(qmlPath));
        return -1;
    }
    L("QML OK - entering event loop");

    int r = app.exec();
    g_shuttingDown.store(true, std::memory_order_relaxed);
    L("app.exec() exited");
    return r;
}
