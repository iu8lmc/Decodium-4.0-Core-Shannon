# Decodium 4 — 1.0.638

Changes since **v1.0.636**, including the upstream v1.0.637 changes and the local fixes and features consolidated in v1.0.638.

## English (UK)

### Dashboard: clean FT4/FT8 mode changes

- Changing mode now immediately resets the Full Spectrum and Signal RX display models rather than waiting for an incremental refresh. This addresses old decode rows remaining visible after the message counter has returned to zero.
- Pending model batches and deferred Full Spectrum updates are cancelled. Background results belonging to an earlier decode session are discarded instead of repopulating the newly cleared panes.
- Scrolling, queued tail-follow callbacks and message counters are reset in both docked and detached dashboard panes, including newest-first layouts.

### Older processors: safe FT8 decoder fallback

- The FT8 BICM extrinsic-information path now goes through the CPU dispatcher rather than entering an AVX2 implementation directly. This fixes an unsafe instruction path relevant to older processors such as Ivy Bridge.
- CPU capabilities, operating-system AVX state support, decoder enablement and environment overrides are checked before using the accelerated implementation. Unsupported configurations retain the safe fallback.
- Added regression coverage for missing AVX, AVX2, FMA, OSXSAVE and YMM state support, as well as disabled acceleration. This is not a blanket guarantee for every Windows installation or third-party runtime.

### Q65: EME Doppler tracking in the dashboard

- Added **Astro → Q65 — EME Doppler tracking**, connecting the QML dashboard to Hamlib CAT with **Rig** or **Fake It** split.
- Available methods are **Constant Frequency on the Moon (CFOM)**, **Full Doppler to DX Grid** and **Own Echo**. A six- or eight-character station locator is required; Full Doppler also requires the DX locator.
- Tracking starts **off** each session and does not enable transmission. RX corrections update while receiving; the TX correction is calculated for the period midpoint and held during transmission.
- Mode changes, manual tuning and disconnection stop tracking. Nominal/logged frequency remains separate from the corrected CAT dial, with protection against delayed CAT reports and startup synchronisation.
- The calculation reuses Decodium's native lunar model; it is **not a new JPL ephemeris implementation**. Coordinate the method with the other station. Real-radio, transverter and especially microwave EME accuracy still require validation. See [the operating notes](https://github.com/elisir80/Decodium-4.0-Core-Shannon/blob/v1.0.638/doc/Q65_EME_DOPPLER.md).

### RTTY: audio-only operation and short exchanges

- Local audio/AFSK transmission can operate without CAT, using VOX or manual PTT. Audio-device readiness and exclusion of competing transmitters remain enforced; audio-session ownership is not presented as a hardware PTT acknowledgement.
- Automatic polarity detection is given time to acquire a new signal before changing polarity.
- Receive framing, filter, spectrum and AFC state are reset together where required, preventing stale state from the preceding transmission or burst from affecting the next message.
- Improved preamble/tail handling and pending-character delivery for short messages and rapid turnarounds. Added regression cases for consecutive bursts and TX/RX transitions.

### Native SSTV: transmission readiness and receive diagnostics

- Transmit Studio reports why transmission is unavailable, including the need to load an image and prepare it with **Preview**. Readiness follows image preparation and busy-state changes.
- Improved handling of audio-only and VOX transmit preflight.
- **Save raw audio** can export retained audio even when VIS acquisition has failed, and the interface shows the diagnostic WAV path and job outcome.
- VIS detection handles an extended leader tone; Martin M1 reception refines the first line-sync boundary and buffers early observations until that boundary is established.
- Added regression coverage for VIS, Martin M1 reception, raw-audio export, transmit coordination and the SSTV interface.

### Decode reporting and alerts

- Separated secondary work for validated native decodes from presentation filtering. Hiding a decode in the display no longer automatically suppresses its eligible alert or PSK Reporter processing.
- Unresolved calls and deep/TX-list-only entries retain their specific reporting restrictions; visible map and UDP behaviour remains explicitly controlled.

### Included from upstream v1.0.637

- FT2 candidate peak detection uses the difference between the smoothed spectrum and estimated noise floor to reduce frequency bias near receiver passband edges. This correction is enabled for FT2; FT4 retains opt-in behaviour. `DECODIUM_FONDO=0` restores the previous behaviour.
- The menu includes **Decodium RX — terminal receiver**; terminal frequency entry accepts kHz as well as MHz, with cleaner startup output.
- Optional `DECODIUM_LLR_DUMP` instrumentation supports offline decoder comparisons. These upstream changes are included without claiming a new independent performance benchmark for this release.

### Downloads and validation

- **Known SSTV limitation:** the automated Robot B/W8 → Colour12 back-to-back test, with both frames meeting inside one audio chunk and no gap, fails to recognise the second frame. The failure also reproduces with the new VIS changes disabled; this release does not claim to resolve that case.
- Source code: GitHub's **Source code (zip)** and **Source code (tar.gz)** for this tag.
- Release workflows publish the Windows x64 installer EXE, macOS Apple Silicon DMGs (Sequoia/Tahoe), macOS Intel DMGs (Ventura/Sonoma/Sequoia), and Linux x86_64/aarch64 AppImages. Binary assets appear as their builds complete; use the matching checksum files where provided.
- Automated model, DSP and QML tests do not replace testing with the affected Windows computer, real radio hardware or on-air conditions. The macOS Intel packaging workflow retains its existing compatibility-check bypass; the OS-specific filenames are not a new certification of runtime compatibility.

---

## Italiano

### Dashboard: cambio FT4/FT8 senza righe residue

- Il cambio modo azzera immediatamente i modelli visualizzati da Full Spectrum e Signal RX, senza attendere un aggiornamento incrementale. La correzione affronta il caso in cui rimaneva visibile una vecchia decodifica con il contatore già a zero.
- Vengono annullati i blocchi di aggiornamento pendenti e gli aggiornamenti differiti di Full Spectrum. I risultati in background della sessione precedente vengono scartati e non possono ripopolare i pannelli appena svuotati.
- Scorrimento, richiami differiti di inseguimento della coda e contatori vengono ripristinati nei pannelli integrati e staccati, anche con le decodifiche più recenti in alto.

### Processori meno recenti: fallback sicuro del decoder FT8

- Il percorso delle informazioni estrinseche BICM di FT8 passa ora dal dispatcher CPU, anziché entrare direttamente nell'implementazione AVX2. È stato corretto un percorso non sicuro per processori meno recenti, come Ivy Bridge.
- Prima di usare l'accelerazione vengono controllati le capacità della CPU, il supporto AVX del sistema operativo, l'abilitazione del decoder e le variabili d'ambiente. Le configurazioni non supportate mantengono il fallback sicuro.
- Aggiunti test per assenza di AVX, AVX2, FMA, OSXSAVE e stato YMM, oltre alla disattivazione dell'accelerazione. Non si tratta di una garanzia generale per qualsiasi installazione Windows o libreria di terze parti.

### Q65: inseguimento Doppler EME nella dashboard

- Aggiunto **Astro → Q65 — Inseguimento Doppler EME**, collegato al CAT Hamlib con split **Rig** o **Fake It**.
- Sono disponibili **CFOM**, **Full Doppler verso il locator DX** e **Own Echo**. Serve il locator di stazione a sei o otto caratteri; Full Doppler richiede anche quello del corrispondente.
- Il tracking parte **spento** a ogni sessione e non abilita la trasmissione. La correzione RX viene aggiornata in ricezione; quella TX è calcolata per il centro del periodo e mantenuta fissa durante la trasmissione.
- Cambio modo, sintonia manuale e disconnessione fermano il tracking. La frequenza nominale e quella registrata nel log restano separate dal valore CAT corretto, con protezione dai riscontri CAT ritardati e dalla sincronizzazione all'avvio.
- Il calcolo riutilizza il modello lunare nativo: **non è una nuova implementazione di effemeridi JPL**. Il metodo va concordato con il corrispondente. Servono ancora verifiche con radio e transverter reali, soprattutto per la precisione EME sulle microonde. Vedere [le note operative](https://github.com/elisir80/Decodium-4.0-Core-Shannon/blob/v1.0.638/doc/Q65_EME_DOPPLER.md).

### RTTY: funzionamento senza CAT e messaggi brevi

- La trasmissione audio/AFSK locale può funzionare senza CAT, usando VOX o PTT manuale. Restano i controlli sul dispositivo audio e sulle trasmissioni concorrenti; lo stato della sessione audio non viene presentato come una conferma hardware del PTT.
- Il rilevamento automatico della polarità concede tempo all'aggancio prima di invertirla.
- Dove necessario, framing, filtri, spettro e AFC vengono ripristinati insieme, evitando che lo stato della trasmissione o del messaggio precedente interferisca con quello successivo.
- Migliorata la gestione di preambolo, coda e caratteri pendenti nei messaggi brevi e nei passaggi rapidi TX/RX, con test per raffiche consecutive e transizioni di ricezione/trasmissione.

### SSTV nativa: disponibilità TX e diagnostica RX

- Transmit Studio indica perché la trasmissione non è disponibile, compresa la necessità di caricare l'immagine e prepararla con **Preview**. Lo stato del pulsante segue preparazione e operazioni in corso.
- Migliorati i controlli preliminari per trasmissioni solo audio e VOX.
- **Save raw audio** può esportare l'audio conservato anche quando l'aggancio VIS fallisce; l'interfaccia mostra il percorso del WAV diagnostico e l'esito dell'operazione.
- Il rilevamento VIS gestisce un tono iniziale prolungato. La ricezione Martin M1 affina il primo sincronismo di riga e conserva le osservazioni iniziali fino alla sua definizione.
- Aggiunti test per VIS, ricezione Martin M1, esportazione audio, coordinamento TX e interfaccia SSTV.

### Segnalazioni e avvisi dei decode

- Le attività secondarie dei decode nativi validati sono separate dai filtri di visualizzazione: nascondere una decodifica non sopprime automaticamente gli avvisi previsti o il suo trattamento per PSK Reporter.
- Restano le restrizioni specifiche per nominativi non risolti e righe deep riservate alla lista TX; il comportamento visibile di mappa e UDP rimane controllato separatamente.

### Novità upstream 1.0.637 incluse

- La ricerca dei picchi FT2 usa la differenza fra spettro smussato e fondo stimato, riducendo l'errore di frequenza vicino ai bordi del passabanda. La correzione è attiva in FT2 e resta opzionale in FT4; `DECODIUM_FONDO=0` ripristina il comportamento precedente.
- Il menu comprende **Decodium RX — terminal receiver**; il terminale accetta frequenze in kHz e MHz e presenta un avvio più pulito.
- La diagnostica opzionale `DECODIUM_LLR_DUMP` permette confronti offline fra decoder. Queste modifiche upstream sono incluse senza attribuire a questo rilascio nuove misure indipendenti delle prestazioni.

### Download e limiti della verifica

- **Limite SSTV noto:** il test automatico con Robot B/W8 seguito immediatamente da Colour12, senza pausa e con il confine fra immagini nello stesso blocco audio, non riconosce il secondo frame. Il problema si riproduce anche disattivando le nuove modifiche VIS; questo rilascio non dichiara risolto quel caso.
- Sorgenti: **Source code (zip)** e **Source code (tar.gz)** del tag GitHub.
- I workflow pubblicano l'installer EXE Windows x64, i DMG macOS Apple Silicon (Sequoia/Tahoe), i DMG macOS Intel (Ventura/Sonoma/Sequoia) e le AppImage Linux x86_64/aarch64. I file compaiono al completamento delle rispettive build; usare i checksum abbinati dove disponibili.
- I test automatici di modelli, DSP e QML non sostituiscono la prova sul PC Windows interessato, con radio reali o in aria. Il workflow macOS Intel conserva il precedente bypass del controllo di compatibilità: i nomi riferiti alle versioni macOS non costituiscono una nuova certificazione di compatibilità a runtime.
