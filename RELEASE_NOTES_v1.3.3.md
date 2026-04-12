# Release Notes / Note di Rilascio - Fork v1.3.3

Date: 2026-02-28  
Scope: upstream Raptor feature import + fork UI/runtime fixes + release alignment.

## English

### Summary

- Imported upstream Raptor features:
  - B4 strikethrough for worked-on-band calls in Band Activity.
  - Auto CQ caller FIFO queue (max 20) with automatic handoff to next caller.
  - TX slot red bracket overlay (`[ ]`) on waterfall in FT2/FT8/FT4.
  - Automatic `cty.dat` refresh at startup when missing or older than 30 days.
  - FT2 decoder improvements:
    - adaptive `syncmin` profile (`0.90 / 0.85 / 0.80`)
    - AP type extension for Tx3/Tx4
    - relaxed deep-search thresholds (`smax`, `nsync_qual_min`)
    - OSD depth boost near `nfqso`
- Fork v1.3.3 runtime/UI refinements:
  - Startup CAT mode/frequency auto-alignment uses the full frequency list.
  - Small-display top controls now auto-switch to a 2-row responsive layout.
  - Light-theme seconds/progress bar visibility fixed.

### Release Artifacts

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/experimental): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Note: `.pkg` installers are intentionally not produced for this release flow.

### Linux minimum requirements

- Architecture: `x86_64` (64-bit)
- CPU: dual-core 2.0 GHz or better
- RAM: 4 GB minimum (8 GB recommended)
- Storage: at least 500 MB free
- Runtime/software:
  - `glibc >= 2.35`
  - `libfuse2` / FUSE2 support
  - ALSA, PulseAudio, or PipeWire audio stack

### If macOS blocks startup

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## Italiano

### Sintesi

- Importate feature upstream Raptor:
  - Testo barrato B4 per stazioni gia' lavorate in banda nella Band Activity.
  - Coda FIFO Auto CQ chiamanti (max 20) con passaggio automatico al prossimo chiamante.
  - Overlay waterfall con parentesi rosse (`[ ]`) sullo slot TX in FT2/FT8/FT4.
  - Refresh automatico `cty.dat` all'avvio se mancante o piu' vecchio di 30 giorni.
  - Migliorie decoder FT2:
    - profilo `syncmin` adattivo (`0.90 / 0.85 / 0.80`)
    - estensione tipi AP su Tx3/Tx4
    - soglie deep-search rilassate (`smax`, `nsync_qual_min`)
    - incremento profondita' OSD vicino a `nfqso`
- Rifiniture fork v1.3.3 su runtime/UI:
  - Auto-allineamento modalita'/frequenza CAT all'avvio su lista frequenze completa.
  - Controlli top su display piccoli con switch automatico a layout responsivo su 2 righe.
  - Correzione visibilita' barra secondi/progressi nel tema chiaro.

### Artifact release

- macOS Apple Silicon Tahoe: DMG, ZIP, SHA256
- macOS Apple Silicon Sequoia: DMG, ZIP, SHA256
- macOS Intel Sequoia: DMG, ZIP, SHA256
- macOS Intel Monterey (best effort/sperimentale): DMG, ZIP, SHA256
- Linux x86_64: AppImage, SHA256

Nota: i pacchetti `.pkg` non vengono prodotti intenzionalmente in questo flusso release.

### Requisiti minimi Linux

- Architettura: `x86_64` (64 bit)
- CPU: dual-core 2.0 GHz o superiore
- RAM: minimo 4 GB (consigliati 8 GB)
- Spazio disco: almeno 500 MB liberi
- Runtime/software:
  - `glibc >= 2.35`
  - supporto `libfuse2` / FUSE2
  - stack audio ALSA, PulseAudio o PipeWire

### Se macOS blocca l'avvio

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```
