#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QSet>
#include <QTemporaryDir>

#include "SecureSettings.hpp"
#include "../src/security/CoordinatedSecretBackend.h"
#include "../src/security/SecretToolLookupStatus.h"
#include "../src/security/SecretToolLookup.h"
#include "../src/security/SettingsRemove.h"
#include "../src/security/SettingsBlobLookup.h"
#include <QUuid>

namespace
{
#if defined (Q_OS_LINUX)
  class ScopedProcessEnvironment final
  {
  public:
    ~ScopedProcessEnvironment ()
    {
      for (auto const& name : recorded_)
        {
          if (originally_set_.contains (name))
            {
              qputenv (name.constData (), original_values_.value (name));
            }
          else
            {
              qunsetenv (name.constData ());
            }
        }
    }

    void set (QByteArray const& name, QByteArray const& value)
    {
      remember (name);
      qputenv (name.constData (), value);
    }

  private:
    void remember (QByteArray const& name)
    {
      if (recorded_.contains (name))
        {
          return;
        }
      recorded_.insert (name);
      if (qEnvironmentVariableIsSet (name.constData ()))
        {
          originally_set_.insert (name);
          original_values_.insert (name, qgetenv (name.constData ()));
        }
    }

    QSet<QByteArray> recorded_;
    QSet<QByteArray> originally_set_;
    QHash<QByteArray, QByteArray> original_values_;
  };
#endif

  class FakeBackend final
    : public secure_settings::Backend
  {
  public:
    bool backend_available {true};
    bool found {false};
    bool store_success {true};
    bool remove_success {true};
    QString lookup_value;
    QString lookup_error;
    QString store_error;
    QString remove_error;

    mutable int lookup_calls {0};
    mutable int store_calls {0};
    mutable int remove_calls {0};
    mutable QString last_service;
    mutable QString last_account;
    mutable QString last_value;

    bool available () const override
    {
      return backend_available;
    }

    secure_settings::LookupResult lookup (QString const& service, QString const& account) const override
    {
      ++lookup_calls;
      last_service = service;
      last_account = account;
      secure_settings::LookupResult result;
      result.backend_available = backend_available;
      result.found = found;
      result.value = lookup_value;
      result.error = lookup_error;
      return result;
    }

    bool store (QString const& service, QString const& account, QString const& value, QString * error) const override
    {
      ++store_calls;
      last_service = service;
      last_account = account;
      last_value = value;
      if (error)
        {
          *error = store_success ? QString {} : store_error;
        }
      return backend_available && store_success;
    }

    bool remove (QString const& service, QString const& account, QString * error) const override
    {
      ++remove_calls;
      last_service = service;
      last_account = account;
      if (error)
        {
          *error = remove_success ? QString {} : remove_error;
        }
      return !backend_available || remove_success;
    }
  };
}

class TestSecureSettings
  : public QObject
{
  Q_OBJECT

private:
  Q_SLOT void settings_blob_lookup_fails_closed ()
  {
    QTemporaryDir dir;
    QVERIFY (dir.isValid ());
    struct SettingsTestBackend final : secure_settings::Backend {
      QString path;
      mutable int writes {0};
      bool available () const override { return true; }
      bool protects_private_values () const override { return true; }
      secure_settings::LookupResult lookup (const QString& service, const QString& account) const override
      {
        QSettings settings (path, QSettings::IniFormat);
        const auto stored = secure_settings::lookupSettingsBlob (settings, service + QLatin1Char ('/') + account);
        if (stored.state == secure_settings::SettingsBlobState::Error)
          return {true, false, {}, QStringLiteral ("corrupt or unreadable secure item")};
        if (stored.state == secure_settings::SettingsBlobState::Missing) return {true, false, {}, {}};
        return {true, true, QString::fromLatin1 (stored.value), {}};
      }
      bool store (const QString& service, const QString& account, const QString& value, QString*) const override
      {
        ++writes;
        QSettings settings (path, QSettings::IniFormat);
        settings.setValue (service + QLatin1Char ('/') + account, value.toLatin1 ());
        settings.sync ();
        return settings.status () == QSettings::NoError;
      }
      bool remove (const QString&, const QString&, QString*) const override { return true; }
    } raw;
    raw.path = dir.filePath (QStringLiteral ("store.ini"));
    secure_settings::CoordinatedSecretBackend backend (raw, dir.filePath (QStringLiteral ("locks")));
    for (const auto& account : {QStringLiteral ("identity-a"), QStringLiteral ("identity-b")}) {
      QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), account, QStringLiteral ("initial")),
                secure_settings::InsertResult::Created);
      QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), account, QStringLiteral ("replacement")),
                secure_settings::InsertResult::AlreadyExists);
      {
        QSettings settings (raw.path, QSettings::IniFormat);
        settings.setValue (QStringLiteral ("service/") + account, QByteArray {});
        settings.sync ();
        QCOMPARE (settings.status (), QSettings::NoError);
      }
      QVERIFY (!backend.lookup (QStringLiteral ("service"), account).error.isEmpty ());
      QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), account, QStringLiteral ("replacement")),
                secure_settings::InsertResult::Error);
      QSettings reopened (raw.path, QSettings::IniFormat);
      QVERIFY (reopened.contains (QStringLiteral ("service/") + account));
      QVERIFY (reopened.value (QStringLiteral ("service/") + account).toByteArray ().isEmpty ());
    }
    QCOMPARE (raw.writes, 2);
    QFile obstacle (dir.filePath (QStringLiteral ("not-a-directory")));
    QVERIFY (obstacle.open (QIODevice::WriteOnly));
    obstacle.write ("x");
    obstacle.close ();
    QSettings invalid (obstacle.fileName () + QStringLiteral ("/store.ini"), QSettings::IniFormat);
    invalid.setValue (QStringLiteral ("pending"), QStringLiteral ("write"));
    QCOMPARE (secure_settings::lookupSettingsBlob (invalid, QStringLiteral ("service/identity-a")).state,
              secure_settings::SettingsBlobState::Error);
  }

  Q_SLOT void synced_removal_reports_failures ()
  {
    QTemporaryDir dir;
    QVERIFY (dir.isValid ());
    for (const auto& account : {QStringLiteral ("identity-a"), QStringLiteral ("identity-b")})
      {
        const QString path = dir.filePath (account + QStringLiteral (".ini"));
        QSettings settings (path, QSettings::IniFormat);
        settings.setValue (QStringLiteral ("service/account"), QStringLiteral ("protected-value"));
        settings.sync ();
        QCOMPARE (settings.status (), QSettings::NoError);
        QString error;
        QVERIFY2 (secure_settings::removeSettingAndSync (settings, QStringLiteral ("service/account"), &error),
                  qPrintable (error));
        QSettings reopened (path, QSettings::IniFormat);
        reopened.sync ();
        QVERIFY (!reopened.contains (QStringLiteral ("service/account")));
      }
    QFile obstacle {dir.filePath (QStringLiteral ("not-a-directory"))};
    QVERIFY (obstacle.open (QIODevice::WriteOnly));
    obstacle.write ("x");
    obstacle.close ();
    for (const auto& account : {QStringLiteral ("identity-a"), QStringLiteral ("identity-b")})
      {
        QSettings inaccessible (obstacle.fileName () + QLatin1Char ('/') + account + QStringLiteral (".ini"),
                                QSettings::IniFormat);
        inaccessible.setValue (QStringLiteral ("other-key"), QStringLiteral ("pending-write"));
        QString error;
        QVERIFY (!secure_settings::removeSettingAndSync (inaccessible, QStringLiteral ("service/account"), &error));
        QVERIFY (!error.isEmpty ());
      }
  }

  Q_SLOT void secret_tool_lookup_exit_classification ()
  {
    using secure_settings::SecretToolLookupStatus;
    using secure_settings::classifySecretToolLookup;
    QCOMPARE (classifySecretToolLookup (0, false), SecretToolLookupStatus::Found);
    QCOMPARE (classifySecretToolLookup (0, true), SecretToolLookupStatus::Found);
    QCOMPARE (classifySecretToolLookup (1, false), SecretToolLookupStatus::Missing);
    QCOMPARE (classifySecretToolLookup (1, true), SecretToolLookupStatus::Failed);
    QCOMPARE (classifySecretToolLookup (2, false), SecretToolLookupStatus::Failed);
    QCOMPARE (classifySecretToolLookup (127, false), SecretToolLookupStatus::Failed);
  }
#if defined (Q_OS_UNIX)
  Q_SLOT void secret_tool_process_failures_preserve_identity ()
  {
    QTemporaryDir dir;
    QVERIFY (dir.isValid ());
    const QString path = dir.filePath (QStringLiteral ("secret-tool"));
    QFile script {path};
    QVERIFY (script.open (QIODevice::WriteOnly));
    const QByteArray body = QByteArrayLiteral (
      "#!/bin/sh\n"
      "case \"$DECODIUM_FAKE_STATUS\" in\n"
      " found) printf 'original-%s' \"$5\"; exit 0 ;;\n"
      " missing) exit 1 ;;\n"
      " error) printf 'keyring failed' >&2; exit 1 ;;\n"
      " blank_error) printf '\\n' >&2; exit 1 ;;\n"
      " bad_exit) exit 2 ;;\n"
      " crashed) kill -TERM $$ ;;\n"
      " partial) printf 'unexpected'; exit 1 ;;\n"
      "esac\nexit 2\n");
    QCOMPARE (script.write (body), body.size ());
    script.close ();
    QVERIFY (QFile::setPermissions (path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner));
    struct ProcessBackend final : secure_settings::Backend
    {
      QString path;
      mutable int writes {0};
      QProcessEnvironment env {QProcessEnvironment::systemEnvironment ()};
      bool available () const override { return true; }
      bool protects_private_values () const override { return true; }
      secure_settings::LookupResult lookup (const QString& service, const QString& account) const override
      { return secure_settings::lookupWithSecretTool (path, service, account, env); }
      bool store (const QString&, const QString&, const QString&, QString*) const override
      { ++writes; return true; }
      bool remove (const QString&, const QString&, QString*) const override { return true; }
    } process;
    process.path = path;
    secure_settings::CoordinatedSecretBackend backend (process, dir.path ());
    for (const auto& account : {QStringLiteral ("identity-a"), QStringLiteral ("identity-b")})
      {
        process.env.insert (QStringLiteral ("DECODIUM_FAKE_STATUS"), QStringLiteral ("found"));
        QCOMPARE (backend.lookup (QStringLiteral ("service"), account).value,
                  QStringLiteral ("original-") + account);
        QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), account, QStringLiteral ("replacement")),
                  secure_settings::InsertResult::AlreadyExists);
        for (const auto& mode : {QStringLiteral ("error"), QStringLiteral ("blank_error"),
                                 QStringLiteral ("bad_exit"), QStringLiteral ("crashed"), QStringLiteral ("partial")})
          {
            process.env.insert (QStringLiteral ("DECODIUM_FAKE_STATUS"), mode);
            QVERIFY (!backend.lookup (QStringLiteral ("service"), account).error.isEmpty ());
            QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), account, QStringLiteral ("replacement")),
                      secure_settings::InsertResult::Error);
          }
      }
    QCOMPARE (process.writes, 0);
    process.env.insert (QStringLiteral ("DECODIUM_FAKE_STATUS"), QStringLiteral ("missing"));
    QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), QStringLiteral ("new-identity"),
                                       QStringLiteral ("new-key")), secure_settings::InsertResult::Created);
    QCOMPARE (process.writes, 1);
  }
#endif
  QTemporaryDir temp_dir_;

  QString settings_path (QString const& name) const
  {
    return temp_dir_.filePath (name + QStringLiteral (".ini"));
  }

  Q_SLOT void service_normalizes_callsign ()
  {
    QCOMPARE (secure_settings::service (QStringLiteral (" 9h1sr/portable ")),
              QStringLiteral ("org.decodium3.ft2.9H1SR_PORTABLE"));
    QCOMPARE (secure_settings::service (QString {}),
              QStringLiteral ("org.decodium3.ft2.DEFAULT"));
  }

  Q_SLOT void logging_credentials_are_scoped_by_callsign ()
  {
    FakeBackend backend;
    QString const personal_service = secure_settings::service (QStringLiteral ("9H1SR"));
    QString const special_service = secure_settings::service (QStringLiteral ("DL75WAU"));

    QVERIFY (personal_service != special_service);

    QCOMPARE (secure_settings::value_for_write (personal_service,
                                                QStringLiteral ("CloudLogApiKey"),
                                                QStringLiteral ("cloud-personal"),
                                                backend),
              secure_settings::placeholder ());
    QCOMPARE (backend.last_service, personal_service);
    QCOMPARE (backend.last_account, QStringLiteral ("CloudLogApiKey"));

    QCOMPARE (secure_settings::value_for_write (special_service,
                                                QStringLiteral ("CloudLogApiKey"),
                                                QStringLiteral ("cloud-special"),
                                                backend),
              secure_settings::placeholder ());
    QCOMPARE (backend.last_service, special_service);
    QCOMPARE (backend.last_account, QStringLiteral ("CloudLogApiKey"));

    QCOMPARE (secure_settings::value_for_write (personal_service,
                                                QStringLiteral ("qrzLogbookApiKey"),
                                                QStringLiteral ("qrz-personal"),
                                                backend),
              secure_settings::placeholder ());
    QCOMPARE (backend.last_service, personal_service);
    QCOMPARE (backend.last_account, QStringLiteral ("qrzLogbookApiKey"));

    QCOMPARE (secure_settings::value_for_write (special_service,
                                                QStringLiteral ("Lotw_pwd"),
                                                QStringLiteral ("lotw-special"),
                                                backend),
              secure_settings::placeholder ());
    QCOMPARE (backend.last_service, special_service);
    QCOMPARE (backend.last_account, QStringLiteral ("Lotw_pwd"));
  }

  Q_SLOT void load_or_import_falls_back_to_plain_when_backend_unavailable ()
  {
    FakeBackend backend;
    backend.backend_available = false;

    QSettings settings {settings_path (QStringLiteral ("fallback")), QSettings::IniFormat};
    settings.setValue (QStringLiteral ("RemoteToken"), QStringLiteral ("plain-secret"));
    settings.sync ();

    auto const value = secure_settings::load_or_import (&settings,
                                                        QStringLiteral ("service"),
                                                        QStringLiteral ("RemoteToken"),
                                                        settings.value (QStringLiteral ("RemoteToken")).toString (),
                                                        backend);
    QCOMPARE (value, QStringLiteral ("plain-secret"));
    QCOMPARE (settings.value (QStringLiteral ("RemoteToken")).toString (), QStringLiteral ("plain-secret"));
    QCOMPARE (backend.lookup_calls, 1);
    QCOMPARE (backend.store_calls, 0);
  }

  Q_SLOT void load_or_import_marks_plaintext_as_secure_when_lookup_finds_secret ()
  {
    FakeBackend backend;
    backend.found = true;
    backend.lookup_value = QStringLiteral ("keychain-secret");

    QSettings settings {settings_path (QStringLiteral ("lookup_hit")), QSettings::IniFormat};
    settings.setValue (QStringLiteral ("OTPSeed"), QStringLiteral ("legacy-secret"));
    settings.sync ();

    auto const value = secure_settings::load_or_import (&settings,
                                                        QStringLiteral ("service"),
                                                        QStringLiteral ("OTPSeed"),
                                                        settings.value (QStringLiteral ("OTPSeed")).toString (),
                                                        backend);
    QCOMPARE (value, QStringLiteral ("keychain-secret"));
    QCOMPARE (settings.value (QStringLiteral ("OTPSeed")).toString (), secure_settings::placeholder ());
    QCOMPARE (backend.store_calls, 0);
  }

  Q_SLOT void load_or_import_imports_plaintext_into_secure_backend ()
  {
    FakeBackend backend;
    backend.found = false;
    backend.store_success = true;

    QSettings settings {settings_path (QStringLiteral ("import_plain")), QSettings::IniFormat};
    settings.setValue (QStringLiteral ("Lotw_pwd"), QStringLiteral ("legacy-pwd"));
    settings.sync ();

    auto const value = secure_settings::load_or_import (&settings,
                                                        QStringLiteral ("service"),
                                                        QStringLiteral ("Lotw_pwd"),
                                                        settings.value (QStringLiteral ("Lotw_pwd")).toString (),
                                                        backend);
    QCOMPARE (value, QStringLiteral ("legacy-pwd"));
    QCOMPARE (settings.value (QStringLiteral ("Lotw_pwd")).toString (), secure_settings::placeholder ());
    QCOMPARE (backend.store_calls, 1);
    QCOMPARE (backend.last_value, QStringLiteral ("legacy-pwd"));
  }

  Q_SLOT void value_for_write_uses_plaintext_when_store_fails ()
  {
    FakeBackend backend;
    backend.store_success = false;
    backend.store_error = QStringLiteral ("simulated-store-error");

    auto const value = secure_settings::value_for_write (QStringLiteral ("service"),
                                                         QStringLiteral ("CloudLogApiKey"),
                                                         QStringLiteral ("api-key"),
                                                         backend);
    QCOMPARE (value, QStringLiteral ("api-key"));
    QCOMPARE (backend.store_calls, 1);
  }

  Q_SLOT void value_for_write_returns_placeholder_when_store_succeeds ()
  {
    FakeBackend backend;

    auto const value = secure_settings::value_for_write (QStringLiteral ("service"),
                                                         QStringLiteral ("RemoteToken"),
                                                         QStringLiteral ("token-value"),
                                                         backend);
    QCOMPARE (value, secure_settings::placeholder ());
    QCOMPARE (backend.store_calls, 1);
    QCOMPARE (backend.last_value, QStringLiteral ("token-value"));
  }

  Q_SLOT void value_for_write_empty_value_removes_secret ()
  {
    FakeBackend backend;

    auto const value = secure_settings::value_for_write (QStringLiteral ("service"),
                                                         QStringLiteral ("RemoteToken"),
                                                         QString {},
                                                         backend);
    QVERIFY (value.isEmpty ());
    QCOMPARE (backend.remove_calls, 1);
  }

#if defined (Q_OS_LINUX)
  Q_SLOT void secret_tool_failures_do_not_replace_existing_identity ()
  {
    QTemporaryDir dir;
    QVERIFY (dir.isValid ());
    const QString path = dir.filePath (QStringLiteral ("secret-tool"));
    QFile script {path};
    QVERIFY (script.open (QIODevice::WriteOnly));
    const QByteArray body = QByteArrayLiteral (
      "#!/bin/sh\n"
      "case \"$1\" in\n"
      " lookup) case \"$DECODIUM_SECRET_STATUS_TEST\" in\n"
      "   absent) exit 1 ;;\n"
      "   diagnostic) echo 'backend failure' >&2; exit 1 ;;\n"
      "   malformed) exit 2 ;;\n"
      "   present) printf 'original-key'; exit 0 ;;\n"
      " esac ;;\n"
      " store) /bin/cat >/dev/null; /usr/bin/touch \"$DECODIUM_SECRET_STATUS_MARKER\"; exit 0 ;;\n"
      "esac\nexit 2\n");
    QCOMPARE (script.write (body), body.size ());
    script.close ();
    QVERIFY (QFile::setPermissions (path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner));
    ScopedProcessEnvironment env;
    env.set ("PATH", dir.path ().toUtf8 () + ':' + qgetenv ("PATH"));
    env.set ("DECODIUM_SECRET_STATUS_MARKER", dir.filePath (QStringLiteral ("stored")).toUtf8 ());
    auto const& platform = secure_settings::default_backend ();
    secure_settings::CoordinatedSecretBackend backend (platform, dir.path ());
    for (const auto& scenario : {QByteArrayLiteral ("diagnostic"), QByteArrayLiteral ("malformed")})
      {
        env.set ("DECODIUM_SECRET_STATUS_TEST", scenario);
        const auto read = backend.lookup (QStringLiteral ("service"), QStringLiteral ("account"));
        QVERIFY (!read.error.isEmpty ());
        QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), QStringLiteral ("account"),
                                           QStringLiteral ("replacement")), secure_settings::InsertResult::Error);
        QVERIFY (!QFile::exists (dir.filePath (QStringLiteral ("stored"))));
      }
    env.set ("DECODIUM_SECRET_STATUS_TEST", QByteArrayLiteral ("present"));
    QCOMPARE (backend.lookup (QStringLiteral ("service"), QStringLiteral ("account")).value,
              QStringLiteral ("original-key"));
    QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), QStringLiteral ("account"),
                                       QStringLiteral ("replacement")), secure_settings::InsertResult::AlreadyExists);
    env.set ("DECODIUM_SECRET_STATUS_TEST", QByteArrayLiteral ("absent"));
    QCOMPARE (backend.store_if_absent (QStringLiteral ("service"), QStringLiteral ("account"),
                                       QStringLiteral ("new-key")), secure_settings::InsertResult::Created);
    QVERIFY (QFile::exists (dir.filePath (QStringLiteral ("stored"))));
  }

  Q_SLOT void appimage_secret_tool_uses_host_environment_for_all_actions ()
  {
    QTemporaryDir tool_dir;
    QVERIFY (tool_dir.isValid ());

    auto const secret_tool_path = tool_dir.filePath (QStringLiteral ("secret-tool"));
    QFile script {secret_tool_path};
    QVERIFY (script.open (QIODevice::WriteOnly | QIODevice::Truncate));
    auto const script_body = QByteArrayLiteral (
      "#!/bin/sh\n"
      "action=\"$1\"\n"
      "capture=\"${DECODIUM_SECRET_TOOL_TEST_CAPTURE}.${action}\"\n"
      "{\n"
      "  printf 'ARG=%s\\n' \"$@\"\n"
      "  printf 'LD_LIBRARY_PATH=%s\\n' \"${LD_LIBRARY_PATH-<unset>}\"\n"
      "  printf 'LD_PRELOAD=%s\\n' \"${LD_PRELOAD-<unset>}\"\n"
      "  printf 'GIO_EXTRA_MODULES=%s\\n' \"${GIO_EXTRA_MODULES-<unset>}\"\n"
      "  printf 'GI_TYPELIB_PATH=%s\\n' \"${GI_TYPELIB_PATH-<unset>}\"\n"
      "  printf 'GSETTINGS_SCHEMA_DIR=%s\\n' \"${GSETTINGS_SCHEMA_DIR-<unset>}\"\n"
      "  printf 'GTK_PATH=%s\\n' \"${GTK_PATH-<unset>}\"\n"
      "  printf 'XDG_DATA_DIRS=%s\\n' \"${XDG_DATA_DIRS-<unset>}\"\n"
      "  printf 'QT_PLUGIN_PATH=%s\\n' \"${QT_PLUGIN_PATH-<unset>}\"\n"
      "  printf 'QML_IMPORT_PATH=%s\\n' \"${QML_IMPORT_PATH-<unset>}\"\n"
      "  printf 'QT_QPA_PLATFORM=%s\\n' \"${QT_QPA_PLATFORM-<unset>}\"\n"
      "  printf 'DBUS_SESSION_BUS_ADDRESS=%s\\n' \"${DBUS_SESSION_BUS_ADDRESS-<unset>}\"\n"
      "  printf 'XDG_RUNTIME_DIR=%s\\n' \"${XDG_RUNTIME_DIR-<unset>}\"\n"
      "  printf 'PATH=%s\\n' \"${PATH-<unset>}\"\n"
      "  printf 'DISPLAY=%s\\n' \"${DISPLAY-<unset>}\"\n"
      "  printf 'XAUTHORITY=%s\\n' \"${XAUTHORITY-<unset>}\"\n"
      "  printf 'WAYLAND_DISPLAY=%s\\n' \"${WAYLAND_DISPLAY-<unset>}\"\n"
      "  printf 'XDG_CURRENT_DESKTOP=%s\\n' \"${XDG_CURRENT_DESKTOP-<unset>}\"\n"
      "  printf 'XDG_SESSION_DESKTOP=%s\\n' \"${XDG_SESSION_DESKTOP-<unset>}\"\n"
      "  printf 'XDG_SESSION_TYPE=%s\\n' \"${XDG_SESSION_TYPE-<unset>}\"\n"
      "  printf 'DESKTOP_SESSION=%s\\n' \"${DESKTOP_SESSION-<unset>}\"\n"
      "  printf 'KDE_FULL_SESSION=%s\\n' \"${KDE_FULL_SESSION-<unset>}\"\n"
      "  printf 'KDE_SESSION_VERSION=%s\\n' \"${KDE_SESSION_VERSION-<unset>}\"\n"
      "  printf 'BROWSER=%s\\n' \"${BROWSER-<unset>}\"\n"
      "  printf 'APPIMAGE=%s\\n' \"${APPIMAGE-<unset>}\"\n"
      "  printf 'APPDIR=%s\\n' \"${APPDIR-<unset>}\"\n"
      "} > \"$capture\"\n"
      "case \"$action\" in\n"
      "  lookup) printf 'fake-keyring-secret\\n' ;;\n"
      "  store) /bin/cat > \"${capture}.stdin\" ;;\n"
      "  clear) : ;;\n"
      "  *) exit 2 ;;\n"
      "esac\n");
    QCOMPARE (script.write (script_body), script_body.size ());
    script.close ();
    QVERIFY (QFile::setPermissions (
      secret_tool_path,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
        | QFileDevice::ReadGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::ExeOther));

    ScopedProcessEnvironment environment;
    auto const capture_prefix = tool_dir.filePath (QStringLiteral ("capture"));
    environment.set ("PATH", tool_dir.path ().toUtf8 () + ':' + qgetenv ("PATH"));
    environment.set ("APPIMAGE", "/tmp/Decodium.AppImage");
    environment.set ("APPDIR", "/tmp/.mount_decodium");
    environment.set ("DECODIUM_SECRET_TOOL_TEST_CAPTURE", capture_prefix.toUtf8 ());

    environment.set ("LD_LIBRARY_PATH", "/tmp/.mount_decodium/usr/lib");
    environment.set ("LD_PRELOAD", "/tmp/.mount_decodium/usr/lib/preload.so");
    environment.set ("GIO_EXTRA_MODULES", "/tmp/.mount_decodium/usr/lib/gio/modules");
    environment.set ("GI_TYPELIB_PATH", "/tmp/.mount_decodium/usr/lib/girepository-1.0");
    environment.set ("GSETTINGS_SCHEMA_DIR", "/tmp/.mount_decodium/usr/share/glib-2.0/schemas");
    environment.set ("GTK_PATH", "/tmp/.mount_decodium/usr/lib/gtk-3.0");
    environment.set ("XDG_DATA_DIRS", "/tmp/.mount_decodium/usr/share");
    environment.set ("QT_PLUGIN_PATH", "/tmp/.mount_decodium/usr/plugins");
    environment.set ("QML_IMPORT_PATH", "/tmp/.mount_decodium/usr/qml");
    environment.set ("QT_QPA_PLATFORM", "xcb");

    environment.set ("DECODIUM_HOST_LD_LIBRARY_PATH", "/host/lib");
    environment.set ("DECODIUM_HOST_LD_PRELOAD", "");
    environment.set ("DECODIUM_HOST_GIO_EXTRA_MODULES", "/host/gio/modules");
    environment.set ("DECODIUM_HOST_GI_TYPELIB_PATH", "");
    environment.set ("DECODIUM_HOST_GSETTINGS_SCHEMA_DIR", "/host/glib/schemas");
    environment.set ("DECODIUM_HOST_GTK_PATH", "");
    environment.set ("DECODIUM_HOST_XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    environment.set ("DECODIUM_HOST_QT_PLUGIN_PATH", "");
    environment.set ("DECODIUM_HOST_QML_IMPORT_PATH", "");
    environment.set ("DECODIUM_HOST_QT_QPA_PLATFORM", "wayland");

    environment.set ("DECODIUM_HOST_DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/1000/bus");
    environment.set ("DECODIUM_HOST_XDG_RUNTIME_DIR", "/run/user/1000");
    environment.set ("DECODIUM_HOST_PATH", "/usr/local/bin:/usr/bin:/bin");
    environment.set ("DECODIUM_HOST_DISPLAY", ":77");
    environment.set ("DECODIUM_HOST_XAUTHORITY", "/run/user/1000/xauth");
    environment.set ("DECODIUM_HOST_WAYLAND_DISPLAY", "wayland-test");
    environment.set ("DECODIUM_HOST_XDG_CURRENT_DESKTOP", "KDE");
    environment.set ("DECODIUM_HOST_XDG_SESSION_DESKTOP", "KDE");
    environment.set ("DECODIUM_HOST_XDG_SESSION_TYPE", "wayland");
    environment.set ("DECODIUM_HOST_DESKTOP_SESSION", "plasmawayland");
    environment.set ("DECODIUM_HOST_KDE_FULL_SESSION", "true");
    environment.set ("DECODIUM_HOST_KDE_SESSION_VERSION", "6");
    environment.set ("DECODIUM_HOST_BROWSER", "librewolf");

    environment.set ("DBUS_SESSION_BUS_ADDRESS", "bundle-bus");
    environment.set ("XDG_RUNTIME_DIR", "/tmp/.mount_decodium/runtime");
    environment.set ("DISPLAY", ":99");
    environment.set ("XAUTHORITY", "/tmp/.mount_decodium/xauth");
    environment.set ("WAYLAND_DISPLAY", "bundle-wayland");
    environment.set ("XDG_CURRENT_DESKTOP", "bundle-desktop");
    environment.set ("XDG_SESSION_DESKTOP", "bundle-desktop");
    environment.set ("XDG_SESSION_TYPE", "bundle-session");
    environment.set ("DESKTOP_SESSION", "bundle-session");
    environment.set ("KDE_FULL_SESSION", "false");
    environment.set ("KDE_SESSION_VERSION", "0");
    environment.set ("BROWSER", "/tmp/.mount_decodium/browser");

    auto const& backend = secure_settings::default_backend ();
    QVERIFY (backend.available ());
    QVERIFY (backend.protects_private_values ());

    auto const lookup = backend.lookup (QStringLiteral ("test-service"),
                                        QStringLiteral ("test-account"));
    QVERIFY2 (lookup.error.isEmpty (), qPrintable (lookup.error));
    QVERIFY (lookup.found);
    QCOMPARE (lookup.value, QStringLiteral ("fake-keyring-secret"));

    QString store_error;
    QVERIFY2 (backend.store (QStringLiteral ("test-service"),
                             QStringLiteral ("test-account"),
                             QStringLiteral ("secret-to-store"),
                             &store_error),
              qPrintable (store_error));

    QString remove_error;
    QVERIFY2 (backend.remove (QStringLiteral ("test-service"),
                              QStringLiteral ("test-account"),
                              &remove_error),
              qPrintable (remove_error));

    auto const verify_capture = [&capture_prefix](QString const& action) {
      QFile capture {capture_prefix + QLatin1Char ('.') + action};
      if (!capture.open (QIODevice::ReadOnly))
        {
          return QString {};
        }
      return QString::fromUtf8 (capture.readAll ());
    };

    for (auto const& action : {QStringLiteral ("lookup"),
                               QStringLiteral ("store"),
                               QStringLiteral ("clear")})
      {
        auto const captured = verify_capture (action);
        QVERIFY2 (!captured.isEmpty (), qPrintable (action));
        QVERIFY (!captured.contains (QStringLiteral (".mount_decodium")));
        QVERIFY (!captured.contains (QStringLiteral ("secret-to-store")));
        QVERIFY (captured.contains (QStringLiteral ("LD_LIBRARY_PATH=/host/lib\n")));
        QVERIFY (captured.contains (QStringLiteral ("LD_PRELOAD=<unset>\n")));
        QVERIFY (captured.contains (QStringLiteral ("GIO_EXTRA_MODULES=/host/gio/modules\n")));
        QVERIFY (captured.contains (QStringLiteral ("GI_TYPELIB_PATH=<unset>\n")));
        QVERIFY (captured.contains (QStringLiteral ("GSETTINGS_SCHEMA_DIR=/host/glib/schemas\n")));
        QVERIFY (captured.contains (QStringLiteral ("GTK_PATH=<unset>\n")));
        QVERIFY (captured.contains (QStringLiteral ("XDG_DATA_DIRS=/usr/local/share:/usr/share\n")));
        QVERIFY (captured.contains (QStringLiteral ("QT_PLUGIN_PATH=<unset>\n")));
        QVERIFY (captured.contains (QStringLiteral ("QML_IMPORT_PATH=<unset>\n")));
        QVERIFY (captured.contains (QStringLiteral ("QT_QPA_PLATFORM=wayland\n")));
        QVERIFY (captured.contains (QStringLiteral ("DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus\n")));
        QVERIFY (captured.contains (QStringLiteral ("XDG_RUNTIME_DIR=/run/user/1000\n")));
        QVERIFY (captured.contains (QStringLiteral ("PATH=/usr/local/bin:/usr/bin:/bin\n")));
        QVERIFY (captured.contains (QStringLiteral ("DISPLAY=:77\n")));
        QVERIFY (captured.contains (QStringLiteral ("XAUTHORITY=/run/user/1000/xauth\n")));
        QVERIFY (captured.contains (QStringLiteral ("WAYLAND_DISPLAY=wayland-test\n")));
        QVERIFY (captured.contains (QStringLiteral ("XDG_CURRENT_DESKTOP=KDE\n")));
        QVERIFY (captured.contains (QStringLiteral ("XDG_SESSION_DESKTOP=KDE\n")));
        QVERIFY (captured.contains (QStringLiteral ("XDG_SESSION_TYPE=wayland\n")));
        QVERIFY (captured.contains (QStringLiteral ("DESKTOP_SESSION=plasmawayland\n")));
        QVERIFY (captured.contains (QStringLiteral ("KDE_FULL_SESSION=true\n")));
        QVERIFY (captured.contains (QStringLiteral ("KDE_SESSION_VERSION=6\n")));
        QVERIFY (captured.contains (QStringLiteral ("BROWSER=librewolf\n")));
        QVERIFY (captured.contains (QStringLiteral ("APPIMAGE=<unset>\n")));
        QVERIFY (captured.contains (QStringLiteral ("APPDIR=<unset>\n")));
      }

    QFile stored_input {capture_prefix + QStringLiteral (".store.stdin")};
    QVERIFY (stored_input.open (QIODevice::ReadOnly));
    QCOMPARE (stored_input.readAll (), QByteArrayLiteral ("secret-to-store"));
  }
#endif
#if defined (Q_OS_WIN)
  Q_SLOT void dpapi_coordinated_create_only_and_reload ()
  {
    QTemporaryDir locks;
    QVERIFY (locks.isValid ());
    auto const& platform = secure_settings::default_backend ();
    QVERIFY (platform.protects_private_values ());
    secure_settings::CoordinatedSecretBackend backend (platform, locks.path ());
    const QString service = QStringLiteral ("Decodium.Sstv.Test.") + QUuid::createUuid ().toString (QUuid::WithoutBraces);
    const QString account = QStringLiteral ("identity");
    const QString secret = QStringLiteral ("test-only-private-value");
    struct Cleanup { const secure_settings::Backend& backend; QString service; QString account;
      ~Cleanup () { backend.remove (service, account); } } cleanup {backend, service, account};
    QCOMPARE (backend.store_if_absent (service, account, secret), secure_settings::InsertResult::Created);
    QCOMPARE (backend.store_if_absent (service, account, QStringLiteral ("replacement")), secure_settings::InsertResult::AlreadyExists);
    const auto loaded = backend.lookup (service, account);
    QVERIFY2 (loaded.error.isEmpty (), qPrintable (loaded.error));
    QVERIFY (loaded.found);
    QCOMPARE (loaded.value, secret);
    QSettings raw (QSettings::IniFormat, QSettings::UserScope, QStringLiteral ("Decodium"), QStringLiteral ("SecureStore"));
    raw.sync ();
    const auto blob = raw.value (service + QLatin1Char ('/') + account).toByteArray ();
    QVERIFY (!blob.isEmpty ());
    QVERIFY (!blob.contains (secret.toUtf8 ()));
    QVERIFY (backend.remove (service, account));
    QVERIFY (!backend.lookup (service, account).found);
    for (const auto& broken : {QStringLiteral ("empty-a"), QStringLiteral ("empty-b")}) {
      Cleanup corruptedCleanup {backend, service, broken};
      raw.setValue (service + QLatin1Char ('/') + broken, QByteArray {});
      raw.sync ();
      QCOMPARE (raw.status (), QSettings::NoError);
      const auto corrupted = backend.lookup (service, broken);
      QVERIFY (!corrupted.error.isEmpty ());
      QCOMPARE (backend.store_if_absent (service, broken, QStringLiteral ("replacement")),
                secure_settings::InsertResult::Error);
      raw.sync ();
      QVERIFY (raw.contains (service + QLatin1Char ('/') + broken));
      QVERIFY (raw.value (service + QLatin1Char ('/') + broken).toByteArray ().isEmpty ());
    }
    for (const auto& broken : {QStringLiteral ("invalid-a"), QStringLiteral ("invalid-b")}) {
      Cleanup corruptedCleanup {backend, service, broken};
      raw.setValue (service + QLatin1Char ('/') + broken, QByteArrayLiteral ("not-valid-dpapi"));
      raw.sync ();
      QCOMPARE (raw.status (), QSettings::NoError);
      QVERIFY (!backend.lookup (service, broken).error.isEmpty ());
      QCOMPARE (backend.store_if_absent (service, broken, QStringLiteral ("replacement")),
                secure_settings::InsertResult::Error);
      raw.sync ();
      QCOMPARE (raw.value (service + QLatin1Char ('/') + broken).toByteArray (), QByteArrayLiteral ("not-valid-dpapi"));
    }
  }
#endif
};

QTEST_MAIN (TestSecureSettings);

#include "test_secure_settings.moc"
