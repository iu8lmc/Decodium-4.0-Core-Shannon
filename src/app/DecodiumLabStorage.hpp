#ifndef DECODIUM_LAB_STORAGE_HPP
#define DECODIUM_LAB_STORAGE_HPP

#include "DecodiumStorageMigration.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

namespace decodium::lab {
enum class StorageSetup { NotRequested, Ready, Failed };

// Run before normal storage migration and graphics-recovery settings probes.
// Lab startup must never migrate/wipe the operator's registry or data folders.
inline StorageSetup configureStorage(int argc, char* argv[], QString* error)
{
    bool requested = false;
    for (int i = 1; i < argc; ++i)
        requested |= QString::fromLocal8Bit(argv[i]) == QStringLiteral("--test-mode");
    if (!requested) return StorageSetup::NotRequested;

    QCoreApplication::setOrganizationName(QString::fromLatin1(DecodiumStorageMigration::organizationName()));
    QCoreApplication::setApplicationName(QStringLiteral("Decodium"));
    QStandardPaths::setTestModeEnabled(true);
    QString root = qEnvironmentVariable("DECODIUM_LAB_SETTINGS_DIR");
    if (root.isEmpty()) {
        root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                   .absoluteFilePath(QStringLiteral("lab-settings"));
    }
    if (!QDir::isAbsolutePath(root) || !QDir().mkpath(root)) {
        if (error) *error = QStringLiteral("Laboratory settings require a writable absolute directory");
        return StorageSetup::Failed;
    }
#ifdef Q_OS_WIN
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       QDir(root).absoluteFilePath(QStringLiteral("system")));
    return StorageSetup::Ready;
}
}
#endif
