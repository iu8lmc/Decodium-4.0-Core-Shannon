# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.6

Scope: update cycle from `1.5.5` to `1.5.6`.

## English

`1.5.6` is the promoted native C++ runtime release for FT8, FT4, FT2, and Q65. It completes the active migration away from mode-specific Fortran orchestration for those modes, expands the in-process worker architecture, hardens transmit/build behavior, and keeps the macOS packaging layout already validated by the previous successful deploy.

### Detailed Changes (`1.5.5 -> 1.5.6`)

- completed the promoted FT8 C++ stage-4 runtime path, including native search/sync/bookkeeping/LDPC/downsample/subtract helper coverage on the active decode path.
- completed the promoted FT4 runtime path so the active decode/TX logic no longer depends on FT4-specific Fortran.
- completed the promoted FT2 runtime path in native C++, with `Detector/FtxFt2Stage7.cpp` owning active decode orchestration and the old `lib/ft2/*.f90` decoder sources removed from the active tree.
- completed the promoted Q65 runtime path in native C++, including decoder orchestration, frontend work, and removal of old Q65-specific Fortran frontend code from the active build/tree.
- promoted native utilities and frontends for `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65`, and `rtty_spec`.
- removed the old `jt9` shared-memory bootstrap from main-app startup for the promoted FTX worker path.
- expanded the worker-based in-process decode model for FT8, FT4, FT2, Q65, FST4, JT fast decode, MSK144, WSPR, and legacy JT worker separation.
- hardened FT2/FT4/Fox/SuperFox transmit flow with precomputed-wave snapshots, cached waveform handoff, safer lead-in timing, and richer `debug.txt` waveform tracing.
- added parity/regression tooling with `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare`, `ft2_make_test_wav`, and expanded `test_qt_helpers`.
- fixed Linux/macOS link-order and static-library cycle failures under GNU `ld` by grouping interdependent runtime libraries correctly.
- fixed GCC 15 / Qt5 / C++11 build failures in tools, tests, and bridge code, including stricter constructor handling, missing standard headers, mixed string conversions, and Fortran/common-block linkage issues.
- kept the macOS release layout and folder placement already validated in the last successful deploy, aligned with Tahoe arm64, Sequoia arm64, Sequoia x86_64, Monterey x86_64, and Linux AppImage release targets.
- kept the experimental/public RTTY user path hidden while the internal work remains outside the release surface.
- aligned local version metadata, workflow defaults, release documents, and About text to `1.5.6`.

### Release Targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Linux Minimum Requirements

Hardware:

- `x86_64`
- dual-core 2.0 GHz or better
- 4 GB RAM minimum
- 500 MB free disk space

Software:

- `glibc >= 2.35`
- `libfuse2`
- ALSA, PulseAudio, or PipeWire

### Startup Guidance

macOS quarantine workaround:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage extract-run flow:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Italiano

`1.5.6` e' la release del runtime promosso nativo C++ per FT8, FT4, FT2 e Q65. Completa la migrazione attiva lontano dall'orchestrazione Fortran specifica di quei modi, estende l'architettura in-process a worker, irrobustisce trasmissione e build, e mantiene il layout di packaging macOS gia' validato dal precedente deploy riuscito.

### Modifiche Dettagliate (`1.5.5 -> 1.5.6`)

- completato il path runtime FT8 stage-4 promosso in C++, con copertura nativa di search/sync/bookkeeping/LDPC/downsample/subtract sul decode attivo.
- completato il path runtime promosso FT4 cosi' la logica attiva decode/TX non dipende piu' da Fortran specifico FT4.
- completato il path runtime promosso FT2 in C++, con `Detector/FtxFt2Stage7.cpp` proprietario dell'orchestrazione attiva e con i vecchi decoder `lib/ft2/*.f90` rimossi dal tree attivo.
- completato il path runtime promosso Q65 in C++, inclusi decoder orchestration, frontend e rimozione del vecchio frontend Fortran specifico Q65 dal build/tree attivo.
- promossi utility e front-end nativi per `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` e `rtty_spec`.
- rimosso dall'avvio principale il vecchio bootstrap `jt9` basato su shared memory per il path worker FTX promosso.
- esteso il modello decoder in-process a worker per FT8, FT4, FT2, Q65, FST4, JT fast decode, MSK144, WSPR e separazione del worker legacy JT.
- irrobustito il flusso TX FT2/FT4/Fox/SuperFox con snapshot delle wave precompute, handoff di waveform cache, lead-in piu' sicuro e tracing waveform piu' ricco in `debug.txt`.
- aggiunti strumenti di parita'/regressione con `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare`, `ft2_make_test_wav` e un `test_qt_helpers` molto piu' ampio.
- corretti i fallimenti Linux/macOS di ordine link e cicli di librerie statiche sotto GNU `ld`, raggruppando correttamente le librerie runtime interdipendenti.
- corretti i fallimenti di build GCC 15 / Qt5 / C++11 in tool, test e bridge, inclusi costruttori piu' severi, header standard mancanti, conversioni stringa miste e problemi di linkage Fortran/common-block.
- mantenuto il layout release macOS e il posizionamento cartelle gia' validati nell'ultimo deploy riuscito, allineati ai target Tahoe arm64, Sequoia arm64, Sequoia x86_64, Monterey x86_64 e Linux AppImage.
- mantenuto nascosto il percorso utente RTTY sperimentale/pubblico, lasciando il lavoro interno fuori dalla superficie release.
- allineati a `1.5.6` metadati versione locali, default dei workflow, documenti release e testo About.

### Target Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / sperimentale)*
- Linux x86_64 AppImage

### Requisiti Minimi Linux

Hardware:

- `x86_64`
- dual-core 2.0 GHz o meglio
- minimo 4 GB RAM
- 500 MB liberi su disco

Software:

- `glibc >= 2.35`
- `libfuse2`
- ALSA, PulseAudio o PipeWire

### Guida Avvio

Workaround quarantena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per evitare problemi dovuti al filesystem in sola lettura delle AppImage, si consiglia di avviare Decodium estraendo prima l'AppImage e poi eseguendo il programma dalla cartella estratta.

Flusso Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

`1.5.6` es la release del runtime promovido nativo C++ para FT8, FT4, FT2 y Q65. Completa la migracion activa lejos de la orquestacion Fortran especifica de esos modos, amplia la arquitectura in-process con workers, endurece transmision y build, y mantiene el layout de packaging macOS ya validado por el despliegue correcto anterior.

### Cambios Detallados (`1.5.5 -> 1.5.6`)

- completado el camino FT8 stage-4 promovido en C++, con cobertura nativa de search/sync/bookkeeping/LDPC/downsample/subtract en el decode activo.
- completado el camino runtime promovido FT4, de forma que la logica activa decode/TX ya no depende de Fortran especifico FT4.
- completado el camino runtime promovido FT2 en C++, con `Detector/FtxFt2Stage7.cpp` como propietario de la orquestacion activa y con los viejos decoders `lib/ft2/*.f90` retirados del arbol activo.
- completado el camino runtime promovido Q65 en C++, incluyendo decoder orchestration, frontend y retirada del antiguo frontend Fortran especifico Q65 del build/arbol activo.
- promovidas utilidades y frontends nativos para `jt9`, `jt9a`, `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` y `rtty_spec`.
- retirado del arranque principal el viejo bootstrap `jt9` basado en shared memory para la ruta worker FTX promovida.
- ampliado el modelo decoder in-process con workers para FT8, FT4, FT2, Q65, FST4, JT fast decode, MSK144, WSPR y separacion del worker legacy JT.
- endurecido el flujo TX FT2/FT4/Fox/SuperFox con snapshots de ondas precomputadas, handoff de waveform cache, lead-in mas seguro y trazas de waveform mas ricas en `debug.txt`.
- anadidas herramientas de paridad/regresion con `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare`, `ft2_make_test_wav` y un `test_qt_helpers` mucho mas amplio.
- corregidos los fallos Linux/macOS de orden de enlace y ciclos de librerias estaticas bajo GNU `ld`, agrupando correctamente las librerias runtime interdependientes.
- corregidos los fallos de build GCC 15 / Qt5 / C++11 en herramientas, tests y bridges, incluidos constructores mas estrictos, cabeceras estandar ausentes, conversiones mixtas de cadena y problemas de linkage Fortran/common-block.
- mantenido el layout release macOS y la colocacion de carpetas ya validados en el ultimo deploy correcto, alineados con los targets Tahoe arm64, Sequoia arm64, Sequoia x86_64, Monterey x86_64 y Linux AppImage.
- mantenida oculta la ruta publica/experimental RTTY, dejando el trabajo interno fuera de la superficie release.
- alineados a `1.5.6` los metadatos de version locales, defaults de workflow, documentos release y texto About.

### Targets Release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
- Apple Intel Monterey *(best effort / experimental)*
- Linux x86_64 AppImage

### Requisitos Minimos Linux

Hardware:

- `x86_64`
- dual-core 2.0 GHz o superior
- minimo 4 GB RAM
- 500 MB libres en disco

Software:

- `glibc >= 2.35`
- `libfuse2`
- ALSA, PulseAudio o PipeWire

### Guia de Arranque

Workaround cuarentena macOS:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Flujo Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```
