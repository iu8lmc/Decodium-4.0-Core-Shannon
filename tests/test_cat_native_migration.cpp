#include <QtTest>

#include "src/radio/CatNativeMigration.h"

using decodium::radio::migrateRemovedNativeCatBackend;

class TestCatNativeMigration : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir_;

    QString iniPath () const {return dir_.filePath (QStringLiteral ("settings.ini"));}

private slots:
    void initTestCase () {QVERIFY (dir_.isValid ());}

    void init () {QFile::remove (iniPath ());}

    void copiesNativeSettingsToHamlibAndSwitchesBackend ()
    {
        QSettings s {iniPath (), QSettings::IniFormat};
        s.setValue ("catBackend", "native");
        s.setValue ("CAT_Native/rigName", "Yaesu FT-991A");
        s.setValue ("CAT_Native/serialPort", "COM5");
        s.setValue ("CAT_Native/baudRate", 38400);
        s.setValue ("CAT_Native/handshake", "Hardware");
        s.setValue ("CAT_Native/pttMethod", "CAT");
        s.setValue ("CAT_Native/civAddress", 0x94);
        s.setValue ("CAT_Native/catAutoConnect", true);

        QCOMPARE (migrateRemovedNativeCatBackend (s), 7);

        QCOMPARE (s.value ("catBackend").toString (), QStringLiteral ("hamlib"));
        QCOMPARE (s.value ("Transceiver/rigName").toString (), QStringLiteral ("Yaesu FT-991A"));
        QCOMPARE (s.value ("Transceiver/serialPort").toString (), QStringLiteral ("COM5"));
        QCOMPARE (s.value ("Transceiver/baudRate").toInt (), 38400);
        QCOMPARE (s.value ("Transceiver/handshake").toString (), QStringLiteral ("Hardware"));
        QCOMPARE (s.value ("Transceiver/civAddress").toInt (), 0x94);
        QVERIFY (s.value ("Transceiver/catAutoConnect").toBool ());
    }

    void replacesStaleHamlibValuesTheUserWasNotUsing ()
    {
        QSettings s {iniPath (), QSettings::IniFormat};
        s.setValue ("catBackend", "NATIVE");   // case does not matter
        s.setValue ("Transceiver/serialPort", "COM1");
        s.setValue ("Transceiver/baudRate", 9600);
        s.setValue ("CAT_Native/serialPort", "COM7");

        QVERIFY (migrateRemovedNativeCatBackend (s) >= 1);

        QCOMPARE (s.value ("Transceiver/serialPort").toString (), QStringLiteral ("COM7"));
        // a value native never stored is left alone
        QCOMPARE (s.value ("Transceiver/baudRate").toInt (), 9600);
    }

    void doesNothingForOtherBackends ()
    {
        for (QString const& backend : {QStringLiteral ("hamlib"), QStringLiteral ("omnirig"),
                                       QStringLiteral ("tci"), QStringLiteral ("cat4om"), QString {}})
          {
            QFile::remove (iniPath ());
            QSettings s {iniPath (), QSettings::IniFormat};
            if (!backend.isEmpty ()) s.setValue ("catBackend", backend);
            s.setValue ("CAT_Native/serialPort", "COM7");
            s.setValue ("Transceiver/serialPort", "COM3");

            QCOMPARE (migrateRemovedNativeCatBackend (s), -1);
            QCOMPARE (s.value ("Transceiver/serialPort").toString (), QStringLiteral ("COM3"));
            QCOMPARE (s.value ("catBackend").toString (), backend);
          }
    }

    void runsOnlyOnce ()
    {
        QSettings s {iniPath (), QSettings::IniFormat};
        s.setValue ("catBackend", "native");
        s.setValue ("CAT_Native/serialPort", "COM7");

        QVERIFY (migrateRemovedNativeCatBackend (s) >= 0);
        // the user later changes the Hamlib port on purpose
        s.setValue ("Transceiver/serialPort", "COM9");
        QCOMPARE (migrateRemovedNativeCatBackend (s), -1);
        QCOMPARE (s.value ("Transceiver/serialPort").toString (), QStringLiteral ("COM9"));
    }

    void nativeWithoutSavedFieldsStillSwitchesToHamlib ()
    {
        QSettings s {iniPath (), QSettings::IniFormat};
        s.setValue ("catBackend", "native");
        QCOMPARE (migrateRemovedNativeCatBackend (s), 0);
        QCOMPARE (s.value ("catBackend").toString (), QStringLiteral ("hamlib"));
    }
};

QTEST_GUILESS_MAIN (TestCatNativeMigration)
#include "test_cat_native_migration.moc"
