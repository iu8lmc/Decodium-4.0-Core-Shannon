// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/security/CoordinatedSecretBackend.h"

#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>

// This plaintext backend is strictly a test fixture, never used by Decodium.
class FileFixture final : public secure_settings::Backend {
public:
    explicit FileFixture(QString path) : path_(std::move(path)) {}
    bool online {true};
    bool privateValues {true}; // test-only fixture, not a production guarantee
    bool failLookup {false};
    bool failStore {false};
    bool inconsistentLookup {false};
    mutable int writes {0};
    bool available() const override { return online; }
    bool protects_private_values() const override { return privateValues; }
    secure_settings::LookupResult lookup(const QString& service, const QString& account) const override {
        if (failLookup) return {true, false, {}, QStringLiteral("read failed")};
        if (inconsistentLookup) return {true, false, QStringLiteral("partial-value"), {}};
        QSettings settings(path_, QSettings::IniFormat);
        settings.sync();
        if (settings.status() != QSettings::NoError) return {true, false, {}, QStringLiteral("read failed")};
        const QString key = service + QLatin1Char('/') + account;
        return {true, settings.contains(key), settings.value(key).toString(), {}};
    }
    bool store(const QString& service, const QString& account, const QString& value, QString*) const override {
        ++writes;
        if (failStore) return false;
        QSettings settings(path_, QSettings::IniFormat);
        settings.setValue(service + QLatin1Char('/') + account, value);
        settings.sync();
        return settings.status() == QSettings::NoError;
    }
    bool remove(const QString& service, const QString& account, QString*) const override {
        QSettings settings(path_, QSettings::IniFormat);
        settings.remove(service + QLatin1Char('/') + account);
        settings.sync();
        return settings.status() == QSettings::NoError;
    }
private:
    QString path_;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 5 && args.at(1) == QStringLiteral("child")) {
        FileFixture fixture(args.at(2));
        secure_settings::CoordinatedSecretBackend backend(fixture, args.at(3));
        const auto result = backend.store_if_absent(QStringLiteral("service"), QStringLiteral("identity"), args.at(4));
        return result == secure_settings::InsertResult::Created ? 10
             : result == secure_settings::InsertResult::AlreadyExists ? 11 : 12;
    }
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString file = directory.filePath(QStringLiteral("fixture.ini"));
    const QString locks = directory.filePath(QStringLiteral("locks"));
    FileFixture fixture(file);
    secure_settings::CoordinatedSecretBackend backend(fixture, locks);
    for (int round = 0; round < 4; ++round) {
        if (!backend.remove(QStringLiteral("service"), QStringLiteral("identity"))) return 2;
        QProcess a, b;
        const auto executable = QCoreApplication::applicationFilePath();
        a.start(executable, {QStringLiteral("child"), file, locks, QStringLiteral("A")});
        b.start(executable, {QStringLiteral("child"), file, locks, QStringLiteral("B")});
        if (!a.waitForFinished(15000) || !b.waitForFinished(15000)
            || a.exitStatus() != QProcess::NormalExit || b.exitStatus() != QProcess::NormalExit
            || !((a.exitCode() == 10 && b.exitCode() == 11)
                 || (a.exitCode() == 11 && b.exitCode() == 10))) return 3;
        const auto stored = backend.lookup(QStringLiteral("service"), QStringLiteral("identity"));
        const QString winner = a.exitCode() == 10 ? QStringLiteral("A") : QStringLiteral("B");
        if (!stored.found || stored.value != winner) return 4;
        if (backend.store_if_absent(QStringLiteral("service"), QStringLiteral("identity"), QStringLiteral("replacement"))
            != secure_settings::InsertResult::AlreadyExists) return 5;
        if (backend.lookup(QStringLiteral("service"), QStringLiteral("identity")).value != winner) return 6;
    }
    secure_settings::CoordinatedSecretBackend invalid(fixture, QStringLiteral("relative/path"));
    if (invalid.supports_private_key_storage()
        || invalid.store_if_absent(QStringLiteral("service"), QStringLiteral("identity"), QStringLiteral("A"))
            != secure_settings::InsertResult::Error) return 7;
    fixture.privateValues = false;
    if (backend.supports_private_key_storage()
        || backend.store_if_absent(QStringLiteral("service"), QStringLiteral("other"), QStringLiteral("A"))
            != secure_settings::InsertResult::Error) return 8;
    fixture.privateValues = true;
    fixture.failLookup = true;
    if (backend.store_if_absent(QStringLiteral("service"), QStringLiteral("other"), QStringLiteral("A"))
        != secure_settings::InsertResult::Error) return 9;
    fixture.failLookup = false;
    for (const auto& account : {QStringLiteral("identity-a"), QStringLiteral("identity-b")}) {
        if (backend.store_if_absent(QStringLiteral("service"), account, QStringLiteral("original"))
            != secure_settings::InsertResult::Created) return 15;
        const int writesBefore = fixture.writes;
        fixture.inconsistentLookup = true;
        const auto result = backend.store_if_absent(QStringLiteral("service"), account,
                                                     QStringLiteral("replacement"));
        fixture.inconsistentLookup = false;
        if (result != secure_settings::InsertResult::Error || fixture.writes != writesBefore
            || backend.lookup(QStringLiteral("service"), account).value != QStringLiteral("original")) return 16;
    }
    fixture.failStore = true;
    if (backend.store_if_absent(QStringLiteral("service"), QStringLiteral("other"), QStringLiteral("A"))
        != secure_settings::InsertResult::Error) return 13;
    fixture.failStore = false;
    if (backend.store_if_absent(QStringLiteral("service"), QStringLiteral("other"), QStringLiteral("A"))
            != secure_settings::InsertResult::Created
        || backend.store_if_absent(QStringLiteral("other-service"), QStringLiteral("other"), QStringLiteral("B"))
            != secure_settings::InsertResult::Created
        || backend.lookup(QStringLiteral("service"), QStringLiteral("other")).value != QStringLiteral("A")
        || backend.lookup(QStringLiteral("other-service"), QStringLiteral("other")).value != QStringLiteral("B")) return 14;
    std::cout << "Cross-process create-only A/B checks passed\n";
    return 0;
}
