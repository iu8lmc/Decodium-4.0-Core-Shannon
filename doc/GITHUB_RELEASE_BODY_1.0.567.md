# Decodium 4 FT2 v1.0.567

This release continues directly from v1.0.566. It adds the receive signal
meter to the shared CAT server, completing the set of readings a remote
client can take from the radio.

## English (British)

### S-meter over the shared CAT server

Until now the shared server exposed only the transmit meters: forward power,
SWR and ALC. Someone operating from another room could see how the station was
transmitting, but nothing at all about what it was hearing.

`\get_level STRENGTH` now answers with the received signal strength in
decibels relative to S9 — the scale Hamlib uses and the one the operator
reads: −54 is S0, 0 is S9, +20 is S9+20.

It is the one **receive** reading among the levels, so it deliberately does
not follow the rule that applies to the others: rather than being refused
while the transmitter is idle, it is only valid then.

The radio is asked for it **only if it declares the capability**. There are no
opportunistic probes here, unlike the ALC: probing for a level the rig never
advertised is what once left a transceiver stuck in transmit, and a
convenience indicator is not worth that risk. A rig that declares the S-meter
and then stops answering is dropped after a few failures, so the rest of the
CAT traffic does not pay for it.

### A meter that was never read is not a meter reading zero

Zero on this scale means S9 — a full signal. A radio without an S-meter would
therefore have looked like one receiving a strong station, which is exactly
the kind of plausible-but-false number an instrument must never show.

The transceiver layer now carries an explicit "this has been read" flag
alongside the value, and the server answers "not available" when it has not.
The same principle already governs the transmit meters.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it.

## Italiano

### S-meter sul server CAT condiviso

Fino a ora il server condiviso esponeva soltanto i misuratori di trasmissione:
potenza diretta, ROS e ALC. Chi operava dall'altra stanza vedeva come stava
trasmettendo la stazione, ma nulla di quello che stava ascoltando.

`\get_level STRENGTH` risponde ora con l'intensità del segnale ricevuto in
decibel rispetto a S9 — la scala di Hamlib, che è poi quella con cui la legge
l'operatore: −54 è S0, 0 è S9, +20 è S9+20.

È l'unico livello di **ricezione** fra quelli disponibili, e per questo non
segue la regola degli altri: invece di essere rifiutato a trasmettitore fermo,
è proprio allora che vale.

Alla radio lo si chiede **solo se dichiara di averlo**. Niente sonde
opportunistiche come per l'ALC: interrogare un livello che il rig non ha mai
annunciato è ciò che una volta ha lasciato un ricetrasmettitore bloccato in
trasmissione, e un indicatore di comodo non vale quel rischio. Un rig che
dichiara l'S-meter e poi smette di rispondere viene lasciato perdere dopo
qualche tentativo, così il resto del traffico CAT non ne paga il conto.

### Un misuratore mai letto non è un misuratore che segna zero

Zero, su questa scala, vuol dire S9: segnale pieno. Una radio senza S-meter
sarebbe quindi sembrata una radio che riceve forte — esattamente quel genere
di numero verosimile e falso che uno strumento non deve mai mostrare.

Il livello del transceiver porta ora con sé, accanto al valore, l'indicazione
esplicita di essere stato letto davvero, e il server risponde «non
disponibile» quando non lo è. È lo stesso principio che regge già i
misuratori di trasmissione.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa.
