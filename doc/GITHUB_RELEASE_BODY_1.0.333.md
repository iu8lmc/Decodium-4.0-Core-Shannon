# Decodium 4 FT2 1.0.333

Release di stabilizzazione UI pubblicata per evitare il blocco osservato riaprendo Decodium dopo l'uso della modalita' DX-Pedition e per rendere `Ocean Blue` il profilo di avvio sicuro.

## Cambiamenti principali dalla 1.0.332

- Avvio forzato su `Ocean Blue`: il theme manager imposta sempre `Ocean Blue` come tema runtime allo startup e corregge la preferenza `theme/current` se era rimasta su `DX-Pedition` o su un valore non desiderato.
- Restore automatico del workspace DX-Pedition disattivato: `Main.qml` non ricarica piu' il layout tattico a tre colonne durante `Component.onCompleted`; se trova `uiDxPeditionMode=true`, lo riscrive a `false` prima di proseguire.
- `DX-Pedition` rimosso solo dall'elenco temi visibile nelle impostazioni. La palette interna e il codice di supporto restano presenti, cosi' non vengono rotti binding o percorsi esistenti.
- Preferenze locali di test riallineate a `theme.current=Ocean Blue` e `uiDxPeditionMode=false`, in modo che il prossimo avvio usi layout classico e tema scuro stabile.
- Versione locale allineata a `1.0.333` tramite `fork_release_version.txt`.
- Installer Windows Inno Setup e script NSIS allineati a `1.0.333`, inclusi nome output e `VIProductVersion` NSIS.
- Workflow macOS legacy allineato a `1.0.333`.

## Impatto utente

- Decodium non rientra piu' automaticamente nella modalita' DX-Pedition al riavvio.
- Nelle impostazioni restano selezionabili solo `Ocean Blue` e `Stellar Light`.
- Il tema DX-Pedition non e' stato cancellato dal codice: e' solo nascosto dall'elenco e neutralizzato allo startup per questa release di test/stabilizzazione.

## Artefatti previsti

- Sorgenti GitHub automatici della release/tag `1.0.333`.
- Installer Microsoft Windows x64: `Decodium_1.0.333_Setup_x64.exe`.
- DMG/ZIP macOS Apple Silicon prodotti dai runner GitHub.
- AppImage Linux x86_64 e Linux aarch64 prodotti dai runner GitHub.

## Verifica locale

Build macOS locale verificata con:

```bash
cmake --build build-local-macos --target decodium -j2
```
