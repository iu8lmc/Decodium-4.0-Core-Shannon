# Decodium 4 FT2 1.0.399

Changes from `1.0.398` to `1.0.399`.

This release focuses on decoder worker stability after mode/slot changes and on cross-platform UI alignment. It keeps the `1.0.398` FT8 trace base and adds a stricter live-decode serial gate so stale queued work cannot report late results after the user changes mode or after a newer live decode supersedes an older one.

## Italiano

### Stabilita' decoder FT8/FT4

- Aggiunto un gate "latest serial" nei worker `FT8DecodeWorker` e `FT4DecodeWorker`.
- Ogni richiesta decode live marca il seriale piu' recente prima di entrare nella coda del worker.
- Il worker scarta in modo cooperativo le richieste vecchie:
  - prima di acquisire il mutex runtime Fortran;
  - dopo l'attesa del mutex;
  - prima di pubblicare i risultati alla UI.
- Il cambio modo invalida esplicitamente i seriali FT8, FT4 e FT2, evitando callback vecchie dopo passaggi `FT4 -> FT8`, `FT8 -> FT2` o ritorni rapidi tra modi.
- La pulizia dei seriali storici rimuove anche lo stato temporaneo di early decode e subpass harvest, riducendo code residue dopo slot trafficati.
- Obiettivo operativo: mantenere la reattivita' dei decode entro la finestra corrente, senza accumulare decode vecchi che possono bloccare o sporcare i risultati sui PC piu' lenti.

### Layout Linux/Windows

- Corretto il layout dell'header su Linux: la toolbar superiore non riserva piu' `520px` fissi quando alcuni pulsanti sono nascosti.
- La larghezza della toolbar ora viene calcolata dai pulsanti realmente visibili, evitando wrap diversi fra Linux e Windows dovuti a metriche font differenti.
- Stabilizzata la band bar del pannello TX con larghezze deterministiche e font monospaziato.
- Stabilizzati i pulsanti TX/CQ:
  - contenuto interno con layout fisso;
  - padding/inset nativi Qt azzerati;
  - label monospaziate e centrate.
- Questo rende coerente il posizionamento delle icone e dei pulsanti fra Windows e Linux.

### Versione e release

- Versione locale aggiornata a `1.0.399` tramite `fork_release_version.txt`.
- Aggiornati anche i default degli installer Windows Inno/NSIS a `1.0.399`.
- Il tag operativo della release e' `v1.0.399`, coerente con `v1.0.398`, per avviare i runner `v*` senza creare un secondo tag/release parallelo non prefissato.
- Asset attesi:
  - source code archive generato automaticamente da GitHub per `v1.0.399`;
  - `Decodium_1.0.399_Setup_x64.exe`;
  - `decodium4-ft2-1.0.399-macos-tahoe-arm64.dmg`;
  - `decodium4-ft2-1.0.399-macos-sequoia-arm64.dmg`;
  - `decodium4-ft2-1.0.399-macos-ventura-x86_64.dmg`;
  - `decodium4-ft2-1.0.399-macos-sonoma-x86_64.dmg`;
  - `decodium4-ft2-1.0.399-macos-sequoia-x86_64.dmg`;
  - `decodium4-ft2-1.0.399-linux-x86_64.AppImage`;
  - `decodium4-ft2-1.0.399-linux-aarch64.AppImage`;
  - file SHA256 generati dai runner macOS e Linux.

### Validazione locale

- Build locale:
  - `cmake --build /Users/salvo/Desktop/Decodium4-build --target decodium_qml --parallel 4`
- Test locali mirati:
  - `ctest --test-dir /Users/salvo/Desktop/Decodium4-build -R "test_ftx_weak_decode|test_ft8_weak_decode_deep|test_ft2_qso_sim" --output-on-failure`
- Controllo patch:
  - `git diff --check`

## English

### FT8/FT4 decoder stability

- Adds a "latest serial" gate to `FT8DecodeWorker` and `FT4DecodeWorker`.
- Each live decode request marks the newest serial before entering the worker queue.
- Workers cooperatively drop stale requests:
  - before taking the Fortran runtime mutex;
  - after waiting for that mutex;
  - before publishing rows back to the UI.
- Mode changes explicitly invalidate FT8, FT4 and FT2 serials, preventing stale callbacks after `FT4 -> FT8`, `FT8 -> FT2` or rapid mode switching.
- Historical serial cleanup now also removes temporary early-decode and subpass-harvest state, reducing queue residue after busy slots.
- Operational goal: keep live decode delivery inside the current timing window without stale work blocking or polluting results on slower PCs.

### Linux/Windows UI alignment

- Fixes the Linux header layout: the top toolbar no longer reserves a fixed `520px` when some buttons are hidden.
- Toolbar width is now computed from the buttons that are actually visible, avoiding Linux/Windows wrap differences caused by platform font metrics.
- Stabilizes the TX panel band bar with deterministic widths and monospaced labels.
- Stabilizes TX/CQ buttons:
  - fixed internal content layout;
  - native Qt padding/insets set to zero;
  - centered monospaced labels.
- This keeps icon and button placement consistent across Windows and Linux.

### Version and release

- Updates `fork_release_version.txt` to `1.0.399`.
- Updates Windows Inno/NSIS installer defaults to `1.0.399`.
- Uses `v1.0.399` as the operational release tag, matching `v1.0.398`, so the `v*` runners start without creating a duplicate non-prefixed release.
- Expected assets include Windows x64 installer, macOS Apple Silicon DMGs, macOS Intel DMGs, Linux x86_64 AppImage, Linux aarch64 AppImage and SHA256 files.
