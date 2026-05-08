# Decodium 4 FT2 1.0.107

Release di manutenzione focalizzata sulla compatibilita' Ham Radio Deluxe.

## Correzioni principali

- Reso piu' tollerante l'handshake iniziale HRD su Windows: Decodium ora attende piu' a lungo la risposta al comando `get context`, allineandosi meglio al comportamento di JTDX/WSJT-X sui sistemi lenti.
- Aggiunto il reset automatico dei tentativi CAT rimasti pendenti, evitando lo stato persistente "connessione gia' in corso" dopo un tentativo HRD bloccato.
- Esteso il watchdog specifico HRD per coprire il nuovo tempo di probe senza bloccare indefinitamente l'interfaccia.

## Asset previsti

- `Decodium_1.0.107_Setup_x64.exe`
- `decodium4-ft2-1.0.107-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.107-linux-x86_64.AppImage.sha256.txt`
- `decodium4-ft2-1.0.107-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.107-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.107-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.107-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.107-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.107-macos-sequoia-arm64-sha256.txt`
