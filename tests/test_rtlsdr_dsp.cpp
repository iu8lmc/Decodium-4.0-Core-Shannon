#include <QtTest>

#include "src/rtl/RtlSdrDsp.h"

#include <cmath>

namespace {
double toneMagnitude(const QVector<short>& samples, double frequencyHz,
                     double sampleRateHz, int firstSample)
{
    double inPhase = 0.0;
    double quadrature = 0.0;
    const int count = samples.size() - firstSample;
    if (count <= 0) return 0.0;
    for (int index = firstSample; index < samples.size(); ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * frequencyHz
            * (index - firstSample) / sampleRateHz;
        inPhase += samples.at(index) * std::cos(phase);
        quadrature += samples.at(index) * std::sin(phase);
    }
    return std::hypot(inPhase, quadrature) / count;
}

double rms(const QVector<short>& samples, int firstSample)
{
    if (samples.size() <= firstSample) return 0.0;
    long double sum = 0.0;
    for (int index = firstSample; index < samples.size(); ++index) {
        const long double value = samples.at(index);
        sum += value * value;
    }
    return std::sqrt(static_cast<double>(sum / (samples.size() - firstSample)));
}
}

class TestRtlSdrDsp final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsIntegerDecimation();
    void rejectsUnsupportedRate();
    void producesPcmForIqTone();
    void wideFmProducesReceiverAudioAndKeepsIq();
    void wideFmRejectsCarrierOffsetWithoutClipping();
    void enhancedSsbAudioRunsOnlyOnReceiverAudioPath();
    void ssbControlsStayOutOfWeakSignalDecoder();
    void ssbVoiceBandwidthAndNotchShapeOnlyListeningAudio();
    void ssbAgcAndNoiseReductionAreOptional();
    void ncoRecentresOffsetChannelBeforeDecoderPath();
    void ifSpectrumInversionConjugatesIq();
};

void TestRtlSdrDsp::acceptsIntegerDecimation()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000));
    QCOMPARE(dsp.decimationFactor(), 20);
    QCOMPARE(dsp.outputSampleRate(), 12000);
}

void TestRtlSdrDsp::rejectsUnsupportedRate()
{
    RtlSdrDsp dsp;
    QVERIFY(!dsp.configure(250000));
    QVERIFY(!RtlSdrDsp::isSupportedSampleRate(240000, 11025));
}

void TestRtlSdrDsp::producesPcmForIqTone()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000, 12000, 1.0));

    QByteArray iq;
    constexpr int samples = 24000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 1000.0 * index / 240000.0;
        const int value = qBound(0, qRound(127.5 + 50.0 * std::sin(phase)), 255);
        iq[2 * index] = static_cast<char>(value);
        iq[2 * index + 1] = static_cast<char>(128);
    }

    const QVector<short> pcm = dsp.process(reinterpret_cast<const unsigned char *>(iq.constData()), iq.size());
    QCOMPARE(pcm.size(), samples / 20);
    int peak = 0;
    for (int index = 100; index < pcm.size(); ++index) {
        peak = qMax(peak, std::abs(static_cast<int>(pcm.at(index))));
    }
    QVERIFY2(peak > 2000, "a valid RTL I/Q tone must survive DC removal and decimation");
}

void TestRtlSdrDsp::wideFmProducesReceiverAudioAndKeepsIq()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(960000, RtlSdrDsp::Demodulator::WideFm, 1.0));
    QVERIFY(!RtlSdrDsp::isSupportedSampleRateForDemodulator(
        240000, RtlSdrDsp::Demodulator::WideFm));

    QByteArray iq;
    constexpr int samples = 96000;
    iq.resize(samples * 2);
    double phase = 0.0;
    for (int index = 0; index < samples; ++index) {
        const double modulation = std::sin(2.0 * 3.14159265358979323846 * 1000.0
                                           * index / 960000.0);
        phase += 2.0 * 3.14159265358979323846 * 30000.0 * modulation / 960000.0;
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::sin(phase)), 255));
    }

    RtlSdrDsp::Result frame = dsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    QCOMPARE(frame.iq.size(), samples * 2);
    QCOMPARE(frame.decoderPcm.size(), 0);
    QCOMPARE(frame.audioPcm.size(), samples / 20);
    int peak = 0;
    for (int index = 500; index < frame.audioPcm.size(); ++index) {
        peak = qMax(peak, std::abs(static_cast<int>(frame.audioPcm.at(index))));
    }
    QVERIFY2(peak > 300, "FM discriminator output must reach the separate receiver-audio path");
}

void TestRtlSdrDsp::wideFmRejectsCarrierOffsetWithoutClipping()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(960000, RtlSdrDsp::Demodulator::WideFm, 3.3));

    QByteArray iq;
    constexpr int samples = 192000;
    iq.resize(samples * 2);
    double phase = 0.0;
    for (int index = 0; index < samples; ++index) {
        const double modulation = std::sin(2.0 * 3.14159265358979323846 * 1000.0
                                           * index / 960000.0);
        const double instantaneousHz = 60000.0 + 30000.0 * modulation;
        phase += 2.0 * 3.14159265358979323846 * instantaneousHz / 960000.0;
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 90.0 * std::sin(phase)), 255));
    }

    const RtlSdrDsp::Result frame = dsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), false);
    QCOMPARE(frame.audioPcm.size(), samples / 20);

    qint64 sum = 0;
    qint64 squareSum = 0;
    int peak = 0;
    constexpr int warmup = 2400;
    for (int index = warmup; index < frame.audioPcm.size(); ++index) {
        const int sample = frame.audioPcm.at(index);
        sum += sample;
        squareSum += static_cast<qint64>(sample) * sample;
        peak = qMax(peak, std::abs(sample));
    }
    const int count = frame.audioPcm.size() - warmup;
    const double mean = static_cast<double>(sum) / count;
    const double rms = std::sqrt(static_cast<double>(squareSum) / count);
    QVERIFY2(std::abs(mean) < rms * 0.10,
             "FM carrier mistuning must not become a large DC audio component");
    QVERIFY2(peak < 30000, "normal WFM audio must retain headroom instead of hard clipping");
    QVERIFY2(rms > 500.0, "the wanted FM modulation must remain audible after DC rejection");
}

void TestRtlSdrDsp::enhancedSsbAudioRunsOnlyOnReceiverAudioPath()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000, RtlSdrDsp::Demodulator::Usb, 1.0));

    QByteArray iq;
    constexpr int samples = 240000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 1200.0
            * index / 240000.0;
        iq[2 * index] = static_cast<char>(qBound(
            0, qRound(127.5 + 48.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(
            0, qRound(127.5 + 48.0 * std::sin(phase)), 255));
    }

    const RtlSdrDsp::Result frame = dsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    QCOMPARE(frame.iq.size(), samples * 2);
    QCOMPARE(frame.decoderPcm.size(), 0);
    QCOMPARE(frame.audioPcm.size(), samples / 5);

    int peak = 0;
    qint64 squareSum = 0;
    for (int index = 4800; index < frame.audioPcm.size(); ++index) {
        const int sample = frame.audioPcm.at(index);
        peak = qMax(peak, std::abs(sample));
        squareSum += static_cast<qint64>(sample) * sample;
    }
    const int count = frame.audioPcm.size() - 4800;
    const double rms = std::sqrt(static_cast<double>(squareSum) / count);
    QVERIFY2(peak < 30000, "enhanced RTL-SDR SSB audio must retain clipping headroom");
    QVERIFY2(rms > 800.0, "enhanced RTL-SDR SSB audio must retain an audible receiver signal");
}

void TestRtlSdrDsp::ssbControlsStayOutOfWeakSignalDecoder()
{
    QByteArray iq;
    constexpr int samples = 24000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 1000.0
            * index / 240000.0;
        iq[2 * index] = static_cast<char>(qBound(
            0, qRound(127.5 + 56.0 * std::sin(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(128);
    }

    RtlSdrDsp defaultDsp;
    RtlSdrDsp controlledDsp;
    QVERIFY(defaultDsp.configure(240000, RtlSdrDsp::Demodulator::WeakSignal, 1.0));
    QVERIFY(controlledDsp.configure(
        240000, RtlSdrDsp::Demodulator::WeakSignal, 1.0, 0, false,
        1800.0, RtlSdrDsp::SsbAgcMode::Medium, 1200,
        RtlSdrDsp::SsbNoiseReductionMode::Medium));

    const auto defaultPcm = defaultDsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), false).decoderPcm;
    const auto controlledPcm = controlledDsp.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), false).decoderPcm;
    QCOMPARE(controlledPcm, defaultPcm);
}

void TestRtlSdrDsp::ssbVoiceBandwidthAndNotchShapeOnlyListeningAudio()
{
    QByteArray iq;
    constexpr int samples = 240000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double lowPhase = 2.0 * 3.14159265358979323846 * 1100.0
            * index / 240000.0;
        const double highPhase = 2.0 * 3.14159265358979323846 * 3000.0
            * index / 240000.0;
        const double i = 0.20 * std::cos(lowPhase) + 0.20 * std::cos(highPhase);
        const double q = 0.20 * std::sin(lowPhase) + 0.20 * std::sin(highPhase);
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 100.0 * i), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 100.0 * q), 255));
    }

    auto receive = [&iq](double bandwidthHz, int notchHz) {
        RtlSdrDsp dsp;
        if (!dsp.configure(240000, RtlSdrDsp::Demodulator::Usb, 1.0, 0, false,
                           bandwidthHz, RtlSdrDsp::SsbAgcMode::Off, notchHz,
                           RtlSdrDsp::SsbNoiseReductionMode::Off)) {
            return QVector<short> {};
        }
        return dsp.processFrame(reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), false)
            .audioPcm;
    };
    const auto wide = receive(4000.0, 0);
    const auto narrow = receive(1800.0, 0);
    const auto notched = receive(4000.0, 1100);
    QCOMPARE(wide.size(), samples / 5);
    QCOMPARE(narrow.size(), samples / 5);
    QCOMPARE(notched.size(), samples / 5);

    constexpr int warmup = 4800;
    const double wideRatio = toneMagnitude(wide, 3000.0, 48000.0, warmup)
        / qMax(1.0, toneMagnitude(wide, 1100.0, 48000.0, warmup));
    const double narrowRatio = toneMagnitude(narrow, 3000.0, 48000.0, warmup)
        / qMax(1.0, toneMagnitude(narrow, 1100.0, 48000.0, warmup));
    QVERIFY2(narrowRatio < wideRatio * 0.35,
             "narrow SSB bandwidth must attenuate high listening audio");

    const double plainLow = toneMagnitude(wide, 1100.0, 48000.0, warmup);
    const double notchedLow = toneMagnitude(notched, 1100.0, 48000.0, warmup);
    QVERIFY2(notchedLow < plainLow * 0.35,
             "SSB notch must attenuate the configured audible whistle");
}

void TestRtlSdrDsp::ssbAgcAndNoiseReductionAreOptional()
{
    QByteArray iq;
    constexpr int samples = 240000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 1300.0
            * index / 240000.0;
        const double amplitude = 0.025;
        iq[2 * index] = static_cast<char>(qBound(
            0, qRound(127.5 + 100.0 * amplitude * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(
            0, qRound(127.5 + 100.0 * amplitude * std::sin(phase)), 255));
    }

    auto receive = [&iq](RtlSdrDsp::SsbAgcMode agcMode,
                         RtlSdrDsp::SsbNoiseReductionMode reduction) {
        RtlSdrDsp dsp;
        if (!dsp.configure(240000, RtlSdrDsp::Demodulator::Usb, 1.0, 0, false,
                           3500.0, agcMode, 0, reduction)) {
            return QVector<short> {};
        }
        return dsp.processFrame(reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), false)
            .audioPcm;
    };
    const auto raw = receive(RtlSdrDsp::SsbAgcMode::Off,
                             RtlSdrDsp::SsbNoiseReductionMode::Off);
    const auto agc = receive(RtlSdrDsp::SsbAgcMode::Slow,
                             RtlSdrDsp::SsbNoiseReductionMode::Off);
    const auto reduced = receive(RtlSdrDsp::SsbAgcMode::Off,
                                 RtlSdrDsp::SsbNoiseReductionMode::Medium);
    QCOMPARE(raw.size(), samples / 5);
    QCOMPARE(agc.size(), samples / 5);
    QCOMPARE(reduced.size(), samples / 5);

    constexpr int warmup = 12000;
    QVERIFY2(rms(agc, warmup) > rms(raw, warmup) * 1.5,
             "optional SSB AGC must raise a weak listening signal");
    QVERIFY2(rms(reduced, warmup) < rms(raw, warmup) * 0.75,
             "optional medium noise reduction must attenuate low-level background audio");
}

void TestRtlSdrDsp::ncoRecentresOffsetChannelBeforeDecoderPath()
{
    RtlSdrDsp dsp;
    QVERIFY(dsp.configure(240000, RtlSdrDsp::Demodulator::WeakSignal, 1.0, -10000));

    QByteArray iq;
    constexpr int samples = 24000;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = -2.0 * 3.14159265358979323846 * 10000.0 * index / 240000.0;
        iq[2 * index] = static_cast<char>(qBound(0, qRound(127.5 + 70.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(0, qRound(127.5 + 70.0 * std::sin(phase)), 255));
    }

    const QVector<short> pcm = dsp.process(reinterpret_cast<const unsigned char *>(iq.constData()), iq.size());
    QCOMPARE(pcm.size(), samples / 20);
    qint64 sum = 0;
    for (int index = 200; index < pcm.size(); ++index) {
        sum += pcm.at(index);
    }
    QVERIFY2(std::abs(sum / qMax(1, pcm.size() - 200)) > 5000,
             "the NCO must move the selected offset channel to decoder baseband");
}

void TestRtlSdrDsp::ifSpectrumInversionConjugatesIq()
{
    RtlSdrDsp normal;
    RtlSdrDsp inverted;
    QVERIFY(normal.configure(240000, RtlSdrDsp::Demodulator::WeakSignal,
                             1.0, 0, false));
    QVERIFY(inverted.configure(240000, RtlSdrDsp::Demodulator::WeakSignal,
                               1.0, 0, true));

    QByteArray iq;
    constexpr int samples = 4096;
    iq.resize(samples * 2);
    for (int index = 0; index < samples; ++index) {
        const double phase = 2.0 * 3.14159265358979323846 * 20000.0
            * index / 240000.0;
        iq[2 * index] = static_cast<char>(qBound(
            0, qRound(127.5 + 70.0 * std::cos(phase)), 255));
        iq[2 * index + 1] = static_cast<char>(qBound(
            0, qRound(127.5 + 70.0 * std::sin(phase)), 255));
    }

    const auto normalFrame = normal.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    const auto invertedFrame = inverted.processFrame(
        reinterpret_cast<const unsigned char *>(iq.constData()), iq.size(), true);
    QCOMPARE(normalFrame.iq.size(), invertedFrame.iq.size());
    for (int sample = 100; sample < samples; ++sample) {
        QCOMPARE(normalFrame.iq.at(2 * sample), invertedFrame.iq.at(2 * sample));
        QVERIFY(std::abs(static_cast<int>(normalFrame.iq.at(2 * sample + 1))
                         + invertedFrame.iq.at(2 * sample + 1)) <= 1);
    }
}

QTEST_MAIN(TestRtlSdrDsp)
#include "test_rtlsdr_dsp.moc"
