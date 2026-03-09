# Decodium v3.0 SE "Raptor" - Fork 9H1SR v1.4.1 (Italiano)

Per la versione bilingue completa (English + Italiano), vedere [README.md](README.md).

## Sintesi italiana (v1.4.1)

Questa release consolida tutti i fix e le nuove feature del ciclo `v1.4.0 -> v1.4.1`:

- Stabilizzazione flusso decode FT2:
  - gestione split righe packed (niente righe fuse/disallineate),
  - soppressione near-duplicate con finestra 5 secondi,
  - mantenimento preferenziale della riga con SNR migliore,
  - filtro uniforme su decode normale e async.
- Correzione comportamento Async L2:
  - visibile solo in FT2,
  - disattivazione automatica quando si esce da FT2.
- Corrette regressioni startup/cambio modalita':
  - auto-selezione startup da frequenza radio ora one-shot (niente riforzatura continua modalita'),
  - risposta iniziale al cambio modalita' ripristinata,
  - niente apertura forzata del waterfall durante il cambio modalita'.
- Maturazione dashboard web remota:
  - impostazioni LAN direttamente dal menu app (bind/porta/user/token),
  - login username/password,
  - UX mobile/PWA migliorata,
  - controlli principali piu' chiari (mode/band/rx/tx/auto-cq).
- Hardening CAT/UDP/TCI delle release precedenti mantenuto.
- Feature mappa/runtime mantenute:
  - toggle greyline opzionale,
  - distanza sul path mappa attivo in km/mi.
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

- [RELEASE_NOTES_v1.4.1.md](RELEASE_NOTES_v1.4.1.md)
- [CHANGELOG.md](CHANGELOG.md)
- [doc/GITHUB_RELEASE_BODY_v1.4.1.md](doc/GITHUB_RELEASE_BODY_v1.4.1.md)
- [doc/README.it.md](doc/README.it.md)
- [doc/WEBAPP_SETUP_GUIDE.it.md](doc/WEBAPP_SETUP_GUIDE.it.md)
- [doc/WEBAPP_SETUP_GUIDE.en-GB.md](doc/WEBAPP_SETUP_GUIDE.en-GB.md)
- [doc/WEBAPP_SETUP_GUIDE.es.md](doc/WEBAPP_SETUP_GUIDE.es.md)
- [doc/SECURITY_BUG_ANALYSIS_REPORT.md](doc/SECURITY_BUG_ANALYSIS_REPORT.md)
