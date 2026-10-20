# Decodium 4 v1.0.654

## English (UK)

FT2: during a QSO the decoder now looks for your partner's reply where and when it is expected.

### FT2: the expected reply

- In an FT2 QSO you know when the reply will come: 0.2 to 1.0 seconds after the end of your own transmission, on your partner's frequency. In that time window and at that frequency the asynchronous decoder now always tries a candidate, even when the synchronisation search has not proposed one, with the AP information of your callsign and your partner's. The final check remains the full error-correcting code.
- It only works when a DX call is set and in the 6 seconds after one of your transmissions; the rest of the band is decoded exactly as before.
- On the test bench: replies decoded 0.6 dB lower (50% threshold from -16.0 to -16.6 dB) and **no false decodes** in an hour of waits with no reply. Confirmation on air is in progress.
- It is now **on by default**; `DECODIUM_FT2_ASYNC_ATTESO=0` switches it off.
- The diagnostic log shows `[FT2-ATTESO]` lines: `forzato=1` marks a reply that the normal search had not found, that is, one that would otherwise have been missed.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

FT2: durante un QSO il decodificatore cerca la risposta del corrispondente dove e quando deve arrivare.

### FT2: la risposta attesa

- In un QSO FT2 si sa quando arriverà la risposta: da 0,2 a 1,0 secondi dopo la fine della propria trasmissione, sulla frequenza del corrispondente. In quella finestra di tempo e a quella frequenza il decodificatore asincrono ora prova sempre un candidato, anche quando la ricerca del sincronismo non l'ha proposto, con le informazioni AP del proprio nominativo e di quello del corrispondente. Il controllo finale resta il codice correttore completo.
- Funziona solo con un nominativo DX impostato e nei 6 secondi dopo una propria trasmissione; il resto della banda si decodifica esattamente come prima.
- Al banco di prova: risposte decodificate 0,6 dB più in basso (soglia del 50% da -16,0 a -16,6 dB) e **nessuna decodifica falsa** in un'ora di attese senza risposta. La conferma in aria è in corso.
- Ora è **acceso di default**; `DECODIUM_FT2_ASYNC_ATTESO=0` lo spegne.
- Il log diagnostico mostra le righe `[FT2-ATTESO]`: `forzato=1` indica una risposta che la ricerca normale non aveva trovato, cioè che altrimenti sarebbe andata persa.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
