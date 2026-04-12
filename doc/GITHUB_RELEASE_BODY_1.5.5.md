# Decodium 1.5.5 (Fork 9H1SR)

## English

Release highlights (`1.5.4 -> 1.5.5`):

- fixed macOS `Preferences...` routing across all UI languages by forcing explicit menu roles and disabling Qt text heuristics for other actions.
- fixed macOS `Settings` height overflow by making the pages scrollable, so the confirm buttons remain reachable.
- migrated FT2 to the in-process decoder path, replacing the older subprocess/shared-memory boundary with worker-based execution and clearer diagnostics.
- removed the older opaque FT2 failure paths in favour of in-process decode reporting.
- standardized FT2 ADIF output to `MODE=MFSK` + `SUBMODE=FT2` and automatically migrate old `MODE=FT2` records with backup preservation.
- improved startup/monitor audio recovery so reopening `Settings > Audio` is no longer required to wake the stream.
- removed the incomplete experimental RTTY UI from the public release path pending further validation.

Release assets:

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64`, dual-core 2.0 GHz+, 4 GB RAM minimum, 500 MB free disk
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

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

Punti principali (`1.5.4 -> 1.5.5`):

- corretta la rotta di `Preferenze...` su macOS in tutte le lingue UI, forzando ruoli menu espliciti e disattivando le euristiche testuali di Qt per le altre azioni.
- corretto lo sforamento verticale di `Settings` su macOS rendendo scrollabili le pagine, cosi' i pulsanti finali restano raggiungibili.
- migrato FT2 sul path decoder in-process, sostituendo il vecchio boundary subprocess/shared memory con esecuzione a worker e diagnostica piu' chiara.
- eliminati i vecchi path opachi di fallimento FT2 a favore del reporting in-process.
- standardizzato l'output ADIF FT2 in `MODE=MFSK` + `SUBMODE=FT2`, con migrazione automatica dei vecchi record `MODE=FT2` e backup.
- migliorato il recupero audio in avvio/monitor, cosi' non serve piu' riaprire `Settings > Audio` per risvegliare lo stream.
- rimossa dal percorso pubblico della release la UI RTTY sperimentale incompleta, in attesa di ulteriore validazione.

Asset release:

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- `x86_64`, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB liberi
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

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

Resumen (`1.5.4 -> 1.5.5`):

- corregida la ruta de `Preferencias...` en macOS para todas las lenguas UI, forzando roles de menu explicitos y desactivando las heuristicas textuales de Qt para las demas acciones.
- corregido el desbordamiento vertical de `Settings` en macOS haciendo scrollables las paginas, de modo que los botones finales sigan accesibles.
- migrado FT2 al camino de decoder in-process, sustituyendo el antiguo boundary subprocess/shared memory por ejecucion con workers y diagnostica mas clara.
- eliminadas las antiguas rutas opacas de fallo FT2 en favor del reporting in-process.
- estandarizado el output ADIF FT2 a `MODE=MFSK` + `SUBMODE=FT2`, con migracion automatica de los viejos registros `MODE=FT2` y backup.
- mejorada la recuperacion de audio al arrancar/monitor, de forma que ya no hace falta reabrir `Settings > Audio` para despertar el stream.
- retirada del camino publico de release la UI RTTY experimental e incompleta, a la espera de validacion adicional.

Artefactos release:

- `decodium3-ft2-1.5.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-1.5.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-1.5.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage`
- `decodium3-ft2-1.5.5-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- `x86_64`, dual-core 2.0 GHz+, minimo 4 GB RAM, 500 MB libres
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

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
