// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "SecureSettings.hpp"
#include "SecretToolLookupStatus.h"

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>

namespace secure_settings {

// Shared by the Linux backend and host-independent process tests. Callers
// provide the environment after applying AppImage/host sanitisation.
inline LookupResult lookupWithSecretTool(const QString& executable, const QString& service,
                                        const QString& account, const QProcessEnvironment& environment)
{
    LookupResult result;
    result.backend_available = !executable.isEmpty();
    if (!result.backend_available) return result;
    QProcess process;
    process.setProcessEnvironment(environment);
    process.start(executable, {QStringLiteral("lookup"), QStringLiteral("service"), service,
                               QStringLiteral("account"), account});
    if (!process.waitForStarted(3000)) {
        result.error = QObject::tr("secret-tool lookup failed to start");
        return result;
    }
    if (!process.waitForFinished(5000)) {
        result.error = QObject::tr("secret-tool lookup timeout");
        return result;
    }
    if (process.exitStatus() == QProcess::CrashExit) {
        result.error = QObject::tr("secret-tool lookup crashed");
        return result;
    }
    const QByteArray stdoutBytes = process.readAllStandardOutput();
    const QByteArray stderrBytes = process.readAllStandardError();
    QString stderrText = QString::fromUtf8(stderrBytes);
    if (stderrText.endsWith('\n')) stderrText.chop(1);
    if (stderrText.endsWith('\r')) stderrText.chop(1);
    switch (classifySecretToolLookup(process.exitCode(), !stderrBytes.isEmpty() || !stdoutBytes.isEmpty())) {
    case SecretToolLookupStatus::Found:
        result.found = true;
        result.value = QString::fromUtf8(stdoutBytes);
        if (result.value.endsWith('\n')) result.value.chop(1);
        if (result.value.endsWith('\r')) result.value.chop(1);
        break;
    case SecretToolLookupStatus::Missing:
        break;
    case SecretToolLookupStatus::Failed:
        result.error = stderrText.isEmpty() ? QObject::tr("secret-tool lookup failed") : stderrText;
        break;
    }
    return result;
}

} // namespace secure_settings
