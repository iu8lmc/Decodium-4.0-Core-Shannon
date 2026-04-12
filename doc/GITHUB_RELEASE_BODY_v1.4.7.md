# Decodium 3 FT2 v1.4.7 (Fork 9H1SR)

## English

Release highlights (`v1.4.6 -> v1.4.7`):

- FT2 runtime/decode hardening:
- async Tx guard raised from `100 ms` to `300 ms`.
- false-decode filter extended with `cty.dat` prefix validation.
- parse-safe FT2 `TΔ` rendering and marker normalization keep columns aligned.
- AutoCQ/QSO correctness:
- stronger active-partner lock during live QSOs.
- missed-period handling aligned with `5` retries.
- stale missed-period state cleared when a valid reply advances the QSO.
- confirmed partner `73` now returns to CQ without an extra `RR73`.
- AutoSpot / DX Cluster:
- configurable AutoSpot endpoint in Settings.
- DxSpider telnet feed used by the cluster window instead of fixed HamQTH feed.
- better telnet prompt/IAC handling, quiet `UNSET/DX` queries, and explicit `autospot.log` diagnostics.
- Web dashboard/runtime:
- AutoSpot toggle exposed in the web panel.
- clearer command/auth feedback and more stable current-mode preset save/apply.
- Desktop/runtime:
- `Astronomical Data` menu state now follows real window closure via titlebar `X`.
- Linux settings pages remain scrollable on small/HiDPI desktops.

Release assets:

- `decodium3-ft2-v1.4.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.7-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.7-linux-x86_64.AppImage.sha256.txt`

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

Punti principali (`v1.4.6 -> v1.4.7`):

- Hardening FT2 runtime/decode:
- guardia async Tx aumentata da `100 ms` a `300 ms`.
- filtro false decode esteso con validazione prefissi `cty.dat`.
- rendering FT2 `TΔ` parse-safe e normalizzazione marker per mantenere l'allineamento colonne.
- Correttezza AutoCQ/QSO:
- lock partner attivo piu' rigoroso durante QSO in corso.
- gestione periodi mancati allineata a `5` retry.
- stato stale dei periodi mancati pulito quando una risposta valida fa avanzare il QSO.
- `73` confermato del partner ora riporta a CQ senza `RR73` extra.
- AutoSpot / DX Cluster:
- endpoint AutoSpot configurabile nelle impostazioni.
- feed cluster via nodo DxSpider telnet al posto del feed HamQTH fisso.
- gestione prompt/IAC telnet migliorata, query silenziose `UNSET/DX` e diagnostica esplicita in `autospot.log`.
- Web dashboard/runtime:
- toggle AutoSpot esposto nel pannello web.
- feedback comandi/auth piu' chiaro e preset modo corrente piu' stabile in save/apply.
- Desktop/runtime:
- stato menu `Dati Astronomici` coerente con chiusura reale tramite `X`.
- pagine impostazioni Linux scrollabili su desktop piccoli/HiDPI.

Asset release:

- `decodium3-ft2-v1.4.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.7-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.7-linux-x86_64.AppImage.sha256.txt`

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

Resumen (`v1.4.6 -> v1.4.7`):

- Hardening FT2 runtime/decode:
- guardia async Tx subida de `100 ms` a `300 ms`.
- filtro false decode ampliado con validacion de prefijos `cty.dat`.
- renderizado FT2 `TΔ` parse-safe y normalizacion de marcador para mantener la alineacion de columnas.
- Correccion AutoCQ/QSO:
- lock de pareja activa mas fuerte durante QSO en curso.
- manejo de periodos perdidos alineado a `5` reintentos.
- estado stale de periodos perdidos limpiado cuando una respuesta valida hace avanzar el QSO.
- `73` confirmado de la pareja ahora devuelve a CQ sin `RR73` extra.
- AutoSpot / DX Cluster:
- endpoint AutoSpot configurable en ajustes.
- feed cluster via nodo DxSpider telnet en lugar del feed HamQTH fijo.
- mejor manejo prompt/IAC telnet, consultas silenciosas `UNSET/DX` y diagnostico explicito en `autospot.log`.
- Web dashboard/runtime:
- toggle AutoSpot expuesto en el panel web.
- feedback de comandos/auth mas claro y preset del modo actual mas estable en save/apply.
- Desktop/runtime:
- estado del menu `Datos Astronomicos` coherente con cierre real mediante `X`.
- paginas de ajustes Linux scrollables en escritorios pequenos/HiDPI.

Artefactos release:

- `decodium3-ft2-v1.4.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.7-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.7-linux-x86_64.AppImage.sha256.txt`

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
