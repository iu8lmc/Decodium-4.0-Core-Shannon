# Decodium 4 v1.0.662

## English (UK)

This release stops FT2 from logging QSOs with stations that never answered.

### FT2: QSOs logged without the other station's reply

- With the radio hearing its own transmission, the decoder reads our own messages at +20 to +28 dB (for example `IW8AOF IQ8DO -06`). Multi-Answer Mode accepted any decode that contained our call in any position, so it took that echo for the other station's reply: the report in the echo became theirs, its signal-to-noise ratio became the report sent back, and the QSO advanced to RR73 and was logged after the station had sent only its grid.
- In a measured period, 34 of 168 FT2 QSOs (20%) had no confirmation at all from the other station; none of them relied on a low-confidence (predictive) decode. FT8 was not affected: its decoder does not listen while transmitting.
- A message whose second element is our call and whose first is not is now recognised as ours and ignored. Messages addressed to us, including non-standard calls and portable suffixes, still pass.
- The same check now applies to the single-QSO auto-sequencer in FT2, FT4 and FT8. There the only guard was an exact match with the last transmitted message, which a late low-confidence re-decode of an earlier message does not satisfy.
- QSOs that the other station confirms with a clean R-report are still logged without waiting for their 73, as since 1.0.656.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Questa versione impedisce a FT2 di registrare QSO con stazioni che non hanno mai risposto.

### FT2: QSO registrati senza la risposta dell'altra stazione

- Quando la radio risente la propria trasmissione, il decoder legge i nostri messaggi a +20/+28 dB (per esempio `IW8AOF IQ8DO -06`). Il Multi-Answer Mode accettava qualunque decodifica contenente il nostro nominativo in una posizione qualsiasi, e scambiava quell'eco per la risposta dell'altra stazione: il rapporto dell'eco diventava il suo, il suo rapporto segnale/rumore diventava il rapporto rimandato, e il QSO avanzava fino all'RR73 e veniva registrato dopo che la stazione aveva mandato solo il locatore.
- In un periodo misurato, 34 QSO FT2 su 168 (20%) non avevano alcuna conferma dell'altra stazione; nessuno si fondava su una decodifica a bassa confidenza (predittiva). L'FT8 non era interessato: il suo decoder non ascolta mentre trasmette.
- Un messaggio il cui secondo elemento è il nostro nominativo e il primo no viene ora riconosciuto come nostro e ignorato. I messaggi diretti a noi, compresi i nominativi non standard e i suffissi portatili, passano ancora.
- Lo stesso controllo vale ora anche per l'auto-sequencer del singolo QSO in FT2, FT4 e FT8. Lì l'unica protezione era il confronto esatto con l'ultimo messaggio trasmesso, che una rilettura tardiva a bassa confidenza di un messaggio precedente non soddisfa.
- I QSO che l'altra stazione conferma con un R-rapporto pulito vengono ancora registrati senza aspettare il suo 73, come dalla 1.0.656.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
