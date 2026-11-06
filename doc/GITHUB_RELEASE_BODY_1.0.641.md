# Decodium 4 v1.0.641

## English (UK)

A single, precise correction on top of v1.0.640: the automatic sequencer no longer answers a message whose destination is hidden behind an unresolved hash unless that destination can really be you.

### The sequencer no longer replies on a guess

- Reported on air on 18 September 2026 on FT4: while calling KG6DX, the station received `<...> KG6DX R-07`. KG6DX was answering a third station whose compound callsign travels as a hash, but Decodium started the reply sequence as if the message had been addressed to it.
- When the destination arrives as an unresolved hash, Decodium used to infer it from the QSO in progress. The existing safeguards — the sender must be the active partner, an exchange must be under way, the payload must be a plausible report — all matched, and four agreeing hints are still not a certainty.
- The rule is now taken from the protocol, not from the context. In a message carrying an unresolved hash, the callsign written in full decides: if it is **non-standard**, the hashed one is a standard call and may be yours; if it is **standard**, the hashed one is necessarily a compound or special call, so it is yours only if your own callsign is compound. Between two standard callsigns the protocol never uses a hash.
- The message is still shown in the list. What changes is that the sequencer no longer answers it.
- Contacts with non-standard callsigns are unaffected: when a station such as II8IHBC calls you, your standard callsign is the hashed one, and that case is explicitly covered by the tests.

### How it is verified

- The rule lives beside `isStandardFtxCall` as `decodium::txmsg::hiddenHashCanBeOwnCall`, so the regression test calls the real function rather than a copy of it.
- The test also checks, on the source itself, that the bridge consults the rule inside the unresolved-hash branch and **before** the context-based safeguards that had given the green light on their own. Removing the guard makes the test fail; putting it back turns it green again (17 checks).
- The production encoder is used to confirm the shape of the reported message: a compound callsign really does travel as a hash inside a standard message carrying a report, and an unknown hash is displayed exactly as `<...>`.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Una sola correzione, precisa, sopra la 1.0.640: la sequenza automatica non risponde più a un messaggio il cui destinatario è nascosto dietro un hash non risolto, a meno che quel destinatario possa davvero essere il vostro.

### Il sequencer non risponde più a intuito

- Segnalato in aria il 18 settembre 2026 su FT4: mentre chiamava KG6DX, la stazione ha ricevuto `<...> KG6DX R-07`. KG6DX stava rispondendo a una terza stazione, il cui nominativo composto viaggia come hash, ma Decodium ha avviato la sequenza di risposta come se il messaggio fosse per lui.
- Quando il destinatario arriva come hash non risolto, Decodium lo deduceva dal QSO in corso. Le cautele che c'erano già — il mittente dev'essere il partner attivo, dev'esserci uno scambio in corso, il payload dev'essere un rapporto plausibile — combaciavano tutte, e quattro indizi concordi non fanno una certezza.
- Ora la regola viene dal protocollo, non dal contesto. In un messaggio con hash non risolto decide il nominativo scritto per esteso: se è **non standard**, l'hash è un nominativo standard e può essere il vostro; se è **standard**, l'hash è per forza un nominativo composto o speciale, quindi è vostro solo se il vostro è composto. Fra due nominativi standard il protocollo non usa l'hash.
- Il messaggio resta visibile nella lista. Quello che cambia è che il sequencer non gli risponde più.
- I contatti con nominativi non standard non cambiano: quando vi chiama una stazione come II8IHBC, l'hash è il vostro nominativo standard, e quel caso è coperto dai test in modo esplicito.

### Come è stato verificato

- La regola sta accanto a `isStandardFtxCall` come `decodium::txmsg::hiddenHashCanBeOwnCall`, così il test di regressione chiama la funzione vera e non una sua copia.
- Il test verifica anche, sul sorgente, che il bridge la consulti dentro il ramo dell'hash non risolto e **prima** delle cautele basate sul contesto, che da sole avevano dato il via libera. Togliendo la guardia il test fallisce; rimettendola torna verde (17 controlli).
- Per confermare la forma del messaggio segnalato si usa l'encoder di produzione: un nominativo composto viaggia davvero come hash dentro un messaggio standard con rapporto, e un hash sconosciuto si presenta esattamente come `<...>`.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
