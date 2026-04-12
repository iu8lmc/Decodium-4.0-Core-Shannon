# Decodium Fork v1.3.3 (base: Decodium v3.0 SE Raptor)

## English

Release focused on upstream Raptor feature import plus fork stability/UI refinements.

### Highlights

- Upstream import:
  - B4 strikethrough in Band Activity.
  - Auto CQ caller FIFO queue (max 20).
  - TX bracket overlay (`[ ]`) on waterfall (FT2/FT8/FT4).
  - Auto-refresh/download for `cty.dat` at startup (30-day policy).
  - FT2 decoder tuning updates (`syncmin`, AP types, deep-search thresholds, OSD near `nfqso`).
- Fork fixes:
  - Startup CAT mode auto-alignment using full frequency list.
  - Responsive 2-row top controls on small displays.
  - Light-theme progress/seconds bar visibility fix.
  - Experimental best-effort Intel Monterey artifact path added in GitHub workflows.
- macOS + Linux artifact set (no `.pkg`).

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

- `decodium3-ft2-v1.3.3-macos-tahoe-arm64.dmg`
- `decodium3-ft2-v1.3.3-macos-tahoe-arm64.zip`
- `decodium3-ft2-v1.3.3-macos-tahoe-arm64-sha256.txt`
- `decodium3-ft2-v1.3.3-macos-sequoia-arm64.dmg`
- `decodium3-ft2-v1.3.3-macos-sequoia-arm64.zip`
- `decodium3-ft2-v1.3.3-macos-sequoia-arm64-sha256.txt`
- `decodium3-ft2-v1.3.3-macos-sequoia-x86_64.dmg`
- `decodium3-ft2-v1.3.3-macos-sequoia-x86_64.zip`
- `decodium3-ft2-v1.3.3-macos-sequoia-x86_64-sha256.txt`
- `decodium3-ft2-v1.3.3-macos-monterey-x86_64.dmg` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.3-macos-monterey-x86_64.zip` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.3-macos-monterey-x86_64-sha256.txt` *(best effort/experimental, when produced)*
- `decodium3-ft2-v1.3.3-linux-x86_64.AppImage`
- `decodium3-ft2-v1.3.3-linux-x86_64.AppImage.sha256.txt`

---

## Italiano

Release focalizzata su import feature upstream Raptor e rifiniture stabilita'/UI del fork.

### Highlight

- Import upstream:
  - Testo barrato B4 in Band Activity.
  - Coda FIFO Auto CQ chiamanti (max 20).
  - Overlay bracket TX (`[ ]`) sulla waterfall (FT2/FT8/FT4).
  - Refresh/download automatico `cty.dat` all'avvio (policy 30 giorni).
  - Aggiornamenti tuning decoder FT2 (`syncmin`, tipi AP, soglie deep-search, OSD vicino a `nfqso`).
- Fix fork:
  - Auto-allineamento modalita' CAT all'avvio su lista frequenze completa.
  - Controlli top responsivi su 2 righe per display piccoli.
  - Correzione visibilita' barra progressi/secondi nel tema chiaro.
  - Aggiunto percorso artifact Intel Monterey sperimentale/best effort nei workflow GitHub.
- Set artifact macOS + Linux (nessun `.pkg`).

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
