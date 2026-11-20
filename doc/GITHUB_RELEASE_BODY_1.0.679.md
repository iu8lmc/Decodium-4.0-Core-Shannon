# Decodium 1.0.679

This release consolidates the changes from 1.0.676 through 1.0.678 and adds the local CW, SSB/CAT, audio and graphical fixes now included in the main branch.

## English (British)

### Highlights

- Strengthened local CW transmission through the radio's native Hamlib/CAT keyer. The controller now raises and releases PTT around CAT-keyed Morse where the radio requires it, stops the native keyer cleanly, and waits for the real transmit gate rather than using a fixed delay.
- Preserved the CW improvements introduced in 1.0.676–1.0.678: typed-text queueing, reliable live typing, native CAT keying, remote DecoLink keying, and FT-991A-compatible PTT handling.
- Made CW decoding more selective in noisy conditions. Tone, speed and decoding cost must remain stable across several frames; sudden cost increases are rejected while already accepted text is retained.
- Kept the CW decoder active when its window is hidden and improved sidetone compatibility with devices that do not support 48 kHz mono PCM. The selected output's supported sample rate and channel layout are now used, with diagnostic telemetry for the active format and startup failures.
- Improved SSB radio controls for Icom IC-7300 operation. AGC OFF uses the supported AGC-time path, FAST/MID/SLOW are labelled with their standard SSB time constants, and incomplete Hamlib capability lists no longer hide SLOW.
- Added CAT readback diagnostics for RF power and RF gain, recording the requested value, the value returned by the radio and the Hamlib result. This makes radio-side quantisation and asynchronous refresh behaviour observable instead of appearing as unexplained UI jumps.
- Synchronized the SSB window's USB/LSB and DATA state with the mode reported by CAT, without sending a spurious command during a programmatic refresh.
- Replaced hard-coded missing `Consolas`/`FreeSans` runtime requests with Decodium's platform-aware monospace or system fixed font. This removes the Qt font-alias startup warning and avoids repeated font-family resolution work.
- Added clearer CW transmitter/keyer status messages and a precise explanation of the local TX tone test.
- Added focused CW controller coverage for native rig-keyer selection, PTT ordering, stop handling, audio-gate completion and short typed chunks.

The release workflow publishes the source archive automatically together with the Windows x64 installer. The dedicated GitHub Actions runners publish macOS Intel and Apple Silicon DMGs and Linux x86_64 and aarch64 AppImages to the same release.

## Italiano

Questa release consolida le modifiche dalla 1.0.676 alla 1.0.678 e aggiunge le correzioni locali ora incluse nel main per CW, SSB/CAT, audio e interfaccia grafica.

### Novità principali

- Rafforzata la trasmissione CW locale tramite il keyer nativo Hamlib/CAT della radio. Il controller attiva e rilascia il PTT attorno al Morse inviato via CAT quando la radio lo richiede, arresta correttamente il keyer nativo e attende il reale stato di trasmissione invece di usare un ritardo fisso.
- Conservati i miglioramenti introdotti nella 1.0.676–1.0.678: coda del testo digitato, digitazione live più affidabile, keying CAT nativo, keying remoto DecoLink e gestione del PTT compatibile con FT-991A.
- Reso più selettivo il decoder CW in presenza di rumore. Tono, velocità e costo di decodifica devono restare stabili per più frame; i peggioramenti improvvisi del costo vengono scartati, mentre il testo già accettato viene mantenuto.
- Il decoder CW resta attivo anche quando la finestra viene nascosta. Il sidetone ora supporta dispositivi che non accettano PCM mono a 48 kHz, utilizzando frequenza di campionamento e numero di canali compatibili con l'uscita selezionata, con telemetria diagnostica del formato attivo e degli errori di avvio.
- Migliorati i controlli radio SSB per Icom IC-7300. AGC OFF usa il percorso AGC-time supportato, FAST/MID/SLOW mostrano i tempi standard SSB e le capability Hamlib incomplete non nascondono più SLOW.
- Aggiunta la diagnostica del readback CAT per RF power e RF gain, con valore richiesto, valore restituito dalla radio ed esito Hamlib. In questo modo la quantizzazione interna della radio e gli aggiornamenti asincroni sono osservabili invece di apparire come salti inspiegabili nell'interfaccia.
- Sincronizzato lo stato USB/LSB e DATA della finestra SSB con il modo comunicato dalla CAT, evitando l'invio di comandi indesiderati durante un aggiornamento programmato.
- Sostituite le richieste runtime hard-coded a `Consolas` e `FreeSans` con il font monospace scelto dalla piattaforma o con il font fixed di sistema. Questo elimina il warning Qt all'avvio sugli alias dei font e riduce il lavoro ripetuto di risoluzione.
- Aggiunti messaggi più chiari sullo stato del keyer/trasmettitore CW e una spiegazione precisa del test locale del tono TX.
- Aggiunti test mirati per la selezione del rig-keyer nativo, l'ordine PTT, l'arresto, il completamento legato allo stato audio e i brevi frammenti di testo digitato.

Il workflow di release pubblica automaticamente l'archivio del codice insieme all'installer Windows x64. I runner GitHub Actions dedicati pubblicano nella stessa release i DMG per macOS Intel e Apple Silicon e gli AppImage Linux per x86_64 e aarch64.
