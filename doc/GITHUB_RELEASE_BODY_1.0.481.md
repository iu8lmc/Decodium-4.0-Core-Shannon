# Decodium 4 FT2 1.0.481

## English

Release highlights (`1.0.480 -> 1.0.481`):

- MSK144 transmitter and timing:
  - added a native MSK144 waveform generator for the bridge audio path: continuous-phase 2-FSK at 2000 baud with tones at audio center +/- 500 Hz.
  - normal 144-symbol and shorthand 40-symbol frames are repeated across the active T/R interval, leaving the final 500 ms idle and applying the reference-style end fade.
  - enabled MSK144 bridge PCM transmission on macOS legacy audio, including period-aware audio cache keys and visual TX duration.
  - added dynamic 5, 10, 15 and 30 second MSK144 periods across TX scheduling, decode collection, display progress and reply-parity selection.
  - restored the expected band defaults: 15 seconds on 6 m / 4 m and 30 seconds on 2 m.
  - late MSK144 starts are deferred to the next valid slot, including legacy PTT paths, preventing a burst from crossing into the peer slot.

- MSK144 decoding, auto-sequencing and logging:
  - pass complete decoder parameters to the native MSK144 decoder, including period, receive frequency, decode depth and tolerance.
  - normalized MSK144 zero UTC worker rows before they are mirrored from the legacy backend, preventing duplicate decodes and unstable auto-sequencing.
  - preserved TX period parity when a stale or synthetic decode is observed.
  - fixed lab station callsign/grid propagation into legacy messages and ADIF records.
  - suppressed a duplicate active-QSO ADIF entry when the legacy backend has already committed the same contact.

- FT2 and FT2-Link reliability:
  - strengthened FT2 CQ semantic validation to reject high-confidence ghost calls and malformed CQ trailing payloads while retaining normal and special-event calls.
  - added deterministic pre-transmission CCA jitter for non-priority FT2-Link traffic, plus queue-aware busy energy handling, to reduce simultaneous transmissions after a clear channel.
  - made runtime lab mode/frequency application atomic so the embedded legacy backend and bridge do not end up on different bands.

- Validation:
  - verified the QML application build and native helper tests.
  - added MSK144 encoder and waveform round-trip coverage through the native decoder for CQ, locator, reports, RR73, 73, shorthand and SWL traffic.
  - validated generated MSK144 waveforms at 5, 10, 15 and 30 second periods.
  - completed a real two-instance BlackHole loopback QSO on 50.260 MHz with separate 2-channel and 16-channel audio routes: CQ, locator, report, R-report, RR73 and final 73 completed in alternating 15-second slots without overlap.

Release assets expected from GitHub Actions:

- `Decodium_1.0.481_Setup_x64.exe`
- Apple Silicon DMGs for Tahoe and Sequoia
- Intel DMGs for Ventura, Sonoma and Sequoia
- `decodium4-ft2-1.0.481-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.481-linux-aarch64.AppImage`
- matching ZIP and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.480 -> 1.0.481`):

- Trasmettitore e timing MSK144:
  - aggiunto un generatore waveform MSK144 nativo per il bridge audio: 2-FSK a fase continua a 2000 baud, con toni al centro audio +/- 500 Hz.
  - i frame normali da 144 simboli e shorthand da 40 simboli vengono ripetuti per l'intervallo T/R attivo; gli ultimi 500 ms restano liberi ed e' applicato un fade finale compatibile con il riferimento.
  - abilitata la trasmissione PCM MSK144 del bridge su macOS con backend audio legacy, inclusi cache TX dipendente dal periodo e durata TX visiva corretta.
  - aggiunti periodi MSK144 dinamici da 5, 10, 15 e 30 secondi per scheduling TX, raccolta decode, avanzamento grafico e scelta della parita' di risposta.
  - ripristinati i default per banda: 15 secondi su 6 m / 4 m e 30 secondi su 2 m.
  - i TX MSK144 troppo tardivi vengono rimandati allo slot successivo anche nel percorso PTT legacy, evitando che il burst invada lo slot del corrispondente.

- Decodifica, auto-sequenza e log MSK144:
  - passati al decoder nativo tutti i parametri utili: periodo, frequenza RX, profondita' e tolleranza.
  - normalizzati i record MSK144 con UTC `000000` del worker prima del mirror dal backend legacy, evitando decode duplicati e auto-sequenze instabili.
  - preservata la parita' TX quando viene osservato un decode vecchio o sintetico.
  - corretta la propagazione di callsign/grid dei profili lab verso messaggi legacy e record ADIF.
  - eliminato il doppio record ADIF del QSO attivo quando il backend legacy ha gia' registrato il collegamento.

- Affidabilita' FT2 e FT2-Link:
  - rafforzata la validazione semantica dei CQ FT2 per rifiutare ghost call ad alta confidenza e payload CQ malformati, mantenendo valide le call normali e special-event.
  - aggiunto jitter CCA deterministico prima della trasmissione del traffico FT2-Link non prioritario e gestione della coda basata sull'energia del canale, per ridurre trasmissioni contemporanee dopo un canale libero.
  - resa atomica l'applicazione di modo/frequenza nei runtime lab, evitando disallineamenti di banda tra bridge e backend legacy integrato.

- Validazione:
  - verificata la build QML e i test helper nativi.
  - aggiunta copertura MSK144 encode/waveform/decode per CQ, locator, rapporti, RR73, 73, shorthand e traffico SWL.
  - verificati waveform MSK144 a 5, 10, 15 e 30 secondi.
  - completato un QSO reale in loopback BlackHole con due istanze su 50.260 MHz e percorsi audio separati 2 canali / 16 canali: CQ, locator, report, R-report, RR73 e 73 finale completati in slot alternati da 15 secondi senza sovrapposizioni.

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.481_Setup_x64.exe`
- DMG Apple Silicon per Tahoe e Sequoia
- DMG Intel per Ventura, Sonoma e Sequoia
- `decodium4-ft2-1.0.481-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.481-linux-aarch64.AppImage`
- relativi ZIP e checksum dove prodotti dal workflow della piattaforma.
