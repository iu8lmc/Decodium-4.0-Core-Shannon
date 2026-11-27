#include "DecodiumBridge.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

QStringList DecodiumBridge::satelliteOptions() const
{
    QStringList options {QString()};
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList candidates {
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).absoluteFilePath(QStringLiteral("sat.dat")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absoluteFilePath(QStringLiteral("sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/Decodium/sat.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/wsjtx/sat.dat")),
        QDir::current().absoluteFilePath(QStringLiteral("sat.dat"))
    };
#ifdef CMAKE_SOURCE_DIR
    candidates << QDir(QStringLiteral(CMAKE_SOURCE_DIR))
                      .absoluteFilePath(QStringLiteral("resources/runtime/sat.dat"));
#endif

    QSet<QString> seenPaths;
    QString satFilePath;
    for (QString const& candidate : candidates) {
        QString const clean = QDir::cleanPath(candidate);
        if (clean.isEmpty() || seenPaths.contains(clean)) {
            continue;
        }
        seenPaths.insert(clean);
        QFileInfo const info {clean};
        if (info.exists() && info.isFile() && info.size() <= 4LL * 1024LL * 1024LL) {
            satFilePath = clean;
            break;
        }
    }

    if (satFilePath.isEmpty()) {
        satFilePath = QStringLiteral(":/sat.dat");
    }

    QFile file {satFilePath};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return options;
    }

    QSet<QString> seenCodes;
    QTextStream stream {&file};
    while (!stream.atEnd()) {
        QString const line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QStringList const parts = line.split(QLatin1Char('|'));
        if (parts.size() < 2) {
            continue;
        }
        QString const code = parts.at(0).trimmed();
        QString const name = parts.at(1).trimmed();
        if (code.isEmpty() || seenCodes.contains(code)) {
            continue;
        }
        seenCodes.insert(code);
        options << QStringLiteral("%1 - %2").arg(code, name.isEmpty() ? code : name);
    }
    return options;
}

QStringList DecodiumBridge::satModeOptions() const
{
    return QStringList {
        QString(),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("BS"),
        QStringLiteral("JA"),
        QStringLiteral("JD"),
        QStringLiteral("K"),
        QStringLiteral("KA"),
        QStringLiteral("KT"),
        QStringLiteral("L"),
        QStringLiteral("LS"),
        QStringLiteral("LU"),
        QStringLiteral("LX"),
        QStringLiteral("S"),
        QStringLiteral("SX"),
        QStringLiteral("T"),
        QStringLiteral("US"),
        QStringLiteral("UV"),
        QStringLiteral("VS"),
        QStringLiteral("VU")
    };
}

