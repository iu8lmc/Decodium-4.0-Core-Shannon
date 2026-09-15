# Decodium 4 FT2 1.0.345

Release di stabilizzazione pubblicata dopo la 1.0.344. Questa build include le modifiche locali fatte dopo l'allineamento con Martino e porta il ramo del fork alla versione `1.0.345`.

## Cambiamenti principali dalla 1.0.344

### Font Windows e DirectWrite

- Decodium imposta ora `Segoe UI` come font UI esplicito su Windows nel percorso Qt Widgets.
- Lo stesso font viene applicato anche al percorso QML durante lo startup.
- `BootLoader.qml` e `Main.qml` dichiarano la famiglia `Segoe UI` su Windows, evitando fallback iniziali su famiglie storiche.
- Il bridge riconosce come alias sans-serif di piattaforma anche `MS Sans Serif`, `MS Serif`, `System` e `Small Fonts`.
- Le sostituzioni Qt coprono ora questi nomi legacy oltre a `MS Shell Dlg` e `MS Shell Dlg 2`.
- Il log filtra il warning DirectWrite noto `CreateFontFaceFromHDC()` quando e' causato da `MS Sans Serif`, cosi' non viene riportato un errore rumoroso ma non utile.

### Stato dei pannelli flottanti

- La finestra flottante Waterfall non resetta piu' `waterfallPanelVisible`, `waterfallDetached` e `waterfallMinimized` durante la chiusura generale dell'app.
- La finestra flottante Live Map non resetta piu' `liveMapPanelVisible`, `liveMapDetached` e `liveMapMinimized` durante lo shutdown.
- Le stesse finestre continuano invece a salvare correttamente lo stato quando vengono chiuse manualmente dall'utente.
- Questo evita che un semplice quit dell'applicazione venga interpretato come chiusura volontaria dei pannelli flottanti.

### Metadati release

- Versione locale allineata a `1.0.345` tramite `fork_release_version.txt`.
- Installer Inno Setup allineato a `1.0.345`.
- Installer NSIS allineato a `1.0.345`, inclusi nome output e `VIProductVersion`.
- Workflow macOS legacy allineato a `1.0.345`.
- Changelog aggiornato con il riepilogo delle modifiche rispetto alla 1.0.344.

## Impatto utente

- Su Windows la UI parte con un font piu' coerente e moderno, riducendo i fallback su font legacy.
- Il log e' meno rumoroso in presenza del warning DirectWrite legato a `MS Sans Serif`.
- Gli utenti che usano Waterfall o Live Map in finestra separata non perdono lo stato detached/minimized solo perche' hanno chiuso Decodium.

## Artefatti previsti

- Sorgenti GitHub automatici della release/tag `1.0.345`.
- Installer Microsoft Windows x64: `Decodium_1.0.345_Setup_x64.exe`.
- DMG/ZIP macOS Apple Silicon prodotti dai runner GitHub.
- DMG/ZIP macOS Intel prodotti dai runner GitHub.
- AppImage Linux x86_64 e Linux aarch64 prodotti dai runner GitHub.

## Verifica locale

Prima della pubblicazione sono stati eseguiti:

```bash
git diff --check
cmake --build build-local-macos --target decodium_qml -j2
```
