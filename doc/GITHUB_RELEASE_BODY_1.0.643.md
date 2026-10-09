# Decodium 4 v1.0.643

## English (UK)

Cumulative changes since **v1.0.639**. This release incorporates upstream v1.0.640–v1.0.642 and adds settings-persistence corrections, weather localisation and an SSTV receive-page rendering fix.

### Settings and profile persistence — issue #84

- Aligned 35 previously root-only bridge write paths with the active-profile read paths. These include FT2/FT4/FT8 sequencing options, manual-TX behaviour, sign-off/re-engagement settings, Auto Call and Target Call preferences, decode depth, filters, hold-frequency behaviour and decode colours.
- Named profiles retain their own settings under `MultiSettings/<profile>`. Saved profile values take precedence over conflicting values in the embedded legacy backend's current configuration when reading legacy-compatible settings and filter snapshots.
- Wanted-callsign alerts, spectrum update timing and remembered CAT connection state now read the intended profile rather than unrelated root values.
- UI scaling remains a machine-wide setting, as it is read before profile selection. UI style now uses the same base-application store for startup, display and saving. Intentionally global settings are not indiscriminately moved into profiles.
- Setup checkboxes explicitly interpret stored boolean text: a value such as `"false"` must not become a checked box merely because it is a non-empty string.
- Checkboxes, spin boxes and sliders save user edits rather than treating their initialisation or binding refresh as an edit. This reduces the risk of a saved value being overwritten while a Setup page is created.
- The final shutdown save is synchronous and includes CAT and DX Cluster managers. It does not depend on an asynchronous completion callback after the GUI event loop has stopped.
- The QML/legacy compatibility path uses the embedded backend's `decodium4.ini`, rather than an unrelated `ft2.ini` file. Preferences may legitimately live in several stores; credentials continue to use SecureSettings where applicable.
- Added a reproducible, per-page source inventory in `doc/ISSUE_84_SETTINGS_AUDIT.md`, a source-contract audit and an isolated cross-process persistence test.

Previously leaked root values are not automatically assigned to a guessed profile. Please review important radio, audio, logging and transmission settings after upgrading if they were previously affected.

### Weather and station telemetry — issue #85

- Removed hard-coded Italian sky descriptions. Weather conditions and update messages now use Decodium's selected application language, with entries for all 15 supplied language catalogues.
- The same translated sky descriptions are used for the weather preview and received station telemetry. Labels are translated at call time rather than cached permanently in the first language used.
- Weather/telemetry checkboxes no longer write preferences simply because they are being initialised. Disabled automatic weather is not presented as an available cached preview.
- “Poco nuvoloso” means “Partly cloudy”; it was a condition description, not a location. Weather coordinates are derived from the configured station locator, **not** the public IP address.

### SSTV receive-page buttons — issue #86

- Replaced style-dependent receive-page buttons with a shared plain-background SSTV control to address the reported chequered/background artefacts.
- Retained hover, pressed, disabled and keyboard-focus feedback, without relying on the native style's background effect.
- Added receive-page regression checks under Basic, Material and Fusion styles using offscreen software rendering. Confirmation on the reporting Linux system and its actual graphics driver is still required.

### Upstream changes incorporated after v1.0.639

- **v1.0.640:** improved crash/startup diagnostics, including processor capabilities, selected decoder/fallback, exception address/module and instruction bytes where supported. The safe CPU-dispatch work remains in place for processors without AVX2; this does not imply that every older CPU or graphics driver has been physically tested.
- Renamed the project's decoder from **fastldpc** to **superldpc** and clarified licence/protocol attribution. Corrected the placement of a declaration that had been confined to the Windows conditional block.
- Added experimental FT2 recent-sender/candidate memory and an optional whole-message hypothesis path. These remain opt-in through `DECODIUM_FT2_STORICO` and `DECODIUM_FT2_STORICO_AP`; no universal on-air sensitivity improvement is claimed here.
- Added **DecoLink/DecoLog integration**: external log information contributes to worked-before classification, log acknowledgements can be displayed, and cluster spots can enter Decodium's cluster/waterfall views. Tuning requests can select the frequency/mode and prepare the DX call; they do not start transmission.
- **v1.0.641:** tightened automatic sequencing for messages addressed to an unresolved `<...>` hash. Protocol/callsign compatibility is checked before contextual inference, reducing replies to messages intended for another station whilst retaining supported non-standard-call exchanges. The message remains visible.
- **v1.0.642:** corrected “TX Delay (s)” in Italian, German, Spanish and Danish: `(s)` denotes seconds, not a plural suffix. The underlying delay setting is unchanged.

### Verification and limits

The local `decodium_qml` target builds on Apple Silicon. Targeted settings, telemetry, CI-V, worked-before and SSTV UI tests are included in the verification. The settings test uses synthetic values in a temporary store and separate processes to check reopening, profile inheritance and isolation; it does not exercise every control through the complete application GUI. Full manual Setup restart coverage, real-radio confirmation and Windows/Linux runtime validation are not claimed. The settings and graphics issues should remain open for reporter confirmation rather than being closed solely on source inspection.

### Downloads

Source ZIP and tar.gz archives correspond to tag `v1.0.643`. Publishing workflows attach binaries as they complete:

- Windows x64 installer: `Decodium_1.0.643_Setup_x64.exe` (unsigned).
- Linux x86_64 and aarch64 AppImages, each with a SHA-256 file.
- macOS Apple Silicon DMGs for Sequoia and Tahoe, with checksums.
- macOS Intel DMGs for Ventura, Sonoma and Sequoia, with checksums.

An absent asset means that its build/publication has not completed successfully; the existence of this release alone is not proof that every binary is ready.

---

## Italiano

Modifiche cumulative dalla **v1.0.639**. Questo rilascio incorpora le versioni upstream v1.0.640–v1.0.642 e aggiunge correzioni alla persistenza delle impostazioni, alla lingua del meteo e al rendering della pagina di ricezione SSTV.

### Salvataggio delle impostazioni e profili — issue #84

- Allineati 35 percorsi del bridge che scrivevano soltanto nell'archivio generale, mentre la lettura avveniva dal profilo attivo. Sono coinvolte opzioni di sequenziamento FT2/FT4/FT8, TX manuale, chiusura/ripresa del QSO, Auto Call e Target Call, profondità di decodifica, filtri, mantenimento della frequenza e colori dei decode.
- I profili nominati conservano le proprie impostazioni sotto `MultiSettings/<profilo>`. Nella lettura delle opzioni compatibili con il backend legacy e dei filtri, i valori salvati nel profilo hanno precedenza sui valori contrastanti della configurazione corrente del backend incorporato.
- Avvisi Wanted callsign, velocità di aggiornamento dello spettro e memoria della connessione CAT leggono ora il profilo previsto, non valori estranei dell'archivio generale.
- La scala dell'interfaccia resta globale per il computer, perché viene letta prima della selezione del profilo. Lo stile UI usa ora lo stesso archivio dell'applicazione base per avvio, visualizzazione e salvataggio. Le preferenze volutamente globali non vengono trasferite indiscriminatamente nei profili.
- Le caselle del Setup interpretano esplicitamente i valori booleani testuali: `"false"` non deve diventare una casella attiva soltanto perché è una stringa non vuota.
- Caselle, campi numerici e slider salvano le modifiche dell'utente, senza considerare un aggiornamento del binding o l'inizializzazione come una modifica. Questo riduce il rischio di sovrascrivere valori salvati durante la creazione di una pagina del Setup.
- Il salvataggio finale alla chiusura è sincrono e comprende i gestori CAT e DX Cluster: non dipende da un callback asincrono quando il ciclo eventi della GUI è già terminato.
- Il collegamento QML/legacy usa `decodium4.ini`, lo stesso file del backend incorporato, invece di un `ft2.ini` estraneo. È previsto l'uso di più archivi; dove applicabile, le credenziali continuano ad essere gestite da SecureSettings.
- Aggiunti inventario del sorgente per pagina in `doc/ISSUE_84_SETTINGS_AUDIT.md`, controllo riproducibile dei percorsi e test isolato di persistenza fra processi.

I valori precedentemente finiti nell'archivio generale non vengono assegnati automaticamente a un profilo presunto. Dopo l'aggiornamento, controllare le impostazioni importanti di radio, audio, log e trasmissione se erano state interessate dal problema.

### Meteo e telemetria di stazione — issue #85

- Eliminate le descrizioni del cielo scritte direttamente in italiano. Condizioni meteo e messaggi di aggiornamento usano la lingua selezionata in Decodium, con traduzioni nei 15 cataloghi distribuiti.
- Le descrizioni tradotte vengono usate sia nell'anteprima meteo sia nella telemetria di stazione ricevuta. La traduzione avviene al momento dell'uso, senza mantenere permanentemente la prima lingua utilizzata.
- Le caselle meteo/telemetria non riscrivono le preferenze durante l'inizializzazione. Quando il meteo automatico è disattivato, la vecchia anteprima in cache non viene dichiarata disponibile.
- “Poco nuvoloso” descriveva il cielo, non la località. Le coordinate meteo derivano dal locator configurato della stazione, **non** dall'indirizzo IP pubblico.

### Pulsanti della pagina di ricezione SSTV — issue #86

- Sostituiti i pulsanti dipendenti dallo stile con un controllo SSTV condiviso a sfondo semplice, per affrontare gli artefatti a scacchiera segnalati.
- Conservati gli stati al passaggio del mouse, pressione, disabilitazione e focus da tastiera, senza dipendere dagli effetti di sfondo dello stile nativo.
- Aggiunti controlli di regressione della pagina negli stili Basic, Material e Fusion, con rendering software offscreen. Resta necessaria la conferma sul sistema Linux e sul driver grafico dell'utente che ha segnalato il problema.

### Modifiche upstream incorporate dopo la v1.0.639

- **v1.0.640:** diagnostiche di crash e avvio più informative, con capacità del processore, decoder/fallback scelto, indirizzo/modulo dell'eccezione e byte dell'istruzione dove supportato. Resta presente il dispatch sicuro per le CPU senza AVX2; questo non equivale a un collaudo fisico di ogni vecchia CPU o driver grafico.
- Decoder del progetto rinominato da **fastldpc** a **superldpc**, con chiarimenti su licenze e attribuzione del protocollo. Corretta anche una dichiarazione che era stata confinata al blocco condizionale Windows.
- Aggiunta memoria sperimentale dei mittenti/candidati recenti in FT2 e un percorso opzionale basato sull'ipotesi del messaggio completo. Restano opt-in tramite `DECODIUM_FT2_STORICO` e `DECODIUM_FT2_STORICO_AP`; non viene dichiarato un miglioramento universale della sensibilità in aria.
- Aggiunta integrazione **DecoLink/DecoLog**: informazioni del log esterno contribuiscono alla classificazione dei contatti già lavorati, possono essere visualizzate conferme di registrazione e gli spot entrano nelle viste cluster/waterfall. Le richieste di sintonia possono scegliere frequenza/modo e preparare il nominativo DX; non avviano la trasmissione.
- **v1.0.641:** sequenziamento automatico più restrittivo per messaggi destinati a un hash `<...>` non risolto. La compatibilità fra protocollo e nominativi viene controllata prima della deduzione dal contesto, riducendo le risposte a messaggi per altre stazioni senza eliminare gli scambi supportati con nominativi non standard. Il messaggio resta visibile.
- **v1.0.642:** corretta l'etichetta “TX Delay (s)” in italiano, tedesco, spagnolo e danese: `(s)` indica i secondi, non un suffisso plurale. Il comportamento del ritardo non cambia.

### Verifiche e limiti

Il target locale `decodium_qml` compila su Apple Silicon. La verifica comprende test mirati su impostazioni, telemetria, CI-V, contatti già lavorati e interfaccia SSTV. Il test delle impostazioni usa valori sintetici in un archivio temporaneo e processi separati per controllare riapertura, ereditarietà e isolamento dei profili; non aziona ogni controllo attraverso la GUI completa. Non vengono dichiarati completati il collaudo manuale di tutte le schermate dopo riavvio, la conferma con radio reali o il collaudo runtime Windows/Linux. Le issue su impostazioni e grafica restano da confermare con gli utenti, non da chiudere sulla sola ispezione del codice.

### Download

Gli archivi sorgente ZIP e tar.gz corrispondono al tag `v1.0.643`. I workflow aggiungono i binari al completamento:

- Installer Windows x64: `Decodium_1.0.643_Setup_x64.exe` (non firmato).
- AppImage Linux x86_64 e aarch64, ciascuna con file SHA-256.
- DMG macOS Apple Silicon per Sequoia e Tahoe, con checksum.
- DMG macOS Intel per Ventura, Sonoma e Sequoia, con checksum.

Un allegato assente indica che la sua compilazione/pubblicazione non è terminata con successo: la sola presenza della release non certifica che tutti i binari siano pronti.

---

Full source comparison / Confronto completo: https://github.com/elisir80/Decodium-4.0-Core-Shannon/compare/v1.0.639...v1.0.643
