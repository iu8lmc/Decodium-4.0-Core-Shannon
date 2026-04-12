#include "Network/NetworkAccessManager.hpp"

#include <QString>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace
{
QString ssl_error_key (QNetworkReply const * reply, QSslError const& error)
{
  auto const url = reply ? reply->request ().url () : QUrl {};
  auto const scheme = url.scheme ().toLower ();
  auto const port = url.port ("https" == scheme ? 443 : -1);
  auto const digest = QCryptographicHash::hash (error.certificate ().toDer (), QCryptographicHash::Sha256).toHex ();

  return QString {"%1|%2|%3|%4"}
    .arg (url.host ().toLower ())
    .arg (port)
    .arg (static_cast<int> (error.error ()))
    .arg (QString::fromLatin1 (digest));
}
}

#include "moc_NetworkAccessManager.cpp"

NetworkAccessManager::NetworkAccessManager (QWidget * parent)
  : QNetworkAccessManager (parent)
  , parent_widget_ {parent}
{
  // Security hardening: never bypass SSL/TLS validation.
  connect (this, &QNetworkAccessManager::sslErrors, this, &NetworkAccessManager::filter_SSL_errors);
}

void NetworkAccessManager::filter_SSL_errors (QNetworkReply * reply, QList<QSslError> const& errors)
{
  if (!reply || errors.isEmpty ())
    {
      return;
    }

  QString message;
  QString certs;
  bool has_unreported = false;
  for (auto const& error: errors)
    {
      auto const key = ssl_error_key (reply, error);
      if (!reported_ssl_error_keys_.contains (key))
        {
          reported_ssl_error_keys_.insert (key);
          has_unreported = true;
          message += '\n' + reply->request ().url ().toDisplayString () + ": " + error.errorString ();
        }
    }
  if (has_unreported)
    {
      for (auto const& cert : reply->sslConfiguration ().peerCertificateChain ())
        {
          certs += cert.toText () + '\n';
        }
      MessageBox::warning_message (parent_widget_, tr ("Network SSL/TLS Errors"), message, certs);
    }
  if (reply->isRunning ())
    {
      reply->abort ();
    }
}

QNetworkReply * NetworkAccessManager::createRequest (Operation operation, QNetworkRequest const& request
                                                     , QIODevice * outgoing_data)
{
  return QNetworkAccessManager::createRequest (operation, request, outgoing_data);
}
