# macOS Porting Notes / Note Porting macOS - Fork v1.2.0

Date / Data: 2026-02-26
Target branch: `merge/raptor-v110-port`

---

## English

## Objective

Keep Decodium Raptor base and make it stable on macOS for continuous FT2 operation with real rigs (CAT/PTT + USB audio).

## 1) Bundle and executable migration (`ft2.app`)

### Problem

Legacy assumptions still expected `wsjtx.app` paths in several runtime/build points.

### Applied

- Introduced `WSJT_APP_BUNDLE_NAME` and `WSJT_APP_EXECUTABLE_NAME` in CMake.
- Switched bundle fixup/copy logic to app variables.
- Set executable name to `ft2`.
- Ensured `jt9` is copied beside `ft2` in `Contents/MacOS`.

### Outcome

`ft2.app` starts `jt9` reliably without legacy path mismatches.

## 2) Application namespace isolation

- Set `QApplication::setApplicationName("ft2")`.
- Avoid profile collision with WSJT-X data.
- Keep settings under dedicated macOS app-support namespace.

## 3) Microphone permission preflight

### Problem

macOS consent popup could appear late during operation and disrupt TX/audio state.

### Applied

- Startup input preflight with short `QAudioInput` open/close.
- Prompt appears at startup time.

## 4) TX/PTT/audio race hardening

- Added PTT request timestamping and guarded delayed-start path.
- Added fallback start if CAT/PTT ack is late.
- Cleared pending timers/flags on stop/restart.
- Added synchronous modulator stop handling to prevent stale state.
- Added support logging in `~/Library/Application Support/ft2/tx-support.log`.

## 5) Audio persistence hardening

- Revalidated stored channel values against runtime capabilities.
- Auto-fallback to valid channels when persisted values are invalid.
- Reduced silent-TX cases caused by stale device/channel state.

## 6) First run command on macOS

If Gatekeeper quarantine blocks the app, run:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## 7) CI release packaging

`build-macos.yml` currently produces per-target `.zip`, `.tar.gz`, and `.sha256` artifacts for:

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia

---

## Italiano

## Obiettivo

Mantenere la base Decodium Raptor e renderla stabile su macOS per uso FT2 continuo con radio reali (CAT/PTT + audio USB).

## 1) Migrazione bundle ed eseguibile (`ft2.app`)

### Problema

Restavano assunzioni legacy su path `wsjtx.app` in vari punti runtime/build.

### Applicato

- Introdotti `WSJT_APP_BUNDLE_NAME` e `WSJT_APP_EXECUTABLE_NAME` in CMake.
- Conversione logica bundle fixup/copy alle variabili app.
- Nome eseguibile impostato a `ft2`.
- `jt9` copiato accanto a `ft2` in `Contents/MacOS`.

### Risultato

`ft2.app` avvia `jt9` in modo affidabile senza mismatch di path legacy.

## 2) Isolamento namespace applicazione

- Impostato `QApplication::setApplicationName("ft2")`.
- Evitata collisione con profili dati WSJT-X.
- Impostazioni mantenute in namespace dedicato macOS.

## 3) Preflight permesso microfono

### Problema

Il popup consenso microfono di macOS poteva apparire in ritardo durante l'operativita, disturbando lo stato TX/audio.

### Applicato

- Preflight input all'avvio con apertura/chiusura breve `QAudioInput`.
- Popup mostrato in fase di startup.

## 4) Hardening race TX/PTT/audio

- Timestamp richieste PTT e percorso start ritardato con guardia.
- Fallback start se ack CAT/PTT in ritardo.
- Pulizia timer/flag pendenti su stop/restart.
- Stop sincrono modulator per evitare stati stale.
- Logging supporto in `~/Library/Application Support/ft2/tx-support.log`.

## 5) Hardening persistenza audio

- Rivalidazione canali salvati rispetto alle capacita runtime.
- Fallback automatico su canali validi se il valore salvato non e valido.
- Ridotti casi TX muta da stato device/canale obsoleto.

## 6) Comando primo avvio macOS

Se la quarantena Gatekeeper blocca l'app, eseguire:

```bash
sudo xattr -r -d com.apple.quarantine /Applications/ft2.app
```

## 7) Packaging CI release

`build-macos.yml` produce attualmente per target: `.zip`, `.tar.gz` e `.sha256` per:

- Apple Silicon Tahoe
- Apple Silicon Sequoia
- Apple Intel Sequoia
