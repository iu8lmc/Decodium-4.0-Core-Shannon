# Note di Documentazione (Italiano)

## Scopo

Note specifiche del fork macOS nel repository.

## Contesto release attuale

- Ultima release stabile: `v1.4.1`
- Target: macOS Tahoe ARM64, Sequoia ARM64, Sequoia Intel, Monterey Intel (sperimentale), Linux x86_64 AppImage

## Note build e runtime

### Output build

- `build/ft2.app/Contents/MacOS/ft2`
- eseguibili di supporto nello stesso bundle (`jt9`, `wsprd`)

### Memoria condivisa su macOS

- Il fork usa `SharedMemorySegment` con backend `mmap` su Darwin.
- Il flusso release non dipende da tuning `sysctl` System V (`kern.sysv.shmmax/shmall`).

### Evidenze consolidate v1.4.1

- Stabilizzazione flusso decode FT2 con split righe packed + soppressione near-duplicate (5 secondi).
- Controllo Async L2 visibile solo in FT2 e auto-disabilitato fuori FT2.
- Auto-selezione startup da frequenza radio resa one-shot; risposta iniziale al cambio modalita' ripristinata.
- Il cambio modalita' non forza piu' il waterfall in primo piano.
- Maturazione dashboard web remota (config LAN, auth username/password, comportamento mobile/PWA).
- Hardening CAT/UDP/TCI e opzioni mappa (greyline + distanza su path) mantenuti.

### Artifact release

- `decodium3-ft2-<version>-<asset-suffix>.dmg`
- `decodium3-ft2-<version>-<asset-suffix>.zip`
- `decodium3-ft2-<version>-<asset-suffix>-sha256.txt`
- `decodium3-ft2-<version>-linux-x86_64.AppImage`
- `decodium3-ft2-<version>-linux-x86_64.AppImage.sha256.txt`

### Requisiti minimi Linux

- Architettura: `x86_64`
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Avvio consigliato AppImage

```bash
chmod +x /path/to/Decodium.AppImage
/path/to/Decodium.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Comando quarantena Gatekeeper

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Riferimenti

- `CHANGELOG.md`
- `RELEASE_NOTES_v1.4.1.md`
- `doc/GITHUB_RELEASE_BODY_v1.4.1.md`
- `doc/WEBAPP_SETUP_GUIDE.it.md`
- `doc/WEBAPP_SETUP_GUIDE.en-GB.md`
- `doc/WEBAPP_SETUP_GUIDE.es.md`
- `doc/SECURITY_BUG_ANALYSIS_REPORT.md`
