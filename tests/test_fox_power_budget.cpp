#include <QtTest>

#include <numeric>

#include "src/bridge/FoxPowerBudget.h"

using namespace decodium::foxbudget;

namespace {
double sum(std::vector<double> const& v) { return std::accumulate(v.begin(), v.end(), 0.0); }
}

class TestFoxPowerBudget final : public QObject
{
    Q_OBJECT

private slots:
    void strongStationsAllFitAndShareTheBudget();
    void gainsAlwaysSumToOne();
    void aWeakStationIsNotAddedToACrowd();
    void aWeakStationFirstGetsTheFrameAlone();
    void agingServesTheDeferredFirst();
    void maxStreamsIsRespected();
    void everyChosenStationKeepsItsMargin();
    void unmeasuredLinksCostMore();
    void nothingToSendGivesAnEmptyAllocation();
    void twoStationsWithoutAnyDataShareAFrame();
    void twoHeardButUnmeasuredStationsFitInAnFt8Frame();
};

void TestFoxPowerBudget::strongStationsAllFitAndShareTheBudget()
{
    Config cfg;
    // links of +5 dB on a full-scale stream: need only ~ -13.5+3-5 = -15.5 dB (0.17)
    std::vector<Candidate> c;
    for (int i = 0; i < 5; ++i) c.push_back({i, 5.0, true, 0});
    Allocation a = allocate(c, cfg);
    QCOMPARE(int(a.ids.size()), 5);
    QCOMPARE(int(a.deferred.size()), 0);
    for (double g : a.gains) QVERIFY(std::abs(g - 0.2) < 1e-9);
}

void TestFoxPowerBudget::gainsAlwaysSumToOne()
{
    Config cfg;
    std::vector<Candidate> c {{1, -2.0, true, 0}, {2, 4.0, true, 0}, {3, 0.0, false, 0}};
    Allocation a = allocate(c, cfg);
    QVERIFY(!a.ids.empty());
    QVERIFY(std::abs(sum(a.gains) - 1.0) < 1e-9);
}

void TestFoxPowerBudget::aWeakStationIsNotAddedToACrowd()
{
    Config cfg;
    // station 9 needs more than the whole budget: -13.5+3-(-12) = +1.5 dB -> 1.19
    std::vector<Candidate> c {{1, 5.0, true, 0}, {2, 5.0, true, 0}, {9, -12.0, true, 0}};
    Allocation a = allocate(c, cfg);
    QCOMPARE(int(a.ids.size()), 2);
    QCOMPARE(a.deferred.front(), 9);
}

void TestFoxPowerBudget::aWeakStationFirstGetsTheFrameAlone()
{
    Config cfg;
    std::vector<Candidate> c {{9, -12.0, true, 0}, {1, 5.0, true, 0}};
    Allocation a = allocate(c, cfg);
    QCOMPARE(int(a.ids.size()), 1);
    QCOMPARE(a.ids.front(), 9);
    QCOMPARE(a.gains.front(), 1.0);
    QCOMPARE(a.deferred.front(), 1);
}

void TestFoxPowerBudget::agingServesTheDeferredFirst()
{
    Config cfg;
    std::vector<Candidate> c {{1, 5.0, true, 0}, {2, 5.0, true, 0}, {9, -12.0, true, 2}};
    Allocation a = allocate(c, cfg);
    QCOMPARE(a.ids.front(), 9);
    QCOMPARE(int(a.ids.size()), 1);
}

void TestFoxPowerBudget::maxStreamsIsRespected()
{
    Config cfg;
    cfg.maxStreams = 3;
    std::vector<Candidate> c;
    for (int i = 0; i < 8; ++i) c.push_back({i, 12.0, true, 0});
    Allocation a = allocate(c, cfg);
    QCOMPARE(int(a.ids.size()), 3);
    QCOMPARE(int(a.deferred.size()), 5);
}

void TestFoxPowerBudget::everyChosenStationKeepsItsMargin()
{
    Config cfg;
    std::vector<Candidate> c {{1, -4.0, true, 0}, {2, 2.0, true, 0}, {3, 6.0, true, 0}, {4, 0.0, true, 0}};
    Allocation a = allocate(c, cfg);
    for (size_t i = 0; i < a.ids.size(); ++i) {
        Candidate const* src = nullptr;
        for (auto const& x : c) if (x.id == a.ids[i]) src = &x;
        double const snr = src->linkDb + 20.0 * std::log10(a.gains[i]);
        QVERIFY2(snr >= cfg.thresholdDb + cfg.marginDb - 1e-9, "stream below its target SNR");
    }
}

void TestFoxPowerBudget::unmeasuredLinksCostMore()
{
    Config cfg;
    Candidate m {1, 0.0, true, 0};
    Candidate e {1, 0.0, false, 0};
    QVERIFY(neededGain(e, cfg) > neededGain(m, cfg));
}

void TestFoxPowerBudget::nothingToSendGivesAnEmptyAllocation()
{
    Allocation a = allocate({}, Config {});
    QVERIFY(a.ids.empty());
    QVERIFY(a.gains.empty());
}

void TestFoxPowerBudget::twoStationsWithoutAnyDataShareAFrame()
{
    // Stations picked by a click have no figure for their link: on the air
    // (24/10) the default made each of them need more than the whole budget,
    // so they were served one per frame and ran out of retries.
    Config cfg;
    std::vector<Candidate> c {Candidate(1, 0.0, false, 0, true), Candidate(2, 0.0, false, 0, true), Candidate(3, 0.0, false, 0, true)};
    Allocation a = allocate(c, cfg);
    QCOMPARE(int(a.ids.size()), 2);
    QCOMPARE(int(a.deferred.size()), 1);
    QVERIFY(std::abs(a.gains[0] - 0.5) < 1e-9);
}

void TestFoxPowerBudget::twoHeardButUnmeasuredStationsFitInAnFt8Frame()
{
    // 25/10/2026, on the air: with a 6 dB margin a station heard at -10 dB asked
    // 0.63 of an FT8 frame and nobody else fitted. With 4 dB it asks 0.501, and a 5% tolerance lets two share the frame.
    Config cfg;
    cfg.thresholdDb = -20.0;
    std::vector<Candidate> c {Candidate(1, -10.0, false, 0, false), Candidate(2, -10.0, false, 0, false)};
    Allocation a = allocate(c, cfg);
    QCOMPARE(int(a.ids.size()), 2);
    QVERIFY(neededGain(c[0], cfg) <= 0.52);
}

QTEST_APPLESS_MAIN(TestFoxPowerBudget)
#include "test_fox_power_budget.moc"
