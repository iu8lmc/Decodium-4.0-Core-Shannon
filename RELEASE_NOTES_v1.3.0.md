# Release Notes / Note di Rilascio - Fork v1.3.0

Date: 2026-02-28  
Scope: security hardening, shared-memory refactor on macOS, CAT/runtime stability, DT/NTP tuning.

## English

### Summary

- Security hardening across network stack (TLS handling, NTP validation improvements, safer defaults).
- macOS shared-memory path migrated to `SharedMemorySegment` (`mmap` backend on Darwin).
- CAT robustness improvements (reconnect handling, startup mode alignment from radio frequency).
- FT2/FT4/FT8 DT-NTP stability tuning and status presentation improvements.
- Release flow updated for platform-specific artifacts:
  - Apple Silicon Tahoe
  - Apple Silicon Sequoia
  - Apple Intel Sequoia

### Packaging

- Release assets now include DMG + ZIP + SHA256.
- `.pkg` generation is removed from default release flow because shared-memory sysctl bootstrapping is no longer required by this fork path.

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Hardening di sicurezza su stack rete (gestione TLS, validazioni NTP, default piu' sicuri).
- Migrazione memoria condivisa macOS a `SharedMemorySegment` (backend `mmap` su Darwin).
- Miglioramenti robustezza CAT (riconnessione, allineamento modalita' iniziale da frequenza radio).
- Tuning stabilita' DT-NTP per FT2/FT4/FT8 e miglioramenti di presentazione in status bar.
- Flusso release aggiornato con artifact separati per:
  - Apple Silicon Tahoe
  - Apple Silicon Sequoia
  - Apple Intel Sequoia

### Packaging

- Gli artifact release includono DMG + ZIP + SHA256.
- La generazione `.pkg` e' rimossa dal flusso standard, poiche' il bootstrap sysctl della shared memory non e' piu' richiesto in questo fork.

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
