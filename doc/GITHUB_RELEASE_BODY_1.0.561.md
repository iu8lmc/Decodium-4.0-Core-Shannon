# Decodium 4 FT2 v1.0.561

This release continues directly from v1.0.560. It is a translation delivery
release: the interface texts revised in v1.0.560 were updated in the source
catalogues but the compiled catalogues shipped with the application were not
regenerated, so the revised decoder wording appeared untranslated at runtime.

## English (British)

### Compiled translation catalogues regenerated

v1.0.560 revised the decoder option label from `Deep decode in TX` to
`Deep decode of last RX slot during TX (list only)` and updated the
corresponding entries in the `.ts` source catalogues for every supported
language. The compiled `.qm` catalogues, which are the files the application
actually loads at run time, were not rebuilt from those sources.

The practical effect was that the revised label had no matching entry in any
compiled catalogue, so Qt fell back to the untranslated English source string.
The option was therefore shown in English in all fourteen translated languages,
including the interface language of the user's choice.

All catalogues have now been regenerated with `lrelease` from the current
sources. Every language reports zero unfinished entries, and the revised
decoder wording is present in both places it occurs, the main Settings dialog
and the tabbed Settings page.

No source code, decoder, transmit path or user interface behaviour is changed
by this release.

### Validation

The catalogues were regenerated for all fifteen files and each was verified to
report zero unfinished entries. The revised decoder string was confirmed present
in the compiled Italian catalogue, where it was previously absent. A complete
`decodium_qml` build was performed and the installer was packaged from it.

## Italiano

### Rigenerati i cataloghi di traduzione compilati

La v1.0.560 ha riformulato l'etichetta dell'opzione del decoder da
`Deep decode in TX` a `Deep decode of last RX slot during TX (list only)`,
aggiornando le voci corrispondenti nei cataloghi sorgente `.ts` di tutte le
lingue supportate. I cataloghi compilati `.qm`, che sono i file effettivamente
caricati dall'applicazione all'avvio, non sono però stati ricostruiti a partire
da quei sorgenti.

L'effetto pratico era che l'etichetta riformulata non aveva alcuna voce
corrispondente nei cataloghi compilati, per cui Qt ripiegava sulla stringa
inglese non tradotta. L'opzione appariva quindi in inglese in tutte e quattordici
le lingue tradotte, compresa la lingua scelta dall'utente.

Tutti i cataloghi sono ora stati rigenerati con `lrelease` a partire dai
sorgenti correnti. Ogni lingua riporta zero voci non completate e la nuova
formulazione del decoder è presente in entrambi i punti in cui compare, la
finestra principale di Impostazioni e la pagina a schede.

Questa release non modifica il codice sorgente, il decoder, il percorso di
trasmissione o il comportamento dell'interfaccia.

### Verifiche

I cataloghi sono stati rigenerati per tutti e quindici i file e per ciascuno è
stata verificata l'assenza di voci non completate. La stringa riformulata del
decoder è stata confermata presente nel catalogo italiano compilato, dove prima
mancava. È stata eseguita una compilazione completa di `decodium_qml` e
l'installer è stato prodotto a partire da essa.
