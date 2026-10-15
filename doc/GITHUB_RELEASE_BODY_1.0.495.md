# Decodium 4.0 v1.0.495

Version 1.0.495 is a cross-platform responsiveness and resource-management
release. It reduces work performed during startup and decode bursts, keeps the
live decode views bounded and incremental, and avoids unnecessary CAT,
reporting, audio and decoder activity while the application is idle.

## Startup and decoder initialization

- Waterfall, Live Map and their GPU resources are staged after the first
  interactive QML frame on macOS, Windows and Linux.
- Secondary dialogs and panels now use asynchronous QML loaders so opening the
  main window does not construct inactive interfaces.
- Decoder workers are created lazily for the selected mode instead of allocating
  every decoder thread at startup.
- On macOS, a requested legacy RX backend owns the selected mode even while its
  deferred construction is pending. The modern FT8 worker is created only if
  the legacy backend is unavailable and the fallback is actually required.
- DXCC data and worked-before ADIF history are loaded outside the GUI thread.
  World-map cache, audio enumeration, UDP, PSK Reporter, US-state data, LoTW and
  DX Cluster startup are distributed across deferred stages.
- Startup setting changes are coalesced into one save after visual staging. A
  waterfall resize is persisted only when its rounded height really changes.
- QML uses the platform fixed-width font resolved by Decodium, avoiding the
  generic `monospace` alias scan during startup.

## Decode views and GUI responsiveness

- Full Spectrum and Signal RX continue to receive incremental deltas through
  budgeted model updates instead of immediate full-list publication.
- New rows are scheduled for a following event-loop turn, preventing decoder
  callbacks from creating QML delegates in the same GUI frame.
- Pending decode rows use bounded hash indexes for duplicate detection. FT2
  overlapping async batches and RX mirror entries no longer require repeated
  linear scans of the visible history.
- Live decode histories are capped in memory while complete RX history remains
  available through the existing persistence path.
- Signal RX follows the tail after a completed snapshot instead of reacting to
  every intermediate row-count change.
- The Full Spectrum `Clear` command now has a visible hover state, pointer
  cursor and tooltip consistent with the compact and pop-out controls.

## Memory, audio and panadapter

- Idle TX/RX waveform, PCM, FT2-Link, forced-decode and spectrum buffers are
  released after bounded idle time and retained while a transmission or decode
  still needs them.
- TX cache lifetime is refreshed when a reusable waveform is prepared, avoiding
  both premature release and indefinite retention.
- Compact waterfall layouts use a smaller history floor and allocation step,
  reducing resident texture and row storage without dropping visible history.

## CAT, reporting and Windows integration

- Hamlib avoids frequency, mode, VFO and split reads while transmitting. PTT
  confirmation and explicitly enabled power/SWR/ALC telemetry remain active,
  reducing serial-bus contention during TX.
- PSK Reporter uses an event-driven flush deadline only while observations are
  queued; idle reception no longer keeps a permanent reporting heartbeat.
- Windows OmniRig source wiring is enabled only when OmniRig is detected, with
  explicit parameter conversion for current compiler compatibility.

## Validation and packages

- The macOS `decodium_qml` target builds successfully with the release changes.
- Decode-list model, Qt helper and FT2-Link QML adapter tests pass locally.
- Startup smoke tests verify both macOS legacy ownership and the forced modern
  FT8 fallback, with one coalesced settings save and no generic font-alias scan.
- GitHub Actions publishes the Windows x64 installer, macOS Apple Silicon and
  Intel packages, and Linux x86_64 and aarch64 AppImages.

---

## Italiano

La versione 1.0.495 e' una release multipiattaforma dedicata a reattivita' e
gestione delle risorse. Riduce il lavoro eseguito durante avvio e burst di
decode, mantiene limitate e incrementali le viste dei messaggi e impedisce
attivita' CAT, reporting, audio e decoder non necessarie durante l'inattivita'.

### Avvio e inizializzazione decoder

- Waterfall, Live Map e relative risorse GPU vengono costruiti dopo il primo
  frame QML interattivo su macOS, Windows e Linux.
- Dialoghi e pannelli secondari usano Loader QML asincroni, evitando di creare
  interfacce non attive durante l'apertura della finestra principale.
- I worker decoder vengono creati solo per il modo selezionato, invece di
  allocare all'avvio tutti i thread di decodifica.
- Su macOS il backend RX legacy richiesto mantiene la proprieta' del modo anche
  mentre la costruzione differita e' in attesa. Il worker FT8 moderno nasce solo
  se il legacy non e' disponibile e serve realmente il fallback.
- DXCC e storico ADIF worked-before vengono caricati fuori dal thread grafico.
  Cache mappa, enumerazione audio, UDP, PSK Reporter, dati USA, LoTW e DX Cluster
  vengono distribuiti in fasi differite.
- I salvataggi richiesti durante l'avvio vengono riuniti in una sola operazione
  dopo lo staging visuale. L'altezza waterfall viene salvata solo se cambia
  realmente il valore arrotondato.
- QML usa il font fixed-width risolto da Decodium, evitando la scansione
  dell'alias generico `monospace` durante l'avvio.

### Viste decode e fluidita' GUI

- Full Spectrum e Signal RX ricevono delta incrementali tramite aggiornamenti
  con budget, senza pubblicare immediatamente ogni lista completa.
- Le nuove righe vengono applicate nel turno successivo dell'event loop, cosi'
  il callback decoder non crea delegate QML nello stesso frame grafico.
- Le righe in attesa usano indici hash limitati per il deduplica. I batch FT2
  asincroni sovrapposti e il mirror RX non eseguono piu' scansioni lineari
  ripetute della cronologia visibile.
- Le cronologie decode live hanno un limite in memoria; lo storico RX completo
  resta disponibile attraverso il percorso di persistenza esistente.
- Signal RX segue la coda al termine dello snapshot, non a ogni variazione
  intermedia del numero di righe.
- Il comando `Clear` di Full Spectrum dispone ora di hover visibile, cursore e
  tooltip coerenti con i controlli compact e pop-out.

### Memoria, audio e panadapter

- Buffer waveform, PCM, FT2-Link, forced decode e spectrum non piu' utilizzati
  vengono rilasciati dopo un timeout limitato, ma restano disponibili durante
  trasmissioni o decode ancora attivi.
- La durata della cache TX viene rinnovata quando una waveform riutilizzabile e'
  preparata, evitando sia rilasci prematuri sia permanenza indefinita.
- I layout waterfall compatti usano una soglia minima e passi di allocazione
  inferiori, riducendo texture e righe residenti senza perdere storia visibile.

### CAT, reporting e integrazione Windows

- Hamlib evita letture di frequenza, modo, VFO e split durante la trasmissione.
  Conferma PTT e telemetria PWR/SWR/ALC esplicitamente abilitata restano attive,
  riducendo la contesa sul bus seriale durante TX.
- PSK Reporter usa una scadenza di flush solo quando sono presenti osservazioni
  in coda; in RX inattivo non mantiene piu' un heartbeat permanente.
- Su Windows il sorgente OmniRig viene collegato solo quando OmniRig e' rilevato,
  con conversione esplicita dei parametri per i compilatori correnti.

### Verifica e pacchetti

- Il target macOS `decodium_qml` compila correttamente con le modifiche della
  release.
- I test del modello decode, degli helper Qt e dell'adapter QML FT2-Link passano
  localmente.
- Gli smoke test di avvio verificano sia il percorso legacy macOS sia il fallback
  FT8 moderno forzato, con un solo salvataggio impostazioni e senza scansione
  degli alias font generici.
- GitHub Actions pubblica installer Windows x64, pacchetti macOS Apple Silicon e
  Intel e AppImage Linux x86_64 e aarch64.
