// WinKeyer: i byte del protocollo K1EL.
#include "WinKeyer.h"

#include <QTest>

using namespace decodium::cw;

class TestWinKeyer : public QObject {
    Q_OBJECT

private slots:
    void bytes()
    {
        QCOMPARE(winkeyer::hostOpen(), QByteArray("\x00\x02", 2));
        QCOMPARE(winkeyer::hostClose(), QByteArray("\x00\x03", 2));
        QCOMPARE(winkeyer::speed(28), QByteArray("\x02\x1c", 2));
        QCOMPARE(winkeyer::speed(200), QByteArray("\x02\x63", 2));   // al massimo 99
        QCOMPARE(winkeyer::clearBuffer(), QByteArray("\x0a", 1));
        QCOMPARE(winkeyer::textBytes("cq test iu8lmc/p 5nn?"), QByteArray("CQ TEST IU8LMC/P 5NN?"));
        // Quello che il WinKeyer non conosce non passa: niente comandi per sbaglio.
        QCOMPARE(winkeyer::textBytes(QString::fromUtf8("à\x01{B}")), QByteArray("B"));
    }

    void openMissingPort()
    {
        WinKeyer wk;
        QVERIFY(!wk.open("COM199"));
        QVERIFY(!wk.isOpen());
    }
};

QTEST_GUILESS_MAIN(TestWinKeyer)
#include "test_winkeyer.moc"
