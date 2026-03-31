# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.8

Scope: update cycle from `1.5.7` to `1.5.8`.

## English

`1.5.8` focuses on closing the promoted native C++ runtime for the FTX family, removing the remaining FST4/Q65/MSK144/SuperFox Fortran residues, hardening macOS shutdown/data-path stability, and fixing Linux GCC/Ubuntu release builds.

### Detailed Changes (`1.5.7 -> 1.5.8`)

- completed the promoted native runtime path for FT8, FT4, FT2, Q65, MSK144, SuperFox, and FST4/FST4W so these modes no longer depend on mode-specific Fortran runtime code.
- migrated the remaining FST4/FST4W decode-core, LDPC, shared-DSP helpers, and reference/simulator chain to native C++ in the promoted tree.
- removed the old promoted-mode Fortran residues including `ana64`, `q65_subs`, the MSK144/MSK40 legacy snapshot files, and the historical SuperFox subtree once their native replacements were validated.
- promoted native replacements for `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101`, and `ldpcsim240_74`.
- fixed the macOS close-time crash caused by premature global `fftwf_cleanup()` while thread-local FFTW plans were still finalizing.
- hardened `MainWindow::dataSink` and `fastSink` with safer frame clamping, guarded indexing, and cached writable-data-dir handling.
- fixed Ubuntu/GCC 15 build failures around `_q65_mask`, `pack28`, legacy tool linkage to migrated symbols such as `four2a_`, and the MSK40 off-by-one bug in `decodeframe40_native`.
- expanded `test_qt_helpers` and utility smoke coverage for shared DSP behavior, FST4 parity/oracle compatibility, and native Q65 compatibility entry points.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, release notes, and GitHub release body to `1.5.8`.

### Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Linux Minimum Requirements

Hardware:

- `x86_64` CPU with SSE2
- dual-core 2.0 GHz or better
- 4 GB RAM minimum
- 500 MB free disk space
- audio/CAT/serial/USB hardware suitable for weak-signal operation

Software:

- `glibc >= 2.35`
- `libfuse2` if mounting the AppImage directly
- ALSA, PulseAudio, or PipeWire
- X11 or Wayland desktop session capable of running Qt5 AppImages

### Startup Guidance

macOS quarantine workaround:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage extract-run flow:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

`1.5.8` e' focalizzata sulla chiusura del runtime promosso nativo C++ per la famiglia FTX, sulla rimozione degli ultimi residui Fortran FST4/Q65/MSK144/SuperFox, sull'hardening della stabilita' macOS in chiusura/percorso dati, e sulla correzione delle build Linux GCC/Ubuntu.

### Modifiche Dettagliate (`1.5.7 -> 1.5.8`)

- completato il path runtime promosso nativo per FT8, FT4, FT2, Q65, MSK144, SuperFox e FST4/FST4W, cosi' questi modi non dipendono piu' da codice runtime Fortran specifico del modo.
- migrata a C++ nativo la catena residua FST4/FST4W composta da decode-core, LDPC, helper DSP condivisi e reference/simulator nel tree promosso.
- rimossi i vecchi residui Fortran dei modi promossi, inclusi `ana64`, `q65_subs`, i file snapshot legacy MSK144/MSK40 e lo storico subtree SuperFox una volta validati i sostituti nativi.
- promossi i sostituti nativi per `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` e `ldpcsim240_74`.
- corretto il crash macOS in chiusura causato da `fftwf_cleanup()` globale prematuro mentre i piani FFTW thread-local erano ancora in finalizzazione.
- resi piu' robusti `MainWindow::dataSink` e `fastSink` con clamp dei frame piu' sicuro, indici protetti e gestione cache della writable-data-dir.
- corretti i fallimenti Ubuntu/GCC 15 relativi a `_q65_mask`, `pack28`, link dei tool legacy verso simboli C++ migrati come `four2a_`, e il bug off-by-one MSK40 in `decodeframe40_native`.
- ampliata la copertura `test_qt_helpers` e smoke-test utility per comportamento DSP condiviso, compatibilita' FST4 parity/oracle e entry point Q65 nativi.
- allineati a `1.5.8` metadati versione locali, default workflow, readme, documentazione, changelog, note release e body GitHub.

### Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / sperimentale)*
- Linux x86_64 AppImage

### Requisiti Minimi Linux

Hardware:

- CPU `x86_64` con SSE2
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM
- 500 MB liberi su disco
- hardware audio/CAT/seriale/USB adatto al weak-signal

Software:

- `glibc >= 2.35`
- `libfuse2` se si vuole montare l'AppImage direttamente
- ALSA, PulseAudio o PipeWire
- sessione desktop X11 o Wayland capace di eseguire AppImage Qt5

### Guida Avvio

Workaround quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l’AppImage e poi eseguendo il programma dalla cartella estratta.

Flusso Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

`1.5.8` se centra en cerrar el runtime promovido nativo C++ para la familia FTX, eliminar los ultimos residuos Fortran FST4/Q65/MSK144/SuperFox, endurecer la estabilidad macOS al cerrar/ruta de datos y corregir las builds Linux GCC/Ubuntu.

### Cambios Detallados (`1.5.7 -> 1.5.8`)

- completado el camino runtime promovido nativo para FT8, FT4, FT2, Q65, MSK144, SuperFox y FST4/FST4W, de modo que estos modos ya no dependen de codigo runtime Fortran especifico del modo.
- migrada a C++ nativo la cadena residual FST4/FST4W de decode-core, LDPC, helpers DSP compartidos y referencia/simulador en el arbol promovido.
- eliminados los viejos residuos Fortran de modos promovidos, incluidos `ana64`, `q65_subs`, los ficheros snapshot legacy MSK144/MSK40 y el historico subtree SuperFox una vez validados los reemplazos nativos.
- promovidos reemplazos nativos para `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` y `ldpcsim240_74`.
- corregido el crash macOS al cerrar causado por `fftwf_cleanup()` global prematuro mientras los planes FFTW thread-local seguian finalizandose.
- reforzados `MainWindow::dataSink` y `fastSink` con clamping de frames mas seguro, indices protegidos y manejo cache del directorio writable-data.
- corregidos los fallos Ubuntu/GCC 15 relacionados con `_q65_mask`, `pack28`, enlace de herramientas legacy con simbolos C++ migrados como `four2a_`, y el bug off-by-one MSK40 en `decodeframe40_native`.
- ampliada la cobertura `test_qt_helpers` y smoke-tests de utilidades para comportamiento DSP compartido, compatibilidad FST4 parity/oracle y puntos de entrada Q65 nativos.
- alineados con `1.5.8` los metadatos locales de version, defaults de workflow, readmes, documentacion, changelog, notas release y body GitHub.

### Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Requisitos Minimos Linux

Hardware:

- CPU `x86_64` con SSE2
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM
- 500 MB libres en disco
- hardware de audio/CAT/serie/USB adecuado para weak-signal

Software:

- `glibc >= 2.35`
- `libfuse2` si se quiere montar la AppImage directamente
- ALSA, PulseAudio o PipeWire
- sesion de escritorio X11 o Wayland capaz de ejecutar AppImages Qt5

### Guia de Arranque

Workaround cuarentena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Flujo Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
