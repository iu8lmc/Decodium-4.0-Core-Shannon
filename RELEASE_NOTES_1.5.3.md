# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.3

Date: 2026-03-22  
Scope: update cycle from `1.5.2` to `1.5.3`.

## English

### Summary

`1.5.3` is a focused maintenance release: Linux `CQRLOG wsjtx remote` compatibility, FT4/FT8 `Wait Features + AutoSeq` behaviour, local version propagation, and proper bundled German/French translations.

### Detailed Changes (`1.5.2 -> 1.5.3`)

- Linux / CQRLOG:
- restored practical `CQRLOG wsjtx remote` interoperability by keeping the historical UDP listen-port behaviour and using `WSJTX` as the compatibility client id.
- kept `UDP Listen Port = 0` as the supported historical/default mode for external software that relies on heartbeat discovery.
- FT4 / FT8 sequencing:
- tightened `Wait Features + AutoSeq` so a busy-slot or late-partner reply pauses the current TX cycle instead of transmitting over an already active QSO.
- local build/version pipeline:
- fixed local rebuilds so changing `fork_release_version.txt` propagates correctly into the compiled app version after configure/build.
- release defaults and workflow defaults are aligned to `1.5.3`, including the experimental Hamlib workflow default string.
- Translations / UI:
- added bundled German (`de`) and French (`fr`) UI translations.
- language selection now lists only translations that are actually bundled.
- saved unsupported language codes now fall back cleanly to English instead of silently restarting in English with dead menu entries.
- Tooling:
- added `tools/generate_ts_translations.py` to regenerate missing TS bundles from the English base when needed.

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

### Linux Local Build Reminder

The AppImage already bundles the required Qt multimedia runtime. For local Ubuntu/Debian source builds, also install:

```bash
sudo apt update
sudo apt install \
  qtmultimedia5-dev \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5multimediawidgets5 \
  libqt5multimediagsttools5 \
  libpulse-mainloop-glib0 \
  pulseaudio-utils \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

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

`1.5.3` e' una release di manutenzione mirata: compatibilita' Linux con `CQRLOG wsjtx remote`, comportamento `Wait Features + AutoSeq` in FT4/FT8, propagazione versione locale e traduzioni bundle corrette per tedesco/francese.

### Modifiche Dettagliate (`1.5.2 -> 1.5.3`)

- Linux / CQRLOG:
- ripristinata l'interoperabilita' pratica con `CQRLOG wsjtx remote`, mantenendo il comportamento storico della listen port UDP e usando `WSJTX` come client id di compatibilita'.
- confermato `UDP Listen Port = 0` come modalita' storica/supportata per software esterni che si appoggiano all'heartbeat discovery.
- Sequenziamento FT4 / FT8:
- irrigidito `Wait Features + AutoSeq`, cosi' una risposta tardiva o uno slot occupato mettono in pausa il ciclo TX corrente invece di trasmettere sopra un QSO gia' attivo.
- Pipeline build/versione locale:
- corrette le rebuild locali: cambiando `fork_release_version.txt`, la versione si propaga correttamente nel binario compilato dopo configure/build.
- allineati a `1.5.3` i default release e i default workflow, incluso il default del workflow Hamlib sperimentale.
- Traduzioni / UI:
- aggiunte le traduzioni UI bundle in tedesco (`de`) e francese (`fr`).
- la selezione lingua mostra ora solo traduzioni realmente bundle.
- eventuali codici lingua salvati ma non supportati fanno ora fallback pulito a inglese, senza voci menu morte.
- Tooling:
- aggiunto `tools/generate_ts_translations.py` per rigenerare i bundle TS mancanti a partire dalla base inglese.

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

### Promemoria Build Locale Linux

L'AppImage include gia' il runtime Qt multimedia richiesto. Per le build locali Ubuntu/Debian da sorgente, installare anche:

```bash
sudo apt update
sudo apt install \
  qtmultimedia5-dev \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5multimediawidgets5 \
  libqt5multimediagsttools5 \
  libpulse-mainloop-glib0 \
  pulseaudio-utils \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

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

`1.5.3` es una release de mantenimiento enfocada: compatibilidad Linux con `CQRLOG wsjtx remote`, comportamiento `Wait Features + AutoSeq` en FT4/FT8, propagacion de version local y traducciones bundle correctas para aleman/frances.

### Cambios Detallados (`1.5.2 -> 1.5.3`)

- Linux / CQRLOG:
- restaurada la interoperabilidad practica con `CQRLOG wsjtx remote`, manteniendo el comportamiento historico de la listen port UDP y usando `WSJTX` como client id de compatibilidad.
- mantenido `UDP Listen Port = 0` como modo historico/soportado para software externo que depende del heartbeat discovery.
- Secuenciacion FT4 / FT8:
- endurecido `Wait Features + AutoSeq`, para que una respuesta tardia o un slot ocupado pausen el ciclo TX actual en lugar de transmitir sobre un QSO ya activo.
- Pipeline local build/version:
- corregidas las rebuilds locales: al cambiar `fork_release_version.txt`, la version se propaga correctamente al binario compilado tras configure/build.
- alineados a `1.5.3` los defaults release y workflow, incluido el default del workflow Hamlib experimental.
- Traducciones / UI:
- anadidas las traducciones UI bundle en aleman (`de`) y frances (`fr`).
- la seleccion de idioma muestra ahora solo traducciones realmente bundle.
- los codigos de idioma guardados pero no soportados vuelven ahora limpiamente a ingles, sin entradas de menu muertas.
- Tooling:
- anadido `tools/generate_ts_translations.py` para regenerar bundles TS ausentes a partir de la base inglesa.

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

### Recordatorio Build Local Linux

La AppImage ya incluye el runtime Qt multimedia requerido. Para builds locales Ubuntu/Debian desde codigo fuente, instalar tambien:

```bash
sudo apt update
sudo apt install \
  qtmultimedia5-dev \
  libqt5multimedia5 \
  libqt5multimedia5-plugins \
  libqt5multimediawidgets5 \
  libqt5multimediagsttools5 \
  libpulse-mainloop-glib0 \
  pulseaudio-utils \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good
```

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
