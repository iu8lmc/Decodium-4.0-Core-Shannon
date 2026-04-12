# Decodium Fork v1.2.0 (base: Decodium v3.0 SE Raptor)

## English

Fork release focused on macOS runtime stability and robust DT/NTP behavior for real FT2 operation.

### Highlights

- Based on upstream Decodium v3.0 SE Raptor.
- Runtime migration completed on `ft2.app` / `ft2`.
- Fixed bundle subprocess path/layout (`jt9`).
- TX/PTT/audio race-condition hardening.
- Persistent audio/radio behavior improvements.
- Robust DT/NTP strategy with weak-sync, confirmations, sparse-jump guard, and hold mode.

### macOS quarantine command

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Documentation

- `RELEASE_NOTES_v1.2.0.md`
- `CHANGELOG.md`
- `doc/MACOS_PORTING_v1.2.0.md`
- `doc/DT_NTP_ROBUST_SYNC_v1.2.0.md`

### CI targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia

---

## Italiano

Release fork focalizzata su stabilita runtime macOS e comportamento DT/NTP robusto per operativita FT2 reale.

### Highlights

- Basata su Decodium v3.0 SE Raptor upstream.
- Migrazione runtime completata su `ft2.app` / `ft2`.
- Correzione path/layout sottoprocessi nel bundle (`jt9`).
- Hardening race-condition TX/PTT/audio.
- Miglioramenti persistenza audio/radio.
- Strategia DT/NTP robusta con weak-sync, conferme, filtro sparse-jump e hold mode.

### Comando quarantena macOS

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Documentazione

- `RELEASE_NOTES_v1.2.0.md`
- `CHANGELOG.md`
- `doc/MACOS_PORTING_v1.2.0.md`
- `doc/DT_NTP_ROBUST_SYNC_v1.2.0.md`

### Target CI

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
