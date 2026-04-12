# Decodium Fork v1.3.4 (base: Decodium v3.0 SE Raptor)

## English

Release focused on TCI hardening, concurrency safety, and runtime stability.

### Highlights

- TCI binary packet parser hardening with strict payload bounds checks.
- Removal of nested `QEventLoop::exec()` pseudo-sync waits in TCI `mysleep1..8`.
- Protected snapshot access for `foxcom_.wave` across UI/audio threads.
- Hardened TCI C++/Fortran buffer boundary (`kin` clamps + bounded writes).
- Improved macOS audio resilience (Sequoia-safe stop path + underrun handling).
- TOTP now uses NTP-corrected time source.
- `QRegExp` -> `QRegularExpression` migration in critical runtime/network paths.

### Linux minimum requirements

- Architecture: `x86_64`
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### macOS quarantine command

If Gatekeeper blocks startup:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Artifacts

- `decodium3-ft2-v1.3.4-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.4-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.4-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.4-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.4-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.4-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.4-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.4-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.4-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.4-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.4-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.4-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.4-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.4-linux-x86_64.AppImage.sha256.txt`

Note: `.pkg` installers are not produced.

---

## Italiano

Release focalizzata su hardening TCI, sicurezza concorrenza e stabilita' runtime.

### Highlight

- Hardening parser pacchetti binari TCI con controlli rigorosi su payload.
- Rimozione attese pseudo-sync con loop annidati `QEventLoop::exec()` in `mysleep1..8`.
- Accesso a `foxcom_.wave` tramite snapshot protetti tra thread UI/audio.
- Hardening boundary buffer TCI C++/Fortran (`kin` clamp + scritture limitate).
- Migliorata resilienza audio macOS (stop path Sequoia-safe + gestione underrun).
- TOTP ora allineato alla sorgente tempo corretta NTP.
- Migrazione `QRegExp` -> `QRegularExpression` nei percorsi runtime/network critici.

### Requisiti minimi Linux

- Architettura: `x86_64`
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Runtime: `glibc >= 2.35`, `libfuse2`/FUSE2, ALSA/PulseAudio/PipeWire

### Comando quarantena macOS

Se Gatekeeper blocca l'avvio:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Nota: i pacchetti `.pkg` non vengono prodotti.
