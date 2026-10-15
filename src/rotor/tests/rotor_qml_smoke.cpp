// Carica RotorWindow.qml con un rotore simulato e profilo in memoria (INI
// temporaneo): stampa gli errori QML e salva uno screenshot per tab.
#include "RotorModule.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <QTemporaryDir>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 3)
        return 2;
    QTemporaryDir dir;
    QSettings settings(dir.filePath("rotor.ini"), QSettings::IniFormat);
    settings.setValue("Rotor/enabled", true);
    settings.setValue("Rotor/simulate", true);
    settings.setValue("Rotor/networkEnabled", argc > 4);
    settings.sync();

    decodium::rotor::RotorModule rotor;
    rotor.setStation("IU8LMC", "JN70");
    rotor.start(&settings);
    rotor.noteDecode("CQ DX EA8ABC IL18", -12, true, "FT8", 14074000);
    rotor.noteDecode("CQ K1ABC FN31", -5, true, "FT8", 14074000);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("C:/msys64/mingw64/share/qt6/qml"));
    engine.rootContext()->setContextProperty("rotor", &rotor);
    engine.load(QUrl::fromLocalFile(argv[1]));
    if (engine.rootObjects().isEmpty())
        return 3;
    auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!win)
        return 4;
    win->show();
    QString out = argv[2];
    QTimer::singleShot(4000, [&] {
        rotor.gotoAzimuth(75.0);
    });
    QTimer::singleShot(7000, [&] {
        win->grabWindow().save(out);
        app.quit();
    });
    return app.exec();
}
