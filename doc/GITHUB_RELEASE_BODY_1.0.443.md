# Decodium 4 FT2 1.0.443

Release di manutenzione rispetto alla `1.0.442`, focalizzata su stabilita' UI durante sessioni FT8 lunghe, comportamento della lista decode al limite massimo, diagnostica degli stall e aggiornamenti Ft2.it Award.

## Novita' principali

- Aggiunto il contest mode `Ft2.it Award 2026`.
- Aggiunte traduzioni e documentazione per `Ft2.it Award 2026` in 13 lingue.
- Aggiornata la versione locale e i metadati installer a `1.0.443`.

## Decode UI e Full Spectrum

- Cambiato il comportamento del limite automatico decode: quando Full Spectrum raggiunge il cap non viene piu' svuotato.
- Full Spectrum ora usa una finestra mobile: elimina solo le righe piu' vecchie e mantiene visibili i decode recenti.
- Signal RX usa la stessa logica conservativa: mantiene gli ultimi decode RX e alcune righe TX locali utili alla timeline.
- Il mirror legacy ricorda le righe potate dal cap, evitando che `ALL.TXT` o il backend legacy le reinseriscano subito dopo il pruning.
- I reset manuali, cambio banda e cambio modo restano reset reali e puliscono anche la memoria delle righe potate.

## Prestazioni UI

- Ridotta la pressione sul main thread nei path di aggiornamento lista decode:
  - gli update Full Spectrum al cap usano emissioni throttle;
  - Signal RX coalesca normalizzazione e rebuild lista invece di emettere a ogni riga;
  - il mirror legacy riusa la configurazione filtri quando sincronizza le liste.
- Il BootLoader nascosto rilascia le risorse scenegraph/Metal dopo il caricamento della finestra principale, riducendo memoria e lavoro grafico residuo.
- Il watchdog UI parte solo dopo `Main.qml ready`, quindi non attribuisce piu' al primo decode gli stall dovuti al bootstrap QML.

## Diagnostica e verifiche

- Migliorate le metriche per distinguere:
  - stall reali durante l'ascolto;
  - caricamento iniziale QML;
  - pressione CPU/render durante i pass FT8 deep/full.
- Verificato che il precedente `MAINWATCH max_ms=1347` era contaminato dal bootstrap QML, non dal callback `onFt8DecodeReady`.
- Dopo la correzione, il primo decode FT8 non mostra blocchi del callback UI; restano solo piccoli spike legati al carico CPU/render dei pass FT8 a thread elevato.

## Build e release

- Predisposta la release `1.0.443` per:
  - sorgente GitHub;
  - Windows x64 installer/exe tramite GitHub Actions;
  - macOS Apple Silicon DMG;
  - macOS Intel DMG;
  - Linux AppImage x86_64;
  - Linux AppImage aarch64.

## Validazione locale

- `git diff --check`
- `cmake --build "/Users/salvo/Desktop/Decodium4-build" --parallel 4 --target decodium_qml`
- `ctest --test-dir "/Users/salvo/Desktop/Decodium4-build" -R "test_qt_helpers|test_streaming_list_model" --output-on-failure`
- Avvio runtime locale con verifica watchdog/decode su FT8.
