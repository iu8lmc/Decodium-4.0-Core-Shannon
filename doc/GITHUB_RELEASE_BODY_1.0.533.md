# Decodium 4.0 v1.0.533

Version 1.0.533 consolidates the work delivered after v1.0.530: complete
translation/catalogue repairs, the optional 3D waterfall spectrum, automatic
Auto Call, safer CAT4OM split/VFO sequencing, a higher-quality RTL-SDR SSB
listening path, richer Live Map and Call Roster intelligence, and clearer LoTW
and eQSL confirmation handling.

## English — changes since 1.0.530

### v1.0.531 — translations and Settings reliability

- The fork is aligned with the upstream Settings split and its associated CAT
  and map reliability work.
- QML translation contexts were rebuilt after the Settings dialog was divided
  into per-tab components, restoring the translated Settings interface instead
  of silently falling back to English.
- The RTL-SDR, eQSL and LoTW additions are translated across the supported
  language catalogues, including proper English strings for English users.

### v1.0.532 — optional 3D stacked spectrum

- The waterfall toolbar now provides an optional 3D stacked-trace spectrum,
  drawing recent band history as a perspective surface.
- Traces, floor level and the signal-history view are adjustable and persisted.
- The feature is disabled by default and uses the existing history without
  introducing a new shader or changing normal waterfall behaviour.
- The normal GPU-direct path remains active when 3D is disabled; the CPU/FFT
  history path is selected only while 3D is enabled, reducing the risk of
  regressions on Windows, macOS and Linux.

### v1.0.533 — Auto Call and transmission safety

- Added an **Auto Call** mode for automatically answering eligible
  CQ/QRZ stations without manually selecting a target.
- Added session limits, QSO counters, reset controls, persisted candidate
  priority and protection against conflicts with Direct Call, Auto CQ, active
  QSOs and unsupported modes.
- Candidate priority now supports **Last decoded**, **Strongest signal** and
  **Furthest grid**. Timestamp ordering, SNR comparison, locator validation,
  duplicate handling and fallback diagnostics are explicit and logged.
- Auto Call is stopped cleanly by the SWR protection path and can be armed again
  only after the operator has corrected the unsafe condition.
- The Auto Call path is decode/control-only and does not touch waterfall,
  panadapter or GPU rendering code.

### CAT4OM and Hamlib split/VFO handling

- CAT4OM now selects the target VFO before programming a non-active VFO,
  applies the requested frequency, and restores the original active VFO.
- Pending frequency sequences are tracked so intermediate state updates cannot
  restart the sequence or alternate between VFO A and VFO B.
- The IC-7300 split reporting case where TX VFO is temporarily reported as the
  active VFO is handled without repeatedly re-entering the VFO switch path.
- Fake It remains an audio-side split and no longer changes the physical radio
  split state; only Rig split programs the second VFO.

### RTL-SDR SSB listening improvements

- USB and LSB RTL-SDR listening now offer configurable voice bandwidth, slow or
  medium audio AGC, an optional notch filter and light or medium noise
  reduction.
- These operations run after demodulation at the audio rate, keeping the RF
  tuning reference and the panadapter path unchanged.
- The enhancements are deliberately restricted to RTL-SDR USB/LSB reception;
  FT8, FT4, FT2 and all other digital weak-signal paths do not use them.

### Live Map and Call Roster

- PSK Reporter Live Map spot history can be selected in five-minute steps from
  5 to 60 minutes, with longer retained-history options still available.
- Call Roster entries now expose grid provenance and reliability, distinguishing
  an on-air decoded locator from PSK Reporter, OAMS/RTSN corroboration and a
  lower-confidence lookup estimate.
- Grid markers, source labels and reliability state are shown consistently in
  the roster and Live Map panels, with migration for existing map databases.
- The Live Map propagation layer now accepts the additional live Tropo/Es
  overlay geometry, including blob-style polygon data, without changing the
  normal decode and map refresh path.

### LoTW, eQSL and confirmation imports

- LoTW usernames are preserved as entered and passwords are treated as opaque
  values rather than being altered by inappropriate trimming or case changes.
- LoTW response handling now distinguishes valid ADIF, authentication failure,
  service unavailability and malformed HTML responses.
- eQSL InBox handling follows the generated ADIF download link when the service
  returns one and correctly handles an empty Inbox.
- Manual confirmed-ADIF imports now run through the background pipeline and
  update confirmation counts and status consistently.

### Validation and compatibility

- Added focused regression coverage for Auto Call-adjacent safety, CAT4OM VFO
  sequencing, RTL-SDR DSP, grid provenance, Live Map spot age and propagation
  overlays, and LoTW/eQSL response parsing.
- The local macOS `decodium_qml` target builds successfully. The release assets
  are generated by the dedicated GitHub Actions workflows for Windows x64,
  macOS Apple Silicon, macOS Intel, Linux x86_64 and Linux aarch64.

## Italiano — cambiamenti dalla 1.0.530

La versione 1.0.533 consolida il lavoro successivo alla 1.0.530: correzioni
complete alle traduzioni e ai cataloghi, spettro waterfall 3D opzionale, Auto
Call automatico, gestione CAT4OM piu' sicura di split e VFO, ascolto SSB
RTL-SDR migliorato, Call Roster e Live Map piu' ricchi e gestione piu' chiara
delle conferme LoTW ed eQSL.

### 1.0.531 — traduzioni e affidabilita' delle Impostazioni

- Il fork e' allineato alla suddivisione upstream delle Impostazioni e ai
  relativi lavori di affidabilita' CAT e mappa.
- I contesti di traduzione QML sono stati ricostruiti dopo la divisione del
  dialogo Impostazioni in componenti separati, ripristinando l'interfaccia
  tradotta invece del fallback silenzioso all'inglese.
- Le aggiunte RTL-SDR, eQSL e LoTW sono tradotte nei cataloghi supportati,
  comprese stringhe inglesi corrette per gli utenti anglofoni.

### 1.0.532 — spettro 3D opzionale a tracce sovrapposte

- La toolbar del waterfall offre ora uno spettro 3D opzionale basato sulla
  cronologia recente della banda e rappresentato come superficie prospettica.
- Numero di tracce, livello del floor e vista della cronologia sono regolabili
  e persistenti.
- La funzione e' disattivata di default, riusa la cronologia gia' disponibile
  e non introduce nuovi shader ne' modifica il waterfall normale.
- Quando il 3D e' spento resta attivo il percorso GPU-direct normale; il
  percorso CPU/FFT con cronologia viene usato soltanto durante il 3D, evitando
  rischi aggiuntivi per Windows, macOS e Linux.

### 1.0.533 — Auto Call e sicurezza della trasmissione

- Aggiunta la modalita' **Auto Call** per rispondere
  automaticamente a stazioni CQ/QRZ idonee senza scegliere manualmente un
  target.
- Aggiunti limite di sessione, contatore QSO, reset, priorita' persistente e
  protezioni contro conflitti con Direct Call, Auto CQ, QSO attivi e modi non
  supportati.
- Le priorita' sono **Last decoded**, **Strongest signal** e **Furthest grid**.
  Ordinamento temporale, confronto SNR, validazione locator, duplicati e
  fallback sono espliciti e vengono registrati nel log.
- La protezione SWR ferma correttamente Auto Call e consente di riarmarlo solo
  dopo la correzione della condizione pericolosa.
- Auto Call opera soltanto sul percorso decode/controllo e non modifica
  waterfall, panadapter o rendering GPU.

### Gestione CAT4OM e split/VFO Hamlib

- CAT4OM seleziona ora il VFO di destinazione prima di programmare un VFO non
  attivo, imposta la frequenza richiesta e ripristina il VFO attivo originale.
- Le sequenze di frequenza pendenti impediscono agli aggiornamenti intermedi di
  riavviare la procedura o alternare continuamente VFO A e VFO B.
- Gestito il caso IC-7300 in cui il TX VFO viene riportato temporaneamente come
  VFO attivo, evitando di ripetere la sequenza di selezione.
- Fake It resta uno split solo audio e non cambia piu' lo split fisico della
  radio; soltanto Rig split programma il secondo VFO.

### Miglioramenti all'ascolto SSB RTL-SDR

- L'ascolto RTL-SDR in USB e LSB offre ora larghezza di banda voce, AGC audio
  lento o medio, notch opzionale e riduzione rumore leggera o media.
- Le operazioni vengono eseguite dopo la demodulazione alla frequenza audio,
  lasciando invariati riferimento di sintonia RF e percorso panadapter.
- I miglioramenti sono limitati alla ricezione RTL-SDR USB/LSB: FT8, FT4, FT2
  e tutti gli altri percorsi digitali non li utilizzano.

### Live Map e Call Roster

- La cronologia degli spot PSK Reporter nella Live Map e' selezionabile a passi
  di cinque minuti da 5 a 60 minuti, mantenendo anche opzioni di cronologia piu'
  lunga.
- Il Call Roster mostra origine e affidabilita' del locator, distinguendo tra
  locator decodificato in onda, corroborazione PSK Reporter/OAMS/RTSN e stima
  da lookup a bassa affidabilita'.
- Marker, origine e livello di affidabilita' sono mostrati coerentemente nel
  roster e nella Live Map, con migrazione dei database esistenti.
- Il layer di propagazione Live Map accetta anche geometrie live Tropo/Es,
  compresi i dati polygon/blob, senza modificare il normale percorso di decode
  e aggiornamento della mappa.

### Importazioni LoTW, eQSL e conferme

- Gli username LoTW vengono conservati come inseriti e le password sono trattate
  come valori opachi, senza trim o modifiche di maiuscole/minuscole inappropriate.
- Le risposte LoTW distinguono ADIF valido, autenticazione rifiutata, servizio
  non disponibile e pagine HTML non valide.
- eQSL InBox segue il collegamento al file ADIF generato dal servizio e gestisce
  correttamente anche una Inbox vuota.
- Le importazioni manuali di ADIF confermato passano ora dalla pipeline in
  background e aggiornano correttamente contatori e stato.

### Validazione e compatibilita'

- Aggiunta copertura di regressione per sicurezza Auto Call, sequenza VFO
  CAT4OM, DSP RTL-SDR, origine locator, eta' degli spot Live Map, overlay di
  propagazione e parsing delle risposte LoTW/eQSL.
- Il target locale macOS `decodium_qml` compila correttamente. Gli asset della
  release vengono generati dai workflow GitHub Actions dedicati per Windows
  x64, macOS Apple Silicon, macOS Intel, Linux x86_64 e Linux aarch64.

## Release assets

The release contains GitHub source archives, the Microsoft Windows x64
executable bundle, macOS Apple Silicon and Intel DMG/ZIP packages, and Linux
x86_64 and aarch64 AppImages with SHA-256 files.

La release contiene gli archivi sorgenti GitHub, il pacchetto eseguibile
Microsoft Windows x64, i pacchetti DMG/ZIP macOS Apple Silicon e Intel e gli
AppImage Linux x86_64 e aarch64 con i file SHA-256.
