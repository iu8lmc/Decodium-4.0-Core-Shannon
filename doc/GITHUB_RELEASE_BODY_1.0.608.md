# Decodium 4 FT2 v1.0.608

Version 1.0.608 is the cumulative fork release from v1.0.606 through v1.0.608. It includes the upstream fast-LDPC and FT2/FT4 work shipped in v1.0.607, together with the fork fixes for RTTY workspace ownership, station identity, working-frequency editing, UDP logging compatibility and Live Map presentation.

## English (UK)

### Upstream decoder work retained from v1.0.607

- The fast-LDPC candidate counter is 64-bit and the measured candidate budget is used instead of an estimate.
- FT2 deep decoding uses the tuned candidate set and pair span, reducing unnecessary CRC work while retaining the measured candidate range.
- The LDPC normalisation and min-sum iteration limits are tuned for the current decoder path. `DECODIUM_LDPC_MAX_ITER` can restore the previous iteration cap when required.
- The two experimental a-priori paths are present but remain opt-in (`DECODIUM_FT8_AP_STORICO` and `DECODIUM_FT8_AP_MSG`).
- CQ a-priori handling is also applied while an FT2/FT8/Q65 QSO is in progress, so an active contact no longer makes the decoder unnecessarily deaf to other CQs.
- Narrow decode-band handling uses the correct noise-floor and signal-peak region. The practical recommendation remains to keep the decode band at or above about 700 Hz.
- FT2 and FT4 non-regression coverage, scalar/AVX2/NEON equivalence checks and the ARM build path from v1.0.607 are retained.

### RTTY is now an exclusive receive workspace

- Opening or selecting RTTY stops the previously selected FT/JT/WSPR decoder path. A queued asynchronous FT8/FT4/JT callback cannot repopulate the RTTY view afterwards.
- The embedded legacy backend is no longer allowed to keep its previous digital mode, restart a hidden monitor or take ownership of the audio stream while RTTY is active.
- RTTY uses the native shared PCM path; the panadapter remains available, while the background dashboard decoder is stopped.
- RTTY state synchronisation and stale audio-buffer handling were added at the final asynchronous boundary, including the macOS and Windows paths.

### Station identity and profile isolation

- The QML Decodium station profile is now the authoritative source for callsign and locator.
- The legacy embedded widget receives an explicit identity synchronisation before mode changes and CQ regeneration.
- A callsign or grid from an unrelated legacy profile can no longer replace the active Decodium identity when changing modes.
- Synchronisation logs now include the active callsign and grid, making profile leakage diagnosable.

### Frequency settings and update provenance

- The lazily loaded frequency-settings page now exports its editor controls explicitly to `SettingsDialog`. Adding, editing and removing working frequencies, station offsets and calibration values therefore operate on the visible controls instead of stale component-local IDs.
- Frequency and QO-100/transverter offset edits continue to use the existing backend persistence and verification path.
- The updater exposes the repository that supplied an available release and displays it in the update dialog. This makes the primary `elisir80` repository and the upstream `iu8lmc` fallback unambiguous to the operator.

### UDP logging and LogHX compatibility

- When the visible `QSO logged` traffic control is enabled, Decodium now always emits both the WSJT-X `QSOLogged` and `LoggedADIF` records to the primary logger.
- Obsolete hidden `LoggedAdifEnabled` keys in older profiles can no longer suppress the ADIF event required by LogHX.
- The same coupling is applied to secondary and tertiary QSO logging paths and external ADIF uploads.
- Diagnostic logging explicitly states whether `QSOLogged` and `LoggedADIF` were emitted and reports the configured destination.

### Live Map station presentation

- PSK Reporter paths no longer use synthetic `PSKPATH<n>` labels. When available, the actual spotted callsign is shown; anonymous paths remain unlabelled rather than being presented as fake stations.
- Live decoder contacts, including stations calling the operator, retain the existing directional roles and filtering semantics.

### Packaging

This release publishes the source code, the Windows x64 executable/installer, macOS Apple Silicon and Intel DMG variants, and Linux x86_64 and aarch64 AppImages. Workflow-generated SHA-256 files are published alongside the binary assets where provided by the packaging workflow.

## Italiano

### Lavoro upstream sul decoder mantenuto dalla v1.0.607

- Il contatore dei candidati fast-LDPC è a 64 bit e viene usato il budget misurato invece di una stima.
- La decodifica profonda FT2 usa l'insieme di candidati e il pair span tarati, riducendo il lavoro CRC inutile e conservando l'intervallo misurato.
- La normalizzazione LDPC e il limite delle iterazioni min-sum sono tarati per il percorso decoder corrente. `DECODIUM_LDPC_MAX_ITER` può ripristinare il limite precedente quando serve.
- I due percorsi sperimentali a-priori sono presenti ma restano opzionali (`DECODIUM_FT8_AP_STORICO` e `DECODIUM_FT8_AP_MSG`).
- La gestione a-priori del CQ viene applicata anche durante un QSO FT2/FT8/Q65: un QSO attivo non rende più il decoder inutilmente sordo agli altri CQ.
- Le bande di decodifica strette usano la regione corretta per fondo e picco del segnale. La raccomandazione pratica resta di non scendere sotto circa 700 Hz.
- Sono mantenute le verifiche di non regressione FT2/FT4, l'equivalenza scalare/AVX2/NEON e il percorso di build ARM della v1.0.607.

### RTTY come spazio di ricezione esclusivo

- Aprire o selezionare RTTY ferma il percorso decoder FT/JT/WSPR selezionato in precedenza. Un callback FT8/FT4/JT asincrono già accodato non può più ripopolare la vista RTTY.
- Il backend legacy incorporato non può più conservare il precedente modo digitale, riavviare un monitor nascosto o riprendere il controllo dell'audio mentre RTTY è attivo.
- RTTY usa il percorso PCM nativo condiviso; il panadapter rimane disponibile, mentre il decoder della dashboard in background viene fermato.
- Sono state aggiunte la sincronizzazione dello stato RTTY e la gestione del buffer audio obsoleto all'ultimo confine asincrono, sia su macOS sia su Windows.

### Identità della stazione e isolamento dei profili

- Il profilo della stazione Decodium in QML è ora la sorgente autorevole per nominativo e locator.
- Il widget legacy incorporato riceve una sincronizzazione esplicita dell'identità prima dei cambi di modo e della rigenerazione del CQ.
- Un nominativo o locator appartenente a un profilo legacy estraneo non può più sostituire l'identità attiva di Decodium quando si cambia modo.
- I log di sincronizzazione riportano ora nominativo e locator attivi, rendendo diagnosticabile ogni fuga di profilo.

### Impostazioni frequenza e provenienza degli aggiornamenti

- La pagina delle impostazioni frequenza, caricata in modo lazy, esporta ora esplicitamente i propri controlli verso `SettingsDialog`. Aggiunta, modifica e rimozione delle frequenze operative, degli offset di stazione e dei valori di calibrazione operano quindi sui controlli visibili invece che su ID locali non accessibili.
- Le modifiche agli offset di frequenza, inclusi QO-100 e transverter, continuano a usare il percorso backend esistente di salvataggio e verifica.
- L'updater espone il repository che ha fornito la release disponibile e lo mostra nella finestra di aggiornamento. L'operatore può così distinguere chiaramente il repository principale `elisir80` dal fallback upstream `iu8lmc`.

### Logging UDP e compatibilità LogHX

- Quando è abilitato il controllo visibile del traffico `QSO logged`, Decodium emette sempre verso il logger primario sia il record WSJT-X `QSOLogged` sia `LoggedADIF`.
- Le vecchie chiavi nascoste `LoggedAdifEnabled` dei profili precedenti non possono più sopprimere l'evento ADIF richiesto da LogHX.
- Lo stesso accoppiamento è applicato ai percorsi QSO secondari e terziari e agli upload ADIF esterni.
- Il log diagnostico indica esplicitamente se `QSOLogged` e `LoggedADIF` sono stati emessi e riporta la destinazione configurata.

### Visualizzazione delle stazioni nella Live Map

- I percorsi PSK Reporter non usano più etichette sintetiche `PSKPATH<n>`. Quando disponibile viene mostrato il nominativo reale della stazione; i percorsi anonimi restano senza etichetta invece di essere presentati come stazioni fittizie.
- I contatti del decoder Live, comprese le stazioni che chiamano l'operatore, mantengono i ruoli direzionali e la semantica dei filtri esistenti.

### Packaging

Questa release pubblica il codice sorgente, l'eseguibile/installer Windows x64, le varianti DMG per macOS Apple Silicon e Intel e le AppImage Linux x86_64 e aarch64. I file SHA-256 generati dai workflow sono pubblicati insieme ai binari quando previsti dal percorso di packaging.
