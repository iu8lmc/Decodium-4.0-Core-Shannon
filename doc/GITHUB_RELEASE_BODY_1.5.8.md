# Decodium 1.5.8 (Fork 9H1SR)

## English

Release highlights (`1.5.7 -> 1.5.8`):

- completed the promoted native C++ runtime for FT8, FT4, FT2, Q65, MSK144, SuperFox, and FST4/FST4W with no mode-specific Fortran residues on the active path.
- migrated the remaining FST4/FST4W decode-core, LDPC, shared-DSP helpers, and reference/simulator chain to native C++.
- removed promoted-mode tree residues such as `ana64`, `q65_subs`, MSK144/MSK40 legacy snapshots, and the historical SuperFox Fortran subtree after native promotion.
- promoted native replacements for `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101`, and `ldpcsim240_74`.
- fixed the macOS close-time FFTW crash and hardened `MainWindow::dataSink` / `fastSink`.
- fixed Linux/GCC 15 issues around `_q65_mask`, `pack28`, legacy tool linkage to migrated symbols, and the MSK40 off-by-one bug in `decodeframe40_native`.
- expanded `test_qt_helpers` and utility smoke coverage for shared DSP, FST4 parity/oracle behavior, and native Q65 compatibility.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, and release notes to semantic version `1.5.8`.

Release assets:

- `decodium3-ft2-1.5.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` with SSE2, dual-core 2.0 GHz+, 4 GB RAM minimum, 500 MB free disk
- `glibc >= 2.35`
- `libfuse2` / FUSE2 for direct AppImage mounting
- ALSA, PulseAudio, or PipeWire
- X11 or Wayland session capable of running Qt5 AppImages

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`1.5.7 -> 1.5.8`):

- completato il runtime promosso nativo C++ per FT8, FT4, FT2, Q65, MSK144, SuperFox e FST4/FST4W senza residui Fortran specifici del modo nel path attivo.
- migrata a C++ nativo la catena residua FST4/FST4W composta da decode-core, LDPC, helper DSP condivisi e reference/simulator.
- rimossi i residui tree-only dei modi promossi come `ana64`, `q65_subs`, snapshot legacy MSK144/MSK40 e il subtree storico SuperFox Fortran dopo la promozione nativa.
- promossi i sostituti nativi per `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` e `ldpcsim240_74`.
- corretto il crash macOS in chiusura legato a FFTW e resi piu' robusti `MainWindow::dataSink` / `fastSink`.
- corretti i problemi Linux/GCC 15 relativi a `_q65_mask`, `pack28`, link dei tool legacy ai simboli migrati e bug off-by-one MSK40 in `decodeframe40_native`.
- ampliata la copertura `test_qt_helpers` e smoke-test utility per DSP condiviso, comportamento parity/oracle FST4 e compatibilita' Q65 nativa.
- allineati alla semver `1.5.8` metadati versione locali, default workflow, readme, documentazione, changelog e note release.

Asset release:

- `decodium3-ft2-1.5.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- `x86_64` con SSE2, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB liberi
- `glibc >= 2.35`
- `libfuse2` / FUSE2 per montare direttamente l'AppImage
- ALSA, PulseAudio o PipeWire
- sessione X11 o Wayland capace di eseguire AppImage Qt5

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l’AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

Non vengono prodotti installer `.pkg`.

## Espanol

Resumen (`1.5.7 -> 1.5.8`):

- completado el runtime promovido nativo C++ para FT8, FT4, FT2, Q65, MSK144, SuperFox y FST4/FST4W sin residuos Fortran especificos del modo en el camino activo.
- migrada a C++ nativo la cadena residual FST4/FST4W de decode-core, LDPC, helpers DSP compartidos y referencia/simulador.
- eliminados residuos tree-only de modos promovidos como `ana64`, `q65_subs`, snapshots legacy MSK144/MSK40 y el subtree historico SuperFox Fortran tras la promocion nativa.
- promovidos reemplazos nativos para `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, `sftx`, `fst4sim`, `ldpcsim240_101` y `ldpcsim240_74`.
- corregido el crash macOS al cerrar ligado a FFTW y reforzados `MainWindow::dataSink` / `fastSink`.
- corregidos los problemas Linux/GCC 15 relacionados con `_q65_mask`, `pack28`, enlazado legacy a simbolos migrados y bug off-by-one MSK40 en `decodeframe40_native`.
- ampliada la cobertura `test_qt_helpers` y smoke-tests de utilidades para DSP compartido, comportamiento parity/oracle FST4 y compatibilidad Q65 nativa.
- alineados con la semver `1.5.8` los metadatos locales de version, defaults de workflow, readmes, documentacion, changelog y notas release.

Artefactos release:

- `decodium3-ft2-1.5.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.8-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- `x86_64` con SSE2, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB libres
- `glibc >= 2.35`
- `libfuse2` / FUSE2 para montar directamente la AppImage
- ALSA, PulseAudio o PipeWire
- sesion X11 o Wayland capaz de ejecutar AppImages Qt5

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
