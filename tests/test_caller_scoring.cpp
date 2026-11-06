#include <QtTest>
#include "Sequencer/CallerScoring.hpp"

using decodium::seq::CallerScoreInput;

class TestCallerScoring : public QObject {
    Q_OBJECT
private slots:
    void stronger_signal_wins() {
        CallerScoreInput weak;   weak.snrKnown = true;   weak.snrDb = -20;
        CallerScoreInput strong; strong.snrKnown = true; strong.snrDb = 0;
        QVERIFY(decodium::seq::callerScore(strong) > decodium::seq::callerScore(weak));
        QCOMPARE(decodium::seq::pickBestCaller({weak, strong}), 1);
    }
    void snr_is_clamped() {
        CallerScoreInput a; a.snrKnown = true; a.snrDb = 30;
        CallerScoreInput b; b.snrKnown = true; b.snrDb = 10;
        QCOMPARE(decodium::seq::callerScore(a), decodium::seq::callerScore(b));
        CallerScoreInput c; c.snrKnown = true; c.snrDb = -60;
        CallerScoreInput d; d.snrKnown = true; d.snrDb = -24;
        QCOMPARE(decodium::seq::callerScore(c), decodium::seq::callerScore(d));
    }
    void unknown_snr_uses_default() {
        CallerScoreInput unknown;
        CallerScoreInput assumed; assumed.snrKnown = true; assumed.snrDb = -10;
        QCOMPARE(decodium::seq::callerScore(unknown), decodium::seq::callerScore(assumed));
    }
    void new_entity_beats_a_slightly_stronger_signal() {
        CallerScoreInput common; common.snrKnown = true; common.snrDb = -2;
        CallerScoreInput rare;   rare.snrKnown = true;   rare.snrDb = -10; rare.newDxccEver = true;
        QCOMPARE(decodium::seq::pickBestCaller({common, rare}), 1);
    }
    void new_on_any_band_outranks_new_on_this_band_only() {
        CallerScoreInput band; band.newDxccBand = true;
        CallerScoreInput ever; ever.newDxccEver = true; ever.newDxccBand = true;
        QVERIFY(decodium::seq::callerScore(ever) > decodium::seq::callerScore(band));
        // The two bonuses never stack.
        CallerScoreInput plain;
        QCOMPARE(decodium::seq::callerScore(ever) - decodium::seq::callerScore(plain), 40.0);
    }
    void already_worked_on_band_is_demoted() {
        CallerScoreInput fresh;  fresh.snrKnown = true;  fresh.snrDb = -12;
        CallerScoreInput dupe;   dupe.snrKnown = true;   dupe.snrDb = 0;
        dupe.workedBand = true; dupe.workedEver = true;
        QCOMPARE(decodium::seq::pickBestCaller({dupe, fresh}), 1);
    }
    void worked_elsewhere_is_demoted_less_than_worked_here() {
        CallerScoreInput other; other.workedEver = true;
        CallerScoreInput here;  here.workedEver = true; here.workedBand = true;
        QVERIFY(decodium::seq::callerScore(other) > decodium::seq::callerScore(here));
        // The two penalties never stack.
        CallerScoreInput plain;
        QCOMPARE(decodium::seq::callerScore(here) - decodium::seq::callerScore(plain), -35.0);
    }
    void waiting_longer_prevents_starvation() {
        CallerScoreInput fresh; fresh.snrKnown = true; fresh.snrDb = -4;  fresh.waitMs = 0;
        CallerScoreInput old;   old.snrKnown = true;   old.snrDb = -12;   old.waitMs = 90000;
        QCOMPARE(decodium::seq::pickBestCaller({fresh, old}), 1);
        // The wait bonus saturates, it never grows without bound.
        CallerScoreInput older = old; older.waitMs = 900000;
        QCOMPARE(decodium::seq::callerScore(older), decodium::seq::callerScore(old));
    }
    void tie_keeps_arrival_order() {
        CallerScoreInput a, b, c;
        QCOMPARE(decodium::seq::pickBestCaller({a, b, c}), 0);
        QCOMPARE(decodium::seq::rankCallers({a, b, c}), (QVector<int> {0, 1, 2}));
    }
    void ranking_is_stable_and_descending() {
        CallerScoreInput weak;   weak.snrKnown = true;   weak.snrDb = -20;
        CallerScoreInput mid1;   mid1.snrKnown = true;   mid1.snrDb = -8;
        CallerScoreInput mid2;   mid2.snrKnown = true;   mid2.snrDb = -8;
        CallerScoreInput strong; strong.snrKnown = true; strong.snrDb = 5;
        QCOMPARE(decodium::seq::rankCallers({weak, mid1, mid2, strong}),
                 (QVector<int> {3, 1, 2, 0}));
    }
    void operator_pick_beats_every_automatic_caller() {
        CallerScoreInput best; best.snrKnown = true; best.snrDb = 10; best.newDxccEver = true;
        best.waitMs = 90000;
        CallerScoreInput picked; picked.pinned = true; picked.snrKnown = true; picked.snrDb = -24;
        picked.workedBand = true; picked.workedEver = true;   // weak AND a dupe: still the operator's choice
        QCOMPARE(decodium::seq::pickBestCaller({best, picked}), 1);
    }
    void operator_picks_keep_their_order() {
        CallerScoreInput first;  first.pinned = true;  first.snrKnown = true;  first.snrDb = -24;
        CallerScoreInput second; second.pinned = true; second.snrKnown = true; second.snrDb = 10;
        CallerScoreInput auto1;  auto1.snrKnown = true; auto1.snrDb = 10;
        QCOMPARE(decodium::seq::rankCallers({auto1, first, second}), (QVector<int> {1, 2, 0}));
    }
    void empty_list() {
        QCOMPARE(decodium::seq::pickBestCaller({}), -1);
        QVERIFY(decodium::seq::rankCallers({}).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestCallerScoring)
#include "test_caller_scoring.moc"
