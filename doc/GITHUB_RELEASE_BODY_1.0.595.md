# Decodium 4 FT2 v1.0.595

This fork release carries everything developed after v1.0.590 — FT2 phantom
decodes fixed, the vectorised decoder extended to FT8 with batch decoding and a
recovery pass, and two faults that kept the FT8 deep stage from ever running —
merged with the upstream native SSTV and Apple Silicon work from v1.0.591.

## English (British)

### FT2: phantom callsigns fixed

- v1.0.590 shipped a wide OSD search (order 3, spans 91/48) that tries about
  21400 candidates per word against roughly 600 for the narrow one. A 14-bit CRC
  admits one wrong candidate in 16384, so widening bought correct and false
  decodes in the same proportion: on air it produced about 2.8 invented
  callsigns per cycle. The search is back to narrow.
- The checks were sitting in the single-word decode function, which FT2 no
  longer calls: it uses the batch path, where `nharderror` was computed and
  returned but never used to reject anything. Words flipping 31, 36 and 40+ bits
  reached the decode list while every tuned threshold sat in dead code. The
  batch path now rejects on flipped bits and on coherence with the a-priori
  hypothesis, adjustable through `DECODIUM_LDPC_MAX_HARD` and
  `DECODIUM_LDPC_AP_CHECK`, with `DECODIUM_LDPC_GATE_LOG=1` reporting what is
  rejected and why.
- Added a structural plausibility test on the 77-bit payload inside the OSD
  acceptance loop: message type, callsign structure, token position, field
  ranges. Certain constraints rather than statistical thresholds.

### FT8: the vectorised decoder, and two faults that hid half the work

- Extended the vectorised decoder to FT8 and converted its decode passes to
  batch decoding. The vectoriser works on sixteen words per register lane, so
  decoding one pass at a time left fifteen lanes idle. Passes are now prepared in
  advance, grouped by their `(Keff, maxosd, norder)` triple and decoded a group
  at a time, with identical output verified against the per-pass path.
- Added a recovery pass with the original decoder for candidates the vectorised
  one leaves undecoded, when time remains. The two do not win under the same
  conditions: on a busy 40 m the vectorised decoder finds 250 distinct messages
  against 198, because the original takes about 16 seconds per slot against an
  8 second deadline and is cut off partway through the candidate list; on a quiet
  80 m the original finds four more, all genuine, its exact belief propagation
  beating the min-sum approximation where there is time for both. Running both
  recovers three of those four while keeping the busy-band advantage.
- Fixed an unreachable threshold that disabled the deep follow-up decode
  entirely. It required a remaining budget of at least 7000 ms, but the budget is
  `latestCompleteMs - now - 250` and `latestCompleteMs` is the end of the slot
  plus 6800 ms: the maximum is 6550 ms. The condition could never hold, so the
  deep pass was always discarded, and each discard was read as worker backlog and
  armed a six-slot cooldown. A-priori decoding and depth 4 had never run in FT8.
- Fixed ten settings written to the file's global section but read from the
  active profile's group, so they silently reverted on every restart:
  `Ft8SubpassHarvest` (the GAL button), `Ft2AdaptiveDecode`, `Ft2ApHashCache`,
  `Ft2Conservative`, `Ft2FullDecodeInAutoCq`, `Ft2PartnerMemoryEnabled`,
  `Ft2QuickGiveUpStrong`, `MamMultiStream`, `MamMaxStreams`, `MamCqSlots`.
  Reads now fall back to the global value when the key is not yet in the profile,
  so values already saved are recovered rather than lost.
- Together the last two restore all three FT8 stages per cycle: fast pass, deep
  pass with a-priori decoding at depth 4, and the low-threshold subpass harvest.
  A cycle now runs depth 4 with AP producing 30 decodes against the 16-20 of the
  fast pass alone.

### Waterfall

- Clicking a decoded callsign now calls that station. It previously only moved
  the transmit frequency, and calling required Ctrl+click. The transmit
  frequency can still be set by clicking away from a label, or with Ctrl+click.

### Merged from upstream v1.0.591

- Native SSTV monitoring stability and Apple Silicon build work, plus
  architecture guards that let the vectorised decoder source compile on ARM,
  where it falls back to the scalar implementation.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.595` are the codebase
  downloads for this release.
- The AVX2 decoder is selected at runtime, so the published binaries remain
  usable on CPUs without AVX2, where the original decoder is used instead.
- Restoring the deep and harvest stages increases per-cycle decode work. It fits
  comfortably with the vectorised decoder, but on a machine without AVX2 the
  cycle will be busier than in previous releases.
- Measurements come from off-air recordings on 20, 40 and 80 metres plus live
  sessions, not from a full range of propagation conditions.

## Italiano

### FT2: nominativi fantasma risolti

- La v1.0.590 usava una ricerca OSD larga (ordine 3, span 91/48) che prova circa
  21400 candidati per parola contro i circa 600 della stretta. Un CRC a 14 bit ne
  lascia passare uno sbagliato ogni 16384, quindi allargare comprava decodifiche
  giuste e false nella stessa proporzione: sull'aria produceva circa 2,8
  nominativi inventati per ciclo. La ricerca è tornata stretta.
- I controlli stavano nella funzione di decodifica a parola singola, che FT2 non
  chiama più: usa la via a blocchi, dove `nharderror` veniva calcolato e
  restituito ma non filtrava nulla. Arrivavano in lista parole che ribaltavano
  31, 36 e più di 40 bit, mentre ogni soglia tarata viveva in codice morto. La
  via a blocchi ora rifiuta sui bit ribaltati e sulla coerenza con l'ipotesi a
  priori, regolabili con `DECODIUM_LDPC_MAX_HARD` e `DECODIUM_LDPC_AP_CHECK`, e
  `DECODIUM_LDPC_GATE_LOG=1` riporta cosa viene scartato e perché.
- Aggiunto un test strutturale di plausibilità sui 77 bit del payload dentro il
  ciclo di accettazione dell'OSD: tipo di messaggio, struttura dei nominativi,
  posizione dei token, intervalli dei campi. Vincoli certi, non soglie
  statistiche.

### FT8: il decoder vettorizzato, e due difetti che nascondevano metà del lavoro

- Esteso il decoder vettorizzato a FT8 e convertite a blocchi le sue passate di
  decodifica. Il vettorizzatore lavora su sedici parole per corsia del registro,
  quindi decodificando una passata alla volta quindici corsie restavano ferme. Le
  passate vengono ora preparate in anticipo, raggruppate per terna
  `(Keff, maxosd, norder)` e decodificate un gruppo per volta, con risultato
  verificato identico alla via per passata.
- Aggiunta una passata di recupero con il decoder originale sui candidati che il
  vettorizzato lascia senza decodifica, quando resta tempo. I due non vincono
  nelle stesse condizioni: in 40 metri affollati il vettorizzato trova 250
  messaggi distinti contro 198, perché l'originale impiega circa 16 secondi per
  slot contro una scadenza di 8 e viene troncato a metà della lista dei
  candidati; in 80 metri scarichi è l'originale a trovarne quattro in più, tutte
  autentiche, con la sua propagazione esatta che batte l'approssimazione min-sum
  dove il tempo basta a entrambi. Usarli entrambi ne recupera tre di quelle
  quattro mantenendo il vantaggio in banda piena.
- Corretta una soglia irraggiungibile che disattivava del tutto il decode
  profondo di recupero. Serviva un budget residuo di almeno 7000 ms, ma il budget
  è `latestCompleteMs - adesso - 250` e `latestCompleteMs` è la fine dello slot
  più 6800 ms: il massimo è 6550 ms. La condizione non poteva mai essere
  soddisfatta, quindi la passata profonda veniva sempre scartata, e ogni scarto
  veniva letto come sovraccarico del worker armando un raffreddamento di sei
  slot. Decodifica a priori e profondità 4 non erano mai state eseguite in FT8.
- Corrette dieci impostazioni scritte nella sezione generale del file ma lette
  dal gruppo del profilo attivo, che quindi tornavano ai valori predefiniti a
  ogni riavvio: `Ft8SubpassHarvest` (il pulsante GAL), `Ft2AdaptiveDecode`,
  `Ft2ApHashCache`, `Ft2Conservative`, `Ft2FullDecodeInAutoCq`,
  `Ft2PartnerMemoryEnabled`, `Ft2QuickGiveUpStrong`, `MamMultiStream`,
  `MamMaxStreams`, `MamCqSlots`. La lettura ora ricade sul valore globale quando
  la chiave non è ancora nel profilo, così i valori già salvati vengono
  recuperati invece di andare persi.
- Insieme, le ultime due ripristinano tutti e tre gli stadi FT8 per ciclo:
  passata veloce, passata profonda con decodifica a priori a profondità 4, e
  harvest col subpass a soglia bassa. Un ciclo esegue ora profondità 4 con AP
  producendo 30 decodifiche contro le 16-20 della sola passata veloce.

### Waterfall

- Cliccando un nominativo decodificato ora si chiama quella stazione. Prima il
  clic spostava soltanto la frequenza di trasmissione e per chiamare serviva
  Ctrl+clic. La frequenza si imposta ancora cliccando lontano da un'etichetta,
  oppure con Ctrl+clic.

### Assorbito dall'upstream v1.0.591

- Stabilità del monitoraggio SSTV nativo e lavoro di build per Apple Silicon,
  più le guardie di architettura che permettono al sorgente del decoder
  vettorizzato di compilare su ARM, dove ricade sull'implementazione scalare.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.595` costituiscono i
  download del codebase di questa release.
- Il decoder AVX2 viene scelto a runtime, quindi i binari pubblicati restano
  utilizzabili su CPU senza AVX2, dove viene usato il decoder originale.
- Ripristinare gli stadi profondo e harvest aumenta il lavoro di decodifica per
  ciclo. Ci sta comodamente col decoder vettorizzato, ma su una macchina senza
  AVX2 il ciclo sarà più carico rispetto alle release precedenti.
- Le misure vengono da registrazioni off-air in 20, 40 e 80 metri e da sessioni
  dal vivo, non da tutta la gamma delle condizioni di propagazione.
