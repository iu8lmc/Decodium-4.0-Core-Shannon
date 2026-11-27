#include <QtTest>

#include <random>

#include "src/bridge/FoxElasticCycle.h"

using decodium::foxelastic::Config;
using decodium::foxelastic::Cycle;

class TestFoxElasticCycle final : public QObject
{
    Q_OBJECT

private slots:
    void closedWindowNeverFires();
    void nobodyExpectedWaitsForTheDefaultFloor();
    void everybodyAnsweredStillWaitsForNewcomers();
    void silentStationIsGivenUpAtItsTimeout();
    void slowStationRaisesItsOwnTimeout();
    void learnedFloorShortensTheCycle();
    void aFirstReplyDoesNotInflateTheTimeoutBeyondTheGridCycle();
    void aSilentStationIsWaitedLessNextTime();
    void theWindowIsNeverLongerThanTheGridCycle();
    void unreliableDecodesDoNotShortenTheWindow();
    void eachStationCountsOnceInAWindow();
    void theWindowIsNeverShorterThanARealAnswer();
    void windowNeverExceedsTheCap();
    void dispatchClosesTheWindow();
    void elasticCycleBeatsTheGridAndNeverCutsAnAnswer();
};

void TestFoxElasticCycle::closedWindowNeverFires()
{
    Cycle c;
    QVERIFY(!c.windowOpen());
    QCOMPARE(c.goAtMs(), 0LL);
    QVERIFY(!c.shouldGo(1000000));
}

void TestFoxElasticCycle::nobodyExpectedWaitsForTheDefaultFloor()
{
    Cycle c;
    c.txEnded(10000);
    // no measurement yet: initialReplyMs (3400) + guard (100)
    QCOMPARE(c.goAtMs(), 10000LL + 3500);
    QVERIFY(!c.shouldGo(13499));
    QVERIFY(c.shouldGo(13500));
}

void TestFoxElasticCycle::everybodyAnsweredStillWaitsForNewcomers()
{
    Cycle c;
    c.setExpected({"AA1AA", "BB2BB"});
    c.txEnded(0);
    c.decodeHeard("AA1AA", 2800);
    c.decodeHeard("BB2BB", 2900);
    QCOMPARE(c.pendingCount(), 0);
    // fewer than 5 samples: the default newcomer delay still applies
    QCOMPARE(c.goAtMs(), 3500LL);
}

void TestFoxElasticCycle::silentStationIsGivenUpAtItsTimeout()
{
    Cycle c;
    c.setExpected({"AA1AA", "SILENT"});
    c.txEnded(0);
    c.decodeHeard("AA1AA", 2800);
    QCOMPARE(c.pendingCount(), 1);
    // initialReplyMs + grace
    QCOMPARE(c.goAtMs(), 3700LL);
}

void TestFoxElasticCycle::slowStationRaisesItsOwnTimeout()
{
    Cycle c;
    // a station that answers late every time (e.g. slower software)
    for (int i = 0; i < 6; ++i) {
        c.setExpected({"SLOW1"});
        c.txEnded(i * 20000LL);
        c.decodeHeard("SLOW1", i * 20000LL + 4000);
        c.dispatched();
    }
    c.setExpected({"SLOW1"});
    c.txEnded(200000);
    QVERIFY2(c.timeoutMs("SLOW1") > 4000, "timeout must exceed the learned delay");
    QVERIFY(c.goAtMs() >= 200000LL + 4000);
    QVERIFY(c.goAtMs() <= 200000LL + 4700);
}

void TestFoxElasticCycle::learnedFloorShortensTheCycle()
{
    Cycle c;
    // callers answer ~2.9 s after our TX ended
    for (int i = 0; i < 8; ++i) {
        c.setExpected({"AA1AA"});
        c.txEnded(i * 20000LL);
        c.decodeHeard("AA1AA", i * 20000LL + 2900);
        c.dispatched();
    }
    c.setExpected({"AA1AA"});
    c.txEnded(500000);
    c.decodeHeard("AA1AA", 500000 + 2900);
    long long const at = c.goAtMs() - 500000;
    QVERIFY2(at >= 2900, "never before the answer");
    QVERIFY2(at <= 3100, "learned floor must be shorter than the 3500 default");
}

void TestFoxElasticCycle::aFirstReplyDoesNotInflateTheTimeoutBeyondTheGridCycle()
{
    // On the air (24/10) the first reply made the next timeout three times the
    // round trip, capped at 6 s: the cycle became 8.9 s, slower than the grid.
    Cycle c;
    c.setExpected({"AA1AA"});
    c.txEnded(0);
    c.decodeHeard("AA1AA", 2500);
    c.dispatched();
    c.setExpected({"AA1AA"});
    c.txEnded(100000);
    QVERIFY2(c.timeoutMs("AA1AA") <= 2 * 2500 + 300 + 1, "first sample must not triple the timeout");
    QVERIFY(c.goAtMs() - 100000 <= 4700);
}

void TestFoxElasticCycle::aSilentStationIsWaitedLessNextTime()
{
    Cycle c;
    for (int i = 0; i < 4; ++i) {          // it used to answer slowly
        c.setExpected({"AA1AA"});
        c.txEnded(i * 20000LL);
        c.decodeHeard("AA1AA", i * 20000LL + 4400);
        c.dispatched();
    }
    c.setExpected({"AA1AA"});
    c.txEnded(100000);
    double const before = c.timeoutMs("AA1AA");
    c.dispatched();                         // ... and then it stayed silent
    c.setExpected({"AA1AA"});
    c.txEnded(200000);
    QVERIFY2(c.timeoutMs("AA1AA") < before, "a silent station must be waited for less");
    QVERIFY(c.timeoutMs("AA1AA") <= 3400 + 300 + 1);
}

void TestFoxElasticCycle::theWindowIsNeverLongerThanTheGridCycle()
{
    Cycle c;
    c.setExpected({"A", "B", "C", "D"});
    c.txEnded(0);
    // nobody answers at all
    QVERIFY(c.goAtMs() <= 4700);
    // and a decode arriving after the cap is not learned
    c.decodeHeard("A", 9000);
    c.dispatched();
    c.setExpected({"A"});
    c.txEnded(50000);
    QVERIFY(c.goAtMs() - 50000 <= 4700);
}

void TestFoxElasticCycle::unreliableDecodesDoNotShortenTheWindow()
{
    // 25/10/2026, on the air: '?' a-priori decodes arrived 0.5-2 s after our TX
    // and the learned delay fell to 2.0 s; the Fox keyed over the real answers.
    Cycle c;
    for (int i = 0; i < 10; ++i) {
        c.setExpected({"AA1AA"});
        c.txEnded(i * 20000LL);
        c.decodeHeard("AA1AA", i * 20000LL + 800, false);   // phantom, early
        c.decodeHeard("AA1AA", i * 20000LL + 3100, true);   // the real answer
        c.dispatched();
    }
    c.setExpected({"AA1AA"});
    c.txEnded(500000);
    QVERIFY2(c.newcomerDelayMs() >= 3000.0, "phantom decodes must not pull the delay down");
    QVERIFY(c.pendingCount() == 1);
    c.decodeHeard("AA1AA", 500000 + 700, false);
    QVERIFY2(c.pendingCount() == 1, "an unreliable decode must not count as the answer");
}

void TestFoxElasticCycle::eachStationCountsOnceInAWindow()
{
    Cycle c;
    for (int i = 0; i < 6; ++i) {
        c.setExpected({"AA1AA"});
        c.txEnded(i * 20000LL);
        // the same frame decoded over and over while it is on the air
        for (int k = 0; k < 8; ++k) {
            c.decodeHeard("AA1AA", i * 20000LL + 2400 + k * 250, true);
        }
        c.dispatched();
    }
    c.setExpected({"AA1AA"});
    c.txEnded(300000);
    // one sample per station and window, the first one: 2400 ms
    QVERIFY2(c.newcomerDelayMs() >= 2400.0 && c.newcomerDelayMs() <= 2401.0, "one sample per station and window");
}

void TestFoxElasticCycle::theWindowIsNeverShorterThanARealAnswer()
{
    Cycle c;
    for (int i = 0; i < 10; ++i) {
        c.setExpected({"AA1AA"});
        c.txEnded(i * 20000LL);
        c.decodeHeard("AA1AA", i * 20000LL + 1500, true);   // implausibly early
        c.dispatched();
    }
    c.setExpected({"AA1AA"});
    c.txEnded(500000);
    c.decodeHeard("AA1AA", 500000 + 1500, true);
    QVERIFY2(c.goAtMs() - 500000 >= 3000, "never start before 3000 ms");
}

void TestFoxElasticCycle::windowNeverExceedsTheCap()
{
    Config cfg;
    cfg.maxWindowMs = 4000;
    Cycle c(cfg);
    for (int i = 0; i < 6; ++i) {
        c.setExpected({"SLOW"});
        c.txEnded(i * 30000LL);
        c.decodeHeard("SLOW", i * 30000LL + 9000 - 1);   // arrives after the cap, still learned
        c.dispatched();
    }
    c.setExpected({"SLOW"});
    c.txEnded(1000000);
    QVERIFY(c.goAtMs() <= 1000000LL + 4000);
}

void TestFoxElasticCycle::dispatchClosesTheWindow()
{
    Cycle c;
    c.txEnded(0);
    QVERIFY(c.windowOpen());
    c.dispatched();
    QVERIFY(!c.windowOpen());
    QCOMPARE(c.goAtMs(), 0LL);
    // a late decode of a window that is not open must not corrupt the state
    c.decodeHeard("AA1AA", 99999);
    QCOMPARE(c.pendingCount(), 0);
}

void TestFoxElasticCycle::elasticCycleBeatsTheGridAndNeverCutsAnAnswer()
{
    // Timeline: our frame lasts 2472 ms; each of 3 stations answers
    // 2700..3300 ms after it ended (jitter), station 3 is occasionally absent.
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> delay(2700, 3300);
    std::uniform_int_distribution<int> coin(0, 9);
    Cycle c;
    long long now = 0;
    long long sumCycle = 0;
    int cycles = 0;
    int cut = 0;
    for (int i = 0; i < 300; ++i) {
        c.setExpected({"AA1AA", "BB2BB", "CC3CC"});
        long long const start = now;
        long long const end = start + 2472;
        c.txEnded(end);
        struct Ev { long long t; const char* call; };
        std::vector<Ev> evs;
        long long lastAnswer = end;
        evs.push_back({end + delay(rng), "AA1AA"});
        evs.push_back({end + delay(rng), "BB2BB"});
        if (coin(rng) != 0) {
            evs.push_back({end + delay(rng), "CC3CC"});
        }
        for (auto const& e : evs) {
            lastAnswer = std::max(lastAnswer, e.t);
        }
        // advance in 20 ms polls, delivering decodes on time
        long long t = end;
        for (;;) {
            for (auto const& e : evs) {
                if (e.t >= t && e.t < t + 20) {
                    c.decodeHeard(e.call, e.t);
                }
            }
            if (c.shouldGo(t)) {
                break;
            }
            t += 20;
        }
        if (t + 20 < lastAnswer) {      // 20 ms = the test's polling step
            ++cut;
        }
        c.dispatched();
        sumCycle += t - start;
        ++cycles;
        now = t;
    }
    double const mean = static_cast<double>(sumCycle) / cycles;
    qInfo() << "mean elastic cycle (ms):" << mean << " cut answers:" << cut << " of" << cycles;
    QVERIFY2(mean < 6400.0, "must beat the 7500 ms grid cycle by a clear margin");
    QCOMPARE(cut, 0);
}

QTEST_APPLESS_MAIN(TestFoxElasticCycle)
#include "test_fox_elastic_cycle.moc"
