#include "src/radio/CatNativeMigration.h"
#include "src/radio/DecodiumProfileSettings.h"
#include <QDebug>
#include <QSettings>

static void migrateRemovedNativeCatBackend()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(s);
    int const copied = decodium::radio::migrateRemovedNativeCatBackend(s);
    if (copied >= 0) {
        s.sync();
        qInfo().noquote() << "[CATDBG] Native CAT backend removed: copied" << copied
                          << "settings to Hamlib, backend now hamlib";
    }
}

void decodiumBridgeMigrateRemovedNativeCatBackend() { migrateRemovedNativeCatBackend(); }

