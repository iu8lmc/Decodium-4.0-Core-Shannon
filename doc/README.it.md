# Note di Documentazione (Italiano)

## Scopo

Note specifiche del fork macOS nel repository.

## Contesto release attuale

- Ultima release stabile: `v1.3.8`
- Target: macOS Tahoe ARM64, Sequoia ARM64, Sequoia Intel, Monterey Intel (sperimentale), Linux x86_64 AppImage

## Note build e runtime

### Output build

- `build/ft2.app/Contents/MacOS/ft2`
- eseguibili di supporto nello stesso bundle (`jt9`, `wsprd`)

### Memoria condivisa su macOS

- Il fork usa `SharedMemorySegment` con backend `mmap` su Darwin.
- Il flusso release non dipende piu' da tuning `sysctl` System V (`kern.sysv.shmmax/shmall`).

### Hardening CAT/rete e aggiornamenti mappa/UI (v1.3.8)

- Hardening CAT/Configure remoto per evitare forzature FT2 da pacchetti generici.
- I comandi di controllo UDP richiedono target id diretto.
- Toggle greyline opzionale in Settings -> General.
- Badge distanza sul path mappa attivo in km/mi secondo unita' impostate.
- Rifiniture layout controlli top per allineamento DX-ped su schermi piccoli.

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
- `RELEASE_NOTES_v1.3.8.md`
- `doc/GITHUB_RELEASE_BODY_v1.3.8.md`
- `doc/README.es.md`
- `doc/SECURITY_BUG_ANALYSIS_REPORT.md`
