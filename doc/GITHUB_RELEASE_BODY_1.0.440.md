# Decodium 4 FT2 1.0.440

Release di manutenzione rispetto alla 1.0.439, con focus su stabilita' audio macOS, robustezza CAT seriale/Icom, registrazioni RX condivise e risoluzione hash FT8.

## Correzioni principali

- Aggiunto un percorso nativo macOS AudioQueue per l'input audio, con gestione dedicata di device, buffer, callback e cleanup, per rendere piu' stabile la cattura su dispositivi CoreAudio/USB.
- Aggiunta una protezione contro avvii audio duplicati quando uno start precedente e' ancora in corso.
- Aggiunti i flag ambiente `DECODIUM_DISABLE_CAT` e `DECODIUM_RX_RECORD_DISABLE_CAT`, utili per registrazioni RX e test in cui la seriale CAT deve restare libera per un altro programma.
- Reso non bloccante il reconnect CAT transitorio, con seriale di guardia e arresto thread controllato.
- Ridotta l'aggressivita' del polling passivo Hamlib sugli Icom seriali: le letture passive di VFO, split, frequenza, modo e PTT sono disattivate di default nei casi in cui possono generare timeout, mantenendo disponibili i comandi CAT espliciti.
- Il poll interval CAT ora rispetta meglio il valore configurato anche con telemetria PWR/SWR attiva.
- Ampliata la raccolta seed hash FT8: cache calls piu' grande, budget di caricamento piu' ampio e priorita' ai log recenti Decodium, JTDX e WSJT-X.
- Aggiunta lettura opzionale di log esterni per seed hash tramite `DECODIUM_HASH_SEED_PATHS` e `DECODIUM_EXTERNAL_LOG_DIRS`.
- Aggiunta risoluzione dei messaggi FT8 con placeholder `<...>` usando righe recenti di `ALL.TXT` compatibili per orario, frequenza e messaggio.

## Verifiche locali

- Build `decodium_qml`: OK.
- Test `test_tx_pipeline`: PASS.
- Test `test_ft2_qso_sim`: PASS.
- `git diff --check`: OK.
