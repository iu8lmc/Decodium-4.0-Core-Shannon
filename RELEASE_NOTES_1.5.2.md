# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.2

Date: 2026-03-20  
Scope: update cycle from `1.5.1` to `1.5.2`.

## English

### Summary

`1.5.2` focuses on operational reliability and release alignment after `1.5.1`: FT2 decoder refresh, FT2 short-QSO maturity, FT4/FT8 Wait Features protection, startup audio recovery, web-app parity, translation completion, and a stricter release/version pipeline.

### Detailed Changes (`1.5.1 -> 1.5.2`)

- FT2 decoder and protocol:
- aligned FT2 to the dedicated upstream FT2 LDPC decoder path.
- refreshed FT2 bitmetrics support from upstream.
- completed the FT2 `Quick QSO` / `2 msg / 3 msg / 5 msg` flow, including current TU mixed-mode handling.
- hid FT2 async/triggered decoder tags (`T`, `aN`) from decode panes and collapsed async/sync duplicates into one logical decode.
- QSO sequencing / AutoCQ / Wait Features:
- fixed multiple late-signoff, late-final-ack, retry, stale-partner, and direct-reply paths across FT2/FT4/FT8 so completed QSOs log correctly.
- restored the FT4/FT8 `Wait Features + AutoSeq` active-QSO lock, preventing queued calling logic from interrupting a running contact.
- fixed stale callsign/report reuse when AutoCQ leaves CQ for a direct caller or hands off between queued callers.
- Audio and UI:
- startup and wake RX-audio recovery is now tied to the real monitor-on transition, with automatic reopen if audio input stays silent.
- completed the bundled UI translations so menu, popup, and in-program strings no longer ship with unfinished gaps.
- extended the web app/dashboard with `Manual TX`, `Speedy`, `D-CW`, async status, `Quick QSO`, and FT2 `2/3/5 msg` controls.
- Tooling / updater / release:
- added Decodium certificate load/autoload/status plus `tools/generate_cert.py`.
- kept the internal updater and refined best-match asset selection for macOS/Linux downloads.
- centralised release versioning through `fork_release_version.txt` so local builds, workflows, and GitHub releases remain aligned.
- Identity / visibility:
- aligned the PSK Reporter identifier to the exact application title string.
- fixed stale/fake live-map paths while returning to CQ or transmitting plain CQ.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Linux Minimum Requirements

Hardware:

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Display: 1280x800 or better recommended
- Station hardware: CAT/audio/PTT equipment according to radio setup

Software:

- Linux with `glibc >= 2.35`
- `libfuse2` / FUSE2 support
- ALSA, PulseAudio, or PipeWire audio stack
- desktop environment capable of running Qt5 AppImages
- network access recommended for NTP, DX Cluster, PSK Reporter, updater, and online features

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Linux AppImage recommendation

To avoid issues caused by the AppImage read-only filesystem, it is recommended to start Decodium by extracting the AppImage first and then running the program from the extracted directory.

Run the following commands in a terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

### Sintesi

`1.5.2` e' focalizzata su affidabilita' operativa e allineamento release dopo `1.5.1`: refresh decoder FT2, maturita' del QSO corto FT2, protezione Wait Features in FT4/FT8, recovery audio all'avvio, parita' web app, completamento traduzioni e pipeline versione/release piu' rigida.

### Modifiche Dettagliate (`1.5.1 -> 1.5.2`)

- Decoder FT2 e protocollo:
- allineato FT2 al decoder LDPC FT2 dedicato upstream.
- aggiornato il supporto FT2 bitmetrics da upstream.
- completato il flow FT2 `Quick QSO` / `2 msg / 3 msg / 5 msg`, inclusa la gestione TU mixed-mode corrente.
- nascosti dalle decode panes i tag decoder FT2 async/triggered (`T`, `aN`) e collassati i duplicati async/sync in un solo decode logico.
- Sequenziamento QSO / AutoCQ / Wait Features:
- corretti molteplici path di late-signoff, late-final-ack, retry, partner stantio e direct-reply in FT2/FT4/FT8, cosi' i QSO completati vanno a log correttamente.
- ripristinato in FT4/FT8 il lock QSO attivo `Wait Features + AutoSeq`, impedendo alla logica di chiamata in coda di interrompere un collegamento in corso.
- corretti riuso di callsign/report stantii quando AutoCQ esce dal CQ verso un caller diretto o passa da un caller in coda al successivo.
- Audio e UI:
- il recovery RX-audio all'avvio e al wake e' ora agganciato alla reale transizione monitor-on, con reopen automatico se l'ingresso audio resta muto.
- completate le traduzioni UI bundle, cosi' menu, popup e stringhe interne non escono piu' con buchi `unfinished`.
- estesa la web app/dashboard con `Manual TX`, `Speedy`, `D-CW`, stato async, `Quick QSO` e controlli FT2 `2/3/5 msg`.
- Tooling / updater / release:
- aggiunti load/autoload/stato certificati Decodium piu' `tools/generate_cert.py`.
- mantenuto l'updater interno e raffinata la selezione dell'asset macOS/Linux piu' adatto.
- centralizzato il versioning release tramite `fork_release_version.txt`, cosi' build locali, workflow e release GitHub restano allineati.
- Identita' / visibilita':
- allineato l'identificativo PSK Reporter esattamente alla stringa del titolo applicazione.
- corretti percorsi live-map stantii o falsi durante ritorno a CQ o CQ puro.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisiti Minimi Linux

Hardware:

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Storage: almeno 500 MB liberi
- Display: consigliato 1280x800 o superiore
- Hardware stazione: CAT/audio/PTT secondo il proprio setup radio

Software:

- Linux con `glibc >= 2.35`
- supporto `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire
- ambiente desktop capace di eseguire AppImage Qt5
- accesso rete consigliato per NTP, DX Cluster, PSK Reporter, updater e funzioni online

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Raccomandazione Linux AppImage

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

`1.5.2` se centra en fiabilidad operativa y alineacion release tras `1.5.1`: refresh decoder FT2, madurez del QSO corto FT2, proteccion Wait Features en FT4/FT8, recuperacion de audio al arranque, paridad web app, traducciones completas y una pipeline version/release mas estricta.

### Cambios Detallados (`1.5.1 -> 1.5.2`)

- Decoder FT2 y protocolo:
- alineado FT2 al decoder LDPC FT2 dedicado upstream.
- actualizado el soporte FT2 bitmetrics desde upstream.
- completado el flow FT2 `Quick QSO` / `2 msg / 3 msg / 5 msg`, incluida la gestion TU mixed-mode actual.
- ocultados en las decode panes los tags decoder FT2 async/triggered (`T`, `aN`) y colapsados los duplicados async/sync en un solo decode logico.
- Secuenciacion QSO / AutoCQ / Wait Features:
- corregidos multiples paths de late-signoff, late-final-ack, retry, partner obsoleto y direct-reply en FT2/FT4/FT8 para que los QSO completados entren a log correctamente.
- restaurado en FT4/FT8 el lock QSO activo `Wait Features + AutoSeq`, evitando que la logica de llamada en cola interrumpa un contacto en curso.
- corregidos reusos de callsign/report obsoletos cuando AutoCQ sale de CQ hacia un caller directo o pasa de un caller en cola al siguiente.
- Audio y UI:
- la recuperacion RX-audio al arranque y al wake queda ahora ligada a la transicion real monitor-on, con reopen automatico si la entrada de audio sigue muda.
- completadas las traducciones UI bundle para que menus, popup y cadenas internas ya no salgan con huecos `unfinished`.
- ampliada la web app/dashboard con `Manual TX`, `Speedy`, `D-CW`, estado async, `Quick QSO` y controles FT2 `2/3/5 msg`.
- Tooling / updater / release:
- anadidos load/autoload/estado de certificados Decodium mas `tools/generate_cert.py`.
- mantenido el updater interno y refinada la seleccion del asset macOS/Linux mas adecuado.
- centralizado el versionado release mediante `fork_release_version.txt` para que builds locales, workflows y releases GitHub permanezcan alineados.
- Identidad / visibilidad:
- alineado el identificador PSK Reporter exactamente con la cadena del titulo de la aplicacion.
- corregidos recorridos live-map obsoletos o falsos durante vuelta a CQ o CQ puro.

### Artefactos Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisitos Minimos Linux

Hardware:

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: minimo 4 GB (8 GB recomendados)
- Storage: al menos 500 MB libres
- Display: recomendado 1280x800 o superior
- Hardware de estacion: CAT/audio/PTT segun el setup de radio

Software:

- Linux con `glibc >= 2.35`
- soporte `libfuse2` / FUSE2
- pila de audio ALSA, PulseAudio o PipeWire
- entorno desktop capaz de ejecutar AppImage Qt5
- acceso de red recomendado para NTP, DX Cluster, PSK Reporter, updater y funciones online

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion Linux AppImage

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
