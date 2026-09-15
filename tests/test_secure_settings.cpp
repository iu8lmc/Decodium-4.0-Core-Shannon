#include <QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QSet>
#include <QTemporaryDir>

#include "SecureSettings.hpp"

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
};

QTEST_MAIN (TestSecureSettings);

#include "test_secure_settings.moc"
