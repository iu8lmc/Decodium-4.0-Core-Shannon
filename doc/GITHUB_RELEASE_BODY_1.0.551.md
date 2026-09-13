# Decodium 4 FT2 v1.0.551

This release brings the fork fully forward from v1.0.549 and combines the
v1.0.550 feature line with a new set of radio-safety, multi-monitor, Linux and
panadapter reliability fixes.

## English (British)

### QMX SWR protection without false TX stops

- The new SWR stabilisation filter is restricted to the direct CAT fallback
  used by QRP Labs QMX when Hamlib does not expose native SWR telemetry. Native
  Hamlib metering and the protection used by every other radio are unchanged.
- SWR state and sample history are cleared on every RX-to-TX and TX-to-RX
  transition, preventing a high value retained from the previous transmission
  from blocking the next one.
- The first QMX reading at 120 ms is treated as an unsettled value and ignored.
  Subsequent readings use a moving median of three samples.
- TX is eligible for an SWR stop only when the filtered median is above the
  configured threshold and at least two post-settling readings in succession
  are also above it. An isolated CAT spike therefore no longer terminates a
  QSO, while genuinely high and persistent SWR can still stop the QMX at about
  the 700 ms sample.
- Delayed telemetry callbacks from an earlier PTT state are cancelled. Invalid
  CAT replies break the high-SWR sequence rather than counting as confirmation.
- `[QMX-SWR]` diagnostics now record raw, filtered and published values, sample
  count, consecutive high readings, transition timing and the exact decision
  reason.

### Settings window and multi-monitor behaviour

- All fourteen Settings pages now share an explicit two-axis scrolling surface
  with reliable `contentWidth` and `contentHeight` calculation.
- Four-column forms switch to two columns when the available width is limited.
  Long translated labels, high-DPI controls and narrow 4:3 displays remain
  reachable through real horizontal and vertical scroll bars instead of being
  clipped outside the window.
- Moving Settings between monitors recalculates the usable screen geometry and
  clamps the window to the destination display. The calculation is repeated
  after the native window is mapped, covering window managers that update the
  screen assignment asynchronously.
- Settings is now a transient dialog of the Decodium main window: it remains
  above Decodium without becoming globally always-on-top over unrelated desktop
  applications.

### Panadapter, 3D rendering and GPU recovery

- The GPU-native 3D panadapter introduced in v1.0.550 keeps spectral history and
  rendering on the graphics processor, with ordered trace drawing on Metal and
  OpenGL. The normal 2D GPU-direct route remains unchanged when 3D is disabled,
  and the asynchronous CPU path remains available as fallback.
- Software scene-graph fallback now rasterises the complete 3D history into the
  spectrum image, so 3D remains visible when hardware scene-graph rendering is
  unavailable.
- CPU fallback releases stale direct-GPU textures and rejects late asynchronous
  GPU results from an older generation, preventing old frames from reappearing
  after a fallback.
- A GPU path paused by the main-thread stall guard can now perform bounded,
  back-off recovery probes. The retry is considered stable only after sustained
  successful operation; CPU FFT remains active until a valid GPU frame arrives.
- Linux DRM accounting recognises a process counter that has stopped advancing.
  The status bar then labels the value as estimated activity rather than showing
  a misleading real `0%` while frames are visibly being rendered.
- CPU FFT display calibration and auto-ranging were refined so that fallback
  spectrum levels remain useful and do not distort the displayed noise floor.

### Callsign lookup and Linux desktop integration

- QRZ, FCC, eQSL and Club Log buttons now use the callsign currently typed in
  the lookup field, not a stale value left by the previous query.
- Explicit provider-button clicks remain available in offline mode because they
  hand a URL to the user's browser rather than performing a background Decodium
  network request. Callsigns and generated URLs are validated before opening.
- AppImage browser launches use a host-clean environment and prefer the system
  `xdg-open` or `gio`, avoiding bundled GLib/GIO and linker paths that can prevent
  the desktop browser from opening.
- The same centralised Linux host-environment sanitation is used by every
  `secret-tool` read, store and clear operation. DBus, runtime-directory,
  display and Wayland variables required to reach the user's keyring are kept.

### Worked-before visibility

- CQ-only mode now preserves worked CQ rows so their B4 colour and optional
  strike-through can be rendered. Explicit “Hide worked” filters keep their
  established behaviour outside CQ-only presentation mode.
- The same policy is applied to both live insertion and asynchronous decode-list
  rebuilds, preventing rows from disappearing only after a refresh.

### Included from v1.0.550

- **Open a second Decodium** creates an isolated profile and can connect the new
  instance to the first instance's shared CAT server without taking the same
  serial port. `--rig-name` no longer overwrites the root profile selection.
- Linux gained optional OpenGL visual-FFT selection, hybrid-GPU eligibility
  checks and improved Intel i915/Xe process accounting.
- AppImage keyring calls were isolated from bundled libraries, the aarch64
  package retained its AppRun wrapper, B4 display was aligned between Full
  Spectrum and Signal RX, and RTL-SDR checkboxes gained clearer contrast.
- The GPU-control and second-instance strings were added to all fifteen
  translation catalogues with no unfinished entries.

### Compatibility and validation

- Linux-only environment and DRM handling is compile-time/platform gated.
- The QMX filter is gated by the exact QMX raw-SWR fallback capability; Windows,
  macOS, TCI, CAT4OM, OmniRig and other Hamlib radios retain their existing
  protection paths.
- The complete local suite passes: 36/36 tests, including QMX telemetry,
  secure settings, callsign intelligence, worked-before filtering, Linux DRM,
  CAT, audio, DSP, satellites, FT2-Link and RTL-SDR. The complete Decodium QML
  application also builds successfully on macOS. A real QMX hardware retest is
  recommended for final on-air confirmation of the new settling behaviour.

---

## Italiano

Questa release porta il fork completamente avanti dalla v1.0.549 e unisce le
funzioni della v1.0.550 a nuove correzioni per sicurezza radio, monitor multipli,
Linux e affidabilità del panadapter.

### Protezione SWR QMX senza falsi arresti della TX

- Il nuovo filtro di stabilizzazione SWR è limitato al fallback CAT diretto
  usato dal QRP Labs QMX quando Hamlib non espone la telemetria SWR nativa. La
  lettura Hamlib nativa e la protezione di tutte le altre radio restano immutate.
- Valore SWR e cronologia dei campioni vengono azzerati a ogni passaggio RX→TX e
  TX→RX, impedendo che un valore alto della trasmissione precedente blocchi
  quella successiva.
- La prima lettura QMX a 120 ms viene considerata non assestata e ignorata. Le
  letture successive usano una mediana mobile di tre campioni.
- La TX può essere interrotta per SWR soltanto quando la mediana filtrata supera
  la soglia configurata e almeno due letture post-assestamento consecutive sono
  anch'esse sopra soglia. Un singolo picco CAT non interrompe più il QSO, mentre
  un SWR realmente alto e persistente può ancora fermare il QMX intorno al
  campione dei 700 ms.
- Le letture ritardate appartenenti a uno stato PTT precedente vengono annullate.
  Una risposta CAT non valida interrompe la sequenza alta invece di confermarla.
- Le righe diagnostiche `[QMX-SWR]` riportano valori grezzi, filtrati e
  pubblicati, numero dei campioni, letture alte consecutive, tempi di transizione
  e motivo esatto della decisione.

### Finestra Impostazioni e monitor multipli

- Tutte le quattordici pagine delle Impostazioni condividono ora una superficie
  di scorrimento su entrambi gli assi, con calcolo esplicito di `contentWidth` e
  `contentHeight`.
- I moduli a quattro colonne passano a due colonne quando la larghezza è
  limitata. Etichette tradotte lunghe, controlli High-DPI e vecchi monitor 4:3
  restano raggiungibili tramite vere barre di scorrimento orizzontale e verticale.
- Spostando Impostazioni fra monitor viene ricalcolata l'area utile dello schermo
  di destinazione e la finestra viene ricondotta al suo interno. Il controllo si
  ripete dopo la creazione della finestra nativa per i window manager che
  aggiornano lo schermo in modo asincrono.
- Impostazioni è ora una finestra transiente della finestra principale: rimane
  davanti a Decodium senza coprire permanentemente le altre applicazioni desktop.

### Panadapter, rendering 3D e recupero GPU

- Il panadapter 3D nativo GPU introdotto nella v1.0.550 conserva cronologia e
  disegno dello spettro sulla scheda grafica, con tracce ordinate su Metal e
  OpenGL. Il percorso GPU-direct 2D resta invariato a 3D spento e il percorso
  CPU asincrono rimane disponibile come fallback.
- Il fallback del scene graph software rasterizza ora l'intera cronologia 3D
  nell'immagine dello spettro, mantenendo il 3D visibile anche senza rendering
  hardware del scene graph.
- Il passaggio alla CPU libera le texture GPU-direct non più valide e ignora i
  risultati GPU asincroni appartenenti a una generazione precedente, evitando
  che vecchi fotogrammi ricompaiano dopo il fallback.
- Un percorso GPU sospeso dal controllo anti-stallo può effettuare tentativi di
  recupero limitati e con attesa progressiva. La CPU resta attiva finché non
  arriva un fotogramma GPU valido e il recupero viene considerato stabile solo
  dopo un periodo continuativo senza errori.
- Su Linux viene riconosciuto un contatore DRM di processo che non avanza più.
  La barra di stato mostra allora un'attività stimata invece di un falso `0%`
  mentre la GPU sta visibilmente disegnando fotogrammi.
- Calibrazione e auto-range della FFT CPU sono stati affinati per mantenere
  leggibili i livelli dello spettro senza alterare il fondo rumore visualizzato.

### Lookup nominativi e integrazione desktop Linux

- I pulsanti QRZ, FCC, eQSL e Club Log usano ora il nominativo presente nel campo
  di ricerca, non un valore rimasto dalla richiesta precedente.
- Un clic esplicito sui provider resta disponibile anche in modalità offline:
  viene affidato un URL al browser dell'utente e non viene eseguita una richiesta
  di rete in background da Decodium. Nominativi e URL vengono validati.
- Nelle AppImage il browser viene aperto con un ambiente host pulito, preferendo
  `xdg-open` o `gio` di sistema ed evitando percorsi GLib/GIO e linker inclusi
  nel pacchetto che potrebbero impedire l'apertura del browser desktop.
- La stessa pulizia centralizzata dell'ambiente Linux viene usata per lettura,
  salvataggio e cancellazione tramite `secret-tool`, conservando DBus, directory
  runtime, DISPLAY e Wayland necessari per raggiungere il portachiavi utente.

### Visibilità dei già lavorati

- La modalità Solo CQ conserva le righe CQ già lavorate, così colore B4 e
  barratura opzionale possono essere disegnati. I filtri espliciti “Nascondi
  lavorati” mantengono il comportamento precedente fuori dalla modalità Solo CQ.
- La stessa regola viene applicata sia all'inserimento live sia alla ricostruzione
  asincrona delle liste, evitando che una riga sparisca soltanto dopo il refresh.

### Incluso dalla v1.0.550

- **Apri un secondo Decodium** crea un profilo isolato e può collegare la nuova
  istanza al server CAT condiviso della prima senza occupare la stessa porta
  seriale. `--rig-name` non sovrascrive più la selezione del profilo principale.
- Linux ha ricevuto la selezione opzionale della FFT visuale OpenGL, controlli
  per GPU ibride e un conteggio di processo Intel i915/Xe migliorato.
- Le chiamate al portachiavi delle AppImage sono state isolate dalle librerie
  incluse, il pacchetto aarch64 conserva il proprio AppRun, la visualizzazione B4
  è stata allineata fra Full Spectrum e Signal RX e le caselle RTL-SDR sono più
  leggibili.
- Le stringhe per controllo GPU e seconda istanza sono state aggiunte a tutti i
  quindici cataloghi di traduzione senza messaggi incompleti.

### Compatibilità e verifica

- La gestione ambiente e DRM specifica di Linux è protetta a compile-time e per
  piattaforma.
- Il filtro QMX è attivo soltanto con l'esatto fallback SWR grezzo del QMX;
  Windows, macOS, TCI, CAT4OM, OmniRig e le altre radio Hamlib conservano i
  percorsi di protezione esistenti.
- Passa l'intera suite locale: 36 test su 36, compresi telemetria QMX,
  impostazioni sicure, intelligence dei nominativi, filtri già lavorati, DRM
  Linux, CAT, audio, DSP, satelliti, FT2-Link e RTL-SDR. Anche l'applicazione
  QML completa di Decodium viene compilata correttamente su macOS. È consigliato
  un test finale con QMX reale per confermare in aria il nuovo assestamento.
