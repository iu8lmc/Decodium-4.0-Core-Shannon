# Decodium 4 v1.0.649

## English (UK)

An on-air correction on top of v1.0.648: the automatic sequencer no longer closes a QSO on a message meant for somebody else.

### A hidden `<...>` destination is never yours

- Reported on 22 September 2026 on FT8 while working **8Z96ND**, a non-standard callsign: `<...> 8Z96ND RR73` arrived and Decodium closed the QSO, but 8Z96ND was answering a third station. The same shape had been reported on 18 September with KG6DX.
- The safeguard added in v1.0.641 was not enough. It allowed the hidden destination to be yours when the callsign written in full is non-standard — but when your correspondent *is* non-standard, **every** message they send has that shape, to you and to everybody else. Inferring the destination from the QSO in progress, with any amount of caution, gets it wrong sooner or later.
- The certainty was already inside the decoder. In non-standard-callsign messages the destination travels as a **12-bit hash**, and the message unpacker compares it with the hash of **your** callsign: when it matches, the text comes out with your callsign spelled in, `<IT9MRM> 8Z96ND RR73`. FT8, FT4 and FT2 all go through that same unpacker with your callsign in its context.
- So if the line still reads `<...>`, the hash was not yours and the message is not for you. The seventy-seven lines that used to guess the destination from the QSO in progress are gone. What remains is the case where the destination is spelled out, which is compared and needs no guessing — the exchanges with special callsigns that motivated v1.0.641 keep working.
- Proved on the production encoder and decoder with the callsigns from the report: the **same bits** decoded with `IT9MRM` in the context give `<IT9MRM> 8Z96ND RR73`, and with any other callsign stay `<...> 8Z96ND RR73`.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Una correzione da esercizio in aria sopra la 1.0.648: la sequenza automatica non chiude più un QSO su un messaggio destinato a un altro.

### Un destinatario nascosto dietro `<...>` non è mai il vostro

- Segnalato il 22 settembre 2026 su FT8 mentre si lavorava **8Z96ND**, nominativo non standard: è arrivato `<...> 8Z96ND RR73` e Decodium ha chiuso il QSO, ma 8Z96ND stava rispondendo a una terza stazione. La stessa forma era già stata segnalata il 18 settembre con KG6DX.
- La cautela aggiunta nella 1.0.641 non bastava. Lasciava che il destinatario nascosto potesse essere il vostro quando il nominativo scritto per esteso è non standard — ma se il corrispondente *è* non standard, **tutti** i suoi messaggi hanno quella forma, per voi come per chiunque altro. Dedurre il destinatario dal QSO in corso, con qualunque prudenza, sbaglia prima o poi.
- La certezza era già dentro il decodificatore. Nei messaggi con nominativo non standard il destinatario viaggia come **hash a 12 bit**, e chi spacchetta il messaggio lo confronta con l'hash del **vostro** nominativo: se coincide, il testo esce con il vostro nominativo scritto, `<IT9MRM> 8Z96ND RR73`. FT8, FT4 e FT2 passano tutti e tre da quello stesso spacchettamento con il vostro nominativo nel contesto.
- Quindi se la riga si legge ancora `<...>`, l'hash non era il vostro e il messaggio non è per voi. Le settantasette righe che indovinavano il destinatario dal QSO in corso sono state tolte. Resta il caso con il destinatario scritto per esteso, che si confronta e non ha bisogno di indovinare: gli scambi con i nominativi speciali che avevano motivato la 1.0.641 continuano a funzionare.
- Provato sull'encoder e sul decodificatore di produzione con i nominativi della segnalazione: gli **stessi bit** decodificati con `IT9MRM` nel contesto danno `<IT9MRM> 8Z96ND RR73`, con qualunque altro nominativo restano `<...> 8Z96ND RR73`.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
