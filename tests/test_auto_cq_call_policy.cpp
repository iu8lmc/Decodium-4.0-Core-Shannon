#include <QtTest>
#include "Sequencer/AutoCqCallPolicy.hpp"

class TestAutoCqCallPolicy : public QObject {
    Q_OBJECT
private slots:
    void genericAndBurstPausesOverlap() {
        // Completed at 10000 ms: generic 1s, two FT2 periods 7.5s.
        QVERIFY(decodium::autoCqPausePending(12000, 11000, 17500));
        QVERIFY(!decodium::autoCqPausePending(17500, 11000, 17500));
        // A longer generic pause wins, not the sum of both delays.
        QVERIFY(decodium::autoCqPausePending(17500, 20000, 17500));
        QVERIFY(!decodium::autoCqPausePending(20000, 20000, 17500));
        QVERIFY(!decodium::autoCqPausePending(10000, 0, 0));
        QVERIFY(!decodium::autoCqPausePending(11000, 11000, 0));
    }
    void completedBudgetExcludesRepliesAndAborts() {
        int count = 0;
        QVERIFY(!decodium::completeAutoCq(false, false, 5, count));
        QVERIFY(!decodium::completeAutoCq(true, true, 5, count));
        QCOMPARE(count, 0);
        for (int i = 1; i <= 5; ++i) {
            QCOMPARE(decodium::completeAutoCq(true, false, 5, count), i == 5);
            QCOMPARE(count, i);
            QVERIFY(!decodium::completeAutoCq(false, false, 5, count));
            QCOMPARE(count, i);
        }
    }
    void unlimitedBudgetAndCustomCalls() {
        int count = 0;
        for (int i = 0; i < 20; ++i)
            QVERIFY(!decodium::completeAutoCq(
                decodium::isAutoCqCall(true, 6, true, false, "TEST VY2XT FN86"),
                false, 0, count));
        QCOMPARE(count, 20);
    }
    void customAndStandardCalls() {
        for (const auto& text : {"CQ VY2XT FN86", "QRZ VY2XT", "TEST VY2XT FN86"})
            QVERIFY(decodium::isAutoCqCall(true, 6, true, false, text));
    }
    void repliesAreNotCalls() {
        for (int tx = 1; tx < 6; ++tx)
            QVERIFY(!decodium::isAutoCqCall(true, tx, true, false, "TEST VY2XT FN86"));
        QVERIFY(!decodium::isAutoCqCall(true, 6, true, true, "CQ VY2XT FN86"));
        QVERIFY(!decodium::isAutoCqCall(true, 6, false, false, "CQ VY2XT FN86"));
    }
    void inactiveAndEmptyAreNotCalls() {
        QVERIFY(!decodium::isAutoCqCall(false, 6, true, false, "TEST VY2XT FN86"));
        QVERIFY(!decodium::isAutoCqCall(true, 6, true, false, "  "));
    }
};
QTEST_GUILESS_MAIN(TestAutoCqCallPolicy)
#include "test_auto_cq_call_policy.moc"
