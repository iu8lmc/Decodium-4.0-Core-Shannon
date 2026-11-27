// Cartelle dati di questa istanza per i semi hash FT8/FT4 (Detector/HashSeedOwnDirs.hpp).
#include "Detector/HashSeedOwnDirs.hpp"

#include <QtTest>

class TestHashSeedOwnDirs : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Come l'app su Windows (DecodiumStorageMigration::organizationName).
        QCoreApplication::setOrganizationName(QStringLiteral("Decodium"));
        QCoreApplication::setApplicationName(QStringLiteral("Decodium"));
    }

    void ownDirsAreThisInstanceDataDirs()
    {
        QStringList const dirs = decodium::hash_seed::own_data_dirs();
        QString const local = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        QString const roaming = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        QVERIFY(dirs.contains(local));
        QVERIFY(dirs.contains(roaming));
        QCOMPARE(dirs.size(), local.compare(roaming, Qt::CaseInsensitive) == 0 ? 1 : 2);
    }

    void recognisesTheEmbeddedLiveLog()
    {
        QString const local = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QString const live = QDir::cleanPath(QDir(local).filePath(QStringLiteral("embedded-ft2/ALL.TXT"))).toUpper();
        QString const own = QDir::cleanPath(QDir(local).filePath(QStringLiteral("ALL.TXT"))).toUpper();
        QVERIFY(decodium::hash_seed::path_in_own_data_dir(live, true));
        QVERIFY(decodium::hash_seed::path_in_own_data_dir(live, false));
        QVERIFY(!decodium::hash_seed::path_in_own_data_dir(own, true));
        QVERIFY(decodium::hash_seed::path_in_own_data_dir(own, false));

        // Un'altra istanza (--rig-name) o un altro programma non sono "nostri".
        QString const other = QDir::cleanPath(local + QStringLiteral(" - RX2/embedded-ft2/ALL.TXT")).toUpper();
        QVERIFY(!decodium::hash_seed::path_in_own_data_dir(other, true));
        QVERIFY(!decodium::hash_seed::path_in_own_data_dir(other, false));
        QVERIFY(!decodium::hash_seed::path_in_own_data_dir(QStringLiteral("C:/USERS/X/APPDATA/LOCAL/JTDX/202611_ALL.TXT"), false));
    }
};

QTEST_GUILESS_MAIN (TestHashSeedOwnDirs)
#include "test_hash_seed_own_dirs.moc"
