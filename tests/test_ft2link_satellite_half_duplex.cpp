#include <QtTest>

#include "src/bridge/Ft2LinkSatelliteHalfDuplex.h"

class TestFt2LinkSatelliteHalfDuplex final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsCrossBandQO100Configuration()
    {
        decodium::ft2link_satellite::HalfDuplexConfiguration configuration;
        configuration.enabled = true;
        configuration.rxDialHz = 10489500000LL;
        configuration.txDialHz = 2400050000LL;
        configuration.settleMs = 900;

        QVERIFY2(decodium::ft2link_satellite::isValid(configuration),
                 qPrintable(decodium::ft2link_satellite::validationError(configuration)));
    }

    void rejectsUnsafeOrIncompleteConfiguration()
    {
        decodium::ft2link_satellite::HalfDuplexConfiguration configuration;
        configuration.enabled = true;

        QVERIFY(!decodium::ft2link_satellite::isValid(configuration));
        QVERIFY(decodium::ft2link_satellite::validationError(configuration).contains(QStringLiteral("RX")));

        configuration.rxDialHz = 10489500000LL;
        configuration.txDialHz = configuration.rxDialHz;
        QVERIFY(!decodium::ft2link_satellite::isValid(configuration));
        QVERIFY(decodium::ft2link_satellite::validationError(configuration).contains(QStringLiteral("different")));
    }

    void boundsCatSettleDelay()
    {
        QCOMPARE(decodium::ft2link_satellite::normalizedSettleMs(1), 250);
        QCOMPARE(decodium::ft2link_satellite::normalizedSettleMs(900), 900);
        QCOMPARE(decodium::ft2link_satellite::normalizedSettleMs(9000), 5000);
    }
};

QTEST_MAIN(TestFt2LinkSatelliteHalfDuplex)
#include "test_ft2link_satellite_half_duplex.moc"
