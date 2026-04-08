// main_qml.cpp - QML entry point for Decodium3

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <cstdio>

#include "DecodiumBridge.h"
#include "DecodiumDxCluster.h"
#include "WaterfallItem.hpp"
#include "PanadapterItem.hpp"

static void L(const char* msg) {
    FILE* f = fopen("C:\\Users\\IU8LMC\\start_log.txt", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

static void qtMsgHandler(QtMsgType, const QMessageLogContext&, const QString& msg) {
    L(msg.toLocal8Bit().constData());
}

int main(int argc, char* argv[])
{
    qInstallMessageHandler(qtMsgHandler);
    L("main() START");
    QQuickStyle::setStyle("Material");
    L("QQuickStyle OK");

    QGuiApplication app(argc, argv);
    L("QGuiApplication OK");
    app.setApplicationName("Decodium");
    app.setApplicationVersion("4.0.0");
    app.setOrganizationName("IU8LMC");
    app.setOrganizationDomain("decodium.iu8lmc.it");

    QQmlApplicationEngine engine;
    L("engine OK");
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));

    DecodiumBridge bridge;
    L("bridge OK");
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
    L("app.exec() exited");
    return r;
}
