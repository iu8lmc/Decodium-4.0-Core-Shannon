#ifndef REMOTE_FILE_HPP__
#define REMOTE_FILE_HPP__

#include <QObject>
#include <QString>
#include <QUrl>
#include <QFileInfo>
#include <QSaveFile>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;

//
// Synchronize an individual file specified by a URL to the local file
// system
//
class RemoteFile final
  : public QObject
{
  Q_OBJECT

public:
  //
  // Clients  of   RemoteFile  must   provide  an  instance   of  this
  // interface. It  may be  used to  receive information  and requests
  // from the RemoteFile instance as it does its work.
  //
  class ListenerInterface
  {
  protected:
    ListenerInterface () {}

  public:
    virtual void error (QString const& title, QString const& message) = 0;
    virtual bool redirect_request (QUrl const&) {return false;} // disallow
    virtual void download_progress (qint64 /* bytes_received */, qint64 /* total_bytes */) {}
    virtual void download_finished (bool /* success */) {}
  };

  explicit RemoteFile (ListenerInterface * listener, QNetworkAccessManager * network_manager
                       , QString const& local_file_path, bool http_only = false
                       , QObject * parent = nullptr);

  // true if local file exists or will do very soon
  bool local () const;

  // download/remove the local file
  bool sync (QUrl const& url, bool local = true, bool force = false);

  // abort an active download
  void abort ();

  // change the local location, this will rename if the file exists locally
  void local_file_path (QString const&);

  QString local_file_path () const {return local_file_.absoluteFilePath ();}
  QUrl url () const {return url_;}

  // always use an http scheme for remote URLs
  void http_only (bool flag = true) {http_only_ = flag;}

private:
  void download (QUrl url);
  void reply_finished ();

  Q_SLOT void store ();

  Q_SIGNAL void redirect (QUrl const&, unsigned redirect_count);
  Q_SIGNAL void downloadProgress (qint64 bytes_received, qint64 total_bytes);
  Q_SIGNAL void finished ();

  ListenerInterface * listener_;
  QNetworkAccessManager * network_manager_;
  QFileInfo local_file_;
  bool http_only_;
  QUrl url_;
  QPointer<QNetworkReply> reply_;
  bool is_valid_;
  unsigned redirect_count_;
  QSaveFile file_;
};

#endif
