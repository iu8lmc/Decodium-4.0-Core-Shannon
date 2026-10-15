#include "CwKeyer.h"
#include "CwTiming.h"

#include <QTest>

using namespace decodium::cw;

class TestCwTiming : public QObject {
    Q_OBJECT
private slots:
    void aSingleDotAndDash()
    {
        const auto e = timelineFor("E", 20);                   // punto = 60 ms
        QCOMPARE(e.size(), 2);
        QVERIFY(e[0].down && e[0].deltaMs == 0);
        QVERIFY(!e[1].down && e[1].deltaMs == 60);
        const auto t = timelineFor("T", 20);
        QCOMPARE(int(t[1].deltaMs), 180);
    }

    void gapsBetweenLettersAndWords()
    {
        const auto et = timelineFor("ET", 20);                 // 3 punti fra le lettere
        QCOMPARE(et.size(), 4);
        QCOMPARE(int(et[2].deltaMs), 180);
        const auto ee = timelineFor("E E", 20);                // 7 punti fra le parole
        QCOMPARE(int(ee[2].deltaMs), 420);
    }

    void insideALetterThereIsOneDot()
    {
        const auto a = timelineFor("A", 20);                   // .-
        QCOMPARE(a.size(), 4);
        QCOMPARE(int(a[2].deltaMs), 60);
    }

    void alwaysEndsWithTheKeyUp()
    {
        for (const char* s : {"CQ DX", "73", "TEST DE IU8LMC", "E", "?"}) {
            const auto e = timelineFor(s, 25);
            QVERIFY(!e.isEmpty());
            QVERIFY(e.first().down);
            QVERIFY(!e.last().down);
            for (int i = 0; i < e.size(); ++i)                 // giu' e su si alternano
                QCOMPARE(e[i].down, i % 2 == 0);
        }
    }

    void durationMatchesTheSerialKeyer()
    {
        for (const char* s : {"CQ CQ DE IU8LMC", "5NN 042", "TU 73 GL"})
            for (int wpm : {5, 12, 20, 35, 60})
                QCOMPARE(totalMs(timelineFor(s, wpm)), CwKeyer::millisFor(s, wpm));
    }

    void unknownCharactersAndEmptyTextGiveNothing()
    {
        QVERIFY(timelineFor("", 20).isEmpty());
        QVERIFY(timelineFor("   ", 20).isEmpty());
        QCOMPARE(timelineFor("~~E~~", 20).size(), 2);
    }

    void speedIsClamped()
    {
        QCOMPARE(int(timelineFor("E", 1)[1].deltaMs), 1200 / 5);
        QCOMPARE(int(timelineFor("E", 500)[1].deltaMs), 1200 / 60);
    }

    void windowsCutOnlyBetweenPairs()
    {
        QVector<KeyEvent> all = timelineFor("CQ CQ DE IU8LMC", 20);
        const int whole = totalMs(all);
        QVector<KeyEvent> rest = all;
        int sum = 0;
        int pieces = 0;
        while (!rest.isEmpty()) {
            const QVector<KeyEvent> piece = takeWindow(rest, 400);
            QVERIFY(!piece.isEmpty());
            QCOMPARE(piece.size() % 2, 0);
            QVERIFY(piece.first().down);
            QVERIFY(!piece.last().down);                       // mai un pezzo a tasto giu'
            sum += totalMs(piece);
            ++pieces;
        }
        QCOMPARE(sum, whole);                                  // niente si perde, niente si duplica
        QVERIFY(pieces > 3);
    }

    void aWindowSmallerThanOneElementStillMakesProgress()
    {
        QVector<KeyEvent> e = timelineFor("T", 5);             // 720 ms di linea
        const auto piece = takeWindow(e, 10);
        QCOMPARE(piece.size(), 2);
        QVERIFY(e.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCwTiming)
#include "test_cw_timing.moc"
