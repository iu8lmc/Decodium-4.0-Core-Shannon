#ifndef SECURESETTINGS_HPP
#define SECURESETTINGS_HPP

#include <QString>

class QSettings;

namespace secure_settings
{
  struct LookupResult
  {
    bool backend_available {false};
    bool found {false};
    QString value;
    QString error;
  };

  class Backend
  {
  public:
    virtual ~Backend () = default;

    virtual bool available () const = 0;
    virtual LookupResult lookup (QString const& service, QString const& account) const = 0;
    virtual bool store (QString const& service, QString const& account, QString const& value, QString * error = nullptr) const = 0;
    virtual bool remove (QString const& service, QString const& account, QString * error = nullptr) const = 0;
  };

  Backend const& default_backend ();

  QString service (QString const& callsign);
  QString placeholder ();
  bool is_placeholder (QString const& value);

  QString load_or_import (QSettings * settings,
                          QString const& service,
                          QString const& setting_key,
                          QString const& plain_value,
                          Backend const& backend = default_backend ());

  QString value_for_write (QString const& service,
                           QString const& setting_key,
                           QString const& value,
                           Backend const& backend = default_backend ());
}

#endif // SECURESETTINGS_HPP
