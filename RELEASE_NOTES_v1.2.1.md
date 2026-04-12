# Release Notes / Note di Rilascio - Fork v1.2.1

Date: 2026-02-27  
Scope: UI cleanup, map freshness, drift UI removal.

## English

### Summary

- Removed soundcard drift display and alternation with DT; drift computation disabled in Detector.
- World map contacts expire after 2 minutes to avoid stale callers.
- Program title and docs bumped to fork `v1.2.1`.

### Known issues / Workarounds

- If Gatekeeper blocks the app after download/install, run:
  ```bash
  sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
  ```
- OpenMP remains optional; install `libomp` if you need it on macOS:
  ```bash
  brew install libomp
  ```

## Italiano

### Sintesi

- Rimossa la visualizzazione del drift scheda audio e l’alternanza con DT; calcolo drift disattivato nel Detector.
- I contatti della mappa scadono dopo 2 minuti per evitare chiamate fantasma.
- Titolo programma e documentazione aggiornati alla fork `v1.2.1`.

### Problemi noti / Workaround

- Se Gatekeeper blocca l’app dopo il download/installazione, eseguire:
  ```bash
  sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
  ```
- OpenMP resta opzionale; installare `libomp` se serve su macOS:
  ```bash
  brew install libomp
  ```
