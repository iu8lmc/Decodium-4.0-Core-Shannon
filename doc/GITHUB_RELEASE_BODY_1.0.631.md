# Decodium 4 v1.0.631

## English (UK)

This maintenance release contains the changes since v1.0.630, focused on reliable receive-audio recovery after leaving RTTY and clearer diagnostics for issue #80.

### Recovery independent of decoder timing

- The RTTY-exit recovery now has its own cancellation generation, independent of the UTC/decoder-period timer session. Rearming the timing scheduler no longer silently invalidates the audio recovery.
- Recovery is scheduled after the backend handover whenever monitoring should remain enabled, including when the reported monitoring state is temporarily inactive.
- If monitoring is still requested but temporarily inactive, the controller requests its reactivation and waits for the handover. It does not override an explicit Stop.

### Bounded recovery and cancellation

- Recovery is deferred during TX/Tune or remote keying. Waiting for transmission or the monitor handover is bounded by a 15-second deadline.
- Changing mode, changing the audio input, stopping monitoring or switching to an unsupported non-local input cancels the pending recovery. Superseded callbacks cannot execute a newer operation's work.
- Local sound-card recovery retains the existing audio restart/watchdog paths and clears residual receive discard/suspension bookkeeping. It does not introduce a new immediate device destruction/recreation sequence.

### PCM verification and diagnostic visibility

- Logs explicitly identify scheduling, execution, deferral, cancellation and completion, with reasons where relevant. A timeout reports that the PCM check could not be performed rather than disappearing silently.
- After recovery, the controller checks for fresh audio-health notifications and records PCM availability, RMS, peak, CAT-reported PTT and mode.
- If no fresh samples arrive, it requests one additional watchdog recovery and checks again. There are at most two recovery attempts. A stream carrying silence is not treated as an absent stream.

### Validation and remaining confirmation

- Adds seven automated controller scenarios covering monitor handover, silent PCM, bounded retry, TX deferral, Stop, superseded callbacks, timeout and non-local input cancellation.
- Local macOS `decodium_qml` compilation and three targeted test suites passed: RTTY RX recovery, audio-sink TX gate and RTTY TX.
- Hardware confirmation on the affected QMX/Linux configuration is still required; this release does not claim definitive resolution of issue #80. Please retest RTTY → FT8/FT4/FT2 and supply the complete diagnostic log, including `RTTY exit RX recovery` and `RTTY exit RX check` messages.

### Downloads

Source code is available in the tag's GitHub ZIP and tar.gz archives. Build workflows publish a Windows x64 installer EXE; Apple Silicon DMGs for Sequoia and Tahoe; Intel DMGs for Ventura, Sonoma and Sequoia; and Linux x86_64 and aarch64 AppImages. DMGs and AppImages include checksum files. Packages appear as each workflow completes; build publication does not constitute live-radio validation.

---

## Italiano

Questo rilascio di manutenzione comprende le modifiche dalla v1.0.630, dedicate al recupero dell'audio RX dopo l'uscita da RTTY e a una diagnostica più chiara per la issue #80.

### Recupero indipendente dai timer di decodifica

- Il recupero all'uscita da RTTY ha ora una propria generazione di annullamento, indipendente dalla sessione del timer UTC/periodo di decodifica. Riarmare lo scheduler temporale non invalida più silenziosamente il recupero audio.
- Il recupero viene programmato dopo il passaggio fra backend quando il monitoraggio deve restare abilitato, anche se lo stato riportato è temporaneamente inattivo.
- Se il monitoraggio è ancora richiesto ma temporaneamente inattivo, il controllore ne richiede la riattivazione e attende il passaggio. Non scavalca uno Stop esplicito.

### Tentativi limitati e annullamento

- Il recupero viene rinviato durante TX/Tune o attivazione remota del TX. L'attesa della fine TX o del monitoraggio è limitata da una scadenza di 15 secondi.
- Cambio modo, cambio ingresso audio, arresto del monitoraggio o passaggio a un ingresso non locale non supportato annullano il recupero pendente. Le vecchie callback non possono eseguire il lavoro di un'operazione successiva.
- Per la scheda audio locale vengono mantenuti i percorsi di riavvio/watchdog esistenti e ripulito lo stato residuo di scarto/sospensione RX. Non viene introdotta una nuova distruzione e ricreazione immediata del dispositivo.

### Verifica PCM e visibilità diagnostica

- Il log distingue programmazione, esecuzione, rinvio, annullamento e completamento, indicando i motivi quando pertinenti. Un timeout segnala che il controllo PCM non è stato eseguito, senza scomparire silenziosamente.
- Dopo il recupero viene verificato l'arrivo di nuove notifiche audio, registrando disponibilità PCM, RMS, picco, PTT riportato dal CAT e modo.
- In assenza di nuovi campioni viene richiesto un ulteriore recupero tramite watchdog, seguito da un nuovo controllo. I tentativi sono al massimo due. Un flusso silenzioso non viene confuso con un flusso assente.

### Verifiche e conferma ancora necessaria

- Aggiunge sette scenari automatici del controllore: passaggio del monitoraggio, PCM silenzioso, tentativi limitati, rinvio durante TX, Stop, callback superate, timeout e annullamento per ingresso non locale.
- Compilazione locale macOS di `decodium_qml` e tre suite mirate superate: recupero RTTY RX, gate TX del sink audio e trasmissione RTTY.
- Serve ancora conferma sulla configurazione QMX/Linux interessata: questo rilascio non dichiara la risoluzione definitiva della issue #80. Si richiede di riprovare RTTY → FT8/FT4/FT2 e inviare il log diagnostico completo, comprese le righe `RTTY exit RX recovery` e `RTTY exit RX check`.

### Download

Il codice sorgente è disponibile negli archivi GitHub ZIP e tar.gz del tag. I workflow pubblicano installer EXE Windows x64; DMG Apple Silicon per Sequoia e Tahoe; DMG Intel per Ventura, Sonoma e Sequoia; AppImage Linux x86_64 e aarch64. DMG e AppImage includono i checksum. I pacchetti compaiono al completamento dei rispettivi workflow; la pubblicazione non equivale a una verifica con radio reale.
