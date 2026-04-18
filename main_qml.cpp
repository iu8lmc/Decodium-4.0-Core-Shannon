// main_qml.cpp - QML entry point for Decodium3

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMetaType>
#include <QStyleFactory>
#include <QFont>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QLockFile>
#include <QList>
#include <QLocale>
#include <atomic>
#include <cstdio>
#include <clocale>

#include "DecodiumBridge.h"
#include "DecodiumDiagnostics.h"
#include "DecodiumDxCluster.h"
#include "DecodiumLogging.hpp"
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

    QApplication app(argc, argv);
    DecodiumLogging::installCrashHandler();
    L("QApplication OK");

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
    parser.addOption(rigOption);
    parser.addOption(configOption);
    parser.addOption(languageOption);
    parser.addOption(testOption);

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
    QString const languageOverride = parser.value(languageOption).trimmed();
    if (!languageOverride.isEmpty()) {
        app.setProperty("decodiumLanguageOverride", languageOverride);
    }

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
    // Disable QML disk cache to prevent stale .qmlc files from overriding
    // updated .qml sources after an upgrade.  The compiled cache is a
    // micro-optimisation (<50ms) that is not worth the risk of loading old UI.
    qputenv("QML_DISABLE_DISK_CACHE", "1");

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

    engine.rootContext()->setContextProperty("bridge", &bridge);
    engine.rootContext()->setContextProperty("appEngine", &bridge);

    QString qmlPath = QDir(QCoreApplication::applicationDirPath())
                          .absoluteFilePath("qml/decodium/Main.qml");

    if (!QFile::exists(qmlPath)) {
        L("QML file NOT FOUND");
        return -1;
    }
    L(("qmlPath=" + qmlPath.toLocal8Bit()).constData());

    qmlRegisterType<WaterfallItem>("Decodium", 1, 0, "WaterfallItem");
    qmlRegisterType<PanadapterItem>("Decodium", 1, 0, "PanadapterItem");
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
    engine.load(QUrl::fromLocalFile(qmlPath));
    L("engine.load() returned");

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
