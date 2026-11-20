// SPDX-License-Identifier: GPL-3.0-or-later
// Manual integration test: run inside an isolated, unlocked Secret Service.
#include "src/security/CoordinatedSecretBackend.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <cstdio>

using secure_settings::CoordinatedSecretBackend;
using secure_settings::InsertResult;

static int worker(const QStringList& args)
{
    CoordinatedSecretBackend backend(secure_settings::default_backend(), args.at(2));
    const QString value = args.at(5) == QStringLiteral("a")
        ? QStringLiteral("fixture-only-key-a") : QStringLiteral("fixture-only-key-b");
    const auto result = backend.store_if_absent(args.at(3), args.at(4), value);
    std::printf("%d\n", static_cast<int>(result));
    return result == InsertResult::Created || result == InsertResult::AlreadyExists ? 0 : 1;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 6 && args.at(1) == QStringLiteral("--worker")) return worker(args);
    if (args.size() != 1) return 2;
    QTemporaryDir locks;
    if (!locks.isValid()) return 3;
    const QString service = QStringLiteral("Decodium.Lab.") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    CoordinatedSecretBackend backend(secure_settings::default_backend(), locks.path());
    if (!backend.available()) return 4;
    for (const auto& account : {QStringLiteral("race-a"), QStringLiteral("race-b")}) {
        QProcess a, b;
        a.start(app.applicationFilePath(), {QStringLiteral("--worker"), locks.path(), service, account, QStringLiteral("a")});
        b.start(app.applicationFilePath(), {QStringLiteral("--worker"), locks.path(), service, account, QStringLiteral("b")});
        if (!a.waitForStarted(10000) || !b.waitForStarted(10000)
            || !a.waitForFinished(30000) || !b.waitForFinished(30000)
            || a.exitStatus() != QProcess::NormalExit || b.exitStatus() != QProcess::NormalExit
            || a.exitCode() != 0 || b.exitCode() != 0) return 5;
        bool aOk = false, bOk = false;
        const int aResult = a.readAllStandardOutput().trimmed().toInt(&aOk);
        const int bResult = b.readAllStandardOutput().trimmed().toInt(&bOk);
        if (!aOk || !bOk || aResult + bResult != 1 || aResult == bResult) return 6;
        const auto found = backend.lookup(service, account);
        const QString expected = aResult == static_cast<int>(InsertResult::Created)
            ? QStringLiteral("fixture-only-key-a") : QStringLiteral("fixture-only-key-b");
        if (!found.backend_available || !found.error.isEmpty() || !found.found || found.value != expected) return 7;
        if (backend.store_if_absent(service, account, QStringLiteral("replacement")) != InsertResult::AlreadyExists) return 8;
        QString error;
        if (!backend.remove(service, account, &error)) return 9;
        const auto removed = backend.lookup(service, account);
        if (!removed.error.isEmpty() || removed.found) return 10;
        std::printf("PASS %s: exactly one identity stored, read back and removed\n", qPrintable(account));
    }
    return 0;
}
