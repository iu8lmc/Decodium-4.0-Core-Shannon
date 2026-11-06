#include <QtTest>
#include "src/bridge/NativeDecodeSecondaryWork.h"

class TestNativeDecodeSecondaryWork : public QObject {
    Q_OBJECT
private slots:
    void myCallHiddenByCqOnly_data() {
        QTest::addColumn<QString>("message");
        QTest::newRow("incoming-call") << QStringLiteral("IU3VGK ON7GB JO21");
        QTest::newRow("report") << QStringLiteral("IU3VGK ON7GB R-06");
        QTest::newRow("signoff") << QStringLiteral("IU3VGK ON7GB 73");
    }
    void myCallHiddenByCqOnly() {
        QFETCH(QString, message);
        decodium::NativeDecodeSecondaryWork work;
        work.entry = {{"message", message}, {"isMyCall", true}, {"isCQ", false}};
        work.key = "070530|1092|" + message;
        work.route(false, false); // Valid unique row hidden by CQ Only.
        QVERIFY(work.hasWork());
        QVERIFY(work.playAlert);
        QCOMPARE(work.entry.value("message").toString(), message);
        QVERIFY(work.entry.value("isMyCall").toBool());
        QVERIFY(work.publishPsk);
        QVERIFY(!work.updateActiveStation);
        QVERIFY(!work.updateWorldMap);
        QVERIFY(!work.sendUdp);
        QVERIFY(!work.reportDecodeTiming);
    }
    void visibleCqRetainsSecondaryWork() {
        decodium::NativeDecodeSecondaryWork work;
        work.entry = {{"message", "CQ ON7GB JO21"}, {"isCQ", true}};
        work.route(true, false);
        QVERIFY(work.playAlert);
        QVERIFY(work.publishPsk);
        QVERIFY(work.updateActiveStation);
        QVERIFY(work.updateWorldMap);
        QVERIFY(work.sendUdp);
        QVERIFY(work.reportDecodeTiming);
    }
    void presentationDoesNotControlAlerts() {
        decodium::NativeDecodeSecondaryWork work;
        work.entry = {{"message", "CQ ON7GB JO21"}, {"isCQ", true}};
        work.route(false, false); // e.g. CQ hidden by My Call Only.
        QVERIFY(work.playAlert); // Actual CQ/Wanted settings still checked by bridge.
        work.route(true, false);
        QVERIFY(work.playAlert);
    }
    void deepDuringTxRemainsSilent() {
        decodium::NativeDecodeSecondaryWork work;
        work.entry = {{"message", "IU3VGK ON7GB R-06"}, {"isMyCall", true}};
        work.route(true, true);
        QVERIFY(!work.playAlert);
        QVERIFY(!work.publishPsk);
        QVERIFY(!work.sendUdp);
        QVERIFY(work.updateWorldMap);
        work.route(false, true);
        QVERIFY(!work.hasWork());
    }
    void unresolvedPeerNeverSpotted() {
        decodium::NativeDecodeSecondaryWork work;
        work.entry = {{"message", "IU3VGK <...> -06"}, {"isMyCall", true},
                      {"hasUnresolvedPeer", true}};
        work.route(false, false);
        QVERIFY(work.playAlert);
        QVERIFY(!work.publishPsk);
        QVERIFY(!work.updateActiveStation);
    }
};

QTEST_GUILESS_MAIN(TestNativeDecodeSecondaryWork)
#include "test_native_decode_secondary_work.moc"
