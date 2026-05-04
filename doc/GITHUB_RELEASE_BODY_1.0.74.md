# Decodium 4 FT2 1.0.74

Release 1.0.74 raccoglie i fix successivi alla 1.0.73 su CAT/Ham Radio Deluxe, gestione locator, log B4 per banda e diagnostica Windows.

## Cambiamenti principali

- Ripristinata la tolleranza di Decodium3 nella connessione Ham Radio Deluxe: handshake e lettura iniziale HRD attendono piu' a lungo sui sistemi Windows lenti.
- Aggiunta diagnostica HRD piu' chiara per connessione TCP, probe protocollo v5/v4 e timeout su `get context`.
- Normalizzati endpoint HRD/Hamlib inseriti in modo incompleto o malformato, inclusi host-only `127.0.0.1` e stringhe con IPv4 incorporato.
- Corretta la visualizzazione dei messaggi con locator, evitando inversioni fuorvianti nei casi standard `MYCALL DX GRID`.
- Reso il flag B4 sensibile alla banda corrente, mantenendo separata l'informazione worked-ever per i controlli globali.

## Asset previsti

- `Decodium_1.0.74_Setup_x64.exe`
- `decodium4-ft2-1.0.74-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.74-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.74-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.74-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.74-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.74-macos-sequoia-arm64-sha256.txt`
