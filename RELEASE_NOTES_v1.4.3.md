# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.4.3

Date: 2026-03-10  
Scope: update cycle from `v1.4.2` to `v1.4.3`, focused on FT2 Async L2 Linux crash hardening and Wait/AutoSeq QSO protection behavior.

## English

### Summary

`v1.4.3` is a stability-focused release:

- fixes Linux FT2 Async L2 crash observed at first decode;
- hardens C++/Fortran async decode boundaries;
- tightens Wait Features + AutoSeq behavior to protect ongoing QSOs from unintended takeover.

### Detailed Changes (`v1.4.2 -> v1.4.3`)

- FT2 Async L2 (Linux crash hardening):
  - added strict row bounds in `ft2_triggered_decode` (`ndecodes/nout` capped to 100);
  - async C++ parser now reads fixed-size Fortran rows safely (no implicit C-string over-read);
  - async buffers are explicitly zeroed and row counters reset on each decode cycle and toggle state changes.
- Wait Features + AutoSeq QSO lock:
  - active-QSO lock now uses current partner call from runtime (`m_hisCall`) as primary source;
  - lock now engages earlier (`REPLYING`..`SIGNOFF`) when Wait Features + AutoSeq are enabled;
  - reduces risk of starting a new transmission over an active QSO.
- Release alignment:
  - fork release version updated to `v1.4.3` in app title/build metadata;
  - README/docs/workflow defaults updated to `v1.4.3` (EN/IT/ES);
  - release targets unchanged: Tahoe/Sequoia Apple Silicon, Sequoia/Monterey Intel, Linux AppImage.

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

### Linux AppImage recommendation

To avoid issues caused by AppImage read-only filesystem mode, it is recommended to extract first and run from the extracted folder.

Run in terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`v1.4.3` e' una release focalizzata sulla stabilita':

- fix crash Linux FT2 Async L2 al primo decode;
- hardening del boundary async C++/Fortran;
- logica Wait Features + AutoSeq irrigidita per proteggere i QSO in corso.

### Modifiche Dettagliate (`v1.4.2 -> v1.4.3`)

- FT2 Async L2 (hardening crash Linux):
  - aggiunti limiti rigidi in `ft2_triggered_decode` (`ndecodes/nout` massimo 100);
  - parser async C++ ora legge righe Fortran a lunghezza fissa in modo sicuro (niente over-read C-string implicito);
  - buffer async azzerati esplicitamente e contatori righe resettati a ogni ciclo decode/toggle.
- Lock QSO attivo con Wait Features + AutoSeq:
  - lock QSO attivo ora usa come riferimento primario il partner runtime corrente (`m_hisCall`);
  - lock attivo anticipato su finestra `REPLYING`..`SIGNOFF` quando Wait Features + AutoSeq sono abilitati;
  - ridotto rischio di avviare una nuova trasmissione sopra un QSO gia' in corso.
- Allineamento release:
  - versione fork aggiornata a `v1.4.3` in titolo app/metadati build;
  - README/documentazione/default workflow aggiornati a `v1.4.3` (EN/IT/ES);
  - target release invariati: Tahoe/Sequoia Apple Silicon, Sequoia/Monterey Intel, Linux AppImage.

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

### Avvio consigliato AppImage su Linux

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

### Resumen

`v1.4.3` es una release centrada en estabilidad:

- corrige el crash Linux FT2 Async L2 al primer decode;
- endurece el limite C++/Fortran del decode async;
- refuerza Wait Features + AutoSeq para proteger QSOs en curso.

### Cambios Detallados (`v1.4.2 -> v1.4.3`)

- FT2 Async L2 (hardening crash Linux):
  - limites estrictos en `ft2_triggered_decode` (`ndecodes/nout` maximo 100);
  - parser async C++ ahora lee filas Fortran de longitud fija de forma segura (sin over-read implicito de C-string);
  - buffers async se limpian explicitamente y los contadores se reinician en cada ciclo decode/toggle.
- Bloqueo de QSO activo con Wait Features + AutoSeq:
  - el bloqueo de QSO activo usa ahora la pareja runtime actual (`m_hisCall`) como fuente principal;
  - bloqueo activo desde antes (`REPLYING`..`SIGNOFF`) cuando Wait Features + AutoSeq estan habilitados;
  - reduce el riesgo de iniciar nueva transmision sobre un QSO en curso.
- Alineacion release:
  - version fork actualizada a `v1.4.3` en titulo app/metadatos de build;
  - README/docs/default workflows actualizados a `v1.4.3` (EN/IT/ES);
  - objetivos release sin cambios: Tahoe/Sequoia Apple Silicon, Sequoia/Monterey Intel, Linux AppImage.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: no se generan instaladores `.pkg`.

### Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio en disco: al menos 500 MB libres
- Runtime/software:
  - Linux con `glibc >= 2.35`
  - soporte `libfuse2` / FUSE2
  - audio ALSA, PulseAudio o PipeWire

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion AppImage Linux

Para evitar problemas causados por el filesystem de solo lectura de AppImage, se recomienda extraer primero y ejecutar desde la carpeta extraida.

Ejecutar en terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
