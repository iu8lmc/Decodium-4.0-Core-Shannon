# Decodium 4 FT2 v1.0.562

This release continues directly from v1.0.561. It extends the shared CAT
server so that a remote client can read the transmit meters while Decodium
holds the serial port.

## English (British)

### Power, SWR and ALC over the shared CAT server

When Decodium holds the radio, no other program can open the same serial
port. An operator working from a distance could key the transmitter without
seeing how much power was actually being delivered, or whether the antenna
was answering. Decodium already measures all three quantities; what was
missing was a way to ask for them from outside.

The shared CAT server now answers the `l` / `\get_level` command with three
levels, using the Hamlib names so that any client written for `rigctl` can
read them without knowing that Decodium is on the other end:

- `SWR` — the standing wave ratio as reported by the radio.
- `ALC` — the automatic level control reading, normalised to the 0..1 range
  that Hamlib clients expect.
- `RFPOWER_METER_WATTS` — the forward power in watts.

While the transmitter is idle these meters measure nothing. Answering zero
would state that no power is being delivered and that the standing wave ratio
is perfect, which reads like a station in excellent order; the server instead
answers `RPRT -11`, the Hamlib code for a value that is not available. The
same answer is given when the radio cannot supply a reading at all, so a
missing measurement is never presented as a good one.

The command is read-only and is refused when the shared server is not
attached to a connected radio. No transmit path, decoder or user interface
behaviour is changed.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it. The five accessors used by the new command were confirmed present on
`DecodiumTransceiverManager`.

## Italiano

### Potenza, ROS e ALC sul server CAT condiviso

Quando la radio la tiene Decodium, nessun altro programma può aprire la stessa
porta seriale. Chi opera da lontano poteva mandare in trasmissione senza vedere
né quanta potenza stesse realmente erogando né se l'antenna rispondesse.
Decodium queste tre grandezze le misura già: mancava soltanto il modo di
chiederle dall'esterno.

Il server CAT condiviso risponde ora al comando `l` / `\get_level` con tre
livelli, usando i nomi Hamlib così che qualunque client scritto per `rigctl`
possa leggerli senza sapere che dall'altra parte c'è Decodium:

- `SWR` — il rapporto di onde stazionarie riportato dalla radio.
- `ALC` — la lettura del controllo automatico di livello, normalizzata
  nell'intervallo 0..1 che i client Hamlib si aspettano.
- `RFPOWER_METER_WATTS` — la potenza diretta in watt.

A trasmettitore fermo questi misuratori non misurano nulla. Rispondere zero
direbbe che non si eroga potenza e che il ROS è perfetto, il che somiglia a una
stazione in ottimo stato; il server risponde invece `RPRT -11`, il codice
Hamlib per un valore non disponibile. La stessa risposta viene data quando la
radio non è in grado di fornire la lettura, così un dato mancante non viene mai
presentato come un dato buono.

Il comando è di sola lettura e viene rifiutato quando il server condiviso non è
collegato a una radio connessa. Non cambia nulla nel percorso di trasmissione,
nel decoder o nel comportamento dell'interfaccia.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa. È stata verificata la presenza sui
`DecodiumTransceiverManager` dei cinque accessori usati dal nuovo comando.
