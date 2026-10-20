# Decodium 4 FT2 v1.0.584

This fork release follows v1.0.583 and reworks the worked-station filters in
Settings > Filters.

## English (British)

### Worked filters now match on the base callsign

- `Hide Worked Today` and the related worked sets are keyed by the base
  callsign. A QSO logged as IK8OLM now also covers a decode of IK8OLM/P or
  IK8OLM/QRP, and a QSO logged with a suffix covers the plain callsign.
- Previously the comparison used the callsign exactly as decoded, so a
  portable or special suffix defeated the filter without any visible reason.

### New: hide stations worked yesterday as well

- A new `Hide Worked Yesterday Too` switch widens `Hide Worked Today` to cover
  today and yesterday in UTC, mirroring the equivalent quick filter in
  WSJT-X and JTDX.
- The switch is only active while `Hide Worked Today` is enabled, and both
  dates roll over at 00:00 UTC exactly as the log does.

### New: hide every station already in the log

- A new `Hide Worked Ever` switch hides any callsign present in the logbook,
  regardless of band, mode or date. `Hide Worked on Band` remains the
  band-scoped filter it has always been.
- All four switches are still cleared at once by `Bypass Filters`, and they
  are now read from a single place in the bridge so the four display
  pipelines cannot drift apart.

### Interface translations

- The new switches and their tooltips are translated in all fifteen shipped
  languages; the catalogues remain at zero unfinished entries.

### Packaging and source availability

- The tagged source code is available through GitHub's generated source-code
  downloads for v1.0.584.
- Release workflows publish the Windows x64 installer, Linux x86_64 and
  aarch64 AppImages with SHA-256 checksums, and macOS DMGs with SHA-256
  checksums for the supported Apple Silicon and Intel runner targets.
- macOS application ZIP files remain intentionally excluded; only DMG packages
  and their checksums are published.

## Italiano

### I filtri delle stazioni lavorate confrontano il nominativo base

- `Nascondi lavorate oggi` e gli insiemi collegati usano ora il nominativo
  base. Un QSO registrato come IK8OLM copre anche una decodifica di IK8OLM/P o
  IK8OLM/QRP, e un QSO registrato con suffisso copre il nominativo semplice.
- Prima il confronto usava il nominativo esattamente come decodificato, quindi
  un suffisso portatile o speciale annullava il filtro senza motivo visibile.

### Novità: nascondere anche le stazioni lavorate ieri

- Il nuovo interruttore `Nascondi lavorate anche ieri` estende `Nascondi
  lavorate oggi` a oggi e ieri in UTC, come il filtro rapido equivalente di
  WSJT-X e JTDX.
- L'interruttore è attivo soltanto quando `Nascondi lavorate oggi` è acceso, e
  le due date cambiano alle 00:00 UTC esattamente come fa il log.

### Novità: nascondere tutte le stazioni già presenti nel log

- Il nuovo interruttore `Nascondi già lavorate` nasconde qualunque nominativo
  presente nel logbook, senza limiti di banda, modo o data. `Nascondi lavorate
  su banda` resta il filtro legato alla banda, come è sempre stato.
- Tutti e quattro gli interruttori continuano a essere azzerati insieme da
  `Bypass Filtri` e vengono ora letti da un unico punto del bridge, così le
  quattro pipeline di visualizzazione non possono divergere.

### Traduzioni dell'interfaccia

- I nuovi interruttori e i relativi suggerimenti sono tradotti in tutte e
  quindici le lingue distribuite; i cataloghi restano a zero voci non
  completate.

### Packaging e disponibilità del sorgente

- Il codice sorgente taggato è disponibile tramite i download del codice
  generati da GitHub per la v1.0.584.
- I workflow di release pubblicano l'installer Windows x64, le AppImage Linux
  x86_64 e aarch64 con checksum SHA-256, e i DMG macOS con checksum SHA-256
  per i runner Apple Silicon e Intel supportati.
- Gli ZIP dell'applicazione macOS restano esclusi intenzionalmente: vengono
  pubblicati soltanto i pacchetti DMG e i rispettivi checksum.
