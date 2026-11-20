# Decodium 4 v1.0.700

## English (British)

This release contains the fixes developed after v1.0.699 for the classic
dashboard appearance and Windows CAT serial connection safety.

### Dashboard and Liquid Glass fallback

- Fixed the main hamburger menu becoming transparent and unreadable when
  Liquid Glass was disabled or unavailable.
- The main menu and language submenu now use the opaque `bgDeep` theme surface
  in classic mode, so dashboard content cannot bleed through the menu text and
  controls.
- The same opaque fallback is applied consistently to detached windows,
  dialogs and popups using the shared surface helpers.
- Liquid Glass behaviour is unchanged when the theme and platform support
  native transparency.
- Added a regression test that verifies both shared main-window surface helpers
  keep an opaque classic fallback.

### Windows CAT serial handshake safety

- Fixed the CAT configuration path for radios using `Handshake: Default`.
- Force DTR and Force RTS are now disabled until an explicit handshake mode is
  selected; they are not passed to the serial opening policy while Hamlib is
  still resolving the default.
- This prevents Windows connections such as COM7 from being opened with an
  incompatible hardware-handshake/forced-RTS combination, which previously
  produced `cannot set RTS with hardware handshake` and `Invalid configuration`.
- Explicit DTR, RTS and hardware-handshake configurations retain their intended
  behaviour.
- Added focused CAT serial regression coverage for the default-handshake case.

### Validation and published assets

- The repository version is aligned to `1.0.700` and the source is tagged
  `v1.0.700`.
- The release contains the source code archive and the Windows x64 installer
  executable.
- GitHub Actions builds and publishes macOS Apple Silicon DMGs for the supported
  macOS runner targets and macOS Intel DMGs for the supported Intel targets.
- GitHub Actions builds and publishes Linux x86_64 and Linux aarch64 AppImages,
  together with SHA-256 checksum files.
- Local validation includes `git diff --check`, the `decodium_qml` build, the
  CAT serial safety tests and the Liquid Glass window tests.

## Italiano

Questa release contiene le correzioni sviluppate dopo la v1.0.699 per
l'aspetto della dashboard classica e la sicurezza della connessione CAT seriale
su Windows.

### Dashboard e fallback Liquid Glass

- Corretto il menu hamburger principale che diventava trasparente e
  illeggibile quando Liquid Glass era disattivato o non disponibile.
- Il menu principale e il sottomenu della lingua usano ora la superficie opaca
  del tema `bgDeep` in modalità classica, impedendo alla dashboard sottostante
  di confondersi con testo e controlli.
- Lo stesso fallback opaco viene applicato in modo coerente a finestre esterne,
  dialoghi e popup che usano gli helper condivisi delle superfici.
- Il comportamento Liquid Glass resta invariato quando trasparenza nativa e
  tema sono supportati dalla piattaforma.
- Aggiunto un test di regressione che verifica il fallback opaco dei due helper
  principali delle superfici.

### Sicurezza handshake CAT seriale su Windows

- Corretto il percorso di configurazione CAT per le radio con
  `Handshake: Default`.
- Force DTR e Force RTS vengono ora disabilitati finché non viene selezionato
  un handshake esplicito e non vengono passati alla politica di apertura della
  seriale mentre Hamlib sta ancora risolvendo il valore predefinito.
- Questo evita che connessioni Windows come COM7 vengano aperte con una
  combinazione incompatibile di handshake hardware e RTS forzato, che prima
  produceva `cannot set RTS with hardware handshake` e `Invalid configuration`.
- Le configurazioni esplicite DTR, RTS e handshake hardware mantengono il
  comportamento previsto.
- Aggiunti test mirati per la sicurezza CAT seriale con handshake predefinito.

### Validazione e asset pubblicati

- La versione del repository è allineata a `1.0.700` e il codice è taggato
  `v1.0.700`.
- La release contiene l'archivio del codice sorgente e l'eseguibile installer
  Windows x64.
- I GitHub Actions generano e pubblicano i DMG macOS Apple Silicon per i runner
  macOS supportati e i DMG macOS Intel per i runner Intel supportati.
- I GitHub Actions generano e pubblicano le AppImage Linux x86_64 e Linux
  aarch64, insieme ai file checksum SHA-256.
- La validazione locale comprende `git diff --check`, la build `decodium_qml`,
  i test di sicurezza CAT seriale e i test delle finestre Liquid Glass.
