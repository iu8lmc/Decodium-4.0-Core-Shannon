# Decodium 4 v1.0.628

## English (UK)

v1.0.628 completes the fix for **non-standard callsigns**: they are no longer merely visible when they call you — Decodium now answers them.

### What was still missing

v1.0.626 made the message visible again: a station such as `II8IHBC`, whose callsign does not fit the standard 28-bit encoding, sends `<IU8LMC> II8IHBC` — your callsign as a hash, its own in full, nothing after — and that line was being discarded as truncated.

Making it visible was not enough. The routine that tells the sequencer "this call is for you" rejected the same message for two independent reasons: it required at least three elements, where this form has two, and it looked for the angle brackets among tokens that had **already been normalised**, where the brackets have been stripped and a hash is indistinguishable from an ordinary callsign. The result was that the operator saw the station calling and the contact never completed.

This affects **FT8, FT4 and FT2**: the routine is shared by all modes.

### What changed

The routine now has two branches. When the hash has already been resolved by the decoder — `<IU8LMC>` — the recipient is explicit, so it is compared directly against your own callsign: a stricter check than the heuristic used until now, which had to infer the recipient from the QSO in progress. When the hash is the unresolved placeholder `<...>`, the previous path applies unchanged.

A message of this form addressed to **someone else** stops the sequencer immediately, as it should.

### Verification

A new automated test performs the whole path with production components: the message is encoded, transmitted as a real FT2 frame with calibrated noise, decoded by the production Fortran decoder, and the recovered text is passed to the same recognition rules the bridge uses. The rules themselves are covered by unit tests including cases that fail against both earlier, incorrect versions of the fix.

### Also in this release

Everything from v1.0.627: FT8 sensitivity features governed adaptively by the decoding time budget, enabled only where the machine has room to spare.

---

## Italiano

La 1.0.628 completa la correzione per i **nominativi non standard**: non si limitano più a essere visibili quando ti chiamano — ora Decodium risponde.

### Cosa mancava ancora

La 1.0.626 aveva restituito la visibilità al messaggio: una stazione come `II8IHBC`, il cui nominativo non entra nella codifica standard a 28 bit, trasmette `<IU8LMC> II8IHBC` — il tuo nominativo come hash, il proprio per esteso, nulla dopo — e quella riga veniva scartata come monca.

Renderla visibile non bastava. La routine che dice al sequencer «questa chiamata è per te» rifiutava lo stesso messaggio per due ragioni indipendenti: pretendeva almeno tre elementi, mentre questa forma ne ha due, e cercava le parentesi angolari fra token **già normalizzati**, dove le parentesi sono state rimosse e un hash è indistinguibile da un nominativo qualsiasi. Il risultato era che l'operatore vedeva la stazione chiamarlo e il contatto non si chiudeva mai.

Riguarda **FT8, FT4 e FT2**: la routine è condivisa fra tutti i modi.

### Cosa è cambiato

La routine ha ora due rami. Quando l'hash è già stato risolto dal decoder — `<IU8LMC>` — il destinatario è esplicito e viene confrontato direttamente con il proprio nominativo: un controllo più stringente dell'euristica usata finora, che doveva dedurre il destinatario dal QSO in corso. Quando l'hash è il segnaposto non risolto `<...>`, resta la strada precedente, invariata.

Un messaggio di questa forma diretto a **qualcun altro** ferma subito il sequencer, come deve essere.

### Verifica

Un test automatico nuovo percorre l'intera catena con i componenti di produzione: il messaggio viene codificato, trasmesso come trama FT2 reale con rumore calibrato, decodificato dal decoder Fortran di produzione, e il testo recuperato viene passato alle stesse regole di riconoscimento che usa il bridge. Le regole sono a loro volta coperte da test unitari, compresi casi che falliscono contro entrambe le versioni sbagliate della correzione.

### Contiene anche

Tutto quanto introdotto nella 1.0.627: le funzioni di sensibilità per FT8 governate in modo adattivo dal budget di tempo di decodifica, attive solo dove la macchina ha margine.
