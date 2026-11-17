# Decodium 4 FT2 1.0.1008

## English (UK)

Release 1.0.1008 improves FT8 sensitivity on weak signals where the computer
has CPU to spare, and repairs most of the continuous-integration builds. The
on-air format is unchanged: LDPC(174,91) matrix, CRC-14, message packing and
waveform are not touched. All changes are on the receive side.

### FT8 decoding

- **BICM-ID, second version.** The iterative demodulation pass now builds its
  feedback from the mean extrinsic information of the five words that have
  just failed (no extra probe decode), runs three rounds, and is tried only
  on candidates with at least 10 correct Costas symbols. It replaces the first
  version wherever BICM-ID runs, i.e. where the adaptive lever governor grants
  it.
- **Phase-trellis demodulator.** One more metric round, tried only after every
  other attempt on a candidate has failed: a forward-backward demodulator that
  tracks the carrier phase, with the Costas symbols as pilots and the BICM-ID
  feedback as a prior. It is also granted by the governor, so it runs only
  after several slots well inside the time budget and is withdrawn at the
  first slot that gets close to it.
- **Measured** on 230 synthetic slots never used for tuning (AWGN, drift,
  phase random walk, fading, QRM), depth 3, levers granted:

  | | 1.0.1007 | 1.0.1008 |
  |---|---|---|
  | Decodes | 1735 | 2331 (+34%) |
  | False decodes, slots with signals | 2 | 1 |
  | False decodes, 100 noise-only slots | 0 | 0 |

  The gain is on weak signals (at -22 dB from about one message in five to
  more than one in two); strong signals were already decoded. BICM-ID alone
  accounts for 1735 to 1864, the trellis round for 1864 to 2331. The new
  BICM-ID takes 29-48% less decode time than the first version; the trellis
  round adds about 14% (unpaired measurement on a shared machine).
- **Where the levers are not granted** (slower computers) the decode lists are
  identical to 1.0.1007 on the regression set (8 recordings x 5 profiles). The
  LDPC ordered-statistics search is about 20% faster per word, about 7% on the
  whole FT8 decoder at depth 3, with bit-identical output.
- **Switches.** `DECODIUM_FT8_BICM_V2=0` restores the first BICM-ID version;
  `DECODIUM_FT8_TRELLIS=0` switches the trellis round off, `=1` forces it.
  With the second version `DECODIUM_FT8_BICM` only switches BICM-ID on or off;
  the number of rounds comes from `DECODIUM_FT8_BICM_V2_ROUNDS` (default 3).

### FT2 and FT4

- Decode lists identical to 1.0.1007 on the regression sets.
- FT2: integer overflow fixed in the legacy noise estimate; results are
  unchanged where the old code did not overflow. `DECODIUM_LDPC_LLR_CLIP` is
  now read in FT2 as well (default still 2.5).

### Optional, off by default

These did not meet the rule used for defaults (a measured gain at no more
false decodes and under about 10% extra decode time):

- `DECODIUM_LDPC_K77=1`: ordered-statistics search on the (174,77) code with
  the CRC in the matrix, plus a margin gate. About 0.02 dB in FT8 with fewer
  false accepts, but 16% (FT8) and 22% (FT2) slower.
- `DECODIUM_FT2_MAX_HARD_SYNC=<cap>`: higher FT2 flipped-bit cap for
  sync-confirmed candidates. More decodes but more false decodes; the cap
  stays at 22.
- `DECODIUM_FT2_METRIC=i0|amp`: alternative FT2 bit metrics; gain within the
  measurement noise.

### Build and continuous integration

- Fixed on CI: the four Linux variants (x86-64 full, analogue, feature-off,
  arm64), the FT2-Link jobs on Linux and Windows, and the Windows installer
  build. The installer build now uses OpenSSL 3.6.5 in its own prefix, because
  MSYS2 moved to OpenSSL 4, which Qt 6.11.0 does not build against, and
  applies the Hamlib 4.7.0 patch for GCC 16.
- `Audio/BWFFile.cpp`: a truncated `bext` chunk in a WAV file is no longer
  read past its end.
- The explicit `DECODIUM_FT2LINK_W2300_MAX_DECODE_MS` request can now go up to
  60 s (was 12 s); the live default stays 900 ms.
- From GCC 16 on, five flow-analysis diagnostics that fire as false positives
  inside Qt headers stay warnings instead of stopping the build; older GCC and
  Clang keep them as errors.
- 56 regression tests that were registered but never built are now built and
  run; several tests whose expectations were out of date are corrected. No
  test was skipped or disabled.
- `packaging/windows/stage_runtime.ps1` rebuilds the installer runtime folder
  (Qt without FFmpeg, MinGW and third-party DLLs with their indirect
  dependencies, Decodium QML folders, data and translations).

### Known limits

- The FT8 figures come from synthetic signals and eight real recordings
  without ground truth. The new decoding has not been validated on air, and
  the application with the governor running slot after slot was not exercised:
  the granted state was reproduced in the `ft8_stage_compare` tool.
- Still red on CI: the Windows SSTV job (it now builds 2128 of 2655 steps; the
  last correction had no result at release time), the macOS jobs (x86-64 never
  completed; on arm64 one sharing-expiry test fails intermittently, cause not
  established) and the ASan/UBSan job (8 of 167 tests: seven exceed their time
  limits in a sanitised Debug build, one fails for a cause not established).
- The RR73 issue in Hound mode is still open.

## Italiano

La release 1.0.1008 migliora la sensibilità FT8 sui segnali deboli dove il
computer ha CPU libera, e ripara la maggior parte delle build di integrazione
continua. Il formato in aria non cambia: matrice LDPC(174,91), CRC-14,
impacchettamento dei messaggi e forma d'onda non sono toccati. Tutte le
modifiche sono in ricezione.

### Decodifica FT8

- **BICM-ID, seconda versione.** La passata di demodulazione iterativa
  costruisce ora la retroazione dalla media dell'informazione estrinseca delle
  cinque parole appena fallite (senza decodifica sonda in più), fa tre giri ed
  è tentata solo sui candidati con almeno 10 simboli Costas corretti.
  Sostituisce la prima versione dovunque il BICM-ID gira, cioè dove lo concede
  il governatore delle leve adattive.
- **Demodulatore a traliccio di fase.** Un giro di metriche in più, tentato
  solo dopo che ogni altro tentativo sul candidato è fallito: un demodulatore
  forward-backward che insegue la fase della portante, con i simboli Costas
  come piloti e la retroazione del BICM-ID come a priori. Anche questo è
  concesso dal governatore: gira solo dopo alcuni slot ben dentro il tempo
  disponibile e viene tolto al primo slot che ci si avvicina.
- **Misure** su 230 slot sintetici mai usati per la taratura (AWGN, deriva,
  random walk di fase, fading, QRM), profondità 3, leve concesse:

  | | 1.0.1007 | 1.0.1008 |
  |---|---|---|
  | Decodifiche | 1735 | 2331 (+34%) |
  | False decodifiche, slot con segnali | 2 | 1 |
  | False decodifiche, 100 slot di solo rumore | 0 | 0 |

  Il guadagno è sui segnali deboli (a -22 dB da circa un messaggio su cinque a
  più di uno su due); i segnali forti erano già decodificati. Il solo BICM-ID
  porta da 1735 a 1864, il giro a traliccio da 1864 a 2331. Il BICM-ID nuovo
  impiega il 29-48% di tempo in meno della prima versione; il giro a traliccio
  aggiunge circa il 14% (misura non appaiata, su macchina condivisa).
- **Dove le leve non sono concesse** (computer più lenti) le liste di
  decodifica sono identiche alla 1.0.1007 sull'insieme di regressione (8
  registrazioni x 5 profili). La ricerca a statistiche ordinate dell'LDPC è
  circa il 20% più veloce per parola, circa il 7% sull'intero decoder FT8 a
  profondità 3, con uscite identiche bit per bit.
- **Interruttori.** `DECODIUM_FT8_BICM_V2=0` ripristina la prima versione del
  BICM-ID; `DECODIUM_FT8_TRELLIS=0` spegne il giro a traliccio, `=1` lo forza.
  Con la seconda versione `DECODIUM_FT8_BICM` accende o spegne soltanto; il
  numero di giri viene da `DECODIUM_FT8_BICM_V2_ROUNDS` (default 3).

### FT2 e FT4

- Liste di decodifica identiche alla 1.0.1007 sugli insiemi di regressione.
- FT2: corretto un overflow intero nella stima di rumore legacy; i risultati
  non cambiano dove il vecchio codice non andava in overflow.
  `DECODIUM_LDPC_LLR_CLIP` ora è letta anche in FT2 (default sempre 2,5).

### Opzionale, spento di default

Queste funzioni non hanno soddisfatto la regola usata per i default (guadagno
misurato senza più false decodifiche e con meno del 10% circa di tempo in più):

- `DECODIUM_LDPC_K77=1`: ricerca a statistiche ordinate sul codice (174,77)
  con il CRC nella matrice, più un gate a margine. Circa 0,02 dB in FT8 con
  meno falsi, ma il 16% (FT8) e il 22% (FT2) più lento.
- `DECODIUM_FT2_MAX_HARD_SYNC=<tetto>`: tetto dei bit invertiti FT2 più alto
  per i candidati con sync confermato. Più decodifiche ma più false
  decodifiche; il tetto resta 22.
- `DECODIUM_FT2_METRIC=i0|amp`: metriche dei bit FT2 alternative; guadagno
  entro il rumore di misura.

### Build e integrazione continua

- Sistemate in CI: le quattro varianti Linux (x86-64 completa, analogica,
  funzioni disattivate, arm64), i job FT2-Link su Linux e Windows e la build
  dell'installer Windows. La build dell'installer usa ora OpenSSL 3.6.5 in un
  prefisso separato, perché MSYS2 è passato a OpenSSL 4, contro cui Qt 6.11.0
  non compila, e applica la patch di Hamlib 4.7.0 per GCC 16.
- `Audio/BWFFile.cpp`: un blocco `bext` troncato in un file WAV non viene più
  letto oltre la sua fine.
- La richiesta esplicita `DECODIUM_FT2LINK_W2300_MAX_DECODE_MS` può ora
  arrivare a 60 s (prima 12 s); il valore dal vivo resta 900 ms.
- Da GCC 16 in su, cinque diagnostiche di analisi del flusso che scattano come
  falsi positivi dentro gli header di Qt restano avvisi invece di fermare la
  build; con GCC più vecchi e Clang restano errori.
- 56 test di regressione registrati ma mai costruiti ora vengono costruiti ed
  eseguiti; diversi test con aspettative superate sono corretti. Nessun test è
  stato saltato o disattivato.
- `packaging/windows/stage_runtime.ps1` ricostruisce la cartella di runtime
  dell'installer (Qt senza FFmpeg, DLL MinGW e di terze parti con le
  dipendenze indirette, cartelle QML di Decodium, dati e traduzioni).

### Limiti noti

- I numeri FT8 vengono da segnali sintetici e da otto registrazioni reali
  senza verità nota. La nuova decodifica non è stata validata in aria, e
  l'applicazione con il governatore in funzione slot dopo slot non è stata
  eseguita: lo stato «leve concesse» è stato riprodotto nello strumento
  `ft8_stage_compare`.
- Ancora rossi in CI: il job Windows SSTV (ora compila 2128 passi su 2655;
  l'ultima correzione non aveva esito al momento della release), i job macOS
  (x86-64 mai arrivato in fondo; su arm64 un test di scadenza della
  condivisione fallisce a intermittenza, causa non accertata) e il job
  ASan/UBSan (8 test su 167: sette superano i limiti di tempo in una build
  Debug con sanitizer, uno fallisce per causa non accertata).
- Il problema RR73 in modalità Hound è ancora aperto.
