# Decodium 4 FT2 1.0.342

Release di stabilizzazione pubblicata dopo la 1.0.341 per ridurre la verbosita' dei log diagnostici, conservare le modifiche locali arrivate da Windows e mantenere il codice compilabile anche su macOS.

## Cambiamenti principali dalla 1.0.341

### Build macOS e portabilita'

- Corretto il build macOS con Clang: `ftOpenMpAffinityForCores()` viene compilata solo su Windows quando OpenMP e' realmente disponibile.
- Il ramo resta compatibile con il build locale `decodium_qml` su macOS dopo le modifiche sviluppate da Windows.
- Ripulite anomalie di fine riga nel worker FT2 che rendevano sporco `git diff --check`.

### Log meno verbosi

- I log `PANMETRIC` del panadapter, waterfall, overlay, QSG frame/phase e decode labels sono stati portati da intervalli brevi a circa 60 secondi.
- Il log profilo `MAPGPU` della live map e' stato ridotto a circa 60 secondi.
- Dopo il primo frame della live map non viene piu' emesso subito un profilo `MAPGPU` duplicato.
- I log `DEPTHDBG` sono ora spenti di default e si abilitano solo con la variabile ambiente `DECODIUM_DEPTHDBG`.
- L'obiettivo e' mantenere disponibili le diagnostiche utili senza riempire il log durante l'uso normale.

### Diagnostica decode e UI

- Aggiunte metriche `DECODEMETRIC` nei worker FT8, FT4 e FT2 con tempi di attesa, decode e totale.
- Le metriche includono thread attivi/richiesti, dimensione audio, parametri di profondita', range di ricerca e thread id.
- Aggiunti hook sul main thread per misurare le fasi `decodeReady` e aggiornamento modello, cosi' gli eventuali stall UI possono essere correlati al decode.
- La status bar espone indicatori runtime aggiuntivi per monitor GPU e thread FT.

### Panadapter, waterfall e QML

- Conservati gli aggiornamenti locali al path GPU-direct del panadapter/waterfall e alle metriche di rendering QSG.
- Aggiornate parti QML di `Main`, `DecodeWindow`, `LiveMapPanel` e `StatusBar` per riflettere le diagnostiche e gli indicatori runtime piu' recenti.
- Mantenute le pulizie minori nel DX cluster e nel percorso UI senza alterare la logica di release precedente.

### Metadati release

- Versione locale allineata a `1.0.342` tramite `fork_release_version.txt`.
- Installer Inno Setup allineato a `1.0.342`.
- Installer NSIS allineato a `1.0.342`, inclusi nome output e `VIProductVersion`.
- Workflow macOS legacy allineato a `1.0.342` per evitare artefatti con versione vecchia se viene lanciato manualmente.
- Changelog aggiornato con il riepilogo delle modifiche rispetto alla 1.0.341.

## Impatto utente

- I log ordinari sono piu' leggibili: `MAPGPU`, `PANMETRIC` e `DEPTHDBG` non sommergono piu' il file durante una sessione normale.
- Le diagnostiche decode restano disponibili quando servono a capire rallentamenti o stall UI.
- Il codice che su Windows compila e si avvia resta verificato anche sul build macOS locale.

## Artefatti previsti

- Sorgenti GitHub automatici della release/tag `1.0.342`.
- Installer Microsoft Windows x64: `Decodium_1.0.342_Setup_x64.exe`.
- DMG/ZIP macOS Apple Silicon prodotti dai runner GitHub.
- DMG/ZIP macOS Intel prodotti dai runner GitHub.
- AppImage Linux x86_64 e Linux aarch64 prodotti dai runner GitHub.

## Verifica locale

Prima della pubblicazione sono stati eseguiti:

```bash
git diff --check
cmake --build build-local-macos --target decodium_qml -j2
```
