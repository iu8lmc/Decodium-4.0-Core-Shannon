# Release Notes / Note di Rilascio - Fork v1.3.6

Date: 2026-03-02  
Scope: UX/localization correctness, CTY/country display reliability, branding alignment, release packaging refresh.

## English

### Summary

- Diagnostic mode menu behavior fixed: true ON/OFF without forced restart.
- CLI language override fixed (`--language`) with strict precedence over locale autoload.
- Country-name visibility in Band Activity restored for normal FT2/FT4/FT8 operation.
- PSKReporter `Using:` branding aligned with app title and fork identity.
- CTY parser hardened for modern large prefix-detail blocks.

### Detailed Changes

- Event logging / diagnostic mode:
  - `Diagnostic mode` removed from exclusive action group.
  - Added persisted `DiagnosticEventLogging` state.
  - Startup state detection now validates `wsjtx_log_config.ini` content.
  - Switching to `Default event logging` / `Disable event logging` now auto-clears diagnostic mode state.
- Localization:
  - `L10nLoader` now treats any explicit `--language` as authoritative and skips locale auto-load.
- Country display / FT points:
  - FT points suffix (`a1`, `a2`, ...) now appears only for ARRL Digi context.
  - Normal operation preserves country name append (fix for missing DXCC country names).
- CTY parsing:
  - Increased prefix-detail safety cap from 64 KiB to 4 MiB to accept recent valid `cty.dat` snapshots.
- Branding consistency:
  - Restored window-title format: `Fork by Salvatore Raccampo 9H1SR`.
  - PSKReporter `Using:` field now uses `program_title()` without legacy appended revision suffix.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced.

### Linux Minimum Requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
  - Linux with `glibc >= 2.35`
  - `libfuse2` / FUSE2 support
  - ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Corretto comportamento menu modalita' diagnostica: ON/OFF reale senza riavvio forzato.
- Corretto override lingua CLI (`--language`) con precedenza rigorosa sul locale automatico.
- Ripristinata visibilita' nomi country in Band Activity in uso normale FT2/FT4/FT8.
- Allineato branding `Using:` di PSKReporter al titolo app e identita' fork.
- Parser CTY irrobustito per blocchi prefissi moderni molto grandi.

### Modifiche Dettagliate

- Logging eventi / modalita' diagnostica:
  - `Modalita' diagnostica` rimossa dal gruppo azioni esclusivo.
  - Aggiunto stato persistente `DiagnosticEventLogging`.
  - Rilevamento stato in avvio basato su validazione contenuto `wsjtx_log_config.ini`.
  - Passaggio a `Default event logging` / `Disable event logging` ora resetta automaticamente lo stato diagnostico.
- Localizzazione:
  - `L10nLoader` tratta ogni `--language` esplicito come autoritativo e salta autoload da locale.
- Display country / punti FT:
  - Suffisso punti FT (`a1`, `a2`, ...) mostrato solo in contesto ARRL Digi.
  - In uso normale viene mantenuto l'append del nome country (fix nomi DXCC mancanti).
- Parsing CTY:
  - Limite di sicurezza blocchi prefisso alzato da 64 KiB a 4 MiB per accettare snapshot `cty.dat` recenti e validi.
- Coerenza branding:
  - Ripristinato formato titolo finestra: `Fork by Salvatore Raccampo 9H1SR`.
  - Campo `Using:` di PSKReporter ora usa `program_title()` senza suffisso revision legacy.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente.

### Requisiti Minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
  - Linux con `glibc >= 2.35`
  - supporto `libfuse2` / FUSE2
  - stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
