#include "SecureSettings.hpp"

#include <QFileInfo>
#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

namespace
{
  [[maybe_unused]] QString trim_single_trailing_newline (QString text)
  {
    if (text.endsWith ('\n'))
      {
        text.chop (1);
      }
    if (text.endsWith ('\r'))
      {
        text.chop (1);
      }
    return text;
  }

  void warn_insecure_secret_fallback (QString const& setting_key)
  {
    static QSet<QString> warned_keys;
    if (warned_keys.contains (setting_key))
      {
        return;
      }

    warned_keys.insert (setting_key);
    qWarning () << "Secure settings backend unavailable for" << setting_key
                << "- falling back to QSettings storage";
  }

  void warn_secure_storage_failure (QString const& setting_key, QString const& action, QString const& error)
  {
    static QSet<QString> warned_actions;
    auto const warning_key = setting_key + QStringLiteral (":") + action;
    if (warned_actions.contains (warning_key))
      {
        return;
      }

    warned_actions.insert (warning_key);
    qWarning () << "Secure settings" << action << "failed for" << setting_key << ":" << error;
  }

  void mark_setting_as_secure (QSettings * settings, QString const& setting_key)
  {
    if (!settings)
      {
        return;
      }

    if (secure_settings::is_placeholder (settings->value (setting_key).toString ()))
      {
        return;
      }

    settings->setValue (setting_key, secure_settings::placeholder ());
    settings->sync ();
  }

  class PlatformBackend final
    : public secure_settings::Backend
  {
  public:
    bool available () const override
    {
#if defined (Q_OS_MACOS)
      return QFileInfo::exists (QStringLiteral ("/usr/bin/security"));
#elif defined (Q_OS_LINUX)
      return !QStandardPaths::findExecutable (QStringLiteral ("secret-tool")).isEmpty ();
#else
      return false;
#endif
    }

    secure_settings::LookupResult lookup (QString const& service, QString const& account) const override
    {
      secure_settings::LookupResult result;
      result.backend_available = available ();
      if (!result.backend_available)
        {
          return result;
        }

#if defined (Q_OS_MACOS)
      QProcess p;
      p.start (QStringLiteral ("/usr/bin/security"),
               QStringList {
                 QStringLiteral ("find-generic-password"),
                 QStringLiteral ("-a"), account,
                 QStringLiteral ("-s"), service,
                 QStringLiteral ("-w")
               });
      if (!p.waitForFinished (5000))
        {
          result.error = QObject::tr ("macOS Keychain read timeout");
          return result;
        }
      if (0 == p.exitCode ())
        {
          result.found = true;
          result.value = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardOutput ()));
          return result;
        }
      auto const stderr_text = QString::fromUtf8 (p.readAllStandardError ());
      if (stderr_text.contains (QStringLiteral ("could not be found"), Qt::CaseInsensitive))
        {
          result.found = false;
          return result;
        }
      result.error = trim_single_trailing_newline (stderr_text);
      return result;
#elif defined (Q_OS_LINUX)
      auto const secret_tool = QStandardPaths::findExecutable (QStringLiteral ("secret-tool"));
      if (secret_tool.isEmpty ())
        {
          result.backend_available = false;
          return result;
        }
      QProcess p;
      p.start (secret_tool,
               QStringList {
                 QStringLiteral ("lookup"),
                 QStringLiteral ("service"), service,
                 QStringLiteral ("account"), account
               });
      if (!p.waitForStarted (3000))
        {
          result.error = QObject::tr ("secret-tool lookup failed to start");
          return result;
        }
      if (!p.waitForFinished (5000))
        {
          result.error = QObject::tr ("secret-tool lookup timeout");
          return result;
        }
      if (0 == p.exitCode ())
        {
          result.found = true;
          result.value = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardOutput ()));
          return result;
        }
      auto const stderr_text = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardError ()));
      if (!stderr_text.isEmpty ())
        {
          result.error = stderr_text;
        }
      result.found = false;
      return result;
#else
      Q_UNUSED (service);
      Q_UNUSED (account);
      return result;
#endif
    }

    bool store (QString const& service, QString const& account, QString const& value, QString * error = nullptr) const override
    {
      if (!available ())
        {
          if (error) *error = QObject::tr ("secure backend unavailable");
          return false;
        }

#if defined (Q_OS_MACOS)
      QProcess p;
      p.start (QStringLiteral ("/usr/bin/security"),
               QStringList {
                 QStringLiteral ("add-generic-password"),
                 QStringLiteral ("-U"),
                 QStringLiteral ("-a"), account,
                 QStringLiteral ("-s"), service,
                 QStringLiteral ("-w"), value
               });
      if (!p.waitForFinished (5000))
        {
          if (error) *error = QObject::tr ("macOS Keychain write timeout");
          return false;
        }
      if (0 != p.exitCode ())
        {
          if (error) *error = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardError ()));
          return false;
        }
      return true;
#elif defined (Q_OS_LINUX)
      auto const secret_tool = QStandardPaths::findExecutable (QStringLiteral ("secret-tool"));
      if (secret_tool.isEmpty ())
        {
          if (error) *error = QObject::tr ("secret-tool not available");
          return false;
        }

      QProcess p;
      p.start (secret_tool,
               QStringList {
                 QStringLiteral ("store"),
                 QStringLiteral ("--label"),
                 QStringLiteral ("Decodium Credentials"),
                 QStringLiteral ("service"), service,
                 QStringLiteral ("account"), account
               });
      if (!p.waitForStarted (3000))
        {
          if (error) *error = QObject::tr ("secret-tool store failed to start");
          return false;
        }
      p.write (value.toUtf8 ());
      p.closeWriteChannel ();
      if (!p.waitForFinished (5000))
        {
          if (error) *error = QObject::tr ("secret-tool store timeout");
          return false;
        }
      if (0 != p.exitCode ())
        {
          if (error) *error = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardError ()));
          return false;
        }
      return true;
#else
      Q_UNUSED (service);
      Q_UNUSED (account);
      Q_UNUSED (value);
      if (error) *error = QObject::tr ("secure backend unsupported");
      return false;
#endif
    }

    bool remove (QString const& service, QString const& account, QString * error = nullptr) const override
    {
      if (!available ())
        {
          return true;
        }

#if defined (Q_OS_MACOS)
      QProcess p;
      p.start (QStringLiteral ("/usr/bin/security"),
               QStringList {
                 QStringLiteral ("delete-generic-password"),
                 QStringLiteral ("-a"), account,
                 QStringLiteral ("-s"), service
               });
      if (!p.waitForFinished (5000))
        {
          if (error) *error = QObject::tr ("macOS Keychain delete timeout");
          return false;
        }
      if (0 == p.exitCode ())
        {
          return true;
        }
      auto const stderr_text = QString::fromUtf8 (p.readAllStandardError ());
      if (stderr_text.contains (QStringLiteral ("could not be found"), Qt::CaseInsensitive))
        {
          return true;
        }
      if (error) *error = trim_single_trailing_newline (stderr_text);
      return false;
#elif defined (Q_OS_LINUX)
      auto const secret_tool = QStandardPaths::findExecutable (QStringLiteral ("secret-tool"));
      if (secret_tool.isEmpty ())
        {
          return true;
        }
      QProcess p;
      p.start (secret_tool,
               QStringList {
                 QStringLiteral ("clear"),
                 QStringLiteral ("service"), service,
                 QStringLiteral ("account"), account
               });
      if (!p.waitForStarted (3000))
        {
          if (error) *error = QObject::tr ("secret-tool clear failed to start");
          return false;
        }
      if (!p.waitForFinished (5000))
        {
          if (error) *error = QObject::tr ("secret-tool clear timeout");
          return false;
        }
      if (0 == p.exitCode ())
        {
          return true;
        }
      auto const stderr_text = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardError ()));
      if (stderr_text.contains (QStringLiteral ("No matching"), Qt::CaseInsensitive)
          || stderr_text.contains (QStringLiteral ("not found"), Qt::CaseInsensitive))
        {
          return true;
        }
      if (error) *error = stderr_text.isEmpty () ? QObject::tr ("secret-tool clear failed") : stderr_text;
      return false;
#else
      Q_UNUSED (service);
      Q_UNUSED (account);
      Q_UNUSED (error);
      return true;
#endif
    }
  };
}

namespace secure_settings
{
  Backend const& default_backend ()
  {
    static PlatformBackend backend;
    return backend;
  }

  QString service (QString const& callsign)
  {
    auto profile = callsign.trimmed ().toUpper ();
    if (profile.isEmpty ())
      {
        profile = QStringLiteral ("DEFAULT");
      }
    profile.replace (QRegularExpression {R"([^A-Z0-9._-])"}, QStringLiteral ("_"));
    return QStringLiteral ("org.decodium3.ft2.%1").arg (profile);
  }

  QString placeholder ()
  {
    return QStringLiteral ("__secure__");
  }

  bool is_placeholder (QString const& value)
  {
    return value == placeholder ();
  }

  QString load_or_import (QSettings * settings,
                          QString const& service,
                          QString const& setting_key,
                          QString const& plain_value,
                          Backend const& backend)
  {
    auto plain = plain_value;
    if (is_placeholder (plain))
      {
        plain.clear ();
      }

    auto const lookup = backend.lookup (service, setting_key);
    if (lookup.found)
      {
        if (!plain_value.isEmpty () && !is_placeholder (plain_value))
          {
            mark_setting_as_secure (settings, setting_key);
          }
        return lookup.value;
      }

    if (!lookup.error.isEmpty ())
      {
        warn_secure_storage_failure (setting_key, QStringLiteral ("lookup"), lookup.error);
      }

    if (!lookup.backend_available)
      {
        if (!plain.isEmpty ())
          {
            warn_insecure_secret_fallback (setting_key);
          }
        return plain;
      }

    if (!plain.isEmpty ())
      {
        QString store_error;
        if (backend.store (service, setting_key, plain, &store_error))
          {
            mark_setting_as_secure (settings, setting_key);
          }
        else if (!store_error.isEmpty ())
          {
            warn_secure_storage_failure (setting_key, QStringLiteral ("import"), store_error);
          }
      }
    return plain;
  }

  QString value_for_write (QString const& service,
                           QString const& setting_key,
                           QString const& value,
                           Backend const& backend)
  {
    if (!backend.available ())
      {
        if (!value.isEmpty ())
          {
            warn_insecure_secret_fallback (setting_key);
          }
        return value;
      }

    if (value.isEmpty ())
      {
        backend.remove (service, setting_key, nullptr);
        return QString {};
      }

    QString error;
    if (backend.store (service, setting_key, value, &error))
      {
        return placeholder ();
      }
    if (!error.isEmpty ())
      {
        warn_secure_storage_failure (setting_key, QStringLiteral ("store"), error);
      }
    return value;
  }
}
