#include <QtTest>

#include <QFile>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

#include "src/rtty/app/RttyEngine.h"
#include "src/rtty/link/DecodiumLink.h"
#include "src/rtty/link/RadioHub.h"
#include "src/rtty/dsp/AutoTuner.h"

namespace {

decortty::link::DecodiumLink::Ganci radioHooks(QString const& name,
                                                QString* mode,
                                                QString* requestedMode = nullptr)
{
    decortty::link::DecodiumLink::Ganci hooks;
    hooks.connesso = [] { return true; };
    hooks.nomeRadio = [name] { return name; };
    hooks.modo = [mode] { return mode ? *mode : QString(); };
    hooks.puoTrasmettere = [] { return true; };
    if (requestedMode) {
        hooks.impostaModo = [requestedMode](QString const& requested) {
            *requestedMode = requested;
        };
    }
    return hooks;
}

} // namespace

class TestDecoRttyTx final : public QObject
{
    Q_OBJECT

private slots:
    void clockTracksTransmitter_data();
    void clockTracksTransmitter();
    void setRadioUsesTheCatHook();
    void prepareProfileIsQmxAware();
    void nativeFskBlocksPttButDataAllowsAudio();
    void qmxRttyProducesAudioAboveTheDetectorThreshold();
    void modeExitStopsAutoCqAndAudio();
    void audioOnlyTransmit_data();
    void audioOnlyTransmit();
    void audioOnlyReadinessNotifiesWithoutCat();
    void rejectedAudioStopsTransmit();
    void audioOnlyBridgeWiring();
    void completeAudioRoundTrip_data();
    void completeAudioRoundTrip();
    void silenceAndCarrierDoNotProduceText();
    void autoTuneWaitsForLiveSignal();
    void reverseChangeRebuildsDemodulator();
    void receiveAfterTransmit_data();
    void receiveAfterTransmit();
    void afcNeedsLivePairAndResetMatchesFreshReceiver();
};

// Long receptions expose positive feedback that short round trips cannot see.
void TestDecoRttyTx::clockTracksTransmitter_data()
{
    QTest::addColumn<double>("baud");
    QTest::addColumn<double>("shift");
    QTest::addColumn<double>("offset");
    for (double baud : {45.45, 50.0, 75.0})
        for (double shift : {170.0, 450.0, 850.0})
            for (double offset : {-0.02, 0.0, 0.02})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(baud).arg(shift).arg(offset)))
                    << baud << shift << offset;
}

void TestDecoRttyTx::clockTracksTransmitter()
{
    QFETCH(double, baud);
    QFETCH(double, shift);
    QFETCH(double, offset);
    using namespace decortty::dsp;
    RttyParams params;
    params.baud = baud;
    params.shiftHz = shift;
    params.afcEnabled = false;
    // The built-in transmitter encodes spaces using the amateur USOS convention.
    params.unshiftOnSpace = true;
    RttyDemodulator receiver(params);
    params.baud = baud * (1.0 + offset);
    FskModulator transmitter(params, kWorkRate);
    transmitter.setDiddle(false);
    const std::string line = "RY TEST 123 456 WEATHER NW 5-6 1.5 M\r\n";
    std::string message;
    for (int i = 0; i < 90; ++i)
        message += line;
    transmitter.enqueueText(message);
    ShiftState shiftState(true);
    std::string received;
    float samples[800];
    while (!transmitter.idle()) {
        transmitter.generate(samples, 800);
        receiver.process(samples, 800, [&](const SoftFrame& frame) {
            const char c = shiftState.feed(frame.hardCode);
            if (c) received += c;
        });
    }
    QVERIFY2(std::abs(receiver.measuredBaud() - params.baud) < baud * 0.003,
             qPrintable(QString("Expected %1, measured %2").arg(params.baud).arg(receiver.measuredBaud())));
    // Allow initial acquisition and the final filter tail, but require sustained copy.
    size_t count = 0;
    for (size_t pos = 0; (pos = received.find(line, pos)) != std::string::npos; pos += line.size())
        ++count;
    QVERIFY2(count >= 85, qPrintable(QString("Only %1 complete lines").arg(count)));
}

void TestDecoRttyTx::receiveAfterTransmit_data()
{
    QTest::addColumn<double>("frequency");
    QTest::addColumn<double>("offset");
    QTest::addColumn<double>("gap");
    QTest::addColumn<bool>("transmitBetween");
    for (double hz : {1950.0, 2000.0, 2043.468, 2070.0, 2097.734, 2125.0, 2150.0})
        for (double drift : {0.0, 30.0})
            for (double pause : {0.1, 1.0, 6.0})
              for (bool txBetween : {false, true}) {
                const QByteArray name = QStringLiteral("%1Hz-drift%2-gap%3-tx%4").arg(hz).arg(drift).arg(pause).arg(txBetween).toLatin1();
                QTest::newRow(name.constData()) << hz << drift << pause << txBetween;
            }
}

void TestDecoRttyTx::receiveAfterTransmit()
{
    QFETCH(double, frequency);
    QFETCH(double, offset);
    QFETCH(double, gap);
    QFETCH(bool, transmitBetween);
    using namespace decortty;
    app::RttyEngine rx;
    rx.setMarkHz(frequency);
    bool localTx = false;
    auto hooks = radioHooks(QString(), nullptr);
    hooks.connesso = [] { return false; };
    hooks.inTrasmissione = [&] { return localTx; };
    hooks.impostaPtt = [&](bool on) { localTx = on; };
    link::RadioHub radio;
    radio.collegaADecodium(std::move(hooks));
    rx.attachRadio(&radio);
    QString received;
    connect(&rx, &app::RttyEngine::characterDecoded, this,
            [&](QString c, double, bool) { received += c; });
    auto silence = [&](double seconds) {
        for (int i = 0; i < qRound(seconds * 50); ++i)
            rx.processRadioAudio(std::vector<float>(960, 0.0f), 480);
    };
    auto send = [&](const char* message, double mark) {
        dsp::RttyParams params;
        params.markHz = static_cast<float>(mark);
        dsp::FskModulator tx(params, 24000);
        tx.setDiddle(false);
        tx.setAmplitude(0.35f);
        for (int i = 0; i < 12; ++i) tx.enqueueCode(dsp::kBaudotLtrs);
        tx.enqueueText(message);
        tx.enqueueCode(dsp::kBaudotLtrs);
        tx.enqueueCode(dsp::kBaudotLtrs);
        std::vector<float> mono(480), stereo(960);
        while (!tx.idle()) {
            tx.generate(mono.data(), 480);
            for (int i = 0; i < 480; ++i)
                stereo[2 * i] = stereo[2 * i + 1] = mono[i];
            rx.processRadioAudio(stereo, 480);
        }
    };
    send("CHE MI DICI DI BELLO????", frequency + offset);
    silence(1.2);
    QVERIFY(!received.isEmpty());
    if (transmitBetween && gap == 1.0) {
        // A device that supplies NO input callbacks during local TX must
        // still reset before its first received samples after release.
        rx.startTransmit();
        QVERIFY(localTx);
        rx.abortTransmit();
        QVERIFY(!localTx);
    } else if (transmitBetween) {
        localTx = true;
        silence(3.0);
        localTx = false;
    }
    silence(gap);
    QCOMPARE(rx.afcOffsetHz(), 0.0);
    received.clear();
    send("SEMPRE LA SOLITA VITACCIA!", frequency);
    silence(1.2);
    QCOMPARE(received, QStringLiteral("SEMPRE LA SOLITA VITACCIA!"));
}

void TestDecoRttyTx::afcNeedsLivePairAndResetMatchesFreshReceiver()
{
    using namespace decortty::dsp;
    RttyParams params;
    RttyDemodulator used(params, 8000);
    std::vector<float> samples(2048);
    // An isolated carrier must not pull AFC, even if it is very strong.
    for (int block = 0; block < 20; ++block) {
        for (int i = 0; i < 2048; ++i)
            samples[i] = 0.35f * std::sin(2 * 3.141592653589793 * (params.markHz + 30)
                                       * (block * 2048 + i) / 8000.0);
        used.process(samples.data(), samples.size(), [](const SoftFrame&) {});
    }
    QCOMPARE(used.afcOffsetHz(), 0.0f);
    // A real displaced tone pair must still be tracked.
    RttyParams txParams = params;
    txParams.markHz += 30;
    FskModulator tx(txParams, 8000);
    tx.enqueueText("RYRYRYRY CQ TEST TEST 599 599 CQ TEST RYRYRYRY");
    for (int i = 0; i < 30; ++i) {
        tx.generate(samples.data(), samples.size());
        used.process(samples.data(), samples.size(), [](const SoftFrame&) {});
    }
    QVERIFY(used.afcOffsetHz() > 20.0f);
    QVERIFY(used.afcOffsetHz() < 40.0f);
    std::fill(samples.begin(), samples.end(), 0.0f);
    // Allow one FFT window to replace the last real signal, then require
    // exact stability through ten seconds of silence (not spectrum ghosts).
    used.process(samples.data(), samples.size(), [](const SoftFrame&) {});
    const float offset = used.afcOffsetHz();
    for (int i = 0; i < 40; ++i) {
        used.process(samples.data(), samples.size(), [](const SoftFrame&) {});
        QCOMPARE(used.afcOffsetHz(), offset);
    }
    used.clearSpectrum();
    QCOMPARE(used.afcOffsetHz(), 0.0f);
    QVERIFY(!used.locked());
    QVERIFY(!used.searchTonePair(400, 2600).found);
    RttyDemodulator fresh(params, 8000);
    FskModulator next(params, 8000);
    next.enqueueText("RYRYRYRYRYRY SEMPRE LA SOLITA VITACCIA!");
    std::vector<int> oldFrames, freshFrames;
    for (int i = 0; i < 35; ++i) {
        next.generate(samples.data(), samples.size());
        used.process(samples.data(), samples.size(), [&](const SoftFrame& f) { oldFrames.push_back(f.hardCode); });
        fresh.process(samples.data(), samples.size(), [&](const SoftFrame& f) { freshFrames.push_back(f.hardCode); });
        QCOMPARE(used.afcOffsetHz(), fresh.afcOffsetHz());
    }
    QVERIFY(!freshFrames.empty());
    QVERIFY(oldFrames == freshFrames);
}

void TestDecoRttyTx::autoTuneWaitsForLiveSignal()
{
    using namespace decortty::dsp;
    RttyParams params;
    params.afcEnabled = false;
    RttyDemodulator demod(params, 8000);
    AutoTuner tuner;
    tuner.setEnabled(true);
    std::vector<float> samples(2000, 0.0f);
    for (int tick = 0; tick < 40; ++tick) {
        demod.process(samples.data(), samples.size(), [](const SoftFrame&) {});
        QVERIFY(!tuner.update(demod, params, 0.25).changed);
    }
    // A balanced pair without Baudot framing must eventually trigger a
    // polarity trial, but not use the preceding ten seconds of silence.
    int firstFlip = -1;
    for (int tick = 0; tick < 20; ++tick) {
        for (int i = 0; i < 2000; ++i) {
            const double t = (tick * 2000 + i) / 8000.0;
            samples[i] = 0.15f * (std::sin(2 * 3.141592653589793 * params.markHz * t)
                                + std::sin(2 * 3.141592653589793 * params.spaceHz() * t));
        }
        demod.process(samples.data(), samples.size(), [](const SoftFrame&) {});
        const auto decision = tuner.update(demod, params, 0.25);
        if (decision.action == AutoTuner::Decision::Action::FlipReverse) {
            firstFlip = tick;
            break;
        }
    }
    QVERIFY(firstFlip >= 11);
    QVERIFY(firstFlip < 20);
    std::fill(samples.begin(), samples.end(), 0.0f);
    for (int tick = 0; tick < 30; ++tick) {
        demod.process(samples.data(), samples.size(), [](const SoftFrame&) {});
        QVERIFY(!tuner.update(demod, params, 0.25).changed);
    }
    QVERIFY(!demod.searchTonePair(400, 2600).found);
}

void TestDecoRttyTx::reverseChangeRebuildsDemodulator()
{
    using namespace decortty::dsp;
    RttyParams params;
    params.afcEnabled = false;
    params.reverse = true;
    RttyDemodulator demod(params, 8000);
    params.reverse = false;
    demod.setParams(params); // No AFC retune to accidentally repair the swap.
    FskModulator tx(params, 8000);
    tx.setDiddle(false);
    for (int i = 0; i < 12; ++i) tx.enqueueCode(kBaudotLtrs);
    tx.enqueueText("CIAO COME STAI?");
    tx.enqueueCode(kBaudotLtrs);
    tx.enqueueCode(kBaudotLtrs);
    ViterbiBaudot decoder;
    QString received;
    auto out = [&](const DecodedChar& c) { received += QChar::fromLatin1(c.text); };
    std::vector<float> samples(160);
    for (int i = 0; i < 600; ++i) {
        tx.generate(samples.data(), samples.size());
        demod.process(samples.data(), samples.size(), [&](const SoftFrame& f) { decoder.push(f, out); });
    }
    decoder.flush(out);
    QCOMPARE(received, QStringLiteral("CIAO COME STAI?"));
}

void TestDecoRttyTx::completeAudioRoundTrip_data()
{
    QTest::addColumn<QString>("message");
    QTest::addColumn<int>("depth");
    QTest::addColumn<bool>("automatic");
    QTest::newRow("short-letters") << QStringLiteral("OK") << 4 << false;
    QTest::newRow("short-figures-deep") << QStringLiteral("599?") << 10 << false;
    QTest::newRow("pauls-message-after-idle") << QStringLiteral("CIAO COME STAI?") << 4 << true;
    QTest::newRow("reply-after-idle") << QStringLiteral("TUTTO BENE?") << 4 << true;
}

void TestDecoRttyTx::completeAudioRoundTrip()
{
    QFETCH(QString, message);
    QFETCH(int, depth);
    QFETCH(bool, automatic);
    decortty::app::RttyEngine tx, rx;
    tx.setMarkHz(2097.734375);
    rx.setMarkHz(2097.734375);
    rx.setCorrectionDepth(depth);
    rx.setAutoTuneEnabled(automatic);
    bool active = false;
    auto hooks = radioHooks(QString(), nullptr);
    hooks.connesso = [] { return false; };
    hooks.inTrasmissione = [&] { return active; };
    hooks.impostaPtt = [&](bool on) { active = on; };
    // Real RttyEngine -> DecodiumLink (24 -> 12 kHz PCM16) -> the host's
    // 12 -> 24 kHz stereo conversion -> independent receiving RttyEngine.
    hooks.mandaAudioTx = [&](QVector<short> const& pcm) {
        std::vector<float> stereo(static_cast<size_t>(pcm.size()) * 4);
        for (qsizetype i = 0; i < pcm.size(); ++i) {
            const float a = pcm[i] / 32768.0f;
            const float b = pcm[std::min(i + 1, pcm.size() - 1)] / 32768.0f;
            stereo[i * 4] = stereo[i * 4 + 1] = a;
            stereo[i * 4 + 2] = stereo[i * 4 + 3] = (a + b) * 0.5f;
        }
        rx.processRadioAudio(stereo, pcm.size() * 2);
    };
    decortty::link::RadioHub radio;
    radio.collegaADecodium(std::move(hooks));
    tx.attachRadio(&radio);
    QString received;
    connect(&rx, &decortty::app::RttyEngine::characterDecoded,
            this, [&](QString c, double, bool) { received += c; });
    QSignalSpy retunes(&rx, &decortty::app::RttyEngine::autoTuned);
    QTimer silence;
    connect(&silence, &QTimer::timeout, this, [&] {
        if (!active) rx.processRadioAudio(std::vector<float>(960, 0.0f), 480);
    });
    silence.start(20);
    // Before the fix this wait consumed the polarity grace period, so AUTO
    // flipped the correct signal before the demodulator could acquire it.
    if (automatic) QTest::qWait(4000);
    tx.transmitText(message);
    QTRY_VERIFY_WITH_TIMEOUT(!tx.transmitting(), 10000);
    QTRY_COMPARE_WITH_TIMEOUT(received, message, 3000);
    QVERIFY(!active);
    QVERIFY(retunes.isEmpty());
    const QString complete = received;
    QTest::qWait(1200);
    QCOMPARE(received, complete); // Tail is emitted once, not on every timeout.
}

void TestDecoRttyTx::silenceAndCarrierDoNotProduceText()
{
    for (const bool carrier : {false, true}) {
        decortty::app::RttyEngine rx;
        QSignalSpy decoded(&rx, &decortty::app::RttyEngine::characterDecoded);
        std::vector<float> stereo(960);
        for (int block = 0; block < 1000; ++block) {
            for (int i = 0; i < 480; ++i) {
                const float sample = carrier ? 0.35f * std::sin(
                    2.0 * 3.141592653589793 * 2125.0 * (block * 480 + i) / 24000.0) : 0.0f;
                stereo[i * 2] = stereo[i * 2 + 1] = sample;
            }
            rx.processRadioAudio(stereo, 480);
        }
        QVERIFY(decoded.isEmpty());
    }
}

void TestDecoRttyTx::audioOnlyTransmit_data()
{
    QTest::addColumn<QString>("savedMode");
    QTest::addColumn<bool>("abort");
    QTest::newRow("manual-radio-stale-fsk") << QStringLiteral("RTTY-U") << false;
    QTest::newRow("manual-radio-stale-usb") << QStringLiteral("USB") << false;
    QTest::newRow("blackhole-abort") << QString() << true;
}

void TestDecoRttyTx::audioOnlyTransmit()
{
    QFETCH(QString, savedMode);
    QFETCH(bool, abort);
    bool active = false;
    int chunks = 0;
    // The saved QMX identity and mode must not override a manual/audio-only
    // setup's volume or pretend to describe the currently attached radio.
    auto hooks = radioHooks(QStringLiteral("QMX"), &savedMode);
    hooks.connesso = [] { return false; };
    hooks.inTrasmissione = [&] { return active; };
    hooks.impostaPtt = [&](bool on) { active = on; };
    hooks.mandaAudioTx = [&](QVector<short> const& samples) {
        QVERIFY(active);
        QVERIFY(!samples.isEmpty());
        ++chunks;
    };
    decortty::link::RadioHub radio;
    decortty::app::RttyEngine engine;
    engine.attachRadio(&radio);
    radio.collegaADecodium(std::move(hooks));
    QSignalSpy errors(&engine, &decortty::app::RttyEngine::errorOccurred);
    QVERIFY(!radio.connected());
    QVERIFY(radio.canTransmit());
    QVERIFY(radio.mode().isEmpty());
    QVERIFY(!radio.requiresFullScaleTransmitAudio());
    if (abort) engine.startTransmit();
    else engine.transmitText(QStringLiteral("RY"));
    QTRY_VERIFY_WITH_TIMEOUT(chunks > 0, 1000);
    QVERIFY(engine.transmitting());
    if (abort) engine.abortTransmit();
    QTRY_VERIFY_WITH_TIMEOUT(!engine.transmitting(), 5000);
    QVERIFY(!active);
    QVERIFY(errors.isEmpty());
    const int finalChunks = chunks;
    QTest::qWait(400);
    QCOMPARE(chunks, finalChunks);
}

void TestDecoRttyTx::audioOnlyReadinessNotifiesWithoutCat()
{
    bool ready = false;
    auto hooks = radioHooks(QString(), nullptr);
    hooks.connesso = [] { return false; };
    hooks.puoTrasmettere = [&] { return ready; };
    decortty::link::RadioHub radio;
    radio.collegaADecodium(std::move(hooks));
    QSignalSpy changed(&radio, &decortty::link::RadioHub::connectionChanged);
    QVERIFY(!radio.canTransmit());
    ready = true;
    QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 1, 1000);
    QVERIFY(radio.canTransmit());
    QVERIFY(!radio.connected());
    QVERIFY(radio.statusText().contains(QStringLiteral("VOX")));
    ready = false;
    QTRY_COMPARE_WITH_TIMEOUT(changed.size(), 2, 1000);
    QVERIFY(!radio.canTransmit());
}

void TestDecoRttyTx::rejectedAudioStopsTransmit()
{
    bool active = false;
    bool ready = true;
    int chunks = 0;
    auto hooks = radioHooks(QString(), nullptr);
    hooks.connesso = [] { return false; };
    hooks.inTrasmissione = [&] { return active; };
    hooks.puoTrasmettere = [&] { return ready; };
    hooks.impostaPtt = [&](bool on) { active = on; };
    hooks.mandaAudioTx = [&](QVector<short> const&) { ++chunks; };
    decortty::link::RadioHub radio;
    decortty::app::RttyEngine engine;
    engine.attachRadio(&radio);
    radio.collegaADecodium(std::move(hooks));
    QSignalSpy errors(&engine, &decortty::app::RttyEngine::errorOccurred);
    engine.startTransmit();
    QTRY_VERIFY_WITH_TIMEOUT(chunks > 0, 1000);
    ready = false; // Output removed or another transmitter owns the path.
    QTRY_VERIFY_WITH_TIMEOUT(!engine.transmitting(), 1000);
    QVERIFY(!active);
    QCOMPARE(errors.size(), 1);
    const int finalChunks = chunks;
    QTest::qWait(400);
    QCOMPARE(chunks, finalChunks);
}

void TestDecoRttyTx::audioOnlyBridgeWiring()
{
    // This complements the real engine/link tests: the application must use
    // local audio ownership, without relaxing the network gateway CAT guard.
    QFile source(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/DecodiumBridge.cpp"));
    QVERIFY(source.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(source.readAll());
    const auto remoteStart = text.indexOf("void DecodiumBridge::decoPortKeyLocalRig(");
    const auto remoteEnd = text.indexOf("void DecodiumBridge::keySharedAudioTransmitter(", remoteStart);
    QVERIFY(remoteStart >= 0 && remoteEnd > remoteStart);
    QVERIFY(text.mid(remoteStart, remoteEnd - remoteStart).contains("keySharedAudioTransmitter(on, false)"));
    const auto localStart = text.indexOf("void DecodiumBridge::rttyAlzaPtt(");
    const auto localEnd = text.indexOf("void DecodiumBridge::rttyMandaAudioTx(", localStart);
    QVERIFY(localStart >= 0 && localEnd > localStart);
    QVERIFY(text.mid(localStart, localEnd - localStart).contains("keySharedAudioTransmitter(true, true)"));
}

void TestDecoRttyTx::modeExitStopsAutoCqAndAudio()
{
    QString mode = QStringLiteral("DIGU");
    bool ptt = false;
    int chunks = 0;
    auto hooks = radioHooks(QStringLiteral("QMX"), &mode);
    hooks.impostaPtt = [&ptt](bool on) { ptt = on; };
    hooks.mandaAudioTx = [&chunks](QVector<short> const&) { ++chunks; };
    decortty::link::RadioHub radio;
    decortty::app::RttyEngine engine;
    engine.attachRadio(&radio);
    radio.collegaADecodium(std::move(hooks));
    engine.startAutoCq(QStringLiteral("CQ CQ TEST"), 3);
    QTRY_VERIFY_WITH_TIMEOUT(chunks > 0, 1000);
    engine.stopAutoCq();
    engine.stopTransmit(true);
    QVERIFY(!ptt);
    QVERIFY(!engine.transmitting());
    const int stoppedChunks = chunks;
    QTest::qWait(3200);
    QCOMPARE(chunks, stoppedChunks);
    QVERIFY(!ptt);
}

void TestDecoRttyTx::nativeFskBlocksPttButDataAllowsAudio()
{
    QString mode;
    bool ptt = false;
    QVector<short> audio;
    auto hooks = radioHooks(QStringLiteral("Icom IC-7100"), &mode);
    hooks.impostaPtt = [&ptt](bool on) { ptt = on; };
    hooks.mandaAudioTx = [&audio](QVector<short> const& samples) { audio += samples; };
    decortty::link::RadioHub radio;
    decortty::app::RttyEngine engine;
    engine.attachRadio(&radio);
    radio.collegaADecodium(std::move(hooks));
    QSignalSpy errors(&engine, &decortty::app::RttyEngine::errorOccurred);
    for (QString const& nativeMode : {QStringLiteral("RTTY"), QStringLiteral("RTTY-R"),
                                    QStringLiteral("RTTY-U"), QStringLiteral("RTTY-L"),
                                    QStringLiteral("FSK"), QStringLiteral("FSK-R")}) {
        mode = nativeMode;
        errors.clear();
        engine.transmitText(QStringLiteral("RY"));
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().first().toString().contains(QStringLiteral("Set radio")));
        QVERIFY(!ptt);
        QVERIFY(!engine.transmitting());
        QVERIFY(audio.isEmpty());
    }
    mode = QStringLiteral("DATA-U");
    errors.clear();
    engine.transmitText(QStringLiteral("RY"));
    QTRY_VERIFY_WITH_TIMEOUT(!audio.isEmpty(), 1000);
    QVERIFY(ptt);
    QVERIFY(errors.isEmpty());
    engine.abortTransmit();
    QVERIFY(!ptt);
}

void TestDecoRttyTx::setRadioUsesTheCatHook()
{
    QFile source(QStringLiteral(DECODIUM_SOURCE_DIR "/src/app/main_qml.cpp"));
    QVERIFY2(source.open(QIODevice::ReadOnly), qPrintable(source.errorString()));
    QString const text = QString::fromUtf8(source.readAll());
    int const start = text.indexOf(QStringLiteral("DecodiumLink::Ganci ganci"));
    int const end = text.indexOf(QStringLiteral("rttyHost.impostaGanciRadio"), start);
    QVERIFY(start >= 0);
    QVERIFY(end > start);

    QString const wiring = text.mid(start, end - start);
    QVERIFY(wiring.contains(QStringLiteral("ganci.impostaModo")));
    QVERIFY(wiring.contains(QStringLiteral("bridge.impostaModoRadioRtty")));
    QVERIFY(!wiring.contains(QStringLiteral("bridge.setMode")));
}

void TestDecoRttyTx::prepareProfileIsQmxAware()
{
    {
        QString mode = QStringLiteral("RTTY-U");
        QString requested;
        decortty::link::RadioHub radio;
        radio.collegaADecodium(radioHooks(QStringLiteral("QRP Labs QMX"),
                                         &mode, &requested));

        QVERIFY(radio.requiresFullScaleTransmitAudio());
        radio.applyRttyProfile(2125, 170);
        QCOMPARE(requested, QStringLiteral("DATA-U"));

        radio.setMode(QStringLiteral("RTTY-U"));
        QCOMPARE(requested, QStringLiteral("DATA-U"));
        radio.setMode(QStringLiteral("RTTY"));
        QCOMPARE(requested, QStringLiteral("DATA-U"));
        radio.setMode(QStringLiteral("RTTY-L"));
        QCOMPARE(requested, QStringLiteral("DATA-L"));
    }

    {
        QString mode = QStringLiteral("RTTY-U");
        QString requested;
        decortty::link::RadioHub radio;
        radio.collegaADecodium(radioHooks(QStringLiteral("Yaesu FT-991A"),
                                         &mode, &requested));

        QVERIFY(!radio.requiresFullScaleTransmitAudio());
        radio.applyRttyProfile(2125, 170);
        QCOMPARE(requested, QStringLiteral("DATA-U"));
    }
}

void TestDecoRttyTx::qmxRttyProducesAudioAboveTheDetectorThreshold()
{
    QString mode = QStringLiteral("RTTY-U");
    bool ptt = false;
    QVector<short> transmitted;

    auto hooks = radioHooks(QStringLiteral("QRP Labs QMX+"), &mode);
    hooks.inTrasmissione = [&ptt] { return ptt; };
    hooks.impostaPtt = [&ptt](bool on) { ptt = on; };
    hooks.mandaAudioTx = [&transmitted](QVector<short> const& samples) {
        transmitted += samples;
    };

    decortty::link::RadioHub radio;
    decortty::app::RttyEngine engine;
    engine.attachRadio(&radio);
    radio.collegaADecodium(std::move(hooks));

    // Existing installations persist 0.35. QMX must override only the
    // effective value because its factory rise threshold is 80%.
    engine.setTransmitLevel(0.35);
    QCOMPARE(engine.transmitLevel(), 1.0);
    QVERIFY(qAbs(engine.configuredTransmitLevel() - 0.35) < 1e-6);

    engine.transmitText(QStringLiteral("RYRY"));
    QTRY_VERIFY_WITH_TIMEOUT(!transmitted.isEmpty(), 1000);
    QVERIFY(ptt);
    QVERIFY(engine.transmitting());

    int peak = 0;
    for (short const sample : transmitted)
        peak = std::max(peak, std::abs(static_cast<int>(sample)));
    QVERIFY2(peak > qRound(32767.0 * 0.80),
             qPrintable(QStringLiteral("QMX RTTY peak was only %1").arg(peak)));

    engine.abortTransmit();
    QVERIFY(!ptt);
    QVERIFY(!engine.transmitting());

    // A later conventional rig must get the saved AFSK value, not the QMX
    // effective override. This protects its ALC from an unexpected 100%.
    radio.collegaADecodium(radioHooks(QStringLiteral("Yaesu FT-991A"), &mode));
    QVERIFY(!radio.requiresFullScaleTransmitAudio());
    QVERIFY(qAbs(engine.configuredTransmitLevel() - 0.35) < 1e-6);
    QVERIFY(qAbs(engine.transmitLevel() - 0.35) < 1e-6);
}

QTEST_GUILESS_MAIN(TestDecoRttyTx)
#include "test_decortty_tx.moc"
