# Decodium 4 FT2 1.0.438

Release di manutenzione e stabilita' rispetto alla 1.0.437, con focus su sequencer FT2/FT4/FT8, watchdog TX e robustezza audio.

## Correzioni principali

- Ripristinata la priorita' del TX watchdog: quando il watchdog e' configurato, il limite `Caller retries` non interrompe piu' la sessione prima del timeout impostato.
- Gestione piu' robusta degli errori audio TX su Windows/legacy: se la periferica audio USB viene invalidata durante una trasmissione, Decodium chiude il TX in modo pulito e riattiva il flusso senza restare appeso ai timer di playback.
- Migliorata la chiamata manuale FT2: risposta a un CQ piu' rapida nello slot utile e protezione contro retry stale che potevano ritrasmettere locator o messaggi non piu' coerenti dopo l'avanzamento di stato.
- Aggiunta una finestra di grace dopo TX1 in FT2 per attendere la risposta del corrispondente prima di ripetere il locator.
- Rafforzati i guard seriali sui retry FT2 asincroni e sui controlli post-TX, cosi' i timer vecchi non possono riattivare uno step TX superato.
- Migliorata la raccolta asincrona dei seed hash FT4/FT8: accumulo bounded, refresh piu' controllato e abort pulito durante la chiusura dell'app.

## Strumenti e diagnostica

- Integrati i nuovi parser FT2 `_ft2_level0.py` e `_ft2_level0bis.py` provenienti dal main upstream.
- Log piu' espliciti quando un retry viene ignorato per priorita' watchdog o quando un retry FT2 viene scartato perche' stale.

## Verifiche locali

- Build `decodium_qml` completata.
- Test `test_ft2_qso_sim`: PASS 3/3.
- Test `test_tx_pipeline`: PASS 18/18.
- `git diff --check`: OK.
