# Decodium 4 FT2 1.0.64

Release 1.0.64 consolida i fix audio TX/RX introdotti dopo 1.0.63 e stabilizza il path legacy embedded su macOS.

## Highlights

- TX: payload FT2/FT4/FT8 su macOS instradato dal bridge audio, evitando il doppio stream legacy sullo stesso USB CODEC.
- Tune: gestione bridge separata dal payload digitale, con stop affidabile.
- Panadapter: riarmo forzato del feed PCM legacy dopo TX/PTT-off, senza dover spegnere e riaccendere Monitor.
- RX: blocco della riapertura del capture Qt moderno quando il backend legacy possiede audio e panadapter.
- UI: barra RX/TX resa coerente con lo stato reale di trasmissione.
- Cluster: soppressione degli errori non bloccanti durante AutoSpot e prompt-to-log.
- Modi: fix cambio frequenza in WSPR.

## Validazione

- Build locale `decodium_qml` completata su macOS.
- Test runtime manuale: Tune OK, payload FT2/FT4/FT8 OK, panadapter riarmato post-TX.

## Assets attesi

- `Decodium_1.0.64_Setup_x64.exe`
- `decodium4-ft2-1.0.64-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.64-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.64-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.64-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.64-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.64-macos-sequoia-arm64-sha256.txt`
