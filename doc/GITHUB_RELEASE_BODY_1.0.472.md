# Decodium 4 FT2 1.0.472

## English

Release highlights (`1.0.471 -> 1.0.472`):

- FT2-Link CPU and UI load reduction:
- FT2-Link now keeps the macOS legacy RX backend as the single RX owner when required, avoiding duplicate modern audio/spectrum capture paths.
- the QML spectrum timer is stopped in FT2-Link legacy RX mode when the backend waterfall already provides visual data.
- embedded legacy QWidget UI refreshes are paused while FT2-Link owns the main operating area.
- decorative and dock-highlight animations are disabled while FT2-Link mode is active.
- panadapter PCM repaint requests now share the same throttle budget as spectrum updates.
- FT2-Link waterfall refresh is throttled more aggressively while preserving the visual panadapter.
- FT2-Link RX stability:
- live wide decode is now limited to full rate only during an active wide session.
- outside a session, W500/W2300 scanning remains available but uses a slower opportunistic window to avoid high CPU on HF noise.
- W2300 live decode logs include profile, estimated center, offset, quality, rate, bytes, sample offset and symbol count.
- HELLO retry state is cleared once the session connects, preventing stale retry queues.
- W2300 acquisition has safer symbol-state bounds and optional threaded search through `DECODIUM_FT2LINK_W2300_SEARCH_THREADS`.
- CAT/Hamlib stability:
- Icom serial frequency polling uses adaptive backoff after repeated timeouts.
- startup avoids fragile frequency-resolution probes on adaptive Icom serial links.
- repeated `rig_get_freq` failures are logged and backed off instead of continuously saturating the event loop.
- CAT polling recovery resets the backoff once the rig answers again.
- layout/runtime cleanup:
- standard TX macro panel space is reduced/hidden in FT2-Link mode.
- FT2-Link status and mode-specific UI remain visible while the normal FT2 controls are not needed.
- release version files and Windows installer metadata are aligned to `1.0.472`.

Validation performed locally:

- `git diff --check`
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml test_ft2link test_ft2link_qml_adapter`
- `test_ft2link_qml_adapter decodeWorkerStopsCleanlyOnAdapterDestruction twoAdaptersRoundTripW2300DataAudio liveW2300RxMetricsAdaptNextPreparedRate`
- `test_ft2link w2300WaveformRoundTripsFrame w2300WaveformCorrectsLargeFrequencyOffset w2300AudioPipelineCompletesChunkedStream`

Release assets expected from GitHub Actions:

- `Decodium_1.0.472_Setup_x64.exe`
- `decodium4-ft2-1.0.472-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.472-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.472-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.472-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.472-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.472-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.472-linux-aarch64.AppImage`
- matching `.zip` and checksum files where produced by the platform workflow.

## Italiano

Punti principali (`1.0.471 -> 1.0.472`):

- Riduzione carico CPU/UI FT2-Link:
- su macOS FT2-Link mantiene il backend RX legacy come unico proprietario dell'audio quando serve, evitando catture audio/spectrum moderne duplicate.
- il timer spectrum QML viene fermato in FT2-Link legacy RX quando il waterfall del backend fornisce gia' i dati visuali.
- gli aggiornamenti UI del QWidget legacy embedded vengono sospesi mentre FT2-Link usa l'area operativa principale.
- animazioni decorative e highlight dock vengono disattivati mentre FT2-Link e' attivo.
- i repaint PCM del panadapter usano lo stesso budget throttled degli update spectrum.
- il refresh waterfall in FT2-Link viene limitato in modo piu' aggressivo senza perdere la vista panadapter.
- Stabilita' RX FT2-Link:
- il decode wide live gira a pieno rate solo durante una sessione wide attiva.
- fuori sessione lo scan W500/W2300 resta disponibile, ma usa una finestra opportunistica piu' lenta per non saturare la CPU sul rumore HF.
- i log W2300 live includono profilo, centro stimato, offset, qualita', rate, byte, sample offset e simboli.
- lo stato HELLO retry viene pulito quando la sessione si connette, evitando retry vecchi in coda.
- l'acquisizione W2300 ha controlli bounds piu' robusti e ricerca threaded opzionale con `DECODIUM_FT2LINK_W2300_SEARCH_THREADS`.
- Stabilita' CAT/Hamlib:
- il polling frequenza Icom seriale usa backoff adattivo dopo timeout ripetuti.
- lo startup evita probe fragili della risoluzione frequenza sui link Icom seriali adattivi.
- i fallimenti ripetuti di `rig_get_freq` vengono loggati e rallentati invece di saturare continuamente l'event loop.
- il backoff si resetta quando la radio torna a rispondere.
- Cleanup layout/runtime:
- lo spazio del pannello macro TX standard viene ridotto/nascosto in FT2-Link.
- stato e UI specifica FT2-Link restano visibili mentre i controlli normali FT2 non servono.
- file versione e metadati installer Windows allineati a `1.0.472`.

Validazione locale eseguita:

- `git diff --check`
- `cmake --build /Users/salvo/Desktop/Decodium4-build --parallel 4 --target decodium_qml test_ft2link test_ft2link_qml_adapter`
- `test_ft2link_qml_adapter decodeWorkerStopsCleanlyOnAdapterDestruction twoAdaptersRoundTripW2300DataAudio liveW2300RxMetricsAdaptNextPreparedRate`
- `test_ft2link w2300WaveformRoundTripsFrame w2300WaveformCorrectsLargeFrequencyOffset w2300AudioPipelineCompletesChunkedStream`

Asset attesi dai runner GitHub Actions:

- `Decodium_1.0.472_Setup_x64.exe`
- `decodium4-ft2-1.0.472-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.472-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.472-macos-ventura-x86_64.dmg`
- `decodium4-ft2-1.0.472-macos-sonoma-x86_64.dmg`
- `decodium4-ft2-1.0.472-macos-sequoia-x86_64.dmg`
- `decodium4-ft2-1.0.472-linux-x86_64.AppImage`
- `decodium4-ft2-1.0.472-linux-aarch64.AppImage`
- relativi `.zip` e checksum dove prodotti dal workflow della piattaforma.
