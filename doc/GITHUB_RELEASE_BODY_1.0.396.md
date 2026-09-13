# Decodium 4 FT2 1.0.396

Changes from `1.0.395` to `1.0.396`.

This release keeps the upstream `1.0.395` decoder base and adds the FT8/JTDX parity work, timing guardrails and comparison tooling used during the latest two-minute WAV investigation.

## Italiano

### FT8 live decode e parita' con JTDX

- Aggiunto il refresh del contesto FT8 da log locali recenti prima del decode live, con coda hash-call fino a `4096` nominativi, in linea con il limite usato da Decodium per il contesto pack77.
- Estesa la lettura di `ALL.TXT`/log mensili per ricavare call hash utili, CQ noti, CQ senza grid e messaggi A7/AP recenti.
- Il seed CQ esterno resta protetto: il replay known-CQ completo da log JTDX viene applicato solo se `DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY` e' abilitato e la sorgente e' riconoscibile come JTDX.
- Aggiunto il seed opzionale degli hint A7/AP da log tramite `DECODIUM_FT8_SEED_A7_HINTS`, con limiti conservativi su numero di messaggi, coppie dirette e inserimenti per slot.
- Normalizzati e filtrati i token usati come seed, scartando report, locator, modi e placeholder non plausibili per ridurre falsi positivi.
- Aggiunta priorita' ai messaggi A7 diretti e recenti, compresa la generazione delle coppie call1/call2 e call2/call1 quando il log contiene un QSO indirizzato.

### Stage4 FT8

- Esteso Stage4 con cache hash-call piu' ampia e funzioni C dedicate per:
  - seed hash-call;
  - seed known-CQ con call/grid;
  - seed known-CQ solo call;
  - inserimento di hint A7.
- Migliorata la gestione di cache AP/CQ e dei candidati deep in modo che il decoder possa sfruttare contesto caldo senza rendere obbligatorio il replay esterno JTDX.
- Aggiunti percorsi di salvataggio/riuso A7 per aiutare il confronto sui decode mancanti rispetto a JTDX.
- Rafforzate le deduplicazioni e i filtri sui messaggi generati dai pass deep/AP per mantenere stabile l'output live.

### Timing live

- La finestra late FT8 e' stata portata da circa `6.5s` a circa `7.0s` dopo l'inizio dello slot successivo.
- Il budget massimo del deep follow-up live passa da `5200ms` a `5700ms`, mantenendo margine di sicurezza prima della consegna massima.
- Il limite di completamento del deep follow-up passa da `6000ms` a `6500ms` oltre il boundary slot, per permettere recuperi deboli senza spostare la consegna fuori finestra.
- Aggiunto logging `[FT8DISPATCH]` per misurare seriale, depth, budget, opzioni deep/AP e deadline effettive durante le prove live.

### Tool di confronto rilasciati

- `tools/compare_alltxt.py` ora confronta meglio due `ALL.TXT` su finestra UTC, normalizza i messaggi, conta CQ-only e rende piu' leggibile il gap Decodium/JTDX.
- `tests/ft8_stage_compare.cpp` e' stato esteso per riprodurre i test offline con seed hash, known-CQ e hint A7/AP, cosi' Martino puo' analizzare i decode mancanti sullo stesso WAV.
- La release include anche un archivio strumenti dedicato: `decodium4-ft8-compare-tools-1.0.396.zip`.

### Validazione locale

- Build locale mirata:
  - `cmake --build /Users/salvo/Desktop/Decodium4-build --target ft8_stage_compare -j 8`
- Controllo whitespace/diff:
  - `git diff --check`

### Asset release attesi

- Source code archive generato automaticamente da GitHub per il tag `1.0.396`.
- `Decodium_1.0.396_Setup_x64.exe`
- `decodium4-ft2-1.0.396-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.396-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.396-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.396-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.396-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.396-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.396-linux-aarch64.AppImage`
- File SHA256 generati dai workflow macOS e Linux.
- `decodium4-ft8-compare-tools-1.0.396.zip`

## English

### FT8 live decode and JTDX parity work

- Added live FT8 context refresh from recent local logs before decode, with a hash-call queue up to `4096` calls.
- Extended `ALL.TXT`/monthly-log parsing to collect useful hash calls, known CQ rows, CQ rows without grids and recent A7/AP message hints.
- External known-CQ replay remains gated: full JTDX known-CQ replay is used only when `DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY` is enabled and the source path is recognizably JTDX.
- Added optional A7/AP hint seeding through `DECODIUM_FT8_SEED_A7_HINTS`, with conservative limits for messages, directed pairs and inserted hints per slot.
- Normalized and filtered seed tokens to reject reports, locators, modes and implausible placeholders.
- Prioritized recent directed A7 messages and generated both directed call pairs when a logged QSO provides enough context.

### FT8 Stage4

- Extended Stage4 with a wider hash-call cache and C entry points for hash-call, known-CQ, call-only known-CQ and A7 hint seeding.
- Improved AP/CQ cache and deep-candidate handling so the decoder can use warm context without making external JTDX replay mandatory.
- Added A7 save/reuse paths for missing-decode investigation against JTDX.
- Tightened duplicate and generated-message filtering for live deep/AP output.

### Live timing

- The FT8 late window moves from about `6.5s` to about `7.0s` after the next slot starts.
- The live deep follow-up maximum budget moves from `5200ms` to `5700ms`.
- The deep follow-up latest-completion guard moves from `6000ms` to `6500ms` after the slot boundary.
- Added `[FT8DISPATCH]` diagnostics for serial, depth, budget, deep/AP options and effective deadline measurements.

### Released comparison tools

- `tools/compare_alltxt.py` now provides clearer UTC-window `ALL.TXT` comparison, message normalization and CQ-only gap reporting.
- `tests/ft8_stage_compare.cpp` was extended to reproduce offline WAV tests with hash, known-CQ and A7/AP seed state.
- A dedicated comparison-tool archive is attached to the release: `decodium4-ft8-compare-tools-1.0.396.zip`.
