# Decodium 4 FT2 1.0.378

## Correzione filtro ghost FT2 (decode diretti a te)

- **Filtro ghost meno aggressivo sulle chiamate dirette al tuo call**: la soppressione dei decode con nominativo valido ma grid geograficamente impossibile (es. un IQ italiano con grid nel Pacifico, oltre 5000 km dal suo DXCC) ora si applica **solo ai decode a bassa confidenza** (il marcatore `?`, tipico dei falsi positivi assistiti da AP). Un decode diretto al tuo nominativo **ad alta confidenza** con grid corrotto non viene più nascosto: si riduce il rischio di perdere una chiamata reale. I ghost AP deboli restano filtrati come prima.
- **Meno spam nel log diagnostico**: lo stesso ghost soppresso non viene più registrato decine di volte a ogni ricostruzione della lista decode — ora una sola riga per messaggio.

## Asset

- Installer Windows x64 `.exe` (allegato).
- AppImage Linux / pacchetti macOS generati dai runner GitHub.
