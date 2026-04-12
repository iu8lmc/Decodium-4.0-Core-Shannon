# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.0

Date: 2026-03-19  
Scope: update cycle from `1.4.9` to `1.5.0`.

## English

### Summary

`1.5.0` focuses on startup audio recovery, FT8/FT4/FT2 end-of-QSO correctness, AutoCQ stability under real on-air conditions, FT2 Quick QSO evolution, upstream decoder/watchdog sync, and the first Decodium certificate/tooling layer.

### Detailed Changes (`1.4.9 -> 1.5.0`)

- Audio startup recovery:
- added RX-audio health-check logic so Decodium can detect a silent startup even when the selected devices are already correct.
- when RX remains silent after startup, the app can now reopen the configured audio streams and re-arm monitor state automatically instead of waiting for a manual "Audio Preferences -> OK" cycle.
- QSO sequencing and final-message correctness:
- fixed FT8, FT4, and FT2 standard 5-message flows so the local station still sends its final `73` when the remote side sends `RR73` or `73` first.
- fixed several FT2 Quick QSO end paths that could miss the final log, skip log-after-own-73, or return to CQ with stale DX state.
- live map direction lines and DX identity state are now cleared correctly when the QSO is completed and the app returns to CQ.
- AutoCQ and caller management:
- recently worked stations are now blocked consistently across all caller-selection paths, preventing immediate duplicate rework of the same station.
- queue handoff now clears retry counters, reports, and stale QSO state before starting the next caller.
- FT8 retry counting now increments only on real missed periods, not on repeated decode passes inside the same period.
- richer `debug.txt` tracing now records partner selection, queue starts, queue handoff, retry progress, and recent-work skips.
- FT2 decoder and upstream sync:
- imported the upstream watchdog-rescue fix so a valid reply is not dropped when TX watchdog shutdown collides with a good response.
- added `lib/ft2/decode174_91_ft2.f90` and routed FT2 triggered decode to the dedicated FT2 LDPC decoder path.
- aligned shared LDPC decoders to the upstream Normalized Min-Sum decoder refresh.
- FT2 Quick QSO / message policies:
- preserved the current `2 msg / 3 msg / 5 msg` selector and added a `Quick QSO` button as a UI alias for FT2 `2 msg`.
- aligned FT2 Quick QSO to the requested short schema, including mixed-mode handling and backward compatibility for older `TU` signalling.
- Certificates/tooling:
- added Decodium certificate load/autoload support, data-dir lookup, and status display in the main window.
- added `tools/generate_cert.py` to generate `.decodium` certificate files compatible with the in-app parser.
- Release/process alignment:
- local UI version, GitHub workflow defaults, asset naming, and release documents are now aligned to semantic version `1.5.0`.

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
- network access recommended for NTP, DX Cluster, and online features

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

`1.5.0` e' focalizzata su recovery audio all'avvio, correttezza di fine QSO FT8/FT4/FT2, stabilita' AutoCQ in condizioni reali on-air, evoluzione Quick QSO FT2, sync decoder/watchdog da upstream e primo layer di tooling/certificati Decodium.

### Modifiche Dettagliate (`1.4.9 -> 1.5.0`)

- Recovery audio all'avvio:
- aggiunta logica di health-check RX audio per rilevare uno startup muto anche quando le periferiche salvate sono gia' corrette.
- quando l'RX resta silente dopo l'avvio, l'app puo' ora riaprire automaticamente gli stream audio configurati e riarmare il monitor invece di aspettare il ciclo manuale "Preferenze Audio -> OK".
- Sequenziamento QSO e correttezza messaggi finali:
- corretti i flussi standard a 5 messaggi FT8, FT4 e FT2 cosi' che la stazione locale mandi comunque il proprio `73` finale quando il corrispondente manda prima `RR73` o `73`.
- corretti vari path finali di FT2 Quick QSO che potevano perdere il log, saltare il log-dopo-proprio-73 o tornare a CQ con stato DX stantio.
- linee direzionali della mappa live e stato identita' DX vengono ora puliti correttamente quando il QSO e' completato e l'app torna a CQ.
- AutoCQ e gestione caller:
- i nominativi appena lavorati vengono ora bloccati coerentemente su tutti i path di selezione caller, evitando doppi richiami immediati dello stesso DX.
- l'handoff della queue azzera ora contatori retry, report e stato QSO sporco prima di iniziare il caller successivo.
- in FT8 il conteggio retry incrementa ora solo sui periodi realmente persi, non sui passaggi decode ripetuti dentro lo stesso periodo.
- `debug.txt` registra ora in modo piu' ricco selezione partner, start queue, handoff queue, progress retry e skip recent-work.
- Decoder FT2 e sync upstream:
- importato il fix upstream watchdog-rescue cosi' una reply valida non viene persa quando lo shutdown watchdog TX collide con una buona risposta.
- aggiunto `lib/ft2/decode174_91_ft2.f90` e instradato il triggered decode FT2 sul path decoder LDPC dedicato FT2.
- allineati i decoder LDPC condivisi al refresh upstream Normalized Min-Sum.
- FT2 Quick QSO / policy messaggi:
- mantenuto il selettore corrente `2 msg / 3 msg / 5 msg` e aggiunto il bottone `Quick QSO` come alias UI del profilo FT2 `2 msg`.
- allineato FT2 Quick QSO allo schema corto richiesto, inclusa gestione mixed-mode e backward compatibility per il signalling `TU` piu' vecchio.
- Certificati/tooling:
- aggiunto supporto load/autoload certificati Decodium, lookup nella data dir e display stato nella finestra principale.
- aggiunto `tools/generate_cert.py` per generare file certificato `.decodium` compatibili con il parser interno.
- Allineamento release/processo:
- versione UI locale, default workflow GitHub, naming asset e documenti release sono ora allineati alla semver `1.5.0`.

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
- Spazio disco: almeno 500 MB liberi
- Display: 1280x800 o superiore consigliato
- Hardware stazione: CAT/audio/PTT secondo il setup radio

Software:

- Linux con `glibc >= 2.35`
- supporto `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire
- ambiente desktop capace di eseguire AppImage Qt5
- accesso rete consigliato per NTP, DX Cluster e feature online

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Avvio consigliato AppImage su Linux

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

`1.5.0` se centra en recuperacion audio al arranque, correccion de fin de QSO FT8/FT4/FT2, estabilidad AutoCQ en condiciones reales on-air, evolucion Quick QSO FT2, sync decoder/watchdog desde upstream y primera capa de tooling/certificados Decodium.

### Cambios Detallados (`1.4.9 -> 1.5.0`)

- Recuperacion audio al arranque:
- anadida logica de health-check RX audio para detectar un arranque mudo incluso cuando los dispositivos guardados ya son correctos.
- cuando RX sigue silencioso tras el arranque, la app puede ahora reabrir automaticamente los streams audio configurados y rearmar el monitor en vez de esperar el ciclo manual "Preferencias Audio -> OK".
- Secuenciacion QSO y correccion de mensajes finales:
- corregidos los flujos estandar de 5 mensajes FT8, FT4 y FT2 para que la estacion local envie igualmente su `73` final cuando la otra estacion manda antes `RR73` o `73`.
- corregidos varios paths finales de FT2 Quick QSO que podian perder el log, saltarse el log-despues-del-propio-73 o volver a CQ con estado DX obsoleto.
- las lineas direccionales del mapa live y el estado de identidad DX se limpian ahora correctamente cuando el QSO termina y la app vuelve a CQ.
- AutoCQ y gestion caller:
- los callsigns recien trabajados se bloquean ahora de forma coherente en todos los paths de seleccion caller, evitando retrabajos inmediatos del mismo DX.
- el handoff de la cola limpia ahora contadores retry, reportes y estado QSO sucio antes de arrancar el siguiente caller.
- en FT8 el conteo retry incrementa ahora solo en periodos realmente perdidos, no en pasadas decode repetidas dentro del mismo periodo.
- `debug.txt` registra ahora con mas detalle seleccion partner, inicio cola, handoff cola, progreso retry y saltos recent-work.
- Decoder FT2 y sync upstream:
- importado el fix upstream watchdog-rescue para que una reply valida no se pierda cuando el apagado watchdog TX colisiona con una buena respuesta.
- anadido `lib/ft2/decode174_91_ft2.f90` y encaminado el triggered decode FT2 al path decoder LDPC dedicado FT2.
- alineados los decoders LDPC compartidos al refresh upstream Normalized Min-Sum.
- FT2 Quick QSO / politicas de mensajes:
- mantenido el selector actual `2 msg / 3 msg / 5 msg` y anadido el boton `Quick QSO` como alias UI del perfil FT2 `2 msg`.
- alineado FT2 Quick QSO al esquema corto solicitado, incluida gestion mixed-mode y backward compatibility para el signalling `TU` antiguo.
- Certificados/tooling:
- anadido soporte load/autoload de certificados Decodium, lookup en data dir y display de estado en la ventana principal.
- anadido `tools/generate_cert.py` para generar ficheros `.decodium` compatibles con el parser interno.
- Alineacion release/proceso:
- version UI local, defaults workflow GitHub, naming de assets y documentos release estan ahora alineados a la semver `1.5.0`.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

### Requisitos Minimos Linux

Hardware:

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio: al menos 500 MB libres
- Pantalla: 1280x800 o superior recomendada
- Hardware de estacion: CAT/audio/PTT segun el setup de radio

Software:

- Linux con `glibc >= 2.35`
- soporte `libfuse2` / FUSE2
- stack audio ALSA, PulseAudio o PipeWire
- entorno de escritorio capaz de ejecutar AppImage Qt5
- acceso a red recomendado para NTP, DX Cluster y funciones online

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Recomendacion AppImage Linux

Para evitar problemas debidos al sistema de archivos de solo lectura de las AppImage, se recomienda iniciar Decodium extrayendo primero la AppImage y ejecutando despues el programa desde la carpeta extraida.

Ejecutar los siguientes comandos en la terminal:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
