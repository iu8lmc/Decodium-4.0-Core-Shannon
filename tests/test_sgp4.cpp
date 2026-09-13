#include <QtTest>

#include <cmath>

#include "src/services/satellite/sgp4/SGP4.h"

class TestSgp4 final : public QObject
{
    Q_OBJECT

private slots:
    void propagatesCurrentTwoLineElement()
    {
        char line1[130] = "1 07530U 74089B   26213.31045441 -.00000039  00000+0  45656-4 0  9991";
        char line2[130] = "2 07530 101.9912 227.0279 0012547  46.4987  23.7609 12.53698890366177";
        decodium_sgp4::elsetrec satellite {};
        double startMfe = 0.0;
        double stopMfe = 0.0;
        double deltaMfe = 0.0;
        decodium_sgp4::SGP4Funcs::twoline2rv(
            line1, line2, 'c', 'e', 'i', decodium_sgp4::wgs84,
            startMfe, stopMfe, deltaMfe, satellite);
        QCOMPARE(satellite.error, 0);

        double position[3] {0.0, 0.0, 0.0};
        double velocity[3] {0.0, 0.0, 0.0};
        QVERIFY(decodium_sgp4::SGP4Funcs::sgp4(satellite, 0.0, position, velocity));
        double const radius = std::sqrt(position[0] * position[0]
                                        + position[1] * position[1]
                                        + position[2] * position[2]);
        double const speed = std::sqrt(velocity[0] * velocity[0]
                                       + velocity[1] * velocity[1]
                                       + velocity[2] * velocity[2]);
        QVERIFY(std::isfinite(radius));
        QVERIFY(std::isfinite(speed));
        QVERIFY(radius > 6378.0);
        QVERIFY(radius < 50000.0);
        QVERIFY(speed > 0.1);
    }
};

QTEST_APPLESS_MAIN(TestSgp4)

#include "test_sgp4.moc"
