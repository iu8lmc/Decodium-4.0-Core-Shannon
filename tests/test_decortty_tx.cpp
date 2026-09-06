#include <QtTest>

#include <QFile>

#include <algorithm>
#include <cmath>
#include <utility>

#include "src/rtty/app/RttyEngine.h"
#include "src/rtty/link/DecodiumLink.h"
#include "src/rtty/link/RadioHub.h"

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
    void setRadioUsesTheCatHook();
    void prepareProfileIsQmxAware();
    void nativeFskBlocksPttButDataAllowsAudio();
    void qmxRttyProducesAudioAboveTheDetectorThreshold();
};

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
