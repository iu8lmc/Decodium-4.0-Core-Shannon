# Decodium 4.0 v1.0.541

Version 1.0.541 carries the upstream 1.0.540 work — per-destination UDP traffic
filters and a reworked Windows graphics fallback chain — with the ten new
interface strings translated into all fourteen languages.

The version number is raised because upstream had already published 1.0.540
under that tag. Two different binaries sharing one number would only confuse
operators, so this build takes the next number instead.

## English (British)

### From upstream 1.0.540

- Independent traffic filters for the primary, secondary and tertiary UDP
  destinations: Decode, Status, Logged QSO and WSPR packets can each be
  enabled or disabled per destination. All four are on by default, so the
  behaviour is unchanged for JTAlert, GridTracker and other WSJT-X-compatible
  consumers.
- A destination configured for logged QSOs only no longer creates a persistent
  realtime client, so it emits no heartbeats, no status packets and no decoded
  traffic. When a QSO is committed it receives exactly one ADIF datagram.
- WSPR reporting follows its own packet path instead of being mixed with
  ordinary decode traffic. The per-destination client identifiers introduced in
  1.0.538 are retained.
- The Windows graphics fallback chain now recovers progressively through D3D12,
  D3D11, D3D11 WARP and the Qt software renderer, with persistent fallback state
  and clearer diagnostics. Slow-PC mode selects the conservative D3D11 hardware
  backend on Windows, which removes the OpenGL startup failure reported on three
  laptops rather than only recovering from it.
- Corrected the CAT poll-interval layout in the settings panel.

### Translations

- The ten new interface strings are translated in all fourteen catalogues: the
  primary, secondary and tertiary traffic labels, the four traffic types, the
  two additional client identifier fields, and the long Slow-PC mode tooltip.
- `WSPR` is left unchanged: it is an international abbreviation.
- Catalogues: 5465 to 5475 messages, zero unfinished and zero vanished entries
  in every language.

### Validation

- Local `decodium_qml` build completed successfully.
- `test_udp_client_id` passed.
- Runtime check of the built application: no QML warnings, FT8 decoding active
  and the waterfall rendering normally.
- Verified that the fixes carried in 1.0.537 through 1.0.539 survived the
  merge: TCI receive audio, per-destination UDP client identifiers, the decode
  de-duplication tolerance, the revision dirty-state check, and the graphics
  startup recovery guard.

## Italiano

La versione 1.0.541 porta il lavoro della 1.0.540 a monte — filtri del traffico
per destinazione UDP e catena di ripiego grafico riscritta su Windows — con le
dieci stringhe nuove dell'interfaccia tradotte in tutte e quattordici le lingue.

Il numero di versione sale perché a monte la 1.0.540 era già stata pubblicata
con quel tag. Due binari diversi con lo stesso numero servirebbero solo a
confondere gli operatori, quindi questa build prende il numero successivo.

### Dalla 1.0.540 a monte

- Filtri del traffico indipendenti per le destinazioni UDP primaria, secondaria
  e terziaria: i pacchetti Decode, Status, QSO registrati e WSPR si possono
  attivare o disattivare separatamente per ciascuna destinazione. Tutti e
  quattro sono attivi di serie, quindi il comportamento non cambia per JTAlert,
  GridTracker e gli altri programmi compatibili con WSJT-X.
- Una destinazione impostata sui soli QSO registrati non crea più un client
  realtime persistente: non emette heartbeat, né pacchetti di stato, né
  traffico decodificato. Quando un QSO viene registrato riceve esattamente un
  datagramma ADIF.
- La segnalazione WSPR segue un percorso proprio invece di essere mescolata al
  traffico ordinario delle decodifiche. Gli identificativi per destinazione
  introdotti nella 1.0.538 restano.
- La catena di ripiego grafico su Windows recupera ora in modo progressivo fra
  D3D12, D3D11, D3D11 WARP e il renderer software di Qt, con stato di ripiego
  persistente e diagnostica più chiara. La Modalità PC lento sceglie il backend
  hardware conservativo D3D11 su Windows, il che elimina alla radice il guasto
  OpenGL all'avvio segnalato su tre portatili invece di limitarsi a recuperarlo.
- Corretta la disposizione dell'intervallo di polling CAT nel pannello delle
  impostazioni.

### Traduzioni

- Le dieci stringhe nuove dell'interfaccia sono tradotte in tutti e quattordici
  i cataloghi: le etichette del traffico primario, secondario e terziario, i
  quattro tipi di traffico, i due campi aggiuntivi di identificativo client e il
  testo lungo del suggerimento della Modalità PC lento.
- `WSPR` resta invariato: è una sigla internazionale.
- Cataloghi: da 5465 a 5475 messaggi, zero voci non tradotte e zero obsolete in
  ogni lingua.

### Verifica

- Build locale di `decodium_qml` completata correttamente.
- `test_udp_client_id` superato.
- Prova a runtime dell'applicazione compilata: nessun avviso QML, decodifica FT8
  attiva e cascata regolare.
- Verificato che le correzioni portate dalla 1.0.537 alla 1.0.539 siano
  sopravvissute all'unione: audio di ricezione TCI, identificativi UDP per
  destinazione, tolleranza nella deduplica delle decodifiche, controllo reale
  dello stato dell'albero nella revisione e guardia di recupero all'avvio
  grafico.

## Release assets

The release workflows publish the Windows x64 executable, macOS Intel and
Apple Silicon DMG packages, and Linux x86_64 and aarch64 AppImages together
with their checksums where provided by the workflow.
