# Decodium 4 FT2 v1.0.583

This fork release follows v1.0.582 and corrects the DecoPort password prompt in
the Windows installer.

## English (British)

### DecoPort password is now confirmed twice

- The installer's `DecoPort security` page now asks for the password in two
  masked fields: the password itself and a `Repeat the password` confirmation.
- Setup refuses to continue while the two entries differ, showing an explicit
  message. Previously a single field was accepted as typed, so a mistyped
  password was stored without any chance of noticing it.
- This matters because the password is never readable again: Decodium converts
  it into a PBKDF2-SHA256 key on first start and removes the word from the
  configuration file. An undetected typo therefore meant a gateway that would
  not start, or a second computer that could not join, with nothing on screen
  explaining why.
- The existing rules are unchanged: a minimum of eight characters, and both
  fields left empty still means `do not publish the radio`, which remains a
  legitimate choice.
- Installations that already have a working DecoPort key are unaffected; no
  reconfiguration is required.

### Packaging and source availability

- The tagged source code is available through GitHub's generated source-code
  downloads for v1.0.583.
- Release workflows publish the Windows x64 installer, Linux x86_64 and
  aarch64 AppImages with SHA-256 checksums, and macOS DMGs with SHA-256
  checksums for the supported Apple Silicon and Intel runner targets.
- macOS application ZIP files remain intentionally excluded; only DMG packages
  and their checksums are published.

## Italiano

### La password DecoPort si conferma due volte

- La pagina `Sicurezza DecoPort` dell'installer chiede ora la password in due
  campi mascherati: la password e la conferma `Ripeti la password`.
- Il programma di installazione non prosegue finché le due voci non
  coincidono, e lo dice con un messaggio esplicito. Prima il campo singolo
  veniva accettato così come scritto, quindi una password digitata male veniva
  salvata senza alcuna possibilità di accorgersene.
- La cosa conta perché la password non è più rileggibile: al primo avvio
  Decodium la converte in una chiave PBKDF2-SHA256 e cancella la parola dal
  file di configurazione. Un refuso non scoperto significava quindi un gateway
  che non parte, oppure un secondo computer che non riesce a collegarsi, senza
  nulla a schermo che ne spiegasse il motivo.
- Le regole esistenti non cambiano: minimo otto caratteri, ed entrambi i campi
  lasciati vuoti continuano a significare `non pubblicare la radio`, che resta
  una scelta legittima.
- Le installazioni che hanno già una chiave DecoPort funzionante non sono
  toccate: non serve alcuna riconfigurazione.

### Packaging e disponibilità del sorgente

- Il codice sorgente taggato è disponibile tramite i download del codice
  generati da GitHub per la v1.0.583.
- I workflow di release pubblicano l'installer Windows x64, le AppImage Linux
  x86_64 e aarch64 con checksum SHA-256, e i DMG macOS con checksum SHA-256
  per i runner Apple Silicon e Intel supportati.
- Gli ZIP dell'applicazione macOS restano esclusi intenzionalmente: vengono
  pubblicati soltanto i pacchetti DMG e i rispettivi checksum.
