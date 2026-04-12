# Release Notes / Note di Rilascio / Notas de Lanzamiento - Fork v1.3.7

Date: 2026-03-03  
Scope: Feature and UX updates from `v1.3.6` to `v1.3.7`, release/documentation alignment, multi-platform artifacts.

## English

### Summary

- Added interactive world-map contact selection and configurable click-to-TX behavior.
- Added new `Ionospheric Forecast` and `DX Cluster` tool windows.
- Improved map rendering/path lifecycle and small-display top controls behavior.
- Fixed decode sequence timing initialization edge-case at startup.
- Cleaned PSKReporter `Using:` branding by removing hardcoded legacy revision suffix source.

### Detailed Changes

- World map / interaction:
  - New contact click signal path (`contactClicked`) from map to main window.
  - Contact click now fills DX call/grid and supports TX start logic.
  - Added setting `Map: single click starts Tx` (default keeps safer double-click TX behavior).
  - Added persistent `View -> World Map` visibility restore.
  - Improved day/night mask rendering and added end-of-QSO role downgrade to clear stale paths.
- New operational windows:
  - `View -> Ionospheric Forecast` (HamQSL XML + sun image, timed refresh, saved geometry/state).
  - `View -> DX Cluster` (live spots, band propagation from rig/band change, mode filter, timed refresh).
- UI/runtime reliability:
  - Top controls row handling improved for compact/two-row modes.
  - Decode sequence-start timestamp is now set in ingest path before decode start.
  - Explicit Qt metatype registration for `Modulator::ModulatorState` to stabilize queued cross-thread delivery.
  - Default Qt style now set to `Fusion` for more consistent rendering across macOS variants.
- Branding/version cleanup:
  - Removed hardcoded early return in `revision()` that forced legacy `mod by IU8LMC...` suffix.
  - Fork release metadata aligned to `v1.3.7` across CMake/workflows/scripts/docs.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced.

### Linux Minimum Requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
  - Linux with `glibc >= 2.35`
  - `libfuse2` / FUSE2 support
  - ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Aggiunta selezione contatti interattiva sul mappamondo con click configurabile per avvio TX.
- Aggiunte nuove finestre strumenti `Ionospheric Forecast` e `DX Cluster`.
- Migliorati rendering mappa/ciclo vita percorsi e comportamento controlli top su schermi piccoli.
- Corretto edge-case di inizializzazione timing decode all'avvio.
- Ripulito branding PSKReporter `Using:` rimuovendo la sorgente hardcoded del suffisso legacy.

### Modifiche Dettagliate

- Mappa / interazione:
  - Nuovo percorso segnale click contatto (`contactClicked`) dalla mappa alla finestra principale.
  - Click contatto ora compila nominativo/griglia DX e supporta logica avvio TX.
  - Aggiunta impostazione `Map: single click starts Tx` (default mantiene comportamento piu' sicuro con TX su doppio click).
  - Aggiunto restore persistente visibilita' `View -> World Map`.
  - Migliorato rendering maschera giorno/notte e downgrade ruolo a fine QSO per eliminare linee stale.
- Nuove finestre operative:
  - `View -> Ionospheric Forecast` (XML HamQSL + immagine sole, refresh temporizzato, geometria/stato salvati).
  - `View -> DX Cluster` (spot live, banda propagata da rig/cambio banda, filtro modo, refresh temporizzato).
- Affidabilita' UI/runtime:
  - Migliorata gestione riga controlli top in modalita' compatta/2 righe.
  - Timestamp sequence-start decode ora impostato nel percorso ingest prima dell'avvio decode.
  - Registrazione esplicita metatype Qt per `Modulator::ModulatorState` per stabilizzare delivery cross-thread queued.
  - Stile Qt predefinito impostato a `Fusion` per rendering piu' uniforme tra varianti macOS.
- Branding/versioning:
  - Rimossa return hardcoded in `revision()` che forzava il suffisso legacy `mod by IU8LMC...`.
  - Metadati release fork allineati a `v1.3.7` su CMake/workflow/script/documentazione.

### Artifact Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente.

### Requisiti Minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
  - Linux con `glibc >= 2.35`
  - supporto `libfuse2` / FUSE2
  - stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Espanol

### Resumen

- Se agrega seleccion interactiva de contactos en el mapamundi con click configurable para iniciar TX.
- Se agregan nuevas ventanas de herramientas `Ionospheric Forecast` y `DX Cluster`.
- Se mejora el render del mapa/ciclo de vida de rutas y el comportamiento de controles superiores en pantallas pequenas.
- Se corrige un caso limite de inicializacion de timing de decode al arranque.
- Se limpia el branding `Using:` de PSKReporter eliminando la fuente hardcoded del sufijo legacy.

### Cambios Detallados

- Mapa / interaccion:
  - Nueva ruta de senal de click de contacto (`contactClicked`) del mapa a la ventana principal.
  - El click de contacto ahora rellena indicativo/cuadricula DX y soporta logica de arranque TX.
  - Nueva opcion `Map: single click starts Tx` (por defecto mantiene comportamiento mas seguro: TX con doble click).
  - Persistencia del estado visible de `View -> World Map`.
  - Mejor render de mascara dia/noche y limpieza de rutas stale al fin de QSO.
- Nuevas ventanas operativas:
  - `View -> Ionospheric Forecast` (XML HamQSL + imagen solar, refresco temporizado, geometria/estado guardados).
  - `View -> DX Cluster` (spots en vivo, banda sincronizada por rig/cambio de banda, filtro de modo, refresco temporizado).
- Fiabilidad UI/runtime:
  - Mejor manejo del layout de controles superiores en modo compacto/2 filas.
  - Timestamp de inicio de secuencia de decode capturado en ingest antes de iniciar decode.
  - Registro explicito de metatype Qt para `Modulator::ModulatorState` para entrega queued robusta entre hilos.
  - Estilo Qt por defecto `Fusion` para consistencia visual entre variantes macOS.
- Branding/versionado:
  - Eliminado el retorno hardcoded en `revision()` que forzaba el sufijo legacy `mod by IU8LMC...`.
  - Metadatos de release del fork alineados a `v1.3.7` en CMake/workflows/scripts/docs.

### Artefactos de Release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: no se generan instaladores `.pkg`.

### Requisitos Minimos Linux

- Arquitectura: `x86_64` (64 bits)
- CPU: dual-core 2.0 GHz o superior
- RAM: 4 GB minimo (8 GB recomendado)
- Espacio en disco: al menos 500 MB libres
- Runtime/software:
  - Linux con `glibc >= 2.35`
  - soporte `libfuse2` / FUSE2
  - audio ALSA, PulseAudio o PipeWire

### Si macOS bloquea el inicio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
