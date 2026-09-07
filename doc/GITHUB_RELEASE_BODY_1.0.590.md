# Decodium 4 FT2 v1.0.590

This fork release adds a vectorised LDPC decoder for FT2, a gate that keeps
false decodes out of the decode list, a corrected sign in the min-sum branch of
the shared LDPC decoder, and a more predictable click on the waterfall.

## English (British)

### v1.0.590: faster FT2 decoding and no more phantom callsigns

- Added `fastldpc`, a vectorised LDPC decoder used by the FT2 decode path. The
  min-sum kernel is written with explicit AVX2 intrinsics and works on sixteen
  int16 lanes per register, because the min1/min2/argmin control flow defeats
  automatic vectorisation on both GCC and Clang. FT2 decode passes that took
  over a second now complete in tens of milliseconds, leaving the rest of the
  cycle free. Only the one source file is built with `-mavx2`; on a CPU without
  AVX2, or for a codeword length other than 174/91, the decoder falls back to
  the original one at runtime, so the binary stays portable.
- Added a Fast LDPC toggle to the toolbar, next to GAL. It switches the FT2
  decoder without restarting and the choice is persisted.
- Added a gate that rejects LDPC candidates which close the syndrome by chance.
  The 14-bit CRC that guards a decoded word admits roughly one candidate in
  16384, and an FT2 cycle tries many thousands, so a steady trickle of
  well-formed but invented callsigns reached the decode list — with an empty
  band they were the only entries. The gate measures how much of the received
  soft-decision energy disagrees with the decoded word, and how many bits the
  word has to flip; bits already known from an a-priori hypothesis are excluded
  from that measure, since their large magnitudes would otherwise mask it.
  Thresholds are adjustable through `DECODIUM_LDPC_ND_MAX` and
  `DECODIUM_LDPC_MAX_HARD`.
- Fixed the sign of the min-sum branch in the shared LDPC decoder. It produced
  messages with the sign opposite to the exact branch directly above it, which
  computes `2*platanh(-tmn)`. On synthetic words at Eb/N0 1 dB the correction is
  worth about 3% more correct decodes. FT8 reaches this branch whenever OSD
  depth is below the exact-BP threshold, and FT2 always does.
- Changed the waterfall so that clicking a decoded callsign calls that station.
  It previously only moved the transmit frequency, and calling required
  Ctrl+click, which is not discoverable. The transmit frequency can still be set
  by clicking the waterfall away from a label, or with Ctrl+click on one.
- Fixed the QML sync step so an in-source build no longer deletes the project's
  own `qml/` directory: the step removed the destination and copied the source
  onto itself when the two were the same path.
- Fixed two `QFile::open` calls in the MAP65 window that Qt 6.11 marks
  `nodiscard`, which broke the build under `-Werror`. Neither result was checked
  before and reading or writing an unopened `QFile` is already a no-op, so the
  behaviour is unchanged.

### Packaging and compatibility

- GitHub's generated source archives for tag `v1.0.590` are the codebase
  downloads for this release.
- Release workflows publish the unsigned Windows x64 installer, Linux Qt 6.11
  AppImages for x86_64 and aarch64, and macOS DMGs for the supported Apple
  Silicon and Intel runner targets, each with a SHA-256 checksum where supplied
  by the workflow.
- The AVX2 decoder is selected at runtime, so the published binaries remain
  usable on CPUs without AVX2, where the original decoder is used instead.
- The decode gate has been calibrated against real on-air traffic and synthetic
  vectors, but not yet across a full range of weak-signal band conditions. If
  marginal decodes appear to be missing, raise `DECODIUM_LDPC_MAX_HARD` and
  report the value that restores them.

## Italiano

### v1.0.590: decodifica FT2 più veloce e niente più nominativi fantasma

- Aggiunto `fastldpc`, decoder LDPC vettorizzato usato dal percorso di decodifica
  FT2. Il nucleo min-sum è scritto con intrinseci AVX2 espliciti e lavora su
  sedici corsie int16 per registro, perché il controllo di flusso di
  min1/min2/argmin impedisce la vettorizzazione automatica sia a GCC sia a
  Clang. Passate di decodifica FT2 che superavano il secondo ora si chiudono in
  decine di millisecondi, lasciando libero il resto del ciclo. Solo quel file
  viene compilato con `-mavx2`; su una CPU senza AVX2, o per una lunghezza di
  parola diversa da 174/91, il decoder ricade a runtime su quello originale,
  quindi il binario resta portabile.
- Aggiunto l'interruttore Fast LDPC nella barra strumenti, accanto a GAL.
  Commuta il decoder FT2 senza riavviare e la scelta viene ricordata.
- Aggiunto un filtro che scarta i candidati LDPC che azzerano la sindrome per
  caso. Il CRC a 14 bit che protegge una parola decodificata ne lascia passare
  circa uno ogni 16384, e un ciclo FT2 ne prova molte migliaia: ne usciva un
  flusso costante di nominativi ben formati ma inventati, che a banda vuota
  erano le uniche righe presenti. Il filtro misura quanta energia della
  decisione morbida ricevuta è in disaccordo con la parola decodificata e quanti
  bit la parola deve ribaltare; i bit già noti per ipotesi a priori sono esclusi
  dalla misura, perché altrimenti le loro ampiezze elevate la mascherano.
  Le soglie si regolano con `DECODIUM_LDPC_ND_MAX` e `DECODIUM_LDPC_MAX_HARD`.
- Corretto il segno del ramo min-sum nel decoder LDPC condiviso. Produceva
  messaggi di segno opposto a quello del ramo esatto immediatamente sopra, che
  calcola `2*platanh(-tmn)`. Su parole sintetiche a Eb/N0 1 dB la correzione vale
  circa il 3% di decodifiche corrette in più. FT8 raggiunge questo ramo ogni
  volta che la profondità OSD è sotto la soglia del BP esatto, FT2 sempre.
- Cambiato il waterfall: cliccando un nominativo decodificato ora si chiama quella
  stazione. Prima il clic spostava soltanto la frequenza di trasmissione e per
  chiamare serviva Ctrl+clic, che non si indovina. La frequenza di trasmissione
  si imposta ancora cliccando il waterfall lontano da un'etichetta, oppure con
  Ctrl+clic su una di esse.
- Corretto il passo di sincronizzazione dei QML, che in una compilazione dentro
  i sorgenti cancellava la cartella `qml/` del progetto: rimuoveva la
  destinazione e ricopiava la sorgente su sé stessa quando i due percorsi
  coincidevano.
- Corrette due chiamate `QFile::open` nella finestra MAP65 che Qt 6.11 marca
  `nodiscard`, cosa che rompeva la compilazione con `-Werror`. L'esito non era
  controllato nemmeno prima e leggere o scrivere su un `QFile` non aperto è già
  un'operazione nulla, quindi il comportamento non cambia.

### Packaging e compatibilità

- Gli archivi sorgente generati da GitHub per il tag `v1.0.590` costituiscono i
  download del codebase di questa release.
- I workflow pubblicano l'installer Windows x64 non firmato, le AppImage Linux
  Qt 6.11 x86_64 e aarch64 e i DMG macOS per i runner Apple Silicon e Intel
  supportati, con checksum SHA-256 dove previsto dal workflow.
- Il decoder AVX2 viene scelto a runtime, quindi i binari pubblicati restano
  utilizzabili su CPU senza AVX2, dove viene usato il decoder originale.
- Il filtro dei decode è stato tarato su traffico reale e su vettori sintetici,
  ma non ancora su tutta la gamma di condizioni di segnale debole. Se dovessero
  mancare decodifiche marginali, alzare `DECODIUM_LDPC_MAX_HARD` e segnalare il
  valore che le ripristina.
