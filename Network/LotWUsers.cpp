#include "LotWUsers.hpp"

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
#include <QSslSocket>
#include <QMetaObject>
#include "qt_helpers.hpp"
#include "Logger.hpp"
#include "FileDownload.hpp"
#include "pimpl_impl.hpp"

#include "moc_LotWUsers.cpp"

namespace
{
  // Dictionary mapping call sign to date of last upload to LotW
  using dictionary = QHash<QString, QDate>;
}

class LotWUsers::impl final
  : public QObject
{
  Q_OBJECT

public:
  impl (LotWUsers * self, QNetworkAccessManager * network_manager)
    : self_ {self}
    , network_manager_ {network_manager}
    , url_valid_ {false}
    , redirect_count_ {0}
    , age_constraint_ {365}
    , connected_ {false}
  {
  }

	  void load (QString const& url, bool fetch, bool forced_fetch)
  {
    abort ();                   // abort any active download
    auto csv_file_name = csv_file_.fileName ();
    auto exists = QFileInfo::exists (csv_file_name);
    if (fetch && (!exists || forced_fetch))
    {
      current_url_.setUrl(url.trimmed());
      if (current_url_.scheme().isEmpty())
      {
        current_url_.setScheme("https");
      }
      if (current_url_.isValid() && current_url_.scheme().compare("https", Qt::CaseInsensitive) != 0)
      {
        current_url_.setScheme("https");
      }
      if (current_url_.isValid() && !QSslSocket::supportsSsl())
      {
        Q_EMIT self_->LotW_users_error (QObject::tr ("SSL/TLS support is required for LotW downloads"));
        return;
      }
      redirect_count_ = 0;

      Q_EMIT self_->progress (QString("Starting download from %1").arg(current_url_.toString()));

      lotw_downloader_.configure(network_manager_,
                                 current_url_.toString(),
                                 csv_file_name,
                                 "Decodium v3.0 SE KP5 LotW User Downloader");
      if (!connected_)
      {
        connect(&lotw_downloader_, &FileDownload::complete, [this, csv_file_name] {
            LOG_INFO(QString{"LotWUsers: Loading LotW file %1"}.arg(csv_file_name));
            future_load_ = std::async(std::launch::async, &LotWUsers::impl::load_dictionary, this, csv_file_name);
        });
        connect(&lotw_downloader_, &FileDownload::error, [this] (QString const& msg) {
            LOG_INFO(QString{"LotWUsers: Error downloading LotW file: %1"}.arg(msg));
            Q_EMIT self_->LotW_users_error (msg);
        });
        connect( &lotw_downloader_, &FileDownload::progress, [this] (QString const& msg) {
            Q_EMIT self_->progress (msg);
        });
        connected_ = true;
      }
        lotw_downloader_.start_download();
      }
    else
      {
        if (exists)
          {
            // load the database asynchronously
            future_load_ = std::async (std::launch::async, &LotWUsers::impl::load_dictionary, this, csv_file_name);
          }
      }
  }

  void abort ()
  {
    lotw_downloader_.abort();
  }

  // Load the database from the given file name
  //
  // Expects the file to be in CSV format with no header with one
  // record per line. Record fields are call sign followed by upload
  // date in yyyy-MM-dd format followed by upload time (ignored)
  dictionary load_dictionary (QString const& lotw_csv_file)
  {
    dictionary result;
    QFile f {lotw_csv_file};
    if (f.open (QFile::ReadOnly | QFile::Text))
      {
        QTextStream s {&f};
        for (auto l = s.readLine (); !l.isNull (); l = s.readLine ())
          {
            auto pos = l.indexOf (',');
            result[l.left (pos)] = QDate::fromString (l.mid (pos + 1, l.indexOf (',', pos + 1) - pos - 1), "yyyy-MM-dd");
          }
      }
    else
      {
        throw std::runtime_error {QObject::tr ("Failed to open LotW users CSV file: '%1'").arg (f.fileName ()).toStdString ()};
      }
    LOG_INFO(QString{"LotWUsers: Loaded %1 records from %2"}.arg(result.size()).arg(lotw_csv_file));
    auto const loaded_count = result.size ();
    QMetaObject::invokeMethod (self_, [this, loaded_count] () {
      Q_EMIT self_->progress (QString {"Loaded %1 records from LotW."}.arg (loaded_count));
      Q_EMIT self_->load_finished ();
    }, Qt::QueuedConnection);
    return result;
  }

  LotWUsers * self_;
  QNetworkAccessManager * network_manager_;
  QSaveFile csv_file_;
  bool url_valid_;
  QUrl current_url_;            // may be a redirect
  int redirect_count_;
  QPointer<QNetworkReply> reply_;
  std::future<dictionary> future_load_;
  dictionary last_uploaded_;
  qint64 age_constraint_;       // days
  FileDownload lotw_downloader_;
  bool connected_;
};

#include "LotWUsers.moc"

LotWUsers::LotWUsers (QNetworkAccessManager * network_manager, QObject * parent)
  : QObject {parent}
  , m_ {this, network_manager}
{

}

LotWUsers::~LotWUsers ()
{
}

void LotWUsers::set_local_file_path (QString const& path)
{
  m_->csv_file_.setFileName (path);
}

void LotWUsers::load (QString const& url, bool fetch, bool force_download)
{
  m_->load (url, fetch, force_download);
}

void LotWUsers::set_age_constraint (qint64 uploaded_since_days)
{
  m_->age_constraint_ = uploaded_since_days;
}

bool LotWUsers::user (QString const& call) const
{
  // check if a pending asynchronous load is ready
  if (m_->future_load_.valid ()
      && std::future_status::ready == m_->future_load_.wait_for (std::chrono::seconds {0}))
    {
      try
        {
          // wait for the load to finish if necessary
          const_cast<dictionary&> (m_->last_uploaded_) = const_cast<std::future<dictionary>&> (m_->future_load_).get ();
        }
      catch (std::exception const& e)
        {
          Q_EMIT LotW_users_error (e.what ());
        }
      Q_EMIT load_finished ();
    }
  if (m_->last_uploaded_.size ())
    {
      auto p = m_->last_uploaded_.constFind (call);
      if (p != m_->last_uploaded_.end ())
        {
          return p.value ().daysTo (QDate::currentDate ()) <= m_->age_constraint_;
        }
    }
  return false;
}
