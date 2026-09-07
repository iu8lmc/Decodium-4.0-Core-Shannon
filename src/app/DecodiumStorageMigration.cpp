#include "DecodiumStorageMigration.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace {

constexpr auto kLegacyOrg  = "IU8LMC";

#ifdef Q_OS_WIN

// Nuova identita' unica. Vecchia: org "IU8LMC" (dati sotto %APPDATA%\IU8LMC\...)
// e org "Decodium"/app "Decodium3" (registro).
constexpr auto kNewOrg     = "Decodium";
constexpr auto kAppDefault = "Decodium";   // scope di QSettings() default
constexpr auto kAppBridge  = "Decodium3";  // scope di QSettings("Decodium","Decodium3")

#endif

#ifdef Q_OS_WIN

// Marker: sta nell'INI di destinazione dello scope "bridge" (il piu' popolato).
constexpr auto kMarkerKey  = "Storage/MigratedFromRegistry";
constexpr int  kMarkerVersion = 1;

// Copia ogni chiave (allKeys() e' ricorsivo: include MultiSettings/<profilo>/...).
// Non sovrascrive chiavi gia' presenti nel target: se l'utente ha gia' scritto
// nel nuovo INI, quel valore e' piu' recente del residuo di registro.
int copyAll(QSettings& from, QSettings& to)
{
    int copied = 0;
    const QStringList keys = from.allKeys();
    for (const QString& key : keys) {
        if (to.contains(key))
            continue;
        to.setValue(key, from.value(key));
        ++copied;
    }
    return copied;
}

// Sposta srcDir dentro dstDir. Se dstDir non esiste e' un semplice rename
// (istantaneo, stesso volume). Se esiste, sposta i figli uno per uno.
bool moveTree(const QString& srcPath, const QString& dstPath)
{
    QFileInfo const src(srcPath);
    if (!src.exists() || !src.isDir())
        return false;
    if (QFileInfo::exists(dstPath)) {
        QDir const srcDir(srcPath);
        bool allOk = true;
        const QStringList entries =
            srcDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString& entry : entries) {
            QString const from = srcDir.absoluteFilePath(entry);
            QString const to   = QDir(dstPath).absoluteFilePath(entry);
            if (QFileInfo::exists(to))
                continue;  // gia' presente nel nuovo layout: non sovrascrivere
            if (!QDir().rename(from, to))
                allOk = false;
        }
        if (allOk)
            QDir(srcPath).removeRecursively();  // vuota solo se tutto e' migrato
        return allOk;
    }
    QDir().mkpath(QFileInfo(dstPath).absolutePath());
    return QDir().rename(srcPath, dstPath);
}

// Cancella l'intero sottoalbero di registro legacy (org compresa).
void wipeRegistryTree(const QString& regPath)
{
    QSettings tree(regPath, QSettings::NativeFormat);
    tree.clear();
    tree.sync();
}

#endif  // Q_OS_WIN

}  // namespace

namespace DecodiumStorageMigration {

const char* organizationName()
{
#ifdef Q_OS_WIN
    return kNewOrg;
#else
    return kLegacyOrg;
#endif
}

void configure()
{
    // Identita' PRIMA di qualunque QStandardPaths (i setter sono statici: non
    // serve che QApplication esista). Senza questo il log, che parte a inizio
    // main(), veniva risolto senza org/app e finiva nella RADICE di
    // %LOCALAPPDATA%, mentre le chiamate successive lo risolvevano sotto la
    // cartella dell'org: stesso log scritto in due posti diversi.
    QCoreApplication::setOrganizationName(QString::fromLatin1(organizationName()));
    QCoreApplication::setApplicationName(QStringLiteral("Decodium"));

#ifdef Q_OS_WIN
    // Una riga sola sposta fuori dal registro TUTTI i QSettings(org, app) del
    // progetto: il formato di default vale per ogni costruttore che non passa
    // un formato esplicito. I QSettings::NativeFormat espliciti restano tali —
    // ed e' corretto: leggono HKLM\HARDWARE\DEVICEMAP\SERIALCOMM (porte COM di
    // Windows), non impostazioni nostre.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    runOnce();
#endif
}

void runOnce()
{
#ifndef Q_OS_WIN
    return;  // nessun registro da bonificare fuori da Windows
#else
    // Il target usa IniFormat ESPLICITO: non dipende da setDefaultFormat ne'
    // dall'esistenza di QApplication (org/app sono espliciti).
    {
        QSettings probe(QSettings::IniFormat, QSettings::UserScope, kNewOrg, kAppBridge);
        if (probe.value(kMarkerKey, 0).toInt() >= kMarkerVersion)
            return;  // gia' migrato
    }

    // 1) PRIMA le cartelle: se scrivessimo l'INI adesso creeremmo
    //    %APPDATA%\Decodium e il rename di %APPDATA%\IU8LMC fallirebbe.
    //    Vecchio org "IU8LMC" + app "Decodium" -> %APPDATA%\IU8LMC\Decodium.
    //    Nuovo  org "Decodium" + app "Decodium" -> %APPDATA%\Decodium\Decodium.
    //    Rinominando la sola cartella org, TUTTI i dati (db, cache, profili
    //    "Decodium - ALPHA"/"- BRAVO", MultiSettings) si riallineano da soli.
    for (const char* var : {"APPDATA", "LOCALAPPDATA"}) {
        QString const base = qEnvironmentVariable(var);
        if (base.isEmpty())
            continue;
        QString const legacyDir = QDir(base).absoluteFilePath(QString::fromLatin1(kLegacyOrg));
        QString const newDir    = QDir(base).absoluteFilePath(QString::fromLatin1(kNewOrg));
        if (QFileInfo::exists(legacyDir))
            moveTree(legacyDir, newDir);
    }

    // 2) Registro -> INI, per entrambi gli scope storici.
    int migrated = 0;
    {
        QSettings fromReg(QSettings::NativeFormat, QSettings::UserScope, kNewOrg, kAppBridge);
        QSettings toIni(QSettings::IniFormat, QSettings::UserScope, kNewOrg, kAppBridge);
        migrated += copyAll(fromReg, toIni);
        toIni.sync();
    }
    {
        QSettings fromReg(QSettings::NativeFormat, QSettings::UserScope, kLegacyOrg, kAppDefault);
        QSettings toIni(QSettings::IniFormat, QSettings::UserScope, kNewOrg, kAppDefault);
        migrated += copyAll(fromReg, toIni);
        toIni.sync();
    }

    // 3) Via le chiavi legacy: da qui in poi il registro non deve piu' esistere.
    wipeRegistryTree(QStringLiteral("HKEY_CURRENT_USER\\Software\\Decodium"));
    wipeRegistryTree(QStringLiteral("HKEY_CURRENT_USER\\Software\\IU8LMC"));

    // 4) Marker: la migrazione non si ripete.
    {
        QSettings done(QSettings::IniFormat, QSettings::UserScope, kNewOrg, kAppBridge);
        done.setValue(kMarkerKey, kMarkerVersion);
        done.sync();
    }
    qInfo("Decodium storage migration: %d chiavi dal registro -> INI, registro legacy rimosso",
          migrated);
#endif  // Q_OS_WIN
}

}  // namespace DecodiumStorageMigration
