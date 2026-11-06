#ifndef DECODIUMSTORAGEMIGRATION_HPP
#define DECODIUMSTORAGEMIGRATION_HPP

// IU8LMC — Persistenza fuori dal registro di Windows.
//
// Storia: le impostazioni vivevano in HKCU\Software\Decodium\Decodium3 (e
// HKCU\Software\IU8LMC\Decodium), e i dati sotto %APPDATA%\IU8LMC\... Questo
// produceva (a) valori zombie che sopravvivono alla disinstallazione — es. un
// uiSpectrumHeight=2808 salvato con un pannello alto 152px rendeva la barra del
// waterfall inservibile per sempre — e (b) dati sparsi su piu' radici.
//
// Ora: TUTTO in file INI sotto un'unica radice "Decodium", cosi' l'uninstaller
// puo' cancellare ogni traccia. Questo header e' volutamente autonomo per
// tenere al minimo la superficie di conflitto negli assorbimenti da elisir80:
// in main_qml.cpp servono solo due righe.

namespace DecodiumStorageMigration {

// Nome organizzazione da passare a QCoreApplication::setOrganizationName().
// Windows: "Decodium" (radice unica, niente piu' %APPDATA%\IU8LMC\...).
// Altrove: "IU8LMC" invariato — su macOS/Linux QSettings scrive gia' su file,
// non c'e' nessun registro da bonificare, e cambiare org li' significherebbe
// solo orfanare le impostazioni degli utenti DMG/AppImage senza alcun vantaggio.
const char* organizationName();

// Da chiamare come PRIMA istruzione di main(), prima di qualunque QSettings,
// apertura di log o database. Su Windows: forza QSettings::IniFormat come
// formato di default (cosi' TUTTI i QSettings(org, app) sparsi nel codice
// smettono di scrivere nel registro senza doverli toccare uno per uno) ed
// esegue runOnce(). Fuori da Windows non fa nulla.
void configure();

// Esegue UNA VOLTA la migrazione legacy -> nuovo layout:
//   1) HKCU\Software\Decodium\Decodium3  -> <config>/Decodium/Decodium3.ini
//   2) HKCU\Software\IU8LMC\Decodium     -> <config>/Decodium/Decodium.ini
//   3) %APPDATA%\IU8LMC     -> %APPDATA%\Decodium      (rename, istantaneo)
//      %LOCALAPPDATA%\IU8LMC-> %LOCALAPPDATA%\Decodium (rename, istantaneo)
//   4) cancella le chiavi di registro legacy
//
// Idempotente: un marker nel nuovo INI impedisce ri-esecuzioni. Va chiamata
// SUBITO dopo QSettings::setDefaultFormat(QSettings::IniFormat) e PRIMA di
// qualunque lettura di impostazioni, apertura del log o del database.
// Non richiede che QApplication esista (usa org/app espliciti).
void runOnce();

}  // namespace DecodiumStorageMigration

#endif  // DECODIUMSTORAGEMIGRATION_HPP
