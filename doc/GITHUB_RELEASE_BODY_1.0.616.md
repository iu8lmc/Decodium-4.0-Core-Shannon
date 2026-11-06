# Decodium 4 FT2 v1.0.616

## English (UK)

### Changes since v1.0.615

- FT8: the learned LDPC gate (the anti-phantom classifier that judges every
  candidate that passes the CRC-14) is now **on by default**; FT2 keeps it
  opt-in. Measured on a 2 h 07 min real-air recording sliced into 510 FT8
  slots and decoded slot by slot with the same binary: rows from callsigns
  never seen in a 1.35-million-decode archive went from **894 to 19** (and 16
  of those 19 are genuine rare stations), i.e. about 40x fewer fabricated
  rows; in the configuration closest to the live application (station
  history and predictive decoding active) from 971 to 20. The cost, measured
  on the same audio: **2.4-3.6% fewer true decodes**, almost all weak (median
  -17 dB) and in collisions. A wider threshold was measured and **not**
  adopted (it recovers a third of the lost weak rows while multiplying the
  fabricated ones by six); the measurement knob `DECODIUM_LDPC_GATE_DELTA`
  stays available, default 0 = bit-identical. A separate, wider validation
  set (55 messages, ~3 M candidates) puts the classifier's true-candidate
  acceptance nearer 80-85% than the 91% quoted at training time, weaker on
  compound callsigns (/P, /QRP); the weights are unchanged and the on-air
  cost above is the number that matters. `DECODIUM_LDPC_GATE=0` turns the
  gate off.
- FT8/FT2: two fixes from a user report on v1.0.615 ("stations calling me
  while I was only listening", "signal reports of +46 and higher"):
  - The message unpacker now rejects signal reports no transmitter can
    produce (the codepoints for "+50" and everything above +49 or below -50
    in both the plain and the "TU" variants). These were wrong codewords that
    had slipped through the CRC; 4 of the 894 phantoms in the bench.
  - The a-priori hypotheses on your own callsign (AP types 2..6, "MyCall ???
    ???" and following) are suspended while the station has not transmitted
    for three minutes and is not in a QSO: in pure listening nobody can be
    calling you, so those hypotheses could only fabricate rows with your
    callsign in front. Types 1 (CQ), 7 (heard stations) and 8 (whole
    message) are untouched. **Verification level:** archive evidence only;
    the bench could not exercise it (zero rows either way in 510 slots).
    Confirm on air; a rare cold call is delayed to its first repeat.
- FT8 and FT2: predictive decoding ("type 8", exact verification of a whole
  message heard two slots earlier at the same frequency instead of guessing
  the bits through LDPC/OSD) is now **on by default**. FT2: a **threshold**
  gain of +3.0 dB on the bench (50% point from -16.6 to -19.6 dB; most
  traffic sits well above the threshold and gains nothing), 0 false in 541
  offline verifications; on air, an overnight session gave 649 confirmations
  out of 87 982 verifications with no false observed. FT8: threshold gain
  +4.4 dB on the bench, not yet quantified on air. On the 510-slot
  recording with history active it produced 142 rows, none in the slot
  parity where a repeat is impossible, all with locators and QSO pairs
  consistent with the same station's non-AP decodes. `DECODIUM_FT8_AP_MSG=0`
  / `DECODIUM_FT2_AP_MSG=0` turn it off.
- FT2: energy accumulation across repeated slots of the same station
  (4-GFSK symbol spectra summed per hypothesis, weighted by noise) to decode
  where a single slot is not enough: +1.5/+2.8/+3.6 dB at 2/3/4 slots on a
  clean bench, zero false over dozens of series. **Opt-in and experimental**:
  new switch in Settings > TX just below "Conservative FT2", off by default
  and not persisted across restarts; slot identity comes from the real clock
  (3.75 s buckets) so it works with the asynchronous decoder. Never confirmed
  on real traffic. Note that with "Hold Tx Freq" forced off in FT2 an
  unanswered CQ hops +-25 Hz between repeats, so accumulation mainly helps a
  QSO already under way.
- FT2: fixed a silent discard of the decodes recovered by the opt-in rescue
  (local and drift-rate search): the recovered row was logged but never
  delivered; the drift rescue now has its own budget so
  `DECODIUM_FT2_DRIFT_SEARCH=1` alone is enough. Sweep at 4 SNR x 9 drift
  rates x 15 seeds: -14 dB 50.4% -> 82.2%, never a regression. Still opt-in.
- Tooling: `tests/ft8_stage_compare --ap-mycall`, `tests/ft8_gate_dump
  --relax`, `DECODIUM_LDPC_GATE_DELTA`. The phantom bench itself (real
  recording, oracle of known callsigns, history-enabled single-process
  variant, consistency and slot-parity checks on history-based decodes) lives
  in the lab folder outside the repository.

This release is published with the source code and platform packages built by
the GitHub Actions runners: Windows x64 executable, macOS Apple Silicon and
Intel DMGs, and Linux x86_64 and aarch64 AppImages.

## Italiano

### Modifiche dalla v1.0.615

- FT8: il gate LDPC appreso (il classificatore anti-fantasmi che giudica ogni
  candidato che passa la CRC-14) è ora **acceso di default**; FT2 lo tiene
  opzionale. Misurato su una registrazione reale di 2 h 07 min affettata in
  510 slot FT8 e decodificata slot per slot con lo stesso binario: le righe
  da nominativi mai visti in un archivio di 1,35 milioni di decodifiche sono
  passate da **894 a 19** (e 16 di quelle 19 sono stazioni rare vere), cioè
  circa 40 volte meno righe fabbricate; nella configurazione più vicina
  all'applicazione in aria (storico delle stazioni e decodifica predittiva
  attivi) da 971 a 20. Il costo, misurato sullo stesso audio: **2,4-3,6% di
  decodifiche vere in meno**, quasi tutte deboli (mediana -17 dB) e in
  collisione. Una soglia più larga è stata misurata e **non** adottata
  (recupera un terzo delle righe deboli perse ma sestuplica quelle
  fabbricate); resta la manopola di misura `DECODIUM_LDPC_GATE_DELTA`,
  default 0 = bit-identico. Un dataset di validazione più largo (55
  messaggi, ~3 M candidati) colloca l'accettazione dei candidati veri più
  vicina all'80-85% che al 91% dichiarato in addestramento, con un punto
  debole sui nominativi composti (/P, /QRP); i pesi sono invariati e il
  numero che conta è il costo in aria qui sopra. `DECODIUM_LDPC_GATE=0`
  spegne il gate.
- FT8/FT2: due correzioni da una segnalazione utente sulla v1.0.615
  ("stazioni che mi chiamano mentre ascoltavo soltanto", "rapporti di +46 e
  oltre"):
  - Lo spacchettatore dei messaggi respinge ora i rapporti che nessun
    trasmettitore può produrre (i codepoint di "+50" e tutto ciò che sta
    sopra +49 o sotto -50, sia nella variante semplice sia in quella "TU").
    Erano parole sbagliate passate dalla CRC; 4 dei 894 fantasmi del banco.
  - Le ipotesi a priori sul proprio nominativo (tipi AP 2..6, "MyCall ???
    ???" e seguenti) vengono sospese quando la stazione non trasmette da tre
    minuti e non è in QSO: in ascolto puro nessuno può starti chiamando,
    quindi quelle ipotesi potevano solo fabbricare righe col tuo nominativo
    davanti. I tipi 1 (CQ), 7 (stazioni sentite) e 8 (messaggio intero) non
    sono toccati. **Livello di verifica:** solo evidenza d'archivio; il banco
    non ha potuto esercitarla (zero righe in entrambi i casi su 510 slot).
    Da confermare in aria; una rara chiamata a freddo viene ritardata alla
    prima ripetizione.
- FT8 e FT2: la decodifica predittiva ("tipo 8", verifica esatta di un
  messaggio intero sentito due slot prima alla stessa frequenza, invece di
  indovinare i bit tramite LDPC/OSD) è ora **accesa di default**. FT2:
  guadagno **di soglia** +3,0 dB al banco (punto del 50% da -16,6 a
  -19,6 dB; la maggior parte del traffico sta ben sopra la soglia e non
  guadagna nulla), 0 falsi su 541 verifiche offline; in aria, una sessione
  notturna ha dato 649 conferme su 87 982 verifiche senza falsi osservati.
  FT8: guadagno di soglia +4,4 dB al banco, non ancora quantificato in aria. Sulla registrazione da 510
  slot con lo storico attivo ha prodotto 142 righe, nessuna nella parità di
  slot in cui una ripetizione è impossibile, tutte con locatori e coppie di
  QSO coerenti con le decodifiche senza a priori della stessa stazione.
  `DECODIUM_FT8_AP_MSG=0` / `DECODIUM_FT2_AP_MSG=0` la spengono.
- FT2: accumulo di energia fra slot ripetuti della stessa stazione (spettri
  di simbolo 4-GFSK sommati per ipotesi, pesati per rumore) per decodificare
  dove il singolo slot non basta: +1,5/+2,8/+3,6 dB a 2/3/4 slot su un banco
  pulito, zero falsi su decine di serie. **Opzionale e sperimentale**: nuovo
  interruttore in Impostazioni > TX appena sotto "Conservative FT2", spento
  di default e non persistente fra riavvii; l'identità di slot viene
  dall'orologio reale (bucket da 3,75 s) per convivere col decoder asincrono.
  Mai confermato su traffico reale. Nota: con "Hold Tx Freq" forzato spento
  in FT2 un CQ senza risposta salta di +-25 Hz fra una ripetizione e l'altra,
  quindi l'accumulo aiuta soprattutto un QSO già avviato.
- FT2: corretto lo scarto silenzioso delle decodifiche recuperate dal rescue
  opzionale (locale e ricerca del tasso di deriva): la riga recuperata veniva
  loggata ma mai consegnata; il rescue di deriva ha ora un budget proprio,
  così `DECODIUM_FT2_DRIFT_SEARCH=1` da solo basta. Sweep 4 SNR x 9 derive x
  15 semi: -14 dB 50,4% -> 82,2%, mai un peggioramento. Resta opzionale.
- Strumenti: `tests/ft8_stage_compare --ap-mycall`, `tests/ft8_gate_dump
  --relax`, `DECODIUM_LDPC_GATE_DELTA`. Il banco dei fantasmi (registrazione
  reale, oracolo dei nominativi noti, variante a processo unico con lo
  storico, controlli di coerenza e di parità di slot sulle decodifiche dallo
  storico) vive nella cartella di laboratorio fuori dal repository.

Questa release viene pubblicata con il codice sorgente e i pacchetti prodotti
dai runner GitHub Actions: eseguibile Windows x64, DMG macOS Apple Silicon e
Intel, AppImage Linux x86_64 e aarch64.
