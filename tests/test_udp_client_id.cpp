#include <QtTest>

#include <QUdpSocket>

#include "Network/MessageClient.hpp"
#include "Network/NetworkMessage.hpp"
#include "Network/UdpClientId.hpp"

class TestUdpClientId final : public QObject
{
  Q_OBJECT

  static void verifyHeartbeat(QUdpSocket& socket, QString const& expectedId)
  {
    QTRY_VERIFY_WITH_TIMEOUT(socket.hasPendingDatagrams(), 3000);
    QByteArray datagram;
    datagram.resize(static_cast<int>(socket.pendingDatagramSize()));
    QCOMPARE(socket.readDatagram(datagram.data(), datagram.size()), qint64(datagram.size()));
    NetworkMessage::Reader reader {datagram};
    QCOMPARE(reader.type(), NetworkMessage::Heartbeat);
    QCOMPARE(reader.id(), expectedId);
  }

private slots:
  void normalizesConfiguredId()
  {
    QCOMPARE(decodium::network::normalizedUdpClientId(QString {}),
             QStringLiteral("Decodium"));
    // 1.0.538: ogni destinazione sceglie il proprio ripiego, cosi' la porta
    // primaria puo' restare "WSJTX" per i programmi locali che lo pretendono.
    QCOMPARE(decodium::network::normalizedUdpClientId(QString {}, QStringLiteral("WSJTX")),
             QStringLiteral("WSJTX"));
    QCOMPARE(decodium::network::normalizedUdpClientId(QStringLiteral("  Decodium  "),
                                                      QStringLiteral("WSJTX")),
             QStringLiteral("Decodium"));
    QCOMPARE(decodium::network::normalizedUdpClientId(QStringLiteral("  Deco   Client  ")),
             QStringLiteral("Deco Client"));
    QCOMPARE(decodium::network::normalizedUdpClientId(QString(80, QLatin1Char('A'))).size(), 64);
  }

  void serializesSameIdOnThreeUdpEndpoints()
  {
    QUdpSocket primary;
    QUdpSocket secondary;
    QUdpSocket tertiary;
    QVERIFY(primary.bind(QHostAddress::LocalHost, 0));
    QVERIFY(secondary.bind(QHostAddress::LocalHost, 0));
    QVERIFY(tertiary.bind(QHostAddress::LocalHost, 0));

    QString const configuredId = QStringLiteral("Decodium Mac Audit");
    MessageClient primaryClient {configuredId, QStringLiteral("test"), QStringLiteral("test"),
                                 QStringLiteral("127.0.0.1"), primary.localPort(), 0, {}, 1,
                                 this, QStringLiteral("test primary")};
    MessageClient secondaryClient {configuredId, QStringLiteral("test"), QStringLiteral("test"),
                                   QStringLiteral("127.0.0.1"), secondary.localPort(), 0, {}, 1,
                                   this, QStringLiteral("test secondary")};
    MessageClient tertiaryClient {configuredId, QStringLiteral("test"), QStringLiteral("test"),
                                  QStringLiteral("127.0.0.1"), tertiary.localPort(), 0, {}, 1,
                                  this, QStringLiteral("test tertiary")};

    verifyHeartbeat(primary, configuredId);
    verifyHeartbeat(secondary, configuredId);
    verifyHeartbeat(tertiary, configuredId);
  }

  void appliesClientIdChangeImmediately()
  {
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

    MessageClient client {QStringLiteral("Before"), QStringLiteral("test"), QStringLiteral("test"),
                          QStringLiteral("127.0.0.1"), receiver.localPort(), 0, {}, 1,
                          this, QStringLiteral("test live update")};
    verifyHeartbeat(receiver, QStringLiteral("Before"));

    client.set_client_id(QStringLiteral("  After   Update "));
    verifyHeartbeat(receiver, QStringLiteral("After Update"));
  }
};

QTEST_MAIN(TestUdpClientId)

#include "test_udp_client_id.moc"
