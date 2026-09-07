# Decodium 4 FT2 v1.0.576

This cumulative release contains the DecoPort remote-radio work and the
follow-up fixes delivered from v1.0.572 through v1.0.576.

## English (British)

### Changes since v1.0.572

#### v1.0.573 — a closed panel says where it went

- The DX-Pedition workspace now counts closed panels in the tactical bar, for
  example `PANELS 1`, and makes the recovery path visible.
- Panel-header controls have explanatory tooltips, including the instruction
  to reopen a closed panel from `PANELS`.
- The DecoPort window now contains a SECURITY section for setting, changing or
  removing the password without reinstalling. The password itself is never
  displayed or stored; only its derived key is kept.
- Publishing remains disabled until a password exists. Removing or changing a
  password stops the gateway or disconnects clients using the old key, with the
  effect made explicit in the window.
- The narrow-window tactical header scales its frequency display more safely,
  and its controls use stable delegate IDs rather than fragile `parent` paths.
- DecoPort sources are present in both application targets, fixing the classic
  target's missing-link integration.

#### v1.0.574 — using the remote radio, not just watching it

- `USE THIS RADIO` makes a discovered remote radio the radio used by this
  application.
- Remote received audio is delivered to the decoder; the local capture device
  is released so two sources cannot be mixed into one buffer.
- Frequency follows in both directions: tuning locally moves the remote radio,
  and tuning the remote radio updates the local display.
- The remote radio is requested in `DIGU` where required. The radio mode and
  the application's FT8/FT4 mode remain deliberately separate concepts.
- `LISTEN` provides a separate monitor path. When a sound card cannot open a
  12 kHz monitor stream, the monitor falls back to 48 kHz without altering the
  decoder samples.
- A remote radio is not re-published while it is in use, preventing accidental
  rebroadcast loops.
- `tools/decoport_probe.py --serve` can emulate a gateway, accept tuning
  commands and stream a test tone for end-to-end receiver-side checks.
- Remote transmit audio and PTT remain intentionally unavailable; transmission
  stays on the local radio and the window reports that limitation.

#### v1.0.575 — filling a buffer is not decoding

- Taking a remote radio into use now starts the complete receive path: decoder,
  period timer and spectrum, before releasing the local capture device.
- The period timer no longer remains stopped merely because local monitoring is
  disabled while the remote radio supplies the samples.
- The audio watchdog and band-change re-arm no longer reopen the local capture
  device while a remote radio is active.

#### v1.0.576 — the remote sink is created on every valid path

v1.0.575 correctly stopped `startAudioCapture()` from reopening the local sound
card for remote audio, but the audio sink itself was still created inside that
function. On a computer that never opened local capture, the sink did not exist
and every injected remote frame was discarded.

The sink creation now has its own function. It is called both from the normal
local-capture path and before the remote-capture guard returns. Selecting a
remote radio also requests the sink directly, including when monitoring is
delegated to the legacy backend. Remote samples therefore have a live
destination regardless of which valid connection path was taken.

### Validation and compatibility

The release notes and source tree include the complete v1.0.572–v1.0.576
history. The application targets compile and link with the DecoPort sources,
and the DecoPort probe provides a repeatable local test source for the remote
radio protocol. Remote transmit audio and PTT are not part of this release.

## Italiano

### Modifiche dalla v1.0.572

#### v1.0.573 — un pannello chiuso dice dov'è andato

- Il workspace DX-Pedition conta ora i pannelli chiusi nella barra tattica, per
  esempio `PANELS 1`, e rende visibile il modo per recuperarli.
- I comandi nell'intestazione dei pannelli hanno tooltip esplicativi, compresa
  l'indicazione di riaprire un pannello da `PANELS`.
- La finestra DecoPort ha ora una sezione SICUREZZA per impostare, cambiare o
  rimuovere la password senza reinstallare. La password non viene mai mostrata
  né salvata: viene conservata solo la chiave derivata.
- La pubblicazione resta disabilitata finché non esiste una password. La
  rimozione o il cambio della password ferma il gateway o disconnette i client
  che usano la vecchia chiave, e la finestra lo comunica esplicitamente.
- L'intestazione tattica si adatta meglio alle finestre strette e i controlli
  usano ID stabili dei delegate invece di percorsi fragili basati su `parent`.
- I sorgenti DecoPort sono presenti in entrambi i target dell'applicazione,
  risolvendo l'integrazione mancante del target classico.

#### v1.0.574 — usare la radio remota, non solo guardarla

- `USE THIS RADIO` rende la radio remota individuata la radio usata da questa
  applicazione.
- L'audio ricevuto remoto viene consegnato al decoder; la cattura locale viene
  rilasciata per impedire che due sorgenti finiscano nello stesso buffer.
- La frequenza segue nei due versi: sintonizzare localmente sposta la radio
  remota e sintonizzare la radio remota aggiorna il display locale.
- Quando serve, alla radio remota viene richiesto `DIGU`. Il modo della radio e
  il modo FT8/FT4 dell'applicazione restano volutamente concetti separati.
- `LISTEN` offre un ascolto distinto. Se la scheda audio non apre un flusso di
  monitor a 12 kHz, l'ascolto passa a 48 kHz senza modificare i campioni inviati
  al decoder.
- Una radio remota in uso non viene ripubblicata, evitando ritrasmissioni o
  loop accidentali.
- `tools/decoport_probe.py --serve` può simulare un gateway, accettare comandi
  di sintonia e trasmettere un tono di prova per verifiche locali del lato
  ricevente.
- L'audio remoto di trasmissione e il PTT restano volutamente non disponibili:
  la trasmissione rimane sulla radio locale e la finestra lo dichiara.

#### v1.0.575 — riempire un buffer non è decodificare

- L'uso di una radio remota avvia ora l'intero percorso di ricezione: decoder,
  timer di periodo e spettro, prima di rilasciare la cattura locale.
- Il timer di periodo non resta più fermo solo perché il monitor locale è
  disabilitato mentre i campioni arrivano dalla radio remota.
- Il watchdog audio e il riarmo dopo un cambio banda non riaprono più la cattura
  locale mentre una radio remota è attiva.

#### v1.0.576 — il sink remoto viene creato in ogni percorso valido

La v1.0.575 impediva correttamente a `startAudioCapture()` di riaprire la scheda
audio locale per l'audio remoto, ma il sink audio veniva ancora creato dentro
quella funzione. Su un computer che non aveva mai aperto la cattura locale il
sink quindi non esisteva e ogni frame remoto iniettato veniva scartato.

La creazione del sink ha ora una funzione propria. Viene chiamata sia dal
percorso normale della cattura locale sia prima del ritorno della guardia per la
cattura remota. Anche la selezione di una radio remota richiede direttamente il
sink, compreso il caso in cui il monitor venga delegato al backend legacy. I
campioni remoti hanno quindi una destinazione attiva qualunque sia il percorso
valido seguito dal collegamento.

### Verifiche e compatibilità

Le note di release e il codice comprendono l'intera cronologia dalla v1.0.572
alla v1.0.576. I target dell'applicazione compilano e linkano con i sorgenti
DecoPort, mentre il probe DecoPort fornisce una sorgente locale ripetibile per
provare il protocollo radio remoto. L'audio remoto di trasmissione e il PTT non
fanno parte di questa release.
