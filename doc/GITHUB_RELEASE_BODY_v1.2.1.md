# Decodium Fork v1.2.1 (base: Decodium v3.0 SE Raptor)

## English

Maintenance release: UI cleanup and fresher map overlay.

### Highlights

- Soundcard drift display removed (status bar + Time Sync panel); drift computation disabled in Detector.
- World map contacts now expire after 2 minutes to avoid stale callers.
- Program title/docs bumped to fork `v1.2.1`.

### macOS quarantine command

If Gatekeeper blocks the app, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Documentation

- `RELEASE_NOTES_v1.2.1.md`
- `CHANGELOG.md`
- `doc/MACOS_PORTING_v1.2.0.md`
- `doc/DT_NTP_ROBUST_SYNC_v1.2.0.md`

### CI targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia

---

## Italiano

Release di manutenzione: pulizia UI e mappa più fresca.

### Highlight

- Rimossa la visualizzazione del drift scheda audio (status bar + Time Sync); calcolo drift disattivato nel Detector.
- I contatti mappa scadono dopo 2 minuti per evitare chiamate fantasma.
- Titolo/documentazione aggiornati alla fork `v1.2.1`.

### Comando quarantena macOS

Se Gatekeeper blocca l’app:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

### Documentazione

- `RELEASE_NOTES_v1.2.1.md`
- `CHANGELOG.md`
- `doc/MACOS_PORTING_v1.2.0.md`
- `doc/DT_NTP_ROBUST_SYNC_v1.2.0.md`

### Target CI

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
