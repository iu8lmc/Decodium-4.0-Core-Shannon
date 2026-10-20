#include <QtTest/QtTest>

#include "src/bridge/AlcCalibrationMath.h"

class TestAlcCalibration final : public QObject {
    Q_OBJECT

private slots:
    void highAlcIncreasesAttenuation()
    {
        // The regression reported in issue #98: 141 -> 161, not 121.
        QCOMPARE(decodium::alc::adjustedAttenuation(141.0, 57.0), 161.0);
    }

    void lowOrZeroAlcReducesAttenuation()
    {
        // A valid zero ALC reading is useful telemetry, not a stalled state.
        QCOMPARE(decodium::alc::adjustedAttenuation(141.0, -20.0), 121.0);
    }

    void attenuationRemainsWithinPhysicalBounds()
    {
        QCOMPARE(decodium::alc::adjustedAttenuation(0.0, -100.0), 0.0);
        QCOMPARE(decodium::alc::adjustedAttenuation(450.0, 100.0), 450.0);
        QCOMPARE(decodium::alc::adjustedAttenuation(-10.0, 0.0), 0.0);
        QCOMPARE(decodium::alc::adjustedAttenuation(500.0, 0.0), 450.0);
    }

    void limitClassificationUsesAttenuationDirection()
    {
        QCOMPARE(decodium::alc::limitReached(0.0, 0.0, -20.0),
                 decodium::alc::DriveLimit::MaxDrive);
        QCOMPARE(decodium::alc::limitReached(450.0, 450.0, 80.0),
                 decodium::alc::DriveLimit::MinDrive);
        QCOMPARE(decodium::alc::limitReached(141.0, 161.0, 57.0),
                 decodium::alc::DriveLimit::None);
    }
};

QTEST_MAIN(TestAlcCalibration)
#include "test_alc_calibration.moc"
