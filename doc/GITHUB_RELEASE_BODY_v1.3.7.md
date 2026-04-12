# Decodium Fork v1.3.7 (base: Decodium v3.0 SE Raptor)

## English

This release includes:

- Interactive world-map contact selection with highlight + DX call/grid transfer.
- Configurable map click behavior (`Map: single click starts Tx`).
- New `Ionospheric Forecast` and `DX Cluster` windows in the View menu.
- Improved map day/night rendering and stale path cleanup at end-of-QSO.
- Improved compact/two-row top controls handling for small displays.
- Decode sequence timestamp startup fix.
- Explicit Qt metatype registration for `ModulatorState` cross-thread delivery.
- Default UI style set to `Fusion` for macOS consistency.
- Removed hardcoded legacy revision suffix source (`mod by IU8LMC...`) from runtime revision path.

Release assets:

- `decodium3-ft2-v1.3.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.7-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.7-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU, 4 GB RAM minimum (8 GB recommended)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

No `.pkg` installers are generated for this fork release line.

## Italiano

Questa release include:

- Selezione contatti interattiva sul mappamondo con highlight + trasferimento nominativo/griglia DX.
- Comportamento click mappa configurabile (`Map: single click starts Tx`).
- Nuove finestre `Ionospheric Forecast` e `DX Cluster` nel menu View.
- Rendering giorno/notte mappa migliorato e pulizia linee stale a fine QSO.
- Migliorata gestione controlli top compatti/2 righe su schermi piccoli.
- Fix timestamp sequenza decode in avvio.
- Registrazione metatype Qt esplicita per delivery cross-thread di `ModulatorState`.
- Stile UI predefinito impostato a `Fusion` per coerenza su macOS.
- Rimossa sorgente hardcoded del suffisso legacy revision (`mod by IU8LMC...`).

Artifact release:

- `decodium3-ft2-v1.3.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64.dmg` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64.zip` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.7-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.7-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64`, minimo 4 GB RAM (consigliati 8 GB)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Per questa linea release fork non vengono generati installer `.pkg`.

## Espanol

Esta release incluye:

- Seleccion interactiva de contactos en el mapamundi con resaltado + transferencia de indicativo/cuadricula DX.
- Comportamiento de click en mapa configurable (`Map: single click starts Tx`).
- Nuevas ventanas `Ionospheric Forecast` y `DX Cluster` en el menu View.
- Mejor render dia/noche del mapa y limpieza de rutas stale al fin de QSO.
- Mejor manejo de controles superiores compactos/2 filas en pantallas pequenas.
- Fix de timestamp de secuencia decode en arranque.
- Registro explicito de metatype Qt para entrega cross-thread de `ModulatorState`.
- Estilo UI por defecto `Fusion` para consistencia en macOS.
- Eliminada fuente hardcoded del sufijo legacy de revision (`mod by IU8LMC...`).

Artefactos de release:

- `decodium3-ft2-v1.3.7-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.7-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.7-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.7-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.7-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.3.7-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.3.7-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.7-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- CPU `x86_64`, 4 GB RAM minimo (8 GB recomendado)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

No se generan instaladores `.pkg` en esta linea de releases del fork.
