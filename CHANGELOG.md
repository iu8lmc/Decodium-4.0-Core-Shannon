# Changelog / Registro Modifiche

## [1.0.503] - 2026-07-27

### Repository and release infrastructure

- Moved required runtime data to `resources/runtime`, configured CMake inputs
  to `CMake/templates`, and platform metadata to `packaging`.
- Preserved historical technical references and acknowledgements under `doc`.
- Removed obsolete Qt 5, Cirrus, duplicate Linux, and hard-coded macOS
  workflows, along with unused root placeholders and the orphan `aethersdr`
  gitlink.
- Added a repository-layout validator and a single release-version resolver.
  Windows, macOS, and Linux release workflows now reject tags or manual version
  inputs that differ from `fork_release_version.txt`.
- Made required runtime data and matching release notes part of the CI
  contract, and removed the stale hard-coded Windows installer version.
- Kept CAT frequency rejection handling source-compatible with Linux
  distributions that still provide Hamlib 4.5.
- Kept installed runtime filenames unchanged so upgrades preserve application
  lookup behavior on Windows, macOS, and Linux.

### UDP reporting

- Normalized the configured WSJT-X Client ID once and serialized the same value
  on primary, secondary and tertiary UDP endpoints.
- Added a complete secondary WSJT-X protocol client for decode, status, WSPR,
  clear and optional logged-QSO traffic instead of limiting secondary output to
  selected ADIF notifications.
- Applied reporting changes immediately to the embedded legacy backend without
  restarting the application.
- Added diagnostic lines that identify the Client ID, destination, interface
  and TTL used by every UDP endpoint.
- Added loopback tests that inspect heartbeat packets from all three endpoints
  and verify live Client ID changes.

### macOS audio and panadapter

- Reduced the native AudioQueue callback quantum to approximately 20 ms by
  default while retaining four queued buffers for stability.
- Added `DECODIUM_MAC_AUDIO_QUEUE_FRAMES` as a diagnostic override.
- Changed the spectrum timer to a precise timer and allowed the accelerated
  legacy GPU path to follow the configured FPS cap.
- Kept adaptive throttling during DEEP decode and real CPU pressure, with
  conservative behavior for CPU fallback rendering.

### Runtime data and UI

- Updated source-tree fallbacks for `CALL3.TXT`, `cty.dat` and `sat.dat` to the
  maintained `resources/runtime` directory.
- Added internal padding to TCP port, frequency, offset and color-entry fields
  in Setup so numeric values no longer touch their borders.

## [1.0.351] - 2026-05-31

### Italiano

Release finale dopo la 1.0.346. Integra il lavoro locale delle serie 1.0.347/1.0.348, l'allineamento di Martino fino a `v1.0.350` e il merge locale finale sul ramo principale.

#### Aggiunto

- Supporto Linux ai path seriali stabili `/dev/serial/by-id` nelle liste CAT/PTT.
- Telemetria ALC con flag di validita' separato, cosi' `ALC --` indica dato non disponibile e `0` resta un valore valido.
- Cablaggio del controllo ZAP nel decoder Q65.
- Componenti QML `DecoComboBox` e `DecoTextField` per evitare rendering errato di emoji/simboli nei campi Material su macOS.
- Refresh automatico delle periferiche audio prima dell'auto-monitor all'avvio.
- Worked-before preciso per chiamata, banda e modo.
- Opzioni Display > Decodes per mostrare/nascondere `Dist` e `Az` in Full Spectrum e Signal RX, piu' colonna `Freq` in Signal RX.

#### Modificato

- Integrata la localizzazione italiana completa post-merge di Martino `v1.0.350`.
- Conservate le funzioni fork-only durante il merge: DX-Pedition Mode, ALC, miglioramenti FT8/FT2 e funzioni UI locali.
- Cloudlog normalizza meglio gli endpoint API e distingue errori HTTP/autenticazione/proxy con messaggi piu' chiari.
- Prompt-to-log e MessageBox hanno layout piu' robusti, senza dim Material invasivo e con pulsanti non sovrapposti.
- Check SWR non blocca piu' Tune: resta protezione su TX reale e AutoCQ.
- Il panadapter non mostra piu' i marker gialli duplicati a 500/1000/1500/2000/2500/3000 Hz.
- Aggiornati metadati locali, Inno Setup, NSIS e workflow macOS legacy alla versione `1.0.351`.

#### Corretto

- Crash Linux TX/Tune dopo errore audio (`QSocketNotifier` da thread errato e `SIGNAL 11`).
- Fallback audio indesiderato con periferiche omonime o cache Qt non aggiornata.
- Falsi worked-before tra modi o bande differenti.
- Autosequenza con CPU lente, nominativi speciali/lunghi e hash diretti al partner attivo.
- Avvio QML su macOS causato da proprieta' non supportate in `TxPanel`.
- Doppia porta CAT/PTT quando `/dev/serial/by-id/...` punta allo stesso device di `/dev/tty*`.

### English

Final release after 1.0.346. It integrates the local 1.0.347/1.0.348 work, Martino's alignment up to `v1.0.350`, and the final local merge on the main branch.

#### Added

- Linux support for stable `/dev/serial/by-id` serial paths in CAT/PTT lists.
- ALC telemetry with a separate validity flag, so `ALC --` means unavailable while `0` remains a valid value.
- ZAP wiring into the Q65 decoder.
- QML `DecoComboBox` and `DecoTextField` controls to avoid Material emoji/symbol rendering corruption on macOS.
- Automatic audio-device refresh before startup auto-monitor.
- Worked-before tracking by exact call, band, and mode.
- Display > Decodes options for `Dist` and `Az` in Full Spectrum and Signal RX, plus `Freq` in Signal RX.

#### Changed

- Integrated Martino's complete Italian localization after the `v1.0.350` merge.
- Preserved fork-only features during merge: DX-Pedition Mode, ALC, FT8/FT2 improvements, and local UI features.
- Cloudlog now normalizes API endpoints better and distinguishes HTTP/authentication/proxy failures more clearly.
- Prompt-to-log and MessageBox layouts are more robust, without invasive Material dimming and without button/text overlap.
- Check SWR no longer blocks Tune; protection remains active for real TX and AutoCQ.
- The panadapter no longer draws duplicate yellow 500/1000/1500/2000/2500/3000 Hz markers.
- Local metadata, Inno Setup, NSIS, and the legacy macOS workflow are aligned to `1.0.351`.

#### Fixed

- Linux TX/Tune crash after audio errors (`QSocketNotifier` from the wrong thread followed by `SIGNAL 11`).
- Unwanted audio fallback with same-name devices or stale Qt audio cache.
- False worked-before matches across different bands or modes.
- Autosequence handling for slow CPUs, long/special callsigns, and hashes directed to the active partner.
- macOS QML startup failure caused by unsupported `TxPanel` properties.
- Duplicate CAT/PTT ports when `/dev/serial/by-id/...` points to the same device as `/dev/tty*`.

## [1.0.348] - 2026-05-31

### Italiano

Release finale dopo la 1.0.347, centrata su stabilita' TX/Tune Linux, autosequenza, worked-before corretto per banda/modo, refresh audio all'avvio e opzioni display richieste dagli utenti.

#### Aggiunto

- Refresh automatico delle periferiche audio durante l'avvio, prima dell'auto-monitor, per evitare che RX parta con una cache audio non aggiornata.
- Opzioni in Setup > Display > Decodes per mostrare/nascondere `Dist` e `Az` in Full Spectrum e Signal RX.
- Colonna `Freq` configurabile nella finestra Signal RX.
- Riconoscimento worked-before piu' preciso per chiamata, banda e modo.
- Gestione autosequenza piu' permissiva per CPU lente nei passaggi TX1/TX2/TX3.

#### Modificato

- `Check SWR` non blocca piu' il Tune: la protezione resta attiva solo per TX reale e AutoCQ, cosi' l'utente puo' misurare e correggere SWR alto con strumenti esterni.
- Il decoder FT2 su CPU lente evita lavoro asincrono non utile durante TX, riducendo ritardi nei casi limite di slot/timing.
- Il panadapter non disegna piu' i marker gialli duplicati a 500/1000/1500/2000/2500/3000 Hz, lasciando la scala inferiore come riferimento unico.
- La risoluzione audio TX/Tune usa callback QAudioSink piu' difensivi, con guardie su oggetti e seriale playback.
- Metadati locali, Inno Setup e NSIS allineati alla versione `1.0.348`.

#### Corretto

- Fix del crash Linux dopo errori audio TX/Tune (`QSocketNotifier` da thread errato e successivo `SIGNAL 11`).
- Corretto il falso worked-before tra modi o bande diverse: un QSO in 20m FT2 non marca piu' come gia' lavorato 20m FT8 o 40m FT2.
- Migliorata l'autosequenza con nominativi lunghi/speciali e hash locali diretti al partner attivo.
- Evitati fallback silenziosi in startup monitor quando la periferica audio salvata esiste ma Qt non ha ancora completato l'enumerazione.

### English

Final release after 1.0.347, focused on Linux TX/Tune stability, autosequence reliability, exact band/mode worked-before checks, startup audio refresh, and requested decode display options.

#### Added

- Automatic audio-device refresh during startup before auto-monitor starts, avoiding RX startup with stale audio cache.
- Setup > Display > Decodes options to show/hide `Dist` and `Az` in Full Spectrum and Signal RX.
- Configurable `Freq` column in Signal RX.
- More precise worked-before tracking by call, band, and mode.
- More tolerant autosequence retry budget for slow CPUs during TX1/TX2/TX3 transitions.

#### Changed

- `Check SWR` no longer blocks Tune: protection remains active only for real TX and AutoCQ, so users can measure high SWR and correct it externally.
- FT2 decoding on slow CPUs avoids non-useful async decode work during TX, reducing slot/timing edge cases.
- The panadapter no longer draws duplicate yellow 500/1000/1500/2000/2500/3000 Hz labels, leaving the bottom scale as the single reference.
- TX/Tune audio handling now uses more defensive QAudioSink callbacks guarded by object lifetime and playback serial.
- Local metadata, Inno Setup, and NSIS are aligned to `1.0.348`.

#### Fixed

- Fixed the Linux crash after TX/Tune audio errors (`QSocketNotifier` from the wrong thread followed by `SIGNAL 11`).
- Fixed false worked-before matches across different modes or bands: 20m FT2 no longer marks 20m FT8 or 40m FT2 as worked.
- Improved autosequence handling for long/special callsigns and local hashes directed to the active partner.
- Avoided silent startup-monitor fallback when the saved audio device exists but Qt has not completed enumeration yet.

## [1.0.347] - 2026-05-31

### Italiano

Release di stabilizzazione dopo la 1.0.346, centrata su affidabilita' audio/CAT, compatibilita' Linux/macOS, pulizia della UI QML e gestione piu' robusta di Cloudlog.

#### Aggiunto

- Supporto Linux ai path seriali stabili `/dev/serial/by-id` nelle liste CAT e PTT, con confronto canonico dei symlink.
- Identita' audio piu' stabile nei percorsi di selezione periferica, con refresh esplicito della cache prima di TX/Tune.
- Telemetria ALC con validita' separata, cosi' il valore `0` non viene piu' confuso con dato assente.
- Controllo ZAP cablato nel percorso decode Q65.
- Componenti QML dedicati `DecoComboBox` e `DecoTextField` per evitare rendering errato di emoji/simboli nei campi Material su macOS.

#### Modificato

- Cloudlog normalizza meglio l'endpoint API, distingue errori HTTP/autenticazione e segnala API key valida/non valida con messaggi piu' chiari.
- La finestra "prompt to log" non usa piu' il dim Material sullo sfondo ed e' leggermente piu' alta per contenere correttamente i pulsanti.
- MessageBox e popup QML hanno dimensioni piu' conservative per evitare sovrapposizioni tra testo e pulsanti.
- La status bar mostra ALC solo quando Hamlib lo considera realmente disponibile.
- Metadati locali, Inno Setup, NSIS e workflow macOS legacy allineati alla versione `1.0.347`.

#### Corretto

- Corretto il crash Linux visto dopo `Tune` quando Qt audio segnalava `IOError` e il callback tentava lo stop da thread non GUI.
- Evitato il falso uso di una porta PTT separata quando `/dev/serial/by-id/...` e `/dev/tty*` puntano allo stesso dispositivo.
- Migliorata la gestione Hamlib ALC sui backend Linux che rispondono a `RIG_LEVEL_ALC` anche se non lo dichiarano nelle capability.
- Ridotto il rischio di fallback audio verso un device omonimo o default non voluto quando piu' periferiche espongono lo stesso nome.
- Corretto l'errore QML su macOS relativo a proprieta' non supportate in `TxPanel`.

### English

Stabilization release after 1.0.346, focused on audio/CAT reliability, Linux/macOS compatibility, QML UI cleanup, and more robust Cloudlog handling.

#### Added

- Linux support for stable `/dev/serial/by-id` serial paths in CAT and PTT lists, including canonical symlink comparison.
- More stable audio identity handling in device selection paths, with explicit cache refresh before TX/Tune.
- ALC telemetry with a separate validity flag, so value `0` is no longer treated as missing data.
- ZAP wiring in the Q65 decode path.
- Dedicated `DecoComboBox` and `DecoTextField` QML controls to avoid emoji/symbol rendering corruption in Material fields on macOS.

#### Changed

- Cloudlog now normalizes API endpoints more reliably, distinguishes HTTP/authentication failures, and reports valid/invalid API keys more clearly.
- The "prompt to log" popup no longer dims the Material background and has a slightly taller layout for its buttons.
- MessageBox and QML popups use more conservative sizing to avoid text/button overlap.
- The status bar displays ALC only when Hamlib reports it as truly available.
- Local version metadata, Inno Setup, NSIS, and the legacy macOS workflow are aligned to `1.0.347`.

#### Fixed

- Fixed the Linux crash seen after `Tune` when Qt audio reported `IOError` and the stop path ran from the wrong thread.
- Avoided treating `/dev/serial/by-id/...` and `/dev/tty*` as separate PTT ports when they point to the same device.
- Improved Hamlib ALC handling on Linux backends that can answer `RIG_LEVEL_ALC` even when the capability mask does not advertise it.
- Reduced the risk of silent audio fallback to an unwanted same-name/default device.
- Fixed the macOS QML load error caused by unsupported properties in `TxPanel`.

## [1.0.345] - 2026-05-31

### Italiano

Release di stabilizzazione dopo la 1.0.344, centrata sulla resa dei font su Windows, sulla pulizia dei warning DirectWrite e sulla persistenza corretta dei pannelli flottanti durante la chiusura dell'applicazione.

#### Aggiunto

- Font UI Windows esplicito su `Segoe UI` per il percorso Qt Widgets e per il percorso QML.
- Sostituzioni Qt per famiglie legacy Windows come `MS Sans Serif`, `MS Serif`, `System` e `Small Fonts`, oltre ai gia' gestiti `MS Shell Dlg` e `MS Shell Dlg 2`.
- Impostazione font dedicata su `BootLoader.qml` e `Main.qml` per partire con la famiglia corretta anche prima del caricamento completo della UI.

#### Modificato

- Il resolver font del bridge tratta le famiglie Windows legacy come alias del font sans-serif di piattaforma.
- Il message handler ignora il warning DirectWrite noto `CreateFontFaceFromHDC()` legato a `MS Sans Serif`, evitando rumore non utile nel log.
- Alla chiusura dell'app, le finestre flottanti Waterfall e Live Map non riscrivono piu' lo stato salvato come se l'utente le avesse chiuse manualmente.
- Metadati locali, Inno Setup, NSIS e workflow macOS legacy allineati alla versione `1.0.345`.

#### Corretto

- Ridotto il rischio di fallback font errato su Windows quando Qt o QML incontrano nomi storici non piu' adatti al rendering moderno.
- Evitata la perdita indesiderata dello stato detached/minimized di Waterfall e Live Map durante lo shutdown.

### English

Stabilization release after 1.0.344, focused on Windows font rendering, DirectWrite warning cleanup, and correct floating-panel persistence during application shutdown.

#### Added

- Explicit Windows UI font selection to `Segoe UI` for both the Qt Widgets and QML startup paths.
- Qt substitutions for legacy Windows families such as `MS Sans Serif`, `MS Serif`, `System`, and `Small Fonts`, in addition to `MS Shell Dlg` and `MS Shell Dlg 2`.
- Dedicated font family assignment in `BootLoader.qml` and `Main.qml` so the correct UI font is active from early startup.

#### Changed

- The bridge font resolver now maps legacy Windows families to the platform sans-serif candidates.
- The message handler filters the known DirectWrite `CreateFontFaceFromHDC()` warning for `MS Sans Serif`.
- During application shutdown, detached Waterfall and Live Map windows no longer rewrite saved layout state as if the user had manually closed them.
- Local version metadata, Inno Setup, NSIS, and the legacy macOS workflow are aligned to `1.0.345`.

#### Fixed

- Reduced the chance of bad Windows font fallback when Qt or QML see old Windows font names.
- Prevented detached/minimized Waterfall and Live Map state from being lost during shutdown.

## [1.0.342] - 2026-05-30

### Italiano

Release di stabilizzazione per allineare il ramo locale alla 1.0.342 dopo la 1.0.341, ridurre la verbosita' dei log diagnostici e mantenere compilabile il codice anche su macOS.

#### Aggiunto

- Metriche `DECODEMETRIC` per i worker FT8, FT4 e FT2 con tempi di attesa, decode e totale, thread attivi/richiesti, dimensione audio e dettagli di profondita'.
- Hook diagnostici sul main thread per misurare le fasi di consegna decode-ready e aggiornamento modello, utili a correlare eventuali stall UI.
- Indicatori runtime aggiuntivi in status bar per monitor GPU e thread FT, coerenti con le diagnostiche gia' presenti nel backend.

#### Modificato

- Log `PANMETRIC` ridotti da intervalli brevi a circa 60 secondi per abbassare il rumore nei log ordinari.
- Log profilo `MAPGPU` ridotti a circa 60 secondi ed evitato il profilo immediato duplicato subito dopo il primo frame.
- Log `DEPTHDBG` nascosti di default e abilitabili solo impostando `DECODIUM_DEPTHDBG`.
- Metadati locali, Inno Setup, NSIS e workflow macOS legacy allineati alla versione `1.0.342`.

#### Corretto

- Corretto il build macOS con Clang: la funzione di affinita' OpenMP dei thread FT viene compilata solo su Windows quando OpenMP e' disponibile.
- Ripulite anomalie di fine riga nel worker FT2 che causavano controlli whitespace sporchi.
- Mantenuti gli aggiornamenti locali al path GPU del panadapter/waterfall e alle finestre QML senza introdurre regressioni nel build macOS.

### English

Stabilization release to align the local branch to 1.0.342 after 1.0.341, reduce diagnostic log noise, and keep the code building cleanly on macOS.

#### Added

- `DECODEMETRIC` timing for FT8, FT4, and FT2 workers, including wait/decode/total time, active/requested thread counts, audio size, and depth details.
- Main-thread diagnostic hooks for decode-ready delivery and model-update phases, useful for correlating UI stalls.
- Additional runtime status indicators for GPU monitoring and FT thread activity.

#### Changed

- `PANMETRIC` logs are throttled to roughly 60-second intervals.
- `MAPGPU` profile logs are throttled to roughly 60-second intervals and no longer emit an immediate duplicate right after the first-frame log.
- `DEPTHDBG` logs are disabled by default and can be enabled with `DECODIUM_DEPTHDBG`.
- Local version metadata, Inno Setup, NSIS installer metadata, and the legacy macOS workflow are aligned to `1.0.342`.

#### Fixed

- Fixed the macOS Clang build by compiling the FT OpenMP affinity helper only on Windows when OpenMP is available.
- Cleaned FT2 worker line-ending anomalies that made whitespace checks fail.
- Preserved the local GPU panadapter/waterfall and QML window updates while restoring a clean macOS build.

## [1.0.335] - 2026-05-30

### Italiano

Release di stabilizzazione per la selezione delle periferiche audio Qt/Windows e per l'autosequenza con nominativi speciali.

#### Aggiunto

- Persistenza degli ID stabili Qt delle periferiche audio in `audioInputDeviceId` e `audioOutputDeviceId`, oltre ai nomi visibili gia' salvati.
- Log diagnostici piu' espliciti per periferica salvata, ID salvato, periferica scelta, ID scelto, motivo del match e default disponibile.
- Log di salute audio RX subito dopo l'avvio, con RMS, picco, range e clipping, per capire rapidamente se l'audio reale arriva dal dispositivo corretto.
- Copertura test per messaggi FT speciali con nominativo locale non standard, incluso il caso `II9MESC` verso `KQ5I`.

#### Modificato

- La risoluzione audio preferisce l'ID stabile del dispositivo e usa il nome visibile solo come fallback esatto e univoco.
- Quando piu' dispositivi hanno lo stesso nome, ad esempio piu' `USB Audio CODEC`, Decodium non riscrive piu' silenziosamente la scelta salvata sul default.
- La cache TX e il riuso dello stream RX distinguono ora dispositivi con stesso nome ma ID diverso.
- Metadati locali e installer NSIS sono allineati alla versione `1.0.335`.

#### Corretto

- Corretto il caso in cui l'utente con piu' periferiche audio omonime vedeva Decodium scegliere o salvare il default sbagliato dopo l'enumerazione Qt.
- Corretta la sequenza FT per nominativo locale speciale e corrispondente standard: il report viene indirizzato al DX (`KQ5I <II9MESC> -15`) e non alla chiamata locale hashata in posizione errata, evitando che il corrispondente continui a rimandare il locator e blocchi l'autosequenza.
- Migliorata la diagnosi di fallback audio: quando un dispositivo salvato non viene trovato o non e' univoco, il log conserva nome e ID richiesti invece di nascondere il problema dietro il default.

### English

Stabilization release for Qt/Windows audio-device selection and special-callsign autosequencing.

#### Added

- Stable Qt audio device IDs are now persisted as `audioInputDeviceId` and `audioOutputDeviceId` alongside the existing visible device names.
- Clearer diagnostics for saved device, saved ID, selected device, selected ID, match reason, and available default.
- RX startup audio-health logging with RMS, peak, range, and clipping to confirm that real audio arrives from the selected device.
- Test coverage for FT special-call messages, including local non-standard call `II9MESC` with standard peer `KQ5I`.

#### Changed

- Audio resolution now prefers the stable device ID and uses the visible name only as an exact, unique fallback.
- When multiple devices share the same visible name, such as multiple `USB Audio CODEC` entries, Decodium no longer silently rewrites the saved selection to the default.
- TX audio caching and RX stream reuse now distinguish devices with the same visible name but different IDs.
- Local metadata and NSIS installer metadata are aligned to version `1.0.335`.

#### Fixed

- Fixed the case where users with multiple same-name audio devices could have Decodium select or save the wrong default device after Qt enumeration.
- Fixed FT sequencing for a non-standard local special call and a standard peer: the report is now addressed to the DX (`KQ5I <II9MESC> -15`), preventing the peer from repeatedly sending its locator and stalling autosequence.
- Improved audio fallback diagnostics by preserving requested name and ID in the log when the saved device is missing or ambiguous.

## [1.0.333] - 2026-05-30

### Italiano

Release di stabilizzazione UI per evitare il blocco osservato riaprendo Decodium in modalita' DX-Pedition e per riportare l'avvio su un profilo grafico sicuro.

#### Modificato

- L'avvio forza `Ocean Blue` come tema runtime e riscrive la preferenza `theme/current` se era rimasta su un tema diverso.
- Il workspace DX-Pedition non viene piu' ripristinato automaticamente allo startup; se `uiDxPeditionMode` era salvato attivo, viene disattivato prima di caricare il layout principale.
- `DX-Pedition` e' stato rimosso dall'elenco temi visibile nelle impostazioni, lasciando intatta la palette interna per eventuali test futuri o percorsi codice esistenti.
- Metadati locali, installer Windows Inno Setup/NSIS e workflow macOS legacy sono allineati alla versione `1.0.333`.

#### Corretto

- Evitato il rientro automatico nel layout DX-Pedition dopo un avvio precedente in quella modalita', riducendo il rischio di blocco UI allo startup.
- Riallineate le preferenze locali di test su `Ocean Blue` e `uiDxPeditionMode=false`.

### English

UI stabilization release to avoid the lock-up seen after reopening Decodium in DX-Pedition mode and to return startup to a safe graphics profile.

#### Changed

- Startup now forces `Ocean Blue` as the runtime theme and rewrites the stored `theme/current` preference when it contains a different theme.
- The DX-Pedition workspace is no longer restored automatically at startup; if `uiDxPeditionMode` was saved as enabled, it is disabled before the main layout loads.
- `DX-Pedition` was removed from the visible theme list in settings, while the internal palette remains available for future testing or existing code paths.
- Local version metadata, Windows Inno Setup/NSIS installers, and the legacy macOS workflow are aligned to `1.0.333`.

#### Fixed

- Prevented automatic re-entry into the DX-Pedition layout after a previous run in that mode, reducing startup UI lock-up risk.
- Local test preferences were realigned to `Ocean Blue` and `uiDxPeditionMode=false`.

## [1.0.332] - 2026-05-29

### Italiano

Release focalizzata sulla stabilita' macOS Apple Silicon, sulla riduzione degli stalli audio/UI e sulla robustezza dei percorsi GPU di panadapter, waterfall e LiveMap.

#### Aggiunto

- Aggiunta una cache debounced dei dispositivi audio Qt per evitare enumerazioni ripetute su startup, wake e cambio dispositivo.
- Aggiunta strumentazione piu' leggibile per timeline audio/TX, stalli main-thread e fasi QSG.
- Aggiunta una texture fallback 1x1 per i layer LiveMap quando la texture reale non e' ancora pronta.

#### Modificato

- `SoundInput` ora esegue start, stop, suspend, resume, reset e gain sul proprio thread, riducendo il lavoro CoreAudio/Qt Multimedia sul thread UI.
- Il TX audio macOS riusa il sink CoreAudio quando possibile e lo mantiene caldo/silenziato fra un TX e il successivo.
- Il percorso GPU panadapter/waterfall ritira le texture QRhi in modo differito e rilascia risorse nello stage corretto del render thread.
- Il detach della finestra waterfall viene differito al giro Qt successivo per evitare collisioni con la sincronizzazione QSG.
- Metadati locali, installer Windows e workflow macOS legacy sono allineati alla versione `1.0.332`.

#### Corretto

- Mitigato il crash CoreAudio in `AudioObjectRemovePropertyListenerBlock` / `QCoreAudioSinkStream::stopAudioUnit()` visto dopo fine TX o cambio stato audio.
- Mitigato il crash QSGRenderThread in `WorldMapGpuItem::updatePaintNode()` causato da `QSGSimpleTextureNode::setTexture(nullptr)`.
- Corretta la gestione delle label decode native sul waterfall quando l'overlay C++/GPU e' attivo, mantenendo leggibilita' e click sugli spot DX.
- Migliorata la leggibilita' dell'overlay panadapter con testo piu' netto e fallback texture sempre valido.

### English

Release focused on macOS Apple Silicon stability, lower audio/UI stalls, and stronger GPU paths for the panadapter, waterfall, and LiveMap.

#### Added

- Added a debounced Qt audio-device cache to avoid repeated device enumeration during startup, wake, and device changes.
- Added clearer timeline instrumentation for audio/TX, main-thread stalls, and QSG frame phases.
- Added a 1x1 fallback texture for LiveMap layers while the real map texture is not ready.

#### Changed

- `SoundInput` now runs start, stop, suspend, resume, reset, and gain changes on its owner thread.
- macOS TX audio now reuses the CoreAudio sink where possible and keeps it warm/muted between transmit cycles.
- The GPU panadapter/waterfall path now retires QRhi textures defensively and releases GPU resources in the render-thread stage.
- Waterfall pop-out activation is deferred to the next Qt turn to avoid QSG synchronization collisions.
- Local version metadata, Windows installers, and the legacy macOS workflow are aligned to `1.0.332`.

#### Fixed

- Mitigated the CoreAudio crash in `AudioObjectRemovePropertyListenerBlock` / `QCoreAudioSinkStream::stopAudioUnit()` after TX finish or audio state changes.
- Mitigated the QSG render-thread crash in `WorldMapGpuItem::updatePaintNode()` caused by `QSGSimpleTextureNode::setTexture(nullptr)`.
- Fixed native decode-label handling on the waterfall while the C++/GPU overlay is active, keeping labels readable and DX spot clicks available.
- Improved panadapter overlay readability and ensured sampled shader textures always have a valid fallback.

## [1.6.0] - 2026-04-03

### English

Release focused on promoting the legacy JT runtime further into native C++, fixing special/non-standard callsign reply handling, hardening GCC/Linux portability, and extending the published release set to Linux `aarch64` AppImage alongside the existing macOS and Linux `x86_64` outputs.

#### Added

- Added native C++ JT65 runtime orchestration, JT65 DSP/IO helpers, JT9 fast/wide decoder building blocks, and broader compare/regression utilities for the continuing legacy JT migration.
- Added Linux `aarch64` AppImage release support using a Debian Trixie ARM64 build path and GitHub Actions ARM runner coverage.
- Added dedicated regression coverage for non-standard/special-event callsign reply flows and additional portability checks in the compare utilities.

#### Changed

- JT65 active runtime now runs through the promoted native C++ path, and the legacy JT65 Fortran active-path sources have been removed from the active build.
- `build-arm.sh` is now version-aware, CI-friendly, and excludes `build-arm-output` from source staging; `build-arm-output/` is ignored permanently.
- Linux release engineering now publishes both `x86_64` and `aarch64` AppImages, while macOS release targets remain Tahoe arm64, Sequoia arm64, Sequoia x86_64, and Monterey x86_64 best effort.
- Local version metadata, workflow defaults, readmes, docs, release notes, package description, and GitHub release body are aligned to semantic version `1.6.0`.

#### Fixed

- Fixed replies to non-standard or special-event callsigns that were incorrectly rejected with `*** bad message ***`.
- Fixed the GCC 14 false-positive `stringop-overflow` build break in `LegacyDspIoHelpers.cpp` without regressing macOS Clang.
- Fixed GCC/libstdc++ portability failures in `jt9_wide_stage_compare.cpp` and `legacy_wsprio_compare.cpp`.
- Fixed ADIF/QRZ upload regressions caused by exporting a free-text `operator` value that did not match `station_callsign`; `operator` is now emitted only when it is a valid distinct callsign.
- Fixed decode-pane double-click behavior so `RR73`, `73`, `RRR`, `R`, `TU`, `OOO`, and Maidenhead locators no longer arm TX as if they were radio callsigns.

### Italiano

Release focalizzata nel promuovere ulteriormente il runtime JT legacy verso il C++ nativo, nel correggere la gestione delle risposte verso callsign speciali/non standard, nel rafforzare la portabilita' GCC/Linux e nell'estendere il set release pubblicato anche alla AppImage Linux `aarch64`, oltre agli output macOS e Linux `x86_64` gia' esistenti.

#### Aggiunto

- Aggiunti orchestrazione runtime JT65 nativa C++, helper DSP/IO JT65, blocchi decoder JT9 fast/wide e una copertura compare/regression piu' ampia per la migrazione JT legacy ancora in corso.
- Aggiunto supporto release Linux AppImage `aarch64` tramite build path ARM64 basato su Debian Trixie e copertura runner ARM GitHub Actions.
- Aggiunta copertura di regressione dedicata per i flussi reply verso callsign non standard/special-event e ulteriori controlli di portabilita' nelle utility compare.

#### Modificato

- Il runtime attivo JT65 gira ora sul path promosso nativo C++, e i vecchi sorgenti Fortran JT65 del path attivo sono stati rimossi dal build attivo.
- `build-arm.sh` e' ora sensibile alla versione, adatto alla CI, ed esclude `build-arm-output` dallo staging dei sorgenti; `build-arm-output/` e' ignorata in modo permanente.
- L'ingegneria release Linux pubblica ora sia AppImage `x86_64` sia `aarch64`, mentre i target macOS restano Tahoe arm64, Sequoia arm64, Sequoia x86_64 e Monterey x86_64 best effort.
- Metadati versione locali, default workflow, readme, documentazione, note release, package description e body GitHub sono allineati alla semver `1.6.0`.

#### Corretto

- Corrette le risposte verso callsign non standard o special-event che venivano rigettate con `*** bad message ***`.
- Corretto il falso positivo GCC 14 `stringop-overflow` in `LegacyDspIoHelpers.cpp` senza regressioni su macOS Clang.
- Corrette le rotture di portabilita' GCC/libstdc++ in `jt9_wide_stage_compare.cpp` e `legacy_wsprio_compare.cpp`.
- Corretta la regressione ADIF/QRZ causata dall'esportazione di un valore `operator` testuale che non corrispondeva a `station_callsign`; `operator` viene ora scritto solo se e' un nominativo valido e distinto.
- Corretto il doppio click nelle finestre decode: `RR73`, `73`, `RRR`, `R`, `TU`, `OOO` e i locator Maidenhead non armano piu' il TX come se fossero callsign radioamatoriali.

### Espanol

Release centrada en promover aun mas el runtime JT legacy hacia C++ nativo, corregir la gestion de respuestas a indicativos especiales/no estandar, reforzar la portabilidad GCC/Linux y ampliar el conjunto release publicado tambien a la AppImage Linux `aarch64`, ademas de las salidas macOS y Linux `x86_64` ya existentes.

#### Anadido

- Anadidas orquestacion runtime JT65 nativa C++, helpers DSP/IO JT65, bloques decoder JT9 fast/wide y una cobertura compare/regression mas amplia para la migracion JT legacy que continua.
- Anadido soporte release Linux AppImage `aarch64` mediante camino de build ARM64 basado en Debian Trixie y cobertura runner ARM de GitHub Actions.
- Anadida cobertura de regresion dedicada para los flujos reply a indicativos no estandar/special-event y controles adicionales de portabilidad en las utilidades compare.

#### Cambios

- El runtime activo JT65 corre ahora por el camino promovido nativo C++, y las viejas fuentes Fortran JT65 del camino activo se han eliminado del build activo.
- `build-arm.sh` es ahora sensible a la version, apto para CI, y excluye `build-arm-output` del staging de fuentes; `build-arm-output/` queda ignorado permanentemente.
- La ingenieria release Linux publica ahora AppImage `x86_64` y `aarch64`, mientras los targets macOS siguen siendo Tahoe arm64, Sequoia arm64, Sequoia x86_64 y Monterey x86_64 best effort.
- Metadatos locales de version, defaults de workflow, readmes, documentacion, notas release, package description y body GitHub quedan alineados con la semver `1.6.0`.

#### Corregido

- Corregidas las respuestas a indicativos no estandar o special-event que se rechazaban con `*** bad message ***`.
- Corregido el falso positivo GCC 14 `stringop-overflow` en `LegacyDspIoHelpers.cpp` sin romper macOS Clang.
- Corregidos los fallos de portabilidad GCC/libstdc++ en `jt9_wide_stage_compare.cpp` y `legacy_wsprio_compare.cpp`.
- Corregida la regresion ADIF/QRZ causada por exportar un valor `operator` de texto libre que no coincidia con `station_callsign`; `operator` ahora solo se emite si es un indicativo valido y distinto.
- Corregido el doble click en las ventanas de decode: `RR73`, `73`, `RRR`, `R`, `TU`, `OOO` y los localizadores Maidenhead ya no arman TX como si fueran indicativos de radio.

## [1.5.9] - 2026-04-01

### English

Release focused on eliminating the remaining practical Linux FT2/FT4 transmit-latency issues on older Ubuntu hosts, hardening macOS shutdown/UI behavior, refreshing FT2 branding, and aligning the release surface to semantic version `1.5.9`.

#### Added

- Added immediate post-waveform TX start plus CAT/PTT fallback start for Linux FT2/FT4 when rig-state confirmation arrives late.
- Added lower-latency Linux TX audio defaults through smaller output queue sizing and low-latency audio category selection.
- Added refreshed FT2 launcher/app icon assets for macOS and Linux release outputs.

#### Changed

- Standard Linux FT2/FT4 TX generation no longer waits behind unnecessary global Fortran runtime mutex contention on the normal native C++ waveform path.
- Linux FT2 now uses zero extra delay and zero extra lead-in on the standard precomputed-wave path.
- Linux FT2/FT4 stop logic now follows real modulator/audio completion instead of slot timing only.
- FT2/FT4 debug waveform dumps are only written when debug logging is enabled.
- Local version metadata, workflow defaults, readmes, docs, release notes, and GitHub release body are aligned to semantic version `1.5.9`.

#### Fixed

- Fixed intermittent Ubuntu/Linux cases where FT2/FT4 keyed the radio on time but started the payload much later or near the end of the slot.
- Fixed a macOS close-time crash caused by `MainWindow` status-bar/member-widget ownership and destruction ordering.
- Fixed the `Band Hopping` UI regression that incorrectly painted `QSOs to upload` red.

### Italiano

Release focalizzata nell'eliminare i problemi pratici residui di latenza TX Linux FT2/FT4 sui sistemi Ubuntu piu' vecchi, nel rafforzare il comportamento macOS in chiusura/UI, nell'aggiornare il branding FT2 e nell'allineare la superficie release alla semver `1.5.9`.

#### Aggiunto

- Aggiunti start immediato post-waveform e fallback CAT/PTT per Linux FT2/FT4 quando la conferma dello stato rig arriva in ritardo.
- Aggiunti default audio TX Linux a latenza piu' bassa tramite coda output piu' piccola e categoria audio low-latency.
- Aggiunti asset icona/launcher FT2 aggiornati per gli output release macOS e Linux.

#### Modificato

- Il TX standard Linux FT2/FT4 non aspetta piu' il contenzioso inutile del mutex globale Fortran sul normale path waveform nativo C++.
- FT2 Linux usa ora zero ritardo extra e zero lead-in extra sul path precomputato standard.
- La logica di stop Linux FT2/FT4 segue ora la fine reale di modulator/audio e non solo il timing dello slot.
- I dump waveform FT2/FT4 vengono scritti solo quando il debug log e' attivo.
- Metadati versione locali, default workflow, readme, documentazione, note release e body GitHub sono allineati alla semver `1.5.9`.

#### Corretto

- Corretti i casi Ubuntu/Linux intermittenti in cui FT2/FT4 mettevano in TX la radio in orario ma iniziavano il payload molto piu' tardi o quasi a fine slot.
- Corretto un crash macOS in chiusura causato dall'ordine di ownership/distruzione dei widget status-bar/member in `MainWindow`.
- Corretta la regressione UI `Band Hopping` che colorava in rosso `QSOs to upload`.

### Espanol

Release centrada en eliminar los problemas practicos restantes de latencia TX Linux FT2/FT4 en sistemas Ubuntu antiguos, reforzar el comportamiento macOS al cerrar/UI, actualizar el branding FT2 y alinear la superficie release con la semver `1.5.9`.

#### Anadido

- Anadidos arranque inmediato post-waveform y fallback CAT/PTT para Linux FT2/FT4 cuando la confirmacion del estado del equipo llega tarde.
- Anadidos defaults de audio TX Linux de menor latencia mediante cola de salida mas pequena y categoria de audio low-latency.
- Anadidos assets icono/launcher FT2 actualizados para las salidas release macOS y Linux.

#### Cambios

- El TX estandar Linux FT2/FT4 ya no espera el bloqueo innecesario del mutex global Fortran en el camino waveform nativo C++ normal.
- FT2 Linux usa ahora cero retraso extra y cero lead-in extra en el camino precomputado estandar.
- La logica de stop Linux FT2/FT4 sigue ahora la finalizacion real de modulator/audio y no solo el timing del slot.
- Los dumps waveform FT2/FT4 se escriben solo cuando el debug log esta activo.
- Metadatos locales de version, defaults de workflow, readmes, documentacion, notas release y body GitHub quedan alineados a la semver `1.5.9`.

#### Corregido

- Corregidos los casos Ubuntu/Linux intermitentes donde FT2/FT4 ponian la radio en TX a tiempo pero iniciaban el payload mucho mas tarde o casi al final del slot.
- Corregido un crash macOS al cerrar causado por el orden de ownership/destruccion de widgets status-bar/member en `MainWindow`.
- Corregida la regresion UI `Band Hopping` que pintaba `QSOs to upload` de rojo.

## [1.5.8] - 2026-03-31

### English

Release focused on completing the promoted native C++ runtime for the FTX family, removing the remaining FST4/Q65/MSK144/SuperFox Fortran residues, hardening macOS shutdown/data-path stability, and fixing Linux GCC/Ubuntu release builds.

#### Added

- Added native C++ FST4/FST4W core/LDPC/shared-DSP pipeline coverage plus native helper/simulator tools such as `fst4sim`, `ldpcsim240_101`, and `ldpcsim240_74`.
- Added native utility/front-end replacements for `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx`, and `sftx`.
- Added broader `test_qt_helpers` and utility smoke coverage for shared DSP, FST4 parity/oracle behavior, and native Q65 compatibility entry points.

#### Changed

- FT8, FT4, FT2, Q65, MSK144, SuperFox, and FST4/FST4W now ship from the promoted native C++ runtime path without mode-specific Fortran active/runtime residues.
- The remaining promoted FST4 decode helpers (`blanker`, `four2a`, `pctile`, `polyfit`) now resolve to native C++ implementations.
- The old promoted-mode trees for `ana64`, `q65_subs`, MSK144/MSK40 snapshots, and the historical SuperFox Fortran subtree have been removed after native promotion.
- Local version metadata, workflow defaults, readmes, docs, release notes, and GitHub release body are aligned to semantic version `1.5.8`.

#### Fixed

- Fixed macOS shutdown crashes triggered by premature global FFTW cleanup while thread-local FFTW plans were still finalizing.
- Fixed `MainWindow::dataSink` / `fastSink` frame clamping and writable-data-dir handling to avoid crash-prone invalid indices and hot-path `QDir` reconstruction.
- Fixed Linux/GCC 15 build breaks involving `_q65_mask`, `pack28`, legacy tool linkage to migrated symbols such as `four2a_`, and the MSK40 off-by-one bug in `decodeframe40_native`.
- Fixed full-build regressions by keeping legacy tools/tests linked against the migrated C++ runtime symbols.

### Italiano

Release focalizzata sul completamento del runtime promosso nativo C++ per la famiglia FTX, sulla rimozione degli ultimi residui Fortran FST4/Q65/MSK144/SuperFox, sull'hardening della stabilita' macOS in chiusura/percorso dati e sulla correzione delle build Linux GCC/Ubuntu.

#### Aggiunto

- Aggiunta copertura nativa C++ per la pipeline FST4/FST4W core/LDPC/DSP condiviso e per gli helper/simulatori `fst4sim`, `ldpcsim240_101` e `ldpcsim240_74`.
- Aggiunti i sostituti utility/front-end nativi per `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx` e `sftx`.
- Aggiunta copertura piu' ampia in `test_qt_helpers` e smoke-test utility per DSP condiviso, parity/oracle FST4 e punti di compatibilita' Q65 nativi.

#### Modificato

- FT8, FT4, FT2, Q65, MSK144, SuperFox e FST4/FST4W vengono ora distribuiti dal runtime promosso nativo C++ senza residui Fortran specifici del modo nel path attivo/runtime.
- Gli ultimi helper decode promossi FST4 (`blanker`, `four2a`, `pctile`, `polyfit`) risolvono ora su implementazioni native C++.
- I vecchi tree promossi per `ana64`, `q65_subs`, snapshot MSK144/MSK40 e storico subtree SuperFox Fortran sono stati rimossi dopo la promozione nativa.
- Metadati versione locali, default workflow, readme, documentazione, note release e body GitHub sono allineati alla semver `1.5.8`.

#### Corretto

- Corretto il crash macOS in chiusura causato dal cleanup FFTW globale prematuro mentre i piani FFTW thread-local erano ancora in finalizzazione.
- Corretti `MainWindow::dataSink` / `fastSink` nel clamp dei frame e nella gestione della writable-data-dir per evitare indici invalidi e ricostruzioni `QDir` nel path caldo.
- Corretti i fallimenti Linux/GCC 15 relativi a `_q65_mask`, `pack28`, link dei tool legacy verso simboli C++ migrati come `four2a_`, e il bug off-by-one MSK40 in `decodeframe40_native`.
- Corrette regressioni di full-build mantenendo tool/test legacy linkati ai simboli del runtime C++ migrato.

### Espanol

Release centrada en completar el runtime promovido nativo C++ para la familia FTX, eliminar los ultimos residuos Fortran FST4/Q65/MSK144/SuperFox, endurecer la estabilidad macOS al cerrar/ruta de datos y corregir las builds Linux GCC/Ubuntu.

#### Anadido

- Anadida cobertura nativa C++ para la pipeline FST4/FST4W core/LDPC/DSP compartido y para los helpers/simuladores `fst4sim`, `ldpcsim240_101` y `ldpcsim240_74`.
- Anadidos reemplazos utility/front-end nativos para `encode77`, `hash22calc`, `msk144code`, `msk144sim`, `sfoxsim`, `sfrx` y `sftx`.
- Anadida cobertura mas amplia en `test_qt_helpers` y smoke-tests de utilidades para DSP compartido, parity/oracle FST4 y puntos de compatibilidad Q65 nativos.

#### Cambios

- FT8, FT4, FT2, Q65, MSK144, SuperFox y FST4/FST4W se distribuyen ahora desde el runtime promovido nativo C++ sin residuos Fortran especificos del modo en el camino activo/runtime.
- Los ultimos helpers decode promovidos FST4 (`blanker`, `four2a`, `pctile`, `polyfit`) resuelven ahora a implementaciones nativas C++.
- Los viejos arboles promovidos para `ana64`, `q65_subs`, snapshots MSK144/MSK40 y el historico subtree SuperFox Fortran se han eliminado tras la promocion nativa.
- Metadatos locales de version, defaults de workflow, readmes, documentacion, notas release y body GitHub quedan alineados a la semver `1.5.8`.

#### Corregido

- Corregido el crash macOS al cerrar causado por el cleanup FFTW global prematuro mientras los planes FFTW thread-local seguian finalizandose.
- Corregidos `MainWindow::dataSink` / `fastSink` en el clamping de frames y en el manejo de writable-data-dir para evitar indices invalidos y reconstrucciones `QDir` en la ruta caliente.
- Corregidos los fallos Linux/GCC 15 relacionados con `_q65_mask`, `pack28`, enlace de herramientas legacy con simbolos C++ migrados como `four2a_`, y el bug off-by-one MSK40 en `decodeframe40_native`.
- Corregidas regresiones de full-build manteniendo herramientas/tests legacy enlazados con los simbolos del runtime C++ migrado.

## [1.5.7] - 2026-03-30

### English

Release focused on FT2 decode sanity, reliable FT2 Band Activity operator workflow, and keeping Linux release publishing intact while aligning the release surface to semantic version `1.5.7`.

#### Added

- Added an FT2 type-4 plausibility filter that rejects clearly impossible callsign-like payloads before they enter the accepted decode path.
- Added FT2 ghost filtering for implausible free-text decodes that masquerade as callsign pairs while preserving legitimate operator free text.
- Added targeted `test_qt_helpers` regression coverage for valid slash/special-event FT2 forms and invalid garbage examples.

#### Changed

- FT2 Band Activity double-click on standard `CQ` / `QRZ` lines now directly arms the selected caller instead of relying on the older generic path.
- FT2 Band Activity double-click now also follows the clicked callsign token inside FT2 rows, matching the map-style station-centric selection flow.
- Local version metadata, workflow defaults, readmes, docs, release notes, and GitHub release body are aligned to semantic version `1.5.7`.
- macOS packaging continues with the folder/layout already validated by the previous successful deploy.
- macOS DMG packaging now stages from an isolated temporary copy and retries `hdiutil create` to survive transient `Resource busy` failures.

#### Fixed

- Fixed bogus FT2 decodes such as `CAYOBTYZCV0`, `7SVPAYTXTIK`, `M9B ZNWF6WH7V`, and similar noise-derived payloads being shown as meaningful traffic.
- Fixed FT2 Band Activity cases where valid callers like `D2UY`, `K1RZ`, `KL7J`, and `N7XR` did not arm reliably on double-click from Band Activity.
- Fixed Linux release/AppImage workflow breakage by restoring the `wsprd` build target to the published binary set.
- Fixed transient Monterey Intel macOS release failures where `hdiutil create` aborted with `Resource busy` during DMG asset generation.

### Italiano

Release focalizzata sulla sanita' del decode FT2, sul workflow operatore FT2 affidabile dalla Band Activity, e sul mantenimento integro della pubblicazione release Linux, allineando al tempo stesso la superficie release alla semver `1.5.7`.

#### Aggiunto

- Aggiunto un filtro di plausibilita' FT2 type-4 che rigetta payload chiaramente impossibili simili a nominativi prima che entrino nel path decode accettato.
- Aggiunto filtraggio FT2 per ghost free-text implausibili che si mascherano da coppie di nominativi, preservando pero' il free-text operatore legittimo.
- Aggiunta copertura di regressione mirata in `test_qt_helpers` per forme FT2 valide con slash/special-event e per esempi garbage non validi.

#### Modificato

- Il doppio click FT2 nella Band Activity sui messaggi standard `CQ` / `QRZ` arma ora direttamente il caller selezionato invece di affidarsi al vecchio path generico.
- Il doppio click FT2 nella Band Activity segue ora anche il nominativo cliccato dentro le righe FT2, allineandosi al flusso station-centric gia' usato dalla mappa.
- Metadati versione locali, default workflow, readme, documentazione, note release e body GitHub sono allineati alla semver `1.5.7`.
- Il packaging macOS continua a usare il layout/cartelle gia' validati dal precedente deploy riuscito.
- Il packaging DMG macOS usa ora staging temporaneo isolato e retry di `hdiutil create` per resistere a fallimenti transitori `Resource busy`.

#### Corretto

- Corretti decode FT2 fasulli come `CAYOBTYZCV0`, `7SVPAYTXTIK`, `M9B ZNWF6WH7V` e payload simili derivati dal rumore che venivano mostrati come traffico significativo.
- Corretti casi FT2 nella Band Activity in cui caller validi come `D2UY`, `K1RZ`, `KL7J` e `N7XR` non si armavano in modo affidabile al doppio click dalla Band Activity.
- Corretto il blocco dei workflow Linux release/AppImage ripristinando il target `wsprd` nel set binario pubblicato.
- Corretto il fallimento transitorio della release macOS Monterey Intel in cui `hdiutil create` abortiva con `Resource busy` durante la generazione del DMG.

### Espanol

Release centrada en la cordura del decode FT2, en un workflow operativa FT2 fiable desde Band Activity, y en mantener intacta la publicacion release Linux mientras se alinea la superficie release con la semver `1.5.7`.

#### Anadido

- Anadido un filtro de plausibilidad FT2 type-4 que rechaza payloads claramente imposibles con aspecto de nominativo antes de entrar en el camino de decode aceptado.
- Anadido filtrado FT2 para ghost free-text implausibles disfrazados de pares de indicativos, preservando al mismo tiempo el free-text legitimo del operador.
- Anadida cobertura de regresion dirigida en `test_qt_helpers` para formas FT2 validas con slash/special-event y para ejemplos garbage no validos.

#### Cambios

- El doble click FT2 en Band Activity sobre mensajes estandar `CQ` / `QRZ` arma ahora directamente el caller seleccionado en lugar de apoyarse en el viejo camino generico.
- El doble click FT2 en Band Activity sigue ahora tambien el indicativo clicado dentro de las lineas FT2, alineandose con el flujo station-centric ya usado por el mapa.
- Metadatos locales de version, defaults de workflow, readmes, documentacion, notas release y body GitHub quedan alineados con la semver `1.5.7`.
- El packaging macOS continua usando el layout/carpetas ya validados por el deploy correcto anterior.
- El packaging DMG macOS usa ahora staging temporal aislado y reintentos de `hdiutil create` para resistir fallos transitorios `Resource busy`.

#### Corregido

- Corregidos decodes FT2 falsos como `CAYOBTYZCV0`, `7SVPAYTXTIK`, `M9B ZNWF6WH7V` y payloads similares derivados del ruido que se mostraban como trafico significativo.
- Corregidos casos FT2 en Band Activity donde callers validos como `D2UY`, `K1RZ`, `KL7J` y `N7XR` no se armaban de manera fiable al doble click desde Band Activity.
- Corregido el bloqueo de los workflows Linux release/AppImage restaurando el target `wsprd` en el conjunto binario publicado.
- Corregido el fallo transitorio de la release macOS Monterey Intel donde `hdiutil create` abortaba con `Resource busy` durante la generacion del DMG.

## [1.5.6] - 2026-03-29

### English

Release focused on completing the promoted native C++ runtime for FT8/FT4/FT2/Q65, extending the worker-based in-process architecture, hardening TX/build behaviour, and aligning the validated macOS/Linux release layout to semantic version `1.5.6`.

#### Added

- Added native C++ utilities/frontends for `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65`, and `rtty_spec`.
- Added parity/regression tooling with `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare`, and `ft2_make_test_wav`.
- Added richer FT2/FT4/Fox waveform snapshot tracing in writable `debug.txt`.

#### Changed

- FT8, FT4, FT2, and Q65 now use the promoted native C++ runtime path without mode-specific Fortran orchestration on the active path.
- The main application no longer provisions the old `jt9` shared-memory bootstrap for promoted FTX runtime workers.
- macOS release packaging keeps the folder/layout changes already validated by the last successful deploy and aligns Tahoe, Sequoia, Intel Sequoia, Monterey, and Linux AppImage release targets.
- Local version metadata, workflow defaults, release docs, and About text are aligned to semantic version `1.5.6`.

#### Fixed

- Fixed GNU `ld` static-library cycle and link-order failures across `wsjt_qt`, `wsjt_cxx`, and `wsjt_fort`.
- Fixed GCC 15 / Qt5 / C++11 build failures in native tools, tests, and bridge code.
- Fixed FT2/FT4/Fox precomputed-wave startup timing by keeping safer lead-in and cached waveform handoff.
- Fixed several parity/regression blind spots by extending stage-compare and helper-test coverage.

### Italiano

Release focalizzata sul completamento del runtime promosso nativo C++ per FT8/FT4/FT2/Q65, sull'estensione dell'architettura in-process a worker, sull'hardening di TX/build e sull'allineamento del layout release macOS/Linux gia' validato alla semver `1.5.6`.

#### Aggiunto

- Aggiunti utility/front-end nativi C++ per `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` e `rtty_spec`.
- Aggiunti strumenti di parita'/regressione con `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare` e `ft2_make_test_wav`.
- Aggiunto tracing piu' ricco in `debug.txt` per snapshot waveform FT2/FT4/Fox.

#### Modificato

- FT8, FT4, FT2 e Q65 usano ora il runtime promosso nativo C++ senza orchestrazione Fortran specifica nel path attivo.
- L'app principale non alloca piu' il vecchio bootstrap `jt9` a shared memory per i worker runtime FTX promossi.
- Il packaging release macOS mantiene le cartelle/layout gia' validati dall'ultimo deploy riuscito e allinea i target Tahoe, Sequoia, Intel Sequoia, Monterey e Linux AppImage.
- Metadati versione locali, default workflow, documenti release e testo About sono allineati alla semver `1.5.6`.

#### Corretto

- Corretti i fallimenti GNU `ld` di cicli di librerie statiche e ordine di link fra `wsjt_qt`, `wsjt_cxx` e `wsjt_fort`.
- Corretti i fallimenti di build GCC 15 / Qt5 / C++11 nei tool nativi, nei test e nel codice bridge.
- Corretta la temporizzazione di avvio delle wave precompute FT2/FT4/Fox mantenendo lead-in piu' sicuro e handoff waveform cache.
- Ridotte diverse zone cieche di parita'/regressione estendendo stage-compare e test helper.

### Espanol

Release centrada en completar el runtime promovido nativo C++ para FT8/FT4/FT2/Q65, ampliar la arquitectura in-process con workers, endurecer TX/build y alinear el layout release macOS/Linux ya validado con la semver `1.5.6`.

#### Anadido

- Anadidas utilidades/frontends nativos C++ para `q65sim`, `q65code`, `q65_ftn_test`, `q65params`, `test_q65` y `rtty_spec`.
- Anadidas herramientas de paridad/regresion con `ft8_stage_compare`, `ft4_stage_compare`, `ft2_stage_compare`, `ft2_equalized_compare`, `q65_stage_compare` y `ft2_make_test_wav`.
- Anadido trazado mas rico en `debug.txt` para snapshots waveform FT2/FT4/Fox.

#### Cambios

- FT8, FT4, FT2 y Q65 usan ahora el runtime promovido nativo C++ sin orquestacion Fortran especifica en el camino activo.
- La aplicacion principal ya no reserva el viejo bootstrap `jt9` de shared memory para los workers runtime FTX promovidos.
- El packaging release macOS mantiene las carpetas/layout ya validados por el ultimo deploy correcto y alinea Tahoe, Sequoia, Intel Sequoia, Monterey y Linux AppImage.
- Metadatos locales de version, defaults de workflow, documentos release y texto About quedan alineados a la semver `1.5.6`.

#### Corregido

- Corregidos los fallos GNU `ld` de ciclos de librerias estaticas y orden de enlace entre `wsjt_qt`, `wsjt_cxx` y `wsjt_fort`.
- Corregidos los fallos de build GCC 15 / Qt5 / C++11 en herramientas nativas, tests y codigo bridge.
- Corregida la temporizacion de arranque de ondas precomputadas FT2/FT4/Fox manteniendo lead-in mas seguro y handoff waveform cache.
- Reducidas varias zonas ciegas de paridad/regresion ampliando stage-compare y tests helper.

## [1.5.5] - 2026-03-23

### English

Release focused on macOS Preferences correctness across all UI languages, FT2 subprocess diagnostics, FT2 ADIF migration, audio startup recovery, and withdrawal of the experimental RTTY UI from the public release.

#### Added

- Added persistent `jt9_subprocess.log` tracing for FT2/jt9 subprocess launch, stderr, and termination events.
- Added clearer shared-memory and stdout/stderr diagnostics for FT2 subprocess failures.
- Added automatic backup-preserving migration of historical FT2 ADIF records to `MODE=MFSK` + `SUBMODE=FT2`.

#### Changed

- macOS now forces explicit menu roles so only `About`, `Preferences`, and `Quit` use native app-menu roles.
- Settings pages are now scrollable on macOS as well as Linux.
- The incomplete RTTY UI is hidden from menus and settings in the public release path.
- Local version metadata, workflow defaults, release docs, and About text are aligned to semantic version `1.5.5`.

#### Fixed

- Fixed macOS native `Preferences...` opening the wrong action in some translated UI languages.
- Fixed the Settings dialog growing beyond screen height on macOS and hiding the confirm buttons.
- Fixed FT2 subprocess crash reporting being too opaque to diagnose on hardened macOS systems.
- Fixed startup/monitor audio cases where audio only resumed after reopening `Settings > Audio`.

### Italiano

Release focalizzata su correttezza di Preferenze su macOS in tutte le lingue UI, diagnostica del subprocess FT2, migrazione ADIF FT2, recupero audio all'avvio e ritiro della UI RTTY sperimentale dal percorso pubblico della release.

#### Aggiunto

- Aggiunto `jt9_subprocess.log` persistente per tracciare avvio, stderr e terminazione del subprocess FT2/jt9.
- Aggiunta diagnostica piu' chiara per i fallimenti FT2 del subprocess, con dettagli shared-memory e stdout/stderr.
- Aggiunta migrazione automatica con backup dei record ADIF FT2 storici verso `MODE=MFSK` + `SUBMODE=FT2`.

#### Modificato

- Su macOS sono ora forzati ruoli menu espliciti, cosi' solo `About`, `Preferences` e `Quit` usano i ruoli nativi del menu app.
- Le pagine di Settings sono ora scrollabili anche su macOS oltre che su Linux.
- La UI RTTY incompleta e' nascosta da menu e impostazioni nel percorso pubblico della release.
- Metadati versione locali, default workflow, documenti release e testo About sono allineati alla semver `1.5.5`.

#### Corretto

- Corretta l'apertura errata di `Preferences...` su macOS in alcune lingue UI tradotte.
- Corretto il dialog Settings che su macOS poteva superare l'altezza dello schermo e nascondere i pulsanti finali.
- Corretta l'eccessiva opacita' del reporting crash del subprocess FT2 su sistemi macOS hardened.
- Corretti i casi di avvio/monitor in cui l'audio ripartiva solo dopo aver riaperto `Settings > Audio`.

### Espanol

Release centrada en la correccion de Preferencias en macOS para todas las lenguas UI, la diagnostica del subprocess FT2, la migracion ADIF FT2, la recuperacion del audio al arrancar y la retirada de la UI RTTY experimental del camino publico de release.

#### Anadido

- Anadido `jt9_subprocess.log` persistente para trazar arranque, stderr y finalizacion del subprocess FT2/jt9.
- Anadida una diagnostica mas clara para fallos FT2 del subprocess, con detalles de shared-memory y stdout/stderr.
- Anadida migracion automatica con backup de los registros ADIF FT2 historicos hacia `MODE=MFSK` + `SUBMODE=FT2`.

#### Cambios

- En macOS se fuerzan ahora roles explicitos de menu, de forma que solo `About`, `Preferences` y `Quit` usan roles nativos del menu app.
- Las paginas de Settings son ahora scrollables tambien en macOS ademas de Linux.
- La UI RTTY incompleta queda oculta de menus y ajustes en el camino publico de release.
- Metadatos locales de version, defaults de workflow, documentos release y texto About quedan alineados a la semver `1.5.5`.

#### Corregido

- Corregida la apertura equivocada de `Preferences...` en macOS en algunas lenguas UI traducidas.
- Corregido el dialogo Settings que en macOS podia superar la altura de pantalla y ocultar los botones finales.
- Corregida la excesiva opacidad del reporting de fallos del subprocess FT2 en sistemas macOS hardened.
- Corregidos los casos de arranque/monitor en los que el audio solo volvia tras reabrir `Settings > Audio`.

## [1.5.4] - 2026-03-22

### English

Release focused on FT2/FT4/FT8 decoder quality, web-app parity, UI/language alignment, downloader and secure-settings hardening, and expanded release/test coverage.

#### Added

- Added FT2 anti-ghost filtering for very weak decodes, with `ghostPass` / `ghostFilt` tracing in `debug.txt`.
- Added web-app `Monitoring ON/OFF`, FT2 `ASYNC` dB display, and `Hide CQ` / `Hide 73` activity filters.
- Added full web-app localization coverage for all bundled UI languages, driven by the desktop app language.
- Added the dedicated `SecureSettings` module.
- Added downloader integration tests plus RFC HOTP/TOTP test vectors for `SHA1`, `SHA256`, and `SHA512`.

#### Changed

- FT2, FT4, and FT8 now use more robust Costas sync strategies and deeper/adaptive subtraction thresholds for weak-signal decoding.
- UTC clock and Astro date/time output now follow the `Day Month Year` plus `UTC` format using the app-selected language.
- User-facing title/version branding keeps `Decodium` and removes the old `v3.0 FT2 "Raptor"` title text.
- The language menu now keeps a single `English` entry.
- Release defaults, workflow defaults, and release documents are aligned to semantic version `1.5.4`.

#### Fixed

- Fixed very weak FT2 ghost or malformed decodes slipping through without a dedicated weak-signal sanity filter.
- Fixed the web app missing live monitoring control, missing ASYNC dB feedback, and drifting to browser language instead of the app language.
- Fixed secure-setting fallback/import edge cases and made LoTW default to `https`.
- Fixed downloader acceptance of bad redirect schemes and unbounded large downloads.
- Fixed silent CAT/transceiver exceptions and reduced blind DXLab startup waits.
- Fixed Linux `GNU ld` unit-test link failures by correcting static-library order.

### Italiano

Release focalizzata su qualita' decoder FT2/FT4/FT8, parita' funzionale della web app, allineamento UI/lingue, hardening di downloader e secure settings, ed estensione della copertura test/release.

#### Aggiunto

- Aggiunto il filtro FT2 anti-ghost per decode molto deboli, con tracing `ghostPass` / `ghostFilt` in `debug.txt`.
- Aggiunti alla web app `Monitoring ON/OFF`, indicatore FT2 `ASYNC` in dB e filtri attivita' `Hide CQ` / `Hide 73`.
- Aggiunta copertura completa della localizzazione web app per tutte le lingue UI bundle, guidata dalla lingua selezionata nell'app desktop.
- Aggiunto il modulo dedicato `SecureSettings`.
- Aggiunti test integrazione downloader e vettori RFC HOTP/TOTP per `SHA1`, `SHA256` e `SHA512`.

#### Modificato

- FT2, FT4 e FT8 usano ora strategie Costas sync piu' robuste e soglie di sottrazione piu' profonde/adattive per il decoding weak-signal.
- Orologio UTC e pannello Astro seguono ora il formato `Giorno Mese Anno` piu' `UTC`, usando la lingua selezionata nell'app.
- Il branding titolo/versione lato utente mantiene `Decodium` e rimuove il vecchio testo `v3.0 FT2 "Raptor"`.
- Il menu lingue mantiene ora una sola voce `English`.
- Default release, default workflow e documentazione release sono allineati alla semver `1.5.4`.

#### Corretto

- Corretti i decode FT2 ghost o malformati molto deboli che passavano senza un filtro weak-signal dedicato.
- Corretta la web app che non esponeva il controllo monitoring, non mostrava i dB ASYNC e usava la lingua del browser invece di quella dell'app.
- Corretti i casi limite di fallback/import dei secure settings e portato LoTW di default a `https`.
- Corretta l'accettazione nel downloader di redirect con scheme non validi e di download troppo grandi senza limite.
- Corretti logging silenziosi delle eccezioni CAT/transceiver e ridotti i blind wait di startup DXLab.
- Corretto il linking dei test su Linux con `GNU ld`, sistemando l'ordine delle librerie statiche.

### Espanol

Release centrada en calidad de decoder FT2/FT4/FT8, paridad funcional de la web app, alineacion UI/idiomas, endurecimiento de downloader y secure settings, y ampliacion de la cobertura test/release.

#### Anadido

- Anadido el filtro FT2 anti-ghost para decodes muy debiles, con trazas `ghostPass` / `ghostFilt` en `debug.txt`.
- Anadidos a la web app `Monitoring ON/OFF`, indicador FT2 `ASYNC` en dB y filtros de actividad `Hide CQ` / `Hide 73`.
- Anadida cobertura completa de localizacion web app para todas las lenguas UI bundle, guiada por el idioma seleccionado en la app de escritorio.
- Anadido el modulo dedicado `SecureSettings`.
- Anadidos tests de integracion del downloader y vectores RFC HOTP/TOTP para `SHA1`, `SHA256` y `SHA512`.

#### Cambios

- FT2, FT4 y FT8 usan ahora estrategias Costas sync mas robustas y umbrales de sustraccion mas profundos/adaptativos para decoding weak-signal.
- Reloj UTC y panel Astro siguen ahora el formato `Dia Mes Ano` mas `UTC`, usando el idioma seleccionado en la app.
- El branding titulo/version del lado del usuario mantiene `Decodium` y elimina el antiguo texto `v3.0 FT2 "Raptor"`.
- El menu de idiomas mantiene ahora una sola entrada `English`.
- Defaults release, defaults workflow y documentacion release quedan alineados a la semver `1.5.4`.

#### Corregido

- Corregidos decodes FT2 ghost o malformados muy debiles que pasaban sin un filtro weak-signal dedicado.
- Corregida la web app que no exponia el control monitoring, no mostraba los dB ASYNC y usaba el idioma del navegador en vez del de la app.
- Corregidos casos limite de fallback/import de secure settings y llevado LoTW por defecto a `https`.
- Corregida la aceptacion en el downloader de redirects con scheme no valido y de descargas demasiado grandes sin limite.
- Corregidos logs silenciosos de excepciones CAT/transceiver y reducidos los blind waits de arranque DXLab.
- Corregido el enlazado de tests en Linux con `GNU ld`, ajustando el orden de las librerias estaticas.

## [1.5.3] - 2026-03-22

### English

Release focused on CQRLOG interoperability, FT4/FT8 Wait Features behaviour, local version propagation, and new bundled German/French translations.

#### Added

- Added bundled German (`de`) and French (`fr`) UI translation packs.
- Added the helper `tools/generate_ts_translations.py` to rebuild missing TS bundles from the English base.

#### Changed

- The language menu now exposes only translations that are actually bundled in the app and falls back safely to English if a saved language is no longer available.
- Release defaults and documentation are aligned to semantic version `1.5.3`, including the experimental Hamlib Linux build default.

#### Fixed

- Fixed Linux `CQRLOG wsjtx remote` interoperability by restoring the historical UDP listen-port behaviour and using `WSJTX` as the compatibility client id.
- Fixed local rebuild version drift so changing `fork_release_version.txt` propagates to the compiled app after rebuild/reconfigure.
- Fixed FT4/FT8 `Wait Features + AutoSeq` so a busy or late partner reply now pauses the current TX cycle instead of transmitting over an active QSO.

### Italiano

Release focalizzata su interoperabilita' CQRLOG, comportamento `Wait Features` in FT4/FT8, propagazione versione locale e nuove traduzioni bundle in tedesco/francese.

#### Aggiunto

- Aggiunti i pacchetti traduzione UI bundle tedesco (`de`) e francese (`fr`).
- Aggiunto l'helper `tools/generate_ts_translations.py` per rigenerare i bundle TS mancanti a partire dalla base inglese.

#### Modificato

- Il menu lingue mostra ora solo traduzioni realmente incluse nell'app e ripiega in sicurezza sull'inglese se la lingua salvata non e' piu' disponibile.
- Default release e documentazione sono allineati alla semver `1.5.3`, incluso il default del build Linux sperimentale Hamlib.

#### Corretto

- Corretta l'interoperabilita' Linux con `CQRLOG wsjtx remote` ripristinando il comportamento storico della listen port UDP e usando `WSJTX` come client id di compatibilita'.
- Corretto il drift della versione nelle build locali, cosi' un cambio in `fork_release_version.txt` si propaga davvero all'app compilata dopo rebuild/reconfigure.
- Corretto `Wait Features + AutoSeq` in FT4/FT8: una risposta tardiva o una stazione occupata mette ora in pausa il ciclo TX corrente invece di trasmettere sopra un QSO attivo.

### Espanol

Release centrada en interoperabilidad CQRLOG, comportamiento `Wait Features` en FT4/FT8, propagacion de version local y nuevas traducciones bundle en aleman/frances.

#### Anadido

- Anadidos los paquetes de traduccion UI bundle aleman (`de`) y frances (`fr`).
- Anadido el helper `tools/generate_ts_translations.py` para regenerar los bundles TS ausentes a partir de la base inglesa.

#### Cambios

- El menu de idiomas muestra ahora solo traducciones realmente incluidas en la app y vuelve de forma segura a ingles si la lengua guardada ya no esta disponible.
- Los defaults de release y la documentacion quedan alineados a la semver `1.5.3`, incluido el default del build Linux experimental Hamlib.

#### Corregido

- Corregida la interoperabilidad Linux con `CQRLOG wsjtx remote` restaurando el comportamiento historico de la listen port UDP y usando `WSJTX` como client id de compatibilidad.
- Corregido el desfase de version en builds locales para que un cambio en `fork_release_version.txt` se propague de verdad a la app compilada tras rebuild/reconfigure.
- Corregido `Wait Features + AutoSeq` en FT4/FT8: una respuesta tardia o una estacion ocupada pausa ahora el ciclo TX actual en lugar de transmitir sobre un QSO activo.

## [1.5.1] - 2026-03-19

### English

Release focused on in-app update discovery, FT2/FT4/FT8 late-signoff correctness, AutoCQ direct-reply state-machine hardening, map cleanup while transmitting CQ, and PSK Reporter identity alignment.

#### Added

- Added in-app update checks against the GitHub Releases feed.
- Added a manual `Help > Check for updates...` action.
- Added direct asset selection for the updater so macOS and Linux users are taken to the best matching download.

#### Changed

- PSK Reporter program information now uses the exact application title-bar string.
- macOS/Linux release workflows and release documentation are aligned to semantic version `1.5.1`.
- macOS release helper script now defaults to publishing against `elisir80/decodium3-build-macos`.

#### Fixed

- Fixed late `73/RR73` handling after local signoff so completed QSOs log instead of repeating `73/RR73`.
- Fixed timeout-abandon paths so delayed final acknowledgements from the active partner can still recover and log correctly.
- Fixed stale DX partner reuse after returning to CQ.
- Fixed direct-caller transitions that decoded the caller but kept transmitting CQ instead of arming `Tx2`.
- Fixed the first direct FT2 reply reusing a stale report from the previous QSO instead of the current caller SNR.
- Fixed the live world map showing a false active QSO path while transmitting plain CQ.

### Italiano

Release focalizzata su updater interno, correttezza late-signoff FT2/FT4/FT8, hardening della state machine AutoCQ sulle direct reply, pulizia mappa durante il CQ e allineamento identita' PSK Reporter.

#### Aggiunto

- Aggiunto controllo aggiornamenti interno contro il feed GitHub Releases.
- Aggiunta voce manuale `Help > Check for updates...`.
- Aggiunta selezione diretta dell'asset corretto nell'updater per macOS e Linux.

#### Modificato

- Le informazioni programma inviate a PSK Reporter usano ora esattamente la stringa della barra del titolo.
- Workflow release macOS/Linux e documentazione release sono ora allineati alla semver `1.5.1`.
- Lo script helper release macOS usa ora di default il repo `elisir80/decodium3-build-macos`.

#### Corretto

- Corretto il trattamento dei `73/RR73` tardivi dopo il signoff locale, cosi' i QSO chiusi vanno a log invece di ripetere `73/RR73`.
- Corretti i path di abbandono per timeout, cosi' i final ack ritardati del partner attivo possono ancora essere recuperati e loggati.
- Corretto il riuso di partner DX stantii dopo il ritorno a CQ.
- Corretti i passaggi caller-diretto in cui il decode era valido ma l'app continuava a trasmettere CQ invece di armare `Tx2`.
- Corretto il primo reply FT2 diretto che riusava un report stantio del QSO precedente invece dell'SNR del caller corrente.
- Corretta la live world map che mostrava un falso collegamento attivo durante il CQ puro.

### Espanol

Release centrada en updater interno, correccion late-signoff FT2/FT4/FT8, endurecimiento de la maquina de estados AutoCQ sobre respuestas directas, limpieza del mapa durante CQ y alineacion de identidad PSK Reporter.

#### Anadido

- Anadida comprobacion de updates contra el feed GitHub Releases.
- Anadida accion manual `Help > Check for updates...`.
- Anadida seleccion directa del asset correcto en el updater para macOS y Linux.

#### Cambios

- La informacion de programa enviada a PSK Reporter usa ahora exactamente la cadena de la barra de titulo.
- Workflows release macOS/Linux y documentacion release alineados ahora con la semver `1.5.1`.
- El script helper de release macOS publica ahora por defecto contra `elisir80/decodium3-build-macos`.

#### Corregido

- Corregido el tratamiento de `73/RR73` tardios tras el signoff local para que los QSO completados vayan a log en lugar de repetir `73/RR73`.
- Corregidos los paths de abandono por timeout para que los final ack retrasados del partner activo todavia puedan recuperarse y logearse.
- Corregido el reuso de partner DX obsoleto tras volver a CQ.
- Corregidos los pasos de caller directo donde el decode era valido pero la app seguia transmitiendo CQ en vez de armar `Tx2`.
- Corregido el primer reply FT2 directo que reutilizaba un reporte obsoleto del QSO anterior en vez del SNR del caller actual.
- Corregido el live world map que mostraba un enlace activo falso durante CQ puro.

## [1.5.0] - 2026-03-19

### English

Release focused on FT8/FT4/FT2 QSO correctness, AutoCQ stability, FT2 Quick QSO evolution, startup audio recovery, decoder sync from upstream, and initial Decodium certificate infrastructure.

#### Added

- Added startup RX-audio recovery logic that can reopen configured audio streams and re-arm monitor state automatically.
- Added `lib/ft2/decode174_91_ft2.f90` and moved FT2 triggered decode to the dedicated FT2 LDPC decoder path.
- Added the `Quick QSO` UI toggle as an alias for FT2 `2 msg`.
- Added AutoCQ diagnostic tracing to `debug.txt`.
- Added Decodium certificate loading/autoload/status integration in the main window.
- Added `tools/generate_cert.py` to generate `.decodium` certificate files.

#### Changed

- Local UI version, release workflows, and GitHub release naming are now aligned to semantic version `1.5.0`.
- Shared LDPC decoders were aligned to the upstream Normalized Min-Sum refresh.
- FT2 Quick QSO flow was refactored to the current short schema with mixed-mode and backward-compatibility handling.
- Linux and macOS release documentation now covers Tahoe/Sequoia/Monterey/AppImage targets with startup guidance and platform requirements.

#### Fixed

- Fixed startup cases where audio devices were selected correctly but RX audio only started after reopening Audio Preferences.
- Fixed standard 5-message FT8/FT4/FT2 QSOs skipping the local final `73` when the other station sent `RR73`/`73` first.
- Fixed FT2 Quick QSO log timing, including missed log-after-own-73 paths.
- Fixed multiple AutoCQ duplicate-rework cases where the app could immediately call the same station again after a completed QSO.
- Fixed queue handoff carrying stale retry counters, reports, or DX state into the next caller.
- Fixed FT8 retry timeouts overcounting decode passes instead of real missed periods.
- Fixed world map / DX context staying on the last QSO after returning to CQ.

### Italiano

Release focalizzata su correttezza QSO FT8/FT4/FT2, stabilita' AutoCQ, evoluzione Quick QSO FT2, recovery audio all'avvio, sync decoder da upstream e infrastruttura iniziale certificati Decodium.

#### Aggiunto

- Aggiunta logica di recovery RX-audio all'avvio che puo' riaprire automaticamente gli stream configurati e riarmare il monitor.
- Aggiunto `lib/ft2/decode174_91_ft2.f90` e spostato il triggered decode FT2 sul decoder LDPC dedicato FT2.
- Aggiunto il toggle UI `Quick QSO` come alias del profilo FT2 `2 msg`.
- Aggiunto tracing diagnostico AutoCQ in `debug.txt`.
- Aggiunta integrazione certificati Decodium con load/autoload/stato nella finestra principale.
- Aggiunto `tools/generate_cert.py` per generare file certificato `.decodium`.

#### Modificato

- Versione UI locale, workflow release e naming release GitHub ora allineati alla semver `1.5.0`.
- Decoder LDPC condivisi allineati al refresh upstream Normalized Min-Sum.
- Il flow FT2 Quick QSO e' stato rifattorizzato nello schema corto corrente con gestione mixed-mode e backward compatibility.
- La documentazione release macOS/Linux ora copre target Tahoe/Sequoia/Monterey/AppImage con istruzioni avvio e requisiti piattaforma.

#### Corretto

- Corretti i casi di avvio in cui le periferiche audio erano selezionate correttamente ma l'RX partiva solo dopo aver riaperto Preferenze Audio.
- Corretti i QSO standard a 5 messaggi FT8/FT4/FT2 che saltavano il `73` finale locale quando il corrispondente mandava prima `RR73`/`73`.
- Corretti i tempi di log FT2 Quick QSO, inclusi i path che perdevano il log dopo il proprio `73`.
- Corretti molteplici casi AutoCQ di retrigger duplicato in cui l'app poteva richiamare subito lo stesso nominativo appena lavorato.
- Corretto l'handoff della queue che portava retry, report o stato DX sporchi nel caller successivo.
- Corretto il timeout retry FT8 che sovracontava i passaggi decode invece dei periodi realmente persi.
- Corretta la persistenza mappa mondo / contesto DX sull'ultimo QSO dopo il ritorno a CQ.

### Espanol

Release centrada en correccion de QSO FT8/FT4/FT2, estabilidad AutoCQ, evolucion Quick QSO FT2, recuperacion audio al arrancar, sincronizacion de decoders desde upstream e infraestructura inicial de certificados Decodium.

#### Anadido

- Anadida logica de recuperacion RX-audio al arranque capaz de reabrir automaticamente streams configurados y rearmar el monitor.
- Anadido `lib/ft2/decode174_91_ft2.f90` y movido el triggered decode FT2 al path LDPC dedicado FT2.
- Anadido el toggle UI `Quick QSO` como alias del perfil FT2 `2 msg`.
- Anadido tracing diagnostico AutoCQ en `debug.txt`.
- Anadida integracion de certificados Decodium con load/autoload/estado en la ventana principal.
- Anadido `tools/generate_cert.py` para generar ficheros `.decodium`.

#### Cambios

- Version UI local, workflows release y naming release GitHub alineados ahora con la semver `1.5.0`.
- Decoders LDPC compartidos alineados al refresh upstream Normalized Min-Sum.
- El flow FT2 Quick QSO fue refactorizado al esquema corto actual con manejo mixed-mode y backward compatibility.
- La documentacion release macOS/Linux cubre ahora Tahoe/Sequoia/Monterey/AppImage con guia de arranque y requisitos de plataforma.

#### Corregido

- Corregidos casos de arranque donde los dispositivos de audio estaban bien seleccionados pero RX solo empezaba tras reabrir Preferencias Audio.
- Corregidos QSO estandar de 5 mensajes FT8/FT4/FT2 que saltaban el `73` final local cuando la otra estacion enviaba antes `RR73`/`73`.
- Corregidos tiempos de log FT2 Quick QSO, incluidos paths que perdian el log tras el propio `73`.
- Corregidos varios casos AutoCQ de retrabajo duplicado en los que la app podia volver a llamar enseguida al mismo callsign.
- Corregido el handoff de cola que arrastraba retry, reportes o estado DX sucios al siguiente caller.
- Corregido el timeout retry FT8 que sobrecontaba pasadas decode en vez de periodos realmente perdidos.
- Corregida la persistencia del mapa mundo / contexto DX sobre el ultimo QSO despues de volver a CQ.

## [1.4.9] - 2026-03-17

Previous stable cycle. See the historical release notes for the prior detailed changelog.
