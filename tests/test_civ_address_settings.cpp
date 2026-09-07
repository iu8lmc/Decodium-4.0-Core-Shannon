#include <QCoreApplication>
#include <QMetaProperty>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "src/bridge/DecodiumLogging.hpp"
#include "src/radio/DecodiumCatManager.h"

// DecodiumCatManager emits diagnostic messages, but this persistence test does
// not initialise the application's asynchronous logging subsystem.
void DecodiumLogging::diag(DiagCategory, const QString&)
{
}

class CivAddressSettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void customAddressIsWritableAndPersists()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());

        auto* app = QCoreApplication::instance();
        QVERIFY(app);
        app->setProperty("decodiumConfigName", QStringLiteral("CI-V test"));

        {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            settings.beginGroup(QStringLiteral("MultiSettings"));
            settings.beginGroup(QStringLiteral("CI-V test"));
            settings.beginGroup(QStringLiteral("CAT_Native"));
            settings.setValue(QStringLiteral("rigName"), QStringLiteral("Icom IC-7300"));
            settings.setValue(QStringLiteral("civAddress"), 0xA4);
            settings.endGroup();
            settings.endGroup();
            settings.endGroup();
            settings.sync();
        }

        {
            DecodiumCatManager manager;
            QCOMPARE(manager.civAddress(), 0xA4);

            QMetaProperty const property = manager.metaObject()->property(
                manager.metaObject()->indexOfProperty("civAddress"));
            QVERIFY(property.isValid());
            QVERIFY(property.isWritable());
            QVERIFY(property.write(&manager, 0xA6));
            QCOMPARE(manager.civAddress(), 0xA6);
            manager.saveSettings();
        }

        DecodiumCatManager reloaded;
        QCOMPARE(reloaded.rigName(), QStringLiteral("Icom IC-7300"));
        QCOMPARE(reloaded.civAddress(), 0xA6);

        app->setProperty("decodiumConfigName", {});
    }
};

QTEST_MAIN(CivAddressSettingsTest)
#include "test_civ_address_settings.moc"
