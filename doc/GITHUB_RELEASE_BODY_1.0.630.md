# Decodium 4 v1.0.630

## English (UK)

This maintenance release contains the changes since v1.0.629. It extends the RTTY mode-exit correction with explicit receive-audio recovery and more precise diagnostics for issue #80.

### Receive audio after leaving RTTY

- Previously, a mode change could preserve a local capture simply because the SoundInput object existed. This did not establish that the source was delivering PCM samples.
- When leaving RTTY with local audio monitoring active, Decodium now clears residual TX-side receive discard/suspension bookkeeping and schedules an explicit capture recovery rather than relying on the existing-source shortcut.
- Recovery uses SoundInput's established restart path, including its delayed terminal-source recovery on Linux/PipeWire, instead of introducing an immediate destroy-and-recreate cycle.
- The recovery is scoped to local sound-card input; it does not intentionally restart remote DecoPort, TCI or active RTL-SDR reception. Deferred work is guarded against a changed monitoring session, monitoring being stopped, and active TX/Tune.

### Verify the flow, not just the object

- A delayed `RTTY exit RX check` diagnostic reports whether fresh audio-health callbacks arrived, together with RMS, peak, CAT-reported PTT and the selected mode.
- If no fresh PCM callback arrives, the existing audio watchdog recovery is requested. A stream carrying silence is distinguished from a missing stream; silence alone does not trigger this retry.
- The RTTY-exit message now says **PTT release requested**, rather than claiming a physical release merely because the software issued the command. CAT feedback and actual radio behaviour still need to be checked independently.

### Deferred audio restart safety

- The restart-generation guard is now available across platforms, not only Linux.
- Delayed macOS restart and reusable-source restart callbacks are rejected when superseded by a newer start, stop, restart or TX-side suspension. This prevents stale recovery callbacks from reopening or resuming an input after a newer operation.

### Validation and remaining confirmation

- Includes an additional audio-sink regression test demonstrating that zero-valued PCM still produces health callbacks after the receive discard gate is cleared, supporting the distinction between silence and absent audio.
- Local `decodium_qml` build and the audio-sink/TX-gate and RTTY TX test suites are used to validate these changes.
- This is a software recovery correction, **not a claim that issue #80 is definitively resolved on QMX/Arch Linux**. Please retest RTTY → FT8/FT4/FT2 without restarting the radio or application and provide the complete diagnostic log if reception remains flat, including the new `RTTY exit RX check` entries.

### Downloads

GitHub source archives are available for this tag. Release workflows provide a Windows x64 installer EXE; Apple Silicon DMGs for Sequoia and Tahoe; Intel DMGs for Ventura, Sonoma and Sequoia; and Linux x86_64/aarch64 AppImages. Assets appear as their builds finish. DMG and AppImage checksum files accompany the packages.

---

## Italiano

Questo rilascio di manutenzione comprende le modifiche dalla v1.0.629. Estende la correzione del cambio modo da RTTY con un recupero esplicito dell'audio in ricezione e diagnostica più precisa per la issue #80.

### Audio RX dopo l'uscita da RTTY

- Prima, al cambio modo la cattura locale poteva essere conservata semplicemente perché esisteva l'oggetto SoundInput. Questo non garantiva che la sorgente consegnasse campioni PCM.
- Uscendo da RTTY con monitoraggio audio locale attivo, Decodium ripulisce ora lo stato residuo di scarto/sospensione RX legato al TX e programma un recupero esplicito della cattura, senza affidarsi alla scorciatoia della sorgente già esistente.
- Il recupero utilizza il percorso di riavvio già previsto da SoundInput, compreso il recupero ritardato delle sorgenti terminate su Linux/PipeWire, senza introdurre un ciclo immediato di distruzione e ricreazione.
- L'intervento è limitato all'ingresso della scheda audio locale: non riavvia intenzionalmente la ricezione remota DecoPort, TCI o RTL-SDR attiva. Le operazioni ritardate controllano che la sessione non sia cambiata, che il monitoraggio sia ancora attivo e che non siano in corso TX/Tune.

### Verifica dei campioni, non soltanto dell'oggetto

- La nuova diagnostica ritardata `RTTY exit RX check` indica se sono arrivate nuove notifiche di campioni audio, insieme a RMS, picco, PTT riportato dal CAT e modo selezionato.
- Se non arriva alcuna nuova notifica PCM, viene richiesto il recupero già previsto dal watchdog audio. Un flusso silenzioso è distinto da un flusso assente: il solo silenzio non attiva questo tentativo aggiuntivo.
- Il messaggio di uscita RTTY indica ora **richiesta di rilascio PTT**, senza dichiarare un rilascio fisico soltanto perché il software ha inviato il comando. Riscontro CAT e comportamento della radio restano da verificare separatamente.

### Sicurezza dei riavvii audio ritardati

- La protezione tramite generazione del riavvio è ora disponibile su tutte le piattaforme, non soltanto Linux.
- Le callback ritardate di riavvio macOS e di recupero della sorgente riutilizzata vengono scartate se superate da un nuovo avvio, arresto, riavvio o sospensione per TX. Una vecchia callback non può quindi riaprire o riprendere l'ingresso dopo un'operazione successiva.

### Verifiche e conferma ancora necessaria

- Aggiunge un test del sink audio che dimostra come campioni PCM a zero producano comunque notifiche di salute audio dopo la riapertura del gate RX: questo sostiene la distinzione fra silenzio e assenza di audio.
- La validazione locale utilizza la build `decodium_qml` e le suite di test del sink/gate TX e della trasmissione RTTY.
- È una correzione software del recupero, **non una dichiarazione di risoluzione definitiva della issue #80 su QMX/Arch Linux**. Si richiede di riprovare RTTY → FT8/FT4/FT2 senza riavviare radio o programma e, se la ricezione resta piatta, inviare il log diagnostico completo con le nuove righe `RTTY exit RX check`.

### Download

Gli archivi sorgente GitHub sono disponibili per questo tag. I workflow producono installer EXE Windows x64; DMG Apple Silicon per Sequoia e Tahoe; DMG Intel per Ventura, Sonoma e Sequoia; AppImage Linux x86_64/aarch64. I file compaiono al completamento delle build. DMG e AppImage sono accompagnati dai checksum.
