# Decodium 4 v1.0.626

## English (UK)

v1.0.626 fixes a decoding defect that made stations with a non-standard callsign disappear whenever they called you. It affects **FT8, FT4 and FT2**.

### What was happening

A callsign such as `II8IHBC` (a special-event prefix with a four-letter suffix, seven characters in all) does not fit the standard 28-bit encoding. The protocol therefore sends it as a type 4 message: your correspondent's callsign as a hash in angle brackets, and the non-standard callsign in full, with nothing after it. `<IU8LMC> II8IHBC` is a **complete** message and means "II8IHBC is calling IU8LMC".

The semantic decode filter treated it as truncated and discarded it, in two independent places:

- messages with only two tokens were rejected as *directed message without payload*;
- messages whose payload was empty were rejected as *missing directed payload*.

The consequence was the worst possible one: the station's **CQ calls were displayed**, but its **replies to you were not**. On the station where this was found, 75 calls were discarded in a single evening without the operator ever seeing them. The first check was the more damaging of the two, because it rejected precisely the **high-confidence** decodes: those carry no trailing `?` and therefore consist of two tokens only.

### What changed

Both checks now recognise the canonical type 4 form. A distinction that did not previously exist has been introduced between a genuine hashed callsign, `<IU8LMC>`, and the `<...>` placeholder that stands for a hash which cannot yet be resolved.

This applies to every non-standard callsign: special-event stations such as `II*` and `IQ*`, portable operations such as `PJ4/K1ABC`, and commemorative prefixes.

### Also in this release

Everything introduced in v1.0.625: the four experimental FT8 sensitivity features enabled by default (iterative BICM-ID demodulation, coherent demodulation pass, a priori passes from previously heard stations, energy-domain accumulation in the CQ history). Each remains individually switchable via its environment variable.

---

## Italiano

La 1.0.626 corregge un difetto di decodifica che faceva sparire le stazioni con nominativo non standard ogni volta che ti chiamavano. Riguarda **FT8, FT4 e FT2**.

### Cosa succedeva

Un nominativo come `II8IHBC` (prefisso per evento speciale con suffisso di quattro lettere, sette caratteri in tutto) non entra nella codifica standard a 28 bit. Il protocollo lo manda quindi come messaggio di tipo 4: il nominativo del corrispondente come hash fra parentesi angolari e quello non standard per esteso, senza nulla dopo. `<IU8LMC> II8IHBC` è un messaggio **completo** e significa "II8IHBC sta chiamando IU8LMC".

Il filtro semantico delle decodifiche lo considerava monco e lo scartava, in due punti indipendenti:

- i messaggi con due soli elementi venivano rifiutati come *directed message without payload*;
- quelli con payload vuoto venivano rifiutati come *missing directed payload*.

La conseguenza era la peggiore possibile: i **CQ** della stazione **si vedevano**, le sue **risposte no**. Sulla stazione dove il problema è emerso, 75 chiamate sono state scartate in una sola serata senza che l'operatore ne sapesse nulla. Il primo controllo era il più dannoso dei due, perché rifiutava proprio le decodifiche **ad alta confidenza**: quelle non portano il `?` finale e constano quindi di due soli elementi.

### Cosa è cambiato

Entrambi i controlli riconoscono ora la forma canonica del tipo 4. È stata introdotta una distinzione che prima non esisteva fra un nominativo realmente hashato, `<IU8LMC>`, e il segnaposto `<...>` che indica un hash non ancora risolvibile.

Vale per tutti i nominativi non standard: stazioni per eventi speciali come `II*` e `IQ*`, operazioni portatili come `PJ4/K1ABC`, prefissi commemorativi.

### Contiene anche

Tutto quello introdotto nella 1.0.625: le quattro funzioni sperimentali di sensibilità FT8 accese di default (demodulazione iterativa BICM-ID, passata di demodulazione coerente, passate a priori dalle stazioni già sentite, accumulo a livello di energia nello storico dei CQ). Ognuna resta spegnibile singolarmente con la propria variabile d'ambiente.
