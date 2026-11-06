# Decodium 1.0.636

## English (UK)

Changes since **1.0.635**. This maintenance release focuses on SuperFox transitions, dashboard layout and Wanted Callsign alerts.

### SuperFox and Hound controls

- Switching SuperFox off from the dashboard now also leaves Hound mode, avoiding the state in which Hound could remain stuck on. Switching it on still enables Hound reception when neither Fox nor Hound is already selected; an explicitly selected Fox role is preserved.
- Reordered the transition so that the SuperFox decoder option is changed outside Hound mode, avoiding an unnecessary intermediate ordinary-Hound configuration.
- Protected synchronisation between the dashboard and the legacy backend against synchronous status callbacks reading a partially updated operating activity.
- Fox/Hound changes are blocked during transmission or tuning, both in the settings controls and in the bridge.
- When reception moves to the legacy backend, pending native decode results are invalidated and native deduplication state is reset. An already active legacy receive path is no longer unnecessarily restarted.

### Smoother mode changes

- Removed a redundant full-profile save from the SuperFox button. The relevant setters continue to persist their own settings.
- Special-activity and SuperFox changes now write their own configuration keys instead of rewriting unrelated settings, including credential/keychain entries.
- Removed duplicate backend synchronisation and added diagnostic timing for FT8/SuperFox reconfiguration (`SPECIALOP-TIMING`). These changes reduce avoidable UI/panadapter pauses; a brief decoder reconfiguration may still be noticeable.

### Dashboard layout

- The docked TX panel now reserves its content height in both the upper and lower dashboard positions, preventing TX1–TX6 from being clipped by an undersized container.
- The lower resize handle updates the saved panel height without breaking its height binding, and persists the value when dragging finishes or is cancelled.

### Wanted Callsign alerts

- Fixed the deferred native decode alert path to pass the decoded message to the alert matcher. Without that payload, Wanted Callsign matching could not identify the wanted station even though its decode was displayed.
- Made the message argument mandatory for alert callers. Existing CQ/My Call filtering and decode deduplication are unchanged. This addresses the identified payload defect associated with issue #83, not every possible alert configuration problem.

### Verification and scope

- Added QML regression coverage for repeated SuperFox on/off transitions, preservation of an explicit Fox role, TX/Tune guards and avoiding an implicit change from FT4.
- Local macOS `decodium_qml` build and the focused QML tests are checked before publication. The QML tests use a simulated backend; they are not an on-air or cross-platform radio validation.
- The reported missing log prompt/restarted exchange after a received 73 remains under investigation and is **not** claimed fixed here. AutoCQ burst/watchdog behaviour is not changed in this release.

### Downloads

Source archives are provided by GitHub. Packaging workflows publish the Windows x64 installer, Linux x86_64 and aarch64 AppImages, and macOS Apple Silicon (Sequoia/Tahoe) and Intel (Ventura/Sonoma/Sequoia) DMGs. Binaries and checksum files appear as their respective workflows complete; a published release page alone does not mean every build has finished.

---

## Italiano

Modifiche rispetto alla **1.0.635**. Questa release di manutenzione riguarda le transizioni SuperFox, il layout della dashboard e gli avvisi Wanted Callsign.

### Comandi SuperFox e Hound

- Disattivando SuperFox dalla dashboard si esce ora anche da Hound, evitando che Hound rimanga bloccato attivo. Attivando SuperFox viene ancora abilitata la ricezione Hound quando non è già selezionato Fox o Hound; il ruolo Fox selezionato esplicitamente viene mantenuto.
- Riordinata la transizione: l'opzione del decoder SuperFox viene modificata fuori dalla modalità Hound, evitando una configurazione intermedia Hound ordinaria non necessaria.
- Protetta la sincronizzazione fra dashboard e backend legacy dalle notifiche sincrone che potevano leggere un'attività operativa aggiornata solo parzialmente.
- Bloccati i cambi Fox/Hound durante trasmissione o Tune, sia nei controlli delle impostazioni sia nel bridge.
- Nel passaggio della ricezione al backend legacy vengono invalidati i risultati nativi ancora in elaborazione e azzerato lo stato di deduplicazione nativo. Non viene più riavviata inutilmente una ricezione legacy già attiva.

### Cambi di modalità più fluidi

- Eliminato un salvataggio completo e ridondante del profilo dal pulsante SuperFox. I rispettivi setter continuano a salvare le proprie impostazioni.
- Le modifiche all'attività speciale e a SuperFox aggiornano ora soltanto le relative chiavi, senza riscrivere impostazioni estranee, comprese quelle di credenziali/portachiavi.
- Eliminata una sincronizzazione duplicata del backend e aggiunta la misura diagnostica dei tempi di riconfigurazione FT8/SuperFox (`SPECIALOP-TIMING`). Si riducono così le pause evitabili dell'interfaccia e del panadapter; può restare percepibile una breve riconfigurazione del decoder.

### Layout della dashboard

- Il pannello TX agganciato riserva ora l'altezza dei propri contenuti sia nella posizione superiore sia in quella inferiore, evitando il taglio dei pulsanti TX1–TX6 quando il contenitore è troppo basso.
- La maniglia inferiore aggiorna l'altezza salvata senza interrompere il collegamento dinamico dell'altezza e memorizza il valore al termine o all'annullamento del trascinamento.

### Avvisi Wanted Callsign

- Corretto il percorso differito degli avvisi delle decodifiche native: ora passa il messaggio decodificato al riconoscimento degli avvisi. Senza questo contenuto, Wanted Callsign non poteva riconoscere la stazione cercata anche quando la decodifica era visualizzata.
- Reso obbligatorio il messaggio per chi richiama gli avvisi. Filtri CQ/My Call e deduplicazione restano invariati. La correzione riguarda il difetto identificato in relazione alla issue #83, non qualsiasi possibile problema di configurazione degli avvisi.

### Verifiche e ambito

- Aggiunti test QML per attivazioni/disattivazioni ripetute di SuperFox, mantenimento del ruolo Fox esplicito, protezioni TX/Tune e assenza di cambi impliciti da FT4.
- Prima della pubblicazione vengono verificati la build locale macOS `decodium_qml` e i test QML mirati. Questi test usano un backend simulato: non equivalgono a prove radio in aria o multipiattaforma.
- La segnalazione relativa al mancato prompt di log o alla ripartenza del collegamento dopo un 73 ricevuto resta in analisi e **non** viene dichiarata risolta. Il comportamento burst/watchdog di AutoCQ non cambia in questa versione.

### Download

Gli archivi dei sorgenti sono forniti da GitHub. I workflow pubblicano installer Windows x64, AppImage Linux x86_64 e aarch64 e DMG macOS Apple Silicon (Sequoia/Tahoe) e Intel (Ventura/Sonoma/Sequoia). Binari e checksum compaiono al completamento dei rispettivi workflow: la sola presenza della pagina release non indica che tutte le build siano terminate.
