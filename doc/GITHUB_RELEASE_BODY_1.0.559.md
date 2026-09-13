# Decodium 4 FT2 v1.0.559

## English (British)

### The dashboard now reopens maximised

Decodium now preserves the native maximised state of the main dashboard when
the application is closed and opened again. This primarily corrects the
behaviour reported on Windows, where closing a maximised dashboard stored its
expanded dimensions but not the actual window state. The next launch could
therefore show an oversized window in normal mode instead of restoring the
maximised dashboard.

The normal, windowed geometry is now stored separately from the maximised
state. Moving or resizing the dashboard updates that normal geometry only
while the window is genuinely in windowed mode. Closing while maximised no
longer overwrites it with full-screen desktop dimensions, so restoring the
window later still returns to a valid size and position.

Startup presentation has also been centralised. After the Qt graphics backend
has been configured, the boot loader repeats the requested native state rather
than issuing an unconditional normal `show()` call that could cancel a
restored maximised state. The implementation uses Qt window states and applies
equally to Windows, macOS and Linux. A minimised launch is not persisted, and
the existing explicit full-screen behaviour remains unchanged.

### Settings no longer creates an enormous horizontal scroll range

The responsive Settings container introduced in v1.0.557 correctly made both
scroll directions available, but its horizontal extent was also derived from
the implicit width of the first page item. A long translated label, a spanning
control or a large layout hint could therefore make a page appear thousands of
pixels wide, producing a very long and mostly empty horizontal scroll range.

Horizontal extent is now governed only by the active responsive layout's
declared minimum width. At normal window sizes the page stays aligned with the
viewport and the unnecessary horizontal bar disappears. On genuinely narrow
screens, a bounded horizontal range remains available so controls are still
reachable. Vertical scrolling, normal mouse-wheel operation, Shift+wheel and
trackpad horizontal gestures remain supported.

This is a correction to the shared Settings viewport, so all Settings pages
benefit without duplicating platform-specific rules in individual tabs.

### Compatibility and validation

The changes are confined to window-state persistence and Settings layout
geometry. CAT control, audio capture, decoding, transmission, the waterfall,
the panadapter and Live Map rendering paths are unchanged.

Validation included QML linting, a focused QML regression test proving that a
50,000-pixel child implicit width can no longer expand the Settings page, a
successful `decodium_qml` build and a clean whitespace/error check.

---

## Italiano

### La dashboard ora si riapre massimizzata

Decodium conserva adesso lo stato nativo massimizzato della dashboard
principale quando il programma viene chiuso e riaperto. La correzione riguarda
in particolare il comportamento segnalato su Windows, dove la chiusura di una
dashboard massimizzata salvava le dimensioni estese ma non lo stato reale della
finestra. Al successivo avvio poteva quindi apparire una finestra enorme in
modalità normale invece della dashboard massimizzata.

La geometria normale della finestra viene ora salvata separatamente dallo stato
massimizzato. Spostamenti e ridimensionamenti aggiornano questa geometria solo
quando la dashboard si trova realmente in modalità finestra. La chiusura mentre
è massimizzata non la sovrascrive più con le dimensioni dell'intero desktop;
quando si ripristina la finestra rimangono quindi disponibili posizione e
dimensioni normali valide.

È stata inoltre centralizzata la presentazione iniziale. Dopo la configurazione
del backend grafico Qt, il boot loader riapplica lo stato nativo richiesto
invece di eseguire sempre una normale chiamata `show()`, che poteva annullare il
ripristino massimizzato. L'implementazione usa gli stati finestra di Qt ed è
valida allo stesso modo su Windows, macOS e Linux. L'avvio minimizzato non viene
memorizzato e il comportamento esplicito dello schermo intero rimane invariato.

### Impostazioni non crea più uno scorrimento orizzontale enorme

Il contenitore responsive di Impostazioni introdotto nella v1.0.557 rendeva
correttamente disponibili entrambe le direzioni di scorrimento, ma calcolava
anche l'estensione orizzontale dalla larghezza implicita del primo elemento
della pagina. Un'etichetta tradotta molto lunga, un controllo esteso su più
colonne o un suggerimento di layout elevato potevano quindi rendere la pagina
larga migliaia di pixel, creando uno scorrimento orizzontale lunghissimo e in
gran parte vuoto.

L'estensione orizzontale dipende ora esclusivamente dalla larghezza minima
dichiarata dal layout responsive attivo. Con dimensioni normali la pagina resta
allineata alla finestra e la barra orizzontale superflua scompare. Sugli schermi
realmente stretti rimane disponibile uno scorrimento orizzontale limitato, così
tutti i controlli restano raggiungibili. Rimangono invariati lo scorrimento
verticale, la rotella normale, Maiusc+rotella e i gesti orizzontali del trackpad.

La correzione interessa il contenitore condiviso, quindi viene applicata a
tutte le pagine di Impostazioni senza duplicare regole specifiche per ogni
sistema operativo.

### Compatibilità e verifiche

Le modifiche sono limitate alla persistenza dello stato finestra e alla
geometria del layout di Impostazioni. Controllo CAT, acquisizione audio,
decodifica, trasmissione, waterfall, panadapter e rendering della Live Map
rimangono invariati.

Le verifiche comprendono il lint QML, un test di regressione mirato che dimostra
che un elemento con larghezza implicita di 50.000 pixel non può più espandere
la pagina Impostazioni, la compilazione completa del target `decodium_qml` e il
controllo degli errori di spaziatura nel diff.
