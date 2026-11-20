# Decodium 4 FT2 1.0.1005

## English (UK)

This release adds support for Club Log personal tokens and automatic QSO upload, and strengthens credential storage and request handling.

- Added optional automatic upload of newly logged QSOs to Club Log. Native ADIF logging and legacy-backend mirrored records enqueue contacts only after the local log is successfully written.
- Added a persistent SQLite outbox with station-specific processing, duplicate suppression, batching, upload status, rejected-contact retry and reconciliation of uncertain results before resending. The interface warns users to enable automatic upload in only one application when also using DecoDXLog.
- Added Club Log REST authentication using personal `cl_` tokens, whilst retaining the legacy authentication path for legacy lookups and OQRS. REST OQRS handling supports pagination and refreshes the stored snapshot; an OQRS request is not treated as confirmation of a QSL card.
- Added persistent handling of rejected REST credentials and rate limits: refused tokens are blocked from further requests and retry timing is retained per token. Sensitive tokens are excluded from response handling where detected.
- Personal tokens use protected credential storage and are not saved as plaintext when secure storage is unavailable; users are told when the token must be entered again after restart. Existing plaintext personal tokens are migrated when possible.
- On macOS, secure settings now use the native Security framework and Keychain API instead of invoking the `security` command, and the required framework is linked into the application and legacy components.
- Expanded Settings with the personal-token and station guidance, automatic-upload control, queue status and rejected-QSO retry. Added regression coverage for credential storage, request authentication, refusals, rate limiting, OQRS and the upload queue.
- Updated the application version to 1.0.1005.

The macOS application and the focused secure-settings and callsign-intelligence tests built successfully; both tests passed with simulated services. These checks do not establish interoperability with a live Club Log account or validate physical radio operation.

## Italiano

Questa versione aggiunge il supporto ai token personali Club Log e all'invio automatico dei QSO, rafforzando la protezione delle credenziali e la gestione delle richieste.

- Aggiunto l'invio automatico facoltativo a Club Log dei nuovi QSO registrati. La registrazione ADIF nativa e i record acquisiti dal backend legacy inseriscono i contatti nella coda solo dopo il salvataggio riuscito nel log locale.
- Aggiunta una coda SQLite persistente con gestione per nominativo di stazione, prevenzione dei duplicati, invio a gruppi, stato della coda, nuovo tentativo dei contatti rifiutati e verifica dei risultati incerti prima di reinviare. L'interfaccia invita ad attivare l'invio automatico in una sola applicazione quando si usa anche DecoDXLog.
- Aggiunta l'autenticazione REST Club Log tramite token personali `cl_`, mantenendo il percorso di autenticazione legacy per le relative ricerche e OQRS. La gestione REST di OQRS supporta la paginazione e aggiorna l'istantanea locale; una richiesta OQRS non viene interpretata come conferma della cartolina QSL.
- Aggiunta la gestione persistente di credenziali REST rifiutate e limiti di frequenza: i token rifiutati vengono fermati e il momento del nuovo tentativo viene conservato per ciascun token. I token sensibili vengono esclusi dalla gestione delle risposte quando rilevati.
- I token personali usano l'archiviazione protetta e non vengono salvati in chiaro se questa non è disponibile; l'utente viene avvisato quando deve reinserire il token dopo il riavvio. I token personali già presenti in chiaro vengono migrati quando possibile.
- Su macOS le impostazioni sicure usano ora il framework Security e l'API Keychain nativa invece di avviare il comando `security`; il framework necessario è collegato all'applicazione e ai componenti legacy.
- Ampliate le impostazioni con indicazioni su token personale e stazione, controllo dell'invio automatico, stato della coda e ripetizione dei QSO rifiutati. Aggiunti test di regressione per protezione delle credenziali, autenticazione, rifiuti, limiti, OQRS e coda di invio.
- Aggiornata la versione dell'applicazione alla 1.0.1005.

L'applicazione macOS e i test mirati sulle impostazioni sicure e sull'intelligenza dei nominativi sono stati compilati; entrambi i test sono passati con servizi simulati. Queste verifiche non dimostrano l'interoperabilità con un account Club Log reale né il funzionamento con una radio fisica.
