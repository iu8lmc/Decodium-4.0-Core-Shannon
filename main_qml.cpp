// main_qml.cpp - QML entry point for Decodium3

#include <QApplication>
#include <QMetaType>
#include <QStyleFactory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QStandardPaths>
#include <atomic>
#include <cstdio>

#include "DecodiumBridge.h"
#include "DecodiumDxCluster.h"
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
}

static std::atomic_bool g_shuttingDown {false};

static bool isIgnorableShutdownQmlMessage(const QString& msg)
{
    return msg.contains(".qml:");
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
    L("QApplication OK");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setApplicationName("Decodium");
    app.setApplicationVersion("4.0.0");
    app.setOrganizationName("IU8LMC");
    app.setOrganizationDomain("decodium.iu8lmc.it");
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        g_shuttingDown.store(true, std::memory_order_relaxed);
    });

    DecodiumBridge bridge;
    L("bridge OK");

    // Keep the bridge alive longer than the QML engine. During shutdown QML
    // bindings still reevaluate while root objects are being torn down.
    // If the context object dies first, QML sees bridge == null and floods the
    // terminal with TypeError messages on exit.
    QQmlApplicationEngine engine;
    L("engine OK");
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

    engine.load(QUrl::fromLocalFile(qmlPath));
    L("engine.load() returned");

    if (engine.rootObjects().isEmpty()) {
        L("ERROR: rootObjects empty");
        return -1;
    }
    L("QML OK - entering event loop");

    int r = app.exec();
    g_shuttingDown.store(true, std::memory_order_relaxed);
    L("app.exec() exited");
    return r;
}
