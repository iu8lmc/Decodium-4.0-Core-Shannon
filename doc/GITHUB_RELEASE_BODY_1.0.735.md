# Decodium 4 FT2 1.0.735

## English (UK)

This release includes the work since 1.0.734 on update checks and the experimental SSTV, radiofax, HAMDRM and remote-sharing subsystems.

### Update checks
- A failed or incomplete GitHub check no longer records a successful check and delays the next automatic attempt by 24 hours. Both repositories are checked in order: elisir80 first, then iu8lmc when needed. A failed source is no longer reported as proof that the application is up to date.
- Release versions are validated, network requests have a timeout, and the version label in the update dialogue is positioned within its badge. Automated tests cover source ordering, fallback, errors and update selection.

### SSTV and radiofax
- Integrated AVT Narrow, QRM and Narrow-QRM mode handling in transmit and receive paths, including VIS identity, picture mapping, acquisition and discontinuity handling. Studio controls and A/B and interference diagnostics have been expanded.
- Expanded experimental FAX480 and radiofax reception and transmission, including phasing, start and stop detection, frequency offsets, continuous reception, partial-frame handling and receiver configuration. Updated the receive and transmit pages and lab instrumentation.
- Local two-application and independent-peer investigations are documented, including both successful and negative observations. FAX480 and FLdigi/QSSTV alignment and broad on-air interoperability remain experimental and are not claimed as fully verified.

### HAMDRM
- Improved native BSR parsing, repair coordination, waveform handling and digital-page state. Added and extended tests and lab tools for lost segments, repeated requests, live repair, Reed–Solomon and independent QSSTV-derived vectors.
- Compatibility documentation records the bounded native implementation and observed QSSTV/EasyPal-related tests. Complete interoperability across QSSTV/EasyPal versions, profiles, real radio and RF conditions is not established.

### Remote image sharing and security
- Extended the experimental encrypted-sharing implementation, identity/key management, encrypted envelopes, optional sender signatures and sender policies, queue and restart handling, expiration, replay and tampering defences, and sharing-page controls. Added P-256 support and improved platform-specific secret-store coordination and error handling.
- Added A/B local application transfer, negative/tampering, storage and backend diagnostics. The protocol has not received an independent security review; platform, keyring and packaged-release results must not be inferred from tests on another system.

### Tests and documentation
- Expanded unit, QML, fuzz-smoke and application-level tests; added reproducible virtual-audio, FLdigi and QSSTV investigation tools, fixtures and diagnostic patches. Detailed results and limitations are in `docs/sstv/`.
- Existing Windows runtime dependency packaging fixes from 1.0.734 remain included. Source archives are attached automatically by GitHub; platform installers and AppImages are supplied as build assets as they become available.

## Italiano

Questa release comprende il lavoro successivo alla 1.0.734 sui controlli degli aggiornamenti e sui sottosistemi sperimentali SSTV, radiofax, HAMDRM e condivisione remota.

### Controllo degli aggiornamenti
- Un controllo GitHub fallito o incompleto non viene più registrato come riuscito rinviando di 24 ore il tentativo automatico successivo. I repository sono controllati nell'ordine elisir80, quindi iu8lmc quando necessario. Una sorgente non raggiungibile non viene più presentata come prova che l'applicazione sia aggiornata.
- Le versioni delle release vengono validate, le richieste hanno un timeout e l'etichetta della versione nella finestra di aggiornamento è posizionata nel relativo riquadro. I test coprono ordine, fallback, errori e selezione degli aggiornamenti.

### SSTV e radiofax
- Integrata la gestione AVT Narrow, QRM e Narrow-QRM nei percorsi di trasmissione e ricezione, con identità VIS, mappatura delle immagini, acquisizione e gestione delle discontinuità. Ampliati i controlli Studio e gli strumenti diagnostici A/B e per le interferenze.
- Estesi ricezione e trasmissione sperimentali FAX480 e radiofax: fasatura, rilevamento di avvio e arresto, scostamenti di frequenza, ricezione continua, immagini parziali e configurazione del ricevitore. Aggiornate le pagine di ricezione e trasmissione e la strumentazione di laboratorio.
- Documentate prove locali con due applicazioni e indagini con programmi indipendenti, comprese osservazioni positive e negative. L'allineamento FAX480 e FLdigi/QSSTV e l'interoperabilità generale via radio restano sperimentali e non sono dichiarati completamente verificati.

### HAMDRM
- Migliorati interpretazione BSR, coordinamento delle riparazioni, gestione della forma d'onda e stato della pagina digitale. Estesi test e strumenti per segmenti persi, richieste ripetute, riparazione dal vivo, Reed–Solomon e vettori indipendenti derivati da QSSTV.
- La documentazione descrive il sottoinsieme nativo e le prove relative a QSSTV/EasyPal. Non è dimostrata l'interoperabilità completa fra versioni, profili, apparati e condizioni RF.

### Condivisione remota e sicurezza
- Estesi la condivisione cifrata sperimentale, la gestione di identità e chiavi, gli involucri cifrati, le firme opzionali e le politiche per il mittente, la coda e il ripristino, le difese contro scadenza, ripetizione e manomissione e i controlli della pagina. Aggiunto il supporto P-256 e migliorate coordinazione ed errori degli archivi di segreti specifici delle piattaforme.
- Aggiunte prove A/B di trasferimento locale, test negativi e di manomissione e diagnostica degli archivi. Il protocollo non ha ancora ricevuto una revisione di sicurezza indipendente; i risultati su una piattaforma non dimostrano quelli su altre piattaforme, portachiavi o pacchetti distribuiti.

### Test e documentazione
- Ampliati test unitari, QML, fuzz-smoke e a livello di applicazione; aggiunti strumenti riproducibili per audio virtuale, FLdigi e QSSTV, fixture e patch diagnostiche. Risultati e limiti dettagliati sono in `docs/sstv/`.
- Rimangono incluse le correzioni alle dipendenze dell'installer Windows della 1.0.734. GitHub allega automaticamente gli archivi sorgente; installer e AppImage vengono aggiunti come artefatti delle build quando disponibili.
