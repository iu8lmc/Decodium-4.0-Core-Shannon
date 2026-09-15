#include <QtTest>

#include "src/rtl/RtlSdrRfSpectrum.h"

#include <algorithm>
#include <cmath>

class TestRtlSdrRfSpectrum final : public QObject
{
    Q_OBJECT

private slots:
    void complexToneAppearsAtItsRfOffset();
};

void TestRtlSdrRfSpectrum::complexToneAppearsAtItsRfOffset()
{
    constexpr int sampleRate = 240000;
    constexpr quint32 centreHz = 100100000U;
    constexpr float toneOffsetHz = 30000.0f;
    QVector<short> iq(RtlSdrRfSpectrum::kFftSize * 2);
    for (int sample = 0; sample < RtlSdrRfSpectrum::kFftSize; ++sample) {
        const float phase = 2.0f * 3.14159265358979323846f * toneOffsetHz
            * static_cast<float>(sample) / static_cast<float>(sampleRate);
        iq[sample * 2] = static_cast<short>(qRound(std::cos(phase) * 24000.0f));
        iq[sample * 2 + 1] = static_cast<short>(qRound(std::sin(phase) * 24000.0f));
    }

    const RtlSdrRfSpectrum::Frame frame =
        RtlSdrRfSpectrum::compute(iq, sampleRate, centreHz);
    QCOMPARE(frame.values.size(), RtlSdrRfSpectrum::kFftSize);
    QCOMPARE(qRound(frame.frequencyMinHz), qRound(centreHz - sampleRate / 2.0));
    QCOMPARE(qRound(frame.frequencyMaxHz), qRound(centreHz + sampleRate / 2.0));

    const auto peak = std::max_element(frame.values.cbegin(), frame.values.cend());
    QVERIFY(peak != frame.values.cend());
    const int peakIndex = static_cast<int>(std::distance(frame.values.cbegin(), peak));
    const float peakFrequency = frame.frequencyMinHz
        + (static_cast<float>(peakIndex) + 0.5f)
              * (frame.frequencyMaxHz - frame.frequencyMinHz) / frame.values.size();
    QVERIFY2(std::abs(peakFrequency - (centreHz + toneOffsetHz)) < 150.0f,
             "the FFT-shifted RF peak must retain its positive frequency offset");
}

QTEST_MAIN(TestRtlSdrRfSpectrum)
#include "test_rtlsdr_rf_spectrum.moc"
