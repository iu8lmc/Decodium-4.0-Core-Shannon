# FT2 Stabilita TX/Audio/NTP (branch merge/raptor-v110-port)

Data: 2026-02-26

## Obiettivo
Stabilizzare la trasmissione FT2 su macOS (PTT CAT + audio USB) eliminando i casi intermittenti di "PTT attivo ma modulazione assente".

## Correzioni stabili applicate
- Prompt microfono macOS anticipato all'avvio.
- Preflight input audio su startup per forzare subito la richiesta permessi, evitando popup tardivi durante TX.
- Scheduler TX/RX su tempo di sistema raw.
- Offset NTP rimosso dalla logica di slot timing (NTP resta diagnostico/monitoring).
- Handshake PTT reso piu robusto.
- Avvio TX su ack PTT e fallback automatico se ack non arriva entro 200ms.
- Cleanup difensivo dei timer/stati PTT su stop TX.
- Stop modulator sincrono nel teardown TX.
- Riduce race tra stop/start ravvicinati nelle sequenze FT2.
- Guardia parametri FT2 nel Modulator.
- Forzati parametri FT2 coerenti nel path modulator (symbols/frames/toneSpacing).
- Migliorata resilienza ai casi di start con stato modulator non allineato.
- Titolo/app e bootstrap coerenti con `ft2.app`.
- Risolti avvii con riferimenti errati a path legacy `wsjtx.app`.

## Logging di supporto TX
Percorso log:
- `~/Library/Application Support/ft2/tx-support.log`

Modalita default (compatta):
- Eventi essenziali: richiesta PTT, ack PTT, fallback PTT, eventi di recovery.
- Ridotta verbosita su eventi ad alta frequenza.

Modalita verbosa (solo debug):
- Avvio con variabile ambiente `FT2_TX_VERBOSE_LOG=1`.
- Abilita log dettagliati su stati audio/modulator e teardown TX.

Esempio avvio da terminale:
- `FT2_TX_VERBOSE_LOG=1 open -n /Users/salvo/Desktop/ft2/decodium3-raptor-merge/build-raptor-port/ft2.app`

## Verifica consigliata
- Eseguire almeno 10-15 cicli TX consecutivi (CQ + risposte).
- Confermare assenza di casi "prima TX ok, successive mute".
- In caso di regressione, allegare blocco log da `ptt request` a `stopTx2`.
