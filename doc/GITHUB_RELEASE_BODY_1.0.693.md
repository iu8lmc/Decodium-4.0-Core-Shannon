# Decodium 4 v1.0.693

This release contains the DecoPort hub work from v1.0.692 and the serial ownership, CAT/CW recovery, diagnostics and interface fixes completed for v1.0.693.

## English (British)

### Highlights from v1.0.691 to v1.0.693

- Added a central process-wide serial-port registry with atomic reservations and RAII release for Hamlib CAT, CW serial keyer, WinKeyer, PTT, DecoPort serial lines and CAT multiplexers.
- Serialised radio startup and recovery. CAT and DecoPort ownership is established before CW services; conflicting opens are queued and retried, without blocking the GUI.
- Improved diagnostics so the Port Hub identifies the Decodium component that owns an internally reserved port instead of incorrectly blaming an external program.
- Added concurrent tests for path aliases, all-or-nothing multi-port reservations and CAT-versus-CW contention.
- Fixed DecoPort Port Hub layout so long device names and owner descriptions are kept in separate, elided columns.
- Unified the SSB microphone item in the main menu with the other Decodium entries.

### Verification

- macOS arm64 `decodium_qml` build completed successfully.
- Focused CAT, DecoPort, CW, WinKeyer and serial-registry tests: 9/9 passed.

## Italiano

Questa release comprende il lavoro DecoPort hub della v1.0.692 e le correzioni di proprieta delle porte seriali, recupero CAT/CW, diagnostica e interfaccia completate nella v1.0.693.

### Novita dalla v1.0.691 alla v1.0.693

- Aggiunto un registro centrale delle porte seriali con prenotazioni atomiche e rilascio RAII per CAT Hamlib, keyer seriale CW, WinKeyer, PTT, linee seriali DecoPort e multiplexer CAT.
- Serializzati avvio e recupero della radio. La proprieta CAT e DecoPort viene acquisita prima dei servizi CW; le aperture in conflitto vengono accodate e ritentate senza bloccare la GUI.
- Migliorata la diagnostica: il Port Hub identifica il componente Decodium che possiede internamente una porta riservata, senza attribuire erroneamente il blocco a un programma esterno.
- Aggiunti test concorrenti per alias dei percorsi, prenotazioni atomiche multiporta e contesa tra CAT e CW.
- Corretto il layout del Port Hub DecoPort: nomi lunghi e descrizioni del proprietario restano in colonne separate e vengono troncati correttamente.
- Uniformata la voce SSB microphone nel menu principale alle altre voci di Decodium.

### Verifica

- Build macOS arm64 di `decodium_qml` completata correttamente.
- Test mirati CAT, DecoPort, CW, WinKeyer e registro seriale: 9/9 superati.

## Downloads

The source archive, Windows x64 executable, macOS Apple Silicon and Intel DMGs, and Linux x86_64 and aarch64 AppImages are published by the corresponding GitHub Actions workflows.
