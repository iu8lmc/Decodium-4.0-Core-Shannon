# Decodium 4 FT2 1.0.439

Release di manutenzione rispetto alla 1.0.438, con focus su TX multi-stream, backend legacy, stabilita' CAT/audio e verifiche automatiche della pipeline TX.

## Correzioni principali

- Reso compatibile il TX MAM multi-stream FT2/FT4/FT8 anche con il backend legacy macOS: quando il legacy delega l'audio al bridge, Decodium ora genera la waveform multi-stream invece di ricadere sul payload mono.
- Allineato il messaggio TX rappresentativo usato da preflight, log e cache audio: in MAM viene usato il primo stream valido solo quando il sequencer multi-stream e' realmente attivo.
- Rimosso un vincolo che disabilitava il sequencer MAM sul legacy anche nei casi in cui il bridge audio era disponibile.
- Rafforzata la tolleranza ai timeout CAT/transceiver durante poll e riconnessione, evitando disconnessioni troppo aggressive su bus USB o radio temporaneamente lente.
- Migliorata la robustezza del poller CAT durante timeout transitori, mantenendo lo stato precedente quando l'errore e' recuperabile.
- Reso piu' sicuro lo shutdown del percorso FFT/bitmetrics su macOS, evitando uso tardivo di risorse thread-local durante la chiusura.

## Test e diagnostica

- Esteso `test_tx_pipeline` con una verifica TX a due frequenze simultanee per FT8, FT4 e FT2.
- La nuova prova controlla che il mix multi-stream contenga entrambi i payload e resti peak-normalizzato senza clipping.
- Confermata la pipeline TX/RX esistente per FT8, FT4 e FT2.

## Verifiche locali

- Build `decodium_qml`: OK.
- Test `test_tx_pipeline`: PASS, inclusa prova two-frequency TX `f0=1000/1800 Hz`.
- Test `test_ft2_qso_sim`: PASS.
- `git diff --check`: OK.
