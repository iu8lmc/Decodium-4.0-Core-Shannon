# Decodium 4 v1.0.623

## English (UK)

This release recovers decodes that the application was discarding after the decoder, and adds the missing Tune watchdog.

- Callsigns with a country prefix before the slash (`IH9/IT9JUI`, `SV8/F6BLP`, `P4/PE1AZX`, `EA8/G6MXL`) are no longer rejected as ghosts: the prefix carries the area digit, which the check did not allow. Measured on 16 hours on air, this alone was 3.3% of all decodes, every one of them a real station.
- Weak DX rising out of the noise is no longer dropped below −23 dB while waiting to be heard once above −20 dB. A callsign now also confirms by repeating in another slot within ten minutes. `CQ YB1BZV OI42` had been discarded 190 times in one night and later arrived at −3 dB.
- Together the two corrections return 4.6% of the decodes that were being thrown away, almost all DX and portable stations.
- The decode archive is now written even when the "CQ only" view filter is active, like ALL.TXT and PSK Reporter. With that filter on it was keeping 14% of the directed messages.
- Tune now stops on its own. The Tune watchdog settings existed in the interface but only the legacy front end honoured them, so the transmitter stayed keyed indefinitely. Default 90 seconds, configurable in Settings.
- Includes the upstream 1.0.622 changes: Wanted Callsigns alerts moved into the Alerts settings, Node 24 GitHub runners, Intel macOS packaging with the runner libomp.

## Italiano

Questa release recupera decodifiche che l'applicazione buttava via dopo il decoder, e aggiunge il watchdog del Tune che mancava.

- I nominativi con il prefisso di paese davanti alla barra (`IH9/IT9JUI`, `SV8/F6BLP`, `P4/PE1AZX`, `EA8/G6MXL`) non vengono più scartati come fantasmi: il prefisso porta la cifra d'area, che il controllo non ammetteva. Misurato su 16 ore in aria, da solo valeva il 3,3% delle decodifiche, tutte stazioni vere.
- Le DX deboli che salgono dal rumore non vengono più scartate sotto i −23 dB in attesa di essere sentite una volta sopra i −20. Ora un nominativo si conferma anche ripetendosi in un altro slot entro dieci minuti. `CQ YB1BZV OI42` era stato scartato 190 volte in una notte, e più tardi è arrivato a −3 dB.
- Le due correzioni insieme restituiscono il 4,6% delle decodifiche che venivano buttate, quasi tutte DX e stazioni portatili.
- L'archivio delle decodifiche viene scritto anche quando è attivo il filtro di visualizzazione "solo CQ", come ALL.TXT e PSK Reporter. Con quel filtro acceso conservava il 14% dei messaggi diretti.
- Il Tune adesso si ferma da solo. Le impostazioni del watchdog del Tune esistevano nell'interfaccia ma le rispettava solo il front-end legacy, così la radio restava in trasmissione a tempo indeterminato. Novanta secondi di default, regolabili nelle Impostazioni.
- Include le modifiche upstream della 1.0.622: avvisi per i nominativi desiderati spostati nelle impostazioni Avvisi, runner GitHub Node 24, pacchetto macOS Intel con la libomp del runner.

The GitHub source archives contain the complete tagged codebase.
