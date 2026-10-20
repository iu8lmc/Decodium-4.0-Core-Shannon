# Decodium 4 v1.0.697

## English (British)

### Highlights since v1.0.696

- Added the selectable Apple Liquid Glass presentation across Decodium windows and dialogs. Glass-capable platforms use a translucent surface, while classic themes and platforms without supported window transparency retain an opaque fallback.
- Applied a consistent Pop/Dock rule to the main panels: embedded panels remain opaque and readable; detached windows use the translucent Glass surface. This covers Live Map, DX Cluster, Full Spectrum, Signal RX, Waterfall, TX and FT2-Link, including the DX-Pedition floating-panel workspace.
- Improved detached-window handling and startup restoration. Floating panels now use dedicated frameless Glass shells where required, avoid showing over the startup splash, and keep their docked placeholders readable.
- Fixed the FT2-Link panel using a translucent background while embedded in the dashboard. It now enables the Glass surface only in its pop-out instance.
- Hardened TX dispatch state management. HALT, QSO completion, AutoCQ changes and delayed callbacks now invalidate stale requests through a common generation guard. A residual TX6 cannot restart CQ when AutoCQ is disabled unless there is a new explicit manual request.
- Added safer manual TX arming and stale-request rejection for deferred, queued and post-QSO paths, reducing the risk of an unintended transmission after a mode or QSO-state transition.
- Improved CW presentation: received and transmitted text are shown in separate areas with distinct colours, and transmitted macro text is visible in the CW window.
- Improved spectrum recovery after QSO logging and visual-layout transitions, so the panadapter producer can be re-armed without restarting audio capture, decoding or CAT.
- Refined Liquid Glass surfaces in settings, logs, radio controls, map/cluster windows, CW, RTTY, SSTV and other auxiliary windows while preserving the existing classic-theme appearance.
- Added regression coverage for Liquid Glass window gating, Pop/Dock surfaces, TX dispatch guards, AutoCQ policy, CW control and waterfall/QSO recovery.

### Verification

- macOS Qt build completed successfully for `decodium_qml` and the Liquid Glass regression test.
- Focused regression suite: 6/6 tests passed.
- `qmllint` completed with exit code 0; remaining diagnostics are existing non-fatal unqualified-access/layout warnings.
- Fresh macOS runtime smoke test completed with Qt 6.11, Metal, the GPU panadapter/waterfall path and the Live Map renderer active.

## Italiano

### Novità dalla v1.0.696

- Introdotta la presentazione Apple Liquid Glass selezionabile nelle finestre e nei dialoghi di Decodium. Sui sistemi compatibili viene usata una superficie traslucida; i temi classici e i sistemi senza trasparenza supportata mantengono il fallback opaco.
- Applicata una regola uniforme Pop/Dock ai pannelli principali: i pannelli inseriti nel dashboard restano opachi e leggibili, mentre le finestre staccate usano la superficie traslucida Glass. La regola copre Live Map, DX Cluster, Full Spectrum, Signal RX, Waterfall, TX e FT2-Link, compreso il workspace dei pannelli flottanti DX-Pedition.
- Migliorata la gestione delle finestre staccate e del ripristino all’avvio. I pannelli flottanti usano shell Glass frameless dedicate quando necessario, non vengono mostrati sopra lo splash iniziale e lasciano placeholder docked leggibili.
- Corretto FT2-Link: il pannello non è più traslucido quando è inserito nel dashboard e abilita Liquid Glass solo nella finestra pop-out.
- Rafforzata la gestione dello stato TX. HALT, completamento del QSO, modifiche ad AutoCQ e callback differiti invalidano ora le richieste obsolete tramite una guardia comune basata su generazione. Un TX6 residuo non può riavviare il CQ quando AutoCQ è disattivato, salvo una nuova richiesta manuale esplicita.
- Aggiunti l’abilitazione manuale TX sicura e il rifiuto delle richieste obsolete nei percorsi differiti, accodati e successivi al QSO, riducendo il rischio di trasmissioni involontarie dopo un cambio modo o stato QSO.
- Migliorata la visualizzazione CW: testo ricevuto e trasmesso sono separati e distinguibili con colori diversi; il testo effettivamente trasmesso dalle macro è visibile nella finestra CW.
- Migliorato il recupero dello spettro dopo il log di un QSO e dopo cambiamenti del layout, riarmando il produttore del panadapter senza riavviare audio, decodifica o CAT.
- Rifinite le superfici Liquid Glass in setup, log, controlli radio, mappe/cluster, CW, RTTY, SSTV e nelle altre finestre ausiliarie, mantenendo invariato l’aspetto dei temi classici.
- Aggiunta copertura di regressione per gating Liquid Glass, superfici Pop/Dock, guardie TX, policy AutoCQ, controllo CW e recupero Waterfall/QSO.

### Verifica

- Build Qt macOS completata correttamente per `decodium_qml` e per il test di regressione Liquid Glass.
- Suite di regressione focalizzata: 6/6 test superati.
- `qmllint` completato con codice 0; restano solo warning non bloccanti già presenti relativi ad accessi non qualificati/layout.
- Smoke test su macOS completato con Qt 6.11, Metal, percorso GPU del panadapter/waterfall e renderer GPU della Live Map attivi.
