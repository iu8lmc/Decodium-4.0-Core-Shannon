#ifndef NETWORK_ACCESS_MANAGER_HPP__
#define NETWORK_ACCESS_MANAGER_HPP__

#include <QNetworkAccessManager>
#include <QList>
#include <QSet>
#include <QSslError>

#include "widgets/MessageBox.hpp"

class QNetworkRequest;
class QIODevice;
class QWidget;

  // sub-class QNAM to surface SSL errors and enforce strict TLS validation
class NetworkAccessManager
  : public QNetworkAccessManager
{
  Q_OBJECT

public:
  explicit NetworkAccessManager (QWidget * parent);

protected:
  QNetworkReply * createRequest (Operation, QNetworkRequest const&, QIODevice * = nullptr) override;

private:
  void filter_SSL_errors (QNetworkReply * reply, QList<QSslError> const& errors);

  QWidget * parent_widget_;
  QSet<QString> reported_ssl_error_keys_;
};

#endif
