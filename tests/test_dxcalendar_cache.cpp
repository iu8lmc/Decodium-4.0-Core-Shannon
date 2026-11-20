#include "src/services/DxCalendarCache.h"
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class DxCalendarCacheTest : public QObject {
    Q_OBJECT
private slots:
    void asynchronousRoundTripAndFailurePreservesCache()
    {
        QTemporaryDir dir(QDir(QStringLiteral(DECODIUM_TEST_DIR)).filePath(QStringLiteral("dxcalendar-test-XXXXXX")));
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("calendar.json"));
        DxCalendarCache cache(nullptr, path);
        QSignalSpy reads(&cache, &DxCalendarCache::cacheRead);
        QSignalSpy writes(&cache, &DxCalendarCache::cacheWritten);
        cache.readCache();
        QCOMPARE(reads.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(reads.count(), 1, 5000);
        QCOMPARE(reads.takeFirst().at(0).toString(), QString());
        const QString data = QStringLiteral("BEGIN:VCALENDAR\nSynthetic\nEND:VCALENDAR\n");
        cache.writeCache(data, QStringLiteral("2026-11-12 12:00 UTC"));
        QCOMPARE(writes.count(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(writes.count(), 1, 5000);
        QCOMPARE(writes.takeFirst().at(0).toString(), QString());
        cache.readCache();
        QTRY_COMPARE_WITH_TIMEOUT(reads.count(), 1, 5000);
        const auto saved = reads.takeFirst();
        QCOMPARE(saved.at(0).toString(), data);
        QCOMPARE(saved.at(1).toString(), QStringLiteral("2026-11-12 12:00 UTC"));
        cache.writeCache(QString(2000001, QLatin1Char('x')), QStringLiteral("bad"));
        QTRY_COMPARE_WITH_TIMEOUT(writes.count(), 1, 5000);
        QVERIFY(!writes.takeFirst().at(0).toString().isEmpty());
        cache.readCache();
        QTRY_COMPARE_WITH_TIMEOUT(reads.count(), 1, 5000);
        QCOMPARE(reads.takeFirst().at(0).toString(), data);
    }
};
QTEST_GUILESS_MAIN(DxCalendarCacheTest)
#include "test_dxcalendar_cache.moc"
