# Decodium 4 FT2 1.0.486

## Release scope

This release contains the Windows packaging corrections completed after
1.0.485. It prevents a Windows installer from combining Qt libraries built from
different package revisions, which could make `decodium.exe` fail at startup
with a missing `QtPrivate_*` entry point in `Qt6QmlMeta.dll`.

There are no modem, decoder, CAT, audio or user-interface behavior changes in
1.0.486. The application behavior remains that of 1.0.485; the release changes
how the Windows runtime is assembled, validated and published.

## English

### One authoritative Windows release pipeline

- Removed the obsolete SignPath workflow that independently assembled and
  published a second Windows installer for the same release tag.
- Windows x64 packages are now produced by one authoritative workflow, avoiding
  a race in which a later job could overwrite a verified installer with a
  differently assembled file under the same asset name.
- The inactive SignPath packaging stages were removed from the canonical
  workflow. Code signing can be reintroduced later as a separate, explicit
  operation without rebuilding or replacing the tested runtime bundle.

### Consistent Qt runtime deployment

- The Windows bundle continues to use Qt's deployment tooling to collect the
  application, plugins, QML modules and their recursive runtime dependencies.
- Packaging now fails if any imported non-system DLL cannot be resolved inside
  the generated distribution.
- Every bundled `Qt6*.dll` is inspected for its required private Qt ABI.
- The required private ABI must match the ABI exported by the bundled
  `Qt6Core.dll`; a mismatch stops the workflow before an installer is created.
- Every bundled Qt DLL that has a corresponding MSYS2 source library is compared
  with that source using SHA-256. This prevents an older DLL from surviving in
  the distribution even when its filename is unchanged.
- The workflow writes a runtime DLL report into the portable bundle for future
  field diagnosis.

### Windows startup failure addressed

- The reported 1.0.485 failure was caused by a mixed Qt private ABI: one runtime
  component expected `QtPrivate_6_11_0` while the packaged `Qt6Core.dll`
  exported `QtPrivate_6_11_1`.
- The corrected pipeline verifies one coherent Qt 6.11 runtime before building
  `Decodium_1.0.486_Setup_x64.exe`.
- The installer replaces stale application runtime files during upgrade, so an
  existing installation cannot retain an incompatible Qt DLL from an older
  package.

### Validation

- The complete Windows x64 workflow was exercised after the packaging changes.
- Recursive dependency closure completed successfully.
- The private ABI gate reported one consistent `QtPrivate_6_11_1` runtime.
- SHA-256 source comparisons completed before installer generation.
- The canonical workflow generated and uploaded a working Windows installer
  without a competing release workflow.
- The macOS Apple Silicon, macOS Intel, Linux x86_64 and Linux aarch64 packages
  use the same 1.0.486 application source.

### Resilient Linux ARM packaging

- The Linux aarch64 workflow now uses HTTPS for the Ubuntu ports repository.
- APT is restricted to IPv4 on the hosted ARM runner and uses bounded retries
  and timeouts for index and package downloads.
- A failed index refresh is retried explicitly and stops the workflow before
  the long Qt build if the mirror remains unavailable.

### Canonical release publishing

- Platform workflows now normalize release references to the canonical
  `v<version>` format.
- This prevents a manual build started with `1.0.486` from creating a second
  release beside `v1.0.486`.
- All Windows, macOS and Linux packages are consolidated under one release page.

### Upgrade guidance for Windows

- Close Decodium before starting the installer.
- Install 1.0.486 over the existing installation; a manual DLL copy from an
  older release is not required.
- If a previous installation was manually modified, uninstall it first and then
  install 1.0.486 to guarantee a clean runtime directory.

### Release assets

GitHub Actions build the Windows x64 installer, macOS Apple Silicon DMGs, macOS
Intel DMGs, Linux x86_64 AppImage and Linux aarch64 AppImage. Matching ZIP
archives and checksums are attached where produced by each platform workflow.
The tagged source tree is available through GitHub's automatic source archives.

## Italiano

### Una sola pipeline autorevole per Windows

- Rimosso il workflow SignPath obsoleto, che assemblava e pubblicava in modo
  indipendente un secondo installer Windows per lo stesso tag.
- I pacchetti Windows x64 vengono ora prodotti da un unico workflow, eliminando
  la gara che poteva sovrascrivere un installer verificato con un file assemblato
  diversamente ma pubblicato con lo stesso nome.
- Rimossi dal workflow principale anche gli stage SignPath inattivi. La firma
  potra' essere reintrodotta in seguito come operazione separata ed esplicita,
  senza ricostruire o sostituire il runtime gia' testato.

### Runtime Qt coerente

- Il bundle Windows continua a usare gli strumenti di deploy Qt per raccogliere
  applicazione, plugin, moduli QML e dipendenze ricorsive.
- Il packaging fallisce se una DLL importata e non di sistema non puo' essere
  risolta dentro la distribuzione generata.
- Ogni `Qt6*.dll` inclusa viene analizzata per individuare la ABI privata Qt
  richiesta.
- La ABI richiesta deve coincidere con quella esportata dalla `Qt6Core.dll`
  inclusa; qualsiasi differenza interrompe il workflow prima dell'installer.
- Ogni DLL Qt che dispone della corrispondente libreria sorgente MSYS2 viene
  confrontata tramite SHA-256. Una DLL vecchia non puo' quindi sopravvivere nel
  pacchetto soltanto perche' mantiene lo stesso nome file.
- Nel bundle portabile viene generato un report delle DLL runtime per facilitare
  eventuali diagnosi future.

### Errore di avvio Windows corretto

- Il problema segnalato nella 1.0.485 dipendeva da ABI private Qt miste: un
  componente richiedeva `QtPrivate_6_11_0`, mentre la `Qt6Core.dll` inclusa
  esportava `QtPrivate_6_11_1`.
- La pipeline corretta verifica un unico runtime Qt 6.11 coerente prima di
  generare `Decodium_1.0.486_Setup_x64.exe`.
- Durante l'aggiornamento l'installer sostituisce i vecchi file runtime, evitando
  che una DLL Qt incompatibile rimanga nella directory dell'applicazione.

### Validazione

- Eseguito integralmente il workflow Windows x64 dopo le correzioni.
- Verificata con successo la chiusura ricorsiva delle dipendenze.
- Il controllo ABI ha confermato un runtime coerente `QtPrivate_6_11_1`.
- Completati i confronti SHA-256 con le librerie Qt sorgente prima della creazione
  dell'installer.
- Il workflow canonico ha generato e pubblicato l'installer senza una seconda
  pipeline concorrente.
- I pacchetti macOS Apple Silicon, macOS Intel, Linux x86_64 e Linux aarch64
  usano lo stesso codice applicativo della 1.0.486.

### Packaging Linux ARM piu' resiliente

- Il workflow Linux aarch64 usa ora HTTPS per il repository Ubuntu ports.
- APT viene limitato a IPv4 sul runner ARM e utilizza retry e timeout bounded
  per indici e download dei pacchetti.
- Un aggiornamento degli indici fallito viene riprovato esplicitamente e ferma
  il workflow prima della lunga build Qt se il mirror resta irraggiungibile.

### Pubblicazione canonica della release

- I workflow delle varie piattaforme normalizzano ora il riferimento nel
  formato canonico `v<versione>`.
- Una build manuale avviata con `1.0.486` non puo' piu' creare una seconda
  release accanto a `v1.0.486`.
- Tutti i pacchetti Windows, macOS e Linux sono raccolti in un'unica pagina.

### Indicazioni di aggiornamento per Windows

- Chiudere Decodium prima di avviare l'installer.
- Installare la 1.0.486 sopra l'installazione esistente; non serve copiare
  manualmente DLL da una release precedente.
- Se l'installazione precedente e' stata modificata manualmente, disinstallarla
  e installare la 1.0.486 per ottenere una directory runtime pulita.

### Asset della release

Le GitHub Actions generano installer Windows x64, DMG macOS Apple Silicon, DMG
macOS Intel, AppImage Linux x86_64 e AppImage Linux aarch64. ZIP e checksum
vengono allegati quando prodotti dal relativo workflow. Il codice sorgente
taggato e' disponibile negli archivi automatici di GitHub.
