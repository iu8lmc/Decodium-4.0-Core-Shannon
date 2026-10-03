# Decodium 4 v1.0.645

## English (UK)

Changes since **v1.0.643**, including upstream v1.0.644 and the local fixes collected in v1.0.645.

### Update window and localisation — upstream v1.0.644

- The update notice now keeps its contents within its own frame. Its layout determines its height, bounded by the hosting window, with separate header and footer areas.
- Release notes are rendered as formatted text in a framed, scrollable panel instead of displaying raw Markdown over the decode list.
- Colours follow the application theme; the new version is highlighted and the buttons follow the application's styling.
- The “Update available: v…” status is translatable, with updated source and compiled translation catalogues. Other existing updater status messages are not yet all localised; this release does not claim complete updater localisation.
- A diagnostic override, `DECODIUM_UPDATE_FAKE_CURRENT`, allows developers to exercise the update notice without waiting for a new release and bypasses the daily check limit. Normal operation is unchanged when it is unset.
- The cross-process settings persistence test accounts for case-insensitive Windows INI keys, avoiding false comparisons between intentional key pairs differing only in case.

### Consistent UDP client identity — v1.0.645

- The default primary UDP client ID is now **Decodium** in the dashboard and both modern and legacy backends. Explicitly configured compatibility IDs, including **WSJTX**, remain supported rather than being forcibly replaced.
- The effective primary, secondary and tertiary IDs are passed to the embedded legacy backend before its first heartbeat, preventing an initial stale identity from being advertised.
- Changes to the dashboard IDs are propagated to the legacy backend's actual settings object, including when the active profile and backend settings stores differ.
- UDP reconfiguration refreshes legacy identities even in native modes such as RTTY. The standalone UDP sender remains suppressed while the legacy TX backend owns reporting, retaining the existing protection against duplicate reporting.
- A regression test exercises changing an existing WSJTX client to Decodium and explicitly switching it back, checking the heartbeat identity.

### Safer network reply handling — v1.0.645

- Completed network replies are read only when they report no network error and are still readable, avoiding attempts to read closed or failed replies.
- This applies to the updater's final download read, remote callsign and confirmation database lookups, POTA and geographic map data, the IOTA catalogue, satellite TLE downloads, CTY/CALL3 data and the LoTW user list.
- Existing error handling remains in place. These guards do not resolve external server outages or guarantee that every remote service is available.

### Downloads and verification scope

- Source ZIP and tar.gz archives are provided by GitHub for this tag.
- Release workflows publish the Windows x64 installer, Linux x86_64 and aarch64 AppImages, Apple Silicon DMGs for Sequoia/Tahoe, and Intel DMGs for Ventura/Sonoma/Sequoia. Binary files appear as their respective workflows finish; macOS and Linux packages include checksum files.
- Build and automated-test results are distinct from live radio and third-party logger validation. The changes do not imply that every radio, operating-system configuration or external logger has been tested.
- Existing user settings are retained. Check the UDP client ID expected by your logging software; select WSJTX explicitly if that software requires this compatibility identity.

---

## Italiano

Modifiche dalla **v1.0.643**, comprendenti la versione upstream v1.0.644 e le correzioni locali raccolte nella v1.0.645.

### Finestra di aggiornamento e traduzioni — upstream v1.0.644

- L'avviso di aggiornamento mantiene ora il contenuto nel proprio riquadro. Il layout ne determina l'altezza entro i limiti della finestra ospitante, con intestazione e area dei pulsanti separate.
- Le note di rilascio vengono visualizzate come testo formattato in un pannello con scorrimento, anziché mostrare Markdown grezzo sopra la lista delle decodifiche.
- I colori seguono il tema dell'applicazione; la nuova versione è evidenziata e i pulsanti rispettano lo stile del programma.
- Lo stato «Update available: v…» è traducibile, con cataloghi sorgente e compilati aggiornati. Altri messaggi preesistenti dell'aggiornamento non sono ancora tutti localizzati: questa versione non dichiara una traduzione completa dell'updater.
- La variabile diagnostica `DECODIUM_UPDATE_FAKE_CURRENT` consente agli sviluppatori di provare l'avviso senza attendere una nuova release e supera il limite giornaliero del controllo. Se non impostata, il funzionamento normale resta invariato.
- Il test di persistenza delle impostazioni fra processi tiene conto delle chiavi INI Windows non sensibili alle maiuscole, evitando confronti errati fra coppie intenzionali di chiavi che differiscono soltanto per la grafia.

### Identità UDP coerente — v1.0.645

- L'ID predefinito del client UDP primario è ora **Decodium** nella dashboard e nei backend moderno e legacy. Gli ID di compatibilità configurati esplicitamente, incluso **WSJTX**, restano utilizzabili e non vengono sostituiti forzatamente.
- Gli ID effettivi delle tre destinazioni UDP vengono trasmessi al backend legacy incorporato prima del suo primo heartbeat, evitando l'annuncio iniziale di un'identità non aggiornata.
- Le modifiche agli ID nella dashboard vengono propagate all'archivio effettivamente usato dal backend legacy, anche quando profilo attivo e archivio del backend differiscono.
- La riconfigurazione UDP aggiorna le identità legacy anche nei modi nativi come RTTY. Il mittente UDP autonomo resta disabilitato quando il backend TX legacy gestisce il reporting, mantenendo la protezione esistente dalle duplicazioni.
- Un test di regressione verifica il passaggio di un client esistente da WSJTX a Decodium e il ritorno esplicito a WSJTX, controllando l'identità negli heartbeat.

### Gestione più sicura delle risposte di rete — v1.0.645

- Le risposte concluse vengono lette soltanto se non riportano errori di rete e risultano ancora leggibili, evitando letture su risposte chiuse o fallite.
- La protezione riguarda la lettura finale del download dell'aggiornamento, le ricerche remote di nominativi e database di conferme, POTA e dati geografici della mappa, catalogo IOTA, download TLE dei satelliti, dati CTY/CALL3 e lista utenti LoTW.
- La gestione degli errori già presente rimane attiva. Questi controlli non risolvono indisponibilità dei server esterni e non garantiscono l'accessibilità di ogni servizio remoto.

### Download e limiti delle verifiche

- GitHub fornisce gli archivi sorgente ZIP e tar.gz associati al tag.
- I workflow pubblicano l'installer Windows x64, le AppImage Linux x86_64 e aarch64, i DMG Apple Silicon per Sequoia/Tahoe e i DMG Intel per Ventura/Sonoma/Sequoia. I binari compaiono al termine dei rispettivi workflow; i pacchetti macOS e Linux includono i file di checksum.
- Compilazione e test automatici sono distinti dalla verifica con radio reali e logger esterni. Le modifiche non implicano che siano stati provati tutti i modelli di radio, le configurazioni dei sistemi operativi o i programmi di log.
- Le impostazioni dell'utente vengono conservate. Controllare quale ID UDP richiede il proprio logger e selezionare esplicitamente WSJTX se necessario per compatibilità.
