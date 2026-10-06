#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include "src/net/DecolinkLink.h"
#include "src/net/DecolinkLossless.h"
#include "src/net/DecolinkPacket.h"
#include "src/net/DecolinkV3.h"

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
                if (v3::looksLikeV3(dg)) {
                    v3::Header vh; QByteArray vbody;
                    v3::parsePacket(dg, &vh, &vbody);
                    switch (vh.type) {
                    case v3::Ctrl:
                        v3Ctrl << vbody;
                        if (answerChoose && vbody.size() >= 2 && uchar(vbody.at(0)) == v3::Choose) {
                            QByteArray act; act.append(char(v3::Active)); act.append(vbody.at(1));
                            sendV3(v3::Ctrl, quint8(vbody.at(1)), 0, act);
                        }
                        break;
                    case v3::AudioTx: v3Tx << qMakePair(vh, vbody); break;
                    case v3::Nack:    v3Nack << vbody; break;
                    default: break;
                    }
                    continue;
                }
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

    void sendV3(quint8 type, quint8 profile, quint16 seq, const QByteArray& body)
    {
        if (!hasClient()) return;
        m_sock.writeDatagram(v3::makePacket(type, profile, 0, 0, seq, 0, body), m_client, m_clientPort);
    }
    // Un blocco dei digitali: 40 ms a 12 kHz compressi senza perdite.
    void sendDigi(quint16 seq, const QVector<short>& s)
    {
        sendV3(v3::AudioRx, v3::Digi, seq, lossless::comprimi(s.constData(), s.size()));
    }

    bool answerChoose {true};
    QVector<QByteArray> v3Ctrl;
    QVector<QPair<v3::Header, QByteArray>> v3Tx;
    QVector<QByteArray> v3Nack;
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

const char* kGoldenBlockHex =
    "01e088d9ff390ecf187c111cfef7ee37e982edcd1693fa1bf418f9700059fedd0378feb1bdf57cfb73a02f9fe27bc7b302d0963cfb831d1b43ff69b55192315c25b76cea91465f12112f58092dab6fe28ae0cd2c518b863349309c18a4676e2b5144f035a06a870ec20b7f2ae21e4ae05e2c9a826a0e1ca61992707b7dc1af19ab002d52147d32fb1e62b6392c28d67a21c239f8197e46d149abf3d8dae24c56e508db692207f1342a9ddb8c22de86d5256c875db50ca08f57314b554246e7fd3a85b8620a57b2fbba75d8ea844ae73a1212b6648c244fd3ff3fc34a8c54f1ba15f6bed335993a41bef71a6e0f25e02208c2726036a204479c5a812b8401077b499118e27e49091b8bde6eb8d4eb7239f3ee113d4e6f8ea553f7ac6914e62f5b755d2be2e97a8cff9ea8140458a839ff765723ae012caa8fe1288e7a9954f4961c84b89ab12663fd8724bb59332388a578faf4b8111c84ecf7346062fc535ff23264834ef665d0b077520859ab8bf342332ca318f7ee75115b0a65f310598476f2bbefad930e68f2cbfa7069a8d8ceacb98681a41714a67fe902c0a4e089b77076a75480e70a8e776a8c361f6ec2c5d8728f6097f42c11f8b9c605839511039f12dd40e8022b3cf91d7a53e9ad6d600aa7023b29db58beb50541815726856ba5cf28a8dae20ef284926292945eb272a95be8f257ce2c961f7c2333a8b2fcabe7a67b917264cd468abc49490e1a4ab8dba27dfa522faea73f277fa323559313bfa3073d634d42b34c528786ce67090508d11a7496888bfce9bcbd7c9895b5f29438916114ef83b0d6776a67a6a26d2c9d3fff0c53101d07e855d615bc4bcc9fff8e726a149d7fed69e08978b0c8b39e251f938c44e953a9536418a4d0093f7a2be174cb6b9374355d8386a0e6be3afa4637cda1b028e417ba7e90f2e7";
const char* kGoldenSilenceHex = "00280000ffffffffff";

// 480 campioni: un tono a 1500 Hz piu' rumore da un generatore congruenziale.
QVector<short> goldenSignal()
{
    QVector<short> s(480);
    quint32 st = 12345;
    for (int i = 0; i < 480; ++i) {
        st = st * 1664525u + 1013904223u;
        const double n = double(int((st >> 16) & 0x3FF) - 512);
        s[i] = short(std::llround(6000.0 * std::sin(2.0 * kPi * 1500.0 * i / 12000.0) + n));
    }
    return s;
}

QVector<short> digiBlock(int index)
{
    QVector<short> s(480);
    for (int i = 0; i < 480; ++i)
        s[i] = short(6000.0 * std::sin(2.0 * kPi * 1000.0 * (index * 480 + i) / 12000.0)
                     + (index % 7) * 10);
    return s;
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

    // ── v3: digitali senza perdite ─────────────────────────────────────────

    void losslessRoundTrip()
    {
        QVector<QVector<short>> cases;
        cases << tone(1500, 12000, 480) << tone(300, 12000, 480, 30000.0)
              << QVector<short>(480, 0) << QVector<short>(480, 32767) << QVector<short>(480, -32768)
              << tone(1000, 12000, 1) << tone(1000, 12000, 2) << tone(1000, 12000, 7)
              << tone(1000, 12000, 40000) << goldenSignal();
        QVector<short> noise(480);
        quint32 st = 99;
        for (short& v : noise) { st = st * 1664525u + 1013904223u; v = short(st >> 16); }
        cases << noise;
        QVector<short> alt(480);                       // alternanza di estremi: il caso peggiore per il predittore
        for (int i = 0; i < alt.size(); ++i) alt[i] = (i & 1) ? 32767 : -32768;
        cases << alt;
        for (const auto& c : cases) {
            const QByteArray z = lossless::comprimi(c.constData(), c.size());
            QVERIFY(!z.isEmpty());
            QCOMPARE(lossless::decomprimi(z), c);      // identico, campione per campione
        }
    }

    void losslessRefusesBrokenBlocks()
    {
        const QByteArray z = lossless::comprimi(goldenSignal().constData(), 480);
        QVERIFY(lossless::decomprimi(QByteArray()).isEmpty());
        QVERIFY(lossless::decomprimi(z.left(3)).isEmpty());
        QVERIFY(lossless::decomprimi(z.left(z.size() / 2)).isEmpty());     // tagliato
        for (int i = 0; i < 300; ++i) {                // spazzatura: nessun crash, nessuna lettura fuori
            QByteArray g(40 + i % 50, 0);
            for (int k = 0; k < g.size(); ++k) g[k] = char((i * 37 + k * 11) & 0xFF);
            lossless::decomprimi(g);
        }
    }

    // Il formato del blocco e' quello del gateway Decolink: questi byte vengono
    // dal suo codificatore, e se cambiassero per qualunque motivo client e
    // gateway smetterebbero di capirsi in silenzio.
    void losslessMatchesTheGatewayFormat()
    {
        const QByteArray golden = QByteArray::fromHex(kGoldenBlockHex);
        QCOMPARE(golden.size(), 668);
        QCOMPARE(lossless::comprimi(goldenSignal().constData(), 480), golden);
        QCOMPARE(lossless::decomprimi(golden), goldenSignal());
        const QByteArray silence = QByteArray::fromHex(kGoldenSilenceHex);
        QCOMPARE(lossless::comprimi(QVector<short>(40, 0).constData(), 40), silence);
    }

    void v3PacketRoundTrip()
    {
        const QByteArray pkt = v3::makePacket(v3::AudioTx, v3::Digi, 0x5, 0, 0xFFFE, 0x01020304u, "ab");
        QCOMPARE(pkt.size(), v3::kHdr + 2);
        QCOMPARE(int(pkt.at(0)), int('D'));
        v3::Header h; QByteArray body;
        QVERIFY(v3::parsePacket(pkt, &h, &body));
        QCOMPARE(int(h.type), int(v3::AudioTx));
        QCOMPARE(int(h.profile), int(v3::Digi));
        QCOMPARE(int(h.flags), 5);
        QCOMPARE(h.seq, quint16(0xFFFE));
        QCOMPARE(h.time, 0x01020304u);
        QCOMPARE(body, QByteArray("ab"));
        QVERIFY(!v3::looksLikeV3(QByteArray("HFGW1234567890")));
        QVERIFY(!v3::looksLikeV3(pkt.left(9)));
        QByteArray v2 = pkt; v2[1] = char((2 << 4) | 1);
        QVERIFY(!v3::looksLikeV3(v2));
    }

    void digiProfileIsRequestedAndRetried()
    {
        Rig r;
        r.link.setAudioProfile(v3::Digi);
        r.relay.answerChoose = false;                  // il gateway non risponde: si ripete
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.v3Ctrl.size() >= 2, 5000);
        QCOMPARE(int(uchar(r.relay.v3Ctrl.at(0).at(0))), int(v3::Hello));
        QCOMPARE(int(uchar(r.relay.v3Ctrl.at(1).at(0))), int(v3::Choose));
        QCOMPARE(int(uchar(r.relay.v3Ctrl.at(1).at(1))), int(v3::Digi));
        QCOMPARE(r.link.activeProfile(), -1);
        const int before = r.relay.v3Ctrl.size();
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.v3Ctrl.size() > before, 8000);   // al giro del keepalive
        r.relay.answerChoose = true;
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 8000);
        QVERIFY(r.link.activeProfileName().contains(QStringLiteral("lossless"), Qt::CaseInsensitive)
                || !r.link.activeProfileName().isEmpty());
    }

    void digiRxInOrder()
    {
        Rig r;
        r.link.setAudioProfile(v3::Digi);
        r.go();
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 5000);
        QVector<short> all;
        for (int i = 0; i < 12; ++i) {
            r.relay.sendDigi(quint16(65530 + i), digiBlock(i));     // attraversa il giro del contatore
            all += digiBlock(i);
        }
        QTRY_COMPARE_WITH_TIMEOUT(r.rx.size(), all.size(), 3000);
        QCOMPARE(r.rx, all);                                        // esattamente i campioni del rig
        QCOMPARE(r.link.lostBlocks(), 0);
    }

    void digiRxReorderedBlockNeedsNoRetransmission()
    {
        Rig r;
        r.link.setAudioProfile(v3::Digi);
        r.go();
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 5000);
        r.relay.sendDigi(10, digiBlock(0));
        r.relay.sendDigi(12, digiBlock(2));
        QTest::qWait(5);
        r.relay.sendDigi(11, digiBlock(1));                         // arriva subito dopo
        r.relay.sendDigi(13, digiBlock(3));
        QVector<short> all;
        for (int i = 0; i < 4; ++i) all += digiBlock(i);
        QTRY_COMPARE_WITH_TIMEOUT(r.rx.size(), all.size(), 3000);
        QCOMPARE(r.rx, all);
        QCOMPARE(r.link.lostBlocks(), 0);
    }

    void digiRxLostBlockIsRequestedAndRecovered()
    {
        Rig r;
        r.link.setAudioProfile(v3::Digi);
        r.go();
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 5000);
        r.relay.sendDigi(20, digiBlock(0));
        r.relay.sendDigi(21, digiBlock(1));
        r.relay.sendDigi(23, digiBlock(3));                         // il 22 si e' perso
        r.relay.sendDigi(24, digiBlock(4));
        QTRY_VERIFY_WITH_TIMEOUT(!r.relay.v3Nack.isEmpty(), 2000);
        const QByteArray n = r.relay.v3Nack.first();
        QCOMPARE(int(uchar(n.at(1))), 1);                           // chiede un blocco solo
        QCOMPARE(quint16((uchar(n.at(2)) << 8) | uchar(n.at(3))), quint16(22));
        QVERIFY(r.rx.size() == 2 * 480);                            // il resto aspetta il buco
        r.relay.sendDigi(22, digiBlock(2));                         // il gateway lo rimanda
        QVector<short> all;
        for (int i = 0; i < 5; ++i) all += digiBlock(i);
        QTRY_COMPARE_WITH_TIMEOUT(r.rx.size(), all.size(), 3000);
        QCOMPARE(r.rx, all);
        QCOMPARE(r.link.lostBlocks(), 0);
        QCOMPARE(r.link.recoveredBlocks(), 1);
    }

    void digiRxBlockThatNeverComesIsFilledWithSilence()
    {
        Rig r;
        r.link.setAudioProfile(v3::Digi);
        r.go();
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 5000);
        r.relay.sendDigi(30, digiBlock(0));
        r.relay.sendDigi(32, digiBlock(2));                         // il 31 non tornera'
        QVector<short> all = digiBlock(0);
        all += QVector<short>(480, 0);
        all += digiBlock(2);
        QTRY_COMPARE_WITH_TIMEOUT(r.rx.size(), all.size(), 3000);
        QCOMPARE(r.rx, all);                                        // il tempo non si accorcia
        QCOMPARE(r.link.lostBlocks(), 1);
        r.relay.sendDigi(33, digiBlock(3));                         // e poi si riparte
        QTRY_COMPARE_WITH_TIMEOUT(r.rx.size(), all.size() + 480, 2000);
    }

    void digiRxIsDecodedEvenWhenNothingWasRequested()
    {
        Rig r;                                                      // profilo automatico
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.relay.sendDigi(1, digiBlock(0));
        QTRY_COMPARE_WITH_TIMEOUT(r.rx.size(), 480, 2000);
        QCOMPARE(r.rx, digiBlock(0));
        QCOMPARE(r.link.activeProfile(), int(v3::Digi));
        QVERIFY(r.relay.v3Ctrl.isEmpty());                          // seguire non e' chiedere
    }

    void unsupportedProfileInAutoMovesToDigi()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.relay.sendV3(v3::AudioRx, v3::Voice, 1, QByteArray(60, 'x'));      // Opus: non si legge
        QTRY_VERIFY_WITH_TIMEOUT(!r.relay.v3Ctrl.isEmpty(), 3000);
        QCOMPARE(int(uchar(r.relay.v3Ctrl.last().at(0))), int(v3::Choose));
        QCOMPARE(int(uchar(r.relay.v3Ctrl.last().at(1))), int(v3::Digi));
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 3000);
        QCOMPARE(r.rx.size(), 0);                                   // e dell'Opus non e' uscito rumore
    }

    void pcmProfileCanBeForcedBack()
    {
        Rig r;
        r.link.setAudioProfile(v3::Pcm48);
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!r.relay.v3Ctrl.isEmpty(), 3000);
        QCOMPARE(int(uchar(r.relay.v3Ctrl.last().at(1))), int(v3::Pcm48));
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Pcm48), 3000);
    }

    void digiTxSendsLosslessBlocksAndResendsOnRequest()
    {
        Rig r;
        r.link.setAudioProfile(v3::Digi);
        r.go();
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Digi), 5000);
        r.link.setPtt(true);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.cat.contains(QStringLiteral("T 1")), 5000);

        const QVector<short> wave = tone(1500, 12000, 12000);       // un secondo
        const quint64 t0 = quint64(QDateTime::currentMSecsSinceEpoch()) * 1000000ull + 200000000ull;
        for (int off = 0, i = 0; off < wave.size(); off += 480, ++i)
            r.link.sendTxAudio(wave.mid(off, 480), t0 + quint64(i) * 40000000ull);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.v3Tx.size() >= 25, 5000);
        QCOMPARE(r.relay.v3Tx.size(), 25);
        QVector<short> back;
        quint16 prev = 0;
        for (int i = 0; i < r.relay.v3Tx.size(); ++i) {
            const auto& p = r.relay.v3Tx.at(i);
            QCOMPARE(int(p.first.profile), int(v3::Digi));
            if (i) QCOMPARE(p.first.seq, quint16(prev + 1));
            prev = p.first.seq;
            back += lossless::decomprimi(p.second);
        }
        QCOMPARE(back, wave);                                       // nessuna alterazione

        // il gateway chiede indietro il blocco 5 e il 9
        QByteArray nack;
        nack.append(char(v3::Report)); nack.append(char(2));
        for (int i : {5, 9}) {
            const quint16 s = quint16(r.relay.v3Tx.at(0).first.seq + i);
            nack.append(char(s >> 8)); nack.append(char(s));
        }
        const int before = r.relay.v3Tx.size();
        r.relay.sendV3(v3::Nack, v3::Digi, 0, nack);
        QTRY_COMPARE_WITH_TIMEOUT(r.relay.v3Tx.size(), before + 2, 2000);
        QCOMPARE(r.relay.v3Tx.at(before).first.seq, quint16(r.relay.v3Tx.at(0).first.seq + 5));
        QCOMPARE(r.relay.v3Tx.at(before).second, r.relay.v3Tx.at(5).second);
        QCOMPARE(r.relay.v3Tx.at(before + 1).second, r.relay.v3Tx.at(9).second);
    }

    void txStaysPcmWhenTheStationIsPcm()
    {
        Rig r;
        r.go();
        QTRY_VERIFY_WITH_TIMEOUT(r.link.isLinked(), 5000);
        r.relay.sendAudio(1);                                       // il gateway parla PCM v2
        QTRY_COMPARE_WITH_TIMEOUT(r.link.activeProfile(), int(v3::Pcm48), 2000);
        r.link.setPtt(true);
        r.link.sendTxAudio(tone(1500, 12000, 1200), 0);
        QTRY_VERIFY_WITH_TIMEOUT(r.relay.txPackets.size() >= 10, 3000);
        QCOMPARE(r.relay.v3Tx.size(), 0);
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
