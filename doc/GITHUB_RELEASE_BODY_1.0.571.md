# Decodium 4.0 Core Shannon — v1.0.571

## English (British English)

### Highlights

Version 1.0.571 consolidates the FT8 special-callsign work, improves the responsiveness of the Windows decode and Live Map presentation paths, and cleans up a pair of compiler diagnostics in the bundled QCustomPlot source.

### FT8 special callsigns and LightHouse activations

- Generalised FT8 type-4 callsign acceptance so valid non-standard callsigns can pass the same plausibility and emission checks as standard calls.
- Added complete support for portable `/LH` LightHouse activations, including callsigns such as `8A81JK/LH`, `8B81JB/LH`, `8A1AA/LH`, `8A1AAA/LH` and ordinary calls such as `9H1SR/LH`.
- Extended callsign-token rules for the Indonesian special-event shapes with a digit-letter prefix and the `LH` portable designator.
- Updated hash-placeholder, directed-message and DX Cluster peer validation so valid `/LH` calls are not rejected by secondary filters.
- Added full-sequence regression coverage for CQ, grid, reports, R-reports, TU, RRR, RR73 and 73 exchanges, including Indonesian special-event variants and `/LH` forms.

### Windows responsiveness and Live Map updates

- Added short-lived visual back-pressure handling to the Windows decode list model. Heavy or late model steps temporarily reduce the visual update rate without touching decoder, audio or CAT timing.
- Batched Live Map snapshot notifications into bounded event-loop slices, reducing long main-thread notification bursts while preserving map, roster, propagation and statistics updates.
- Improved layout minimums and alignment for the settings and map-operation panels on narrower windows.

### Build quality

- Fixed misleading-indentation diagnostics in the radial and angular polar-axis helpers shipped in QCustomPlot.
- Kept the release version source, generated package metadata and application version aligned at `1.0.571`.

### Validation

- Focused FT8 special-callsign and QSO-sequencer regression tests pass.
- The release assets are produced by the repository's GitHub Actions runners for Windows x64, macOS Apple Silicon, macOS Intel, Linux x86_64 and Linux aarch64.

## Italiano

### Novità principali

La versione 1.0.571 consolida il supporto ai nominativi speciali FT8, migliora la reattività dei percorsi di visualizzazione del decoder su Windows e della Live Map, e corregge due diagnostiche del compilatore presenti nel codice QCustomPlot incluso nel progetto.

### Nominativi speciali FT8 e attivazioni LightHouse

- Generalizzata la validazione dei nominativi FT8 type-4 non standard, così che i nominativi validi possano superare gli stessi controlli di plausibilità e trasmissione dei nominativi standard.
- Aggiunto il supporto completo alle attivazioni portatili `/LH`, inclusi nominativi come `8A81JK/LH`, `8B81JB/LH`, `8A1AA/LH`, `8A1AAA/LH` e nominativi ordinari come `9H1SR/LH`.
- Estese le regole dei token nominativo per le forme speciali indonesiane con prefisso cifra-lettera e per il designatore portatile `LH`.
- Aggiornati i controlli dei placeholder hash, dei messaggi diretti e dei peer DX Cluster, evitando che nominativi `/LH` validi vengano scartati dai filtri secondari.
- Aggiunta una copertura di regressione sull'intera sequenza: CQ, griglia, rapporti, R-report, TU, RRR, RR73 e 73, incluse le varianti indonesiane speciali e `/LH`.

### Reattività Windows e aggiornamenti Live Map

- Aggiunta una gestione temporanea del back-pressure visivo nel modello delle decodifiche Windows. Quando un aggiornamento è lento o arriva in ritardo, viene temporaneamente ridotta la frequenza degli aggiornamenti grafici senza modificare i tempi di decoder, audio o CAT.
- Raggruppate le notifiche degli snapshot della Live Map in porzioni limitate del ciclo eventi, riducendo i blocchi del thread principale e mantenendo gli aggiornamenti di mappa, roster, propagazione e statistiche.
- Migliorati i minimi di layout e l'allineamento dei pannelli impostazioni e operazioni mappa nelle finestre più strette.

### Qualità della compilazione

- Corrette le diagnostiche di indentazione ambigua negli helper degli assi polari radiale e angolare del QCustomPlot incluso.
- Allineati il file della versione, i metadati dei pacchetti e la versione applicativa a `1.0.571`.

### Verifica

- I test mirati di regressione per i nominativi speciali FT8 e le regole del sequencer QSO risultano superati.
- Gli asset della release vengono prodotti dai runner GitHub Actions del repository per Windows x64, macOS Apple Silicon, macOS Intel, Linux x86_64 e Linux aarch64.
