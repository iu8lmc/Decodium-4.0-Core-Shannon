# Decodium 3 FT2 v1.4.6 (Fork 9H1SR)

## English

Release highlights (`v1.4.5 -> v1.4.6`):

- AutoCQ/QSO reliability hardening:
- stable FIFO caller queue behavior restored (v1.3.8 baseline).
- active-partner lock reinforced during running QSO.
- deferred RR73/73 retry flow unified across FT2/FT8/FT4/FST4/Q65/MSK144.
- partner matching improved via normalized decode payload tokens.
- Logging/signoff correctness:
- deferred autolog context recovery stabilized after retry windows.
- reduced premature/no-confirmation logging cases.
- reduced stale-state delayed logging cases.
- Decode and Rx-pane continuity:
- more robust decode payload extraction for variable marker spacing.
- reduced partner-frame misses where messages stayed only on Band Activity.
- Desktop/runtime fixes:
- fixed `View -> World Map` toggle behavior on macOS.
- improved splitter layout handling for secondary decode/map panes.
- Linux settings tabs now scroll on small/HiDPI screens so OK remains reachable.
- macOS startup audio refresh added for already-selected devices.
- Remote dashboard/web app:
- added full `set_tx_frequency` command support.
- added Rx+Tx combined set, separate Rx/Tx actions, and per-mode frequency presets.
- access-password minimum hint remains explicit (12 chars, localized IT/EN).

Release assets:

- `decodium3-ft2-v1.4.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64.dmg` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64.zip` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, if generated)*
- `decodium3-ft2-v1.4.6-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.6-linux-x86_64.AppImage.sha256.txt`

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

Punti principali (`v1.4.5 -> v1.4.6`):

- Hardening affidabilita' AutoCQ/QSO:
- ripristinata logica FIFO stabile della coda caller (baseline v1.3.8).
- lock partner attivo rafforzato durante QSO in corso.
- flusso deferred retry RR73/73 unificato su FT2/FT8/FT4/FST4/Q65/MSK144.
- matching partner migliorato via token payload decode normalizzati.
- Correttezza logging/signoff:
- recovery contesto deferred autolog stabilizzato dopo finestre retry.
- ridotti casi di logging prematuro/senza conferma.
- ridotti casi di logging tardivo da stato stale.
- Continuita' decode e pannello Frequenza Rx:
- estrazione payload decode resa piu robusta con marker a spaziatura variabile.
- ridotti casi in cui frame partner restavano solo in Attivita' di Banda.
- Correzioni desktop/runtime:
- corretto toggle `Vista -> World Map` su macOS.
- migliorata gestione layout splitter per pannelli decode secondario/mappa.
- su Linux le tab impostazioni scorrono su schermi piccoli/HiDPI (OK sempre raggiungibile).
- aggiunto refresh audio in avvio su macOS per periferiche gia' selezionate.
- Dashboard remota/web app:
- aggiunto supporto completo comando `set_tx_frequency`.
- aggiunti set combinato Rx+Tx, set Rx/Tx separati, preset frequenze per modo.
- hint password accesso resta esplicito (minimo 12 caratteri, localizzato IT/EN).

Asset release:

- `decodium3-ft2-v1.4.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64.dmg` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64.zip` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, se generato)*
- `decodium3-ft2-v1.4.6-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.6-linux-x86_64.AppImage.sha256.txt`

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

Resumen (`v1.4.5 -> v1.4.6`):

- Hardening de fiabilidad AutoCQ/QSO:
- restaurada logica FIFO estable de cola callers (baseline v1.3.8).
- lock de pareja activa reforzado durante QSO en curso.
- flujo deferred de reintentos RR73/73 unificado en FT2/FT8/FT4/FST4/Q65/MSK144.
- matching de pareja mejorado mediante tokens payload decode normalizados.
- Correccion logging/signoff:
- recovery de contexto deferred autolog estabilizado tras ventanas de reintento.
- reducidos casos de log prematuro/sin confirmacion.
- reducidos casos de log tardio por estado stale.
- Continuidad decode y panel Frecuencia Rx:
- extraccion payload decode mas robusta para espaciado variable de marcador.
- reducidos casos donde tramas validas de pareja quedaban solo en Actividad de Banda.
- Correcciones desktop/runtime:
- corregido toggle `Vista -> World Map` en macOS.
- mejorada gestion layout splitter para paneles decode secundario/mapa.
- en Linux, pestañas de ajustes con scroll en pantallas pequenas/HiDPI (OK accesible).
- agregado refresh audio al inicio en macOS para dispositivos ya seleccionados.
- Dashboard remota/web app:
- agregado soporte completo para comando `set_tx_frequency`.
- agregados set combinado Rx+Tx, set Rx/Tx separados y presets por modo.
- hint de password de acceso sigue explicito (minimo 12 caracteres, localizado IT/EN).

Artefactos release:

- `decodium3-ft2-v1.4.6-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.6-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.6-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.6-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.6-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.6-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.6-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.6-linux-x86_64.AppImage.sha256.txt`

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
