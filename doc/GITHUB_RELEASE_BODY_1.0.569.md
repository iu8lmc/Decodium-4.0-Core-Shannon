# Decodium 4 FT2 v1.0.569

This release moves the whole transmitting side of a DX-pedition into the
DX-Pedition workspace, opens the multi-slot answering machine from one slot up
to ten, and lets the free slots call CQ in parallel. Along the way it fixes a
panadapter that stopped dead whenever the DX-Pedition workspace was open.

## English (British)

### Transmitting from one place

The right-hand column of the DX-Pedition workspace used to hold the ordinary TX
macros. It now holds **DX-Ped TX · Slot**, which gathers everything the operator
needs while working a pile-up: the multi-slot switch, the number of slots, the
list of parallel QSOs with their frequency, step, report and retries, the queue
of callers still waiting, and — folded underneath, one click away — the classic
TX macros, ENABLE TX, HALT and TUNE.

Each slot row carries two actions of its own. **LOG** writes the QSO without
waiting for the sign-off, for the station that has vanished after the exchange
was already complete. **✕** abandons the slot; the station can come back later.

### One to ten slots

The cap was two to five and is now **one to ten**. One slot is not the same as
the old serial mode: it still answers on the **caller's** frequency, which is
the MSHV model. Ten is a full pile-up.

The physics has not changed and is worth stating plainly: the transmitter power
is divided between the streams, roughly one over the square root of their
number, so at ten slots each signal is about ten decibels weaker than a single
one would be. With a modest antenna it is better to start at three or four and
climb only if the reports hold up.

### The free slots now call CQ

Until now a CQ went out only when there was no active slot at all, and it was a
single CQ on the transmit frequency. That is not how a DX-pedition works.

Every position not occupied by a QSO now calls **CQ on a frequency of its own**,
mixed into the same transmission as the QSOs in progress: with a cap of ten and
three QSOs running, three QSO messages and seven CQs go out together. As
answers arrive the slots convert to QSOs, and whatever is left keeps calling.

The frequencies start at the operator's transmit frequency and climb in steps of
60 Hz, then fall back below it, skipping anything within 50 Hz of a frequency
already in use — an FT8 signal is 50 Hz wide, and those are the same figures the
slot de-duplication already used.

This is on by default inside multi-stream, which is itself opt-in and off by
default, and there is a **CQ MULTI** switch in the panel to go back to the
single CQ.

### Detachable and interchangeable panels

The seven panels of the workspace — Cluster/MAM, PSK Reporter, Waterfall, Full
Spectrum, Signal RX, DX-Ped TX, Log — can now be rearranged and torn off.

The **⠿** handle in a panel header drags it onto another panel to exchange the
two. The **↗** button gives the panel a window of its own; closing that window
docks it back where it came from. Panel positions, which panels are detached,
and the position and size of each detached window are all remembered.

Underneath, the panels are never rebuilt: they are moved between fixed slots by
re-parenting, so the waterfall keeps its audio feed and the TX panel keeps its
logging confirmation across a swap.

### A button to get in, and one thing that had to go

The classic toolbar gains a **◤ DX-Ped** button between History and CAT, so the
workspace is one click away instead of a trip through the settings. It can be
moved like any other toolbar button, or hidden from Settings → UI Buttons.

The draggable world clock no longer appears while the DX-Pedition workspace is
open. The workspace covers the whole window and has its own UTC clock in the
tactical bar; the floating one simply landed on top of it.

### The panadapter that stopped in DX-Pedition mode

With the DX-Pedition workspace open, the spectrum and waterfall froze. The bug
had been there since the workspace was introduced and showed itself only when
the GPU path was unavailable — software rendering, or the OpenGL GPU FFT turned
off in Advanced.

A hidden panadapter accepts an audio frame and quietly does nothing with it,
returning success. The workspace adds a second panadapter, so the classic one
carries on registered but hidden, and its polite acceptance masked the refusal
of the visible one. The frame dispatcher concluded that the GPU path was still
working, never fell back to the CPU FFT, and delivered nothing to anybody.

Panels that cannot draw are now excluded from that vote, so the refusal of the
visible panadapter is heard and the CPU fallback engages as it should.

### Validation

Built and started clean, QML free of errors, and the panadapter confirmed alive
in the workspace against real signals on 20 m. The parallel CQ transmission path
has **not** been exercised on the air: it needs a real transmitter and a real
pile-up. The log line to look for when arming it reads
`MAM dispatch: N parallel CQ stream(s)`.

## Italiano

### Trasmettere da un posto solo

La colonna destra del workspace DX-Pedition conteneva le normali macro TX. Ora
contiene **DX-Ped TX · Slot**, che raccoglie tutto quello che serve durante un
pile-up: l'interruttore multi-slot, il numero di slot, la lista dei QSO
paralleli con frequenza, passo, rapporto e ritentativi, la coda dei chiamanti in
attesa e, ripiegate sotto a un click di distanza, le macro TX classiche con
ENABLE TX, HALT e TUNE.

Ogni riga di slot ha due azioni proprie. **LOG** registra il QSO senza aspettare
il saluto finale, per la stazione sparita a scambio già completo. **✕** abbandona
lo slot; la stazione può rifarsi viva più tardi.

### Da uno a dieci slot

Il limite era da due a cinque, ora è da **uno a dieci**. Un solo slot non
equivale al vecchio modo seriale: risponde comunque sulla frequenza **del
chiamante**, che è il modello MSHV. Dieci è il pile-up pieno.

La fisica non cambia e conviene dirla chiaramente: la potenza si divide fra gli
stream, all'incirca uno su radice del loro numero, quindi a dieci slot ogni
segnale è circa dieci decibel più debole di uno singolo. Con un'antenna modesta
conviene partire da tre o quattro e salire solo se i rapporti reggono.

### Gli slot liberi ora chiamano CQ

Fino a ieri il CQ partiva solo se non c'era nessuno slot attivo, ed era un CQ
solo sulla frequenza di trasmissione. Non è così che lavora una DX-pedition.

Ogni posizione non occupata da un QSO ora chiama **CQ su una frequenza propria**,
mescolata alla stessa trasmissione dei QSO in corso: con dieci slot e tre QSO in
atto escono insieme tre messaggi di QSO e sette CQ. Man mano che arrivano le
risposte gli slot si convertono in QSO, e quel che resta continua a chiamare.

Le frequenze partono da quella di trasmissione e salgono a passi di 60 Hz, poi
ripiegano sotto, saltando qualunque cosa disti meno di 50 Hz da una frequenza
già occupata: un segnale FT8 è largo 50 Hz, e sono le stesse misure che il
de-duplicatore degli slot usava già.

È attivo di default dentro il multi-stream, che a sua volta è opzionale e
spento di default, e nel pannello c'è l'interruttore **CQ MULTI** per tornare al
CQ singolo.

### Pannelli staccabili e interscambiabili

I sette pannelli del workspace — Cluster/MAM, PSK Reporter, Waterfall, Full
Spectrum, Signal RX, DX-Ped TX, Log — si possono riordinare e staccare.

La maniglia **⠿** nell'intestazione trascina il pannello su un altro per
scambiarli. Il pulsante **↗** gli dà una finestra propria; chiudendo quella
finestra il pannello torna al suo posto. Vengono ricordati la disposizione, quali
pannelli sono staccati e posizione e dimensione di ogni finestra staccata.

Sotto il cofano i pannelli non vengono mai ricostruiti: si spostano fra slot
fissi cambiando genitore, così la cascata non perde il flusso audio e il
pannello TX non perde la conferma di registrazione quando lo si sposta.

### Un pulsante per entrare, e una cosa che doveva sparire

La toolbar classica guadagna un pulsante **◤ DX-Ped** fra Cronologia e CAT: il
workspace è a un click invece che dentro le impostazioni. Si sposta come
qualsiasi altro pulsante della toolbar, o si nasconde da Impostazioni →
Pulsanti UI.

L'orologio trascinabile non compare più mentre il workspace DX-Pedition è
aperto. Il workspace occupa tutta la finestra e ha il suo orologio UTC nella
barra tattica; quello flottante gli finiva semplicemente sopra.

### Il panadapter che si fermava in DX-Pedition

Con il workspace DX-Pedition aperto, spettro e cascata si bloccavano. Il difetto
c'era da quando il workspace esiste e si manifestava solo quando il percorso GPU
non era disponibile: rendering software, oppure la FFT GPU OpenGL disattivata
nelle impostazioni avanzate.

Un panadapter nascosto accetta un frame audio e non ne fa nulla, restituendo
successo. Il workspace ne aggiunge un secondo, quindi quello classico resta
registrato ma invisibile, e il suo assenso di cortesia mascherava il rifiuto di
quello visibile. Il distributore dei frame concludeva che il percorso GPU
funzionasse ancora, non passava mai alla FFT su CPU e non consegnava niente a
nessuno.

I pannelli che non possono disegnare sono ora esclusi da quel conteggio, così il
rifiuto del panadapter visibile viene sentito e il ripiego su CPU entra in
funzione come deve.

### Verifiche

Compilato e avviato pulito, QML senza errori, panadapter confermato vivo nel
workspace su segnali reali in 20 metri. Il percorso di trasmissione dei CQ
paralleli **non** è stato provato in aria: servono un trasmettitore vero e un
pile-up vero. La riga di log da cercare quando lo si arma è
`MAM dispatch: N parallel CQ stream(s)`.
