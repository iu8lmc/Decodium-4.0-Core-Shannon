# Decodium 4 FT2 1.0.483

## Release scope

This release contains the complete local validation and integration work since
1.0.481, including the upstream 1.0.482 baseline and the follow-up audio,
JT65 and JT9 changes validated on the macOS legacy backend.

## English

### 1.0.482 baseline included

- Moved Windows application settings from the registry to INI storage with
  automatic first-run migration and clean uninstall handling.
- Fixed the spectrum/waterfall splitter recovery when an old saved height was
  outside the usable panel range.
- Stabilized diagnostic-log path resolution.
- Added QRZ.com callsign lookup from decode-list context menus.

### 1.0.483 changes

- **macOS legacy audio capture**:
  - virtual input devices such as BlackHole, Soundflower, Loopback, VB-Cable
    and Virtual Audio now use an explicit QAudioSource pull path;
  - physical macOS inputs keep the native AudioQueue path;
  - pull buffering is frame-aligned and bounded, with clean stream retirement
    and no repeated audio reopen loop;
  - optional `DECODIUM_LEGACY_AUDIO_TRACE` diagnostics report callback reads,
    accepted frames, peak, RMS and non-zero sample ratio.
- **JT65 transmission and validation**:
  - added bridge PCM TX generation for JT65 at the reference symbol timing;
  - corrected JT65 encoder initialization and comparison utilities;
  - added waveform export support for repeatable BlackHole and WAV tests.
- **JT9 transmission and decoding**:
  - added bridge PCM TX generation for JT9-1 with the correct 60-second slot
    timing and 1.736111 Hz tone spacing;
  - corrected JT9 message normalization for blank and padded startup messages;
  - corrected JT9 narrow soft-symbol timing and downsampling assumptions;
  - added FFT-plan cleanup and optional `DECODIUM_JT9_TRACE` diagnostics for
    decoder dispatch, soft-spectrum contents and returned rows;
  - preserved valid UTC slot timing in lab tests instead of starting JT9 at an
    arbitrary instant.
- **Legacy decoder scheduling**:
  - JT4, JT65 and JT9 decode dispatch now tolerates block-sized audio callbacks
    that cross the stop boundary, avoiding missed decode triggers;
  - a real audio callback resets the one-shot recovery guard, while repeated
    reopen attempts are suppressed to keep the UI responsive.
- **Comparison and regression coverage**:
  - extended CMake targets for JT65 reference comparisons when an official
    WSJT-X Fortran archive is supplied;
  - added `jt9_narrow_compare` for deterministic generated-waveform decode
    validation;
  - expanded JT9/JT65 encoder, sync, extraction and decode checks.

### Validation

- `decodium_qml` built successfully on macOS arm64 with Qt 6.11 and Metal.
- `jt9_narrow_compare` completed successfully, including:
  `JT9 narrow decode compare passed`.
- `git diff --check` completed without whitespace errors.
- Generated test recordings and external build trees remain local under
  `tmp/` and are intentionally not packaged as source or release assets.

### Release assets

GitHub Actions produce the Windows x64 installer, macOS Apple Silicon DMGs,
macOS Intel DMGs, Linux x86_64 AppImage, Linux aarch64 AppImage and matching
archives/checksums where provided by each workflow. The tagged source tree is
also available through GitHub's source archives.

## Italiano

### Baseline 1.0.482 incluso

- Spostate su Windows le impostazioni applicative dal registro ai file INI,
  con migrazione automatica al primo avvio e disinstallazione pulita.
- Corretto il recupero dello splitter spettro/waterfall quando una vecchia
  altezza salvata era fuori dall'area utilizzabile.
- Stabilizzato il percorso del log diagnostico.
- Aggiunta la ricerca QRZ.com dal menu contestuale delle decodifiche.

### Modifiche 1.0.483

- **Acquisizione audio legacy macOS**:
  - i dispositivi virtuali come BlackHole, Soundflower, Loopback, VB-Cable e
    Virtual Audio usano ora un percorso QAudioSource pull esplicito;
  - gli ingressi fisici macOS mantengono il percorso nativo AudioQueue;
  - buffering pull allineato ai frame, limitato e con chiusura corretta dello
    stream, senza riaperture audio ripetute;
  - diagnostica opzionale `DECODIUM_LEGACY_AUDIO_TRACE` con letture callback,
    frame accettati, picco, RMS e percentuale di campioni non nulli.
- **Trasmissione e validazione JT65**:
  - aggiunta la generazione PCM TX JT65 nel bridge con timing di riferimento;
  - corretta l'inizializzazione dell'encoder JT65 e delle utility di confronto;
  - aggiunto il dump waveform per test ripetibili via BlackHole e WAV.
- **Trasmissione e decodifica JT9**:
  - aggiunta la generazione PCM TX JT9-1 con slot corretto da 60 secondi e
    spaziatura toni di 1.736111 Hz;
  - corretta la normalizzazione dei messaggi JT9 vuoti o con padding;
  - corretti timing soft-symbol e assunzioni di downsampling del decoder JT9;
  - aggiunta la distruzione dei piani FFT e diagnostica opzionale
    `DECODIUM_JT9_TRACE`;
  - mantenuto il corretto allineamento UTC nei test lab JT9.
- **Scheduling decoder legacy**:
  - JT4, JT65 e JT9 gestiscono ora callback audio a blocchi che attraversano il
    limite di fine finestra, evitando di saltare l'avvio della decodifica;
  - un callback audio reale riattiva il controllo di recupero, mentre i tentativi
    di riapertura ripetuti vengono limitati per mantenere reattiva la GUI.
- **Confronti e regressioni**:
  - estesi i target CMake per il confronto JT65 con l'archivio Fortran ufficiale
    WSJT-X quando fornito;
  - aggiunto `jt9_narrow_compare` per la validazione deterministica della
    decodifica su waveform generato;
  - ampliati i controlli encoder, sync, extraction e decode JT9/JT65.

### Validazione

- `decodium_qml` compilato correttamente su macOS arm64 con Qt 6.11 e Metal.
- `jt9_narrow_compare` completato correttamente con il risultato
  `JT9 narrow decode compare passed`.
- `git diff --check` completato senza errori di whitespace.
- Registrazioni di test e build esterne restano localmente sotto `tmp/` e non
  vengono inserite nel codice o negli asset della release.

### Asset della release

Le GitHub Actions producono installer Windows x64, DMG macOS Apple Silicon,
DMG macOS Intel, AppImage Linux x86_64, AppImage Linux aarch64 e relativi
archivi/checksum quando previsti dai workflow. Il codice sorgente taggato e'
disponibile anche tramite gli archivi sorgente automatici di GitHub.
