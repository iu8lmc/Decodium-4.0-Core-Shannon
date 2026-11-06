# Decodium 4 FT2 1.0.737

## English

This release collects the changes since 1.0.736 to experimental SSTV,
HAMDRM image exchange and encrypted remote sharing.

### HAMDRM and interoperability

- Improve selective repair, partial-image recovery and persistence. The
  application can find saved partials and restore their associated profile;
  terminal segment markers and incomplete objects are handled more carefully.
- Fix packetisation of wide PPI fragments, which could reject valid larger
  images, and tighten packet identity, continuity, padding and CRC checks.
- Add an optional **QSSTV RS compatibility** setting for JPEG images. When
  enabled it may append zero bytes to the JPEG to avoid a padding issue in
  some QSSTV versions; received file bytes can therefore differ from the
  original even though the image remains unchanged. The option is off by
  default and the interface explains the trade-off.
- Extend the A/B evidence with QSSTV and adapted historical TRXAMADRM,
  including both directions of virtual-audio BSR repair and Hybrid references
  with a local FTP server. Selected EasyPal 2014 exchanges and retained test
  fixtures are also documented. These results cover selected profiles, not
  complete QSSTV/EasyPal interoperability or on-air operation.

### Analog SSTV and Radiofax

- Extend the configurable interference notch range to 3,500 Hz and expand
  AVT-QRM/Narrow-QRM A/B comparisons, including independent Revenant codec
  captures, timing and edge diagnostics. The experimental AVT24 edge-timing
  candidate remains restricted to laboratory test mode.
- Make the existing experimental Radiofax controls easier to identify for
  selected HFFAX/WEFAX profiles. The documented FAX480/QSSTV comparison
  limitation remains; this release does not claim every fax profile.

### Encrypted remote sharing and server

- Strengthen and expand signed and unsigned E2EE A/B checks, including
  tampering, key-pin handling, restart and expiry scenarios. Updated guidance
  explains recipient verification and optional sender authentication.
- Add an **experimental installable HTTPS sharing server** under
  `tools/sstv-share-server`, with separately authenticated accounts, SQLite
  persistence, token rotation, bounded storage, expiry and resumable uploads.
  Signed and unsigned A/B transfers passed between two Decodium sessions and
  the server over local HTTPS. The server is not independently security-audited
  or ready for public exposure; no production endpoint is included. See its
  README for installation, limitations and configuration.

### Tests and documentation

- Strengthen Windows installer verification: imported DLLs must resolve from
  the application or importing module's directory, rather than merely exist
  somewhere in the package. Invalid PE files fail the check; the runner checks
  the installed files and performs a startup probe with a restricted PATH.
  The portable bundle no longer includes unused PostgreSQL, MySQL and
  Firebird plugins without their optional client libraries; SQLite remains.
- Add and update focused waveform, packet, RS, partial-store, controller,
  QML, AVT, sharing and server tests, independent-software fixtures and
  laboratory tools. The QSSTV decoder helper now removes reproducible
  intermediate WAVs only after successful checks to reduce test storage.
- Update the Italian translation and correct stale E2EE documentation.
  See `docs/sstv/GOAL_VERIFICATION.md` and the relevant protocol documents for
  the precise evidence and remaining limitations.

Local virtual-audio and loopback tests are not radio, public-provider or
independent security-audit evidence. Platform packages are built separately by
the release workflows.

## Italiano

Questa versione raccoglie le modifiche dalla 1.0.736 a SSTV sperimentale,
scambio di immagini HAMDRM e condivisione remota cifrata.

### HAMDRM e interoperabilità

- Migliorati la riparazione selettiva, il recupero e la persistenza delle
  immagini parziali. È possibile cercare i parziali salvati e ripristinare il
  profilo associato; la gestione dei segmenti finali e incompleti è più robusta.
- Corretta la suddivisione dei frammenti PPI estesi, che poteva impedire
  l'invio di immagini valide più grandi; rafforzati i controlli su identità,
  continuità, riempimento e CRC dei pacchetti.
- Aggiunta l'opzione facoltativa **Compatibilità RS con QSSTV** per immagini
  JPEG. Quando attiva può aggiungere byte zero in coda per evitare un problema
  di padding di alcune versioni QSSTV: il file ricevuto può quindi differire
  nei byte, pur mantenendo la stessa immagine. È disattivata per impostazione
  predefinita e l'interfaccia spiega il compromesso.
- Ampliate le prove A/B con QSSTV e con TRXAMADRM storico adattato, comprese
  riparazioni BSR nei due sensi su audio virtuale e riferimenti Hybrid con
  server FTP locale. Sono documentati anche scambi selezionati con EasyPal
  2014 e relativi dati di test. Queste prove riguardano profili selezionati,
  non l'interoperabilità completa QSSTV/EasyPal o l'uso via radio.

### SSTV analogica e Radiofax

- Estesa a 3.500 Hz la regolazione del filtro notch e ampliate le prove A/B
  AVT-QRM/Narrow-QRM, incluse acquisizioni del codec indipendente Revenant e
  diagnostica di temporizzazione e bordi. La variante sperimentale di
  temporizzazione del bordo AVT24 rimane limitata ai test di laboratorio.
- Resi più riconoscibili i controlli Radiofax sperimentali per profili
  HFFAX/WEFAX selezionati. Rimane il limite documentato nel confronto
  FAX480/QSSTV; non è dichiarata la copertura di tutti i profili fax.

### Condivisione remota cifrata e server

- Rafforzate e ampliate le prove A/B E2EE con e senza firma, comprese
  alterazioni, verifica delle chiavi, riavvio e scadenza. La guida aggiornata
  spiega la verifica del destinatario e l'autenticazione facoltativa del mittente.
- Aggiunto un **server HTTPS sperimentale installabile** in
  `tools/sstv-share-server`, con account autenticati separati, persistenza
  SQLite, rotazione dei token, limiti di spazio, scadenza e ripresa degli invii.
  I trasferimenti A/B cifrati, con e senza firma, sono riusciti tra due
  sessioni Decodium e il server su HTTPS locale. Il server non ha ricevuto
  una revisione indipendente della sicurezza e non è pronto per l'esposizione
  pubblica; non è incluso un endpoint di produzione. Installazione, limiti e
  configurazione sono descritti nel relativo README.

### Test e documentazione

- Rafforzata la verifica dell'installer Windows: le DLL importate devono
  trovarsi nella cartella dell'applicazione o del modulo che le richiede, non
  semplicemente in una cartella qualsiasi del pacchetto. I file PE invalidi
  fanno fallire il controllo; il runner verifica l'installazione e prova
  l'avvio con un PATH limitato. Il pacchetto portabile non include più i
  plugin PostgreSQL, MySQL e Firebird non usati privi delle rispettive librerie
  opzionali; il driver SQLite rimane incluso.
- Aggiunti e aggiornati test mirati per forme d'onda, pacchetti, RS, salvataggi
  parziali, controller, QML, AVT, condivisione e server, oltre a fixture e
  strumenti di laboratorio per software indipendenti. Il supporto al decoder
  QSSTV elimina i WAV intermedi riproducibili solo dopo verifiche riuscite,
  riducendo lo spazio occupato dai test.
- Aggiornata la traduzione italiana e corretta documentazione E2EE superata.
  `docs/sstv/GOAL_VERIFICATION.md` e i documenti di protocollo riportano
  prove esatte e limiti ancora aperti.

Le prove locali su audio virtuale e loopback non dimostrano funzionamento via
radio, con un provider pubblico o una revisione indipendente della sicurezza.
I pacchetti per le piattaforme vengono generati separatamente dai workflow di
rilascio.
