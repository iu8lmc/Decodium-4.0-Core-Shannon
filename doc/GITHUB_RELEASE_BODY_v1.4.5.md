# Decodium 3 FT2 v1.4.5 (Fork 9H1SR)

## English

Release highlights (`v1.4.4 -> v1.4.5`):

- FT2 AutoCQ reliability hardening:
- stronger active-partner continuity during QSO.
- directed reply detection improved while calling CQ.
- non-partner `73` no longer hijacks signoff.
- stale deferred autolog triggers are safely ignored.
- FT2 signoff/log flow stabilization:
- deferred snapshot-based autolog handoff refined.
- retry window updated to `MAX_TX_RETRIES=5`.
- FT2 decode/runtime improvements:
- `TΔ` decode column support.
- column alignment fixes for marker spacing and Unicode minus glyphs.
- weak async confirmations preserved before dedupe.
- TX timing fixes for FT2 on TCI and soundcard paths.
- Remote/web/UI fixes:
- remote dashboard startup/fetch regression fixed.
- `View -> Display a cascata` (waterfall) activation fixed.
- localized 12-char minimum web password hint restored.
- FT2 UDP logger interoperability fix:
- payload extraction corrected so RumLog-style clients keep full callsign (no missing first character).

Release assets:

- `decodium3-ft2-v1.4.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.5-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.5-linux-x86_64.AppImage.sha256.txt`

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

Punti principali (`v1.4.4 -> v1.4.5`):

- Hardening affidabilita' FT2 AutoCQ:
- continuita' partner attivo rafforzata durante QSO.
- migliorato aggancio risposte dirette durante CQ.
- `73` non-partner non altera piu il signoff.
- trigger deferred autolog stale ignorati in sicurezza.
- Stabilizzazione signoff/log FT2:
- handoff autolog basato su snapshot deferred migliorato.
- finestra retry aggiornata a `MAX_TX_RETRIES=5`.
- Migliorie decode/runtime FT2:
- supporto colonna decode `TΔ`.
- fix allineamento colonne per spaziatura marker e segni meno Unicode.
- conferme async deboli preservate prima della deduplica.
- Fix timing TX FT2 su percorsi TCI e soundcard.
- Fix web/UI:
- corretta regressione startup/fetch dashboard remota.
- corretta apertura `Vista -> Display a cascata` (waterfall).
- ripristinato hint web localizzato per password minima 12 caratteri.
- Fix interoperabilita' logger UDP FT2:
- payload decode corretto, client stile RumLog non perdono il primo carattere del callsign.

Asset release:

- `decodium3-ft2-v1.4.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.5-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.5-linux-x86_64.AppImage.sha256.txt`

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

Resumen (`v1.4.4 -> v1.4.5`):

- Hardening de fiabilidad FT2 AutoCQ:
- continuidad de pareja activa reforzada durante QSO.
- mejor captura de respuestas dirigidas durante CQ.
- `73` de no-pareja ya no secuestra el signoff.
- triggers deferred autolog stale ignorados de forma segura.
- Estabilizacion signoff/log FT2:
- handoff autolog con snapshot deferred mejorado.
- ventana de reintentos actualizada a `MAX_TX_RETRIES=5`.
- Mejoras decode/runtime FT2:
- soporte de columna decode `TΔ`.
- fix de alineacion de columnas por espaciado de marcador y signos menos Unicode.
- confirmaciones async debiles preservadas antes de dedupe.
- Fix de timing TX FT2 en rutas TCI y soundcard.
- Fix web/UI:
- corregida regresion startup/fetch de dashboard remota.
- corregida apertura `Vista -> Display a cascata` (waterfall).
- restaurado hint web localizado para password minima de 12 caracteres.
- Fix de interoperabilidad logger UDP FT2:
- payload decode corregido, clientes tipo RumLog mantienen callsign completo.

Artefactos release:

- `decodium3-ft2-v1.4.5-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.5-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.5-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.5-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.5-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.5-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.5-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.5-linux-x86_64.AppImage.sha256.txt`

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
