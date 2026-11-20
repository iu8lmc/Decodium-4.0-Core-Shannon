#include "SecureSettings.hpp"
#include "SecretToolLookup.h"
#include "SettingsRemove.h"
#include "SettingsBlobLookup.h"
#include "src/core/HostProcessEnvironment.h"

#include <QFileInfo>
#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <cstring>

#if defined (Q_OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#endif
#if defined (Q_OS_MACOS)
#include <Security/Security.h>
#endif

namespace
{
#if defined (Q_OS_MACOS)
  CFMutableDictionaryRef macKeychainQuery (QString const& service, QString const& account)
  {
    const QByteArray s = service.toUtf8 (), a = account.toUtf8 ();
    CFStringRef serviceString = CFStringCreateWithBytes (nullptr, reinterpret_cast<const UInt8 *> (s.constData ()),
                                                          s.size (), kCFStringEncodingUTF8, false);
    CFStringRef accountString = CFStringCreateWithBytes (nullptr, reinterpret_cast<const UInt8 *> (a.constData ()),
                                                          a.size (), kCFStringEncodingUTF8, false);
    CFMutableDictionaryRef query = CFDictionaryCreateMutable (nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
    if (!serviceString || !accountString || !query)
      {
        if (serviceString) CFRelease (serviceString);
        if (accountString) CFRelease (accountString);
        if (query) CFRelease (query);
        return nullptr;
      }
    CFDictionarySetValue (query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue (query, kSecAttrService, serviceString);
    CFDictionarySetValue (query, kSecAttrAccount, accountString);
    CFRelease (serviceString);
    CFRelease (accountString);
    return query;
  }
#endif
#if defined (Q_OS_LINUX)
  void prepare_secret_tool_process (QProcess& process)
  {
    process.setProcessEnvironment (
      decodium::host_process::sanitizedSystemEnvironment ());
  }
#endif

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
    bool protects_private_values () const override
    {
#if defined (Q_OS_WIN) || defined (Q_OS_LINUX) || defined (Q_OS_MACOS)
      return true;
#else
      return false;
#endif
    }

    bool available () const override
    {
#if defined (Q_OS_MACOS)
      return true;
#elif defined (Q_OS_LINUX)
      return !QStandardPaths::findExecutable (QStringLiteral ("secret-tool")).isEmpty ();
#elif defined (Q_OS_WIN)
      return true;  // Windows DPAPI always available
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
      CFMutableDictionaryRef query = macKeychainQuery (service, account);
      if (!query) { result.error = QObject::tr ("macOS Keychain query allocation failed"); return result; }
      CFDictionarySetValue (query, kSecReturnData, kCFBooleanTrue);
      CFDictionarySetValue (query, kSecMatchLimit, kSecMatchLimitOne);
      CFTypeRef value = nullptr;
      const OSStatus status = SecItemCopyMatching (query, &value);
      CFRelease (query);
      if (status == errSecSuccess && value && CFGetTypeID (value) == CFDataGetTypeID ())
        {
          auto const data = static_cast<CFDataRef> (value);
          result.value = QString::fromUtf8 (reinterpret_cast<const char *> (CFDataGetBytePtr (data)),
                                             static_cast<qsizetype> (CFDataGetLength (data)));
          result.found = true;
        }
      else if (status != errSecItemNotFound)
        result.error = QObject::tr ("macOS Keychain read failed (%1)").arg (status);
      if (value) CFRelease (value);
      return result;
#elif defined (Q_OS_LINUX)
      auto const secret_tool = QStandardPaths::findExecutable (QStringLiteral ("secret-tool"));
      return secure_settings::lookupWithSecretTool (secret_tool, service, account,
        decodium::host_process::sanitizedSystemEnvironment ());
#elif defined (Q_OS_WIN)
      // Windows DPAPI: the encrypted blob is stored in a QSettings key
      // named "dpapi:<service>/<account>" as a base64 string.
      QSettings reg (QSettings::IniFormat, QSettings::UserScope, QStringLiteral ("Decodium"), QStringLiteral ("SecureStore"));
      auto const key = service + QStringLiteral ("/") + account;
      auto const stored = secure_settings::lookupSettingsBlob (reg, key);
      if (stored.state == secure_settings::SettingsBlobState::Error)
        {
          result.error = QObject::tr ("DPAPI secure store read failed");
          return result;
        }
      if (stored.state == secure_settings::SettingsBlobState::Missing)
        {
          result.found = false;
          return result;
        }
      auto const blob = QByteArray::fromBase64 (stored.value);
      DATA_BLOB in_blob;
      in_blob.cbData = static_cast<DWORD> (blob.size ());
      in_blob.pbData = reinterpret_cast<BYTE *> (const_cast<char *> (blob.data ()));
      DATA_BLOB out_blob;
      if (CryptUnprotectData (&in_blob, nullptr, nullptr, nullptr, nullptr, 0, &out_blob))
        {
          result.found = true;
          result.value = QString::fromUtf8 (reinterpret_cast<char *> (out_blob.pbData), static_cast<int> (out_blob.cbData));
          LocalFree (out_blob.pbData);
          return result;
        }
      result.error = QObject::tr ("DPAPI CryptUnprotectData failed");
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
      const QByteArray serviceBytes = service.toUtf8 ();
      const QByteArray accountBytes = account.toUtf8 ();
      QByteArray secretBytes = value.toUtf8 ();
      CFStringRef serviceString = CFStringCreateWithBytes (nullptr,
        reinterpret_cast<const UInt8 *> (serviceBytes.constData ()), serviceBytes.size (), kCFStringEncodingUTF8, false);
      CFStringRef accountString = CFStringCreateWithBytes (nullptr,
        reinterpret_cast<const UInt8 *> (accountBytes.constData ()), accountBytes.size (), kCFStringEncodingUTF8, false);
      CFMutableDataRef secretData = CFDataCreateMutable (nullptr, secretBytes.size ());
      CFMutableDictionaryRef query = CFDictionaryCreateMutable (nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      CFMutableDictionaryRef changes = CFDictionaryCreateMutable (nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      OSStatus status = errSecAllocate;
      if (serviceString && accountString && secretData && query && changes)
        {
          CFDataAppendBytes (secretData, reinterpret_cast<const UInt8 *> (secretBytes.constData ()), secretBytes.size ());
          CFDictionarySetValue (query, kSecClass, kSecClassGenericPassword);
          CFDictionarySetValue (query, kSecAttrService, serviceString);
          CFDictionarySetValue (query, kSecAttrAccount, accountString);
          CFDictionarySetValue (changes, kSecValueData, secretData);
          status = SecItemUpdate (query, changes);
          if (status == errSecItemNotFound)
            {
              CFDictionarySetValue (query, kSecValueData, secretData);
              status = SecItemAdd (query, nullptr);
            }
        }
      if (secretData && CFDataGetLength (secretData) > 0)
        std::memset (CFDataGetMutableBytePtr (secretData), 0, static_cast<size_t> (CFDataGetLength (secretData)));
      if (changes) CFRelease (changes);
      if (query) CFRelease (query);
      if (secretData) CFRelease (secretData);
      if (accountString) CFRelease (accountString);
      if (serviceString) CFRelease (serviceString);
      secretBytes.fill ('\0');
      if (status != errSecSuccess && error)
        *error = QObject::tr ("macOS Keychain write failed (%1)").arg (status);
      return status == errSecSuccess;
#elif defined (Q_OS_LINUX)
      auto const secret_tool = QStandardPaths::findExecutable (QStringLiteral ("secret-tool"));
      if (secret_tool.isEmpty ())
        {
          if (error) *error = QObject::tr ("secret-tool not available");
          return false;
        }

      QProcess p;
      prepare_secret_tool_process (p);
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
      auto value_bytes = value.toUtf8 ();
      const auto written = p.write (value_bytes);
      const auto expected = value_bytes.size ();
      value_bytes.fill ('\0');
      if (written != expected)
        {
          if (error) *error = QObject::tr ("secret-tool store input failed");
          p.kill ();
          p.waitForFinished (1000);
          return false;
        }
      p.closeWriteChannel ();
      if (!p.waitForFinished (5000))
        {
          if (error) *error = QObject::tr ("secret-tool store timeout");
          return false;
        }
      if (p.exitStatus () == QProcess::CrashExit)
        {
          if (error) *error = QObject::tr ("secret-tool store crashed");
          return false;
        }
      if (0 != p.exitCode ())
        {
          if (error) *error = trim_single_trailing_newline (QString::fromUtf8 (p.readAllStandardError ()));
          return false;
        }
      return true;
#elif defined (Q_OS_WIN)
      auto const utf8 = value.toUtf8 ();
      DATA_BLOB in_blob;
      in_blob.cbData = static_cast<DWORD> (utf8.size ());
      in_blob.pbData = reinterpret_cast<BYTE *> (const_cast<char *> (utf8.data ()));
      DATA_BLOB out_blob;
      if (!CryptProtectData (&in_blob, nullptr, nullptr, nullptr, nullptr, 0, &out_blob))
        {
          if (error) *error = QObject::tr ("DPAPI CryptProtectData failed");
          return false;
        }
      auto const blob64 = QByteArray (reinterpret_cast<char *> (out_blob.pbData), static_cast<int> (out_blob.cbData)).toBase64 ();
      LocalFree (out_blob.pbData);
      QSettings reg (QSettings::IniFormat, QSettings::UserScope, QStringLiteral ("Decodium"), QStringLiteral ("SecureStore"));
      auto const key = service + QStringLiteral ("/") + account;
      reg.setValue (key, blob64);
      reg.sync ();
      if (reg.status () != QSettings::NoError)
        {
          if (error) *error = QObject::tr ("DPAPI secure store write failed");
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
      CFMutableDictionaryRef query = macKeychainQuery (service, account);
      if (!query) { if (error) *error = QObject::tr ("macOS Keychain query allocation failed"); return false; }
      const OSStatus status = SecItemDelete (query);
      CFRelease (query);
      if (status == errSecSuccess || status == errSecItemNotFound) return true;
      if (error) *error = QObject::tr ("macOS Keychain delete failed (%1)").arg (status);
      return false;
#elif defined (Q_OS_LINUX)
      auto const secret_tool = QStandardPaths::findExecutable (QStringLiteral ("secret-tool"));
      if (secret_tool.isEmpty ())
        {
          return true;
        }
      QProcess p;
      prepare_secret_tool_process (p);
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
#elif defined (Q_OS_WIN)
      QSettings reg (QSettings::IniFormat, QSettings::UserScope, QStringLiteral ("Decodium"), QStringLiteral ("SecureStore"));
      auto const key = service + QStringLiteral ("/") + account;
      return secure_settings::removeSettingAndSync (reg, key, error);
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
