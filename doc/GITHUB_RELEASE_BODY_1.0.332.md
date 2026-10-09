# Decodium 4 FT2 1.0.332

Release focalizzata sulla stabilita' macOS Apple Silicon, sulla riduzione degli stalli audio/UI e sulla robustezza dei percorsi GPU di panadapter, waterfall e LiveMap.

## Cambiamenti principali dalla 1.0.331

- Ridotti gli stalli del main thread causati dall'enumerazione ripetuta dei dispositivi audio Qt. Decodium ora mantiene una cache dei dispositivi input/output, aggiornata in modo debounced quando macOS o Qt notificano modifiche reali.
- Spostata la gestione dello stream RX `SoundInput` sul thread proprietario, con dispatch sicuro per start, stop, suspend, resume, reset e gain. Questo evita lavoro CoreAudio/Qt Multimedia sul thread UI durante avvio, wake e cambi device.
- Rafforzata la riproduzione TX su macOS CoreAudio. Lo stream di uscita viene riusato quando possibile, resta caldo e silenziato tra un TX e il successivo, e non passa piu' da callback Qt/CoreAudio ritardate durante la distruzione.
- Parcheggiati in modo sicuro i sink CoreAudio raw usati dal bridge TX, evitando chiamate `stop()`, query di stato o delete ritardati che potevano riattivare listener CoreAudio gia' invalidi.
- Corretta la causa piu' probabile dei crash `QSGSimpleTextureNode::setTexture(nullptr)` nella LiveMap: il layer mappa usa ora una texture fallback 1x1 quando la texture reale non e' ancora disponibile.
- Rafforzato il percorso GPU del panadapter/waterfall: le texture QRhi dirette vengono ritirate in modo differito, le risorse GPU vengono rilasciate nello stage corretto del render thread e gli shader ricevono una texture fallback valida se una texture reale non e' pronta.
- Migliorata la leggibilita' dell'overlay panadapter con testo piu' netto, outline scuro e campionamento nearest per le texture overlay.
- Reso piu' prudente il detach della finestra waterfall: lo show/raise/activate viene differito al giro Qt successivo per ridurre le collisioni con la sincronizzazione QSG.
- Ripristinata la visibilita' e l'interazione delle label decode native sul waterfall quando il percorso overlay C++/GPU e' attivo, con hit area cliccabile verso lo spot DX.
- Aggiunta strumentazione piu' leggibile per analizzare stalli main-thread, fasi QSG e timeline TX/audio (`[AUDIO-TL]`, `[TX-TL]`, `[MAIN-TL]`).
- Il PTT Hamlib legacy resta su percorso asincrono, riducendo blocchi percepibili durante la transizione TX/RX.

## Stabilita' macOS

- Mitigato il crash CoreAudio visto in `AudioObjectRemovePropertyListenerBlock` / `QCoreAudioSinkStream::stopAudioUnit()` dopo fine TX o cambio stato audio.
- Mitigato il crash QSGRenderThread visto in `WorldMapGpuItem::updatePaintNode()` quando il nodo texture mappa veniva aggiornato senza texture valida.
- Ridotta la pressione del thread UI durante resume/wake e durante refresh dei dispositivi audio.

## Packaging e release

- Versione locale allineata a `1.0.332`.
- Installer Windows Inno Setup e script NSIS allineati a `1.0.332`.
- Workflow macOS legacy allineato a `1.0.332`.
- La release pubblica include sorgenti GitHub automatici, installer Windows x64, DMG/ZIP macOS Apple Silicon e AppImage Linux x86_64/aarch64 prodotti dai runner GitHub.

## Verifica locale

Build macOS locale verificata con:

```bash
cmake --build build-local-macos --target decodium_app -j 8
```
