# Decodium 4 FT2 v1.0.615

## English (UK)

### Changes since v1.0.614

- Absorbed upstream elisir80 v1.0.613 (QMX RTTY USB-audio TX fix) and v1.0.614
  (Live Map GPU/CPU rendering fallback, RTTY AFSK/FSK clarity, SuperFox/
  SuperHound UI wording) via merge; both behave as described in their own
  upstream release notes.
- FT2: added an in-app diagnostic log line (`[FT2-DRIFT-RESCUE]`) for when the
  opt-in drift-rate search (introduced in a prior WIP commit, still off by
  default via `DECODIUM_FT2_DRIFT_SEARCH`) recovers a decode. No behaviour
  change for anyone who has not enabled that toggle.
- FT2: fixed a real undefined-behaviour bug in the drift-rate search, isolated
  with Dr. Memory — a stack layout issue sensitive to aggressive GCC -O3
  inlining around `run_decode_passes`, fixed with `__attribute__((noinline))`.
  The drift-rate search feature itself remains off by default; this is a
  correctness fix for opt-in code, zero risk to anyone not using it.
- FT8: wired the SuperFox decoder into the live receive worker for the Hound
  role. SuperFox decoding existed in the codebase since the initial C++ port
  but was never connected to the real asynchronous decode pipeline — a
  pre-existing architectural gap, not a regression. It now runs through an
  atomic setter (`ftx_ft8_stage4_set_superfox_options_c`), active only when
  role=Hound (`ncontest==7`) and the Setup "SuperFox" checkbox is on; zero
  risk for anyone not using it.
  - While wiring this in, found and fixed a real pre-existing bug:
    `superfox_decode_lines_from_wave` allocated roughly 2.16 MB on the stack
    instead of the heap, causing a genuine `STATUS_STACK_OVERFLOW`
    (reproduced with `utils/sfrx.exe` before the fix). Converted to
    `std::vector`.
  - Also fixed two pre-existing minor compile errors in `utils/sfoxsim.cpp`
    that had never surfaced before with this GCC/`-Werror` combination.
  - **Verification level: offline only.** Checked with
    `tests/ft8_stage_compare.cpp --superfox` against a synthetic WAV generated
    by `utils/sfoxsim.exe`, output identical to the `utils/sfrx.exe`
    reference. This has **not** yet been tried against a real SuperFox
    DXpedition signal on the air. Treat the RX path as unverified under real
    conditions until confirmed against an actual DXpedition or monitor
    session.

This release is published with the source code and platform packages built by
the GitHub Actions runners: Windows x64 executable, macOS Apple Silicon and
Intel DMGs, and Linux x86_64 and aarch64 AppImages.

## Italiano

### Modifiche dalla v1.0.614

- Assorbite via merge la v1.0.613 di elisir80 (fix trasmissione audio USB
  QMX/RTTY) e la v1.0.614 (fallback GPU/CPU del rendering della mappa live,
  chiarezza RTTY AFSK/FSK, testo UI SuperFox/SuperHound); il comportamento
  resta quello descritto nelle rispettive note di rilascio upstream.
- FT2: aggiunto un log diagnostico in-app (`[FT2-DRIFT-RESCUE]`) per quando la
  ricerca opzionale del tasso di deriva (introdotta in un commit WIP
  precedente, ancora spenta di default tramite `DECODIUM_FT2_DRIFT_SEARCH`)
  recupera una decodifica. Nessun cambiamento per chi non ha attivato
  quell'opzione.
- FT2: risolto un vero bug di comportamento indefinito nella ricerca del
  tasso di deriva, isolato con Dr. Memory — un problema di layout dello stack
  sensibile all'inlining aggressivo di GCC -O3 attorno a `run_decode_passes`,
  risolto con `__attribute__((noinline))`. La funzione di ricerca del tasso
  di deriva resta spenta di default: è un fix di correttezza per codice
  opzionale, zero rischio per chi non lo usa.
- FT8: agganciato il decoder SuperFox al worker di ricezione live per il
  ruolo Hound. Il decoder SuperFox esisteva nel codice fin dal porting C++
  iniziale ma non era mai stato collegato alla vera pipeline di decodifica
  asincrona — una lacuna architetturale preesistente, non una regressione.
  Ora è attivo tramite un setter atomico
  (`ftx_ft8_stage4_set_superfox_options_c`), solo quando ruolo=Hound
  (`ncontest==7`) e la spunta "SuperFox" in Setup è accesa; zero rischio per
  chi non la usa.
  - Durante l'integrazione, trovato e corretto un bug reale preesistente:
    `superfox_decode_lines_from_wave` allocava circa 2,16 MB sullo stack
    invece che sull'heap, causando un vero `STATUS_STACK_OVERFLOW`
    (riprodotto con `utils/sfrx.exe` prima del fix). Convertito a
    `std::vector`.
  - Corretti anche due piccoli errori di compilazione preesistenti in
    `utils/sfoxsim.cpp`, mai emersi prima con questa combinazione
    GCC/`-Werror`.
  - **Livello di verifica: solo offline.** Verificato con
    `tests/ft8_stage_compare.cpp --superfox` contro un WAV sintetico generato
    da `utils/sfoxsim.exe`, output identico al riferimento `utils/sfrx.exe`.
    **Non** ancora provato contro un vero segnale SuperFox di una DXpedition
    in aria. Considera il percorso RX non verificato in condizioni reali
    finché non sarà confermato da una DXpedition o sessione di monitoraggio
    effettiva.

Questa release viene pubblicata con il codice sorgente e i pacchetti prodotti
dai runner GitHub Actions: eseguibile Windows x64, DMG macOS Apple Silicon e
Intel, AppImage Linux x86_64 e aarch64.
