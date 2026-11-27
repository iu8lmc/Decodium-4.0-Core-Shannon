#include <QtTest>

#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include "Network/wsprnet.h"

class TestWsprNetPost final : public QObject
{
  Q_OBJECT

private slots:
  void decodedSpotStartsUploaderAndPostsFormData ()
  {
    QTcpServer server;
    QVERIFY (server.listen (QHostAddress::LocalHost, 0));

    QNetworkAccessManager network;
    QUrl const endpoint {QStringLiteral ("http://127.0.0.1:%1/post/")
                           .arg (server.serverPort ())};
    WSPRNet uploader {&network, endpoint};
    QSignalSpy status_spy {&uploader, &WSPRNet::uploadStatus};

    uploader.post (QStringLiteral ("9H1SR"),
                   QStringLiteral ("JM75"),
                   QStringLiteral ("14.095600"),
                   QStringLiteral ("14.095600"),
                   QStringLiteral ("WSPR"),
                   120.0f,
                   QStringLiteral ("0"),
                   QStringLiteral ("0"),
                   QStringLiteral ("Decodium/test"),
                   QStringLiteral ("2256 -21 -0.3 14.097090 0 DU1MGA PK04 37"));

    QTRY_VERIFY_WITH_TIMEOUT (server.hasPendingConnections (), 2000);
    QTcpSocket * socket = server.nextPendingConnection ();
    QVERIFY (socket);

    QByteArray request = socket->readAll ();
    connect (socket, &QTcpSocket::readyRead, this, [&request, socket] {
      request += socket->readAll ();
    });

    QTRY_VERIFY_WITH_TIMEOUT (request.contains ("\r\n\r\n"), 2000);
    int const header_end = request.indexOf ("\r\n\r\n") + 4;
    QRegularExpression const length_re {
      QStringLiteral ("Content-Length:\\s*(\\d+)"),
      QRegularExpression::CaseInsensitiveOption};
    QRegularExpressionMatch const length_match =
      length_re.match (QString::fromLatin1 (request.left (header_end)));
    QVERIFY (length_match.hasMatch ());
    int const content_length = length_match.captured (1).toInt ();
    QTRY_VERIFY_WITH_TIMEOUT (request.size () >= header_end + content_length, 2000);

    QByteArray const body = request.mid (header_end, content_length);
    QVERIFY (body.contains ("function=wspr"));
    QVERIFY (body.contains ("rcall=9H1SR"));
    QVERIFY (body.contains ("rgrid=JM75"));
    QVERIFY (body.contains ("tcall=DU1MGA"));
    QVERIFY (body.contains ("tgrid=PK04"));
    QVERIFY (body.contains ("dbm=37"));
    QVERIFY (body.contains ("mode=2"));

    QByteArray const response_body {"1 spot(s) added"};
    QByteArray response {"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "};
    response += QByteArray::number (response_body.size ());
    response += "\r\nConnection: close\r\n\r\n";
    response += response_body;
    QCOMPARE (socket->write (response), qint64 (response.size ()));
    socket->flush ();

    auto has_status = [&status_spy] (QString const& expected, bool prefix) {
      for (QList<QVariant> const& arguments : status_spy)
        {
          QString const status = arguments.at (0).toString ();
          if ((prefix && status.startsWith (expected)) || (!prefix && status == expected))
            return true;
        }
      return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT (
      has_status (QStringLiteral ("Uploading Spot 1/1"), true), 2000);
    QTRY_VERIFY_WITH_TIMEOUT (has_status (QStringLiteral ("done"), false), 2000);

    socket->deleteLater ();
  }
};

QTEST_GUILESS_MAIN (TestWsprNetPost)
#include "test_wsprnet_post.moc"
