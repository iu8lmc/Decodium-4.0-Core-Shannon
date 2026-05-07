# Decodium 4 FT2 1.0.105

Release ricostruita dalla base stabile 1.0.74 con cherry-pick selettivo delle parti a basso rischio.

## Punti principali

- Panadapter e waterfall su Qt RHI/Metal/Direct3D11, con upload waterfall persistente e normalizzazione dB lato shader.
- Mantenuto il motore FT2 della base 1.0.74, senza importare il nuovo engine/QSO sperimentale.
- Ripristinato il comportamento CAT `Default` stile Decodium3/WSJT-X: Hamlib riceve i parametri default invece di valori seriali forzati.
- Aggiunti Fake It, Force DTR e Force RTS nei parametri CAT.
- Fix parser callsign per non scartare chiamativi validi ambigui con grid a 4 caratteri.
- Fix audio TX sui buffer di uscita e retry TX più conservativi.
- Aggiornati workflow Windows e AppImage Linux.

## Asset previsti

- `Decodium_1.0.105_Setup_x64.exe`
- `decodium4-ft2-1.0.105-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.105-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.105-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.105-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.105-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.105-macos-sequoia-arm64-sha256.txt`
