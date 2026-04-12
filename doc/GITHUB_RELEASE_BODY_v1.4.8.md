# Decodium 3 FT2 v1.4.8 (Fork 9H1SR)

## English

Release highlights (`v1.4.7 -> v1.4.8`):

- FT2 timing/operator workflow:
- `NUM_FT2_SYMBOLS` aligned from `105` to `103`.
- FT2 timing margin reduced from `0.5 s` to `0.2 s`.
- added FT2 `Speedy`, `D-CW`, and `TX NOW` support.
- FT2 AutoSeq logic no longer depends on the hidden standard AutoSeq checkbox.
- FT2 QSO/signoff correctness:
- FT2 now waits for real partner final acknowledgment before logging.
- no auto-log when only local signoff was transmitted.
- clean no-log stop when FT2 signoff retries are exhausted.
- same-payload FT2 async duplicates are suppressed across nearby bins.
- Remote Web security:
- non-loopback remote bind now requires a token.
- token length must be at least `12` characters outside loopback.
- wildcard CORS removed from HTTP API.
- WebSocket `Origin` validation added.
- String/buffer safety:
- concrete COM port overflow risk fixed in `lib/ptt.c`.
- bounded formatting extended to related `lib/ft2` and `map65` paths.
- Release/build:
- macOS release packaging now tolerates leftover CPack DMG mounts.

Release assets:

- `decodium3-ft2-v1.4.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.8-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.8-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU (dual-core 2.0 GHz+)
- 4 GB RAM minimum (8 GB recommended)
- at least 500 MB free disk space
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio, or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage recommendation:

To avoid issues caused by the AppImage read-only filesystem, start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`v1.4.7 -> v1.4.8`):

- FT2 timing/workflow operatore:
- `NUM_FT2_SYMBOLS` allineato da `105` a `103`.
- margine FT2 ridotto da `0.5 s` a `0.2 s`.
- aggiunto supporto FT2 `Speedy`, `D-CW` e `TX NOW`.
- AutoSeq FT2 non dipende piu' dalla checkbox standard nascosta.
- Correttezza FT2 QSO/signoff:
- FT2 attende il vero ack finale del partner prima del log.
- niente auto-log se e' stato trasmesso solo il signoff locale.
- stop pulito senza log se i retry signoff FT2 finiscono.
- duplicate async FT2 con stesso payload soppresse anche su bin vicini.
- Sicurezza Remote Web:
- bind remoto non-loopback consentito solo con token.
- token minimo `12` caratteri fuori loopback.
- rimosso wildcard CORS dall'API HTTP.
- aggiunta validazione `Origin` WebSocket.
- Sicurezza stringhe/buffer:
- corretto rischio concreto overflow COM in `lib/ptt.c`.
- estesa formattazione bounded ai path correlati `lib/ft2` e `map65`.
- Release/build:
- packaging macOS piu' robusto contro mount DMG CPack residui.

Asset release:

- `decodium3-ft2-v1.4.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.8-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.8-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64` (dual-core 2.0 GHz+)
- RAM minima 4 GB (consigliati 8 GB)
- almeno 500 MB liberi su disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Avvio consigliato AppImage Linux:

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

I pacchetti `.pkg` non vengono prodotti.

## Espanol

Resumen (`v1.4.7 -> v1.4.8`):

- FT2 timing/workflow operador:
- `NUM_FT2_SYMBOLS` alineado de `105` a `103`.
- margen FT2 reducido de `0.5 s` a `0.2 s`.
- anadido soporte FT2 `Speedy`, `D-CW` y `TX NOW`.
- AutoSeq FT2 ya no depende de la checkbox estandar oculta.
- Correccion FT2 QSO/signoff:
- FT2 espera el ack final real de la pareja antes del log.
- sin auto-log cuando solo se transmitio el signoff local.
- parada limpia sin log si se agotan los reintentos signoff FT2.
- duplicados async FT2 con mismo payload suprimidos tambien en bins cercanos.
- Seguridad Remote Web:
- bind remoto no-loopback permitido solo con token.
- token minimo `12` caracteres fuera de loopback.
- wildcard CORS eliminado del API HTTP.
- anadida validacion `Origin` WebSocket.
- Seguridad cadenas/buffer:
- corregido riesgo concreto overflow COM en `lib/ptt.c`.
- extendida formateacion bounded a paths relacionados `lib/ft2` y `map65`.
- Release/build:
- packaging macOS mas robusto frente a mounts DMG CPack residuales.

Artefactos release:

- `decodium3-ft2-v1.4.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.8-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.8-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- CPU `x86_64` (dual-core 2.0 GHz+)
- RAM 4 GB minimo (8 GB recomendado)
- al menos 500 MB libres en disco
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Recomendacion AppImage Linux:

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
