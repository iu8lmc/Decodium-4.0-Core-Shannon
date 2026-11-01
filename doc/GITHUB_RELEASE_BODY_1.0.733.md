# Decodium 4 v1.0.733

## English (British)

This release includes the changes since v1.0.728, including the upstream v1.0.732 updates and the continuing experimental SSTV work. The SSTV interoperability and security limitations below are part of the release description.

### FT2, pile-up and display

- PILEUP adds an optional daily maximum of 1, 2 or 3 QSOs per callsign, band and mode; zero disables the limit. Today's counts are read from the log at startup.
- FT2 waits at least two periods before logging a QSO after a single RR73. AutoCQ and pile-up avoid reopening a recently worked or logged station for ten minutes, including late reports. A stale CQ is not repeatedly transmitted after AutoCQ stops.
- Full Spectrum no longer reuses display rows in a way that causes overlapping text during rapid updates.

### SSTV: Radiofax, FAX480 and AVT

- Experimental Radiofax receive and transmit paths support explicitly selected IOC, dimensions, line rates and phasing conventions. Phasing-clock estimation and bounded black/white level calibration improve raster alignment and grayscale mapping; missing samples remain visible as incomplete coverage. Explicit FAX480 conventions remain separate because published geometry and signalling conventions differ.
- AVT Narrow, QRM and Narrow-QRM variants are integrated into the existing receive/transmit path. Receive controls include an optional manual interference notch; VIS/FSK acquisition retains a separate unfiltered signal path.
- A/B tests include two local Decodium sessions and virtual-audio transfers, independent FLdigi recordings and selected QSSTV/libsstv comparisons. Simulated clock-offset captures were also checked. These are selected laboratory cases: an intermittent partial 90 LPM reception was observed, and some detailed FAX480 comparisons with unmodified QSSTV did not meet their image-quality threshold. RF and complete cross-application compatibility are not established.

### Digital SSTV and HAMDRM

- Reed–Solomon protected image containers, recovery of missing or damaged data within the correction budget, BSR generation and repair handling, MOT assembly and duplicate-carousel handling have been extended. Invalid BSR segment values are rejected before 16-bit conversion.
- The waveform receiver adds CRC-gated soft-decision recovery and improved channel/timing experiments. An explicitly selectable experimental adaptive receive option is off by default and uses more processing. It can be changed only while HAMDRM is idle.
- Selected A/B local Decodium transfers and file-mediated comparisons with QSSTV cover several profiles, payload sizes, BSRs, protected images and controlled channel impairments. Two public recordings labelled EasyPal were also investigated. These results do not establish full QSSTV/EasyPal interoperability, live EasyPal exchange, RF performance or all profile combinations.

### Remote sharing and key storage

- Experimental end-to-end encrypted image sharing uses X25519 key agreement, HKDF-SHA256 and AES-256-GCM with recipient-key pinning. Encrypted-only download policy and authenticated manifest/payload validation reject the tested downgrade and tampering cases.
- Secure-key storage paths and create-only coordination have been strengthened for macOS Keychain, Windows DPAPI and Linux Secret Service. Ambiguous, malformed or inconsistent storage responses fail closed rather than silently replacing an identity or recipient pin.
- Local two-application HTTPS transfer and tampering tests were performed on macOS ARM64; earlier Linux ARM64 Secret Service tests covered selected paths. Windows, Linux x86_64, packaged builds and other keyrings require further verification. Metadata remains visible to the provider, the protocol does not authenticate the sender, and it has not received an independent security review.

### Packaging and verification

- Windows, macOS and Linux build/release workflows and SSTV test infrastructure have been updated, including pinned macOS OpenSSL build and runtime checks, Linux packaging checks, parser fuzzing and A/B laboratory tools.
- macOS ARM64 builds and focused Radiofax, HAMDRM, QML, secure-settings and sharing tests passed locally; documented A/B and sanitizer runs distinguish synthetic, local-application and independent-receiver evidence. Platform packages are supplied by their respective GitHub runners; successful packaging does not by itself prove RF or third-party interoperability.
- See `docs/sstv/AVT_PROTOCOL.md`, `RADIOFAX_PROTOCOL.md`, `HAMDRM_QSSTV_ORACLE.md`, `HAMDRM_COMPATIBILITY_MATRIX.md`, `E2EE_IMPLEMENTATION.md` and `TEST_STRATEGY.md` for detailed evidence and remaining limitations.

## Italiano

Questa release comprende le modifiche successive alla v1.0.728, inclusi gli aggiornamenti upstream della v1.0.732 e il lavoro SSTV sperimentale ancora in corso. I limiti di interoperabilità e sicurezza indicati sotto fanno parte della descrizione della release.

### FT2, pile-up e visualizzazione

- PILEUP aggiunge un limite giornaliero facoltativo di 1, 2 o 3 QSO per nominativo, banda e modo; zero disattiva il limite. Il conteggio della giornata viene letto dal log all'avvio.
- FT2 attende almeno due periodi prima di registrare un QSO dopo un solo RR73. AutoCQ e pile-up evitano di riaprire per dieci minuti una stazione lavorata o registrata, anche in presenza di rapporti tardivi. Un CQ residuo non viene ritrasmesso ripetutamente dopo l'arresto di AutoCQ.
- Full Spectrum non riutilizza più le righe in modo da causare testo sovrapposto durante aggiornamenti rapidi.

### SSTV: Radiofax, FAX480 e AVT

- I percorsi sperimentali Radiofax in ricezione e trasmissione permettono di selezionare IOC, dimensioni, velocità e convenzioni di phasing. La stima dell'orologio e la calibrazione limitata dei livelli di bianco e nero migliorano allineamento e scala di grigi; i campioni mancanti rimangono visibili come copertura incompleta. Le convenzioni FAX480 esplicite rimangono separate perché geometria e segnalazione documentate differiscono.
- Le varianti AVT Narrow, QRM e Narrow-QRM sono integrate nei percorsi di ricezione e trasmissione. I controlli RX includono un notch manuale facoltativo; l'acquisizione VIS/FSK conserva un percorso separato senza filtro.
- Le prove A/B includono due sessioni locali di Decodium, audio virtuale, registrazioni FLdigi indipendenti e confronti selezionati con QSSTV/libsstv. Sono stati provati anche segnali con scarto di clock simulato. Sono casi di laboratorio selezionati: è stata osservata una ricezione parziale intermittente a 90 LPM e alcuni confronti FAX480 dettagliati con QSSTV non modificato non hanno raggiunto la soglia di qualità. Non sono dimostrate affidabilità RF o interoperabilità completa.

### SSTV digitale e HAMDRM

- Sono stati estesi i contenitori immagine protetti Reed–Solomon, il recupero di dati mancanti o danneggiati entro il limite di correzione, la generazione BSR, la riparazione, l'assemblaggio MOT e la gestione dei gruppi ripetuti. I segmenti BSR non validi vengono rifiutati prima della conversione a 16 bit.
- Il ricevitore aggiunge recupero soft-decision controllato dal CRC e miglioramenti sperimentali di canale e temporizzazione. Una ricezione adattiva sperimentale selezionabile è disattivata per impostazione iniziale, richiede più elaborazione e può essere cambiata solo con HAMDRM inattivo.
- Trasferimenti A/B tra sessioni Decodium e confronti tramite file con QSSTV coprono alcuni profili, dimensioni, BSR, immagini protette e disturbi controllati. Sono state esaminate anche due registrazioni pubbliche attribuite a EasyPal. Questi risultati non dimostrano interoperabilità completa QSSTV/EasyPal, scambio EasyPal dal vivo, prestazioni RF o tutte le combinazioni di profilo.

### Condivisione remota e chiavi

- La condivisione sperimentale di immagini cifrate end-to-end usa X25519, HKDF-SHA256 e AES-256-GCM con verifica della chiave del destinatario. La policy di download solo cifrato e la verifica autenticata di manifest e contenuto rifiutano i casi di downgrade e manomissione provati.
- Sono stati rafforzati archiviazione sicura e creazione coordinata delle chiavi per macOS Keychain, Windows DPAPI e Linux Secret Service. Risposte ambigue, non valide o incoerenti vengono rifiutate senza sostituire silenziosamente identità o chiavi associate ai destinatari.
- Trasferimento HTTPS e manomissioni sono stati provati localmente tra due applicazioni su macOS ARM64; precedenti prove Linux ARM64 con Secret Service coprono percorsi selezionati. Restano da verificare Windows, Linux x86_64, pacchetti distribuiti e altri portachiavi. I metadati rimangono visibili al provider, il protocollo non autentica il mittente e non ha ancora ricevuto una revisione di sicurezza indipendente.

### Pacchetti e verifiche

- Aggiornati workflow di build/release Windows, macOS e Linux e infrastruttura di test SSTV, inclusi build e controlli runtime di OpenSSL fissati per macOS, controlli di packaging Linux, fuzzing dei parser e strumenti di laboratorio A/B.
- Su macOS ARM64 sono riuscite compilazioni e prove mirate Radiofax, HAMDRM, QML, impostazioni sicure e condivisione; la documentazione distingue prove sintetiche, applicazioni locali, ricevitori indipendenti e sanitizzatori. I pacchetti sono generati dai rispettivi runner GitHub; la sola build non dimostra funzionamento RF o interoperabilità con terzi.
- Per evidenze dettagliate e limiti residui consultare `docs/sstv/AVT_PROTOCOL.md`, `RADIOFAX_PROTOCOL.md`, `HAMDRM_QSSTV_ORACLE.md`, `HAMDRM_COMPATIBILITY_MATRIX.md`, `E2EE_IMPLEMENTATION.md` e `TEST_STRATEGY.md`.
