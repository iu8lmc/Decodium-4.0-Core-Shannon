# Decodium 4 FT2 1.0.736

## English (UK)

This release collects the changes made since 1.0.735 to experimental SSTV, digital image exchange, remote sharing and application lifecycle handling.

### SSTV, FAX and Radiofax

- Improves experimental FAX480 acquisition and consecutive reception, including VIS leader and dropout handling, retained audio diagnostics and safer recovery after interruptions. Dayton 512 × 480, QSSTV-style 512 × 500 and VIS 85 profiles remain distinct because their timing and geometry differ.
- Extends experimental Radiofax reception and transmission with symmetric phasing, the WMO dead sector and an optional 30-second timed phasing mode following a recognised IOC start. Improves phasing-clock acquisition and reacquisition, signal-loss handling, stop detection, clock-offset tolerance and grayscale calibration. The timing option is explicitly selected; non-standard phasing lengths can affect the image boundary.
- Adds receive and transmit controls, status information and laboratory diagnostics for these options. Local A/B tests include separate Decodium sessions over virtual audio, independent WEFAX and FLdigi comparisons, and captured off-air excerpts. These do not replace radio, antenna or on-air testing.
- Documents additional AVT Narrow, QRM and Narrow-QRM A/B and interference investigations, including independent codec comparisons and remaining acquisition and raster limitations.

### HAMDRM and hybrid transfers

- Extends the experimental native HAMDRM controller, waveform processing, repair and replay diagnostics, and adds bounded hybrid image-reference handling with FTP upload and download support. The digital page exposes hybrid transfer state and guards a save selection against an inbox entry being replaced while a dialogue is open.
- Expands A/B component, application, FTP and independent QSSTV-stage checks, including selected controlled exchanges and recovery cases. These tests cover specific profiles and configurations; complete EasyPal interoperability, all QSSTV profiles, live uninstrumented repair and RF operation are not established.

### Encrypted remote sharing

- Extends experimental end-to-end encrypted sharing with encrypted envelopes and optional pinned sender signatures, secure key handling, recipient and sender identity rotation, trust replacement, replay and tamper rejection, durable queue and inbox handling, and additional sharing controls and diagnostics.
- Improves failure handling, key and pin lifecycle checks, acknowledgement and retry behaviour, and protection against modified or stale manifests. Local two-application A/B tests use isolated HTTPS providers and secure-store fixtures.
- Encryption remains experimental: there is no bundled production sharing service, independently verified fingerprint exchange remains necessary for authenticated senders, and local fixtures are not an external security audit or proof of every provider's behaviour.

### Application lifecycle and settings

- Improves FT4/FT8 hash-seeding lifecycle on mode changes, allowing obsolete work to be cancelled and the reusable worker to resume safely when returning to a mode.
- Revises RTL-SDR audio output thread creation and shutdown, protects against stale callbacks and makes the enabled-state cache respect the active settings profile. Adds profile-switch and restart coverage.
- Adds focused tests, fixtures and reproducible laboratories for SSTV, sharing, HAMDRM and profile handling, and updates native SSTV and test workflow configuration.
- Fixes a GCC warning treated as an error in the test-mode SDR lifecycle loop, restoring Linux release compilation.

### Compatibility and known limits

The Radiofax, FAX480, HAMDRM hybrid and encrypted sharing features remain experimental. A/B virtual-audio WEFAX and FLdigi tests passed for the documented profiles. QSSTV-Experimental 10.0's unmodified FAX480 receiver exceeded the detailed-image error threshold both for Decodium transmissions and for its own transmitter; a transmit-side shift would degrade edge detail, so this limitation is documented rather than concealed. No general EasyPal, all-profile QSSTV, public-provider or over-the-air interoperability is claimed. See `docs/sstv/` for the test conditions, results and remaining work.

## Italiano

Questa versione raccoglie le modifiche dalla 1.0.735 per SSTV sperimentale, scambio digitale di immagini, condivisione remota e gestione del ciclo di vita dell'applicazione.

### SSTV, FAX e Radiofax

- Migliora l'acquisizione FAX480 sperimentale e la ricezione consecutiva, inclusi il leader VIS, le interruzioni, la diagnostica audio conservata e il recupero dopo anomalie. I profili Dayton 512 × 480, QSSTV 512 × 500 e VIS 85 restano distinti perché hanno tempi e geometrie differenti.
- Estende la ricezione e la trasmissione Radiofax sperimentale con phasing simmetrico, settore morto WMO e modalità opzionale di phasing temporizzato a 30 secondi dopo il riconoscimento del segnale IOC. Migliora acquisizione e riacquisizione del clock, gestione della perdita del segnale, rilevamento dello stop, tolleranza agli scostamenti e calibrazione dei grigi. La modalità temporizzata è selezionata esplicitamente; durate di phasing non standard possono influire sull'inizio dell'immagine.
- Aggiunge controlli, stato e diagnostica di laboratorio. Le prove A/B locali comprendono sessioni Decodium separate su audio virtuale, confronti con WEFAX e FLdigi indipendenti ed estratti ricevuti via radio. Non sostituiscono prove con radio e antenna.
- Documenta ulteriori verifiche A/B e con interferenze per AVT Narrow, QRM e Narrow-QRM, confronti con codec indipendenti e limiti ancora aperti nell'acquisizione e nel raster.

### HAMDRM e trasferimenti ibridi

- Estende controller HAMDRM nativo sperimentale, elaborazione della forma d'onda, riparazione e diagnostica, e aggiunge gestione limitata dei riferimenti a immagini ibride con caricamento e scaricamento FTP. La pagina digitale mostra lo stato del trasferimento e protegge la selezione di salvataggio se l'elemento viene sostituito mentre la finestra è aperta.
- Amplia le prove A/B di componenti, applicazione e FTP e i confronti con stadi QSSTV indipendenti, inclusi scambi controllati e casi di recupero selezionati. Questi test riguardano profili e configurazioni specifici; non dimostrano interoperabilità EasyPal completa, tutti i profili QSSTV, riparazioni live senza strumentazione o funzionamento RF.

### Condivisione remota cifrata

- Estende la condivisione sperimentale cifrata end-to-end con buste cifrate, firme opzionali con identità del mittente verificata, gestione sicura delle chiavi, rotazione delle identità, sostituzione della fiducia, rifiuto di replay e manomissioni, gestione persistente di coda e posta in arrivo, controlli e diagnostica.
- Migliora la gestione degli errori, il ciclo di vita di chiavi e identità fidate, conferme e tentativi, e la protezione contro manifest modificati o non aggiornati. Le prove A/B con due applicazioni usano provider HTTPS e archivi sicuri isolati.
- La cifratura resta sperimentale: non è incluso un servizio di condivisione in produzione; per autenticare il mittente occorre verificare le impronte indipendentemente. I test locali non sono un audit di sicurezza esterno né dimostrano il comportamento di ogni provider.

### Ciclo di vita e impostazioni

- Migliora il caricamento degli hash FT4/FT8 durante il cambio modalità, annullando il lavoro superato e consentendo al worker riutilizzabile di riprendere quando si torna alla modalità.
- Rivede avvio e arresto del thread audio RTL-SDR, protegge dalle callback non più valide e mantiene la cache dell'abilitazione coerente con il profilo attivo. Aggiunge prove di cambio profilo e riavvio.
- Aggiunge test mirati, fixture e laboratori riproducibili per SSTV, condivisione, HAMDRM e profili, e aggiorna i workflow di test SSTV nativo.
- Corregge un avviso GCC trattato come errore nel ciclo di diagnostica SDR usato dai test, ripristinando la compilazione Linux per la release.

### Compatibilità e limiti noti

Radiofax, FAX480, HAMDRM ibrido e condivisione cifrata restano sperimentali. Le prove A/B su audio virtuale con WEFAX e FLdigi sono passate per i profili documentati. Il ricevitore FAX480 originale di QSSTV-Experimental 10.0 supera la soglia di errore sulle immagini dettagliate sia con trasmissioni Decodium sia con il proprio trasmettitore; uno spostamento in trasmissione degraderebbe i bordi, quindi il limite è documentato. Non viene dichiarata interoperabilità generale con EasyPal, tutti i profili QSSTV, provider pubblici o via radio. Le condizioni delle prove e il lavoro restante sono descritti in `docs/sstv/`.
