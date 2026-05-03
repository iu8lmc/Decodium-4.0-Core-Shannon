# Decodium 4 FT2 1.0.69

Release 1.0.69 raccoglie i fix successivi alla 1.0.68 su panadapter/waterfall, log, pannelli decode e persistenza impostazioni.

## Cambiamenti principali

- Stabilizzato il waterfall su Windows/Direct3D11: il path shader sperimentale resta disabilitato di default e viene usato il caricamento texture colorata con fallback robusto.
- Mantenuto il path Metal sicuro su macOS con fallback CPU/texture quando lo shader non e' abilitato o non e' stabile.
- Ridotto l'indicatore GPU nella status bar ai soli FPS, senza barra colore.
- Migliorato lo spacing del QSO Log tra data/ora e nominativo, e aumentata l'altezza dei campi edit inferiori per evitare testo tagliato.
- Aggiunto spazio tra `DT`, `Freq` e `Message` nei pannelli decode, sia docked sia floating.
- Reso piu' robusto il salvataggio delle impostazioni dal dialogo Settings: salvataggio debounce dopo modifiche live e salvataggio esplicito alla chiusura.

## Asset previsti

- `Decodium_1.0.69_Setup_x64.exe`
- `decodium4-ft2-1.0.69-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.69-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.69-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.69-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.69-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.69-macos-sequoia-arm64-sha256.txt`
