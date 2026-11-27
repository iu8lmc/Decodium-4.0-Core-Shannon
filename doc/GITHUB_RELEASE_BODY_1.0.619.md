# Decodium 4 FT2 v1.0.619

## English (UK)

This release brings the elisir80 main branch forward from v1.0.614, incorporates the upstream v1.0.615–v1.0.618 changes, and adds the local FT8 AP corrections and display improvements described below. The source archives and platform packages belong to the same release tag.

### New in v1.0.619

#### FT8: apply the own-callsign AP protection in the live decoder

- Fixed missing wiring between the active embedded decoder and the existing protection against speculative calls addressed to the local station. Previously, an alternative bridge path set the protection, but the MainWindow live requests did not carry it into the worker.
- Each decode request now carries its own AP eligibility snapshot. The worker applies it while holding the shared DSP runtime mutex and restores the previous setting afterwards. Preparing a later request can no longer change this setting during an earlier decode.
- Live AP hypotheses involving the local callsign are allowed during an FT8 message transmission and for three minutes from a recorded FT8 message transmission start. Tuning, merely enabling Auto TX, selecting a station, or retaining an old QSO state does not establish recent transmission. File decoding retains its explicit AP behaviour.
- Normal decoding of unsolicited calls addressed to the local station remains available. CQ and independent historical hypotheses retain their existing policy. No arbitrary cut-off has been added to reject reports below −28 dB: the report carried inside a message is distinct from the SNR measured by the receiver.
- Removed two live-pass overrides that could turn AP back on after the user had switched it off. This completes the behaviour of the existing AP controls across the affected live request builders.
- Added `ap_mycall=0/1` to FT8 `DECODEMETRIC` diagnostics to show the effective state used by the worker.

This addresses a confirmed wiring defect. It does not establish that every possible false decode has been eliminated, and the exact reported phantom cannot be reproduced without its original audio recording.

#### Clearer operating controls

- Band labels, working frequency, mode selectors and operating-button labels now use bold text, including inactive buttons. Colours and borders continue to indicate their state.
- Styled dropdowns now honour the control's requested bold font in both the selected value and menu entries.
- Added a SuperFox status indicator between the received report and the next-message display. It becomes active when FT8, the SuperFox setting and the Fox/Hound role are enabled; its tooltip distinguishes the receive and transmit roles. It indicates configuration, not proof that an incoming signal has been authenticated or decoded as SuperFox.
- Limited the Next/TX message field to a compact, scaled width, leaving room for the adjacent controls. A tooltip exposes the complete message when the visible text is shortened.
- The Next/TX label and message are also bold for readability.

### Changes incorporated since v1.0.614

#### v1.0.615: SuperFox receive integration and FT2 drift rescue

- Connected SuperFox decoding to the live FT8 receive worker for the Hound role when SuperFox is enabled.
- Moved a large SuperFox working buffer from the stack to heap storage to avoid a reproduced stack overflow, and corrected SuperFox simulation utility compilation issues.
- Corrected undefined behaviour in the optional FT2 drift-rate search and added `[FT2-DRIFT-RESCUE]` diagnostics when that search recovers a message. Drift search remains opt-in.
- Upstream verified the SuperFox receive path with generated audio and a reference decoder; this release does not add confirmation from an actual DXpedition signal.

#### v1.0.616: FT8 false-decode controls and optional FT2 accumulation

- Enabled the learned FT8 LDPC candidate gate by default; the FT2 gate remains opt-in. Upstream measurements reported fewer false decodes with a sensitivity trade-off, including approximately 2.4–3.6% fewer genuine decodes on the recording used for that comparison. These are upstream measurements, not a new benchmark performed for v1.0.619.
- Retained separately trained FT8/FT2 gate weights and diagnostic tools for collecting and evaluating real LLR candidates.
- Rejected message report codepoints outside the transmitter-supported −50 to +49 range, including the affected plain and TU message variants.
- Introduced the own-callsign AP protection whose missing live-path wiring is completed in v1.0.619.
- Enabled predictive whole-message verification (type 8) by default for FT8 and FT2. This checks a recently received message against an appropriately timed repeat. The `DECODIUM_FT8_AP_MSG=0` and `DECODIUM_FT2_AP_MSG=0` overrides remain available.
- Added experimental FT2 energy accumulation across repeated slots, controlled from Settings > TX below Conservative FT2. It is off by default and is not persisted across restarts; its recorded gains are from controlled tests, not confirmed general on-air performance.
- Fixed silent loss of messages recovered by the optional FT2 rescue paths, and gave drift rescue its own processing budget.

#### v1.0.617: stop CQ history echoes in the wrong slot

- Added transmit-slot parity checks to three FT8 history replay paths. Recently heard messages are no longer replayed into the opposite transmit parity solely because their age and frequency match.
- Upstream reported a reduction from 66 fabricated CQ echoes to 1 on its 510-slot recording, while legitimate history replays changed from 2,333 to 2,332. These figures describe that recorded comparison, not a guarantee for every band or station.

#### v1.0.618: AP settings and decoder investigation tools

- Added the FT8 AP checkbox to Decoder Options alongside the existing decoder settings. v1.0.619 additionally prevents the affected live passes from overriding the user's off selection.
- Extended FT8 candidate collection with synthetic pile-up data and gate-evaluation options. The proposed replacement gate weights were not adopted after poorer results on real recorded traffic; v1.0.619 does not silently substitute those experimental weights.

### Validation and outstanding items

- Local application build succeeded on Apple Silicon with Qt 6.11.0.
- The new FT8 AP suite passed 9/9 results, including state expiry, queued-request isolation, AP off, CQ preservation and decoding a generated directed call with own-callsign AP disabled.
- Local Qt rendering checks passed for the toolbar at three scales and for SuperFox state/layout behaviour. Windows monitor rendering and real-radio behaviour are not established by those local checks.
- Selected existing decoder checks returned 15 passes and one pre-existing default-profile mismatch: an older test expects 13 AP passes, whereas the current upstream defaults select 17. The same selection passes 16/16 under the historical AP profile. This is documented in `docs/diagnostics/2026-09-08-ft8-mycall-ap.md`; it is not presented as an all-green full-suite run.
- Upstream reported open follow-ups concerning directed-message archival and rejection of some valid compound callsigns. These are not claimed fixed here.
- The reported IC-7851 CAT/PTT issue on Windows remains under investigation pending the affected session log. No speculative CAT/PTT change is included in this release.

### Downloads

GitHub Actions builds the Windows x64 `.exe` installer; Apple Silicon DMGs for macOS Sequoia and Tahoe; Intel DMGs for Ventura, Sonoma and Sequoia; and Linux x86_64 and aarch64 AppImages. The macOS and Linux workflows also provide SHA-256 files. GitHub's **Source code (zip)** and **Source code (tar.gz)** links provide the complete tagged codebase. Package uploads appear as their individual workflows finish; an absent asset is not a completed build.

## Italiano

Questa release porta il ramo main di elisir80 dalla v1.0.614 alla nuova versione, incorpora le modifiche upstream v1.0.615–v1.0.618 e aggiunge le correzioni locali dell'AP FT8 e della leggibilità descritte qui sotto. Archivi sorgenti e pacchetti delle piattaforme appartengono allo stesso tag di release.

### Novità della v1.0.619

#### FT8: protezione AP sul proprio nominativo nel decoder live

- Corretto il collegamento mancante fra il decoder embedded attivo e la protezione già esistente contro le ipotesi di chiamata alla propria stazione. Prima la protezione veniva impostata da un percorso alternativo del bridge, ma le richieste live di MainWindow non la trasportavano al worker.
- Ogni richiesta di decodifica ora contiene il proprio stato di abilitazione AP. Il worker lo applica mentre detiene il mutex del runtime DSP condiviso e ripristina lo stato precedente alla fine. La preparazione di una richiesta successiva non può più cambiare questa impostazione durante una decodifica precedente.
- Le ipotesi AP sul proprio nominativo sono consentite durante la trasmissione di un messaggio FT8 e per tre minuti dal suo avvio registrato. Il tuning, la sola attivazione di Auto TX, la selezione di una stazione o uno stato QSO rimasto precedente non valgono come trasmissione recente. La decodifica da file conserva l'uso esplicito dell'AP.
- Le chiamate spontanee alla propria stazione restano ricevibili tramite la decodifica normale. CQ e ipotesi indipendenti dallo storico conservano la loro politica. Non è stato introdotto un taglio arbitrario dei rapporti sotto −28 dB: il rapporto contenuto nel messaggio è distinto dall'SNR misurato dal ricevitore.
- Eliminati due punti dei passaggi live che potevano riattivare AP dopo che l'utente l'aveva spento. La selezione dei controlli AP viene così rispettata anche nei costruttori delle richieste interessate.
- Aggiunto `ap_mycall=0/1` alla diagnostica FT8 `DECODEMETRIC`, per mostrare lo stato effettivo usato dal worker.

La modifica corregge un difetto di collegamento verificato. Non dimostra l'eliminazione di ogni possibile falsa decodifica; senza la registrazione originale non è possibile riprodurre il singolo messaggio fantasma segnalato.

#### Controlli operativi più leggibili

- Etichette delle bande, frequenza operativa, selettori di modalità e pulsanti operativi ora usano il grassetto anche quando inattivi. Colori e bordi continuano a indicarne lo stato.
- I menu a tendina stilizzati rispettano il grassetto richiesto dal controllo sia nel valore selezionato sia nelle voci del menu.
- Aggiunto un indicatore SuperFox fra il rapporto ricevuto e il messaggio successivo. Si attiva quando sono abilitati FT8, l'impostazione SuperFox e il ruolo Fox/Hound; il tooltip distingue ricezione e trasmissione. Indica la configurazione, non certifica l'autenticazione o la decodifica SuperFox di un segnale ricevuto.
- Limitata la larghezza del campo Next/TX con un dimensionamento compatto e proporzionato alla scala, lasciando spazio ai controlli adiacenti. Il tooltip mostra il messaggio completo quando il testo visibile viene abbreviato.
- Anche l'etichetta Next/TX e il relativo messaggio sono in grassetto.

### Modifiche incorporate dalla v1.0.614

#### v1.0.615: ricezione SuperFox e recupero della deriva FT2

- Collegato il decoder SuperFox al worker di ricezione FT8 live per il ruolo Hound con SuperFox abilitato.
- Spostato un grande buffer di lavoro SuperFox dallo stack alla memoria dinamica per evitare uno stack overflow riprodotto; corretti anche problemi di compilazione dell'utilità di simulazione SuperFox.
- Corretto un comportamento indefinito nella ricerca opzionale della deriva FT2 e aggiunta la diagnostica `[FT2-DRIFT-RESCUE]` quando recupera un messaggio. La ricerca della deriva resta opzionale.
- Upstream ha verificato il percorso SuperFox con audio generato e un decoder di riferimento; questa release non aggiunge una conferma su un segnale reale di DXpedition.

#### v1.0.616: controlli delle false decodifiche FT8 e accumulo opzionale FT2

- Attivato per impostazione predefinita il filtro appreso dei candidati LDPC FT8; quello FT2 resta opzionale. Le misure upstream riportano meno false decodifiche con un compromesso di sensibilità: circa il 2,4–3,6% di decodifiche genuine in meno sulla registrazione usata per il confronto. Sono misure upstream, non un nuovo benchmark della v1.0.619.
- Conservati i pesi addestrati separatamente per FT8 e FT2 e gli strumenti per raccogliere e valutare candidati LLR reali.
- Respinti i codepoint dei rapporti fuori dall'intervallo −50…+49 supportato dal trasmettitore, nelle varianti normali e TU interessate.
- Introdotta la protezione AP sul proprio nominativo, il cui collegamento mancante al percorso live viene completato nella v1.0.619.
- Attivata per impostazione predefinita la verifica predittiva del messaggio intero, tipo 8, in FT8 e FT2. Verifica un messaggio ricevuto di recente contro una ripetizione temporalmente compatibile. Restano disponibili `DECODIUM_FT8_AP_MSG=0` e `DECODIUM_FT2_AP_MSG=0` per disabilitarla.
- Aggiunto l'accumulo sperimentale di energia fra slot FT2 ripetuti, gestibile in Impostazioni > TX sotto Conservative FT2. È spento per impostazione predefinita e non viene mantenuto fra riavvii; i guadagni registrati provengono da test controllati, non da prestazioni generali confermate in aria.
- Corretta la perdita silenziosa dei messaggi recuperati dai percorsi opzionali di rescue FT2 e assegnato un budget di elaborazione indipendente al recupero della deriva.

#### v1.0.617: eliminazione degli eco CQ nello slot sbagliato

- Aggiunti controlli sulla parità dello slot di trasmissione a tre percorsi di replay FT8 dallo storico. Un messaggio ricevuto di recente non viene più riproposto nella parità opposta soltanto perché età e frequenza coincidono.
- Upstream riporta un passaggio da 66 eco CQ fabbricati a 1 su una registrazione di 510 slot, con le repliche genuine da 2.333 a 2.332. I numeri descrivono quel confronto registrato, non una garanzia per ogni banda o stazione.

#### v1.0.618: impostazioni AP e strumenti di analisi

- Aggiunta la casella FT8 AP in Decoder Options accanto alle altre opzioni del decoder. La v1.0.619 impedisce inoltre ai passaggi live interessati di scavalcare la scelta di spegnerla.
- Estesa la raccolta dei candidati FT8 con dati sintetici di pileup e opzioni di valutazione del filtro. I pesi sostitutivi proposti non sono stati adottati dopo risultati peggiori sul traffico reale registrato; la v1.0.619 non introduce quei pesi sperimentali.

### Verifiche e punti ancora aperti

- Build locale dell'applicazione riuscita su Apple Silicon con Qt 6.11.0.
- Nuova suite AP FT8 superata con 9/9 risultati: scadenza della trasmissione recente, isolamento delle richieste in coda, AP spento, mantenimento dei CQ e decodifica di una chiamata diretta generata con AP sul proprio nominativo disabilitato.
- Verifiche grafiche Qt locali superate per la barra a tre scale e per stato e disposizione di SuperFox. Questi controlli non dimostrano la resa sui monitor Windows né il comportamento con radio reali.
- I controlli selezionati del decoder esistente hanno dato 15 risultati positivi e una discrepanza preesistente nel profilo predefinito: un vecchio test si aspetta 13 passaggi AP, mentre le impostazioni upstream attuali ne selezionano 17. La stessa selezione passa 16/16 con il profilo AP storico. È documentato in `docs/diagnostics/2026-09-08-ft8-mycall-ap.md`; non viene presentato come superamento dell'intera suite.
- Upstream segnala ancora problemi nell'archiviazione dei messaggi diretti e nel rifiuto di alcuni nominativi composti validi. Non sono dichiarati risolti qui.
- Il problema CAT/PTT segnalato con IC-7851 su Windows resta in analisi in attesa del log della sessione interessata. Questa release non contiene modifiche speculative al CAT/PTT.

### Download

GitHub Actions produce l'installer `.exe` Windows x64, i DMG Apple Silicon per macOS Sequoia e Tahoe, i DMG Intel per Ventura, Sonoma e Sequoia e le AppImage Linux x86_64 e aarch64. I workflow macOS e Linux forniscono anche i file SHA-256. I collegamenti GitHub **Source code (zip)** e **Source code (tar.gz)** contengono il codebase completo del tag. I pacchetti vengono caricati al termine dei rispettivi workflow: un asset assente non corrisponde a una build completata.

**Full source comparison / Confronto completo dei sorgenti:** https://github.com/elisir80/Decodium-4.0-Core-Shannon/compare/v1.0.614...v1.0.619
