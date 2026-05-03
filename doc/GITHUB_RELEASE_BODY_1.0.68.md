# Decodium 4 FT2 1.0.68

Release 1.0.68 raccoglie i fix successivi alla 1.0.67 su TX/QSO automation, CAT, pannelli decode e panadapter.

## Cambiamenti principali

- Stabilizzata la visualizzazione actor-first dei messaggi: le righe decode mostrano il nominativo trasmittente a sinistra senza alterare il messaggio grezzo usato dalla pipeline TX.
- Corretta la persistenza dei parametri Decode Depth e Single Decode nel menu Decode.
- Migliorato il flusso QSO automatico e manuale, inclusi avanzamento verso RR73/73, disarmo TX a contatto chiuso e pulizia dei path attivi sulla mappa.
- Sistemato il layout TX: bande in scroller dedicato e controlli TX su riga/flow separato, senza sovrapposizioni.
- Rafforzato il path panadapter/waterfall: texture GPU quando sicuro, fallback CPU robusto, scheduling FFT visuale asincrono e log diagnostici `[GPUDBG]` / `[PANDBG]`.
- Aggiunte protezioni CAT su Linux per evitare keying PTT indesiderato all'apertura Hamlib quando DTR/RTS non sono usati per PTT.

## Asset previsti

- `Decodium_1.0.68_Setup_x64.exe`
- `decodium4-ft2-1.0.68-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.68-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.68-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.68-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.68-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.68-macos-sequoia-arm64-sha256.txt`
