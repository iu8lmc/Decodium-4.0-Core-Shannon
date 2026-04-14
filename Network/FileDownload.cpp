
#include "FileDownload.hpp"
#include <QCoreApplication>
#include <QUrl>
#include <QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QSslError>
#include <QSslSocket>
#include <QFileInfo>
#include <QDir>
#include <QIODevice>
#include "qt_helpers.hpp"
#include "Logger.hpp"

namespace
{
int constexpr max_redirects {10};
int constexpr transfer_timeout_ms {30000};
qint64 constexpr max_download_bytes {64LL * 1024 * 1024};

bool is_head_operation (QNetworkReply const * reply)
{
  return reply && reply->operation () == QNetworkAccessManager::HeadOperation;
}

int http_status_code (QNetworkReply const * reply)
{
  return reply ? reply->attribute (QNetworkRequest::HttpStatusCodeAttribute).toInt () : 0;
}

bool should_retry_head_with_get (QNetworkReply const * reply)
{
  if (!is_head_operation (reply))
    {
      return false;
    }

  auto const status = http_status_code (reply);
  if (status == 405 || status == 501)
    {
      return true;
    }

  switch (reply->error ())
    {
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::ProtocolInvalidOperationError:
      return true;

    default:
      break;
    }

  return false;
}

bool is_supported_download_url (QUrl const& url)
{
  if (!url.isValid ())
    {
      return false;
    }

  auto const scheme = url.scheme ().toLower ();
  return scheme == QStringLiteral ("http") || scheme == QStringLiteral ("https");
}

QString unsupported_download_reason (QUrl const& url)
{
  return QObject::tr ("Network Error:\nUnsupported download URL or scheme:\n%1")
    .arg (url.toDisplayString ());
}

QString oversized_download_reason (QUrl const& url)
{
  return QObject::tr ("Network Error:\nDownloaded file exceeds limit of %1 MiB:\n%2")
    .arg (max_download_bytes / (1024 * 1024))
    .arg (url.toDisplayString ());
}

bool is_unsupported_redirect_target (QNetworkReply const * reply, QUrl * resolved_url = nullptr)
{
  if (!reply)
    {
      return false;
    }

  auto const redirect_url = reply->attribute (QNetworkRequest::RedirectionTargetAttribute).toUrl ();
  if (redirect_url.isEmpty ())
    {
      return false;
    }

  auto const resolved = reply->url ().resolved (redirect_url);
  if (!is_supported_download_url (resolved))
    {
      if (resolved_url)
        {
          *resolved_url = resolved;
        }
      return true;
    }

  return false;
}
}

FileDownload::FileDownload() : QObject(nullptr)
{
  redirect_count_ = 0;
  url_valid_ = false;
  downloaded_bytes_ = 0;
  size_limit_exceeded_ = false;
}

FileDownload::~FileDownload()
{
}
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
void FileDownload::errorOccurred(QNetworkReply::NetworkError code)
{
  auto * failed_reply = qobject_cast<QNetworkReply *> (sender ());
  if (!failed_reply)
    {
      failed_reply = reply_;
    }
  if (!failed_reply)
    {
      return;
    }
  if (should_retry_head_with_get (failed_reply))
    {
      LOG_INFO(QString{"FileDownload [%1]: HEAD unsupported for %2, retrying with GET"}
               .arg(user_agent_)
               .arg(failed_reply->url().toString()));
      return;
    }
  if (size_limit_exceeded_ && failed_reply->error () == QNetworkReply::OperationCanceledError)
    {
      destfile_.cancelWriting ();
      destfile_.commit ();
      return;
    }
  QUrl rejected_redirect;
  if (is_unsupported_redirect_target (failed_reply, &rejected_redirect))
    {
      auto const reason = unsupported_download_reason (rejected_redirect);
      LOG_WARN (QString {"FileDownload [%1]: redirect rejected after network error -> %2"}
                .arg (user_agent_)
                .arg (rejected_redirect.toDisplayString ()));
      Q_EMIT error (reason);
      destfile_.cancelWriting ();
      destfile_.commit ();
      return;
    }
  LOG_INFO(QString{"FileDownload [%1]: errorOccurred %2 -> %3"}.arg(user_agent_).arg(code).arg(failed_reply->errorString()));
  Q_EMIT error (failed_reply->errorString ());
  destfile_.cancelWriting ();
  destfile_.commit ();
}
#else
void FileDownload::obsoleteError()
{
  auto * failed_reply = qobject_cast<QNetworkReply *> (sender ());
  if (!failed_reply)
    {
      failed_reply = reply_;
    }
  if (!failed_reply)
    {
      return;
    }
  if (should_retry_head_with_get (failed_reply))
    {
      LOG_INFO(QString{"FileDownload [%1]: HEAD unsupported for %2, retrying with GET"}
               .arg(user_agent_)
               .arg(failed_reply->url().toString()));
      return;
    }
  if (size_limit_exceeded_ && failed_reply->error () == QNetworkReply::OperationCanceledError)
    {
      destfile_.cancelWriting ();
      destfile_.commit ();
      return;
    }
  QUrl rejected_redirect;
  if (is_unsupported_redirect_target (failed_reply, &rejected_redirect))
    {
      auto const reason = unsupported_download_reason (rejected_redirect);
      LOG_WARN (QString {"FileDownload [%1]: redirect rejected after network error -> %2"}
                .arg (user_agent_)
                .arg (rejected_redirect.toDisplayString ()));
      Q_EMIT error (reason);
      destfile_.cancelWriting ();
      destfile_.commit ();
      return;
    }
  LOG_INFO(QString{"FileDownload [%1]: error -> %2"}.arg(user_agent_).arg(failed_reply->errorString()));
  Q_EMIT error (failed_reply->errorString ());
  destfile_.cancelWriting ();
  destfile_.commit ();
}
#endif

void FileDownload::configure(QNetworkAccessManager *network_manager, const QString &source_url, const QString &destination_path, const QString &user_agent)
{
  manager_ = network_manager;
  source_url_ = source_url;
  destination_filename_ = destination_path;
  user_agent_ = user_agent;
}

void FileDownload::store()
{
  auto * source_reply = qobject_cast<QNetworkReply *> (sender ());
  if (!source_reply)
    {
      source_reply = reply_;
    }
  if (!source_reply)
    {
      return;
    }
  if (destfile_.isOpen())
    {
      auto const data = source_reply->read (source_reply->bytesAvailable ());
      downloaded_bytes_ += data.size ();
      if (downloaded_bytes_ > max_download_bytes)
        {
          size_limit_exceeded_ = true;
          auto const reason = oversized_download_reason (source_reply->url ());
          LOG_WARN (QString {"FileDownload [%1]: %2"}.arg (user_agent_).arg (reason));
          destfile_.cancelWriting ();
          destfile_.commit ();
          Q_EMIT error (reason);
          if (source_reply->isRunning ())
            {
              source_reply->abort ();
            }
          return;
        }
      destfile_.write (data);
    }
  else
    LOG_INFO(QString{ "FileDownload [%1]: file is not open."}.arg(user_agent_));
}

void FileDownload::replyComplete()
{
  QFileInfo destination_file(destination_filename_);
  QDir tmpdir_(destination_file.absoluteFilePath());

  LOG_DEBUG(QString{ "FileDownload [%1]: replyComplete"}.arg(user_agent_));
  QPointer<QNetworkReply> finished_reply {qobject_cast<QNetworkReply *> (sender ())};
  if (!finished_reply)
    {
      finished_reply = reply_;
    }
  if (!finished_reply)
  {
    Q_EMIT load_finished ();
    return;           // we probably deleted it in an earlier call
  }
  if (reply_ == finished_reply)
    {
      reply_.clear ();
    }

  QUrl const redirect_url {finished_reply->attribute (QNetworkRequest::RedirectionTargetAttribute).toUrl ()};
  QUrl const finished_url = finished_reply->url ();
  auto const reply_error = finished_reply->error ();

  if (reply_error == QNetworkReply::NoError && !redirect_url.isEmpty ())
  {
    auto const resolved_redirect_url = finished_url.resolved (redirect_url);
    if (!is_supported_download_url (resolved_redirect_url))
    {
      auto const reason = unsupported_download_reason (resolved_redirect_url);
      LOG_WARN (QString {"FileDownload [%1]: refusing redirect to unsupported URL %2"}
                .arg (user_agent_)
                .arg (resolved_redirect_url.toDisplayString ()));
      Q_EMIT error (reason);
      destfile_.cancelWriting ();
      destfile_.commit ();
      url_valid_ = false;
      Q_EMIT load_finished ();
    }
    else if ("https" == resolved_redirect_url.scheme () && !QSslSocket::supportsSsl ())
    {
      Q_EMIT error (tr ("Network Error - SSL/TLS support not installed, cannot fetch:\n\'%1\'")
                                     .arg (resolved_redirect_url.toDisplayString ()));
      destfile_.cancelWriting ();
      destfile_.commit ();
      url_valid_ = false; // reset
      Q_EMIT load_finished ();
    }
    else if (++redirect_count_ < max_redirects)
    {
      download (resolved_redirect_url);
    }
    else
    {
      Q_EMIT error (tr ("Network Error - Too many redirects:\n\'%1\'")
                            .arg (resolved_redirect_url.toDisplayString ()));
      destfile_.cancelWriting ();
      destfile_.commit ();
      url_valid_ = false; // reset
      Q_EMIT load_finished ();
    }
  }
  else if (!url_valid_ && should_retry_head_with_get (finished_reply.data ()))
  {
    url_valid_ = true;
    download (finished_url);
  }
  else if (reply_error != QNetworkReply::NoError)
  {
    destfile_.cancelWriting();
    destfile_.commit();
    url_valid_ = false;     // reset
    // report errors that are not due to abort
    if (!size_limit_exceeded_ && QNetworkReply::OperationCanceledError != reply_error)
    {
      Q_EMIT download_error (tr ("Network Error:\n%1")
                                              .arg (finished_reply->errorString ()));
    }
    Q_EMIT load_finished ();
  }
  else
  {
      if (!url_valid_)
      {
        // now get the body content
        url_valid_ = true;
        download (finished_url);
      }
      else // the body has completed. Save it.
      {
        url_valid_ = false; // reset
        // load the database asynchronously
        // future_load_ = std::async (std::launch::async, &LotWUsers::impl::load_dictionary, this, csv_file_.fileName ());
        LOG_INFO(QString{ "FileDownload [%1]: complete. File path is %2"}.arg(user_agent_).arg(destfile_.fileName()));
        destfile_.commit();
        emit complete(destination_filename_);
      }
  }

  if (finished_reply && finished_reply->isFinished ())
  {
    finished_reply->deleteLater ();
  }

}

void FileDownload::downloadComplete(QNetworkReply *data)
{
  if (data != reply_)
  {
    return;
  }
  // make a temp file in the same place as the file we're downloading. Needs to be on the same
  // filesystem as where we eventually want to 'mv' it.

  QUrl r = request_.url();
  LOG_INFO(QString{"FileDownload [%1]: finished %2 of %3 -> %4 (%5)"}.arg(user_agent_).arg(data->operation()).arg(source_url_).arg(destination_filename_).arg(r.url()));

#ifdef DEBUG_FILEDOWNLOAD
  LOG_INFO("Request Headers:");
  Q_FOREACH (const QByteArray& hdr, request_.rawHeaderList()) {
      LOG_INFO(QString{ "%1 -> %2"}.arg(QString(hdr)).arg(QString(request_.rawHeader(hdr))));
  }

  LOG_INFO("Response Headers:");
  Q_FOREACH (const QByteArray& hdr, reply_->rawHeaderList()) {
      LOG_INFO(QString{ "%1 -> %2"}.arg(QString(hdr)).arg(QString(reply_->rawHeader(hdr))));
  }
#endif
  if (data && data->isFinished())
  {
    data->deleteLater();
  }
}

void FileDownload::start_download()
{
  url_valid_ = false;
  redirect_count_ = 0;
  downloaded_bytes_ = 0;
  size_limit_exceeded_ = false;
  download(QUrl(source_url_));
}

void FileDownload::download(QUrl qurl)
{
  if (!manager_) {
    Q_EMIT error(tr("Network Error:\nNetwork manager not configured"));
    return;
  }

  if (!is_supported_download_url (qurl))
    {
      auto const reason = unsupported_download_reason (qurl);
      LOG_WARN (QString {"FileDownload [%1]: %2"}.arg (user_agent_).arg (reason));
      Q_EMIT error (reason);
      return;
    }

  if (qurl.scheme ().compare (QStringLiteral ("https"), Qt::CaseInsensitive) == 0 && !QSslSocket::supportsSsl ())
    {
      auto const reason = tr ("Network Error - SSL/TLS support not installed, cannot fetch:\n\'%1\'")
        .arg (qurl.toDisplayString ());
      LOG_WARN (QString {"FileDownload [%1]: %2"}.arg (user_agent_).arg (reason));
      Q_EMIT error (reason);
      return;
    }

  request_.setUrl(qurl);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  if (QNetworkAccessManager::Accessible != manager_->networkAccessible ())
      {
        // try and recover network access for QNAM
        manager_->setNetworkAccessible (QNetworkAccessManager::Accessible);
      }
#endif

  LOG_INFO(QString{"FileDownload [%1]: Starting download of %2 to %3"}.arg(user_agent_).arg(source_url_).arg(destination_filename_));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  request_.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  request_.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request_.setTransferTimeout (transfer_timeout_ms);
#endif
  request_.setRawHeader("Accept", "*/*");
  request_.setRawHeader ("User-Agent", user_agent_.toLocal8Bit());  // Must have a UA for some sites, like country-files

  if (!url_valid_)
  {
    reply_ = manager_->head(request_);
  }
  else
  {
    reply_ = manager_->get (request_);
  }
  if (!reply_) {
    Q_EMIT error(tr("Network Error:\nUnable to create request"));
    return;
  }

  QObject::connect(reply_, &QNetworkReply::downloadProgress, this, &FileDownload::downloadProgress, Qt::UniqueConnection);
  QObject::connect(reply_, &QNetworkReply::finished, this, &FileDownload::replyComplete, Qt::UniqueConnection);
  QObject::connect(reply_, &QNetworkReply::redirected, this, &FileDownload::redirected, Qt::UniqueConnection);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QObject::connect(reply_, &QNetworkReply::errorOccurred,this, &FileDownload::errorOccurred, Qt::UniqueConnection);
#else
  QObject::connect(reply_, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, &FileDownload::obsoleteError, Qt::UniqueConnection);
#endif
#if QT_CONFIG(ssl)
  QObject::connect(reply_, &QNetworkReply::sslErrors, this, &FileDownload::sslErrors, Qt::UniqueConnection);
#endif
  QObject::connect(reply_, &QObject::destroyed, this, [this] () { reply_ = nullptr; }, Qt::UniqueConnection);
  QObject::connect(reply_, &QNetworkReply::readyRead, this, &FileDownload::store, Qt::UniqueConnection);

  QFileInfo destination_file(destination_filename_);
  QString const tmpfile_base = destination_file.fileName();
  QString const &tmpfile_path = destination_file.absolutePath();
  QDir tmpdir{};
  if (!tmpdir.mkpath(tmpfile_path))
  {
      auto const reason = tr("File System Error:\nCannot create directory:\n%1").arg(tmpfile_path);
      LOG_INFO(QString{"FileDownload [%1]: %2"}.arg(user_agent_).arg(reason));
      if (reply_)
        {
          reply_->abort ();
        }
      Q_EMIT error (reason);
      return;
  }
  
  if (url_valid_) {
      downloaded_bytes_ = 0;
      size_limit_exceeded_ = false;
      destfile_.setFileName(destination_file.absoluteFilePath());
      if (!destfile_.open(QSaveFile::WriteOnly | QIODevice::WriteOnly)) {
          auto const reason = tr("File System Error:\nCannot open file:\n%1\nError: %2")
                                  .arg(destfile_.fileName())
                                  .arg(destfile_.errorString());
          LOG_INFO(QString{"FileDownload [%1]: %2"}.arg(user_agent_).arg(reason));
          if (reply_)
            {
              reply_->abort ();
            }
          Q_EMIT error (reason);
          return;
      }
  }
}

void FileDownload::redirected(QUrl const& url)
{
  auto const resolved = reply_ ? reply_->url ().resolved (url) : url;
  if (!is_supported_download_url (resolved))
    {
      auto const reason = unsupported_download_reason (resolved);
      LOG_WARN (QString {"FileDownload [%1]: redirect blocked to unsupported URL %2"}
                .arg (user_agent_)
                .arg (resolved.toDisplayString ()));
      Q_EMIT error (reason);
      if (reply_ && reply_->isRunning ())
        {
          reply_->abort ();
        }
      return;
    }

  if (resolved.scheme ().compare (QStringLiteral ("https"), Qt::CaseInsensitive) == 0 && !QSslSocket::supportsSsl ())
    {
      auto const reason = tr ("Network Error - SSL/TLS support not installed, cannot fetch:\n\'%1\'")
        .arg (resolved.toDisplayString ());
      LOG_WARN (QString {"FileDownload [%1]: %2"}.arg (user_agent_).arg (reason));
      Q_EMIT error (reason);
      if (reply_ && reply_->isRunning ())
        {
          reply_->abort ();
        }
    }
}

#if QT_CONFIG(ssl)
void FileDownload::sslErrors(QList<QSslError> const& errors)
{
  if (!reply_)
    {
      return;
    }

  QString details;
  for (auto const& error : errors)
    {
      if (!details.isEmpty ())
        {
          details += QStringLiteral ("; ");
        }
      details += error.errorString ();
    }

  LOG_WARN (QString {"FileDownload [%1]: refusing insecure TLS certificate for %2: %3"}
            .arg (user_agent_)
            .arg (reply_->url ().toDisplayString ())
            .arg (details));

  if (reply_->isRunning ())
    {
      reply_->abort ();
    }
}
#endif

void FileDownload::downloadProgress(qint64 received, qint64 total)
{
  if (total > max_download_bytes && reply_ && reply_->operation () == QNetworkAccessManager::GetOperation)
    {
      if (!size_limit_exceeded_)
        {
          size_limit_exceeded_ = true;
          auto const reason = oversized_download_reason (reply_->url ());
          LOG_WARN (QString {"FileDownload [%1]: %2"}.arg (user_agent_).arg (reason));
          if (destfile_.isOpen ())
            {
              destfile_.cancelWriting ();
              destfile_.commit ();
            }
          Q_EMIT error (reason);
        }

      if (reply_->isRunning ())
        {
          reply_->abort ();
        }
      return;
    }

  LOG_DEBUG(QString{"FileDownload: [%1] Progress %2 from %3, total %4, so far %5"}.arg(user_agent_).arg(destination_filename_).arg(source_url_).arg(total).arg(received));
  Q_EMIT progress(QString{"%1 bytes downloaded"}.arg(received));
}

void FileDownload::abort ()
{
  if (reply_ && reply_->isRunning ())
  {
    reply_->abort ();
  }
}
