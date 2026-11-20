# Decodium 4 FT2 1.0.398

Changes from `1.0.397` to `1.0.398`.

This release focuses on decoder diagnostics and mode-switch stability. It keeps the `1.0.397` decoder and release-runner base, adds targeted FT8 tracing for JTDX-only missing decodes, and hardens FT2/FT4/FT8 transitions so changing mode cannot leave stale decoder work running behind the UI.

## Italiano

### Trace FT8 per i decode mancanti

- Aggiunto un trace FT8 "expected target" per investigare i segnali che JTDX decodifica ma Decodium non recupera ancora.
- Il trace permette di fissare target attesi su uno specifico WAV e seguire il percorso del candidato dentro lo stage FT8.
- L'obiettivo e' capire in quale fase il segnale muore: sync/candidate harvest, pruning, AP/A7, bitmetric, Fano o filtro finale.
- Il lavoro e' pensato per i casi deboli osservati nel confronto JTDX, in particolare i segnali intorno a `-24/-25 dB`.

### Stabilita' cambio modo FT2/FT4/FT8

- Risolto un problema in cui, dopo il passaggio tra `FT2`, `FT4` e `FT8`, Decodium poteva continuare a inviare audio al decoder ma non ricevere piu' risultati utili.
- Il cambio modo ora invalida esplicitamente i decode in coda e resetta lo stato temporaneo FTx:
  - serial e sessioni decode;
  - early decode FT8/FT4;
  - deep follow-up FT8;
  - callback stale;
  - stato `decoding` della UI.
- FT8 e FT2 ricevono una cancellazione cooperativa del decode corrente al cambio modo.
- Le chiamate asincrone ai worker FT2/FT4/FT8 ora catturano il worker corretto al momento del dispatch, evitando che una lambda vecchia usi il worker sbagliato dopo uno switch di modo.
- La sorgente audio puo' restare aperta, ma lo stato dei decoder viene separato e ripulito in modo deterministico.

### Diagnostica utenti

- Il fix risponde ai log in cui si vedevano sequenze come:
  - `setMode: preserving active QAudioSource for mode change FT2 -> FT8`;
  - `feedAudioToDecoder: mode=FT8 samples=180000`;
  - assenza di callback decode utili;
  - `ft8 did not finish in 15s`;
  - `ft4 did not finish in 15s`.
- Questi casi non erano semplicemente "audio assente": l'audio arrivava, ma i worker potevano restare in uno stato sporco dopo il cambio modo.

### Release infrastructure

- Versione locale aggiornata a `1.0.398` tramite `fork_release_version.txt`.
- Il tag operativo della release e' `v1.0.398`, cosi' i runner che ascoltano `v*` partono correttamente senza creare una seconda release parallela non prefissata.
- Gli asset attesi sono:
  - Source code archive generato automaticamente da GitHub per `v1.0.398`;
  - `Decodium_1.0.398_Setup_x64.exe`;
  - `decodium4-ft2-1.0.398-macos-tahoe-arm64.dmg`;
  - `decodium4-ft2-1.0.398-macos-sequoia-arm64.dmg`;
  - `decodium4-ft2-1.0.398-macos-ventura-x86_64.dmg`;
  - `decodium4-ft2-1.0.398-macos-sonoma-x86_64.dmg`;
  - `decodium4-ft2-1.0.398-macos-sequoia-x86_64.dmg`;
  - `decodium4-ft2-1.0.398-linux-x86_64.AppImage`;
  - `decodium4-ft2-1.0.398-linux-aarch64.AppImage`;
  - file SHA256 generati dai runner macOS e Linux.

### Validazione locale

- Build locale del target QML:
  - `cmake --build /Users/salvo/Desktop/Decodium4-build --target decodium_qml --parallel 4`
- Controllo whitespace:
  - `git diff --check`

## English

### FT8 missing-decode trace

- Adds an FT8 "expected target" trace for investigating signals decoded by JTDX but still missed by Decodium.
- The trace can pin expected targets on a specific WAV and follow the candidate through the FT8 stage.
- The goal is to identify the exact failing stage: sync/candidate harvest, pruning, AP/A7, bitmetrics, Fano or final filtering.
- This is aimed at weak JTDX-only cases observed around `-24/-25 dB`.

### FT2/FT4/FT8 mode-switch stability

- Fixes a case where Decodium could keep feeding audio to the decoder after switching between `FT2`, `FT4` and `FT8`, while no useful decode callbacks returned.
- Mode changes now explicitly invalidate queued decode work and reset temporary FTx state:
  - decode serials and sessions;
  - FT8/FT4 early decode state;
  - FT8 deep follow-up state;
  - stale callbacks;
  - UI `decoding` state.
- FT8 and FT2 receive cooperative cancellation of the current decode when the mode changes.
- Async dispatch calls now capture the worker pointer selected at dispatch time, avoiding stale lambdas reading a different worker after a mode switch.
- RX audio can stay open, but decoder state is now cleaned independently and deterministically.

### User diagnostics

- This addresses diagnostic logs showing:
  - `setMode: preserving active QAudioSource for mode change FT2 -> FT8`;
  - `feedAudioToDecoder: mode=FT8 samples=180000`;
  - no useful decode callbacks;
  - `ft8 did not finish in 15s`;
  - `ft4 did not finish in 15s`.
- These were not pure "no audio" cases: audio was present, but decoder workers could remain dirty after mode changes.

### Release infrastructure

- Updates `fork_release_version.txt` to `1.0.398`.
- Uses `v1.0.398` as the release tag so the `v*` runners start correctly without creating a duplicate non-prefixed release.
- Expected assets include Windows x64 installer, macOS Apple Silicon DMGs, macOS Intel DMGs, Linux x86_64 AppImage, Linux aarch64 AppImage and SHA256 files.
