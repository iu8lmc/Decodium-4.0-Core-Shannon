# Decodium 4 FT2 v1.0.566

This release continues directly from v1.0.565. It makes the transmit meters
follow the modulation instead of stepping once a second, and repairs the
DECØMETER window, which could not be resized at all.

## English (British)

### Transmit meters now follow the modulation

The rig was polled once per second, and that is also the floor the polling
code allowed: the interval is expressed in whole seconds. Forward power, SWR
and ALC therefore changed value once a second, so the needle jumped from one
reading to the next instead of following the transmission. Watching the meter
during an over told you almost nothing.

While the transmitter is keyed the interval is now a quarter of the
configured one, and never below 250 ms — roughly four readings a second
instead of one. The moment the transmitter drops, the full interval returns.

Reception is deliberately left alone. Frequent polling in receive is what used
to block the main thread on slow serial links, the fault repaired in v1.0.204,
and there is nothing to gain there: the transmit meters read zero anyway.

A short burst of fast polling already existed, but only for the two readings
immediately after the PTT changed; the rest of the over ran at one reading per
second.

### The DECØMETER window could not be resized

Since v1.0.560 the frameless meter window handed its corner grips to Windows
through `startSystemResize()`. That call answers `true` — meaning "I will take
care of the resize" — and then does nothing, because a frameless window has no
system border for Windows to drag. That answer switched off the manual resize
path, which is guarded by exactly the flag the answer sets, so the window
became impossible to resize: the corners responded to the mouse and nothing
happened.

Confirmed on the spot with a probe: the grip receives the press, the button is
down, the call returns `true`, and the geometry never changes.

The manual path is used again. It updates width and height on every mouse
move: less smooth than the system one, but it is under our control, it keeps
the exact 15:7 face ratio, and above all it works.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it. The resize was confirmed by hand on the running application.

## Italiano

### I misuratori di trasmissione seguono la modulazione

La radio veniva interrogata una volta al secondo, ed era anche il minimo
consentito dal codice di polling: l'intervallo è espresso in secondi interi.
Potenza diretta, ROS e ALC cambiavano quindi valore una volta al secondo, e
l'ago faceva un salto da una lettura all'altra invece di seguire la
trasmissione. Guardare lo strumento durante un over non diceva quasi nulla.

Mentre il trasmettitore è in aria l'intervallo è ora un quarto di quello
impostato, e mai sotto i 250 ms — circa quattro letture al secondo invece di
una. Appena si torna in ascolto riprende l'intervallo pieno.

La ricezione è lasciata deliberatamente com'era. È lì che il polling fitto
bloccava il thread principale sulle seriali lente, il guasto risolto nella
1.0.204, e non ci sarebbe nulla da guadagnare: a trasmettitore fermo i
misuratori di trasmissione leggono zero comunque.

Una breve accelerazione esisteva già, ma solo per le due letture subito dopo
il cambio di PTT; il resto dell'over andava a una lettura al secondo.

### La finestra DECØMETER non si poteva ridimensionare

Dalla 1.0.560 la finestra senza cornice affidava i propri angoli a Windows
attraverso `startSystemResize()`. Quella chiamata risponde `true` — cioè «al
ridimensionamento ci penso io» — e poi non fa nulla, perché una finestra senza
cornice non ha un bordo di sistema da trascinare. Quella risposta spegneva il
percorso manuale, che è protetto proprio dalla condizione che la risposta
accende, e la finestra diventava impossibile da ridimensionare: gli angoli
rispondevano al mouse e non succedeva niente.

Verificato sul posto con una sonda: la maniglia riceve la pressione, il tasto
risulta premuto, la chiamata torna `true`, e la geometria non cambia mai.

Si torna al percorso manuale, che aggiorna larghezza e altezza a ogni
movimento del mouse: meno fluido di quello di sistema, ma è sotto il nostro
controllo, mantiene esatto il rapporto 15:7 del quadrante e soprattutto
funziona.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa. Il ridimensionamento è stato confermato a
mano sull'applicazione in esecuzione.
