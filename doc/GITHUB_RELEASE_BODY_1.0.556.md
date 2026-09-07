# Decodium 4 FT2 v1.0.556

**Install this one.** On Windows, 1.0.554 and 1.0.555 could leave the
transmitter keyed after the first transmission of a session.

## English (British)

### The radio stayed on the air

Reported by an operator: from the second transmission onwards the rig does not
unkey, and Halt has to be pressed to stop it. The operator's diagnostic log
settled it — thirteen lines reading `PTT OFF deduplicated` against a single
release actually executed.

Unkeying goes through a guard that discards duplicate commands using a flag.
The flag is meant to cover **one** transmission and to be cleared at the next,
but the only place that clears it is compiled for macOS alone. On Windows it is
never reached: after the first release the flag stays raised, every later PTT
OFF is thrown away as a duplicate, and nothing commands the rig to stop
transmitting.

Two corrections rather than one. The flag is now cleared at the start of every
transmission, on every platform. And a duplicate is discarded only while the
rig no longer reports transmitting: an extra unkey command costs nothing, a
radio left on the air costs a great deal, and between the two possible mistakes
the harmless one is the one to make.

The defect arrived with the PTT confirmation work in 1.0.554 and affects any
Windows installation using the legacy audio path. Reported upstream.

### Everything else

Unchanged from 1.0.555: the noise threshold back to its 1.0.495 calibration
with a slider to set it, the corrected scale for the 3D spectrum ridges, and
elisir80's work up to 1.0.554.

---

## Italiano

### La radio restava in aria

Segnalato da un operatore: dalla seconda trasmissione in poi la radio non si
sgancia, e bisogna premere Halt per fermarla. Il log diagnostico dell'operatore
ha chiuso la questione: tredici righe `PTT OFF deduplicated` contro un solo
sgancio davvero eseguito.

Lo sgancio passa da una guardia che scarta i comandi doppi con una bandierina.
La bandierina vale per **una** trasmissione e va abbassata alla successiva, ma
l'unico punto che la abbassa e' compilato soltanto per macOS. Su Windows non ci
si arriva mai: dopo il primo sgancio resta alzata, ogni PTT OFF successivo
viene buttato via come doppione, e nessuno comanda piu' alla radio di smettere
di trasmettere.

Due correzioni, non una. La bandierina si abbassa all'inizio di ogni
trasmissione, su tutte le piattaforme. E il doppione si scarta soltanto finche'
la radio non riporta piu' di trasmettere: un comando di sgancio in piu' non
costa nulla, una radio che resta in aria costa moltissimo, e fra i due sbagli
possibili va scelto sempre quello innocuo.

Il difetto e' arrivato con la conferma del PTT della 1.0.554 e riguarda ogni
installazione Windows che usa il percorso audio legacy. Segnalato a monte.

### Il resto

Invariato rispetto alla 1.0.555: la soglia di rumore tornata alla taratura
della 1.0.495 con il cursore per regolarla, la scala corretta con cui il 3D
misura le creste, e il lavoro di elisir80 fino alla sua 1.0.554.
