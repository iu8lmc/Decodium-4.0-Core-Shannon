#!/usr/bin/env python3
"""Forza QSettings::IniFormat su tutti i punti che aprono le impostazioni di Decodium.

PERCHE' ESISTE QUESTO SCRIPT
----------------------------
Su Windows QSettings("Decodium", "Decodium3") scrive nel REGISTRO. I valori
sopravvivono alla disinstallazione e tornano a mordere: un uiSpectrumHeight=2808
salvato con il pannello alto 152px rendeva la barra del waterfall inservibile
per sempre. Vogliamo tutto in file INI, cosi' l'uninstaller puo' cancellare
davvero ogni traccia.

TRAPPOLA Qt (verificata empiricamente, non e' un'opinione):
QSettings::setDefaultFormat(IniFormat) NON basta. Vale solo per il costruttore
QSettings(QObject*); i costruttori QSettings(org, app) e QSettings(scope, org, app)
usano NativeFormat CABLATO e ignorano il default. Sonda a runtime:
    defaultFormat=1 (Ini)  ->  fileName=\\HKEY_CURRENT_USER\\Software\\Decodium\\Decodium3
Quindi ogni singolo sito va convertito al costruttore con formato esplicito.

MANUTENZIONE DOPO GLI ABSORB DA elisir80
----------------------------------------
Upstream continua a scrivere QSettings("Decodium","Decodium3") (91 siti solo in
src/bridge/DecodiumBridge.cpp). Dopo ogni "allineati a elisir80" rilancia:

    python tools/enforce_ini_settings.py            # applica
    python tools/enforce_ini_settings.py --check    # solo verifica (exit 1 se ne restano)

Lo script e' IDEMPOTENTE: i siti gia' convertiti non vengono ritoccati.
"""

import argparse
import pathlib
import re
import sys

# Un sito = QSettings [nome] ( [scope,] <org>, <app> )  con org fra quelle nostre.
# Le forme reali nel codice includono spazi, QStringLiteral opzionale e TEMPORANEI
# senza nome di variabile (facili da dimenticare: sono write-and-forget):
#   QSettings s("Decodium", "Decodium3")
#   QSettings catLastSettings(QStringLiteral("Decodium"), QStringLiteral("Decodium3"))
#   QSettings reg (QSettings::UserScope, QStringLiteral ("Decodium"), QStringLiteral ("SecureStore"))
#   QSettings("Decodium", "Decodium3").setValue(...)      <-- temporaneo, senza nome
SITE = re.compile(
    r'(QSettings\s*(?:\w+\s*)?)'                                   # 1 dichiarazione (nome opzionale)
    r'([({])\s*'                                                   # 2 apertura ( o {
    r'(?:QSettings::UserScope\s*,\s*)?'                            #   scope opzionale: lo riscriviamo noi
    r'((?:QStringLiteral\s*\(\s*)?"(?:Decodium|IU8LMC)"\s*\)?)'    # 3 organizzazione
    r'\s*,\s*'
    r'((?:QStringLiteral\s*\(\s*)?"[A-Za-z0-9_ .-]+"\s*\)?)'       # 4 applicazione
    r'\s*([)}])'                                                   # 5 chiusura
)
REPL = r'\1\2QSettings::IniFormat, QSettings::UserScope, \3, \4\5'

# File da NON toccare:
#  - DecodiumStorageMigration.cpp: legge di proposito il registro legacy per migrarlo.
#  - DecodiumCatManager/TransceiverManager: i NativeFormat li' leggono
#    HKLM\HARDWARE\DEVICEMAP\SERIALCOMM (porte COM di Windows), non roba nostra —
#    ma usano anche QSettings(org, app) per le impostazioni, quindi vanno convertiti:
#    il regex tocca solo i siti con le NOSTRE org, gli altri restano intatti.
SKIP_FILES = {"DecodiumStorageMigration.cpp"}
SKIP_DIRS = ("build", "repo", "dist-windows-x64", "_parity_slots", ".git",
             "build_mingw64", "build-ft2link-tests", "build-ft2link-tests-clean")


def candidate_files(root: pathlib.Path):
    for path in sorted(root.rglob("*")):
        if path.suffix not in (".cpp", ".h", ".hpp"):
            continue
        if path.name in SKIP_FILES:
            continue
        rel = path.relative_to(root)
        if any(part.startswith(SKIP_DIRS) or part in SKIP_DIRS for part in rel.parts):
            continue
        yield path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true",
                        help="non modifica nulla; exit 1 se restano siti da convertire")
    parser.add_argument("--root", default=".", help="radice del repo (default: cwd)")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    total_sites = 0
    touched_files = 0

    for path in candidate_files(root):
        # newline="" in LETTURA disabilita la traduzione universal-newline: i \r\n
        # restano nella stringa. I sorgenti qui sono CRLF e riscriverli in LF
        # produrrebbe un diff dell'INTERO file (e conflitti giganti a ogni absorb).
        with open(path, "r", encoding="utf-8", errors="surrogateescape", newline="") as fh:
            original = fh.read()
        converted, count = SITE.subn(REPL, original)
        if not count:
            continue
        total_sites += count
        touched_files += 1
        rel = path.relative_to(root)
        print(f"{'DA CONVERTIRE' if args.check else 'convertiti'} {count:3d}  {rel}")
        if not args.check:
            with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
                fh.write(converted)

    if args.check:
        if total_sites:
            print(f"\nRESTANO {total_sites} siti su registro in {touched_files} file "
                  f"-> lancia: python tools/enforce_ini_settings.py")
            return 1
        print("OK: nessun sito QSettings su registro.")
        return 0

    print(f"\nTotale: {total_sites} siti convertiti in {touched_files} file.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
