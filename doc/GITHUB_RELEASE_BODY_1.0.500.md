# Decodium 4.0 v1.0.500

Version 1.0.500 focuses on FST4/FST4W integration, decode/logging correctness
and waterfall usability. It also hardens several edge cases found while testing
new weak-signal modes through lab audio and on-air style workflows.

## Changes from 1.0.499 to 1.0.500

### FST4 and FST4W bridge support

- Added bridge-owned TX/RX handling for FST4 and FST4W, including generated
  waveform support, TX validation and mode-aware audio dispatch.
- Added slot-aware period handling for FST4/FST4W variants, including 15, 30,
  60, 120, 300, 900 and 1800 second schedules where supported by the selected
  mode suffix.
- Added mode-specific timing helpers for FST4/FST4W symbol periods, lead-in,
  minimum useful payload and manual TX scheduling.
- Extended lab/no-CAT mode handling so FST4 and FST4W preserve their real slot
  timing in automated test sessions.
- Updated generated TX messages so FST4W can expose the WSPR-style power field
  from the selected dBm setting.

### Decode quality and false decode filtering

- Improved FST4/FST4W decode post-processing so only actionable messages are
  promoted to the UI and downstream services.
- Suppressed strong artifact clusters where a very strong candidate creates
  weak non-actionable companion decodes.
- Added guards for unresolved hash/grid-only placeholders so they do not feed
  DXCC, map updates, PSK Reporter, persistence, worked-before state or QSO
  enrichment.
- Improved decoded callsign extraction so grid tokens and placeholder-only
  fragments are not treated as valid stations.

### Logging and filtering correctness

- Fixed the CQ-only frontend filter so it affects display only. Valid decodes
  continue to be written to `ALL.TXT` and background logs even when the UI is
  showing only CQ calls.
- Kept legacy decoder backends independent from presentation-only CQ filters,
  preventing hidden decoder state from dropping non-CQ signal reports.
- Appended raw valid decode lines before UI filtering in the legacy FT2/FT8
  paths, protecting complete logging and later review.

### Waterfall and UI polish

- Changed the default embedded waterfall view to begin at 200 Hz at normal
  zoom, reducing the empty left-side area that looked like a rendering fault.
- Applied the same 200 Hz lower bound to detached waterfall windows and their
  fallback labels.
- Added small UI adjustments around generated mode labels and lab handling so
  the new long-slot modes behave consistently with the rest of Decodium4.

### Build compatibility

- Guarded FFTW planner thread calls behind the available FFTW thread feature so
  builds without threaded FFTW support remain valid.

## Validation

- Local macOS build gate:
  `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- Temporary lab artifacts, diagnostic scratch files and generated data under
  `tmp/` are intentionally excluded from this release commit.

---

## Italiano

La versione 1.0.500 si concentra su FST4/FST4W, correttezza del logging e
usabilita' del waterfall. Include anche alcuni irrigidimenti nati dai test dei
modi weak-signal in laboratorio e in scenari simili all'uso reale.

### Supporto bridge per FST4 e FST4W

- Aggiunto il percorso TX/RX bridge-owned per FST4 e FST4W, con generazione
  waveform, validazione TX e dispatch audio specifico per modo.
- Aggiunta gestione degli slot FST4/FST4W per varianti da 15, 30, 60, 120, 300,
  900 e 1800 secondi dove previste dal suffisso del modo selezionato.
- Aggiunti helper specifici per symbol period, lead-in, payload utile minimo e
  schedulazione manuale TX.
- Estesa la modalita' lab/no-CAT in modo che FST4 e FST4W rispettino il timing
  reale anche nei test automatici.
- Aggiornati i messaggi TX generati: FST4W usa il campo potenza in stile WSPR
  derivato dall'impostazione dBm selezionata.

### Qualita' decode e filtro falsi positivi

- Migliorato il post-processing FST4/FST4W: solo i messaggi realmente
  utilizzabili vengono promossi a UI e servizi secondari.
- Ridotti gli artefatti in presenza di candidati molto forti che generavano
  decodifiche deboli non utilizzabili.
- Aggiunte protezioni per placeholder hash/grid-only non risolti, evitando che
  alimentino DXCC, mappa, PSK Reporter, persistenza, stato worked-before o
  arricchimento QSO.
- Migliorata l'estrazione dei nominativi per evitare che grid locator o
  frammenti placeholder vengano trattati come stazioni valide.

### Logging e filtri

- Corretto il filtro CQ-only: ora e' solo un filtro di presentazione. Le
  decodifiche valide continuano a finire in `ALL.TXT` e nei log di background
  anche quando la UI mostra soltanto i CQ.
- Separato lo stato del backend legacy dai filtri grafici, impedendo la perdita
  dei rapporti non-CQ.
- Nei percorsi legacy FT2/FT8 le righe decode valide vengono salvate prima dei
  filtri UI, proteggendo il logging completo.

### Waterfall e UI

- Il waterfall embedded ora parte da 200 Hz a zoom normale, riducendo lo spazio
  vuoto a sinistra che poteva sembrare un errore grafico.
- Lo stesso limite inferiore a 200 Hz e' applicato anche al waterfall staccato
  e alle relative label di fallback.
- Aggiustati alcuni dettagli UI e lab mode per rendere i nuovi modi a slot
  lungo coerenti con il resto di Decodium4.

### Compatibilita' build

- Le chiamate al planner FFTW threaded sono ora protette dal feature guard
  corretto, cosi' anche build senza supporto FFTW threaded restano valide.
