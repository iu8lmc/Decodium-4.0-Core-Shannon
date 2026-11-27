# Decodium 4 FT2 1.0.1015

## English (UK)

This release brings together the changes made after 1.0.1009 through 1.0.1015. The application code is aligned with the iu8lmc 1.0.1015 release; this fork adds these consolidated release notes.

### Decoding on older computers

- **SuperLDPC SSE2:** on x86-64, the decoder chooses AVX2/FMA when available, then the new SSE2 implementation, and finally the original generic decoder. SSE2 provides a SIMD path for machines without usable AVX2/FMA, including Sandy Bridge, Ivy Bridge, older AMD processors and some virtual machines. The implementation is designed to produce bit-identical results to the AVX2 algorithm in the tested cases. ARM64 retains its existing NEON path.
- **FT2 timing:** the compatibility path for processors without AVX2 retains its two-second decode safety budget. This is a software budget, not a guarantee that every possible operation on every machine will finish by an absolute deadline.
- **Measurements reported upstream:** on a Ryzen 9 5950X with tiers forced, SSE2 took 34–50 µs per codeword, AVX2 32–48 µs and the generic decoder 425–6,200 µs. The FT8 weak-signal test took about 1.2 seconds with SSE2 or AVX2, versus 15–30 seconds with the generic decoder; an emulated Ivy Bridge FT2 test took 430 ms versus 870 ms. These are individual benchmark results, not a promise of a whole-application speed-up. An AVX-only variant was evaluated upstream and not included because it was no faster than SSE2.
- The diagnostic log identifies the selected decoder, for example `decoder=superldpc-sse2`. The older-CPU tier has not yet been validated on a physical processor without AVX2; the reported tests used a modern CPU with forced tiers and simulated CPU features.

### Frequency control

- **Mouse-wheel tuning:** place the pointer over a frequency digit and scroll to change that digit, using the same step and direction as the existing upper/lower digit clicks. Touchpad movements are accumulated too.
- Rapid movements are combined into one CAT frequency change 120 ms after the last event, avoiding a flood of commands and retaining movements while awaiting the radio's response. Wheel tuning is ignored during transmission or TUNE.

### Decode lists and LoTW

- **Row backgrounds:** when a decode matches several colour categories, the first matching category with an enabled background now supplies the background. This allows lower-priority categories such as the LoTW marker to appear when earlier categories have no background enabled.
- **LoTW label:** the former small dot is replaced by a “LoTW” label in embedded, floating and DX-Pedition Full Spectrum views, embedded and floating Signal RX views, and decode windows. It uses the LoTW marker colour and the LoTW user activity list.
- **Settings → Colours:** an option to mark LoTW users is available and off by default; it shares the switch with “LotW Enabled” in the LoTW settings. A note explains background priority. The new text is translated into all 15 interface languages.

### Accessibility and interface

- **Optional interface enlargement:** Settings includes “Enlarge the interface (accessibility)”, off by default. When enabled, select 110%, 125%, 150%, 175% or 200%; the chosen scale takes effect after restarting. With the option off, the interface uses its normal size and the selector is disabled.
- Existing profiles with a saved scale other than 100% retain that value and show the option enabled. Clear the option to return to normal size. The option is translated into all 15 interface languages.
- Text fields, combo boxes, glass buttons, SSTV buttons and TX message buttons have per-control `scaleFactor` hooks. These hooks are not connected to a setting and currently remain at 1; the SSTV button retains its template-based appearance.

### SSTV, start-up and packaging

- Closing the SSTV window releases its lazily loaded workspace and remembers the selected page. A detached window reopened before deferred cleanup is kept alive rather than unloaded.
- Optional `DECODIUM_STARTUP_PROFILE=1` logging records start-up checkpoints and the QML content fingerprint cost; it is off by default.
- Windows packaging excludes Qt SQL drivers whose client libraries are not shipped, while retaining the SQLite driver.
- Regression coverage was updated for SSTV workspace release, decode-colour priority and SuperLDPC tiers. The FFT and downsample thread-lifetime probes report private memory on Windows, and the MinGW weak-signal test uses a larger stack.

### Validation and known limitation

The historical upstream notes describe isolated QML and start-up checks, injected decode/colour tests, real Qt wheel events and an operator-station tuning check. These are not blanket proof of all platforms, radio hardware or over-the-air operation. The previously reported Hound/SuperFox RR73 issue remains open.

## Italiano

Questa release raccoglie le modifiche successive alla 1.0.1009 fino alla 1.0.1015. Il codice dell'applicazione è allineato alla release 1.0.1015 di iu8lmc; questo fork aggiunge le presenti note consolidate.

### Decodifica sui computer meno recenti

- **SuperLDPC SSE2:** su x86-64 il decoder sceglie AVX2/FMA quando disponibile, poi la nuova implementazione SSE2 e infine il decoder generico originale. SSE2 offre un percorso SIMD alle macchine senza AVX2/FMA utilizzabile, comprese Sandy Bridge, Ivy Bridge, CPU AMD meno recenti e alcune macchine virtuali. L'implementazione è progettata per produrre risultati identici bit per bit all'algoritmo AVX2 nei casi verificati. Su ARM64 resta il percorso NEON esistente.
- **Tempi FT2:** il percorso di compatibilità per processori senza AVX2 mantiene il budget di sicurezza di due secondi per decodifica. È un limite software e non garantisce che ogni singola operazione termini entro una scadenza assoluta su qualsiasi macchina.
- **Misure riportate upstream:** su Ryzen 9 5950X, forzando i diversi livelli, SSE2 ha impiegato 34–50 µs per parola, AVX2 32–48 µs e il generico 425–6.200 µs. Il test FT8 a segnale debole ha richiesto circa 1,2 secondi con SSE2 o AVX2, contro 15–30 secondi con il generico; un test FT2 con Ivy Bridge emulato ha richiesto 430 ms contro 870 ms. Sono risultati di prove specifiche, non una promessa di accelerazione dell'intera applicazione. Una variante solo AVX è stata valutata upstream e non inclusa perché non più veloce di SSE2.
- Il log diagnostico indica il decoder selezionato, per esempio `decoder=superldpc-sse2`. Il percorso per CPU meno recenti non è ancora stato verificato su un processore fisico senza AVX2: le prove riportate usano una CPU moderna con livelli forzati e caratteristiche CPU simulate.

### Controllo della frequenza

- **Sintonia con la rotella:** posizionando il puntatore sopra una cifra della frequenza, lo scorrimento modifica quella cifra con lo stesso passo e verso dei clic sulla metà superiore o inferiore. Anche i movimenti del touchpad vengono accumulati.
- I movimenti rapidi sono riuniti in un unico cambio di frequenza CAT 120 ms dopo l'ultimo evento, evitando di saturare il collegamento e conservando gli scatti mentre si attende la risposta della radio. La rotella viene ignorata durante trasmissione o TUNE.

### Liste decode e LoTW

- **Sfondi delle righe:** se una decodifica corrisponde a più categorie di colore, lo sfondo viene preso dalla prima categoria corrispondente che abbia lo sfondo attivato. Così una categoria successiva, come il marker LoTW, può apparire quando quelle precedenti non hanno uno sfondo abilitato.
- **Etichetta LoTW:** il precedente puntino è sostituito da una scritta «LoTW» nelle viste Full Spectrum integrate, staccate e DX-Pedition, nelle viste Signal RX integrate e staccate e nelle finestre decode. Usa il colore del marker e l'elenco di attività degli utenti LoTW.
- **Impostazioni → Colori:** è disponibile un'opzione per segnalare gli utenti LoTW, inizialmente disattivata e condivisa con «LotW Enabled» nelle impostazioni LoTW. Una nota spiega la priorità degli sfondi. Le nuove stringhe sono tradotte in tutte le 15 lingue dell'interfaccia.

### Accessibilità e interfaccia

- **Ingrandimento facoltativo:** nelle Impostazioni è disponibile «Ingrandisci l'interfaccia (accessibilità)», disattivato per impostazione iniziale. Quando è attivo si può scegliere 110%, 125%, 150%, 175% o 200%; la scala si applica dopo il riavvio. Quando è spento, l'interfaccia mantiene la dimensione normale e il selettore è disabilitato.
- I profili con una scala già salvata diversa dal 100% conservano il valore e mostrano l'opzione attiva. Disattivala per tornare alla dimensione normale. L'opzione è tradotta in tutte le 15 lingue dell'interfaccia.
- Campi di testo, caselle combinate, pulsanti glass, pulsanti SSTV e messaggi TX dispongono di agganci `scaleFactor` per singolo controllo. Non sono ancora collegati a un'impostazione e valgono attualmente 1; il pulsante SSTV mantiene il disegno basato sui template.

### SSTV, avvio e pacchetti

- Chiudendo la finestra SSTV viene rilasciata l'area di lavoro caricata su richiesta e viene ricordata la pagina selezionata. Una finestra staccata riaperta prima della pulizia differita resta attiva, senza essere scaricata.
- La variabile facoltativa `DECODIUM_STARTUP_PROFILE=1` registra i punti di avvio e il costo dell'impronta dei contenuti QML; è disattivata per impostazione iniziale.
- Il pacchetto Windows esclude i driver SQL Qt per i quali non vengono distribuite le librerie client, mantenendo il driver SQLite.
- Aggiornati i test di regressione per rilascio dell'area SSTV, priorità dei colori e livelli SuperLDPC. Le sonde sulla vita dei thread FFT e downsample riportano la memoria privata su Windows e il test MinGW per i segnali deboli usa uno stack maggiore.

### Verifiche e limite noto

Le note storiche upstream riportano prove QML e avvii isolati, test con decodifiche e colori iniettati, eventi rotella Qt reali e una prova di sintonia sulla stazione dell'operatore. Non costituiscono una verifica generale di tutte le piattaforme, dell'hardware radio o del funzionamento in aria. Il problema Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
