#include "src/app/MultiSettings.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QtTest>

class MultiSettingsSwitchTest : public QObject
{
    Q_OBJECT
private slots:
    void switchesAndRestoresSettingsWithinOneProcess()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString appName = QStringLiteral("DecodiumMultiSwitchTest-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QCoreApplication::setApplicationName(appName);
        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
                                 .filePath(appName + QStringLiteral(".ini"));
        {
            QSettings seed(path, QSettings::IniFormat);
            seed.setValue("RtlSdrEnabled", false);
            seed.setValue("MultiSettings/CurrentName", "Default");
            seed.setValue("MultiSettings/Enabled/RtlSdrEnabled", true);
            seed.setValue("MultiSettings/Disabled/RtlSdrEnabled", false);
            seed.sync();
            QCOMPARE(seed.status(), QSettings::NoError);
        }
        {
            MultiSettings settings;
            QCOMPARE(settings.settings()->value("RtlSdrEnabled").toBool(), false);
            QMainWindow first;
            QMenu firstMenu;
            settings.create_menu_actions(&first, &firstMenu);
            QSignalSpy changed(&settings, &MultiSettings::configurationNameChanged);
            settings.select_configuration("Enabled");
            QCOMPARE(settings.configuration_name(), QStringLiteral("Enabled"));
            QVERIFY(!changed.isEmpty());
            QVERIFY(!settings.exit());
            QCOMPARE(settings.settings()->value("RtlSdrEnabled").toBool(), true);
            QMainWindow second;
            QMenu secondMenu;
            settings.create_menu_actions(&second, &secondMenu);
            settings.select_configuration("Disabled");
            QVERIFY(!settings.exit());
            QCOMPARE(settings.settings()->value("RtlSdrEnabled").toBool(), false);
            QMainWindow third;
            QMenu thirdMenu;
            settings.create_menu_actions(&third, &thirdMenu);
            settings.select_configuration("Enabled");
            QVERIFY(!settings.exit());
            QCOMPARE(settings.settings()->value("RtlSdrEnabled").toBool(), true);
        }
        QVERIFY(QFile::remove(path));
    }
};

QTEST_MAIN(MultiSettingsSwitchTest)
#include "test_multisettings_switch.moc"
