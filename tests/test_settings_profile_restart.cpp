// Cross-process storage test. Does not instantiate radio/audio services or use
// the operator's settings. GUI/backend integration is a separate validation.
#include "../src/radio/DecodiumProfileSettings.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

static QJsonArray keys()
{
    QFile file(QStringLiteral(SETTINGS_AUDIT_KEYS));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).array();
}

static int child(const QStringList &args)
{
    // QSettings::setPath, rather than HOME/XDG overrides, is portable on macOS
    // and Windows too. All instances, including profile initialisation, use it.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, args.at(2));
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, args.at(2));
    QCoreApplication::instance()->setProperty("decodiumConfigName", args.at(3));
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    settings.setFallbacksEnabled(false);
    decodium::beginActiveSettingsProfile(settings);
    const auto inventory = keys();
    if (inventory.isEmpty()) return 10;
    const QString operation = args.at(4);
    const QString stamp = args.at(5);
    for (const auto &entry : inventory) {
        const QString key = entry.toString();
        if (key == "WeatherApiEnable" || key == "SendStationTelemetry") continue;
        // Chiavi che differiscono solo per le maiuscole esistono apposta (backend
        // QML e classico). Su Windows le chiavi INI sono case-insensitive: se il
        // valore portasse il nome esatto, la seconda grafia sovrascriverebbe la
        // prima e la rilettura fallirebbe. Col nome in minuscolo le due grafie
        // scrivono lo stesso valore e il test resta valido ovunque.
        if (operation == "write") settings.setValue(key, stamp + ":" + key.toLower());
        else if (settings.value(key).toString() != stamp + ":" + key.toLower()) return 11;
    }
    if (operation == "write") {
        settings.setValue("WeatherApiEnable", false);
        settings.setValue("SendStationTelemetry", false);
        settings.setValue("Typed/Integer", 17);
        settings.setValue("Typed/Double", 1.25);
        settings.setValue("Typed/List", QStringList{"a", "b"});
    } else {
        if (!settings.contains("WeatherApiEnable") || settings.value("WeatherApiEnable").toBool()
            || !settings.contains("SendStationTelemetry") || settings.value("SendStationTelemetry").toBool()
            || settings.value("Typed/Integer").toInt() != 17
            || settings.value("Typed/Double").toDouble() != 1.25
            || settings.value("Typed/List").toStringList() != QStringList{"a", "b"}) return 12;
    }
    settings.sync();
    return settings.status() == QSettings::NoError ? 0 : 13;
}

class SettingsRestartTest : public QObject
{
    Q_OBJECT
private slots:
    void separateProcessesAndProfiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto run = [&](const QString &profile, const QString &operation, const QString &stamp) {
            QProcess process;
            process.start(QCoreApplication::applicationFilePath(),
                          {"--child", dir.path(), profile, operation, stamp});
            if (!process.waitForFinished(15000)) return -1;
            return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -2;
        };
        QCOMPARE(run("", "write", "root"), 0);
        QCOMPARE(run("", "read", "root"), 0);
        // A new profile inherits once, then must remain independent.
        QCOMPARE(run("TEST-A", "read", "root"), 0);
        QCOMPARE(run("TEST-A", "write", "alpha"), 0);
        QCOMPARE(run("TEST-B", "write", "beta"), 0);
        QCOMPARE(run("TEST-A", "read", "alpha"), 0);
        QCOMPARE(run("TEST-B", "read", "beta"), 0);
        QCOMPARE(run("", "read", "root"), 0);
        QCOMPARE(run("", "write", "new-root"), 0);
        QCOMPARE(run("TEST-A", "read", "alpha"), 0);
        QCOMPARE(run("TEST-B", "read", "beta"), 0);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().value(1) == "--child") return child(app.arguments());
    SettingsRestartTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_settings_profile_restart.moc"
