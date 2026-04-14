# Decodium 3 FT2 v1.4.4 (Fork 9H1SR)

## English

Release highlights (`v1.4.3 -> v1.4.4`):

- Added DXped certificate validation workflow (`.dxcert`) with HMAC-SHA256 signature checks.
- Added DXped certificate gating: DXped mode now requires valid, non-expired, authorized operator certificate.
- Added DXped certificate tools and manager launcher from application menu.
- Added DXped runtime protections:
- dedicated DXped FSM sequencing in decode paths.
- standard `processMessage()` blocked while DXped mode is active.
- CQ fallback from `tx6` into `tx5` when needed.
- FT2 Async L2 is now mandatory:
- local and remote disable requests are ignored in FT2.
- async state is explicitly reset on transitions.
- Added ASYMX progress states `GUARD/TX/RX/IDLE` with 300 ms TX guard.
- Decode/UI correctness fixes:
- forced fixed-pitch decode fonts.
- AP marker alignment fix.
- FT2 marker normalization (`~` to `+`).
- safer double-click decode parsing.
- UDP client id now derives from application name for multi-instance separation.

Release assets:

- `decodium3-ft2-v1.4.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.4-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.4-linux-x86_64.AppImage.sha256.txt`

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are produced.

## Italiano

Punti principali (`v1.4.3 -> v1.4.4`):

- Aggiunto workflow certificati DXped (`.dxcert`) con verifica firma HMAC-SHA256.
- Aggiunto gating DXped: certificato valido/non scaduto/operatore autorizzato ora obbligatorio.
- Aggiunti tool certificati DXped e avvio manager dal menu applicazione.
- Aggiunte protezioni runtime DXped:
- sequenza FSM DXped dedicata nei percorsi decode.
- blocco del `processMessage()` standard quando DXped e' attivo.
- fallback CQ da `tx6` verso `tx5` quando necessario.
- Async L2 in FT2 ora obbligatorio:
- richieste locali/remoto di disabilitazione ignorate in FT2.
- reset stato async esplicito su transizioni.
- Nuovi stati progress ASYMX `GUARD/TX/RX/IDLE` con guardia TX 300 ms.
- Fix decode/UI:
- font decode monospazio forzato.
- fix allineamento marker AP.
- normalizzazione marker FT2 (`~` in `+`).
- parsing doppio click decode piu sicuro.
- ID client UDP derivato dal nome applicazione per separare istanze multiple.

Asset release:

- `decodium3-ft2-v1.4.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.4-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.4-linux-x86_64.AppImage.sha256.txt`

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

Resumen (`v1.4.3 -> v1.4.4`):

- Anadido flujo de certificados DXped (`.dxcert`) con verificacion HMAC-SHA256.
- Anadido gating DXped: certificado valido/no expirado/operador autorizado obligatorio.
- Anadidas herramientas DXped y arranque del manager desde menu.
- Anadidas protecciones runtime DXped:
- secuencia FSM DXped dedicada en rutas decode.
- bloqueo de `processMessage()` estandar cuando DXped esta activo.
- fallback CQ de `tx6` hacia `tx5` cuando hace falta.
- Async L2 en FT2 ahora obligatorio:
- intentos locales/remotos de desactivar se ignoran en FT2.
- reset explicito de estado async en transiciones.
- Nuevos estados ASYMX `GUARD/TX/RX/IDLE` con guardia TX de 300 ms.
- Fixes decode/UI:
- fuente decode monoespaciada forzada.
- fix alineacion de marcador AP.
- normalizacion de marcador FT2 (`~` a `+`).
- parseo de doble click decode mas seguro.
- ID cliente UDP derivado del nombre de aplicacion para separar instancias multiples.

Artefactos release:

- `decodium3-ft2-v1.4.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.4-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.4-linux-x86_64.AppImage.sha256.txt`

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

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Eseguire i seguenti comandi nel terminale:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg`.
