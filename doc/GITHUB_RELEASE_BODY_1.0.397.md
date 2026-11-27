# Decodium 4 FT2 1.0.397

Changes from `1.0.395` to `1.0.397`.

This release consolidates the `1.0.396` FT8 decoder line, the local JTDX-parity work and the new Latvian UI translation. It also keeps the release runners aligned so all platform assets publish to the same `v1.0.397` GitHub release.

## Italiano

### Decoder FT8 e confronto con JTDX

- Integra il nuovo ramo FT8 `1.0.396` con le ottimizzazioni recenti sul decoder:
  - candidate harvest piu' aggressivo;
  - partizionamento per frequenza del loop candidati;
  - isolamento per-bin delle cache usate da DD, history CQ/call-grid e A7;
  - wrapping OpenMP del loop candidati;
  - riduzione dei bin harvest da 8 a 4 per eliminare il borrow a banda piena.
- Mantiene il lavoro locale di parita' con JTDX introdotto nella linea `1.0.396`:
  - refresh del contesto FT8 da `ALL.TXT` e log mensili locali;
  - cache hash-call fino a `4096` nominativi;
  - seed CQ/call-grid e hint A7/AP recenti;
  - filtri sui token di seed per evitare report, locator, modi e placeholder non plausibili.
- Conserva i percorsi di confronto e diagnosi dei decode mancanti rispetto a JTDX:
  - `tools/compare_alltxt.py`;
  - `tools/compare_alltxt_window.py`;
  - test offline `ft8_stage_compare`.

### Timing dei decode live

- Mantiene la finestra late decode sotto la soglia operativa richiesta:
  - prima consegna intorno ai 13 secondi dallo slot;
  - consegna late intorno a 6.5-7 secondi dall'inizio dello slot successivo.
- Include le guardrail introdotte per il deep follow-up FT8:
  - budget live deep esteso;
  - deadline deep follow-up estesa ma ancora sotto la finestra di rilascio;
  - diagnostica `[FT8DISPATCH]` e `[DECODEMETRIC]` per verificare tempi, depth, AP, thread attivi e nout.

### Lingua Latvian

- Aggiunta la traduzione Latvian (`lv_LV`) da:
  - `https://gitea.com/yl3gbc/decodium-lv.git`
- Aggiunto `translations/decodium_lv.ts` alla sorgente del progetto.
- Aggiunto `lv` alla lista CMake delle lingue compilate.
- Aggiunto `Latviešu` al menu lingua della UI.
- Aggiunta label locale `Valoda`.
- La build genera e incorpora:
  - `decodium_lv.qm`;
  - `qt_lv.qm`.
- Stato della traduzione al momento dell'integrazione:
  - `2631` traduzioni generate;
  - `2544` finite;
  - `87` unfinished;
  - `1033` sorgenti ancora non tradotte.

### UI e nomenclatura

- Integra i commit upstream che rinominano il riferimento interno in-app da Shannon a Gallager dove previsto dalla linea `1.0.396`.
- Mantiene il selettore lingua coerente con le lingue gia' disponibili: English, Catalan, Danish, German, Spanish, French, Hungarian, Italian, Japanese, Latvian, Russian, Simplified Chinese and Traditional Chinese.

### Release infrastructure

- `fork_release_version.txt` aggiornato a `1.0.397`.
- I workflow macOS Apple Silicon e macOS Intel ora distinguono:
  - `VERSION`, usato per i nomi file asset senza prefisso `v`;
  - `RELEASE_REF`, usato per pubblicare gli asset sulla release `v1.0.397`.
- Questo evita la creazione accidentale di una release parallela `1.0.397` quando il tag operativo e' `v1.0.397`.
- Corretti i build release Linux, Windows e macOS:
  - l'override temporaneo della cache FT8 known call-grid in `FtxFt8Stage4.cpp` non conserva piu' l'indirizzo di una variabile locale;
  - le copie snapshot di `state.dd` evitano il falso positivo `-Wnonnull` di GCC 16 nei runner MinGW;
  - `wsjt_qt` propaga il link a `OpenMP::OpenMP_CXX`, evitando simboli `___kmpc_*` mancanti nei tool opzionali costruiti dai runner macOS.

### Validazione locale

- Compilazione traduzioni:
  - `cmake --build /Users/salvo/Desktop/Decodium4-build --target translations -j 8`
- Compilazione app QML:
  - `cmake --build /Users/salvo/Desktop/Decodium4-build --target decodium_qml -j 8`
- Smoke test lingua Latvian:
  - avvio breve con `-l lv`;
  - loader verificato su `:/Translations` per `lv`;
  - `QML OK`;
  - `Main.qml created as top-level window`.
- Controllo whitespace:
  - `git diff --check`

### Asset release attesi

- Source code archive generato automaticamente da GitHub per il tag `v1.0.397`.
- Windows:
  - `Decodium_1.0.397_Setup_x64.exe`
- macOS Apple Silicon:
  - `decodium4-ft2-1.0.397-macos-tahoe-arm64.dmg`
  - `decodium4-ft2-1.0.397-macos-sequoia-arm64.dmg`
- macOS Intel:
  - `decodium4-ft2-1.0.397-macos-ventura-x86_64.dmg`
  - `decodium4-ft2-1.0.397-macos-sonoma-x86_64.dmg`
  - `decodium4-ft2-1.0.397-macos-sequoia-x86_64.dmg`
- Linux AppImage:
  - `decodium4-ft2-1.0.397-linux-x86_64.AppImage`
  - `decodium4-ft2-1.0.397-linux-aarch64.AppImage`
- File SHA256 generati dai workflow macOS e Linux.

## English

### FT8 decoder and JTDX comparison work

- Integrates the `1.0.396` FT8 decoder line with recent decoder optimization work:
  - more aggressive candidate harvesting;
  - frequency-partitioned candidate loop;
  - per-bin isolation for DD, CQ/call-grid history and A7 caches;
  - OpenMP wrapping of the candidate loop;
  - harvest bins reduced from 8 to 4 to remove full-band borrowing.
- Keeps the local JTDX-parity work from the `1.0.396` fork line:
  - live FT8 context refresh from local `ALL.TXT` and monthly logs;
  - hash-call cache up to `4096` calls;
  - CQ/call-grid seeding and recent A7/AP hints;
  - seed-token filtering to reject reports, locators, modes and implausible placeholders.
- Keeps the comparison and diagnostic tooling used for missing-decode investigations:
  - `tools/compare_alltxt.py`;
  - `tools/compare_alltxt_window.py`;
  - offline `ft8_stage_compare` tests.

### Live decode timing

- Keeps live FT8 release timing within the requested operating window:
  - first delivery around 13 seconds into the slot;
  - late delivery around 6.5-7 seconds after the next slot starts.
- Includes the deep-follow-up guardrails:
  - extended live deep budget;
  - extended deep-follow-up deadline while staying inside the release window;
  - `[FT8DISPATCH]` and `[DECODEMETRIC]` diagnostics for timing, depth, AP, active threads and nout.

### Latvian language

- Adds Latvian (`lv_LV`) from:
  - `https://gitea.com/yl3gbc/decodium-lv.git`
- Adds `translations/decodium_lv.ts`.
- Adds `lv` to the CMake translation language list.
- Adds `Latviešu` to the UI language menu.
- Adds the local label `Valoda`.
- The build generates and embeds:
  - `decodium_lv.qm`;
  - `qt_lv.qm`.

### Release infrastructure

- Updates `fork_release_version.txt` to `1.0.397`.
- Adjusts the macOS release workflows so manually dispatched builds upload to `v1.0.397` instead of creating a parallel non-prefixed release.
- Fixes the Linux, Windows and macOS release builds:
  - the temporary FT8 known call-grid override in `FtxFt8Stage4.cpp` no longer stores the address of a local stack object;
  - `state.dd` snapshot copies avoid the GCC 16 MinGW `-Wnonnull` false positive;
  - `wsjt_qt` now propagates `OpenMP::OpenMP_CXX`, avoiding missing `___kmpc_*` symbols in optional tools built by the macOS runners.
