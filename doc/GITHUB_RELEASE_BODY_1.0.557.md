# Decodium 4 FT2 v1.0.557

## English (British)

### Settings pages no longer overlap in compact layouts

This release fixes a layout failure that could make controls, labels and
selectors overlap at the top of the Settings window. It was most visible on
Windows systems using display scaling, reduced window sizes or monitors whose
available geometry caused Settings to switch from its four-column layout to
the compact two-column layout.

Several controls still requested a three-column span after that switch. Qt
then attempted to place them in cells that did not exist or were already in
use, collapsing rows and stacking unrelated controls on top of one another.
Every affected span is now calculated from the active column count: it remains
three columns in the normal layout and becomes one control column beside its
label in the compact layout.

The correction covers the Radio, Audio, Display, Decode, Reporting, Colours
and Advanced pages, including long rows such as audio devices, RTL-SDR
settings, network reporting destinations, CAT controls, decoder options and
display sliders. Pages that were already compatible with both layouts remain
unchanged.

### Reliable Settings scrolling on Linux and smaller displays

The shared Settings scrolling container now measures both the implicit width
and height of its page contents and explicitly enables horizontal and vertical
scrolling. This prevents controls from becoming unreachable when a page is
larger than the available screen or when moving Settings to a monitor with a
different resolution or aspect ratio.

A portable mouse-wheel bridge has also been added for Linux desktop
combinations where wheel events were previously retained by the focused field
or selector instead of scrolling the page. The normal wheel scrolls
vertically, Shift+wheel scrolls horizontally, and trackpad horizontal gestures
remain supported. The bridge does not intercept clicks from controls or the
scroll bars.

### Scope and compatibility

These changes are confined to the Settings user interface. Radio control,
audio processing, decoding, transmission, waterfall and panadapter paths are
unchanged. The same responsive layout rules are used on Windows, macOS and
Linux.

---

## Italiano

### Le schede di Impostazioni non si sovrappongono piu' nei layout compatti

Questa versione corregge un problema di impaginazione che poteva sovrapporre
controlli, etichette e selettori nella parte superiore della finestra
Impostazioni. Il difetto era particolarmente visibile sui sistemi Windows con
ridimensionamento dello schermo, con finestre ridotte o su monitor la cui area
disponibile faceva passare Impostazioni dal layout a quattro colonne a quello
compatto a due colonne.

Dopo il passaggio alcuni controlli continuavano a richiedere uno spazio di tre
colonne. Qt tentava quindi di inserirli in celle inesistenti o gia' occupate,
comprimendo le righe e impilando controlli non correlati. Ogni estensione
interessata viene ora calcolata in base al numero di colonne attivo: resta di
tre colonne nel layout normale e diventa una sola colonna di controllo accanto
alla propria etichetta nel layout compatto.

La correzione copre le pagine Radio, Audio, Display, Decodifica, Reporting,
Colori e Avanzate, comprese le righe piu' lunghe come dispositivi audio,
impostazioni RTL-SDR, destinazioni di reporting di rete, controlli CAT, opzioni
del decoder e cursori del display. Le pagine gia' compatibili con entrambi i
layout rimangono invariate.

### Scorrimento affidabile di Impostazioni su Linux e sugli schermi piccoli

Il contenitore condiviso delle pagine Impostazioni ora misura sia la larghezza
sia l'altezza implicita dei contenuti e abilita esplicitamente lo scorrimento
orizzontale e verticale. In questo modo i controlli non diventano
irraggiungibili quando una pagina supera lo spazio disponibile o quando
Impostazioni viene spostata su un monitor con risoluzione o rapporto differente.

E' stato inoltre aggiunto un ponte portabile per la rotella del mouse destinato
alle combinazioni desktop Linux nelle quali l'evento veniva trattenuto dal
campo o dal selettore attivo invece di scorrere la pagina. La rotella normale
scorre verticalmente, Maiusc+rotella scorre orizzontalmente e restano supportati
i gesti orizzontali del trackpad. Il ponte non intercetta i clic destinati ai
controlli o alle barre di scorrimento.

### Ambito e compatibilita'

Le modifiche sono limitate all'interfaccia delle Impostazioni. Controllo radio,
elaborazione audio, decodifica, trasmissione, waterfall e panadapter rimangono
invariati. Le stesse regole responsive vengono utilizzate su Windows, macOS e
Linux.
