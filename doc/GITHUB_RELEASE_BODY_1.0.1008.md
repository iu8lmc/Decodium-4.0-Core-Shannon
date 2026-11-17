# Decodium 4 FT2 1.0.1008

## English (UK)

Release 1.0.1008 is the current iu8lmc fork build with the runtime packaging
script included in the repository.

- Application code is the same as 1.0.1007.
- New: `packaging/windows/stage_runtime.ps1` reproduces the installer runtime
  folder (Qt without FFmpeg, MinGW and third-party DLLs with their indirect
  dependencies, Decodium QML folders, data and translations).
- The RR73 issue in Hound mode is still open.

## Italiano

La release 1.0.1008 è la build attuale del fork iu8lmc, con lo script di
preparazione del runtime incluso nel repository.

- Il codice dell'applicazione è lo stesso della 1.0.1007.
- Novità: `packaging/windows/stage_runtime.ps1` ricostruisce la cartella di
  runtime dell'installer (Qt senza FFmpeg, DLL MinGW e di terze parti con le
  dipendenze indirette, cartelle QML di Decodium, dati e traduzioni).
- Il problema RR73 in modalità Hound è ancora aperto.
