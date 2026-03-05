# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.3.8 (Italiano)

Per la versione bilingue completa (English + Italiano), vedere [README.md](README.md).

## Sintesi italiana

Questa release fork (`v1.3.8`) aggiorna e completa i fix dalla `v1.3.7` con focus su robustezza CAT/UDP, mappa e UX:

- hardening CAT/Configure remoto: pacchetti Configure generici non forzano piu' FT2;
- hardening UDP controllo: richiesto target id diretto per i comandi di controllo;
- opzione greyline in `Settings -> General` (disattivabile per mappa sempre illuminata);
- distanza visualizzata sul path mappa attivo in km/mi in base alle unita' configurate;
- rifinitura layout controlli top su display piccoli e riallineamento area DX-ped;
- `.pkg` non necessario: release solo DMG/ZIP/SHA256 (macOS) e AppImage/SHA256 (Linux).

## Target release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey (12.x, sperimentale/best-effort)
- Linux x86_64 AppImage

## Hamlib nelle build release

- Su macOS i workflow eseguono `brew update` + `brew upgrade hamlib` prima della compilazione.
- Su Linux i workflow compilano Hamlib dall'ultima release ufficiale GitHub e lo installano in `/usr/local` prima di compilare `ft2`.
- I log CI riportano sempre la versione effettiva usata (`rigctl --version`, `pkg-config --modversion hamlib`).

## Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi (AppImage + log + impostazioni)
- Runtime:
  - Linux con `glibc >= 2.35` (classe Ubuntu 22.04 o successiva)
  - supporto `libfuse2` / FUSE2 per avvio AppImage
  - stack audio ALSA, PulseAudio o PipeWire
- Integrazione stazione: hardware CAT/audio secondo setup radio

## Avvio consigliato AppImage su Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Comando macOS (quarantena)

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Documentazione

- [RELEASE_NOTES_v1.3.8.md](RELEASE_NOTES_v1.3.8.md)
- [CHANGELOG.md](CHANGELOG.md)
- [README.es.md](README.es.md)
- [doc/GITHUB_RELEASE_BODY_v1.3.8.md](doc/GITHUB_RELEASE_BODY_v1.3.8.md)
- [doc/SECURITY_BUG_ANALYSIS_REPORT.md](doc/SECURITY_BUG_ANALYSIS_REPORT.md)
