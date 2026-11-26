// QSettings riusati sul thread principale (src/radio/DecodiumSettingsPool.h):
// devono leggere esattamente cio' che leggerebbe un QSettings nuovo.
#include "src/radio/DecodiumProfileSettings.h"

#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

class TestSettingsPool : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir_;

    QString iniPath() const { return dir_.filePath(QStringLiteral("user/Decodium/Decodium3.ini")); }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir_.filePath(QStringLiteral("user")));
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, dir_.filePath(QStringLiteral("system")));
    }

    void writesElsewhereInTheProcessAreVisibleAtOnce()
    {
        decodium::PooledSettings first;
        QVERIFY(!first->contains(QStringLiteral("Key")));
        {
            QSettings writer(QSettings::IniFormat, QSettings::UserScope,
                             QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            writer.setValue(QStringLiteral("Key"), 1);
            // Nessun sync(): il valore non e' ancora sul disco.
            QCOMPARE(first->value(QStringLiteral("Key")).toInt(), 1);
        }
        QCOMPARE(first->value(QStringLiteral("Key")).toInt(), 1);
    }

    void releaseReturnsToTheRootGroup()
    {
        {
            decodium::PooledSettings pooled;
            pooled->setValue(QStringLiteral("Root"), QStringLiteral("root"));
            pooled->beginGroup(QStringLiteral("A"));
            pooled->beginGroup(QStringLiteral("B"));
            pooled->setValue(QStringLiteral("Root"), QStringLiteral("nested"));
        }
        decodium::PooledSettings again;
        QCOMPARE(again->group(), QString());
        QCOMPARE(again->value(QStringLiteral("Root")).toString(), QStringLiteral("root"));
        QCOMPARE(again->value(QStringLiteral("A/B/Root")).toString(), QStringLiteral("nested"));
    }

    void nestedAcquisitionsGetDifferentObjects()
    {
        decodium::PooledSettings outer;
        outer->beginGroup(QStringLiteral("Outer"));
        decodium::PooledSettings inner;
        QVERIFY(&*outer != &*inner);
        QCOMPARE(inner->group(), QString());
        QCOMPARE(outer->group(), QStringLiteral("Outer"));
    }

    void changesFromOutsideArriveAfterTheResync()
    {
        {
            decodium::PooledSettings pooled;
            pooled->setValue(QStringLiteral("External"), QStringLiteral("before"));
            pooled->sync();
        }
        // Un altro processo riscrive il file.
        QByteArray content;
        {
            QFile file(iniPath());
            QVERIFY(file.open(QIODevice::ReadOnly));
            content = file.readAll();
        }
        content.replace("External=before", "External=after-outside");
        {
            QFile file(iniPath());
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(file.write(content), qint64(content.size()));
            QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(5), QFileDevice::FileModificationTime));
        }
        QThread::msleep(decodium::kPooledSettingsResyncMs + 50);
        decodium::PooledSettings pooled;
        QCOMPARE(pooled->value(QStringLiteral("External")).toString(), QStringLiteral("after-outside"));
    }

    void otherThreadsGetAFreshObject()
    {
        {
            QSettings writer(QSettings::IniFormat, QSettings::UserScope,
                             QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            writer.setValue(QStringLiteral("Threaded"), 7);
        }
        QSettings* seenByThread = nullptr;
        int value = 0;
        std::unique_ptr<QThread> thread(QThread::create([&] {
            decodium::PooledSettings pooled;
            seenByThread = &*pooled;
            value = pooled->value(QStringLiteral("Threaded")).toInt();
        }));
        thread->start();
        QVERIFY(thread->wait(5000));
        QCOMPARE(value, 7);
        decodium::PooledSettings mainThread;
        QVERIFY(seenByThread != &*mainThread);
    }

    void activeProfileReadsTheProfileGroup()
    {
        {
            QSettings writer(QSettings::IniFormat, QSettings::UserScope,
                             QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            writer.setValue(QStringLiteral("Shared"), QStringLiteral("general"));
            writer.setValue(QStringLiteral("CurrentMultiSettingsConfiguration"), QStringLiteral("LAB"));
            writer.setValue(QStringLiteral("LAB/_ProfileSettingsInitialized"), true);
            writer.setValue(QStringLiteral("LAB/Shared"), QStringLiteral("profile"));
        }
        QCOMPARE(decodium::activeSettingsProfileName(), QStringLiteral("LAB"));
        decodium::PooledSettings pooled;
        QVERIFY(decodium::beginActiveSettingsProfile(*pooled));
        QCOMPARE(pooled->value(QStringLiteral("Shared")).toString(), QStringLiteral("profile"));
        QCOMPARE(decodium::profiledSettingsValue(QString(), QStringLiteral("Shared")).toString(),
                 QStringLiteral("profile"));
        {
            QSettings writer(QSettings::IniFormat, QSettings::UserScope,
                             QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            writer.remove(QStringLiteral("CurrentMultiSettingsConfiguration"));
        }
        QCOMPARE(decodium::activeSettingsProfileName(), QString());
        QCOMPARE(decodium::profiledSettingsValue(QString(), QStringLiteral("Shared")).toString(),
                 QStringLiteral("general"));
    }
};

QTEST_GUILESS_MAIN (TestSettingsPool)
#include "test_settings_pool.moc"
