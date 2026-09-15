# Decodium 4 FT2 v1.0.573

A small release about not losing things. Closing a panel in the DX-Pedition
workspace made it vanish without saying where it had gone; now the workspace
says so. The DecoPort password can also be changed from its own window instead
of only during installation.

## English (British)

### A closed panel says where it went

Every panel in the DX-Pedition workspace has a ✕ in its header, and closing one
collapses its slot so the others take the space. That part was right — but
nothing told you how to get the panel back, and the answer, the **PANELS** button
in the tactical bar, was easy to miss.

Now the button counts: with one panel closed it reads **PANELS 1**, in amber
instead of grey. And the three controls in every panel header finally say what
they do when you hover them — in particular the ✕, which now reads *"Close this
panel — reopen it from PANELS in the top bar"*.

### The DecoPort password, without reinstalling

The password was asked during installation and there was no way to change it
afterwards short of reinstalling or editing the settings file by hand. The
DecoPort window now has a **SECURITY** section: it says whether a password is
set, lets you set or change one, and lets you remove it — removing it stops the
gateway, because a radio without a key is not published.

The field never shows the password in use, because it does not exist anywhere:
what is stored is the derived key. The **PUBLISH** button stays inert until a
password exists, since without one the gateway would refuse to start anyway.
Changing it shuts out anyone connected with the old one, and the window says so
rather than letting you discover it.

### Smaller things

On a narrow window the frequency in the tactical header now steps down in size
instead of crowding the controls next to it. The buttons in that bar refer to
their delegate by id rather than through `parent`, which is the fragile chain in
a delegate with required properties.

### Absorbed from upstream

The DecoPort sources are now part of **both** application targets. They had been
added only to the QML frontend; the classic target compiles the same bridge and
therefore failed to link. It was missed here because only the QML target is built
locally — upstream CI, which builds everything, caught it.

### Validation

The workspace was exercised with a panel closed: the slot collapses, the column
redistributes, and the button reads `PANELS 1`. Every button in the tactical bar
was verified present, visible and correctly labelled by instrumenting the live
scene, after two screen captures had suggested otherwise.

## Italiano

### Un pannello chiuso dice dov'è andato

Ogni pannello del workspace DX-Pedition ha una ✕ nell'intestazione, e chiudendone
uno il suo posto collassa perché gli altri si prendano lo spazio. Quella parte
era giusta — ma niente ti diceva come riaverlo indietro, e la risposta, il
pulsante **PANELS** nella barra tattica, era facile da non notare.

Adesso il pulsante conta: con un pannello chiuso legge **PANELS 1**, in ambra
invece che in grigio. E i tre comandi nell'intestazione di ogni pannello dicono
finalmente cosa fanno quando ci passi sopra — in particolare la ✕, che ora
riporta *"Chiudi questo pannello — lo riapri da PANELS nella barra in alto"*.

### La password DecoPort, senza reinstallare

La password si chiedeva durante l'installazione e non c'era modo di cambiarla se
non reinstallando o mettendo le mani nel file delle impostazioni. La finestra
DecoPort ha ora una sezione **SICUREZZA**: dice se una password c'è, permette di
metterla o cambiarla, e di toglierla — togliendola il gateway si ferma, perché
una radio senza chiave non viene pubblicata.

Il campo non mostra mai la password in uso, perché non esiste da nessuna parte:
quello che è salvato è la chiave derivata. Il pulsante **PUBBLICA** resta inerte
finché non c'è una password, dato che senza il gateway rifiuterebbe comunque di
partire. Cambiandola, chi era collegato con la vecchia resta fuori, e la finestra
lo dice invece di lasciartelo scoprire.

### Cose minori

Su una finestra stretta la frequenza nell'intestazione tattica si rimpicciolisce
a scalini invece di schiacciare i comandi accanto. I pulsanti di quella barra si
riferiscono al proprio delegate per id invece che tramite `parent`, che dentro un
delegate con proprietà richieste è la catena fragile.

### Assorbito da monte

I sorgenti DecoPort fanno ora parte di **entrambi** i target dell'applicazione.
Erano stati aggiunti solo al frontend QML; il target classico compila lo stesso
bridge e quindi non linkava. Qui è sfuggito perché in locale si compila solo il
target QML — l'ha preso la CI a monte, che li compila tutti.

### Verifiche

Il workspace è stato provato con un pannello chiuso: il posto collassa, la
colonna ridistribuisce, e il pulsante legge `PANELS 1`. Ogni pulsante della barra
tattica è stato verificato presente, visibile ed etichettato correttamente
strumentando la scena viva, dopo che due catture dello schermo avevano suggerito
il contrario.
