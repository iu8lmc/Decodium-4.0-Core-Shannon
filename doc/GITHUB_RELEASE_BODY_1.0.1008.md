# Decodium 4 FT2 1.0.1008

## English (UK)

Release 1.0.1008 is the iu8lmc fork build with the Claude Code web work
merged, plus the runtime packaging script.

- FT8 decoding: BICM-ID second version (gated, mean-of-five feedback, three
  rounds), OSD (174,77), margin gate and phase trellis, with bit-metric speed-up
  and comparison benches (fork PR #117, draft; not yet validated on air).
- Windows CI: corrections for failures already present on main (SSTV,
  feature-off builds, ASan, MinGW GCC 16 warnings) (fork PR #118, draft).
- Project permissions in `.claude/settings.json`.
- `packaging/windows/stage_runtime.ps1` rebuilds the installer runtime folder
  (Qt without FFmpeg, MinGW and third-party DLLs with their indirect
  dependencies, Decodium QML folders, data and translations).
- The RR73 issue in Hound mode is still open.

## Italiano

La release 1.0.1008 è la build del fork iu8lmc con il lavoro delle sessioni
Claude Code web unito, più lo script di preparazione del runtime.

- Decodifica FT8: BICM-ID seconda versione (gate, feedback su cinque, tre giri),
  OSD (174,77), gate a margine e traliccio di fase, con accelerazione delle
  metriche dei bit e banchi di confronto (PR #117 del fork, in bozza; non ancora
  validata in aria).
- CI Windows: correzioni per i fallimenti già presenti su main (SSTV, build con
  funzioni disattivate, ASan, avvisi MinGW GCC 16) (PR #118 del fork, in bozza).
- Permessi di progetto in `.claude/settings.json`.
- `packaging/windows/stage_runtime.ps1` ricostruisce la cartella di runtime
  dell'installer (Qt senza FFmpeg, DLL MinGW e di terze parti con le dipendenze
  indirette, cartelle QML di Decodium, dati e traduzioni).
- Il problema RR73 in modalità Hound è ancora aperto.
