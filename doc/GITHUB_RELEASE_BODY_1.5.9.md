# Decodium 1.5.9 (Fork 9H1SR)

## English

Release highlights (`1.5.8 -> 1.5.9`):

- fixed intermittent Linux FT2/FT4 transmissions where the carrier started on time but the payload started much later.
- removed standard Linux FT2/FT4 TX generation from unnecessary global Fortran runtime contention on the native C++ waveform path.
- added immediate post-waveform start, CAT fallback start, lower-latency Linux TX audio queue defaults, and zero-delay/zero-lead-in FT2 Linux startup alignment.
- changed Linux FT2/FT4 stop behavior to follow real audio/modulator completion.
- limited FT2/FT4 debug waveform dump writes to debug-enabled sessions only.
- fixed a macOS close-time `MainWindow` widget ownership/destruction crash.
- fixed the `Band Hopping` UI so `QSOs to upload` no longer turns red.
- refreshed the FT2 app icon set for macOS and Linux artifacts.
- aligned local version metadata, workflow defaults, readmes, docs, changelog, and release notes to semantic version `1.5.9`.

Release assets:

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

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

Punti principali (`1.5.8 -> 1.5.9`):

- corretti i TX Linux FT2/FT4 intermittenti in cui la portante partiva in orario ma il payload iniziava molto dopo.
- rimosso dal path TX Linux standard FT2/FT4 il contenzioso inutile con il runtime globale Fortran sul path waveform nativo C++.
- aggiunti start immediato post-waveform, fallback CAT, code audio TX Linux a latenza piu' bassa, e allineamento FT2 Linux a zero ritardo/zero lead-in.
- cambiato lo stop Linux FT2/FT4 per seguire la fine reale di audio/modulator.
- limitata la scrittura dei dump waveform FT2/FT4 ai soli casi con debug attivo.
- corretto un crash macOS in chiusura legato a ownership/distruzione widget `MainWindow`.
- corretta la UI `Band Hopping` in modo che `QSOs to upload` non diventi piu' rosso.
- aggiornata la nuova iconografia FT2 per gli artifact macOS e Linux.
- allineati alla semver `1.5.9` metadati versione locali, default workflow, readme, documentazione, changelog e note release.

Asset release:

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

Non vengono prodotti installer `.pkg`.

## Espanol

Resumen (`1.5.8 -> 1.5.9`):

- corregidos los TX Linux FT2/FT4 intermitentes donde la portadora arrancaba a tiempo pero el payload empezaba mucho despues.
- eliminado del camino TX Linux estandar FT2/FT4 el bloqueo innecesario con el runtime global Fortran en el camino waveform nativo C++.
- anadidos arranque inmediato post-waveform, fallback CAT, colas de audio TX Linux de menor latencia, y alineacion FT2 Linux a cero retraso/cero lead-in.
- cambiado el stop Linux FT2/FT4 para seguir la finalizacion real de audio/modulator.
- limitada la escritura de dumps waveform FT2/FT4 solo a casos con debug activo.
- corregido un crash macOS al cerrar ligado a ownership/destruccion de widgets `MainWindow`.
- corregida la UI `Band Hopping` para que `QSOs to upload` ya no se vuelva rojo.
- actualizada la nueva iconografia FT2 para artefactos macOS y Linux.
- alineados con la semver `1.5.9` metadatos locales de version, defaults de workflow, readmes, documentacion, changelog y notas release.

Artefactos release:

- `decodium3-ft2-1.5.9-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.9-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.9-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.9-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.9-linux-x86_64.AppImage.sha256.txt`

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
