# Decodium 4 FT2 v1.0.563

This release continues directly from v1.0.562. It repairs a build step that
could delete the application's QML sources, and completes the shared CAT
server's answer to the handshake Hamlib performs when a client connects.

## English (British)

### In-source builds no longer delete the QML tree

The `sync_decodium_qml` build step copies a clean QML tree next to the
executable, removing the destination first so that stale `qmlcachegen`
artifacts from older builds cannot shadow updated sources. Qt prefers an
adjacent `.qmlc` file over the source it was generated from, so that removal
is deliberate.

When the build directory is the source directory, however, origin and
destination are the same place. The step deleted the QML files and then tried
to copy them from themselves, so they were simply lost. Anyone configuring an
in-source build was left with seventy-four missing files and nothing to
explain why.

The step is now guarded: when the two directories differ it behaves exactly as
before, and when they coincide it becomes an empty target, because the files
are already where they need to be. Out-of-source builds, which is how the
released installers are produced, are unaffected.

### `\get_lock_mode` answered on the shared CAT server

Hamlib asks for the VFO lock state when a client opens a connection. The
shared CAT server did not handle the command, so every connecting client left
an "unhandled command" line in the log. The server now answers `0`: the radio
has no VFO lock, and saying so is clearer than staying silent.

### Validation

A complete `decodium_qml` build was performed and the installer was packaged
from it. The guarded build step was exercised by the out-of-source build used
for this release.

## Italiano

### Compilare dentro i sorgenti non cancella più l'albero QML

Il passo di compilazione `sync_decodium_qml` copia un albero QML pulito
accanto all'eseguibile, cancellando prima la destinazione perché i residui
`qmlcachegen` delle compilazioni precedenti non coprano i sorgenti aggiornati.
Qt preferisce un file `.qmlc` adiacente al sorgente da cui è stato generato,
quindi quella cancellazione è voluta.

Quando però la cartella di compilazione è la cartella dei sorgenti, origine e
destinazione sono la stessa cosa. Il passo cancellava i file QML e poi provava
a ricopiarli da sé stessi, cioè li perdeva. Chi configurava una compilazione
dentro i sorgenti si ritrovava settantaquattro file spariti e nessun indizio
sul perché.

Il passo è ora protetto da una guardia: quando le due cartelle sono diverse si
comporta esattamente come prima, quando coincidono diventa un target vuoto,
perché i file sono già al loro posto. Le compilazioni fuori dai sorgenti, con
cui vengono prodotti gli installer pubblicati, non cambiano.

### `\get_lock_mode` gestito sul server CAT condiviso

Hamlib chiede lo stato di blocco del VFO quando un client apre il
collegamento. Il server CAT condiviso non gestiva quel comando, così ogni
client che si collegava lasciava una riga «comando non gestito» nel registro.
Il server risponde ora `0`: la radio non ha un blocco del VFO, e dirlo è più
chiaro che tacere.

### Verifiche

È stata eseguita una compilazione completa di `decodium_qml` e l'installer è
stato prodotto a partire da essa. Il passo protetto è stato esercitato dalla
compilazione fuori dai sorgenti usata per questa release.
