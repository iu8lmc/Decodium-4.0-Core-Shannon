// Both production TCP bridges/clients, with only the physical radio replaced.
#include "src/radio/DecodiumCatShare.h"
#include "core/RigControl.h"
#include "core/CatShare.h"
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTest>

class Radio : public decolog::core::RigLink {
public:
    int wpm{24}, stops{0};
    bool ptt{false};
    QString currentMode{"CW"};
    QStringList sent;
    void disconnectFromRig() override {}
    bool connected() const override { return true; }
    qint64 frequencyHz() const override { return 7030000; }
    QString mode() const override { return currentMode; }
    int speedWpm() const override { return wpm; }
    QString status() const override { return {}; }
    void refresh() override {}
    void setFrequency(qint64) override {}
    void setMode(const QString& value) override { currentMode = value; }
    void setPtt(bool on) override { ptt = on; }
    void setSpeedWpm(int value) override { wpm = value; }
    void sendMorse(const QString& text) override { sent << text; }
    void stopMorse() override { ++stops; ptt = false; }
};

class TestCatShareInterop : public QObject {
    Q_OBJECT
private slots:
    void decologClientWithDecodiumMaster()
    {
        Radio radio;
        DecodiumCatShare::Hooks hooks;
        hooks.state = [&]() {
            DecodiumCatShare::State state;
            state.connected = true; state.frequency = radio.frequencyHz(); state.mode = radio.mode();
            return state;
        };
        hooks.mode = [&](const QString& mode) { radio.setMode(mode); };
        hooks.ptt = [&](bool on) { radio.setPtt(on); };
        hooks.morseAvailable = []() { return true; };
        hooks.morseTextLimit = []() { return 30; };
        hooks.morseSpeed = [&](bool write, int wpm, DecodiumCatShare::SpeedCompletion done) {
            if (write) radio.setSpeedWpm(wpm);
            QTimer::singleShot(10, &radio, [&, done]() { done(0, radio.wpm); });
        };
        hooks.sendMorse = [&](const QString& text, DecodiumCatShare::Completion done) {
            radio.sendMorse(text);
            QTimer::singleShot(10, &radio, [done]() { done(0); });
        };
        hooks.stopMorse = [&](DecodiumCatShare::Completion done) { radio.stopMorse(); done(0); };
        DecodiumCatShare master(hooks);
        QVERIFY(master.configure(true, 0, true, true));
        decolog::core::RigControl client;
        QSignalSpy errors(&client, &decolog::core::RigControl::failed);
        QSignalSpy unsupported(&client, &decolog::core::RigControl::morseUnsupported);
        QSignalSpy accepted(&client, &decolog::core::RigControl::morseSent);
        client.connectTo(QStringLiteral("127.0.0.1"), master.findChild<QTcpServer*>()->serverPort());
        QTRY_COMPARE(client.frequencyHz(), radio.frequencyHz());
        QTRY_COMPARE(client.speedWpm(), 24);
        client.setSpeedWpm(40);
        QTRY_COMPARE(radio.wpm, 40);
        client.sendMorse(QStringLiteral("?"));
        QTRY_COMPARE(accepted.size(), 1);
        QCOMPARE(radio.sent, QStringList({"?"}));
        QVERIFY(!radio.ptt); // keying never owns continuous PTT
        client.emergencyStop();
        QTRY_VERIFY(radio.stops >= 1);
        client.sendMorse(QStringLiteral("CQ TEST"));
        QTRY_COMPARE_WITH_TIMEOUT(accepted.size(), 2, 5000);
        QCOMPARE(radio.sent.last(), QStringLiteral("CQ TEST"));
        client.emergencyStop();
        QTRY_VERIFY(radio.stops >= 2);
        QCOMPARE(unsupported.size(), 0);
        QCOMPARE(errors.size(), 0);
    }

    void hamlibClientWithDecologMaster()
    {
        Radio radio;
        radio.currentMode = QStringLiteral("USB");
        decolog::core::CatShare master;
        master.setRigProvider([&]() { return &radio; });
        QVERIFY(master.configure(true, 0, true, true));
        QProcess client;
        client.start(QString::fromUtf8(HAMLIB_TEST_CLIENT),
                     {QStringLiteral("127.0.0.1:%1").arg(master.findChild<QTcpServer*>()->serverPort())});
        QVERIFY(client.waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QProcess::NotRunning, 15000);
        const QByteArray output = client.readAllStandardOutput() + client.readAllStandardError();
        QVERIFY2(client.exitStatus() == QProcess::NormalExit && client.exitCode() == 0, output.constData());
        QCOMPARE(radio.wpm, 35);
        QCOMPARE(radio.currentMode, QStringLiteral("CW"));
        QCOMPARE(radio.sent, QStringList({"?"}));
        QVERIFY(radio.stops >= 1);
        QVERIFY(!radio.ptt);
    }
};
QTEST_GUILESS_MAIN(TestCatShareInterop)
#include "test_cat_share_interop.moc"
