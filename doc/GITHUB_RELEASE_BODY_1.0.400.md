# Decodium 4 FT2 1.0.400

Changes from `1.0.399` to `1.0.400`.

This release focuses on the FT8 weak-signal gap found during the JTDX comparison window recorded on `2026-06-14 17:21:00 UTC`. It keeps the `1.0.399` mode-switch and UI alignment work, and adds a targeted A7 replay improvement for repeated directed reports that are present in JTDX at about `-24 dB`.

## Italiano

### FT8 weak repeated report recovery

- Anticipato il fast A7 replay nel primo passaggio utile dello stage FT8, prima che il decoder consumi tutto il budget sul pass principale.
- Aggiunto un retry stretto di frequenza per report diretti gia' visti nello slot precedente.
- Il retry e' limitato ai messaggi standard esatti con report finale, ad esempio `CALL1 CALL2 R-15`.
- Le varianti terminali `73`, `RR73` e `RRR` restano con metriche rigide: il fix non apre una scorciatoia larga sui messaggi finali.
- I report diretti recuperati dal fast A7 vengono salvati con peso storico sufficiente per alimentare il replay dello slot successivo.
- La priorita' del fast A7 ora favorisce report diretti ripetuti e recenti, senza aumentare globalmente il numero massimo di tentativi.

### Risultato sul WAV di confronto

Sul WAV `/Users/salvo/Documents/Decodium/recordings/decodium_20260614_172100.wav`, con `depth=3`, `ap=1`, `cycles=2`, `rxfsens=3`, `candthin=100` e deadline `6500 ms`, ora vengono recuperati:

- `17:25:00 -24 0.2 1594 M7JVT HA8CQ R-15`
- `17:25:30 -24 0.2 1594 M7JVT HA8CQ R-15`
- `17:25:30 -22/-24 1.3 2453 BA3QIQ DK1MCS JN57`

Il target `17:24:45 -24 -1.2 2001 CQ LZ1BT KN12` resta fuori da questo fix. E' stato provato anche con:

- known-CQ seeded su `LZ1BT KN12`;
- frequenza raffinata intorno a `2002.6 Hz`;
- `max-ms=0`;
- `cycles=3`;
- subtract mirato dei segnali vicini `CQ OK1ZJK JN79` e `CQ ON3CH JO20`.

Non viene recuperato nemmeno in queste condizioni, quindi non e' un semplice problema di timing o di replay AP/A7. Richiede un intervento separato sul blind CQ weak path.

### Timing

- Il replay verificato usa `--max-ms 6500`.
- I target M7JVT vengono recuperati dentro la finestra operativa richiesta, senza spostare i late decode oltre i circa `6.5 s` dello slot successivo.
- Il fix non aumenta il budget globale dei tentativi: aggiunge solo retry locali su candidati gia' filtrati.

### Versione e release

- Versione locale aggiornata a `1.0.400` tramite `fork_release_version.txt`.
- Aggiornati anche i default degli installer Windows Inno/NSIS a `1.0.400`.
- Il tag operativo della release e' `v1.0.400`, coerente con `v1.0.399`, per avviare i runner `v*` senza creare un secondo tag/release parallelo non prefissato.
- Asset attesi:
  - source code archive generato automaticamente da GitHub per `v1.0.400`;
  - `Decodium_1.0.400_Setup_x64.exe`;
  - `decodium4-ft2-1.0.400-macos-tahoe-arm64.dmg`;
  - `decodium4-ft2-1.0.400-macos-sequoia-arm64.dmg`;
  - `decodium4-ft2-1.0.400-macos-ventura-x86_64.dmg`;
  - `decodium4-ft2-1.0.400-macos-sonoma-x86_64.dmg`;
  - `decodium4-ft2-1.0.400-macos-sequoia-x86_64.dmg`;
  - `decodium4-ft2-1.0.400-linux-x86_64.AppImage`;
  - `decodium4-ft2-1.0.400-linux-aarch64.AppImage`;
  - file SHA256 generati dai runner macOS e Linux.

### Validazione locale

- Build locale:
  - `cmake -S /Users/salvo/Desktop/Decodium4/Decodium-4.0-Core-Shannon -B /Users/salvo/Desktop/Decodium4-build`
  - `cmake --build /Users/salvo/Desktop/Decodium4-build --target ft8_stage_compare decodium_qml --parallel 4`
- Replay mirato:
  - `ft8_stage_compare --max-ms 6500` sulla finestra `17:23:30 -> 17:25:30 UTC`
- Controllo patch:
  - `git diff --check`
- Avvio locale:
  - `/Users/salvo/Desktop/Decodium4-build/decodium`

## English

### FT8 weak repeated report recovery

- Runs the fast A7 replay earlier in the useful FT8 stage pass, before the main pass consumes the whole decode budget.
- Adds a narrow frequency retry for directed reports already seen in the previous slot.
- The retry is limited to exact standard messages ending with a report token, for example `CALL1 CALL2 R-15`.
- Terminal variants `73`, `RR73` and `RRR` keep strict metrics.
- Directed reports recovered by fast A7 are saved with enough history weight to feed the next-slot replay.
- Fast A7 priority now favors recent repeated directed reports without globally increasing the maximum attempt count.

### Comparison WAV result

On `/Users/salvo/Documents/Decodium/recordings/decodium_20260614_172100.wav`, with `depth=3`, `ap=1`, `cycles=2`, `rxfsens=3`, `candthin=100` and `max-ms=6500`, Decodium now recovers:

- `17:25:00 -24 0.2 1594 M7JVT HA8CQ R-15`
- `17:25:30 -24 0.2 1594 M7JVT HA8CQ R-15`
- `17:25:30 -22/-24 1.3 2453 BA3QIQ DK1MCS JN57`

The remaining `17:24:45 -24 -1.2 2001 CQ LZ1BT KN12` target did not recover even with known-CQ seeding, refined frequency around `2002.6 Hz`, unlimited deadline, `cycles=3`, and targeted subtracts for nearby decoded signals. That case needs a separate blind-CQ weak-path fix.

### Timing

- The verified replay uses `--max-ms 6500`.
- M7JVT is recovered inside the requested late-decode window, without pushing delivery past roughly `6.5 s` into the next slot.
- The fix does not raise the global attempt budget; it only adds narrow retries on already-filtered candidates.

### Version and release

- Updates `fork_release_version.txt` to `1.0.400`.
- Updates Windows Inno/NSIS installer defaults to `1.0.400`.
- Uses `v1.0.400` as the operational release tag, matching `v1.0.399`, so the `v*` runners start without creating a duplicate non-prefixed release.
- Expected assets include the Windows x64 installer, macOS Apple Silicon DMGs, macOS Intel DMGs, Linux x86_64 AppImage, Linux aarch64 AppImage and SHA256 files.
