#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "src/app/DecodiumUpdateSupport.hpp"

class TestDecodiumUpdateSupport final : public QObject
{
    Q_OBJECT

private slots:
    void releaseVersionsAreComparedNumerically()
    {
        QVERIFY(decodium::update::isVersionNewer(
            QStringLiteral("1.0.560"), QStringLiteral("1.0.99")));
        QVERIFY(!decodium::update::isVersionNewer(
            QStringLiteral("1.0.559"), QStringLiteral("1.0.559")));
    }

    void primaryNewReleaseStopsTheFallback()
    {
        QCOMPARE(decodium::update::releaseCheckDecision(
                     QStringLiteral("1.0.560"), QStringLiteral("1.0.559"), true),
                 decodium::update::ReleaseCheckDecision::UseUpdate);
    }

    void primaryWithoutNewReleaseChecksTheFallback()
    {
        QCOMPARE(decodium::update::releaseCheckDecision(
                     QStringLiteral("1.0.559"), QStringLiteral("1.0.559"), true),
                 decodium::update::ReleaseCheckDecision::CheckFallback);
        QCOMPARE(decodium::update::releaseCheckDecision(
                     QString(), QStringLiteral("1.0.559"), true),
                 decodium::update::ReleaseCheckDecision::CheckFallback);
    }

    void secondaryWithoutNewReleaseCompletesTheCheck()
    {
        QCOMPARE(decodium::update::releaseCheckDecision(
                     QStringLiteral("1.0.559"), QStringLiteral("1.0.559"), false),
                 decodium::update::ReleaseCheckDecision::NoUpdate);
    }

    void linuxX86DoesNotSelectArmFirst()
    {
        const QStringList assets {
            QStringLiteral("decodium4-ft2-1.0.548-linux-aarch64.AppImage"),
            QStringLiteral("decodium4-ft2-1.0.548-linux-aarch64.AppImage.sha256.txt"),
            QStringLiteral("decodium4-ft2-1.0.548-linux-x86_64.AppImage")
        };
        QCOMPARE(decodium::update::bestAssetIndex(
                     assets, QStringLiteral("linux"), QStringLiteral("x86_64")), 2);
    }

    void linuxArmDoesNotSelectX86First()
    {
        const QStringList assets {
            QStringLiteral("decodium4-ft2-1.0.548-linux-x86_64.AppImage"),
            QStringLiteral("decodium4-ft2-1.0.548-linux-aarch64.AppImage")
        };
        QCOMPARE(decodium::update::bestAssetIndex(
                     assets, QStringLiteral("linux"), QStringLiteral("arm64")), 1);
    }

    void architectureSpecificPackageBeatsGenericPackage()
    {
        const QStringList assets {
            QStringLiteral("decodium4-ft2-linux.AppImage"),
            QStringLiteral("decodium4-ft2-linux-x86_64.AppImage")
        };
        QCOMPARE(decodium::update::bestAssetIndex(
                     assets, QStringLiteral("linux"), QStringLiteral("amd64")), 1);
    }

    void macArchitectureIsAlsoRespected()
    {
        const QStringList assets {
            QStringLiteral("decodium-macos-sequoia-arm64.dmg"),
            QStringLiteral("decodium-macos-sequoia-x86_64.dmg")
        };
        QCOMPARE(decodium::update::bestAssetIndex(
                     assets, QStringLiteral("macos"), QStringLiteral("x86_64")), 1);
    }

    void windowsStillSelectsTheSetupExecutable()
    {
        const QStringList assets {
            QStringLiteral("Decodium-portable.exe"),
            QStringLiteral("Decodium_1.0.548_Setup_x64.exe")
        };
        QCOMPARE(decodium::update::bestAssetIndex(
                     assets, QStringLiteral("windows"), QStringLiteral("x86_64")), 1);
    }

    void unknownArchitectureRejectsTaggedPackages()
    {
        const QStringList assets {
            QStringLiteral("decodium-linux-aarch64.AppImage"),
            QStringLiteral("decodium-linux-x86_64.AppImage"),
            QStringLiteral("decodium-linux.AppImage")
        };
        QCOMPARE(decodium::update::bestAssetIndex(
                     assets, QStringLiteral("linux"), QStringLiteral("riscv64")), 2);
    }

    void appImageReplacementIsAtomicAndExecutable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString target = directory.filePath(QStringLiteral("Decodium.AppImage"));

        QFile original(target);
        QVERIFY(original.open(QIODevice::WriteOnly));
        QCOMPARE(original.write("old-image"), qint64(9));
        original.close();

        QSaveFile update(target);
        update.setDirectWriteFallback(false);
        QVERIFY(update.open(QIODevice::WriteOnly));
        QCOMPARE(update.write("new-image"), qint64(9));

        const QFileDevice::Permissions permissions =
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
            | QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser
            | QFileDevice::ReadGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::ExeOther;
        QString error;
        QVERIFY2(decodium::update::commitAtomicFile(update, permissions, &error),
                 qPrintable(error));

        QFile installed(target);
        QVERIFY(installed.open(QIODevice::ReadOnly));
        QCOMPARE(installed.readAll(), QByteArray("new-image"));
    #ifdef Q_OS_UNIX
    // Il bit di esecuzione esiste su Unix. Su Windows QFileInfo lo deduce
    // dall'estensione, quindi un file .AppImage risulterebbe non eseguibile
    // e la prova fallirebbe per costruzione, non per un difetto.
    QVERIFY(QFileInfo(installed).permission(QFileDevice::ExeOwner));
#endif
    }
};

QTEST_MAIN(TestDecodiumUpdateSupport)
#include "test_decodium_update_support.moc"
