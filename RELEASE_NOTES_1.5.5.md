# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork 1.5.5

Scope: update cycle from `1.5.4` to `1.5.5`.

## English

`1.5.5` is a maintenance and hardening release focused on macOS Preferences correctness, FT2 decoder observability, ADIF alignment, audio-start recovery, and removal of the incomplete RTTY user path from the public release.

### Detailed Changes (`1.5.4 -> 1.5.5`)

- fixed macOS app-menu role handling so `Preferences...` always opens the real settings dialog, regardless of the selected UI language.
- fixed the macOS `Settings` dialog becoming taller than the available screen and hiding the confirm buttons.
- made settings pages scrollable on macOS, not only Linux.
- added persistent `jt9_subprocess.log` in the writable data directory for FT2/jt9 launch, stderr, error, and termination tracing.
- improved FT2 subprocess crash popups by surfacing captured stdout/stderr context instead of only `Process crashed`.
- improved shared-memory failure diagnostics in the `jt9` decoder path and replaced opaque abort behaviour with clearer reporting.
- standardized FT2 ADIF export to `MODE=MFSK` and `SUBMODE=FT2`.
- added automatic in-place migration of historical `MODE=FT2` ADIF records with `.pre317bak` backup preservation.
- applied the same FT2 ADIF migration path to temporary LoTW merge/download files.
- improved startup and monitor audio recovery by replaying the same reopen path used by `Settings > Audio`, with health checks if no RX frames arrive.
- added extra audio debug tracing so future startup regressions are visible in `debug.txt`.
- withdrew the experimental RTTY menu entry and modem settings from the public user interface pending further validation.
- blocked hidden-mode activation through the remote mode API and removed RTTY from the public external-controller mode list.
- aligned local version metadata, workflow defaults, release documents, and About text to `1.5.5`.

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

`1.5.5` e' una release di manutenzione e hardening focalizzata sulla correttezza di Preferenze su macOS, sull'osservabilita' del decoder FT2, sull'allineamento ADIF, sul recupero dell'audio all'avvio e sulla rimozione dal percorso pubblico della UI RTTY incompleta.

### Modifiche Dettagliate (`1.5.4 -> 1.5.5`)

- corretto il trattamento dei ruoli menu dell'app su macOS, cosi' `Preferenze...` apre sempre il vero dialog impostazioni, indipendentemente dalla lingua UI selezionata.
- corretto il dialog `Settings` su macOS che poteva diventare piu' alto dello schermo e nascondere i pulsanti finali.
- rese scrollabili le pagine di `Settings` anche su macOS, non solo su Linux.
- aggiunto `jt9_subprocess.log` persistente nella directory dati scrivibile per tracciare avvio, stderr, errori e terminazione del subprocess FT2/jt9.
- migliorati i popup di crash del subprocess FT2 mostrando il contesto stdout/stderr catturato invece del solo `Process crashed`.
- migliorata la diagnostica dei fallimenti shared-memory nel path decoder `jt9`, sostituendo abort opachi con reporting piu' chiaro.
- standardizzato l'export ADIF FT2 in `MODE=MFSK` e `SUBMODE=FT2`.
- aggiunta la migrazione automatica in-place dei record ADIF storici `MODE=FT2` con backup `.pre317bak`.
- applicata la stessa migrazione FT2 ADIF ai file temporanei LoTW di merge/download.
- migliorato il recupero audio all'avvio e all'attivazione monitor ripetendo lo stesso reopen usato da `Settings > Audio`, con health check se non arrivano frame RX.
- aggiunto tracing audio extra in `debug.txt` per rendere visibili future regressioni di startup.
- ritirate dalla UI pubblica la voce menu RTTY sperimentale e le relative impostazioni modem, in attesa di ulteriore validazione.
- bloccata l'attivazione del modo nascosto via API remote e rimosso RTTY dalla lista pubblica dei modi del controller esterno.
- allineati a `1.5.5` metadati versione locali, default dei workflow, documenti release e testo About.

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

Flusso Linux AppImage extract-run:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## Espanol

`1.5.5` es una release de mantenimiento y hardening centrada en la correccion de Preferencias en macOS, la observabilidad del decoder FT2, la alineacion ADIF, la recuperacion del audio al arrancar y la retirada del camino publico de la UI RTTY incompleta.

### Cambios Detallados (`1.5.4 -> 1.5.5`)

- corregido el tratamiento de roles de menu de la app en macOS para que `Preferencias...` abra siempre el dialogo real de ajustes, independientemente del idioma UI seleccionado.
- corregido el dialogo `Settings` en macOS que podia hacerse mas alto que la pantalla y ocultar los botones finales.
- hechas scrollables las paginas de `Settings` tambien en macOS, no solo en Linux.
- anadido `jt9_subprocess.log` persistente en la carpeta de datos escribible para trazar arranque, stderr, errores y terminacion del subprocess FT2/jt9.
- mejorados los popups de fallo del subprocess FT2 mostrando el contexto stdout/stderr capturado en lugar de solo `Process crashed`.
- mejorada la diagnostica de fallos shared-memory en la ruta del decoder `jt9`, sustituyendo aborts opacos por reporting mas claro.
- estandarizado el export ADIF FT2 a `MODE=MFSK` y `SUBMODE=FT2`.
- anadida la migracion automatica in-place de registros ADIF historicos `MODE=FT2` con backup `.pre317bak`.
- aplicada la misma migracion FT2 ADIF a los ficheros temporales LoTW de merge/download.
- mejorada la recuperacion de audio al arrancar y al activar monitor repitiendo el mismo reopen usado por `Settings > Audio`, con health checks si no llegan tramas RX.
- anadido tracing extra de audio en `debug.txt` para hacer visibles futuras regresiones de arranque.
- retiradas de la UI publica la entrada de menu RTTY experimental y sus ajustes de modem, a la espera de validacion adicional.
- bloqueada la activacion del modo oculto via API remota y eliminado RTTY de la lista publica de modos del controlador externo.
- alineados a `1.5.5` los metadatos de version locales, los defaults de workflow, los documentos release y el texto About.

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
