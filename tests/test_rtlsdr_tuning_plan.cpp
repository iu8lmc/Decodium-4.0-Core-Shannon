#include <QtTest/QtTest>

#include "../src/rtl/RtlSdrTuningPlan.h"

class TestRtlSdrTuningPlan final : public QObject
{
    Q_OBJECT

private slots:
    void directRfUsesDialAndDcOffset()
    {
        decodium::rtl_sdr::TuningRequest request;
        request.dialFrequencyHz = 7074000;
        request.sampleRate = 240000;

        const auto plan = decodium::rtl_sdr::makeTuningPlan(request);
        QCOMPARE(plan.selectedInputFrequencyHz, 7074000);
        QCOMPARE(plan.hardwareCenterFrequencyHz, 7134000U);
        QCOMPARE(plan.channelOffsetHz, -60000);
        QCOMPARE(plan.displayCenterFrequencyHz, 7134000U);
        QCOMPARE(plan.displaySelectedFrequencyHz, 7074000U);
        QVERIFY(!plan.ifEnabled);
        QVERIFY(!plan.spectrumInverted);
    }

    void fixedIfKeepsDialScaleAndAppliesUsbShift()
    {
        decodium::rtl_sdr::TuningRequest request;
        request.dialFrequencyHz = 7074000;
        request.sampleRate = 240000;
        request.ifEnabled = true;
        request.ifFrequencyHz = 8830000;
        request.usbShiftHz = 1500;
        request.sideband = decodium::rtl_sdr::IfSideband::Usb;

        const auto plan = decodium::rtl_sdr::makeTuningPlan(request);
        QCOMPARE(plan.selectedInputFrequencyHz, 8831500);
        QCOMPARE(plan.hardwareCenterFrequencyHz, 8891500U);
        QCOMPARE(plan.channelOffsetHz, -60000);
        QCOMPARE(plan.displayCenterFrequencyHz, 7134000U);
        QCOMPARE(plan.displaySelectedFrequencyHz, 7074000U);
        QVERIFY(plan.ifEnabled);

        request.dialFrequencyHz = 14074000;
        const auto secondBand = decodium::rtl_sdr::makeTuningPlan(request);
        QCOMPARE(secondBand.selectedInputFrequencyHz, 8831500);
        QCOMPARE(secondBand.hardwareCenterFrequencyHz, 8891500U);
        QCOMPARE(secondBand.displayCenterFrequencyHz, 14134000U);
        QCOMPARE(secondBand.displaySelectedFrequencyHz, 14074000U);
    }

    void lsbUsesIndependentShift()
    {
        decodium::rtl_sdr::TuningRequest request;
        request.dialFrequencyHz = 7074000;
        request.ifEnabled = true;
        request.ifFrequencyHz = 8830000;
        request.usbShiftHz = 1500;
        request.lsbShiftHz = -1500;
        request.sideband = decodium::rtl_sdr::IfSideband::Lsb;

        const auto plan = decodium::rtl_sdr::makeTuningPlan(request);
        QCOMPARE(plan.selectedInputFrequencyHz, 8828500);
        QCOMPARE(plan.hardwareCenterFrequencyHz, 8888500U);
        QCOMPARE(plan.channelOffsetHz, -60000);
    }

    void invertedIfReversesLogicalCentreOffset()
    {
        decodium::rtl_sdr::TuningRequest request;
        request.dialFrequencyHz = 7074000;
        request.ifEnabled = true;
        request.ifFrequencyHz = 8830000;
        request.usbShiftHz = 1500;
        request.spectrumInverted = true;

        const auto plan = decodium::rtl_sdr::makeTuningPlan(request);
        QCOMPARE(plan.hardwareCenterFrequencyHz, 8891500U);
        QCOMPARE(plan.displayCenterFrequencyHz, 7014000U);
        QCOMPARE(plan.displaySelectedFrequencyHz, 7074000U);
        QVERIFY(plan.spectrumInverted);
    }
};

QTEST_MAIN(TestRtlSdrTuningPlan)
#include "test_rtlsdr_tuning_plan.moc"
