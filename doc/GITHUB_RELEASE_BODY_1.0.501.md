# Decodium 4.0 v1.0.501

Version 1.0.501 corrects panadapter spectrum rendering when the visible
frequency viewport extends beyond the FFT data supplied by the audio backend.
The fix prevents artificial edge slopes and floor-level triangles while
preserving the real receiver passband response.

## Changes from 1.0.500 to 1.0.501

### Panadapter frequency mapping

- Added a shared frequency-view calculation for the CPU painter and Qt Quick
  scene graph paths.
- Validated the configured start frequency, bandwidth, zoom and pan against the
  actual FFT frequency range before building spectrum geometry.
- Added finite-value and minimum-range guards so invalid or transient viewport
  values cannot produce malformed spectrum coordinates.
- Kept frequency-to-pixel and frequency-to-bin conversion on the same reference
  system throughout the renderer.

### Edge rendering correctness

- Stopped drawing spectrum lines through frequency areas for which the backend
  supplied no FFT bins.
- Split the CPU spectrum fill into valid data segments instead of connecting
  the last real sample to the graph floor at the edge of the viewport.
- Prevented empty edge regions from creating false diagonal roll-off shapes or
  filled triangles that could be mistaken for the radio filter response.
- Added bounds checks to peak-hold rendering so an empty path is never submitted
  to the painter.

### GPU fallback and diagnostics

- Disabled the direct GPU spectrum graph only when the current viewport clips
  the available FFT data; the normal GPU path remains active when the complete
  view is backed by valid bins.
- Clamped scene-graph sample frequencies to the real FFT range as a defensive
  guard against rounding at the boundaries.
- Added a rate-limited `PANDBG` diagnostic containing the visible range, FFT
  data range and bin count whenever the safe CPU fallback is selected.
- Kept waterfall GPU acceleration independent from this spectrum-graph
  fallback.

## Validation

- Local macOS build gate:
  `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `git diff --check`
- Temporary lab artifacts, diagnostic scratch files and generated data under
  `tmp/` are intentionally excluded from this release commit.

---

## Italiano

La versione 1.0.501 corregge il rendering dello spettro del panadapter quando
la finestra di frequenza visibile supera i dati FFT realmente forniti dal
backend audio. Il fix elimina pendenze artificiali e triangoli al livello di
fondo, mantenendo invece visibile la reale risposta del filtro del ricevitore.

### Mappatura delle frequenze

- Aggiunto un calcolo condiviso della finestra di frequenza per il percorso CPU
  e per il scene graph Qt Quick.
- Verificati frequenza iniziale, larghezza di banda, zoom e pan rispetto
  all'intervallo FFT realmente disponibile prima di costruire la geometria.
- Aggiunte protezioni per valori non finiti o intervalli non validi, evitando
  coordinate malformate durante stati transitori.
- Uniformata la base di riferimento tra conversione frequenza-pixel e
  frequenza-bin.

### Correttezza ai bordi

- Lo spettro non viene piu' disegnato nelle aree di frequenza prive di bin FFT.
- Il riempimento CPU viene suddiviso in segmenti validi, senza collegare
  l'ultimo campione reale al fondo del grafico sul bordo della finestra.
- Eliminate le pendenze diagonali e i triangoli artificiali che potevano essere
  confusi con la risposta del filtro della radio.
- Aggiunti controlli al peak hold per evitare l'invio di path vuoti al painter.

### Fallback GPU e diagnostica

- Il grafico spettro GPU diretto viene disattivato soltanto quando il viewport
  supera i dati FFT; con un intervallo completamente valido resta operativo il
  normale percorso accelerato.
- Le frequenze campionate dal scene graph vengono limitate all'intervallo FFT
  reale per proteggere i bordi dagli arrotondamenti.
- Aggiunto un messaggio diagnostico `PANDBG`, limitato nel tempo, con intervallo
  visibile, intervallo FFT e numero di bin quando viene scelto il fallback CPU.
- L'accelerazione GPU del waterfall resta indipendente da questo fallback del
  solo grafico spettro.
