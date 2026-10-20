# Decodium 4 v1.0.648

## English (UK)

A single fix on top of v1.0.647: the update window is visible again.

### The update window was measuring itself against a zero-sized parent

- Reported with a screenshot: clicking **Check for updates** produced no panel at all. The text and the buttons were scattered over the main window, over the waterfall and the decode list, with nothing behind them.
- The dialog is created inside a `Loader`, and a Loader has no size. Its width and height were computed as "the parent's space minus a margin", which came out **negative**. With negative dimensions the background draws nothing — hence the missing panel — while the children, which layouts do not clip, spread across the window. The same zero-sized parent is why the window was never centred but pinned to a corner.
- The window now measures itself against the application overlay, which has real dimensions: parent and centring on the overlay, width and height within the available space with a floor and a ceiling. The height is no longer tied to the implicit content height, which formed a loop with the scrollable notes panel.
- Measured before and after with a temporary probe at open time: `w=-48 h=-48 contentH=-108` became `w=680 h=560 contentH=436`, and a capture of the window shows the framed panel, the contents inside it and the release notes rendered as formatted text.

### Downloads

Source ZIP and tar.gz archives are available for this tag. The Windows x64 installer EXE is attached to this release; GitHub workflows add Linux x86_64/aarch64 AppImages with checksum files as they finish.

---

## Italiano

Una sola correzione sopra la 1.0.647: la finestra dell'aggiornamento si vede di nuovo.

### La finestra dell'aggiornamento misurava se stessa su un genitore largo zero

- Segnalata con un'immagine: cliccando **Verifica aggiornamenti** non compariva alcun riquadro. Testo e pulsanti finivano sparsi sopra la finestra principale, sopra la cascata e la lista delle decodifiche, senza niente dietro.
- La finestra nasce dentro un `Loader`, e un Loader non ha dimensioni. Larghezza e altezza venivano calcolate come «lo spazio del genitore meno un margine», e risultavano **negative**. Con misure negative lo sfondo non disegna nulla — di qui il riquadro mancante — mentre i figli, che i layout non ritagliano, si spargono sulla finestra. Lo stesso genitore di dimensione zero è il motivo per cui la finestra non era mai centrata ma appoggiata in un angolo.
- Ora la finestra si misura sull'overlay dell'applicazione, che le dimensioni vere ce l'ha: genitore e centratura sull'overlay, larghezza e altezza entro lo spazio disponibile con un minimo e un tetto. L'altezza non è più legata all'altezza implicita del contenuto, che formava un anello con il pannello scorrevole delle note.
- Misurato prima e dopo con una sonda temporanea all'apertura: `w=-48 h=-48 contentH=-108` è diventato `w=680 h=560 contentH=436`, e nella cattura della finestra si vedono il riquadro, il contenuto dentro la cornice e le note di rilascio rese come testo formattato.

### Download

Per questo tag sono disponibili gli archivi sorgente ZIP e tar.gz. L'installer Windows x64 EXE è allegato a questo rilascio; i workflow di GitHub aggiungono le AppImage Linux x86_64/aarch64 con i file di checksum man mano che finiscono.
