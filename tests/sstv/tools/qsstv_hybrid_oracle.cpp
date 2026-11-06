// SPDX-License-Identifier: GPL-3.0-or-later
// Adapter for independently compiled QSSTV hybridCrypt; synthetic data only.
#include "hybridcrypt.h"
#include <QFile>
#include <QString>

QString hybridFtpRemoteHost, hybridFtpLogin, hybridFtpPassword, hybridFtpRemoteDirectory;

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const bool b = QByteArray(argv[1]) == "b";
    if (!b && QByteArray(argv[1]) != "a") return 2;
    hybridFtpRemoteHost = qEnvironmentVariableIntValue("DECODIUM_QSSTV_ORACLE_LOOPBACK") == 1
        ? QStringLiteral("127.0.0.1") : (b ? "b.example.invalid" : "a.example.invalid");
    hybridFtpLogin = b ? "operator-b" : "operator-a";
    hybridFtpPassword = b ? "dummy-B-1234" : "dummy-A";
    hybridFtpRemoteDirectory = b ? "radio/b" : "radio/a";
    QByteArray data;
    hybridCrypt oracle;
    if (!oracle.enCrypt(&data)) return 3;
    hybridCrypt decoder;
    if (!decoder.deCrypt(&data) || decoder.host() != hybridFtpRemoteHost
        || decoder.user() != hybridFtpLogin || decoder.passwd() != hybridFtpPassword
        || decoder.dir() != hybridFtpRemoteDirectory) return 5;
    QFile file(QString::fromLocal8Bit(argv[2]));
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) return 4;
    return 0;
}
