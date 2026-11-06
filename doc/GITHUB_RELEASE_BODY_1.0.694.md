# Decodium 4 v1.0.694

## English (British)

Decodium 4 v1.0.694 is the fork release of v1.0.693. It preserves the serial-port ownership, CAT and CW recovery, DecoPort diagnostics and direct-CAT Power On / Power Off fixes, and adds the fork-specific Windows packaging and release contract.

### Changes since 1.0.693

- Added the fork release metadata and repository contract for v1.0.694, including the bilingual release body used by the CI validation and publishing workflows.
- Added the Windows x64 packaging path for this fork, including com0com, Hamlib and the installer payload.
- Kept the tested serial-port registry and DecoPort ownership diagnostics from v1.0.693: CAT, CW keyer, WinKeyer, PTT and DecoPort lines reserve ports through one registry, while serial ports are never opened merely to inspect them.
- The release workflows publish the source archive, Windows x64 installer, macOS Intel and Apple Silicon DMGs, and Linux x86_64 and aarch64 AppImages with checksums where provided.

### Notes

- Verified on a Yaesu FT-991 over direct USB: Power Off followed by Power On, without restarting Decodium.
- Use a dummy load for the first transmissions of the voice keyer, as in 1.0.686.

## Italiano

Decodium 4 v1.0.694 è la release del fork basata sulla v1.0.693. Mantiene la proprietà delle porte seriali, il recupero CAT e CW, la diagnostica DecoPort e le correzioni Power On / Power Off per le radio con CAT diretto, aggiungendo il packaging Windows e il contratto di release del fork.

### Modifiche dalla 1.0.693

- Aggiunti i metadati e il contratto di repository della release 1.0.694, inclusa la descrizione bilingue usata dai workflow CI e di pubblicazione.
- Aggiunto il percorso di packaging Windows x64 del fork, con com0com, Hamlib e installer.
- Mantenute le correzioni testate della v1.0.693 per il registro seriale e la diagnostica DecoPort: CAT, keyer CW, WinKeyer, PTT e linee DecoPort riservano le porte tramite un unico registro e le porte seriali non vengono aperte solo per controllarle.
- I workflow pubblicano l archivio sorgente, l installer Windows x64, i DMG macOS Intel e Apple Silicon e gli AppImage Linux x86_64 e aarch64, con checksum dove previsti.

### Note

- Verificata con una Yaesu FT-991 via USB diretto: Power Off seguito da Power On, senza riavviare Decodium.
- Per le prime trasmissioni del voice keyer usa un carico fittizio, come nella 1.0.686.

## Downloads

The source archive, Windows x64 installer EXE, macOS Intel and Apple Silicon DMGs, and Linux x86_64 and aarch64 AppImages are attached to this release by the corresponding GitHub Actions workflows.
