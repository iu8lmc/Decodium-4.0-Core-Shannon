# Decodium 4.0 v1.0.523

Versione di manutenzione dedicata alla pulizia del repository e al
riallineamento della pipeline di release dopo la v1.0.522.

## Modifiche dalla 1.0.522 alla 1.0.523

### Pulizia del repository

- Rimosso il vecchio progetto qmake `plots/`, compresa la copia duplicata di
  QCustomPlot, escluso dagli archivi sorgente e non utilizzato dalla build
  CMake attuale.
- Rimosso `qmake_only/`, che conteneva soltanto due header legacy senza
  riferimenti nella build corrente.
- Rimossa la nota storica non referenziata
  `installer/RELEASE_NOTES_1.0.418.md`.
- Rimossi file C++ residui e non inclusi da alcun target CMake attuale:
  `Detector/PctileCompat.cpp`,
  `logbook/countriesworked.cpp/.h`,
  `src/core/TraceFile.cpp/.hpp`,
  `src/core/killbyname.cpp`,
  `src/radio/HamlibTransceiverLite.cpp/.h`,
  `src/ui/AetherWaterfall.cpp/.h` e
  `src/ui/AetherWaterfallItem.cpp/.h`.

### Pulizia degli artefatti locali

Nel workspace di sviluppo sono stati rimossi gli artefatti generati e le
prove locali che non appartengono al codebase distribuito: `tmp/`,
`local-release/`, `release-local/`, `build_mingw64/`, le vecchie directory
`dist-linux-appimage*`, `build-arm-output/`, `build-logs/`, `CMakeFiles/` e i
log locali sotto `analysis/`. Questi contenuti erano output locali di build o
prove radio e non vengono inclusi nell'archivio sorgente della release.

### Versione e packaging

- Aggiornato `fork_release_version.txt` a `1.0.523`, unica sorgente della
  versione usata da CMake e dai workflow di packaging.
- La release include il codebase sorgente generato automaticamente da
  GitHub, l'installer EXE Windows x64, i pacchetti DMG/ZIP per Apple Silicon
  e Intel, e le AppImage Linux x86_64 e aarch64 quando i rispettivi runner
  completano la pubblicazione.
- Per ogni pacchetto vengono pubblicati anche i relativi file SHA-256 quando
  previsti dal workflow.

### Ambito funzionale

Questa versione non introduce modifiche funzionali al comportamento CAT o
CAT4OM all'avvio: l'analisi di quella sequenza resta separata da questa
release di manutenzione.

## Verifiche

- Risoluzione della versione con `scripts/ci/resolve-release-version.sh`.
- Verifica del layout del repository con
  `scripts/ci/validate-repository-layout.sh`.
- Controllo del diff Git con `git diff --check`.
- Build e packaging eseguiti dai runner GitHub Actions dedicati a ciascuna
  piattaforma.
