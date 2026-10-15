# Decodium 4 FT2 v1.0.568

This release continues directly from v1.0.567. The DECØMETER on the desktop
gains the readings that had been developed for the phone companion app: the
received signal, the frequency and band, the power in dBm, and a HOLD button.

## English (British)

### A fourth screen: what the station is hearing

The instrument had three screens and all three looked outwards — power,
match, drive. Nothing said anything about what was coming in.

The new screen shows the **S-meter** (`S6`, `S9+20`), the **frequency** in MHz
and the forward power in **dBm**, with the **band** and the state of the
reading in the right-hand column, where the other screens show impedance.

It is the one screen that speaks of reception, and so it deliberately works
the opposite way round from the rest of the panel: rather than being blank
while the transmitter is idle, that is exactly when it is valid.

The signal strength comes from the CAT manager, which already declares
whether the value has genuinely been read. That matters here more than
elsewhere: zero on this scale means S9, so a radio without an S-meter would
otherwise have looked like one hearing a full-strength station.

### HOLD

The readings can be frozen where they are. It is useful on a desk instrument
to read at leisure, and more so when the meter lives on a second screen that
nobody is watching while talking.

HOLD freezes what is **shown**, not what is **measured**: the SWR keeps being
read and the alarm keeps applying. A hold that silenced the alarm would be a
way of not noticing a fault in the antenna precisely while staring at the
instrument.

### Where these come from

They were written for the Decometer companion app and are brought back here,
so that the instrument on the computer and the one in your hand show the same
things. The band is worked out from the frequency rather than asked for: it is
a two-line calculation and it keeps the panel from depending on one more
property of the bridge.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it. The band table was checked against the real FT8 frequencies: 14.074
gives 20m, 7.074 gives 40m, 3.573 gives 80m, 10.136 gives 30m, 50.313 gives
6m and 144.174 gives 2m.

## Italiano

### Una quarta schermata: cosa sta ascoltando la stazione

Lo strumento aveva tre schermate e tutte e tre guardavano in uscita: potenza,
adattamento, pilotaggio. Nulla diceva niente di quello che entrava.

La nuova schermata mostra l'**S-meter** (`S6`, `S9+20`), la **frequenza** in
MHz e la potenza diretta in **dBm**, con la **banda** e lo stato della lettura
nella colonna di destra, dove le altre schermate mostrano l'impedenza.

È l'unica schermata che parla di ricezione, e per questo funziona
deliberatamente al contrario del resto del pannello: invece di essere muta a
trasmettitore fermo, è proprio allora che vale.

L'intensità del segnale arriva dal gestore CAT, che dichiara già se il valore
è stato letto davvero. Qui conta più che altrove: zero, su questa scala, vuol
dire S9, e una radio senza S-meter sarebbe altrimenti sembrata una radio che
riceve a piena forza.

### HOLD

Le letture si possono fermare dove sono. Serve su uno strumento da tavolo per
leggere con comodo, e serve di più quando il misuratore sta su un secondo
schermo che nessuno guarda mentre parla.

HOLD ferma ciò che si **vede**, non ciò che si **misura**: il ROS continua a
essere letto e l'allarme continua a valere. Un fermo che zittisse l'allarme
sarebbe un modo per non accorgersi di un guasto all'antenna proprio mentre si
fissa lo strumento.

### Da dove arrivano

Sono state scritte per l'app Decometer per il telefono e riportate qui, così
che lo strumento sul computer e quello che si tiene in mano mostrino le stesse
cose. La banda si ricava dalla frequenza invece di chiederla: è un conto di
due righe e non lega il frontalino a una proprietà in più del ponte.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa. La tabella delle bande è stata verificata
sulle frequenze FT8 reali: 14,074 dà 20m, 7,074 dà 40m, 3,573 dà 80m, 10,136
dà 30m, 50,313 dà 6m e 144,174 dà 2m.
