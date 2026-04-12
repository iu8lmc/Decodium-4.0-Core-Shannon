# Decodium Fork v1.3.8 (base: Decodium v3.0 SE Raptor)

## English

This release includes:

- CAT/remote Configure hardening (no forced FT2 mode from generic Configure traffic).
- UDP control hardening with direct target-id enforcement.
- TCI hardening: safer websocket endpoint normalization, safer command argument parsing, and timer/null guards.
- LotW/download hardening: query-builder URLs, password redaction in logs, no-less-safe redirects, and guard checks on manager/reply creation.
- Auto-CQ/DX-ped workflow stability refinements (queue timeout handling + 2-slot flow integration).
- Optional world-map greyline toggle in Settings -> General.
- Distance badge on active world-map path (km/mi according to unit setting).
- Compact top-controls and DX-ped alignment refinements for small displays.
- Multi-monitor geometry safety: tool windows now restore on-screen after display topology changes.
- Legacy tool hardening (`map65`/`qmap`): bounded string copies, device index clamps, safer filename encoding.
- CI compatibility fixes: Linux Hamlib latest-tag parsing fixed and Hamlib 4.x fallback build kept compatible.

Release assets:

- `decodium3-ft2-v1.3.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.8-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.8-linux-x86_64.AppImage.sha256.txt`

Linux minimum requirements:

- `x86_64` CPU, 4 GB RAM minimum (8 GB recommended)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio or PipeWire

If macOS blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Linux AppImage recommendation (avoid read-only filesystem issues):

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No `.pkg` installers are generated for this fork release line.

## Italiano

Questa release include:

- Hardening CAT/Configure remoto (niente forzatura FT2 da traffico Configure generico).
- Hardening controllo UDP con enforcement target-id diretto.
- Hardening TCI: normalizzazione endpoint websocket piu' robusta, parsing argomenti piu' sicuro, guardie timer/null.
- Hardening LotW/download: URL su query builder, redazione password nei log, redirect no-less-safe e guardie su manager/reply.
- Rafforzamento stabilita' workflow Auto-CQ/DX-ped (timeout coda + integrazione 2-slot).
- Toggle opzionale greyline mappa in Settings -> General.
- Badge distanza sul path mappa attivo (km/mi in base all'unita' impostata).
- Rifiniture controlli top compatti e allineamento DX-ped su schermi piccoli.
- Sicurezza geometria multi-monitor: restore finestre su schermo dopo cambi topologia display.
- Hardening tool legacy (`map65`/`qmap`): copie stringa bounded, clamp indici device, encoding filename piu' sicuro.
- Fix compatibilita' CI: parsing latest-tag Hamlib su Linux corretto e fallback Hamlib 4.x compatibile.

Artifact release:

- `decodium3-ft2-v1.3.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64.dmg` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64.zip` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.3.8-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.8-linux-x86_64.AppImage.sha256.txt`

Requisiti minimi Linux:

- CPU `x86_64`, minimo 4 GB RAM (consigliati 8 GB)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Se macOS blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Avvio consigliato AppImage su Linux:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

Per questa linea release fork non vengono generati installer `.pkg`.

## Espanol

Esta release incluye:

- Hardening CAT/Configure remoto (sin forzado FT2 por Configure generico).
- Hardening de control UDP con target-id directo obligatorio.
- Hardening TCI: normalizacion de endpoint websocket, parsing de argumentos mas seguro y guardas timer/null.
- Hardening LotW/descargas: URLs con query builder, redaccion de password en logs, redirects no-less-safe y guardas manager/reply.
- Mejora de estabilidad en flujo Auto-CQ/DX-ped (timeout de cola + integracion 2-slot).
- Opcion greyline del mapa en Settings -> General.
- Distancia sobre la ruta activa del mapa (km/mi segun unidad configurada).
- Ajustes de controles superiores compactos y alineacion DX-ped en pantallas pequenas.
- Seguridad de geometria multi-monitor: restauracion de ventanas en pantalla tras cambios de displays.
- Hardening de herramientas legacy (`map65`/`qmap`): copias bounded, clamp de indices de dispositivos y encoding de rutas mas seguro.
- Fix de compatibilidad CI: parsing latest-tag Hamlib en Linux corregido y fallback Hamlib 4.x mantenido.

Artefactos de release:

- `decodium3-ft2-v1.3.8-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.8-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.8-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.8-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.8-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.3.8-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.3.8-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.8-linux-x86_64.AppImage.sha256.txt`

Requisitos minimos Linux:

- CPU `x86_64`, 4 GB RAM minimo (8 GB recomendado)
- `glibc >= 2.35`
- `libfuse2` / FUSE2
- ALSA, PulseAudio o PipeWire

Si macOS bloquea el inicio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Recomendacion AppImage en Linux:

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

No se generan instaladores `.pkg` en esta linea de releases del fork.
