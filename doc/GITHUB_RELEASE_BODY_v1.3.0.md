# Decodium Fork v1.3.0 (base: Decodium v3.0 SE Raptor)

## English

Release focused on security hardening and runtime stability on macOS.

### Highlights

- Network/TLS and NTP hardening updates.
- Shared-memory migration on macOS to `SharedMemorySegment` (`mmap` backend).
- CAT reconnect resilience and startup mode alignment from rig frequency.
- DT/NTP behavior tuning for FT2/FT4/FT8.
- Release assets for:
  - Apple Silicon Tahoe
  - Apple Silicon Sequoia
  - Apple Intel Sequoia

### macOS quarantine command

If Gatekeeper blocks the app, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Artifacts

- `decodium3-ft2-v1.3.0-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.0-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.0-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.0-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.0-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.0-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.0-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.0-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.0-macos-sequoia-x86_64-sha256.txt`

---

## Italiano

Release focalizzata su hardening sicurezza e stabilita' runtime su macOS.

### Highlight

- Aggiornamenti di hardening su rete/TLS e NTP.
- Migrazione memoria condivisa macOS a `SharedMemorySegment` (backend `mmap`).
- Maggiore resilienza CAT in riconnessione e allineamento modo in avvio da frequenza radio.
- Tuning comportamento DT/NTP per FT2/FT4/FT8.
- Asset release per:
  - Apple Silicon Tahoe
  - Apple Silicon Sequoia
  - Apple Intel Sequoia

### Comando quarantena macOS

Se Gatekeeper blocca l'app:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
