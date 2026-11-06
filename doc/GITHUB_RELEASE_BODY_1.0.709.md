# Decodium 4 v1.0.709

## English (British)

This release consolidates the work delivered in v1.0.706, v1.0.707 and v1.0.708,
and adds the CAT/rigctl, CW/SSB lifecycle, graphics-cache and Liquid Glass fixes
developed after v1.0.708.

### v1.0.706 — optional best-caller-first scoring

- Added the **Best caller first** option for FT8, FT4 and FT2. It is disabled by
  default, so the previous queue behaviour remains unchanged unless the operator
  enables it.
- The score considers signal strength, new DXCC status, whether the caller has
  already been worked on the current band or elsewhere, and waiting time.
- Manual queue choices and double-clicked callers remain pinned ahead of automatic
  scoring. The decode list itself is not reordered; only the caller selected by
  the sequencer is affected.
- Queue reordering is written to the diagnostic log with the reason and scores.

### v1.0.707 — faster FT2, smaller Windows package and Light theme

- Optimised the FT2 synchronisation and Stage 7 path. The measured test workload
  reduced average decode cost from 687 ms to 214 ms and the first-decode delay
  from 1.3 s to 0.4 s, while retaining the same synthetic and recorded decode
  results.
- Reduced the Windows package by removing unused runtime libraries and added a
  bundle DLL audit. Windows 10 version 1809 or later, 64-bit, is required by the
  Qt 6.11 build.
- Added the flat **Light** theme. It uses solid surfaces and automatically avoids
  the effects disabled by Low graphics quality while preserving the user's saved
  quality setting for other themes.
- The first-run Slow-PC offer is now shown only when the machine appears modest or
  a previous start was slow. The setting remains available to everyone.
- Added the diagnostic-panel action to the bug-report window.

### v1.0.708 — less passive map work and completed translations

- New decodes and PSK spots no longer force a full Map Intelligence database
  refresh on every event. Passive refreshes are spaced by at least 2.5 seconds;
  explicit filter, layer and import changes remain immediate.
- Completed translations for the Slow-PC wording and the diagnostic-panel action
  in all 15 interface languages.
- Removed four unused QML components: `AstroWindowContent`, `TxRow`, `BandScale`
  and `MessageAvgPanel`.

### v1.0.709 — CAT/rigctl and radio-control reliability

- Dashboard radio controls now use the capabilities discovered after a CAT or
  NET rigctl session becomes online. ATU and power controls therefore remain
  available when DecoDXLog/rigctld exposes them through a shared connection.
- **Power On** over a live network rigctl session no longer closes and reopens the
  CAT client. This removes the extra click, avoids replaying cached state and
  prevents an unnecessary transient VFO/band change. Direct serial and USB CAT
  retain their dedicated wake-up path.
- NET rigctl startup now adopts the server's current radio state before Decodium
  writes mode, VFO, split or TX-frequency values. Idle startup writes are skipped
  and mode synchronisation is deferred until the initial state is known, reducing
  relay clicks and unwanted frequency changes when DecoDXLog owns the radio.
- CAT updates now carry their worker sequence. A state report already in transit
  cannot overwrite a newer operator request, preventing an old CW snapshot from
  changing a newly selected DATA/USB mode.

### v1.0.709 — CW and SSB window lifecycle

- Closing the CW or SSB pop-out now stops its activity and requests destruction of
  the owning asynchronous Loader. Closing the window no longer merely hides a
  large decoder/audio scene graph in the background.
- CW refreshes its CAT transmit state when opened, stops an active send on close,
  and keeps the emergency-stop path intact.
- SSB stops voice-keyer and assistant activity on close. USB, PipeWire, PulseAudio
  and WASAPI device-list refreshes are treated as potentially transient; the
  controller performs one bounded microphone reopen before releasing PTT on a real
  failure.
- Application shutdown now releases loaded radio windows before quitting, allowing
  Qt scene graphs to be torn down cleanly on Linux/XFCE and similar compositors.

### v1.0.709 — graphics recovery and Liquid Glass readability

- Replaced the directly-written graphics pipeline cache with a versioned,
  per-application/per-role/per-backend transactional cache. Caches are written to
  staging files and promoted atomically only after a clean shutdown.
- An interrupted previous session now discards stale or incomplete pipeline-cache
  files automatically. `--disable-gpu` and `--software-renderer` select the
  cross-platform software renderer on non-Windows systems as well.
- Improved the Liquid Glass hamburger menu, language submenu and clock menu. They
  retain translucency but now have a minimum 88% surface opacity, so waterfall
  labels and decode rows cannot compete with the menu text. Unsupported or
  disabled transparency still uses an opaque fallback.

### Tests and verification

- Added or extended regression coverage for CAT capability discovery and Power On,
  shared NET rigctl read-only startup, CAT sequence ordering, CW/SSB window release,
  SSB microphone recovery, graphics-cache recovery, Liquid Glass contrast and TX
  dispatch guards.
- The focused local suite passed: `test_ssb`, `test_graphics_pipeline_cache`,
  `test_liquid_glass_windows`, `test_glass_theme_switch`, `test_cw_window`,
  `test_ssb_window_focus`, `test_tx_dispatch_guard` and
  `test_rigctl_dashboard_controls`.

### Release contents and limits

- The release is based on Decodium 4 v1.0.705 and includes the cumulative changes
  from v1.0.706, v1.0.707, v1.0.708 and v1.0.709.
- Windows, macOS Apple Silicon, macOS Intel and Linux x86_64/aarch64 packages are
  produced by the GitHub Actions release runners.
- Local tests do not replace a real-radio test. CAT relay behaviour, PTT safety and
  the final visual appearance of Liquid Glass should still be confirmed on the
  operator's own radio, compositor and display scaling.

## Italiano

Questa release riunisce il lavoro delle versioni 1.0.706, 1.0.707 e 1.0.708 e
aggiunge le correzioni CAT/rigctl, del ciclo di vita delle finestre CW/SSB, della
cache grafica e del tema Liquid Glass sviluppate dopo la v1.0.708.

### v1.0.706 — punteggio opzionale per scegliere prima il miglior chiamante

- Aggiunta l'opzione **Best caller first** per FT8, FT4 e FT2. È disattivata di
  default, quindi il comportamento precedente della coda resta invariato finché
  l'operatore non la abilita.
- Il punteggio considera intensità del segnale, nuovo DXCC, contatti già effettuati
  sulla banda corrente o su altre bande e tempo di attesa.
- Le scelte manuali della coda e i chiamanti aggiunti con doppio clic restano
  prioritari rispetto al punteggio automatico. La lista delle decodifiche non viene
  riordinata: cambia soltanto il chiamante scelto dal sequencer.
- Ogni riordino viene scritto nel log diagnostico con motivo e punteggi.

### v1.0.707 — FT2 più veloce, pacchetto Windows più piccolo e tema Light

- Ottimizzati la sincronizzazione FT2 e il percorso Stage 7. Nel carico di prova il
  costo medio è sceso da 687 ms a 214 ms e il ritardo della prima decodifica da
  1,3 s a 0,4 s, mantenendo gli stessi risultati sulle scene sintetiche e sulle
  registrazioni verificate.
- Ridotto il pacchetto Windows eliminando librerie runtime inutilizzate e aggiunto
  un controllo delle DLL incluse. La build Qt 6.11 richiede Windows 10 versione
  1809 o successiva, a 64 bit.
- Aggiunto il tema piatto **Light**, con superfici piene. Disattiva anche gli
  effetti esclusi dalla qualità grafica Low e conserva la qualità scelta per gli
  altri temi.
- La proposta iniziale della modalità PC lento appare solo quando il computer
  sembra modesto o un avvio precedente è stato lento; l'impostazione resta
  disponibile per tutti.
- Aggiunta alla finestra di segnalazione problemi l'azione per aprire il pannello
  diagnostico.

### v1.0.708 — meno lavoro passivo della mappa e traduzioni completate

- Le nuove decodifiche e gli spot PSK non forzano più un aggiornamento completo del
  database Map Intelligence a ogni evento. Gli aggiornamenti passivi sono distanziati
  di almeno 2,5 secondi; filtri, livelli e importazioni espliciti restano immediati.
- Completate in tutte le 15 lingue dell'interfaccia le traduzioni della modalità PC
  lento e dell'azione per il pannello diagnostico.
- Rimossi quattro componenti QML inutilizzati: `AstroWindowContent`, `TxRow`,
  `BandScale` e `MessageAvgPanel`.

### v1.0.709 — affidabilità CAT/rigctl e comandi radio

- I controlli radio della dashboard usano ora le capacità scoperte quando la
  sessione CAT o NET rigctl diventa realmente online. ATU e alimentazione restano
  quindi disponibili quando DecoDXLog/rigctld li espone su una connessione condivisa.
- **Power On** su una sessione NET rigctl già attiva non chiude più e non riapre il
  client CAT. Questo elimina il doppio clic, evita il replay dello stato memorizzato
  e impedisce il cambio transitorio di VFO/banda. Il percorso dedicato per CAT
  seriale e USB resta invariato.
- All'avvio NET rigctl Decodium adotta prima lo stato corrente del server, poi
  decide se scrivere modo, VFO, split o frequenza TX. Le scritture inattese vengono
  saltate e la sincronizzazione del modo viene rimandata finché lo stato iniziale non
  è noto, riducendo ticchettii dei relè e cambi di frequenza quando la radio è gestita
  da DecoDXLog.
- Gli aggiornamenti CAT ora hanno la sequenza del worker. Un vecchio stato già in
  transito non può sovrascrivere una richiesta più recente dell'operatore, evitando
  che un'istantanea CW riporti la radio dal nuovo modo DATA/USB al modo precedente.

### v1.0.709 — ciclo di vita delle finestre CW e SSB

- La chiusura della finestra CW o SSB arresta l'attività e richiede la distruzione
  del Loader asincrono proprietario. La finestra non si limita più a nascondere in
  background un grande scene graph con decoder e audio ancora attivi.
- CW aggiorna lo stato CAT di trasmissione all'apertura, arresta un invio attivo alla
  chiusura e conserva il percorso di arresto di emergenza.
- SSB arresta voice keyer e assistente alla chiusura. Gli aggiornamenti degli
  endpoint USB, PipeWire, PulseAudio e WASAPI sono trattati come potenzialmente
  temporanei: il controller prova una sola riapertura controllata del microfono e
  rilascia il PTT in caso di guasto reale.
- Alla chiusura dell'applicazione le finestre radio caricate vengono rilasciate prima
  di uscire, permettendo a Qt di smontare correttamente i scene graph su Linux/XFCE
  e compositori simili.

### v1.0.709 — recupero grafico e leggibilità Liquid Glass

- Sostituita la cache grafica scritta direttamente con una cache transazionale
  versionata, separata per applicazione, ruolo, backend e versione Qt. I dati vengono
  scritti in file temporanei e promossi atomicamente solo dopo una chiusura pulita.
- Dopo un'interruzione la sessione successiva elimina automaticamente cache vecchie o
  incomplete. `--disable-gpu` e `--software-renderer` selezionano il renderer software
  multipiattaforma anche sui sistemi non Windows.
- Migliorati menu hamburger, sottomenu della lingua e menu dell'orologio in Liquid
  Glass. Restano traslucidi, ma hanno ora un'opacità minima dell'88%, così le scritte
  del waterfall e delle decodifiche non competono con le voci del menu. Quando la
  trasparenza non è disponibile viene usato il fallback opaco.

### Test e verifiche

- Aggiunta o estesa la copertura per scoperta capacità CAT e Power On, avvio NET
  rigctl in sola adozione dello stato, ordinamento delle sequenze CAT, rilascio delle
  finestre CW/SSB, recupero del microfono SSB, recupero della cache grafica,
  contrasto Liquid Glass e protezioni del dispatch TX.
- La suite locale mirata ha superato: `test_ssb`, `test_graphics_pipeline_cache`,
  `test_liquid_glass_windows`, `test_glass_theme_switch`, `test_cw_window`,
  `test_ssb_window_focus`, `test_tx_dispatch_guard` e
  `test_rigctl_dashboard_controls`.

### Contenuto e limiti della release

- La release parte dalla Decodium 4 v1.0.705 e comprende cumulativamente le modifiche
  delle versioni 1.0.706, 1.0.707, 1.0.708 e 1.0.709.
- I pacchetti Windows, macOS Apple Silicon, macOS Intel e Linux x86_64/aarch64 sono
  prodotti dai runner GitHub Actions della release.
- I test locali non sostituiscono una prova con la radio reale. Il comportamento dei
  relè CAT, la sicurezza PTT e l'aspetto finale di Liquid Glass devono comunque essere
  verificati sulla radio, sul compositore e sul ridimensionamento dello schermo
  dell'operatore.
