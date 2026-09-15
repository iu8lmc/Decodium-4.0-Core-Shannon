# Decodium 4 v1.0.624

## English (UK)

v1.0.624 brings together the user-visible improvements introduced since v1.0.622, including the decode-output corrections delivered in v1.0.623.

### Decoding, archive and Tune safety

- Composite callsigns with a country prefix before the slash, such as `IH9/IT9JUI`, `SV8/F6BLP` and `EA8/G6MXL`, are no longer rejected by the post-decoder plausibility filter.
- Weak DX can now be retained when the same callsign repeats in a different time slot within ten minutes, instead of requiring an earlier strong decode.
- The decode archive is now written even when the `CQ only` display filter is active. Display filtering no longer causes directed messages to be omitted from the archive.
- Tune is protected by the bridge watchdog as well as by the legacy front end. Tune therefore stops automatically after the configured timeout; the default remains 90 seconds.

### AutoCQ burst and listening cadence

- The `CALL` window now contains an AutoCQ burst/listening cadence:
  - **CQs per burst**
  - **Listening cycles between bursts**
- Set, for example, `3` CQs and `2` listening cycles to call CQ three times, listen for two complete mode periods, then resume the next CQ burst.
- The historic continuous AutoCQ behaviour is preserved by default: the cadence activates only when both values are above zero.
- The choice is saved in the active profile and restored after restarting Decodium.
- Only automatic CQ transmissions are paused. Valid callers and normal directed QSO traffic can still be answered during the listening interval.
- The cadence is handled by both the current bridge TX path and the legacy TX backend.

### Logging and interoperability

- The EasyLog lowercase-band option now also applies to the primary WSJT-X-compatible UDP `Logged ADIF` message, not only to raw ADIF UDP targets.
- When **EasyLog band format** is enabled, `BAND` is sent as `20m`, `40m`, and so on, including to applications receiving the primary UDP log message.

### Panadapter recovery after minimising or restoring the window

- The panadapter now discards queued waterfall and PCM work while its window is hidden or minimised, instead of replaying stale buffered data when it is restored.
- Render timing and RHI/GPU readback state are reset across the visibility change, avoiding false UI-stall pressure and unnecessary processing while no local panadapter view can present frames.
- On restoration, the panadapter resumes from fresh incoming data.

### Settings and build maintenance

- Wanted Callsign alert controls are available under **Settings → Alerts**, alongside the other alert controls.
- GitHub Actions workflows have been updated to current action versions compatible with the Node 24 runner runtime.

The GitHub source archives contain the complete tagged codebase.

## Italiano

La v1.0.624 riunisce i miglioramenti visibili all’utente introdotti dalla v1.0.622 in poi, comprese le correzioni al post-processing dei decode distribuite con la v1.0.623.

### Decodifica, archivio e sicurezza del Tune

- I nominativi composti con prefisso di paese prima della barra, come `IH9/IT9JUI`, `SV8/F6BLP` ed `EA8/G6MXL`, non vengono più respinti dal filtro di plausibilità successivo al decoder.
- Una DX debole può ora essere mantenuta quando lo stesso nominativo si ripete in uno slot diverso entro dieci minuti, senza richiedere un decode forte precedente.
- L’archivio dei decode viene ora scritto anche con il filtro di visualizzazione `Solo CQ` attivo. Il filtro della vista non fa più omettere dall’archivio i messaggi diretti.
- Il Tune è protetto dal watchdog del bridge oltre che dal front end legacy. Si ferma quindi automaticamente allo scadere del timeout configurato; il valore predefinito resta 90 secondi.

### Cadenza AutoCQ a raffiche e ascolto

- Nella finestra `CALL` è disponibile una nuova cadenza AutoCQ:
  - **CQ per raffica**
  - **Cicli di ascolto tra le raffiche**
- Ad esempio, impostando `3` CQ e `2` cicli di ascolto, Decodium chiama CQ tre volte, ascolta per due periodi completi del modo e poi riprende con la raffica successiva.
- Il comportamento storico di AutoCQ continuo resta quello predefinito: la cadenza si attiva soltanto quando entrambi i valori sono maggiori di zero.
- Le impostazioni vengono salvate nel profilo attivo e ripristinate al riavvio di Decodium.
- Durante l’ascolto vengono sospesi soltanto i CQ automatici. I chiamanti validi e il normale traffico QSO diretto possono comunque ricevere risposta.
- La cadenza è gestita sia dal percorso TX del bridge corrente sia dal backend TX legacy.

### Logging e interoperabilità

- L’opzione EasyLog per la banda in minuscolo viene ora applicata anche al messaggio UDP primario WSJT-X compatibile `Logged ADIF`, non solo ai destinatari ADIF UDP raw.
- Quando **Formato banda EasyLog** è attivo, il campo `BAND` viene inviato come `20m`, `40m` e così via, anche alle applicazioni che ricevono il messaggio di log UDP primario.

### Ripristino del panadapter dopo riduzione a icona o riapertura

- Il panadapter ora elimina le righe waterfall e i dati PCM in coda quando la finestra è nascosta o ridotta a icona, invece di riprodurre dati vecchi memorizzati al ripristino.
- Le metriche di rendering e lo stato di readback RHI/GPU vengono reimpostati al cambio di visibilità, evitando falsa pressione da blocco dell’interfaccia e lavoro inutile quando non esiste una vista locale che possa mostrare i frame.
- Al ripristino il panadapter riparte dai dati ricevuti in quel momento.

### Impostazioni e manutenzione della build

- I controlli degli avvisi per i nominativi desiderati sono disponibili in **Impostazioni → Avvisi**, accanto agli altri controlli degli alert.
- I workflow GitHub Actions sono stati aggiornati a versioni correnti compatibili con il runtime Node 24 dei runner.

Gli archivi sorgenti GitHub contengono il codebase completo del tag.
