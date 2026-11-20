#include "src/app/DecodiumLabStorage.hpp"
#include <QTemporaryDir>
#include <QtTest>

class LabStorageIsolationTest : public QObject {
    Q_OBJECT
private slots:
    void redirectsBeforeGraphicsAndMigration()
    {
        QTemporaryDir area;
        QVERIFY(area.isValid());
        QString const normal = area.path() + "/normal";
        QString const lab = area.path() + "/lab";
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, normal);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, normal + "/system");
        {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
            settings.setValue("Graphics/ResetSafeGraphicsNextLaunch", true);
            settings.setValue("Sentinel", "operator settings");
            settings.sync();
        }
        QFile original(normal + "/Decodium/Decodium3.ini");
        QVERIFY(original.open(QIODevice::ReadOnly));
        QByteArray const before = original.readAll();
        original.close();
        qputenv("DECODIUM_LAB_SETTINGS_DIR", lab.toUtf8());
        char app[] = "decodium", test[] = "--test-mode";
        char* args[] = {app, test};
        QString error;
        QCOMPARE(decodium::lab::configureStorage(2, args, &error), decodium::lab::StorageSetup::Ready);
        QVERIFY(error.isEmpty());
        QVERIFY(QStandardPaths::isTestModeEnabled());
        {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
            QVERIFY(settings.fileName().startsWith(lab + '/'));
            QVERIFY(!settings.value("Graphics/ResetSafeGraphicsNextLaunch", false).toBool());
            QVERIFY(!settings.contains("Sentinel"));
            settings.setValue("OnlyLab", true);
            settings.sync();
        }
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), before);
        qunsetenv("DECODIUM_LAB_SETTINGS_DIR");
        QStandardPaths::setTestModeEnabled(false);
    }
    void normalStartupIsNotRedirected()
    {
        char app[] = "decodium";
        char* args[] = {app};
        QString error;
        QCOMPARE(decodium::lab::configureStorage(1, args, &error), decodium::lab::StorageSetup::NotRequested);
    }
    void rejectsRelativeRoot()
    {
        qputenv("DECODIUM_LAB_SETTINGS_DIR", "relative-lab-path");
        char app[] = "decodium", test[] = "--test-mode";
        char* args[] = {app, test};
        QString error;
        QCOMPARE(decodium::lab::configureStorage(2, args, &error), decodium::lab::StorageSetup::Failed);
        QVERIFY(!error.isEmpty());
        qunsetenv("DECODIUM_LAB_SETTINGS_DIR");
        QStandardPaths::setTestModeEnabled(false);
    }
};
QTEST_GUILESS_MAIN(LabStorageIsolationTest)
#include "test_lab_storage_isolation.moc"
