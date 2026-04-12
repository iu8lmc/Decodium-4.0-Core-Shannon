# Release Notes / Note di Rilascio - Fork v1.2.0

Date / Data: 2026-02-26

Base: Decodium v3.0 SE "Raptor" (upstream IU8LMC)

---

## English

### Scope

Fork release `v1.2.0` keeps the Raptor coding/decoding baseline and adds macOS runtime hardening for stable FT2 operation.

### Main points

- Runtime/app migration finalized on `ft2.app` / `ft2`.
- Subprocess path fixes (`jt9` in bundle runtime path).
- Startup microphone preflight to avoid delayed permission popup during QSO.
- TX/PTT/audio stability improvements for repeated FT2 cycles.
- Audio/radio persistence hardening for device/channel restore.
- DT/NTP robust strategy:
  - weak-sync deadband and confirmations,
  - lock-aware hold mode,
  - sparse server jump confirmation,
  - safer fallback behavior.
- Branding and UX cleanup aligned with Decodium fork identity.

### macOS first run (quarantine)

If macOS blocks the app after install/extract, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Then start `ft2.app` again.

### CI release targets

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia

### Notes on assets

Current online `v1.2.0` assets include `.zip`, `.tar.gz`, and `.sha256` for each target.

---

## Italiano

### Ambito

La release fork `v1.2.0` mantiene la baseline Raptor per codifica/decodifica e aggiunge hardening runtime su macOS per un uso FT2 stabile.

### Punti principali

- Migrazione runtime/app completata su `ft2.app` / `ft2`.
- Correzione path sottoprocessi (`jt9` nel bundle runtime).
- Preflight microfono all'avvio per evitare popup tardivo durante i QSO.
- Miglioramenti stabilita TX/PTT/audio sui cicli FT2 ripetuti.
- Persistenza audio/radio piu robusta nel ripristino device/canali.
- Strategia DT/NTP robusta:
  - deadband e conferme weak-sync,
  - modalita hold con lock-aware,
  - conferma salti con pochi server,
  - fallback piu sicuro.
- Pulizia branding/UX allineata all'identita del fork Decodium.

### Primo avvio macOS (quarantena)

Se macOS blocca l'app dopo installazione/estrazione, eseguire:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

Poi riavviare `ft2.app`.

### Target CI release

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia

### Nota sugli asset

Gli asset online correnti di `v1.2.0` includono `.zip`, `.tar.gz` e `.sha256` per ciascun target.
