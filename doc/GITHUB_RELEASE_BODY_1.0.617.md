# Decodium 4 FT2 v1.0.617

## English (UK)

### Changes since v1.0.616

- FT8: fixed a bug that produced fabricated "echo" CQ decodes in the slot
  immediately after a real one. Three history-replay mechanisms in
  `Detector/FtxFt8Stage4.cpp` (which re-test a recently-heard station's
  message against the current slot's signal instead of decoding blind)
  checked the message's age and its closeness in frequency, but never
  whether the current slot actually falls on that station's transmit
  parity. FT8 stations only repeat every 30 seconds, always on the same
  pair of seconds (:00/:30 or :15/:45); an age that is a multiple of 15 but
  not of 30 is physically impossible. Added `ft8_utc_same_transmit_parity()`
  and required it in all three call sites. Measured before/after on the
  same 510-slot real-air recording, same settings: fabricated CQ echoes
  went from 66 to 1 (2.4% down to 0.04% of all CQ decodes, the residual is
  in line with ordinary noise), while legitimate history replays were
  essentially untouched (2333 -> 2332): the fix removes only the physically
  impossible cases.
  This fix came out of an in-depth on-air measurement against a second FT8
  decoder on the same audio, which also turned up two other issues not yet
  fixed: the local decode archive drops a majority of directed (non-CQ)
  messages while keeping every CQ, and the callsign-sanity filter
  ("ghost" filter) rejects some legitimate composite callsigns (e.g.
  `IH9/IT9JUI`) together with the fabricated ones it is meant to catch.
  Both are open follow-ups.

This release is published with the source code and platform packages built by
the GitHub Actions runners: Windows x64 executable, macOS Apple Silicon and
Intel DMGs, and Linux x86_64 and aarch64 AppImages.

## Italiano

### Modifiche dalla v1.0.616

- FT8: corretto un bug che produceva decodifiche CQ fabbricate come "eco"
  nello slot subito dopo quello vero. Tre meccanismi di replay dallo
  storico in `Detector/FtxFt8Stage4.cpp` (che riverificano il messaggio di
  una stazione sentita di recente contro il segnale dello slot attuale
  invece di decodificare alla cieca) controllavano l'età del messaggio e la
  vicinanza in frequenza, ma mai se lo slot attuale cadesse davvero sulla
  parità di trasmissione di quella stazione. Una stazione FT8 ripete solo
  ogni 30 secondi, sempre sulla stessa coppia di secondi (:00/:30 oppure
  :15/:45): un'età multipla di 15 ma non di 30 è fisicamente impossibile.
  Aggiunta `ft8_utc_same_transmit_parity()`, richiesta in tutti e tre i
  punti. Misurato prima e dopo sulla stessa registrazione reale di 510 slot,
  stesse impostazioni: gli eco di CQ fabbricati sono passati da 66 a 1
  (dal 2,4% allo 0,04% di tutte le decodifiche CQ, il residuo è in linea
  col rumore ordinario), mentre le repliche legittime dallo storico sono
  rimaste praticamente intatte (2333 -> 2332): la correzione toglie solo i
  casi fisicamente impossibili.
  Questa correzione nasce da una misura approfondita in aria contro un
  secondo decoder FT8 sullo stesso audio, che ha fatto emergere anche altri
  due problemi non ancora corretti: l'archivio locale delle decodifiche
  scarta la maggioranza dei messaggi diretti (non-CQ) pur conservando tutti
  i CQ, e il filtro di validità dei nominativi ("ghost") respinge insieme
  ai fantasmi anche alcuni nominativi composti legittimi (per esempio
  `IH9/IT9JUI`). Restano entrambi da correggere.

Questa release viene pubblicata con il codice sorgente e i pacchetti prodotti
dai runner GitHub Actions: eseguibile Windows x64, DMG macOS Apple Silicon e
Intel, AppImage Linux x86_64 e aarch64.
