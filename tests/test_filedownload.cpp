#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <functional>

#include "Network/FileDownload.hpp"

namespace
{
  struct HttpResponse
  {
    int status {200};
    QByteArray reason {"OK"};
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    qint64 repeated_body_bytes {0};
    qint64 declared_content_length {-1};
    char repeated_body_char {'x'};
  };

  class TestHttpServer final
    : public QObject
  {
  public:
    using Handler = std::function<HttpResponse (QByteArray const& method, QByteArray const& path)>;

    explicit TestHttpServer (QObject * parent = nullptr)
      : QObject {parent}
    {
      connect (&server_, &QTcpServer::newConnection, this, &TestHttpServer::on_new_connection);
    }

    bool listen ()
    {
      if (server_.listen (QHostAddress::LocalHost, 0))
        {
          return true;
        }
      return server_.listen (QHostAddress::AnyIPv4, 0);
    }

    QString error_string () const
    {
      return server_.errorString ();
    }

    QUrl url (QString const& path) const
    {
      return QUrl {QStringLiteral ("http://127.0.0.1:%1%2").arg (server_.serverPort ()).arg (path)};
    }

    void set_handler (Handler handler)
    {
      handler_ = std::move (handler);
    }

  private:
    void on_new_connection ()
    {
      while (auto * socket = server_.nextPendingConnection ())
        {
          connect (socket, &QTcpSocket::readyRead, this, [this, socket] () { on_ready_read (socket); });
          connect (socket, &QTcpSocket::disconnected, this, [this, socket] () {
            buffers_.remove (socket);
            socket->deleteLater ();
          });
        }
    }

    void on_ready_read (QTcpSocket * socket)
    {
      auto& buffer = buffers_[socket];
      buffer += socket->readAll ();
      auto const request_end = buffer.indexOf ("\r\n\r\n");
      if (request_end < 0)
        {
          return;
        }

      auto const request_line_end = buffer.indexOf ("\r\n");
      auto const request_line = buffer.left (request_line_end);
      auto const parts = request_line.split (' ');
      QByteArray const method = parts.value (0);
      QByteArray const path = parts.value (1);

      HttpResponse response;
      if (handler_)
        {
          response = handler_ (method, path);
        }

      QByteArray const body = method == "HEAD" ? QByteArray {} : response.body;
      qint64 const implicit_length = method == "HEAD"
        ? (!response.body.isEmpty () ? response.body.size () : response.repeated_body_bytes)
        : (response.repeated_body_bytes > 0 ? response.repeated_body_bytes : body.size ());
      qint64 const content_length = response.declared_content_length >= 0
        ? response.declared_content_length
        : implicit_length;

      QByteArray payload;
      payload += "HTTP/1.1 " + QByteArray::number (response.status) + ' ' + response.reason + "\r\n";
      payload += "Connection: close\r\n";
      payload += "Content-Length: " + QByteArray::number (content_length) + "\r\n";
      for (auto const& header : response.headers)
        {
          payload += header.first + ": " + header.second + "\r\n";
        }
      payload += "\r\n";
      socket->write (payload);

      if (method != "HEAD")
        {
          if (response.repeated_body_bytes > 0)
            {
              QByteArray chunk (1024 * 1024, response.repeated_body_char);
              qint64 remaining = response.repeated_body_bytes;
              while (remaining > 0)
                {
                  auto const to_write = static_cast<int> (qMin<qint64> (remaining, chunk.size ()));
                  socket->write (chunk.constData (), to_write);
                  remaining -= to_write;
                }
            }
          else if (!body.isEmpty ())
            {
              socket->write (body);
            }
        }

      socket->flush ();
      if (response.declared_content_length > body.size () && response.repeated_body_bytes == 0)
        {
          socket->abort ();
        }
      else
        {
          socket->disconnectFromHost ();
        }
    }

    QTcpServer server_;
    Handler handler_;
    QHash<QTcpSocket *, QByteArray> buffers_;
  };
}

class TestFileDownload
  : public QObject
{
  Q_OBJECT

private:
  QTemporaryDir temp_dir_;

  QString temp_file_path (QString const& name) const
  {
    return temp_dir_.filePath (name);
  }

  Q_SLOT void download_retries_head_with_get_and_saves_body ()
  {
    TestHttpServer server;
    if (!server.listen ())
      {
        QSKIP (qPrintable (QStringLiteral ("loopback HTTP server unavailable: %1")
                               .arg (server.error_string ())));
      }
    server.set_handler ([] (QByteArray const& method, QByteArray const& path) {
      if (path != QByteArray {"/head-get"})
        {
          HttpResponse response;
          response.status = 404;
          response.reason = "Not Found";
          return response;
        }
      if (method == "HEAD")
        {
          HttpResponse response;
          response.status = 405;
          response.reason = "Method Not Allowed";
          return response;
        }

      HttpResponse response;
      response.body = QByteArray {"download-ok"};
      return response;
    });

    QNetworkAccessManager manager;
    FileDownload download;
    QSignalSpy complete_spy (&download, SIGNAL (complete (QString)));
    QSignalSpy error_spy (&download, SIGNAL (error (QString)));
    auto const destination = temp_file_path (QStringLiteral ("head_get.txt"));
    download.configure (&manager, server.url (QStringLiteral ("/head-get")).toString (), destination, QStringLiteral ("test-agent"));

    download.start_download ();

    QTRY_COMPARE_WITH_TIMEOUT (complete_spy.count (), 1, 5000);
    QCOMPARE (error_spy.count (), 0);

    QFile file {destination};
    QVERIFY (file.open (QIODevice::ReadOnly));
    QCOMPARE (file.readAll (), QByteArray {"download-ok"});
  }

  Q_SLOT void download_rejects_redirect_to_unsupported_scheme ()
  {
    TestHttpServer server;
    if (!server.listen ())
      {
        QSKIP (qPrintable (QStringLiteral ("loopback HTTP server unavailable: %1")
                               .arg (server.error_string ())));
      }
    server.set_handler ([] (QByteArray const&, QByteArray const& path) {
      if (path != QByteArray {"/redirect-bad"})
        {
          HttpResponse response;
          response.status = 404;
          response.reason = "Not Found";
          return response;
        }
      HttpResponse response;
      response.status = 302;
      response.reason = "Found";
      response.headers.append ({QByteArray {"Location"}, QByteArray {"ftp://example.com/evil.txt"}});
      return response;
    });

    QNetworkAccessManager manager;
    FileDownload download;
    QSignalSpy complete_spy (&download, SIGNAL (complete (QString)));
    QSignalSpy error_spy (&download, SIGNAL (error (QString)));
    auto const destination = temp_file_path (QStringLiteral ("redirect_bad.txt"));
    download.configure (&manager, server.url (QStringLiteral ("/redirect-bad")).toString (), destination, QStringLiteral ("test-agent"));

    download.start_download ();

    QTRY_COMPARE_WITH_TIMEOUT (error_spy.count (), 1, 5000);
    QCOMPARE (complete_spy.count (), 0);
    auto const error_text = error_spy.takeFirst ().at (0).toString ();
    QVERIFY (error_text.contains (QStringLiteral ("Unsupported download URL or scheme"))
             || error_text.contains (QStringLiteral ("Unknown protocol specified")));
  }

  Q_SLOT void download_rejects_oversized_body ()
  {
    TestHttpServer server;
    if (!server.listen ())
      {
        QSKIP (qPrintable (QStringLiteral ("loopback HTTP server unavailable: %1")
                               .arg (server.error_string ())));
      }
    server.set_handler ([] (QByteArray const& method, QByteArray const& path) {
      if (path != QByteArray {"/oversized"})
        {
          HttpResponse response;
          response.status = 404;
          response.reason = "Not Found";
          return response;
        }
      HttpResponse response;
      if (method == "HEAD")
        {
          response.headers.append ({QByteArray {"Content-Type"}, QByteArray {"application/octet-stream"}});
          return response;
        }

      response.repeated_body_bytes = (64LL * 1024 * 1024) + 1024;
      response.headers.append ({QByteArray {"Content-Type"}, QByteArray {"application/octet-stream"}});
      response.declared_content_length = (64LL * 1024 * 1024) + 1024;
      response.body = QByteArray {"x"};
      return response;
    });

    QNetworkAccessManager manager;
    FileDownload download;
    QSignalSpy complete_spy (&download, SIGNAL (complete (QString)));
    QSignalSpy error_spy (&download, SIGNAL (error (QString)));
    auto const destination = temp_file_path (QStringLiteral ("oversized.bin"));
    download.configure (&manager, server.url (QStringLiteral ("/oversized")).toString (), destination, QStringLiteral ("test-agent"));

    download.start_download ();

    QTRY_COMPARE_WITH_TIMEOUT (error_spy.count (), 1, 10000);
    QCOMPARE (complete_spy.count (), 0);
    QVERIFY (error_spy.takeFirst ().at (0).toString ().contains (QStringLiteral ("exceeds limit")));
  }
};

QTEST_MAIN (TestFileDownload);

#include "test_filedownload.moc"
