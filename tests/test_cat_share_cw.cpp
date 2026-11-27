#include "src/radio/DecodiumCatShare.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

namespace {
struct FakeRadio : QObject {
    int wpm{60}, limit{30}, speedError{0}, sendError{0}, stopError{0}, stops{0};
    bool available{true}, connected{true}, holdSend{false};
    QStringList sent;
    QList<bool> ptt;
    DecodiumCatShare::Completion pending;
    DecodiumCatShare::Hooks hooks()
    {
        DecodiumCatShare::Hooks h;
        h.state = [this]() {
            DecodiumCatShare::State s;
            s.connected = connected; s.frequency = 7030000; s.mode = QStringLiteral("CW");
            return s;
        };
        h.ptt = [this](bool on) { ptt << on; };
        h.morseAvailable = [this]() { return available; };
        h.morseTextLimit = [this]() { return limit; };
        h.morseSpeed = [this](bool write, int value, DecodiumCatShare::SpeedCompletion done) {
            QTimer::singleShot(10, this, [this, write, value, done]() {
                if (write && !speedError) wpm = value;
                done(speedError, wpm);
            });
        };
        h.sendMorse = [this](const QString& text, DecodiumCatShare::Completion done) {
            sent << text;
            if (holdSend) pending = done;
            else done(sendError);
        };
        h.stopMorse = [this](DecodiumCatShare::Completion done) { ++stops; done(stopError); };
        return h;
    }
};

quint16 listen(DecodiumCatShare& server, bool control = true, bool tx = true)
{
    if (!server.configure(true, 0, control, tx)) return 0;
    return server.findChild<QTcpServer*>()->serverPort();
}

QByteArray exchange(QTcpSocket& client, const QByteArray& command, int lines = 1)
{
    client.write(command + '\n');
    QByteArray reply;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 4000 && reply.count('\n') < lines) {
        QTest::qWait(1);
        reply += client.readAll();
    }
    return reply;
}
}

class TestCatShareCw : public QObject {
    Q_OBJECT
private slots:
    void actualHamlibNetRigctlClient()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QProcess client;
        client.start(QString::fromUtf8(HAMLIB_TEST_CLIENT),
                     {QStringLiteral("127.0.0.1:%1").arg(listen(server))});
        QVERIFY(client.waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QProcess::NotRunning, 15000);
        const QByteArray output = client.readAllStandardOutput() + client.readAllStandardError();
        QVERIFY2(client.exitStatus() == QProcess::NormalExit && client.exitCode() == 0, output.constData());
        QCOMPARE(radio.wpm, 35);
        QCOMPARE(radio.sent, QStringList({"?"}));
        QVERIFY(radio.stops >= 1);
        QVERIFY(radio.ptt.isEmpty());
    }
    void idleAndPermissions()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server, false, false));
        QCOMPARE(exchange(client, "f"), QByteArray("7030000\n"));
        QVERIFY(radio.sent.isEmpty());
        QCOMPARE(radio.stops, 0);
        QCOMPARE(exchange(client, "b CQ"), QByteArray("RPRT -1\n"));
        QCOMPARE(exchange(client, "L KEYSPD 24"), QByteArray("RPRT -1\n"));
        QVERIFY(server.configure(true, 0, true, false));
        QCOMPARE(exchange(client, "L KEYSPD 24"), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.wpm, 24);
        QCOMPARE(exchange(client, "b CQ"), QByteArray("RPRT -1\n"));
        QVERIFY(radio.sent.isEmpty());
        QVERIFY(radio.ptt.isEmpty());
    }

    void speedReadbackAndValidation()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QCOMPARE(exchange(client, "+\\set_level KEYSPD 35"), QByteArray("RPRT 0\n"));
        QCOMPARE(exchange(client, "l KEYSPD"), QByteArray("35\n"));
        QCOMPARE(exchange(client, "+l KEYSPD", 3), QByteArray("get_level: KEYSPD\nLevel Value: 35\nRPRT 0\n"));
        QCOMPARE(exchange(client, "L KEYSPD 61"), QByteArray("RPRT -1\n"));
        QCOMPARE(exchange(client, "L KEYSPD junk"), QByteArray("RPRT -1\n"));
        QCOMPARE(radio.wpm, 35);
        radio.speedError = -11;
        QCOMPARE(exchange(client, "l KEYSPD"), QByteArray("RPRT -11\n"));
        QCOMPARE(exchange(client, "L KEYSPD 30"), QByteArray("RPRT -11\n"));
        QCOMPARE(radio.wpm, 35);
    }

    void asynchronousRepliesStayInRequestOrder()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QCOMPARE(exchange(client, "L KEYSPD 25\nl KEYSPD\nf", 3), QByteArray("RPRT 0\n25\n7030000\n"));
    }

    void sendsLiteralSingleCharacterWithoutPttOn()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QCOMPARE(exchange(client, "+\\send_morse ?"), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.sent, QStringList({"?"}));
        QVERIFY(radio.ptt.isEmpty());
        QCOMPARE(exchange(client, "\\stop_morse"), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.stops, 1);
        QCOMPARE(exchange(client, "T 0"), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.stops, 2);
        QCOMPARE(exchange(client, QByteArray(1, char(0xbb))), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.stops, 3);
    }

    void reportsRealErrorsAndInvalidPayloads()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        radio.available = false;
        QCOMPARE(exchange(client, "b CQ"), QByteArray("RPRT -11\n"));
        radio.available = true;
        QCOMPARE(exchange(client, "b"), QByteArray("RPRT -1\n"));
        QCOMPARE(exchange(client, "b CQ;TX1;"), QByteArray("RPRT -1\n"));
        QVERIFY(radio.sent.isEmpty());
        radio.sendError = -6;
        QCOMPARE(exchange(client, "b CQ"), QByteArray("RPRT -6\n"));
        QCOMPARE(radio.stops, 1);
        radio.stopError = -5;
        QCOMPARE(exchange(client, "\\stop_morse"), QByteArray("RPRT -5\n"));
        radio.connected = false;
        QCOMPARE(exchange(client, "b CQ"), QByteArray("RPRT -6\n"));
    }

    void sendsWhenOptionalSpeedReadIsUnavailable()
    {
        FakeRadio radio;
        radio.speedError = -11;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QCOMPARE(exchange(client, "b TEST"), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.sent, QStringList({"TEST"}));
    }

    void longMessagesRespectFiniteRadioBuffer()
    {
        FakeRadio radio;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        const QByteArray text(61, 'E');
        QCOMPARE(exchange(client, "b " + text), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.sent.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(radio.sent.size(), 3, 8000);
        QCOMPARE(radio.sent.join(QString()), QString::fromLatin1(text));
        for (const auto& chunk : radio.sent) QVERIFY(chunk.size() <= 30);
        QVERIFY(radio.ptt.isEmpty());
    }

    void stopInvalidatesLateAcknowledgementAndQueuedText()
    {
        FakeRadio radio;
        radio.holdSend = true;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QTRY_VERIFY(client.state() == QAbstractSocket::ConnectedState);
        client.write("b EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n");
        QTRY_VERIFY(bool(radio.pending));
        QCOMPARE(exchange(client, "\\stop_morse", 2), QByteArray("RPRT -9\nRPRT 0\n"));
        radio.pending(0);
        QTest::qWait(2800);
        QCOMPARE(radio.sent.size(), 1);
        QCOMPARE(radio.stops, 1);
        QCOMPARE(client.bytesAvailable(), 0);
    }

    void disconnectOrRevokedPermissionStopsQueue_data()
    {
        QTest::addColumn<bool>("disconnect");
        QTest::newRow("disconnect") << true;
        QTest::newRow("revoke-tx") << false;
    }
    void disconnectOrRevokedPermissionStopsQueue()
    {
        QFETCH(bool, disconnect);
        FakeRadio radio;
        radio.limit = 1;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QCOMPARE(exchange(client, "b EEE"), QByteArray("RPRT 0\n"));
        if (disconnect) client.disconnectFromHost();
        else QVERIFY(server.configure(true, 0, true, false));
        QTRY_COMPARE(radio.stops, 1);
        QTest::qWait(500);
        QCOMPARE(radio.sent, QStringList({"E"}));
    }

    void secondClientCannotInterleaveButCanStop()
    {
        FakeRadio radio;
        radio.limit = 1;
        DecodiumCatShare server(radio.hooks());
        const quint16 port = listen(server);
        QTcpSocket first, second;
        first.connectToHost(QHostAddress::LocalHost, port);
        second.connectToHost(QHostAddress::LocalHost, port);
        QCOMPARE(exchange(first, "b TTT"), QByteArray("RPRT 0\n"));
        QCOMPARE(exchange(second, "b TEST"), QByteArray("RPRT -9\n"));
        QCOMPARE(exchange(second, "\\stop_morse"), QByteArray("RPRT 0\n"));
        QTest::qWait(500);
        QCOMPARE(radio.sent, QStringList({"T"}));
        QCOMPARE(radio.stops, 1);
    }

    void localStopInvalidatesSharedQueueAndAllowsNewMessage()
    {
        FakeRadio radio;
        radio.holdSend = true;
        radio.limit = 1;
        DecodiumCatShare server(radio.hooks());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QTRY_VERIFY(client.state() == QAbstractSocket::ConnectedState);
        client.write("b EEE\n");
        QTRY_VERIFY(bool(radio.pending));
        // Called by the production adapter on morseStopRequested.
        server.cancelMorse();
        QTRY_VERIFY(client.bytesAvailable() > 0);
        QCOMPARE(client.readAll(), QByteArray("RPRT -9\n"));
        radio.pending(0);
        QTest::qWait(500);
        QCOMPARE(radio.sent, QStringList({"E"}));
        radio.holdSend = false;
        QCOMPARE(exchange(client, "b T"), QByteArray("RPRT 0\n"));
        QCOMPARE(radio.sent, QStringList({"E", "T"}));
    }

    void cachedTelemetryAndControlsArePreserved()
    {
        FakeRadio radio;
        auto hooks = radio.hooks();
        DecodiumCatShare::State state;
        state.connected = state.ptt = state.strengthValid = state.alcValid = true;
        state.frequency = 7030000; state.txFrequency = 7030500; state.mode = QStringLiteral("DATA-U");
        state.strengthDb = -25; state.swr = 1.5; state.alc = 42; state.powerWatts = 17;
        hooks.state = [&]() { return state; };
        hooks.frequency = [&](double hz, bool tx) { (tx ? state.txFrequency : state.frequency) = hz; };
        hooks.mode = [&](const QString& value) { state.mode = value; };
        DecodiumCatShare server(hooks);
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, listen(server));
        QCOMPARE(exchange(client, "l SWR"), QByteArray("1.500000\n"));
        QCOMPARE(exchange(client, "l ALC"), QByteArray("0.420000\n"));
        QCOMPARE(exchange(client, "l RFPOWER_METER_WATTS"), QByteArray("17.000000\n"));
        QCOMPARE(exchange(client, "l STRENGTH"), QByteArray("-25.000000\n"));
        QCOMPARE(exchange(client, "m", 2), QByteArray("PKTUSB\n3000\n"));
        QCOMPARE(exchange(client, "F 14074000\nf", 2), QByteArray("RPRT 0\n14074000\n"));
        QCOMPARE(exchange(client, "I 14074500\ni", 2), QByteArray("RPRT 0\n14074500\n"));
        QCOMPARE(exchange(client, "M CW 500\nm", 3), QByteArray("RPRT 0\nCW\n3000\n"));
    }
};
QTEST_GUILESS_MAIN(TestCatShareCw)
#include "test_cat_share_cw.moc"
