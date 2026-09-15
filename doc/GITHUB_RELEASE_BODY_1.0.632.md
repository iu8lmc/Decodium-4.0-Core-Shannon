# Decodium 4 v1.0.632

## English (UK)

This maintenance release covers the changes since v1.0.631. It addresses a further CAT-state problem when leaving RTTY and improves receive-audio diagnostics for issue #80.

### Restore the radio mode after RTTY

- Entering RTTY can temporarily change the radio's CAT mode. Previously, when the normal CAT Mode setting was **None**, leaving RTTY did not restore the preceding radio mode. The application could therefore return to FT8 whilst the radio retained the RTTY-specific mode.
- Decodium now remembers the reported CAT mode before entering RTTY and records whether it requested a different mode. On exit, with CAT Mode set to None, it requests restoration of that previous mode.
- Restoration preserves the exact reported mode rather than assuming that every radio should be forced to USB or DATA-U. An explicit normal CAT Mode setting takes precedence.
- If RTTY did not request a different radio mode, no restoration is requested. Unknown modes and remote DecoPort contexts are not used as restoration targets.

### Handover safeguards and verification

- Restoration uses the existing CAT synchronisation path, including the post-frequency-change settling callback, with an additional delayed check for transitions that do not generate that callback.
- The saved state is temporary and bounded. A newer application mode change supersedes delayed work; disconnection or a backend change clears the snapshot. A mismatched radio context cannot receive the old restoration request.
- The restoration path defers writes during TX/Tune or remote keying. A delayed diagnostic reports the requested mode, reported mode and whether they match, or whether the operation was cancelled. This is CAT-reported status, not independent physical-radio confirmation.

### Better RX diagnostics without an arbitrary gain threshold

- New `RTTY entry state` logging records the CAT mode, RX level, calculated software gain, automatic RX-level setting and selected channel before the transition.
- `RTTY exit RX check` now includes the same gain/channel context and CAT mode alongside fresh-PCM availability, RMS, peak and CAT PTT status.
- The existing audio recovery and fresh-PCM checks remain in place. This release does not impose a minimum RMS threshold, force extra gain, or treat a lower RMS reading alone as proof of a failed stream.

### Validation and remaining confirmation

- Adds six automated CAT-mode-state scenarios: restoration of the exact preceding mode, explicit-setting precedence, no-override behaviour, connection/context cancellation, unknown-mode rejection, and expiry/new-transition handling.
- Local macOS `decodium_qml` compilation and four targeted test suites passed during development: CAT-mode restoration state, RTTY RX recovery, audio-sink TX gate and RTTY TX.
- The correction still needs testing on the affected QMX/Arch Linux configuration. Issue #80 is not declared definitively resolved; please provide a complete diagnostic log after testing RTTY → FT8/FT4/FT2, including the new entry/restore/check messages.

### Downloads

Source ZIP and tar.gz archives are available for this tag. GitHub workflows publish a Windows x64 installer EXE; Apple Silicon DMGs for Sequoia and Tahoe; Intel DMGs for Ventura, Sonoma and Sequoia; and Linux x86_64/aarch64 AppImages. DMGs and AppImages include checksum files. Packages appear as their workflows finish.

---

## Italiano

Questo rilascio di manutenzione comprende le modifiche dalla v1.0.631. Corregge un ulteriore problema dello stato CAT all'uscita da RTTY e migliora la diagnostica dell'audio RX per la issue #80.

### Ripristino del modo radio dopo RTTY

- Entrare in RTTY può cambiare temporaneamente il modo CAT della radio. Prima, con l'impostazione CAT Mode normale su **None**, all'uscita non veniva ripristinato il modo radio precedente. L'applicazione poteva quindi tornare in FT8 lasciando la radio nel modo specifico di RTTY.
- Decodium ora conserva il modo CAT riportato prima di entrare in RTTY e registra se ha richiesto un modo diverso. All'uscita, con CAT Mode su None, richiede il ripristino del modo precedente.
- Il ripristino conserva il valore riportato, senza presumere che ogni radio debba essere forzata in USB o DATA-U. Un'impostazione CAT Mode normale esplicita ha la precedenza.
- Se RTTY non ha richiesto un modo radio diverso, non viene richiesto alcun ripristino. Modi sconosciuti e contesti DecoPort remoti non vengono usati come destinazioni del ripristino.

### Protezioni e verifica del passaggio

- Il ripristino utilizza il percorso di sincronizzazione CAT esistente, compresa la callback di assestamento dopo il cambio frequenza, con un controllo ritardato aggiuntivo per i passaggi che non generano tale callback.
- Lo stato salvato è temporaneo e limitato nel tempo. Un nuovo cambio modo applicativo invalida le operazioni ritardate; disconnessione o cambio backend cancellano lo stato. Un contesto radio diverso non può ricevere la vecchia richiesta.
- Il percorso di ripristino rinvia le scritture durante TX/Tune o attivazione remota del TX. La diagnostica ritardata riporta modo richiesto, modo riportato e corrispondenza, oppure annullamento. È uno stato riportato dal CAT, non una conferma fisica indipendente.

### Diagnostica RX senza una soglia di guadagno arbitraria

- Il nuovo messaggio `RTTY entry state` registra modo CAT, livello RX, guadagno software calcolato, regolazione automatica del livello RX e canale selezionato prima del passaggio.
- `RTTY exit RX check` include ora anche il contesto guadagno/canale e il modo CAT, oltre alla disponibilità di nuovi campioni PCM, RMS, picco e stato PTT CAT.
- Restano attivi il recupero audio e i controlli sui nuovi campioni. Questo rilascio non impone una soglia RMS minima, non forza un aumento del guadagno e non considera un RMS più basso, da solo, prova di un flusso guasto.

### Verifiche e conferma ancora necessaria

- Aggiunge sei scenari automatici: ripristino esatto del modo precedente, precedenza delle impostazioni esplicite, assenza di override, annullamento per connessione/contesto, rifiuto dei modi sconosciuti e scadenza/nuovo passaggio.
- Durante lo sviluppo sono state superate la compilazione locale macOS di `decodium_qml` e quattro suite mirate: stato del ripristino CAT, recupero RTTY RX, gate TX del sink audio e trasmissione RTTY.
- Serve ancora la prova sulla configurazione QMX/Arch Linux interessata. La issue #80 non viene dichiarata definitivamente risolta: dopo RTTY → FT8/FT4/FT2 si richiede il log diagnostico completo, con i nuovi messaggi di ingresso, ripristino e verifica.

### Download

Gli archivi sorgente ZIP e tar.gz sono disponibili per il tag. I workflow GitHub pubblicano installer EXE Windows x64; DMG Apple Silicon per Sequoia e Tahoe; DMG Intel per Ventura, Sonoma e Sequoia; AppImage Linux x86_64/aarch64. DMG e AppImage includono i checksum. I pacchetti compaiono al completamento dei rispettivi workflow.
