# Decodium 4 FT2 1.0.65

Release 1.0.65 raccoglie i fix successivi a 1.0.64 su TX, CAT/Fake It, schedulazione automatica e UI decode.

## Highlights

- TX: allineamento del timing FT2/FT4/FT8 al comportamento Decodium3 per evitare lo slittamento di uno slot.
- TX audio: mantenuto il fix del payload e della riduzione di attenuazione dopo Tune/TX.
- CAT Hamlib: Fake It ora invia frequenza TX e PTT come stato coerente, con split attivo quando serve.
- Auto TX Windows: riarmo dello scheduler quando TX/AutoCQ risulta armato ma lo stato interno non è sincronizzato.
- Panadapter: riarmo del feed dopo la prima trasmissione senza dover riavviare Monitor.
- Live map / decode table: allineamento colonne distanza, DXCC/country e azimuth.
- Cluster: soppressione degli errori non bloccanti durante AutoSpot e prompt-to-log.

## Validazione

- Build locale `decodium_qml` completata su macOS.
- `git diff --check` pulito.
- Fix multipiattaforma nei path condivisi C++/QML, con workflow GitHub separati per Windows e macOS Apple Silicon.

## Assets attesi

- `Decodium_1.0.65_Setup_x64.exe`
- `decodium4-ft2-1.0.65-linux-aarch64.AppImage`
- `decodium4-ft2-1.0.65-linux-aarch64.AppImage.sha256.txt`
- `decodium4-ft2-1.0.65-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.65-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.65-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.65-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.65-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.65-macos-sequoia-arm64-sha256.txt`
