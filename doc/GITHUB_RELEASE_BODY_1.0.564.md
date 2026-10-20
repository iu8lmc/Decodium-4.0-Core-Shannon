# Decodium 4 FT2 v1.0.564

This release continues directly from v1.0.563. It completes the shared CAT
server's power, SWR and ALC meters introduced in v1.0.562 by advertising them
to connecting clients.

## English (British)

### Meter capability advertised on the shared CAT server

Hamlib clients ask the server what it can do before asking it for anything
specific: the `\dump_state` handshake reports which levels are available for
reading. That capability line still said "none" after v1.0.562 added `SWR`,
`ALC` and `RFPOWER_METER_WATTS` to `\get_level`, so a well-behaved client
never sent the question in the first place, and any meter built on top of it
stayed empty.

The server now reports `SWR`, `ALC`, `STRENGTH`, `RFPOWER_METER` and
`RFPOWER_METER_WATTS` as available for reading, matching what `\get_level`
already answers. They remain read-only: setting the power of someone else's
radio through this channel was never the intent.

No transmit path, decoder or user interface behaviour is changed.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it. The capability bitmask was checked bit by bit against Hamlib's own
`RIG_LEVEL_*` definitions.

## Italiano

### Capacità dei misuratori dichiarata sul server CAT condiviso

I client Hamlib chiedono al server cosa sa fare prima di chiedergli qualcosa
di specifico: la fase di collegamento `\dump_state` riporta quali livelli sono
disponibili in lettura. Quella riga continuava a dire "nessuno" anche dopo che
la 1.0.562 aveva aggiunto `SWR`, `ALC` e `RFPOWER_METER_WATTS` a `\get_level`,
per cui un client ben scritto non mandava affatto la richiesta, e qualunque
misuratore costruito sopra restava vuoto.

Il server ora dichiara disponibili in lettura `SWR`, `ALC`, `STRENGTH`,
`RFPOWER_METER` e `RFPOWER_METER_WATTS`, in linea con quanto `\get_level` già
risponde. Restano di sola lettura: impostare la potenza della radio di
qualcun altro attraverso questo canale non è mai stato l'obiettivo.

Non cambia nulla nel percorso di trasmissione, nel decoder o nel
comportamento dell'interfaccia.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa. La bitmask delle capacità è stata verificata
bit per bit contro le definizioni `RIG_LEVEL_*` di Hamlib.
