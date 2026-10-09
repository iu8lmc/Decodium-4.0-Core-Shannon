# Decodium 4 FT2 1.0.435

Release di manutenzione focalizzata sul timing FT4 live e sulla protezione dei computer meno potenti, mantenendo attivi i miglioramenti di sensibilita' introdotti nelle versioni precedenti.

## Novita'

- FT4 ora rilascia i decode in modo progressivo: una prima passata rapida viene pubblicata subito, mentre la passata profonda successiva aggiunge solo i decode nuovi.
- Il follow-up FT4 riusa lo stesso audio dello slot e deduplica le righe gia' mostrate, evitando duplicati in Full Spectrum e in ALL.TXT.
- La prima consegna FT4 resta prioritaria rispetto alla passata profonda, cosi' i decode utili arrivano entro la finestra operativa dello slot.
- La passata profonda viene avviata solo se la prima passata e' arrivata in tempo; in caso contrario viene saltata per non accumulare ritardo sullo slot successivo.

## Correzioni

- `depth4` FT4 non viene piu' attivato implicitamente dal vecchio bitmask `NDepth`.
- `depth4` FT4 live viene consentito solo con profilo esplicito `DEEP + AP`, CPU non sotto pressione e almeno 16 thread effettivi.
- In Low CPU mode o sotto CPU pressure FT4 viene ridotto automaticamente, anche se l'utente ha attivato DEEP e AP.
- Il bridge moderno usa una passata early FT4 rapida, allineata al comportamento del backend legacy.
- Ridotto il logging per-decode sul path caldo: i dettagli `enqueueDecod`, `callB4`, `callB4onBand` e `isDx` sono ora disponibili solo con `DECODIUM_VERBOSE_DECODE_POST_LOG=1`.
- Il file `debug.txt` viene aperto direttamente in append testuale, riducendo overhead su log molto grandi.

## Verifica

- Build locale `decodium_qml` completata con successo.
- Test live FT4 su backend legacy: prima consegna rapida entro la finestra FT4, follow-up profondo rilasciato dopo senza bloccare la prima pubblicazione.

## Build

- Codice sorgente allegato automaticamente da GitHub.
- Windows x64: installer generato dal runner Windows.
- macOS Apple Silicon: DMG/ZIP generati dal runner dedicato.
- macOS Intel: DMG/ZIP generati dal runner dedicato.
- Linux x86_64: AppImage generata dal runner dedicato.
- Linux aarch64: AppImage generata dal runner dedicato.
