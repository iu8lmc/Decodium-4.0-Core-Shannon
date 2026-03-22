# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.4

Date: 2026-03-22  
Scope: update cycle from `1.5.3` to `1.5.4`.

## English

### Summary

`1.5.4` is a feature-and-hardening release focused on decoder quality, web-app parity, UI/language alignment, secure storage and downloader guardrails, macOS bundle correctness, and release/test coverage.

### Detailed Changes (`1.5.3 -> 1.5.4`)

- Decoders / weak-signal handling:
- added an FT2 anti-ghost filter for very weak decodes. It ignores known non-callsign tokens, strips trailing async markers, rejects malformed weak payloads, and traces decisions with `ghostPass` / `ghostFilt` in `debug.txt`.
- adopted FT2 `best 3 of 4 Costas sync` with retuned thresholds in both the main FT2 decoder and the FT2 triggered decoder.
- adopted FT4 `best 3 of 4 Costas sync`, a fourth subtraction pass for deeper decoding, adaptive `syncmin` on later passes, and relaxed sync quality tuned for recovered weak signals.
- adopted FT8 `best 2 of 3 Costas sync` in both the fixed and variable decoders, plus a fourth subtraction pass and adaptive `syncmin` for deep weak-signal recovery.
- Web app / remote console:
- added a real `Monitoring ON/OFF` remote control instead of exposing monitoring as read-only state.
- added the FT2 `ASYNC` dB value card to the web app so remote users see the same SNR indicator shown on the desktop UI.
- added `Hide CQ` and `Hide 73` filters for the RX/TX activity table to clean up monitoring views.
- the web app now follows the language selected in the desktop app instead of the browser language, and now covers all bundled UI languages.
- Desktop / UI:
- removed the duplicate `English (UK)` menu entry and kept a single `English` entry in the language menu.
- reformatted the UTC date/time display and Astro panel to `Day Month Year` plus `HH:mm:ss UTC`, using the month name from the language selected inside Decodium.
- user-facing title/version text keeps `Decodium` and removes the old `v3.0 FT2 "Raptor"` branding from the local program title.
- Security / stability:
- extracted secure settings handling to a dedicated `SecureSettings` module for macOS Keychain / Linux `secret-tool` access.
- hardened secure fallback behaviour: clearer one-shot warnings, better plaintext-to-secure placeholder migration, and safer handling when the secure backend is unavailable or partially failing.
- hardened `FileDownload`: supported schemes only, redirect validation, TLS error aborts, timeout, `64 MiB` maximum payload, and explicit oversize/redirect test coverage.
- reduced blind DXLab startup sleeps by polling for the frequency settle window instead of waiting fixed long sleeps.
- improved CAT/transceiver exception logging so failures are no longer silently swallowed.
- changed the default LoTW activity URL from `http` to `https`.
- corrected macOS app packaging: sounds now live under `Contents/Resources/sounds`, Hamlib helper binaries are bundled as real files instead of Homebrew symlinks, Mach-O references are normalized to `@rpath`, and stale legacy bundle artifacts are cleaned from reused build trees.
- Tests / release process:
- added RFC `4226` / `6238` validation tests for `HOTP/TOTP`, covering `SHA1`, `SHA256`, and `SHA512`.
- added dedicated tests for secure settings fallback/migration and downloader redirect/oversize behaviour.
- fixed Linux test-linking for `GNU ld` by linking `wsjt_qt` before `wsjt_cxx` where required.
- aligned local version metadata, release docs, and workflow defaults to `1.5.4`.

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

`1.5.4` e' una release di feature e hardening focalizzata su qualita' decoder, parita' funzionale della web app, allineamento UI/lingue, protezione secure storage/downloader, correttezza del bundle macOS e copertura test/release.

### Modifiche Dettagliate (`1.5.3 -> 1.5.4`)

- Decoder / weak-signal:
- aggiunto un filtro FT2 anti-ghost per decode molto deboli. Ignora token noti che non sono callsign, rimuove i marker async finali, scarta payload deboli malformati e traccia le decisioni con `ghostPass` / `ghostFilt` in `debug.txt`.
- adottato `best 3 of 4 Costas sync` in FT2 con soglie riscalate sia nel decoder principale sia nel decoder FT2 triggered.
- adottato `best 3 of 4 Costas sync` in FT4, con quarto pass di sottrazione per deep decode, `syncmin` adattivo nei pass successivi e `sync_qual` rilassato per i segnali recuperati.
- adottato `best 2 of 3 Costas sync` in FT8 nei decoder fisso e variabile, con quarto pass di sottrazione e `syncmin` adattivo per recovery weak-signal.
- Web app / console remota:
- aggiunto un vero controllo remoto `Monitoring ON/OFF` invece di esporre il monitoraggio come solo stato in lettura.
- aggiunta alla web app la card FT2 `ASYNC` con il valore in dB, come sulla UI desktop.
- aggiunti i filtri `Hide CQ` e `Hide 73` nella tabella attivita' RX/TX per pulire le viste di monitoraggio.
- la web app segue ora la lingua scelta nell'app desktop invece della lingua del browser, e copre tutte le lingue UI bundle.
- Desktop / UI:
- rimossa la voce duplicata `English (UK)` dal menu lingue, lasciando una sola voce `English`.
- riformattato l'orologio UTC e il pannello Astro in `Giorno Mese Anno` piu' `HH:mm:ss UTC`, usando il nome mese della lingua selezionata in Decodium.
- il titolo/versioning locale mantiene `Decodium` e rimuove il vecchio branding `v3.0 FT2 "Raptor"` dal titolo programma.
- Sicurezza / stabilita':
- estratta la gestione secure settings in un modulo dedicato `SecureSettings` per macOS Keychain / Linux `secret-tool`.
- irrigidito il fallback secure: warning one-shot piu' chiari, migrazione migliore da plaintext a placeholder `__secure__`, e gestione piu' sicura dei casi in cui il backend protetto e' assente o fallisce parzialmente.
- irrigidito `FileDownload`: solo scheme supportati, validazione redirect, abort su errori TLS, timeout, limite massimo `64 MiB`, e copertura test esplicita per oversize/redirect.
- ridotti i blind sleep di startup DXLab, sostituendoli con polling breve sulla finestra di assestamento frequenza.
- migliorato il logging delle eccezioni CAT/transceiver, che non vengono piu' inghiottite in silenzio.
- cambiata la URL di default dell'attivita' LoTW da `http` a `https`.
- corretto il packaging dell'app macOS: i suoni sono ora in `Contents/Resources/sounds`, gli helper Hamlib sono inclusi come file reali e non symlink Homebrew, i riferimenti Mach-O sono normalizzati a `@rpath`, e i residui legacy del bundle vengono rimossi anche nelle build riutilizzate.
- Test / processo release:
- aggiunti test RFC `4226` / `6238` per `HOTP/TOTP`, coprendo `SHA1`, `SHA256` e `SHA512`.
- aggiunti test dedicati per fallback/migrazione secure settings e per redirect/oversize del downloader.
- corretto il linking test su Linux con `GNU ld`, linkando `wsjt_qt` prima di `wsjt_cxx` dove richiesto.
- allineati a `1.5.4` i metadati versione locali, la documentazione release e i default dei workflow.

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

`1.5.4` es una release de funciones y hardening centrada en calidad de decoder, paridad funcional de la web app, alineacion UI/idiomas, proteccion de secure storage/downloader, correccion del bundle macOS y cobertura de test/release.

### Cambios Detallados (`1.5.3 -> 1.5.4`)

- Decoders / weak-signal:
- anadido un filtro FT2 anti-ghost para decodes muy debiles. Ignora tokens conocidos que no son callsigns, elimina marcadores async finales, descarta payloads debiles malformados y deja trazas `ghostPass` / `ghostFilt` en `debug.txt`.
- adoptado `best 3 of 4 Costas sync` en FT2 con umbrales reajustados tanto en el decoder principal como en el decoder FT2 triggered.
- adoptado `best 3 of 4 Costas sync` en FT4, con cuarto pase de sustraccion para deep decode, `syncmin` adaptativo en pases posteriores y `sync_qual` relajado para senales recuperadas.
- adoptado `best 2 of 3 Costas sync` en FT8 en los decoders fijo y variable, con cuarto pase de sustraccion y `syncmin` adaptativo para recovery weak-signal.
- Web app / consola remota:
- anadido un control remoto real `Monitoring ON/OFF` en vez de exponer el monitoreo solo como estado de lectura.
- anadida a la web app la tarjeta FT2 `ASYNC` con el valor en dB, igual que en la UI de escritorio.
- anadidos los filtros `Hide CQ` y `Hide 73` en la tabla de actividad RX/TX para limpiar vistas de monitoreo.
- la web app sigue ahora el idioma seleccionado en la app de escritorio en lugar del idioma del navegador, y cubre todas las lenguas UI bundle.
- Desktop / UI:
- eliminada la entrada duplicada `English (UK)` del menu de idiomas, dejando una sola entrada `English`.
- reformateado el reloj UTC y el panel Astro a `Dia Mes Ano` mas `HH:mm:ss UTC`, usando el nombre del mes del idioma elegido en Decodium.
- el titulo/versioning local mantiene `Decodium` y elimina el antiguo branding `v3.0 FT2 "Raptor"` del titulo del programa.
- Seguridad / estabilidad:
- extraida la gestion de secure settings a un modulo dedicado `SecureSettings` para macOS Keychain / Linux `secret-tool`.
- endurecido el fallback secure: warnings one-shot mas claros, mejor migracion de plaintext a placeholder `__secure__`, y manejo mas seguro cuando el backend protegido no existe o falla parcialmente.
- endurecido `FileDownload`: solo schemes soportados, validacion de redirects, abort ante errores TLS, timeout, limite maximo `64 MiB`, y cobertura de test explicita para oversize/redirect.
- reducidos los blind sleeps de arranque DXLab, sustituyendolos por polling corto de la ventana de estabilizacion de frecuencia.
- mejorado el logging de excepciones CAT/transceiver, que ya no quedan tragadas en silencio.
- cambiada la URL por defecto de la actividad LoTW de `http` a `https`.
- corregido el empaquetado de la app macOS: los sonidos viven ahora en `Contents/Resources/sounds`, los helpers de Hamlib se incluyen como archivos reales y no como symlinks de Homebrew, las referencias Mach-O se normalizan a `@rpath`, y los residuos legacy del bundle se eliminan tambien en arboles de build reutilizados.
- Test / proceso release:
- anadidos tests RFC `4226` / `6238` para `HOTP/TOTP`, cubriendo `SHA1`, `SHA256` y `SHA512`.
- anadidos tests dedicados para fallback/migracion de secure settings y para redirect/oversize del downloader.
- corregido el enlazado de tests en Linux con `GNU ld`, enlazando `wsjt_qt` antes de `wsjt_cxx` donde se requiere.
- alineados a `1.5.4` los metadatos de version locales, la documentacion release y los defaults de workflow.

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
- Hardware de estacion: CAT/audio/PTT segun el propio setup de radio

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
