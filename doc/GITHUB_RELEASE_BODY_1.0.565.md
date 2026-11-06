# Decodium 4 FT2 v1.0.565

This release continues directly from v1.0.564. It fixes the actual wire
format of the shared CAT server's meter readings, found while checking
compatibility with the Decodium mobile companion app.

## English (British)

### Extended-response format for `\get_level`

Hamlib's `netrigctl` protocol, used by real rigctl clients including the
Decodium mobile app, prefixes a command with `+` to ask for the "extended"
response: instead of a bare number, the server is expected to echo which
level was asked for and then label the value, so a client polling several
levels in a row can tell which reply belongs to which question.

The shared CAT server always answered with the bare number, regardless of the
`+` prefix. A client that actually relies on the extended form — as the
mobile app does for `SWR`, `ALC` and `RFPOWER_METER_WATTS` — received a
number it could not attribute to any level and silently discarded it, so its
meters never lit up even though the server was answering correctly by its own
older convention.

`\get_level` now replies in the form the client expects when it asked with
`+`:

```
get_level: SWR
Level Value: 2.500000
RPRT 0
```

and, when the meter is not available, echoes the level before the error code
instead of leaving the client guessing which reading failed:

```
get_level: SWR
RPRT -11
```

Every other command still answers in the plain form, since none of the
clients this server talks to ever request the extended form for them.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it. The exact two-line shape was checked against the Decodium mobile
app's own parser (`AppBridge.cpp`), which is the authoritative reference for
this protocol on the client side.

## Italiano

### Formato di risposta esteso per `\get_level`

Il protocollo `netrigctl` di Hamlib, usato dai veri client rigctl compresa
l'app mobile di Decodium, antepone un `+` al comando quando vuole la risposta
«estesa»: invece del solo numero, il server deve ripetere quale livello è
stato chiesto e poi etichettare il valore, così un client che interroga più
livelli in sequenza sa a quale domanda appartiene ogni risposta.

Il server CAT condiviso rispondeva sempre col numero nudo, a prescindere dal
prefisso `+`. Un client che si basa davvero sulla forma estesa — come fa
l'app mobile per `SWR`, `ALC` e `RFPOWER_METER_WATTS` — riceveva un numero che
non poteva attribuire a nessun livello e lo scartava in silenzio, per cui i
suoi misuratori non si accendevano mai, pur essendo il server corretto secondo
la sua vecchia convenzione.

`\get_level` risponde ora nella forma che il client si aspetta quando chiede
con `+`:

```
get_level: SWR
Level Value: 2.500000
RPRT 0
```

e, quando il misuratore non è disponibile, ripete il livello prima del codice
di errore invece di lasciare al client il dubbio su quale lettura sia fallita:

```
get_level: SWR
RPRT -11
```

Tutti gli altri comandi continuano a rispondere nella forma semplice, perché
nessuno dei client con cui parla questo server chiede mai la forma estesa per
loro.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa. La forma esatta a due righe è stata
verificata contro il parser stesso dell'app mobile di Decodium
(`AppBridge.cpp`), che è il riferimento autorevole per questo protocollo lato
client.
