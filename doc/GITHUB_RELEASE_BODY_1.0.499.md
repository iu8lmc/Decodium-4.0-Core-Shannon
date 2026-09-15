# Decodium 4.0 v1.0.499

Version 1.0.499 brings the local fork from 1.0.496 through the
1.0.497/1.0.498 slimming work and adds the next JT4/audio validation layer.
The focus is smaller packages, safer first-run behavior on slower PCs, more
reliable long-slot lab testing and clearer diagnostics when waterfall audio and
decoder audio do not follow the same path.

## Changes from 1.0.496 to 1.0.499

### Packaging and first-run profile

- Added the staged 1.0.497 installer slimming work:
  - removed orphaned codec payload from the installer;
  - introduced the runtime "Slow PC" mode;
  - moved installer content to a more modular component layout.
- Added the 1.0.498 first-start prompt for Slow PC mode, so lower-end Windows
  systems can opt into a lighter runtime profile before the first heavy decode
  session.
- Added the second codec cleanup pass and completed the related UI/i18n updates,
  including the missing Nederlands language entry.

### JT4 transmit, receive and validation

- Added a dedicated JT4 waveform generator with the correct 4.375 baud symbol
  clock and submode-dependent tone spacing for JT4A through JT4G.
- Added bridge TX support for JT4 messages, including validation through the
  existing standard message pipeline.
- Added JT4 to the long synchronous mode scheduler so lab/no-CAT runs preserve
  the 60-second UTC slot timing instead of forcing arbitrary immediate starts.
- Added a bridge-owned JT decode fallback for no-CAT lab sessions, covering
  JT4, JT9 and JT65 when the bridge owns PCM capture and decode.
- Added JT4 acquisition windows at small offsets for on-air and virtual-audio
  cases where the burst reaches the decoder a few seconds away from the ideal
  aperture.
- Added non-zero JT4 decode tolerance defaults, preventing exact-frequency-only
  acquisition during normal frontend dispatch.

### macOS virtual audio and legacy diagnostics

- On macOS, BlackHole and similar virtual devices now prefer the native
  AudioQueue callback path. The previous QAudioSource pull path can still be
  forced with `DECODIUM_MAC_QAUDIO_PULL_FOR_VIRTUAL` for diagnostics.
- Added low-rate legacy PCM diagnostics, reporting sample count, RMS, peak and
  non-zero samples so a visible waterfall can be compared with actual decoder
  input.
- Added low-rate legacy waterfall diagnostics with bin count, min, max, mean and
  non-zero data.
- Long synchronized modes no longer restart audio capture just because a quiet
  flat block is observed. This protects the 60/120-second accumulation required
  by JT4, JT65, JT9, Q65 and WSPR.

### UI and startup polish

- The Settings dialog is now warmed asynchronously after the main visual stage
  is ready and only while the selected mode is in a quiet part of its receive
  period. Opening Setup should therefore be faster without disturbing startup,
  TX or decode timing.
- Mode changes regenerate standard TX messages whenever a valid station
  callsign is available, not only when entering or leaving WSPR.
- Synthetic TESTA/TESTB-style lab callsigns are accepted by the ghost filter in
  controlled lab runs while normal on-air plausibility filtering remains active.

### Tests and developer tools

- Added `test_jt4_roundtrip`, covering JT4A through JT4G waveform generation and
  decode roundtrip.
- Extended the Test Runner workflow so the JT4 decoder, message encoder,
  waveform generator and roundtrip test are built and run on macOS, Linux and
  Windows.
- Added `jt4_wav_decode`, a small utility for testing a mono 16-bit WAV file
  against the JT4 decoder.
- Kept `jt4_compare` behind the reference library guard and fixed printf-size
  conversions for stricter compilers.

## Validation

- Local macOS build of `decodium_qml` was used as the release gate for this
  fork release.
- The new JT4 roundtrip target is part of the cross-platform Test Runner and
  will be executed by GitHub Actions after the push.
- Temporary lab artifacts, WAV scratch data and diagnostic logs were not added
  to the release commit.

---

## Italiano

La versione 1.0.499 porta il fork locale dalla 1.0.496 attraverso il lavoro di
alleggerimento 1.0.497/1.0.498 e aggiunge il livello successivo di validazione
JT4/audio. L'obiettivo e' ridurre i pacchetti, rendere piu' sicuro il primo
avvio sui PC lenti, migliorare i test dei modi a slot lungo e chiarire subito
quando waterfall e decoder non stanno ricevendo lo stesso audio.

### Pacchetti e primo avvio

- Integrato il lavoro 1.0.497 di alleggerimento installer:
  - rimossi payload codec orfani;
  - introdotta la Modalita' PC lento;
  - resa piu' modulare la struttura dei componenti installer.
- Aggiunta in 1.0.498 la proposta di Modalita' PC lento al primo avvio, utile
  per i sistemi Windows meno potenti prima di una sessione di decode pesante.
- Completato il secondo passaggio sui codec e le relative correzioni UI/i18n,
  compresa la voce Nederlands mancante.

### JT4 TX/RX e test

- Aggiunto un generatore waveform JT4 dedicato con symbol clock a 4.375 baud e
  spaziatura toni corretta per i sottos modi JT4A-JT4G.
- Aggiunto il supporto TX JT4 nel bridge e nella validazione dei messaggi
  standard.
- JT4 ora segue lo scheduler dei modi sincroni lunghi: nei test lab/no-CAT la
  trasmissione rispetta lo slot UTC da 60 secondi.
- Aggiunto un fallback di decode bridge-owned per JT4, JT9 e JT65 quando, nei
  test senza CAT, il bridge possiede sia PCM sia decoder.
- Aggiunte finestre di acquisizione JT4 a piccoli offset per casi reali o
  loopback virtuali in cui il burst arriva fuori dall'apertura ideale.
- Impostata una tolleranza JT4 non nulla, evitando acquisizioni limitate alla
  frequenza audio esatta.

### Audio macOS e diagnostica legacy

- Su macOS, BlackHole e dispositivi virtuali simili preferiscono ora il percorso
  nativo AudioQueue. Il percorso QAudioSource pull resta forzabile con
  `DECODIUM_MAC_QAUDIO_PULL_FOR_VIRTUAL` per diagnosi mirate.
- Aggiunta diagnostica PCM legacy a bassa frequenza: campioni, RMS, picco e
  campioni non zero.
- Aggiunta diagnostica waterfall legacy: numero bin, minimo, massimo, media e
  bin non zero.
- I modi sincroni lunghi non riavviano piu' la cattura audio solo per un blocco
  quieto/piatto, preservando l'accumulo necessario a JT4, JT65, JT9, Q65 e
  WSPR.

### UI e startup

- La finestra Setup viene pre-caricata in modo asincrono dopo lo staging visuale
  iniziale e solo in una parte tranquilla del periodo RX. L'apertura di Setup
  risulta piu' rapida senza disturbare TX o decode.
- I messaggi TX standard vengono rigenerati a ogni cambio modo quando e'
  disponibile un nominativo valido.
- I nominativi sintetici da laboratorio tipo TESTA/TESTB sono accettati dal
  filtro anti-ghost solo nel contesto di test controllato.

### Test e strumenti

- Aggiunto `test_jt4_roundtrip` per verificare JT4A-JT4G end-to-end.
- Esteso il Test Runner per compilare ed eseguire i test JT4 su macOS, Linux e
  Windows.
- Aggiunto `jt4_wav_decode` per provare file WAV mono 16-bit contro il decoder
  JT4.
- `jt4_compare` resta protetto dal guard della libreria di riferimento e usa
  conversioni esplicite per compilatori piu' severi.
