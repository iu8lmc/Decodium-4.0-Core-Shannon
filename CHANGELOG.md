# Changelog / Registro Modifiche

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
