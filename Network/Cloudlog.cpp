#include "Cloudlog.hpp"

#include <future>
#include <chrono>

#include <QHash>
#include <QString>
#include <QDate>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSaveFile>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDebug>
#include <QMessageBox>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QCoreApplication>

#include "pimpl_impl.hpp"

#include "moc_Cloudlog.cpp"

#include "Configuration.hpp"

namespace
{
  // Dictionary mapping call sign to date of last upload to LotW
  using dictionary = QHash<QString, QDate>;
  constexpr qint64 kMaxCloudlogReplyBytes = 64 * 1024;

  enum class CloudlogApiKeyState
  {
    invalid,
    read_only,
    writable,
  };

  QUrl ensure_https_cloudlog_url (QString base, QString const& endpoint_path, QString * error = nullptr)
  {
    base = base.trimmed ();
    if (base.isEmpty ())
      {
        if (error) *error = QObject::tr ("Cloudlog URL is empty");
        return {};
      }

    if (!base.contains ("://"))
      {
        base.prepend ("https://");
      }

    QUrl endpoint {base};
    if (!endpoint.isValid () || endpoint.host ().isEmpty ())
      {
        if (error) *error = QObject::tr ("Cloudlog URL is invalid: %1").arg (base);
        return {};
      }

    endpoint.setScheme ("https");
    endpoint.setPath ("/index.php/api/" + endpoint_path);
    return endpoint;
  }

  QString read_limited_reply (QNetworkReply * reply, QString * error = nullptr)
  {
    if (!reply)
      {
        if (error) *error = QObject::tr ("empty reply");
        return {};
      }

    auto const length_header = reply->header (QNetworkRequest::ContentLengthHeader);
    if (length_header.isValid () && length_header.toLongLong () > kMaxCloudlogReplyBytes)
      {
        if (error) *error = QObject::tr ("reply too large");
        return {};
      }

    auto const body = reply->read (kMaxCloudlogReplyBytes + 1);
    if (body.size () > kMaxCloudlogReplyBytes || !reply->atEnd ())
      {
        if (error) *error = QObject::tr ("reply exceeds limit");
        return {};
      }

    return QString::fromUtf8 (body);
  }

  CloudlogApiKeyState parse_cloudlog_api_test_reply (QString const& result)
  {
    if (result.contains ("<status>Valid</status>", Qt::CaseInsensitive))
      {
        return result.contains ("<rights>rw</rights>", Qt::CaseInsensitive)
            ? CloudlogApiKeyState::writable
            : CloudlogApiKeyState::read_only;
      }

    auto const document = QJsonDocument::fromJson (result.toUtf8 ());
    if (document.isObject ())
      {
        auto const object = document.object ();
        auto const status = object.value ("status").toString ().trimmed ().toLower ();
        auto const reason = object.value ("reason").toString ().trimmed ().toLower ();

        if (status == "ok" || status == "success")
          {
            return CloudlogApiKeyState::writable;
          }
        if (reason.contains ("missing api key"))
          {
            return CloudlogApiKeyState::invalid;
          }
        if (reason.contains ("station profile id"))
          {
            return CloudlogApiKeyState::writable;
          }
      }

    if (result.contains ("station profile id", Qt::CaseInsensitive))
      {
        return CloudlogApiKeyState::writable;
      }

    return CloudlogApiKeyState::invalid;
  }
}

class Cloudlog::impl final
  : public QObject
{
  Q_OBJECT

public:
  impl (Cloudlog * self, Configuration const * config, QNetworkAccessManager * network_manager)
    : self_ {self}
    , config_ {config}
    , network_manager_ {network_manager}
  {
  }

  void logQso (QByteArray ADIF)
  {
    QString url_error;
    QUrl const u = ensure_https_cloudlog_url (config_->cloudlog_api_url (), "qso", &url_error);
    if (!u.isValid ())
      {
        qWarning () << "Cloudlog upload aborted:" << url_error;
        return;
      }

    QJsonObject payload;
    payload.insert ("key", config_->cloudlog_api_key ());
    payload.insert ("station_profile_id", QVariant (config_->cloudlog_api_station_id ()).toString ());
    payload.insert ("type", "adif");
    payload.insert ("string", QString::fromUtf8 (ADIF) + "<eor>");
    QByteArray const data = QJsonDocument (payload).toJson (QJsonDocument::Compact);

    QNetworkRequest request {u};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    auto * reply = network_manager_->post(request, data);
    reply_ = reply;
    connect (reply, &QNetworkReply::finished, this, [this, reply] {
        reply_logqso (reply);
      });

  }

  void testApi (QString const& url, QString const& apiKey)
  {
    QString apiUrl = url;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    if (QNetworkAccessManager::Accessible != network_manager_->networkAccessible ())
      {
        // try and recover network access for QNAM
        network_manager_->setNetworkAccessible (QNetworkAccessManager::Accessible);
      }
#endif

    QString url_error;
    QUrl endpoint = ensure_https_cloudlog_url (apiUrl, "qso", &url_error);
    if (!endpoint.isValid ())
      {
        qWarning () << "Cloudlog API test aborted:" << url_error;
        Q_EMIT self_->apikey_invalid ();
        return;
      }

    QNetworkRequest request {endpoint};
    request.setHeader (QNetworkRequest::ContentTypeHeader, QVariant ("application/json"));
    request.setRawHeader ("User-Agent", "Decodium3SE-KP5 Cloudlog API");
    request.setOriginatingObject (this);
    QJsonObject payload;
    payload.insert ("key", apiKey);
    payload.insert ("station_profile_id", QStringLiteral ("0"));
    payload.insert ("type", QStringLiteral ("adif"));
    payload.insert ("string", QStringLiteral ("<eor>"));
    auto * reply = network_manager_->post (request, QJsonDocument (payload).toJson (QJsonDocument::Compact));
    reply_ = reply;
    connect (reply, &QNetworkReply::finished, this, [this, reply] {
        reply_apitest (reply);
      });
  }

  void reply_apitest(QNetworkReply * finished_reply)
  {
    QPointer<QNetworkReply> reply {finished_reply};
    auto const finalize = [this, &reply] {
      if (reply_ == reply)
        {
          reply_.clear ();
        }
      if (reply)
        {
          reply->deleteLater ();
        }
    };

    if (!reply || !reply->isFinished ())
      {
        finalize ();
        Q_EMIT self_->apikey_invalid ();
        return;
      }

    if (reply->error () != QNetworkReply::NoError)
      {
        qWarning () << "Cloudlog API test failed:" << reply->errorString ();
        finalize ();
        Q_EMIT self_->apikey_invalid ();
        return;
      }

    QString read_error;
    auto const result = read_limited_reply (reply.data (), &read_error);
    if (!read_error.isEmpty ())
      {
        qWarning () << "Cloudlog API test rejected:" << read_error;
        finalize ();
        Q_EMIT self_->apikey_invalid ();
        return;
      }

    auto const state = parse_cloudlog_api_test_reply (result);
    finalize ();
    switch (state)
      {
      case CloudlogApiKeyState::writable:
        Q_EMIT self_->apikey_ok ();
        break;

      case CloudlogApiKeyState::read_only:
        Q_EMIT self_->apikey_ro ();
        break;

      case CloudlogApiKeyState::invalid:
      default:
        Q_EMIT self_->apikey_invalid ();
        break;
      }
  }

  void reply_logqso(QNetworkReply * finished_reply)
  {
    QPointer<QNetworkReply> reply {finished_reply};
    auto const finalize = [this, &reply] {
      if (reply_ == reply)
        {
          reply_.clear ();
        }
      if (reply)
        {
          reply->deleteLater ();
        }
    };

    if (!reply || !reply->isFinished ())
      {
        finalize ();
        return;
      }

    QString read_error;
    auto const result = read_limited_reply (reply.data (), &read_error);
    if (!read_error.isEmpty ())
      {
        qWarning () << "Cloudlog upload reply rejected:" << read_error;
        finalize ();
        return;
      }

    auto const data = QJsonDocument::fromJson(result.toUtf8());
    auto const obj = data.object();
    if (obj["status"] == "failed")
      {
        auto * msgBox = new QMessageBox {QMessageBox::Warning,
                                         tr ("Cloudlog Error!"),
                                         tr ("QSO could not be sent to Cloudlog!\nPlease check your log.")};
        msgBox->setAttribute (Qt::WA_DeleteOnClose, true);
        msgBox->setDetailedText (tr ("Reason: %1").arg (obj["reason"].toString ()));
        msgBox->open ();
      }
    finalize ();
  }

  void abort ()
  {
    if (reply_ && reply_->isRunning ())
      {
        reply_->abort ();
      }
  }

  Cloudlog * self_;
  Configuration const * config_;
  QNetworkAccessManager * network_manager_;
  QPointer<QNetworkReply> reply_;
};

#include "Cloudlog.moc"

Cloudlog::Cloudlog (Configuration const * config, QNetworkAccessManager * network_manager, QObject * parent)
  : QObject {parent}
  , m_ {this, config, network_manager}
{
}

Cloudlog::~Cloudlog ()
{
}

void Cloudlog::testApi (QString const& url, QString const& apiKey)
{
  m_->testApi (url, apiKey);
}

void Cloudlog::logQso (QByteArray ADIF)
{
  m_->logQso(ADIF);
}
