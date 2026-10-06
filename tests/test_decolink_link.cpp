#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include "src/net/DecolinkLink.h"
#include "src/net/DecolinkPacket.h"

using namespace decolink;

namespace {

// Un server di accesso che risponde sempre con lo stesso corpo JSON.
class FakeAuth : public QObject {
public:
    explicit FakeAuth(QObject* parent = nullptr) : QObject(parent)
    {
        if (!m_server.listen(QHostAddress::LocalHost)) qFatal("fake auth: listen failed");
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket* s = m_server.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [this, s]() {
                m_buf[s].append(s->readAll());
                const int end = m_buf[s].indexOf("\r\n\r\n");
                if (end < 0) return;
                const QByteArray head = m_buf[s].left(end);
                int len = 0;
                for (const QByteArray& l : head.split('\n'))
                    if (l.toLower().startsWith("content-length:"))
                        len = l.mid(15).trimmed().toInt();
                if (m_buf[s].size() < end + 4 + len) return;
                lastBody = QJsonDocument::fromJson(m_buf[s].mid(end + 4, len)).object();
                ++requests;
                m_buf.remove(s);
                const QByteArray body = QJsonDocument(reply).toJson(QJsonDocument::Compact);
                s->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\n"
                         "Content-Type: application/json\r\nContent-Length: "
                         + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                s->disconnectFromHost();
            });
        });
    }
    QString url() const { return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()); }

    QJsonObject reply;
    int status {200};
    QJsonObject lastBody;
    int requests {0};

private:
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buf;
};

QJsonObject okReply(const QString& role, bool canTx)
{
    QJsonObject st;
    st["slug"] = "shack"; st["name"] = "IU8LMC shack"; st["role"] = role;
    return QJsonObject{{"ok", true}, {"token", "dl1.fake.token"}, {"expires_in", 3600},
                       {"callsign", "IU8LMC"}, {"station", "shack"},
                       {"station_name", "IU8LMC shack"}, {"role", role},
                       {"can_transmit", canTx}, {"stations", QJsonArray{st}}};
}

// Un relay finto: risponde come quello vero e registra quello che riceve.
class FakeRelay : public QObject {
public:
    explicit FakeRelay(QObject* parent = nullptr) : QObject(parent)
    {
        if (!m_sock.bind(QHostAddress::LocalHost, 0)) qFatal("fake relay: bind failed");
        connect(&m_sock, &QUdpSocket::readyRead, this, [this]() {
            while (m_sock.hasPendingDatagrams()) {
                QByteArray dg(int(m_sock.pendingDatagramSize()), Qt::Uninitialized);
                m_sock.readDatagram(dg.data(), dg.size(), &m_client, &m_clientPort);
                Header h; QByteArray body;
                if (!parsePacket(dg, &h, &body)) continue;
                switch (h.flag) {
                case Register:
                    registers << QString::fromLatin1(body);
                    if (!silent) {
                        send(Register, h.seq, {});
                        if (announcePeer) send(PeerUp, 0, {});
                    }
                    break;
                case Ping:
                    send(Pong, h.seq, {}, h.tMs);
                    break;
                case CatReq: {
                    const QString line = QString::fromLatin1(body).trimmed();
                    cat << line;
                    if (!answerCat) break;
                    QByteArray rsp = "RPRT 0\n";
                    if (line == "f") rsp = "14074000\n";
                    else if (line == "m") rsp = "PKTUSB\n3000\n";
                    else if (line == "t") rsp = pttState ? "1\n" : "0\n";
                    else if (line == "l STRENGTH") rsp = "-12\n";
                    send(CatRsp, h.seq, rsp);
                    break;
                }
                case TxAudio:
                    txPackets << qMakePair(h, body);
                    break;
                default: break;
                }
            }
        });
    }
    quint16 port() const { return m_sock.localPort(); }
    bool hasClient() const { return m_clientPort != 0; }

    void send(quint8 flag, quint32 seq, const QByteArray& body, quint64 tMs = 0, quint32 rate = 48000)
    {
        if (!hasClient()) return;
        m_sock.writeDatagram(makePacket(flag, seq, body, rate,
                                        tMs ? tMs : quint64(QDateTime::currentMSecsSinceEpoch())),
                             m_client, m_clientPort);
    }
    // Un pacchetto d'audio ricevuto: 10 ms a 48 kHz di un tono.
    void sendAudio(quint32 seq, int hz = 1000, int rate = 48000, int amp = 8000)
    {
        const int n = rate / 100;
        QVector<short> s(n);
        for (int i = 0; i < n; ++i)
            s[i] = short(amp * std::sin(2.0 * kPi * hz * (double(seq) * n + i) / rate));
        send(Audio, seq, samplesToPcm(s.constData(), n), 0, quint32(rate));
    }

    bool silent {false};
    bool announcePeer {true};
    bool answerCat {true};
    bool pttState {false};
    QStringList registers;
    QStringList cat;
    QVector<QPair<Header, QByteArray>> txPackets;

private:
    QUdpSocket m_sock;
    QHostAddress m_client;
    quint16 m_clientPort {0};
};

QVector<short> tone(int hz, int rate, int n, double amp = 8000.0)
{
    QVector<short> s(n);
    for (int i = 0; i < n; ++i) s[i] = short(amp * std::sin(2.0 * kPi * hz * i / rate));
    return s;
}

double rms(const QVector<short>& s, int from = 0)
{
    double a = 0; int n = 0;
    for (int i = from; i < s.size(); ++i) { a += double(s[i]) * s[i]; ++n; }
    return n ? std::sqrt(a / n) : 0.0;
}

}  // namespace

class TestDecolinkLink : public QObject {
    Q_OBJECT

private:
    struct Rig {
        FakeAuth auth;
        FakeRelay relay;
        DecolinkLink link;
        QVector<short> rx;
        explicit Rig(const QString& role = "opr", bool canTx = true)
        {
            auth.reply = okReply(role, canTx);
            QObject::connect(&link, &DecolinkLink::rxAudio, &link, [this](const QVector<short>& s, quint64) { rx += s; });
        }
        void go()
        {
            link.connectTo(auth.url(), QStringLiteral("127.0.0.1"), relay.port(),
                           QStringLiteral("iu8lmc@example.org"), QStringLiteral("secret"));
        }
    };

private slots:
    void packetRoundTrip()
    {
        const QByteArray pkt = makePacket(CatReq, 0x01020304u, "f\n", 12000, 0x1122334455667788ULL);
        QCOMPARE(pkt.size(), kHdrSize + 2);
        Header h; QByteArray body;
        QVERIFY(parsePacket(pkt, &h, &body));
        QCOMPARE(int(h.flag), int(CatReq));
        QCOMPARE(h.seq, 0x01020304u);
        QCOMPARE(h.tMs, 0x1122334455667788ULL);
        QCOMPARE(h.rate, 12000u);
        QCOMPARE(body, QByteArray("f\n"));
    }

    void packetRejections()
    {
        Header h; QByteArray b;
        QVERIFY(!parsePacket(QByteArray("short"), &h, &b));
        QByteArray bad = makePacket(Audio, 1, {}, 48000, 1);
        bad[0] = 'X';
        QVERIFY(!parsePacket(bad, &h, &b));
        QByteArray v1 = makePacket(Audio, 1, {}, 48000, 1);
        v1[4] = 1;                      // la v1 non e' piu' accettata dal relay
        QVERIFY(!parsePacket(v1, &h, &b));
    }

    void pcmRoundTrip()
    {
        const short v[] = {0, 1, -1, 32767, -32768, 1234};
        const QByteArray pcm = samplesToPcm(v, 6);
        QCOMPARE(pcm.size(), 12);
        const QVector<short> back = pcmToSamples(pcm);
        QCOMPARE(back.size(), 6);
        for (int i = 0; i < 6; ++i) QCOMPARE(back[i], v[i]);
    }

    void decimatorPassesTheBandAndKillsTheRest()
    {
        Decimator d(4);
        const QVector<short> in1k = tone(1000, 48000, 4800);
        const QVector<short> out1k = d.process(in1k);
        QCOMPARE(out1k.size(), 1200);
        // in banda: l'ampiezza resta (salto il transitorio iniziale)
        QVERIFY2(rms(out1k, 100) > 0.95 * 8000.0 / std::sqrt(2.0), qPrintable(QString::number(rms(out1k, 100))));

        Decimator e(4);
        const QVector<short> out10k = e.process(tone(10000, 48000, 4800));
        // 10 kHz sta oltre la banda di uscita (6 kHz): deve sparire, non ripiegarsi
        QVERIFY2(rms(out10k, 100) < 0.01 * 8000.0, qPrintable(QString::number(rms(out10k, 100))));
    }

    void decimatorIsTheSameInPiecesAsInOneGo()
    {
        const QVector<short> in = tone(1500, 48000, 4800);
        Decimator whole(4);
        const QVector<short> a = whole.process(in);
        Decimator pieces(4);
        QVector<short> b;
        for (int i = 0; i < in.size(); i += 480)
            b += pieces.process(in.constData() + i, 480);
        QCOMPARE(a, b);
    }

    void factorsAndRates()
    {
        QCOMPARE(decimationFactorFor(48000), 4);
        QCOMPARE(decimationFactorFor(24000), 2);
        QCOMPARE(decimationFactorFor(12000), 1);
        QCOMPARE(decimationFactorFor(44100), 0);
        QCOMPARE(decimationFactorFor(0), 0);
    }

    void loginOperatorThenLinks()
    {
        Rig r;
        r.go();
        QTRY_VERIFY2_WITH_TIMEOUT(r.link.isLinked(), qPrintable(r.link.status()), 5000);
        QCOMPARE(r.auth.lastBody.value("email").toString(), QStringLiteral("iu8lmc@example.org"));
        QCOMPARE(r.link.role(), QStringLiteral("opr"));
        QVERIFY(r.link.canTransmit());
        QCOMPARE(r.link.callsign(), QStringLiteral("IU8LMC"));
        QCOMPARE(r.link.stationName(), QStringLiteral("IU8LMC shack"));
        QCOMPARE(r.link.rigLabel(), QStringLiteral("IU8LMC shack"));
        QCOMPARE(r.link.stationList().size(), 1);
        QVERIFY(!r.relay.registers.isEmpty());
        QCOMPARE(r.relay.registers.first(), QStringLiteral("op dl1.fake.token"));
    }

    void loginRefusedSaysWhy()
    {
        Rig r;
        r.auth.status = 401;
        r.auth.reply = QJsonObject{{"ok", false}, {"error", "email o password non corretti"}};
        r.go();
        QTRY_COMPARE_WITH_TIMEOUT(r.link.status(), QStringLiteral("email o password non corretti"), 5000);
        QVERIFY(!r.link.loggedIn());
        QVERIFY(!r.link.isLinked());
        QVERIFY(r.relay.registers.isEmpty());       // senza accesso non si bussa al relay
    }

    void loginAsksForTheStation()
    {
        Rig r;
        QJsonObject a; a["slug"] = "a"; a["name"] = "A"; a["role"] = "opr";
        QJsonObject b; b["slug"] = "b"; b["name"] = "B"; b["role"] = "lst";
        r.auth.reply = QJsonObject{{"ok", false}, {"need_station", true}, {"stations", QJsonArray{a, b}},
                                   {"error", "indica su quale stazione collegarti"}};
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.stationList().size() == 2, 5000);
        QVERIFY(!r.link.loggedIn());
        QCOMPARE(r.link.stationList().at(1).toMap().value("role").toString(), QStringLiteral("lst"));
    }

    void unreachableServerIsNotABadPassword()
    {
        DecolinkLink link;
        link.login(QStringLiteral("http://127.0.0.1:1"), QStringLiteral("a@b.c"), QStringLiteral("x"));
        QTRY_VERIFY_WITH_TIMEOUT(link.status().startsWith(QStringLiteral("Server not reachable")), 8000);
    }

    void receivesAudioAt12k()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        for (quint32 s = 1; s <= 50; ++s) r.relay.sendAudio(s);
        QTRY_VERIFY_WITH_TIMEOUT(r.rx.size() >= 50 * 120, 5000);
        QCOMPARE(r.rx.size(), 50 * 120);            // 10 ms a 12 kHz = 120 campioni
        // il tono da 1 kHz arriva intatto dopo il transitorio
        QVERIFY(rms(r.rx, 300) > 0.9 * 8000.0 / std::sqrt(2.0));
    }

    void audioGapIsFilledWithSilence()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        for (quint32 s = 1; s <= 10; ++s) r.relay.sendAudio(s);
        QTRY_VERIFY_WITH_TIMEOUT(r.rx.size() >= 10 * 120, 5000);
        r.relay.sendAudio(14);                      // 11, 12, 13 sono andati persi
        QTRY_VERIFY_WITH_TIMEOUT(r.rx.size() >= 14 * 120, 5000);
        QCOMPARE(r.rx.size(), 14 * 120);            // il periodo non si accorcia
    }

    void oldAndDuplicatePacketsAreDropped()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        for (quint32 s = 1; s <= 5; ++s) r.relay.sendAudio(s);
        QTRY_VERIFY_WITH_TIMEOUT(r.rx.size() >= 5 * 120, 5000);
        r.relay.sendAudio(5);
        r.relay.sendAudio(3);
        r.relay.sendAudio(6);
        QTRY_VERIFY_WITH_TIMEOUT(r.rx.size() >= 6 * 120, 5000);
        QTest::qWait(150);
        QCOMPARE(r.rx.size(), 6 * 120);
    }

    void unsupportedRateIsDiscarded()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.relay.sendAudio(1, 1000, 44100);
        QTest::qWait(150);
        QCOMPARE(r.rx.size(), 0);
    }

    void catPollReadsFrequencyModeAndPtt()
    {
        Rig r;
        r.relay.pttState = true;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(r.link.frequencyHz(), 14074000.0, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(r.link.modeName(), QStringLiteral("DIGU"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(r.link.ptt(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(r.link.sMeterDbm() < -80.0, 5000);   // -12 dB su S9 = -85 dBm
        QCOMPARE(r.link.sMeterDbm(), -85.0);
    }

    void tuningAndModeAreSentAsRigctl()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.link.tune(7074000.0);
        r.link.setModeName(QStringLiteral("DIGU"));
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("F 7074000")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("M PKTUSB 0")), 5000);
    }

    void pttAndTxAudioReachTheGateway()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.link.setPtt(true);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("T 1")), 5000);
        QVERIFY(r.link.ptt());

        const QVector<short> wave = tone(1500, 12000, 6000);       // mezzo secondo
        r.link.sendTxAudio(wave, 0);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.txPackets.size() >= 50, 5000);
        QCOMPARE(r.relay.txPackets.size(), 50);
        QVector<short> back;
        quint32 prev = 0;
        for (const auto& p : r.relay.txPackets) {
            QCOMPARE(p.first.rate, 12000u);
            QCOMPARE(p.second.size(), 240);                        // 120 campioni
            if (prev) QCOMPARE(p.first.seq, prev + 1);
            prev = p.first.seq;
            back += pcmToSamples(p.second);
        }
        QCOMPARE(back, wave);                                      // nessuna alterazione
        r.link.setPtt(false);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("T 0")), 5000);
    }

    void txAudioIsPacedInRealTime()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.link.setPtt(true);
        QElapsedTimer t; t.start();
        r.link.sendTxAudio(tone(1500, 12000, 12000), 0);           // un secondo
        QTest::qWait(300);
        const int sofar = r.relay.txPackets.size();
        // non e' partito tutto in un colpo: dopo 300 ms ce ne sono ~30, non 100
        QVERIFY2(sofar >= 20 && sofar <= 45, qPrintable(QString::number(sofar)));
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.txPackets.size() == 100, 5000);
        QVERIFY(t.elapsed() >= 900);
    }

    // Il bridge consegna il segnale a pezzi da 40 ms, tutti insieme e in anticipo,
    // ognuno col suo istante: nessun pezzo deve cancellare i precedenti.
    void txAudioChunksAreQueuedNotReplaced()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.link.setPtt(true);
        const QVector<short> wave = tone(1500, 12000, 12000);      // un secondo
        const quint64 t0 = quint64(QDateTime::currentMSecsSinceEpoch()) * 1000000ull + 200000000ull;
        for (int off = 0, i = 0; off < wave.size(); off += 480, ++i)
            r.link.sendTxAudio(wave.mid(off, 480), t0 + quint64(i) * 40000000ull);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.txPackets.size() >= 100, 5000);
        QCOMPARE(r.relay.txPackets.size(), 100);
        QVector<short> back;
        for (const auto& p : r.relay.txPackets)
            back += pcmToSamples(p.second);
        QCOMPARE(back, wave);
    }

    void listenerCannotTransmitOrPoll()
    {
        Rig r("lst", false);
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        QVERIFY(!r.link.canTransmit());
        r.link.setPtt(true);
        r.link.sendTxAudio(tone(1500, 12000, 1200), 0);
        r.link.tune(14074000.0);
        QTest::qWait(1500);                                        // ci sarebbe stato almeno un giro di poll
        QVERIFY(r.relay.cat.isEmpty());
        QCOMPARE(r.relay.txPackets.size(), 0);
        QVERIFY(!r.link.ptt());
        QVERIFY(r.link.status().contains(QStringLiteral("listen-only")));
        // ma l'audio lo riceve
        for (quint32 s = 1; s <= 5; ++s) r.relay.sendAudio(s);
        QTRY_VERIFY_WITH_TIMEOUT(r.rx.size() >= 5 * 120, 5000);
    }

    void waitsForTheStationWhenNoGateway()
    {
        Rig r;
        r.relay.announcePeer = false;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(!r.relay.registers.isEmpty(), 5000);
        QTest::qWait(300);
        QVERIFY(!r.link.isLinked());                               // registrato, ma senza gateway
        QVERIFY(r.link.status().contains(QStringLiteral("waiting")));
        r.relay.send(PeerUp, 0, {});
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
    }

    void relayRestartMakesItRegisterAgain()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        const int before = r.relay.registers.size();
        r.relay.send(Denied, 0, "non registrato");
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.registers.size() > before, 1500);
    }

    void expiredTokenLogsInAgain()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        QCOMPARE(r.auth.requests, 1);
        // il rifiuto per credenziale scaduta ripete l'accesso (non appena la guardia dei
        // 10 s lo consente: si forza spostando indietro l'ultimo accesso)
        QTest::qWait(10100);
        r.auth.reply = okReply("opr", true);
        r.auth.reply["token"] = "dl1.second.token";
        r.relay.send(Denied, 0, "chiave scaduto");
        QTRY_COMPARE_WITH_TIMEOUT(r.auth.requests, 2, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.registers.contains(QStringLiteral("op dl1.second.token")), 5000);
    }

    void disconnectReleasesPttThreeTimes()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.link.setPtt(true);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("T 1")), 5000);
        r.relay.cat.clear();
        r.link.disconnectFromRelay();
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.count(QStringLiteral("T 0")) >= 3, 5000);
        QVERIFY(!r.link.isLinked());
        QVERIFY(!r.link.loggedIn());
    }

    void pttLeftOnWithoutAudioIsReleased()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.link.setPtt(true);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("T 1")), 5000);
        r.relay.cat.clear();
        // nessun audio: dopo 3 s la rete di sicurezza lato client lo molla
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("T 0")), 6500);
        QVERIFY(!r.link.ptt());
    }
};

QTEST_GUILESS_MAIN(TestDecolinkLink)
#include "test_decolink_link.moc"
