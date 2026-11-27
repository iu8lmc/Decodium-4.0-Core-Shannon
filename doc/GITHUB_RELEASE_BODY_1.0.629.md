# Decodium 4 v1.0.629

## English (UK)

This release brings together the changes after v1.0.624, including the upstream v1.0.625–v1.0.628 improvements and the local transmission and layout corrections in v1.0.629.

### FT8 sensitivity and decoding workload

- Integrates iterative BICM-ID demodulation, a coherent demodulation pass, a priori passes based on previously heard stations, and energy-domain CQ-history accumulation.
- These experimental sensitivity features are now adaptive rather than unconditionally enabled: they start disabled, become eligible after four consecutive slots below 50% of the decoding budget without CPU pressure, and are disabled when budget use exceeds 80% or CPU-pressure throttling occurs.
- Diagnostic `[LEVE]` entries record transitions. Explicit environment overrides remain available through `DECODIUM_FT8_BICM`, `DECODIUM_FT8_COERENTE`, `DECODIUM_FT8_AP_STORICO` and `DECODIUM_FT8_STORICO_ENERGIA`.
- Includes fast-LDPC batch work which skips empty lane groups, together with decoding benchmarks and experimental evaluation tools. Sensitivity gains depend on the signal and machine; no universal decoding gain is promised.

### Non-standard callsigns: display and automatic replies

- FT8, FT4 and FT2 no longer discard valid two-element type-4 messages simply because they do not resemble a conventional three-element exchange.
- Directed-call recognition now handles a resolved hashed recipient explicitly, allowing the sequencer to respond to non-standard callsigns rather than merely displaying their calls.
- Unresolved hashes retain the existing handling. Messages explicitly addressed to another station are not treated as calls to the operator.
- Shared recognition rules and an encoded/noisy FT2 waveform regression test cover this path.

### AutoCQ / Multi-Answer / Multi-Slot transmission cleanup

- Clears pending multi-stream messages and frequencies when their operating state ends, including disabling TX, disabling Multi-Slot, clearing slots and halting transmission. Disabling AutoCQ preserves a still-enabled Multi-Answer exchange.
- Invalidates cached transmit audio during cleanup so a subsequent manual transmission regenerates its waveform instead of reusing a residual multi-carrier payload.
- Multi-stream eligibility now requires the active sequencer state, and the audio cache distinguishes single-stream from composite audio.
- Adds regression coverage for payload eligibility, clearing and cache compatibility. This addresses the software path behind reported manual-TX power changes after AutoCQ; physical RF power remains dependent on the radio and audio setup.

### Leaving RTTY — issue #80

- Stops the RTTY AutoCQ timer and transmitter before releasing the shared audio output during a mode change.
- Requests PTT release, stops the RTTY output and resets its output-rate state so the next mode can initialise its audio path.
- Rejects late RTTY audio or PTT-on callbacks once RTTY is no longer selected; PTT-off remains permitted.
- Adds an automated regression test checking that stopped AutoCQ does not restart audio or re-key. The specific Arch Linux/QMX hardware configuration still requires user confirmation.

### Repeatable Reset Layout — issue #78

- Restores panel width bindings replaced by SplitView handle dragging, resets the four column size targets and restores the waterfall height to its default.
- Docks the DX Cluster again, stops an outstanding splitter animation and saves the resulting layout after the reset.
- Both the confirmation dialogue and keyboard shortcut use the same reset path. Repeated visual testing across operating systems remains advisable.

### Downloads and scope

Source archives are provided by GitHub for this tag. Platform assets are uploaded as their GitHub Actions builds finish: Windows x64 installer EXE; macOS Apple Silicon DMGs for Sequoia and Tahoe; macOS Intel DMGs for Ventura, Sonoma and Sequoia; Linux x86_64 and aarch64 AppImages.

No JT4/JT9/JT65 decoder fix is claimed here: the WAV supplied for issue #70 contains decodable FT8 traffic and does not establish the reported JT4 failure.

---

## Italiano

Questo rilascio raccoglie le modifiche successive alla v1.0.624, inclusi i miglioramenti upstream delle v1.0.625–v1.0.628 e le correzioni locali alla trasmissione e al layout della v1.0.629.

### Sensibilità FT8 e carico di decodifica

- Integra demodulazione iterativa BICM-ID, una passata coerente, passate a priori basate sulle stazioni già ricevute e accumulo dello storico CQ nel dominio dell'energia.
- Queste funzioni sperimentali sono ora adattive: partono disabilitate, diventano disponibili dopo quattro cicli consecutivi sotto il 50% del budget di decodifica senza pressione CPU e vengono disabilitate oltre l'80% del budget o quando interviene la limitazione per pressione CPU.
- Le righe diagnostiche `[LEVE]` registrano i cambiamenti. Restano disponibili le variabili di override `DECODIUM_FT8_BICM`, `DECODIUM_FT8_COERENTE`, `DECODIUM_FT8_AP_STORICO` e `DECODIUM_FT8_STORICO_ENERGIA`.
- Include ottimizzazioni fast-LDPC per saltare gruppi di corsie vuoti, benchmark e strumenti sperimentali di valutazione. Il guadagno dipende da segnale e computer: non viene promesso un incremento universale delle decodifiche.

### Nominativi non standard: visualizzazione e risposta automatica

- FT8, FT4 e FT2 non scartano più messaggi validi di tipo 4 con due elementi solo perché diversi dagli scambi convenzionali a tre elementi.
- Il riconoscimento delle chiamate dirette confronta esplicitamente il destinatario quando il suo hash è risolto, permettendo al sequencer di rispondere ai nominativi non standard anziché limitarsi a mostrarli.
- Gli hash non risolti mantengono la gestione precedente. I messaggi esplicitamente destinati a un'altra stazione non vengono interpretati come chiamate all'operatore.
- Regole condivise e un test con forma d'onda FT2 codificata e rumore coprono questo percorso.

### Pulizia della trasmissione AutoCQ / Multi-Answer / Multi-Slot

- Cancella messaggi e frequenze multi-stream pendenti quando termina lo stato operativo che li utilizza: disattivazione TX o Multi-Slot, cancellazione slot e arresto della trasmissione. Spegnere AutoCQ preserva uno scambio Multi-Answer ancora abilitato.
- Invalida la cache audio durante la pulizia: la successiva trasmissione manuale rigenera la forma d'onda, senza riutilizzare un payload multiportante residuo.
- L'utilizzo multi-stream richiede ora il sequencer effettivamente attivo; la cache distingue audio singolo e composito.
- Aggiunge test per abilitazione, pulizia del payload e compatibilità della cache. La correzione riguarda il percorso software associato ai cali di potenza segnalati dopo AutoCQ; la potenza RF effettiva dipende anche da radio e configurazione audio.

### Uscita da RTTY — issue #80

- Ferma timer AutoCQ e trasmettitore RTTY prima di rilasciare l'uscita audio condivisa al cambio modo.
- Richiede il rilascio PTT, arresta l'uscita RTTY e azzera lo stato della frequenza di campionamento in uscita, consentendo al modo successivo di inizializzare l'audio.
- Rifiuta callback RTTY tardive di audio o PTT-on quando il modo non è più selezionato; il PTT-off resta consentito.
- Un test automatico verifica che AutoCQ arrestato non riavvii audio o PTT. Resta necessaria la conferma dell'utente sulla specifica configurazione Arch Linux/QMX.

### Reset Layout ripetibile — issue #78

- Ripristina i binding delle larghezze sostituiti dal trascinamento dei separatori SplitView, reimposta le dimensioni delle quattro colonne e l'altezza predefinita della waterfall.
- Riaggancia il DX Cluster, ferma eventuali animazioni del separatore e salva il layout risultante dopo il reset.
- Dialogo di conferma e scorciatoia da tastiera utilizzano lo stesso percorso. Restano consigliate prove visive ripetute sui diversi sistemi operativi.

### Download e ambito

GitHub fornisce gli archivi sorgente del tag. I pacchetti vengono aggiunti al completamento delle build GitHub Actions: installer EXE Windows x64; DMG Apple Silicon per Sequoia e Tahoe; DMG Intel per Ventura, Sonoma e Sequoia; AppImage Linux x86_64 e aarch64.

Non viene dichiarato un fix ai decoder JT4/JT9/JT65: il WAV della issue #70 contiene traffico FT8 decodificabile e non dimostra il guasto JT4 segnalato.
