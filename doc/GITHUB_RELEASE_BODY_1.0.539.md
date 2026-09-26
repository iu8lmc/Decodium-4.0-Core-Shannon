# Decodium 4.0 v1.0.539

Version 1.0.539 repairs a startup failure that could leave the application
permanently unable to open. An operator reported the same message on three
different laptops: **Failed to initialize graphics backend for OpenGL**.

## English (British)

### Slow-PC mode could make Decodium unstartable

- Five seconds after the first run, Decodium offers Slow-PC mode, describing it
  as a switch to OpenGL graphics, stable on older video cards. Accepting it
  stores `LowEndMode`, and from the next run onwards the OpenGL backend is
  forced before the application object exists.
- On a laptop whose driver does not expose a usable OpenGL implementation, Qt
  aborts with `Failed to initialize graphics backend for OpenGL.` The setting is
  persisted, so every subsequent run failed the same way, and the operator could
  not reach Settings to turn the mode off. The application was effectively
  bricked by accepting a well-meant dialogue.
- Worse, the two recovery paths were switched off precisely in this case. The
  OpenGL choice was written into `DECODIUM_GRAPHICS_BACKEND` by Decodium itself,
  and the code then treated a non-empty value as an explicit operator choice,
  which disabled both the automatic Direct3D 11 fallback and the safe-graphics
  path.

### What changed

- Slow-PC mode no longer re-forces OpenGL when the previous run left the
  graphics startup marker behind, which is the sign that it never reached the
  point of drawing anything. The automatic fallback decides instead.
- OpenGL selected by Slow-PC mode is no longer counted as an explicit operator
  choice, so the Direct3D 11 fallback and safe-graphics recovery work again.
  A backend set by the operator through the environment still wins, unchanged.

### If you are affected by an older version

- Start Decodium once with `--safe-graphics`, then turn Slow-PC mode off in
  Settings and remove the switch again.

### Validation

- Local `decodium_qml` build completed successfully.
- Reproduced the reported failure path: Slow-PC mode enabled, graphics startup
  marker present, no graphics environment variables set. The log now reads
  `Modalità PC lento: OpenGL non riproposto`, the automatic fallback selects
  `QSG_RHI_BACKEND=d3d11`, and the application starts and renders on
  Direct3D 11. Before the change it insisted on OpenGL.

## Italiano

La versione 1.0.539 ripara un guasto all'avvio che poteva lasciare
l'applicazione definitivamente incapace di aprirsi. Un operatore ha riportato
lo stesso messaggio su tre portatili diversi: **Failed to initialize graphics
backend for OpenGL**.

### La Modalità PC lento poteva murare Decodium

- Cinque secondi dopo il primo avvio Decodium propone la Modalità PC lento,
  descrivendola come un passaggio alla grafica OpenGL, stabile sulle schede
  video più vecchie. Accettandola viene salvato `LowEndMode`, e dall'avvio
  successivo il backend OpenGL viene forzato prima ancora che esista l'oggetto
  applicazione.
- Su un portatile il cui driver non espone un OpenGL utilizzabile, Qt si arrende
  con `Failed to initialize graphics backend for OpenGL.` L'impostazione è
  salvata, quindi ogni avvio successivo falliva allo stesso modo e l'operatore
  non poteva raggiungere le impostazioni per disattivare la modalità.
  L'applicazione restava murata per aver accettato una proposta ben intenzionata.
- Peggio: proprio in questo caso le due vie di recupero risultavano spente. La
  scelta di OpenGL veniva scritta in `DECODIUM_GRAPHICS_BACKEND` da Decodium
  stesso, e il codice trattava poi qualunque valore non vuoto come una scelta
  esplicita dell'operatore, disattivando sia il ripiego automatico su
  Direct3D 11 sia la grafica di sicurezza.

### Cosa cambia

- La Modalità PC lento non ripropone OpenGL quando l'avvio precedente ha
  lasciato il marcatore grafico sul disco, il segno che non è mai arrivato a
  disegnare nulla. A quel punto decide il ripiego automatico.
- OpenGL scelto dalla Modalità PC lento non conta più come scelta esplicita
  dell'operatore, quindi il ripiego su Direct3D 11 e la grafica di sicurezza
  tornano a funzionare. Un backend impostato dall'operatore tramite le variabili
  d'ambiente continua a prevalere, come prima.

### Se sei bloccato con una versione precedente

- Avvia Decodium una volta con `--safe-graphics`, poi disattiva la Modalità PC
  lento dalle impostazioni e togli di nuovo l'opzione.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- Riprodotto il guasto segnalato: Modalità PC lento attiva, marcatore di avvio
  grafico presente, nessuna variabile d'ambiente grafica impostata. Il registro
  ora riporta `Modalità PC lento: OpenGL non riproposto`, il ripiego automatico
  sceglie `QSG_RHI_BACKEND=d3d11` e l'applicazione si avvia e disegna su
  Direct3D 11. Prima della modifica insisteva con OpenGL.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
