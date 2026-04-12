# Decodium Fork v1.4.0 (base: Decodium v3.0 SE Raptor)

## English

This release includes:

- FT2 decode stream stabilization:
  - packed-row split handling to avoid merged/misaligned lines,
  - near-duplicate suppression (5-second window, best-SNR preference),
  - consistent filtering on both normal and async decode paths.
- Async L2 now appears only in FT2 mode and is auto-disabled when leaving FT2.
- Remote Web Dashboard maturity updates:
  - LAN settings (bind/port/user/token) from application settings,
  - username/password auth flow,
  - improved mobile/PWA usability,
  - cleaner operator UI with mode/band/rx/tx/auto-cq controls.
- Existing CAT/UDP/TCI hardening retained.
- Existing map/runtime improvements retained (optional greyline + map distance badge).

Release assets:

- `decodium3-ft2-v1.4.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.4.0-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.0-linux-x86_64.AppImage.sha256.txt`

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

Detailed web app setup guides (novice):

- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`

## Italiano

Questa release include:

- Stabilizzazione stream decode FT2:
  - split delle righe packed per evitare linee fuse/disallineate,
  - soppressione near-duplicate (finestra 5 secondi, preferenza miglior SNR),
  - filtro uniforme su decode normale e percorso async.
- Async L2 visibile solo in modalita' FT2 e auto-disabilitato in uscita da FT2.
- Aggiornamenti di maturazione Dashboard Web Remota:
  - impostazioni LAN (bind/porta/user/token) dal pannello impostazioni,
  - autenticazione username/password,
  - usabilita' mobile/PWA migliorata,
  - UI operatore piu' pulita con controlli mode/band/rx/tx/auto-cq.
- Hardening CAT/UDP/TCI delle release precedenti mantenuto.
- Migliorie mappa/runtime precedenti mantenute (greyline opzionale + distanza su path mappa).

Artifact release:

- `decodium3-ft2-v1.4.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64.dmg` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64.zip` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64-sha256.txt` *(best effort/sperimentale, quando prodotto)*
- `decodium3-ft2-v1.4.0-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.0-linux-x86_64.AppImage.sha256.txt`

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

Guide web app dettagliate (neofiti):

- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`

## Espanol

Esta release incluye:

- Estabilizacion del flujo decode FT2:
  - split de lineas packed para evitar lineas fusionadas/desalineadas,
  - supresion near-duplicate (ventana 5 segundos, preferencia mejor SNR),
  - filtro consistente en decode normal y ruta async.
- Async L2 visible solo en modo FT2 y auto-desactivado al salir de FT2.
- Mejoras de madurez del Dashboard Web Remoto:
  - ajustes LAN (bind/puerto/user/token) desde configuracion,
  - autenticacion usuario/password,
  - mejor usabilidad mobile/PWA,
  - UI mas limpia con controles mode/band/rx/tx/auto-cq.
- Se mantiene hardening CAT/UDP/TCI de releases previas.
- Se mantienen mejoras mapa/runtime previas (greyline opcional + distancia en ruta del mapa).

Artefactos de release:

- `decodium3-ft2-v1.4.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.4.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.4.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.4.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.4.0-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64.dmg` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64.zip` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.0-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, si se genera)*
- `decodium3-ft2-v1.4.0-linux-x86_64.AppImage`
- `decodium3-ft2-v1.4.0-linux-x86_64.AppImage.sha256.txt`

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

Guias web app detalladas (principiantes):

- `doc/WEBAPP_SETUP_GUIDE.es.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
