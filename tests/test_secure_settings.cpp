#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include "SecureSettings.hpp"

namespace
{
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
};

QTEST_MAIN (TestSecureSettings);

#include "test_secure_settings.moc"
