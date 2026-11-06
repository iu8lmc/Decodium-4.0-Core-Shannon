#include "FtDecodeThreadBudget.hpp"
#include "FtRuntimeAdaptivePolicy.hpp"
#include "Detector/DecodeWorkerScheduling.hpp"
#include "Detector/FT8DecodeWorker.hpp"

#include <QtTest>

class TestFtDecodeThreadBudget final : public QObject
{
    Q_OBJECT

private slots:
    void scalesAcrossCpuSizes_data();
    void scalesAcrossCpuSizes();
    void ft8MicroStallsReduceOnlyDeepPasses();
    void ft8MicroStallsExpireOutsideObservationWindow();
    void ft8MicroStallsRestoreAfterCleanPeriods();
    void legacyPanadapterCadenceTracksRealLoad();
    void reservesOneLogicalCoreForUi();
    void structuredFt8PriorityTargetsQsoTraffic();
};

void TestFtDecodeThreadBudget::scalesAcrossCpuSizes_data()
{
    QTest::addColumn<int>("logicalCores");
    QTest::addColumn<int>("normalLimit");
    QTest::addColumn<int>("expected");

    QTest::newRow("single-core") << 1 << 1 << 1;
    QTest::newRow("dual-core") << 2 << 2 << 1;
    QTest::newRow("four-thread") << 4 << 3 << 3;
    QTest::newRow("six-thread") << 6 << 4 << 4;
    QTest::newRow("eight-thread-intel") << 8 << 6 << 5;
    QTest::newRow("ten-thread-apple") << 10 << 8 << 6;
    QTest::newRow("sixteen-thread") << 16 << 12 << 10;
    QTest::newRow("large-workstation") << 24 << 12 << 12;
    QTest::newRow("manual-low-limit") << 16 << 4 << 4;
    QTest::newRow("runtime-pressure-limit") << 10 << 5 << 5;
}

void TestFtDecodeThreadBudget::scalesAcrossCpuSizes()
{
    QFETCH(int, logicalCores);
    QFETCH(int, normalLimit);
    QFETCH(int, expected);

    QCOMPARE(decodium::decode::adaptiveInteractiveThreadCount(
                 logicalCores, normalLimit, 12),
             expected);
}

void TestFtDecodeThreadBudget::ft8MicroStallsReduceOnlyDeepPasses()
{
    decodium::decode::Ft8MicroStallGuard guard;

    QVERIFY(!guard.recordStall(1000, 100));
    QVERIFY(!guard.recordStall(9000, 120));
    QVERIFY(guard.recordStall(25000, 91));
    QVERIFY(guard.active());
    QCOMPARE(guard.adjustedThreadCount(6, 2), 6);
    QCOMPARE(guard.adjustedThreadCount(6, 3), 5);
    QCOMPARE(guard.adjustedThreadCount(1, 4), 1);
}

void TestFtDecodeThreadBudget::ft8MicroStallsExpireOutsideObservationWindow()
{
    decodium::decode::Ft8MicroStallGuard guard;

    QVERIFY(!guard.recordStall(1000, 100));
    QVERIFY(!guard.recordStall(2000, 100));
    QVERIFY(!guard.recordStall(32001, 100));
    QVERIFY(!guard.active());
    QCOMPARE(guard.recentStallCount(), 1);
}

void TestFtDecodeThreadBudget::ft8MicroStallsRestoreAfterCleanPeriods()
{
    using Guard = decodium::decode::Ft8MicroStallGuard;
    QCOMPARE(Guard::CleanPeriodsToRestore, 16);
    Guard guard;
    guard.recordStall(1000, 100);
    guard.recordStall(2000, 100);
    guard.recordStall(3000, 100);
    QVERIFY(guard.active());

    QCOMPARE(guard.notePeriod(100), Guard::PeriodTransition::None);
    QCOMPARE(guard.cleanPeriods(), 0);
    for (int period = 101; period < 116; ++period) {
        QCOMPARE(guard.notePeriod(period), Guard::PeriodTransition::None);
        QVERIFY(guard.active());
    }
    QCOMPARE(guard.notePeriod(116), Guard::PeriodTransition::Restored);
    QVERIFY(!guard.active());
    QCOMPARE(guard.adjustedThreadCount(6, 4), 6);
}

void TestFtDecodeThreadBudget::legacyPanadapterCadenceTracksRealLoad()
{
    using decodium::decode::legacyPanadapterIntervalMs;
    QCOMPARE(legacyPanadapterIntervalMs(false, false, false), 66);
    QCOMPARE(legacyPanadapterIntervalMs(true, false, false), 125);
    QCOMPARE(legacyPanadapterIntervalMs(false, true, false), 180);
    QCOMPARE(legacyPanadapterIntervalMs(true, true, false), 180);
    QCOMPARE(legacyPanadapterIntervalMs(false, true, true), 250);

    QCOMPARE(legacyPanadapterIntervalMs(false, false, false, true, 33), 33);
    QCOMPARE(legacyPanadapterIntervalMs(false, false, false, true, 50), 50);
    QCOMPARE(legacyPanadapterIntervalMs(false, false, false, true, 66), 66);
    QCOMPARE(legacyPanadapterIntervalMs(true, false, false, true, 33), 50);
    QCOMPARE(legacyPanadapterIntervalMs(false, true, false, true, 33), 66);
    QCOMPARE(legacyPanadapterIntervalMs(false, true, true, true, 33), 125);
}

void TestFtDecodeThreadBudget::reservesOneLogicalCoreForUi()
{
    using decodium::decode::threadLimitWithUiReserveForCores;
    QCOMPARE(threadLimitWithUiReserveForCores(12, 16), 12);
    QCOMPARE(threadLimitWithUiReserveForCores(12, 8), 7);
    QCOMPARE(threadLimitWithUiReserveForCores(4, 4), 3);
    QCOMPARE(threadLimitWithUiReserveForCores(2, 2), 1);
    QCOMPARE(threadLimitWithUiReserveForCores(1, 1), 1);
}

void TestFtDecodeThreadBudget::structuredFt8PriorityTargetsQsoTraffic()
{
    decodium::ft8::DecodedEntry general;
    decodium::ft8::DecodedEntry directed;
    directed.addressedToMe = true;
    decodium::ft8::DecodedEntry partnerReport;
    partnerReport.fromActivePartner = true;
    partnerReport.qsoExchange = true;

    QVERIFY(!general.isUiPriority());
    QVERIFY(directed.isUiPriority());
    QVERIFY(partnerReport.isUiPriority());
}

QTEST_MAIN(TestFtDecodeThreadBudget)
#include "test_ft_decode_thread_budget.moc"
