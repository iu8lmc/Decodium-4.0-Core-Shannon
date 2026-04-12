# Decodium Fork v1.3.1 (base: Decodium v3.0 SE Raptor)

## English

Release focused on version consistency, Linux release readiness, and cross-platform packaging.

### Highlights

- Program title/version alignment to fork release `v1.3.1`.
- Linux compatibility fixes (`rig_get_conf2` fallback, `libudev` CI dependencies, `ft2` executable packaging path).
- macOS matrix release for Tahoe arm64, Sequoia arm64, Sequoia x86_64.
- Linux x86_64 AppImage release.

### Linux minimum requirements

- Architecture: `x86_64`
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### macOS quarantine command

If Gatekeeper blocks the app, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Artifacts

- `decodium3-ft2-v1.3.1-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.1-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.1-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.1-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.1-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.1-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.1-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.1-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.1-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.1-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.1-linux-x86_64.AppImage.sha256.txt`

---

## Italiano

Release focalizzata su consistenza versione, prontezza release Linux e packaging multipiattaforma.

### Highlight

- Allineamento titolo/versione programma alla release fork `v1.3.1`.
- Fix compatibilita' Linux (`fallback rig_get_conf2`, dipendenze CI `libudev`, path packaging eseguibile `ft2`).
- Release matrix macOS per Tahoe arm64, Sequoia arm64, Sequoia x86_64.
- Release Linux x86_64 AppImage.

### Requisiti minimi Linux

- Architettura: `x86_64`
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Comando quarantena macOS

Se Gatekeeper blocca l'app:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
