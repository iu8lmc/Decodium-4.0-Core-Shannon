# Decodium 4 FT2 v1.0.570

A fix release for v1.0.569. Tearing a panel off in the DX-Pedition workspace
could bring the application down with an access violation. **Anyone running
v1.0.569 should update.**

## English (British)

### What went wrong

v1.0.569 made the workspace panels detachable, and it did so by moving the live
panel out of its slot and into the new window. That is the one thing this
application had always avoided: for the popped-out waterfall and the popped-out
TX panel, Main.qml has always created a *second* instance rather than moving the
docked one, and a comment in the TX panel says as much — "the docked and popped
TxPanel instances coexist".

There is a reason for that. A live panel carries things bound to the window it
was built in: scene-graph nodes and GPU textures in the case of the waterfall,
popup overlays in the case of the TX panel. Carried into another window they
point at a graphics context that is no longer theirs, and the process dies. The
crash log ends with an Expose event on the floating window, immediately followed
by `CRASH: code=0xc0000005`.

### The fix

A detached panel is now a **separate instance**, loaded by the floating window
itself. The docked panel never leaves its slot; while it is shown in a window,
or closed, its slot is simply hidden. The only move that still happens is
between two slots of the same window, which shares one scene and is safe — so
swapping panels still keeps the waterfall's audio feed alive, as before.

The Cluster/MAM composite has been lifted out of the workspace into
`DxPedClusterPanel.qml` so the floating window can instantiate it too.

### Also in this release

The panel-close behaviour asked for after v1.0.569 shipped: closing a panel now
removes it from **both** places at once — the window and the dock — and the slot
**collapses** instead of leaving an empty gap. A column with no panels left
collapses too, so the remaining ones take the full width. A closed panel stops
drawing and stops consuming, and a new **PANELS** menu in the tactical bar shows
where every panel is (docked, window, closed), reopens the closed ones and
restores the default arrangement.

The floating window's title bar gains a **⇲ DOCK** button: its X now closes the
panel, so docking needed a control of its own.

### Validation

Reproduced the exact configuration that crashed — the TX panel detached, in the
DX-Pedition workspace — and the application now starts, shows the panel in its
own window and keeps running with no QML errors. The parallel CQ transmission
introduced in v1.0.569 remains untested on the air.

## Italiano

### Cos'era andato storto

La 1.0.569 ha reso staccabili i pannelli del workspace, e lo faceva spostando il
pannello vivo dal suo posto dentro la nuova finestra. È esattamente la cosa che
questa applicazione aveva sempre evitato: per la cascata staccata e per il
pannello TX staccato, Main.qml crea da sempre una *seconda* istanza invece di
spostare quella agganciata, e un commento dentro il pannello TX lo dice —
"the docked and popped TxPanel instances coexist".

Il motivo c'è. Un pannello vivo si porta dietro cose legate alla finestra in cui
è nato: nodi di scenegraph e texture GPU nel caso della cascata, overlay dei
popup nel caso del pannello TX. Portati in un'altra finestra puntano a un
contesto grafico che non è più il loro, e il processo muore. Il log del crash si
chiude con un evento Expose sulla finestra flottante e subito dopo
`CRASH: code=0xc0000005`.

### La correzione

Un pannello staccato è ora un'**istanza propria**, caricata dalla finestra
stessa. Il pannello agganciato non lascia mai il suo posto: mentre è mostrato in
finestra, o è chiuso, il suo slot viene semplicemente nascosto. L'unico
spostamento che resta è fra due slot della stessa finestra, che condividono la
stessa scena ed è sicuro — quindi scambiare i pannelli continua a non
interrompere il flusso audio della cascata, come prima.

Il composito Cluster/MAM è stato estratto dal workspace in
`DxPedClusterPanel.qml`, così anche la finestra staccata può istanziarlo.

### Anche in questa versione

Il comportamento di chiusura chiesto dopo la 1.0.569: chiudere un pannello ora
lo toglie da **entrambe** le parti insieme — finestra e posto agganciato — e lo
slot **collassa** invece di lasciare un buco. Anche una colonna rimasta senza
pannelli collassa, così le altre si prendono tutta la larghezza. Un pannello
chiuso smette di disegnare e di consumare, e il nuovo menu **PANELS** nella
barra tattica mostra dove si trova ogni pannello (agganciato, finestra, chiuso),
riapre quelli chiusi e ripristina la disposizione di partenza.

La finestra staccata guadagna un pulsante **⇲ AGGANCIA** nella sua intestazione:
la sua X adesso chiude il pannello, quindi riagganciare aveva bisogno di un
comando suo.

### Verifiche

Riprodotta la configurazione esatta che andava in crash — pannello TX staccato,
workspace DX-Pedition — e adesso l'applicazione parte, mostra il pannello nella
sua finestra e continua a funzionare senza errori QML. I CQ paralleli introdotti
con la 1.0.569 restano non provati in aria.
