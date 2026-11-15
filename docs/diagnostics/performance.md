# Diagnostica prestazioni

Per individuare il lavoro che rallenta l'avvio, avviare una build locale con
`DECODIUM_STARTUP_PROFILE=1`. Le righe `[STARTUP-PROFILE]` riportano il tempo
trascorso nelle successive fasi del costruttore del bridge; confrontare più
avvii con impostazioni e macchina uguali. Il tempo del primo frame e quello
necessario per iniziare a ricevere sono misure distinte e non sono inclusi nei
singoli intervalli del bridge.

La fase che comprendeva audio, tema e mappa è suddivisa in checkpoint per
dispositivi, notifiche, tema/propagazione, satelliti, mappa e diagnostica/WAV.
In due avvii isolati con rendering software offscreen, la costruzione del
servizio mappa ha richiesto rispettivamente 210 e 120 ms; i profili erano
isolati e questi valori non sono tempi di avvio complessivi né un confronto
tra database nuovo ed esistente. La sequenza LOW CPU FT8 → FT4 → FT8 è
passata in entrambe le prove. I checkpoint aggiuntivi restano disabilitati
quando `DECODIUM_STARTUP_PROFILE` non è impostata.

Un confronto aggiuntivo con lo stesso profilo isolato ha misurato 211 ms
per la mappa creando il database e 61 ms riaprendolo; è una sola coppia,
non una stima generalizzabile. L'apertura controllava separatamente la
struttura della stessa tabella per ciascuna colonna da migrare. Ora carica
l'elenco colonne una sola volta per tabella e per apertura, aggiorna tale
elenco dopo ogni migrazione e ricontrolla il database se un'operazione ALTER
fallisce mentre un'altra connessione potrebbe averla già eseguita.

Nel probe `tests/tools/map_schema_columns_probe.py`, che usa le funzioni e
l'elenco migrazioni effettivi e SQLite in memoria, 200 verifiche di uno
schema già migrato hanno richiesto 258 ms prima e 26 ms dopo; schema prodotto
e un dato preesistente coincidono. Il confronto dell'intera inizializzazione
in due coppie alternate ha dato 118/102 ms prima e 165/97 ms dopo per un
database nuovo, 41/54 ms prima e 39/47 ms dopo per uno riaperto. La
variabilità non dimostra un risparmio di avvio complessivo. Gli schemi SQLite
dei due eseguibili coincidono, passa il test della mappa con migrazioni e
una ricezione FT8 virtuale LOW CPU ha recuperato il messaggio atteso. I test
non coprono ogni database reale o tutti gli accessi concorrenti.

L'apertura del database ricostruisce anche eventuali associazioni mancanti
fra QSO e locator. Ora evita di tentare l'inserimento di associazioni già
presenti, mantenendo `INSERT OR IGNORE` per quelle da aggiungere. Su 100.000
QSO sintetici già associati, un confronto in memoria a ordine alternato ha
misurato 77/114 ms CPU per la query precedente e 64/63 ms per quella nuova.
Il probe confronta inoltre il risultato su tabelle vuote, parziali e complete,
con valori nulli e associazioni preesistenti. La suite completa della mappa,
inclusi i test di migrazione, e la prova LOW CPU FT8 → FT4 → FT8 su WAV sono
passate. Il beneficio reale dipende dai dati e non è stato quantificato durante
la ricezione: `tests/tools/map_grid_backfill_bench.py` usa dati sintetici e
SQLite in memoria, non un registro dell'utente.

Per verificare l'effetto nell'app, il laboratorio
`tests/tools/startup_map_reuse_lab.py --synthetic-qsos 100000` crea un profilo
isolato, inserisce 100.000 QSO e relative associazioni in un database su disco
e riavvia l'app sullo stesso database. In tre prove per versione, la fase di
inizializzazione mappa ha richiesto 193/171/153 ms con l'eseguibile precedente
alle due ottimizzazioni del database e 113/135/129 ms con quello modificato.
Ogni riavvio ha conservato 100.000 righe in ciascuna tabella; nell'ultima
coppia è stata controllata anche la corrispondenza dei valori di ogni QSO e
associazione. Sono misure di una fase, con dati sintetici uniformi e database
di circa 45 MB, non dell'intero avvio né di ricezione RF. Il confronto include
entrambe le modifiche e non ne separa i contributi.

Con l'eseguibile di riferimento `decodium-before` e quello corrente, lo
stesso QML corrente, CAT e monitor disattivati e file WAV, il laboratorio
`tests/tools/bridge_wav_parity_lab.py` ha confrontato messaggio e frequenza:
due registrazioni FT8 hanno mantenuto rispettivamente 7/7 e 13/13 righe;
tre segnali FT4 sintetici e tre FT2 sintetici hanno mantenuto una riga
ciascuno. Un terzo WAV FT8 ha prodotto zero righe con entrambe le versioni e
non dimostra quindi la conservazione di decode. I dati FT4/FT2 sono singoli
segnali e il confronto non dimostra parità su tutti i segnali, su radio reale
o su altre piattaforme. I risultati dettagliati sono nei file
`ft8-parity-current.json`, `ft4-parity-current.json` e `ft2-parity-current.json`
in `build-baseline/performance/results/`.

Con `DECODIUM_MODE_PROFILE=1` le righe `[MODE-PROFILE]` riportano la durata
sincrona di `setMode`, con modo precedente e richiesto. I passaggi asincroni,
l'effettiva disponibilità dell'audio e il primo slot decodificabile richiedono
osservazioni separate. Le due diagnostiche sono disattivate per default.

La prova di laboratorio `tests/tools/bridge_decodecontext_runtime_lab.py`
accetta `--low-cpu`: avvia un profilo isolato con LOW CPU già abilitato,
decodifica un WAV, passa FT8 → FT4 → FT8 e decodifica un secondo WAV. Il test
usa file e monitoraggio disattivato: non sostituisce la verifica con audio e
radio reali. L'output include i tempi fino alla comparsa dei due decode e il
tempo sincrono dei due cambi modo; i tempi dei decode includono fino a 250 ms
di polling dell'interfaccia di prova. Su Linux e macOS include anche tempo CPU
complessivo e picco di memoria residente del processo durante il test; il tempo
totale include l'attesa iniziale di 12 secondi. Il confronto offline tra decoder è documentato in
`decode_bench/README.md`.

## Ciclo di vita al cambio modo: punti da misurare

Priorità corrente concordata: FT8, FT4 e FT2 e le finestre generali usate
durante la ricezione, con particolare attenzione a LOW CPU. Gli altri modi
sono rinviati; le verifiche già documentate rimangono come riferimento.
La copertura FT4 e FT2 fin qui comprende tre segnali sintetici per modo,
mentre per FT8 comprende anche due registrazioni: non basta a stabilire la
parità per ogni segnale. Le future modifiche ai decoder richiedono confronti
sugli stessi segnali, inclusi casi deboli e affollati ove disponibili.

### Ricerca nella finestra del log

Le due varianti della finestra accorpano le battute ravvicinate nella ricerca:
il filtro viene applicato 180 ms dopo l'ultima modifica, evitando di calcolare
lista e statistiche per ogni carattere. Gli aggiornamenti espliciti e quelli
dovuti a cambiamenti del log restano immediati e cancellano il timer pendente.
`log_search_debounce_lab.py` apre entrambe le varianti in LOW CPU e verifica
con un QSO importato in un log temporaneo che tre modifiche ravvicinate
producano una sola ricerca, con risultato corretto, e che una richiesta
immediata non sia ripetuta. Una ricezione FT8 con audio virtuale ha prodotto
il messaggio atteso. Il numero di ricerche è verificato, non il risparmio CPU
su logbook grandi; non è una verifica FT4/FT2 o su radio reale.

Il filtro C++ del log normalizza ora gli intervalli di data una sola volta per
ricerca e legge i campi del QSO solo quando necessari ai filtri. Il banco
`qso_search_filter_ab.py` confronta le righe complete e il loro ordine su
512 combinazioni di testo, banda, modo e date, usando 6.000 QSO sintetici:
risultati e hash coincidono. Due esecuzioni alternate delle 512 ricerche hanno
impiegato 26,5/27,2 s prima e 3,17/3,29 s dopo, tempo trascorso del banco
isolato su questa macchina. Il risultato non stima l'interfaccia intera né un
singolo logbook reale. La build, il test applicativo di importazione, filtri,
modifica ed eliminazione e il test delle due finestre sono passati.

Un confronto successivo (`qso_search_live_ab.py`) esegue nell'app i due
filtri alternativamente sulla stessa cache temporanea di 12.000 QSO sintetici.
Tre ricerche diverse, ripetute tre volte, hanno richiesto 3.004/3.037 ms con
il filtro precedente e 90/83 ms con quello attuale; le liste complete e il
loro ordine coincidono. Le durate comprendono le nove chiamate al filtro e
sono tempi trascorsi, non CPU dell'intero programma. Monitor e CAT erano
spenti, quindi la prova non misura l'impatto durante una ricezione FT. La
build normale è stata ripristinata; dettagli in
`build-baseline/performance/results/qso-search-live-ab.json`.

### Lettura della cache LoTW

Il caricamento della cache LoTW viene già eseguito in un worker. La ricerca
del separatore CSV tuttavia poteva esaminare tutto il resto del file a ogni
riga priva di virgola; ora è limitata ai byte della riga corrente. Non
cambiano normalizzazione, deduplicazione o risultati accettati. Il probe
`tests/tools/lotw_parser_bounded_ab.py` confronta i due parser su 300 input
sintetici misti, inclusi byte non UTF-8, e su un CSV sintetico con 250.000
nominativi: insiemi ed errori coincidono. Su un file sintetico estremo con
90.000 righe senza separatore, la lettura è passata da circa 3.187 ms a
2 ms; quattro coppie sul file regolare si sovrappongono e non dimostrano
un miglioramento affidabile nel caso normale. Sono tempi della sola funzione
su questa macchina, non dell'avvio completo né della cache reale dell'utente.
La build e la prova applicativa LOW CPU FT8 → FT4 → FT8 su WAV sono passate.

Il tentativo di profilare i thread dell'interfaccia dopo l'avvio usando il
profilatore locale non ha prodotto un campione valido; in una prova il
processo isolato è terminato con un errore durante la profilazione. Non si
traggono conclusioni sulle cause o sull'effetto del worker SSTV da tale prova.

In un'esplorazione LOW CPU senza monitoraggio né CAT, il laboratorio
`tests/tools/settings_prewarm_resource_lab.py` ha confrontato la preparazione
anticipata della finestra impostazioni con una variante che la rimanda alla
prima apertura. Per rendere breve la prova il timer di preparazione è stato
ridotto da 30 a 5 s in entrambi i casi. In una sola coppia, prima di aprire le
impostazioni la RSS dell'intero processo era 555,9 MB con preparazione e
553,1 MB senza; la finestra risultava visibile rispettivamente dopo 73 e
159 ms. Non è un confronto alternato né una misura durante RX; la differenza
di memoria è troppo piccola rispetto al processo e alla variabilità per
giustificare una modifica. Il comportamento di produzione resta invariato.
Risultato locale: `build-baseline/performance/results/settings-prewarm-explore.json`.

Nel campionamento LOW CPU inattivo `tests/tools/runtime_thread_profile.py`,
sempre senza monitoraggio e CAT, il processo ha utilizzato 1,97 s CPU su
10,05 s trascorsi e RSS finale di circa 522 MiB. Il thread principale ha
accumulato 0,63 s fra quelli presenti a entrambi i campionamenti; la somma
dei thread elencati non comprende quelli creati o terminati nell'intervallo.
La finestra principale era stata creata circa otto secondi prima dell'inizio
del campione, quindi possono essere ancora presenti attività di avvio; non è
una misura di ricezione né dello stato stabile su altre macchine. Il worker
di condivisione SSTV esegue una verifica ogni 750 ms anche da disabilitato,
ma questa produce anche aggiornamenti su scadenze, elementi persistenti e
handoff recuperati: non è stato fermato senza una verifica della continuità.
Risultato locale: `build-baseline/performance/results/idle-lowcpu-current.json`.

I worker FT vengono creati quando servono e mantenuti per cambi successivi.
Il passaggio di modo invalida le code, richiede la cancellazione dei lavori
FT8/FT2 e richiede la cancellazione cooperativa FT4; le scansioni hash del modo precedente vengono
disabilitate. Un thread conservato non implica necessariamente CPU attiva.

WSPR controlla se una richiesta è stata ritirata prima di eseguirla, dopo
l'attesa del mutex DSP condiviso e durante la ricerca. Le misure e le verifiche
di pulizia associate a questa cancellazione sono descritte più sotto.

Prima misura esplorativa sul computer i7-3520M: `wsprd` autonomo compilato con
GCC `-O2`, con un file C2 sintetico contenente otto segnali e le opzioni
`-C 500 -o 4 -d`, ha decodificato gli otto messaggi in 4,30 s di tempo reale
e 4,25 s di CPU, con build e altri benchmark sospesi. Il worker applicativo
detiene il mutex intorno a `wsprd_run`; questo rende plausibile un'attesa per
un decoder successivo. La prova autonoma non misura il cambio modo della GUI
e usa opzioni di compilazione diverse dalla build applicativa. Il C2 e l'output
sono conservati localmente in `build-baseline/performance/`.

La variante OSD WSPR con righe del generatore compattate è stata confrontata
con l'originale su 180 vettori deterministici (profondità 0–5, anche con
maschere AP): codeword, distanza, profondità ed errori sono identici. Il target
opzionale `wspr_osd_vector_probe` consente di ripetere il confronto tra due
build sulla stessa macchina. Sul C2 sintetico a otto segnali, con `wsprd`
autonomo compilato con GCC `-Os`, due esecuzioni alternate hanno prodotto gli
stessi otto messaggi: 7,31 e 6,49 s prima, 3,52 e 3,65 s dopo. Un controllo
di risorse su tre esecuzioni per versione non ha rilevato allocazioni o file
aperti residui fra quelli conteggiati dal probe; non include le cache FFTW.
Questa prima ottimizzazione riduceva il lavoro senza cancellare un WSPR già in corso; la cancellazione cooperativa successiva è descritta sotto.

Sul [WAV WSPR di riferimento 150426_0918.wav](https://downloads.sourceforge.net/project/wsjt/samples/WSPR/150426_0918.wav) indicato dai test del progetto
(SHA-256 `0f1e41e6a94fefd3df9efd8234dd6342dfd7e6675700f70e79e69eec955537da`), le due build autonome
`-Os` hanno prodotto le stesse otto righe in due prove a ordine alternato.
I tempi osservati sono 17,10 e 17,11 s prima, 10,13 e 10,24 s dopo; i test
sono stati eseguiti con `-C 500 -o 4 -d` e altri benchmark sospesi. La prova
riguarda il file di riferimento, non la ricezione RF o la GUI.

### Cambio WSPR → FT8 con decodifica in corso

Sullo stesso computer e con LOW CPU, il laboratorio
`tests/tools/wspr_switch_runtime_lab.py` avvia il WAV di riferimento WSPR,
attende che lo stato di decodifica sia attivo, cambia modo dopo 1,5 s e apre
un WAV FT8 noto. Il confronto usa build locali equivalenti salvo le modifiche
già descritte; CAT e monitor audio sono disabilitati. In due prove alternate
(originale/modificata e modificata/originale), il cambio sincrono ha richiesto
30–36 ms per l'originale e 29–44 ms per la modificata; il tempo fino al
decode FT8 rilevato è stato rispettivamente 15,22–15,46 s e 8,73–8,76 s.
La rilevazione del decode avviene ogni 250 ms. Entrambe le versioni hanno
recuperato il messaggio atteso; il log diagnostico conferma che il risultato
WSPR della vecchia sessione è stato ritirato. È coerente con la minore durata
misurata per WSPR, ma due esecuzioni di un solo WAV non stabiliscono un
risultato universale. La prova usa file e non acquisizione radio; non dimostra
che una decodifica già iniziata venga cancellata. I log sono in
`build-baseline/performance/results/wspr-switch-*`.

### Cancellazione di una richiesta WSPR ritirata

Il worker può ora notificare al decoder che la sua richiesta è stata
ritirata durante un cambio modo. I controlli nei cicli di ricerca lasciano
terminare la singola operazione corrente, quindi attraversano la pulizia di
FFTW, memoria e file e non pubblicano un risultato parziale né sovrascrivono
la tabella hash con i dati parziali. Una richiesta non ritirata segue gli
stessi percorsi di decodifica. Il controllo non è istantaneo durante singole
operazioni interne e non è una garanzia di latenza massima.

Sul WAV WSPR di riferimento, il test WSPR → FT8 in LOW CPU ha osservato
474 ms fino al decode FT8 dopo il cambio; ripetendo e tornando poi a WSPR
ha osservato 727 ms e otto righe WSPR. Il polling è ogni 250 ms. Prima
della cancellazione cooperativa le due prove equivalenti richiedevano
8,73–8,76 s; i risultati sono limitati a questa macchina, questo file e
al laboratorio senza radio. La decodifica WSPR senza cancellazione ha
restituito le stesse otto righe del precedente eseguibile. I test della coda,
delle risorse, della cancellazione e del riavvio passano; il conteggio delle
risorse non comprende le cache FFTW. I log sono in
`build-baseline/performance/results/wspr-switch-cancel*` e
`build-baseline/performance/results/wspr-switch-return*`.

Anche FST4 e Q65 controllano la validità delle richieste. La cancellazione in
corso è intenzionalmente disabilitata per FST4W e per Q65 con media coerente,
perché questi casi mantengono informazioni utilizzabili da decodifiche
successive. I test `test_fst4_decode_cancellation` e
`test_q65_decoder_continuity` passano: verificano tra l'altro che una richiesta
FST4 già ritirata non attenda il mutex DSP e che la cancellazione Q65 non
invalidi richieste nuove. I test non dimostrano che interrompere FST4W o Q65
con media coerente preservi la sensibilità futura; tali casi continuano a
terminare e possono occupare risorse dopo il cambio modo.

Un laboratorio applicativo LOW CPU dedicato,
`tests/tools/fst4w_switch_runtime_lab.py`, apre un WAV FST4W-120, passa a
FT8 mentre la decodifica è ancora attiva, apre un WAV FT8 noto e torna poi a
FST4W. Con il segnale WSJT-X a SNR simulato -10, il cambio sincrono ha
richiesto 16–19 ms e il decode FT8 è comparso dopo circa 7,5–7,7 s in tre
prove; nella prova completa al ritorno è riapparso `K1ABC FN42 33`. CAT e
monitor sono disattivati, la rilevazione usa polling a 250 ms e il WAV non è
una ricezione RF.

In una prova, il nuovo checkpoint disabilitato per default
`DECODIUM_MODE_PROFILE=1` ha registrato 6.920 ms di attesa del mutex DSP nel
reset FT8 accodato prima della richiesta; il successivo worker FT8 ha invece
registrato zero ms di attesa e 477 ms di decodifica. Il dispatch è avvenuto
circa 18 ms dopo l'apertura del WAV e il primo polling dopo 240 ms. Questo
identifica l'attesa del reset nella prova, non un blocco continuo
dell'interfaccia. Rimane aperta la progettazione di una condivisione della
risorsa che permetta a FST4W di completare e conservare il suo stato, senza
interferire con i decoder successivi. Log locali:
`build-baseline/performance/results/fst4w-switch-reset-profile.log`.

#### Cessione sperimentale del mutex FST4W

Una variante separata, generata da `tests/tools/prepare_fst4w_yield_lab.py`,
cede temporaneamente il mutex ai confini fra candidati soltanto dopo il
ritiro di una richiesta FST4W. Nella prova applicativa LOW CPU sullo stesso
WAV rumoroso, il decode FT8 è comparso dopo 497 ms, contro circa 7,5–7,7 s
nelle prove precedenti; il checkpoint del reset FT8 ha registrato 4 ms di
attesa. Il messaggio atteso è stato nuovamente trovato tornando a FST4W. È
una sola esecuzione per la variante, con polling a 250 ms e senza radio.

In prove separate senza un decoder concorrente, il worker ritirato ha
completato e salvato gli stessi nominativi prima e dopo la variante, sia per
il WAV rumoroso sia per quello pulito; il risultato ritirato non è mostrato
in entrambi i casi, perciò questo confronto non verifica tutte le righe
interne della prima ricerca. Tra i nominativi salvati nel caso pulito vi
sono i falsi candidati già documentati. Il codice sperimentale non è nella
build normale: resta da proteggere e verificare l'accesso concorrente allo
stato FST4 condiviso, alle cache e alle altre entry point, oltre a verificare
la continuità dei decode sotto interleaving effettivo con altri modi e in
caso di chiusura. Il singolo esperimento non basta a garantire l'assenza di
perdite. Log locale: `fst4w-switch-yield-lab.log`; confronti dei file di
nominativi in `fst4w-retire-before-yield.json`, `fst4w-retire-yield-lab.json`
e `fst4w-yield-clean-retire.json` sotto `build-baseline/performance/results/`.

Una successiva strumentazione della sola build di laboratorio registra le
righe interne prima che quelle della sessione ritirata vengano scartate.
Sul WAV rumoroso, sia nel cambio verso FT8 sia nel cambio verso FT4, le sei
righe e i due nominativi salvati dalla prima ricerca coincidono con la
versione senza cessione, a parte l'ora dello slot. Nella prova FT4 il decode
atteso è apparso dopo 7,22 s senza cessione e 0,73 s con cessione; tornando a
FST4W è riapparso il messaggio previsto. Queste prove usano lo stesso WAV e
una esecuzione per configurazione; il confronto include i falsi candidati già
segnalati. `tests/tools/fst4w_compare_retired_lab.py` confronta le righe
catturate conservandone ordine e contenuto, escludendo soltanto l'ora.

Un controllo negativo ha evidenziato un problema della cessione semplice:
forzando un reset dello stato FST4 durante una ricerca ritirata, nel WAV pulito
il file conteneva due nominativi invece dei cinque del riferimento. La
variante `--protect-session` del generatore di laboratorio serializza la
decodifica FST4 e quel reset con un mutex dedicato: nella stessa prova il
reset ha atteso la fine e tutti e cinque i nominativi coincidevano. Con questa
protezione, una prova FST4W → FT8 → FST4W ha mantenuto le sei righe interne e
il messaggio atteso, con il primo FT8 a 0,50 s. Il reset concorrente è un test
artificiale delle entry point, non un evento dimostrato nell'uso normale.

La versione protetta iniziale riacquisiva il mutex con un'attesa bloccante.
Un controllo negativo ha mantenuto il mutex occupato da un altro thread e
richiesto la chiusura: FST4W non terminava nei tre secondi del controllo.
La versione sperimentale successiva verifica la chiusura durante l'attesa e
ha terminato in 52 ms con il mutex ancora occupato, salvando gli stessi quattro
nominativi già appresi; il controllo usa il WAV pulito e un solo interleaving.
Nel test applicativo FST4W → FT8 → FST4W la nuova variante ha mantenuto le
sei righe interne e il messaggio previsto, con il primo FT8 a 0,50 s.

La protezione resta sperimentale: il mutex dedicato copre la decodifica e il
reset, ma le entry point ausiliarie che usano lo stato o la cache FST4
richiedono ancora un audit dell'ordine dei lock. Non è quindi stata promossa
nella build normale. Risultati locali:
`fst4w-yield-reset-negative.json`, `fst4w-yield-reset-protected.json` e
`fst4w-yield-protected-retired-ab.json`, `fst4w-yield-shutdown-ab.json` e
`fst4w-yield-cancellable-retired-ab.json` in `build-baseline/performance/results/`.

L'audit ha individuato due funzioni ausiliarie che condividono la cache di
sincronizzazione e una diagnostica che usa lo stato persistente. La variante
di laboratorio `--protect-session --protect-aux` isola la cache per thread e
usa uno stato locale per la diagnostica. Un controllo negativo ha però
mostrato che la prima versione per thread non invalidava le altre cache dopo
un reset: in una sequenza artificiale con cambio di modulazione l'indice era
649 anziché 1953. La variante successiva propaga l'invalidazione tramite una
generazione condivisa; lo stesso test e tre confronti fra due ricerche
ausiliarie simultanee sono passati bit per bit. È passato anche il controllo
di chiusura di due worker in attesa del DSP.

Con quest'ultima variante, nella prova applicativa FST4W → FT8 → FST4W il
primo FT8 è apparso dopo 0,50 s; le sei righe interne e i due nominativi
salvati della ricerca ritirata coincidono con il riferimento, a parte l'ora.
Sono controlli di laboratorio su una configurazione e un WAV, non una prova
di parità generale o di tutte le entry point esterne. La variante rimane
esclusa dalla build normale. Risultato locale:
`build-baseline/performance/results/fst4w-yield-epoch-retired-ab.json`.

Anche il passaggio FST4W → FT4 → FST4W con la stessa variante e lo stesso
WAV ha conservato le sei righe interne e i due nominativi della ricerca
ritirata; il messaggio atteso al ritorno è presente. Il primo FT4 è apparso
dopo 0,47 s, contro 7,22 s nel riferimento: una sola prova per versione,
che misura il tempo di risposta in questo scenario e non il consumo CPU.
Risultato: `build-baseline/performance/results/fst4w-yield-epoch-ft4-ab.json`.

Con la stessa build di laboratorio, passando invece a FT2 e tornando a
FST4W, la variante disattivata e quella attiva hanno conservato le stesse
sei righe interne e gli stessi due nominativi; il messaggio atteso al ritorno
è presente in entrambe. Il primo FT2 è apparso rispettivamente dopo 9,22 e
2,23 s. Anche qui si tratta di una prova per variante su un WAV, non di una
misura del consumo CPU né di parità generale. Risultato:
`build-baseline/performance/results/fst4w-yield-epoch-ft2-ab.json`.

Per verificare un diverso momento della ricerca, il banco accetta ora
`--switch-delay-ms`: con cambio FST4W → FT8 dopo 5 s, anziché 1,5 s,
le due varianti hanno prodotto le stesse sei righe interne e salvato gli
stessi due nominativi, con il messaggio atteso al ritorno. Il primo FT8 è
apparso dopo 3,99 s senza cessione e 0,50 s con cessione. È un singolo
confronto su un WAV: non dimostra la parità per altri tempi o segnali.
Risultato: `build-baseline/performance/results/fst4w-yield-epoch-delay5000-ab.json`.

Il banco riporta anche il tempo CPU utente+sistema e il picco RSS del
processo figlio. In una coppia LOW CPU FST4W → FT8 → FST4W, sulla stessa
build di laboratorio, la variante disattivata ha consumato 34,01 s CPU e
raggiunto 588760 KiB RSS; quella attiva 33,51 s CPU e 596688 KiB RSS.
La durata complessiva è stata 41,59 e 41,10 s; il primo FT8 è apparso dopo
7,49 e 0,74 s. Le sei righe interne e i due nominativi coincidono e il
messaggio atteso al ritorno è presente. I valori includono avvio e tutta la
sequenza: una sola coppia non dimostra una variazione di CPU o memoria,
né isola il costo della cessione. Log `fst4w-cpu-baseline1.log`,
`fst4w-cpu-yield1.log` e confronto `fst4w-cpu-parity1.json` nella directory
`build-baseline/performance/results/`.

Una seconda coppia, eseguita in ordine inverso, ha richiesto 34,77 s CPU
con cessione e 33,65 s senza; i picchi RSS sono stati 598712 e 591292 KiB.
Il primo FT8 è apparso dopo 0,74 e 7,75 s. Anche qui le sei righe interne,
i due nominativi e il messaggio atteso al ritorno coincidono. Le due coppie
non mostrano un risparmio CPU ripetibile; il picco RSS è maggiore nelle due
prove con cessione, ma i campioni non bastano a stabilirne la causa o la
persistenza. Log `fst4w-cpu-yield2.log`, `fst4w-cpu-baseline2.log` e
confronto `fst4w-cpu-parity2.json` nella stessa directory.

Il banco può attendere il completamento del decoder successivo con
`--capture-next-complete` e registrarne tutte le righe prima del ritorno a
FST4W. Su una coppia FST4W → FT8 → FST4W, FT8 ha prodotto una riga per
variante, identica in tutti i campi salvo gli orari della sessione; le sei
righe FST4W ritirate e i due nominativi salvati coincidono. Il ritorno ha
ritrovato il messaggio atteso in entrambe. Il WAV FT8 contiene un solo
messaggio: questa prova non copre ricezioni affollate. Log
`fst4w-next-full-baseline.log` e `fst4w-next-full-yield.log` nella stessa
directory.

La stessa prova con il WAV sintetico affollato `multi-snr-16-seed1001.wav`
ha prodotto quattro righe FT8 in entrambe le varianti, identiche in tutti i
campi dopo esclusione degli orari e riordino per messaggio e frequenza. Le
sei righe FST4W ritirate e i due nominativi coincidono e il messaggio atteso
è presente al ritorno. Il primo FT8 è apparso dopo 7,49 s senza cessione e
0,51 s con cessione. La prova copre una sola registrazione sintetica; il
segnale debole aggiuntivo non compare nemmeno nel riferimento. Confronti:
`fst4w-next-multi-rows-ab.json` e `fst4w-next-multi-retired-ab.json` nella
stessa directory.

Con la registrazione FT8 `181201_180245.wav`, il confronto applicativo
FST4W → FT8 → FST4W ha prodotto sette righe FT8 per variante, identiche in
tutti i campi dopo esclusione degli orari e riordino per messaggio e
frequenza. Coincidono le sei righe FST4W ritirate e i due nominativi; il
messaggio atteso è presente al ritorno. Il primo FT8 è apparso dopo 7,99 s
senza cessione e 1,00 s con cessione. È una sola registrazione e non
garantisce parità per ogni ricezione. Confronti:
`build-baseline/performance/results/fst4w-next-real-rows-ab.json` e
`build-baseline/performance/results/fst4w-next-real-retired-ab.json`.

Il confronto delle righe del modo successivo è ripetibile con
`tests/tools/fst4w_compare_next_lab.py`: ignora solo i tre campi di orario,
considera tutti gli altri campi e confronta le righe come multinsiemi,
quindi riconosce anche duplicati mancanti o aggiunti. Sui log FT8 reale e
sintetico ha confermato rispettivamente 7/7 e 4/4 righe identiche. Controlli
artificiali hanno verificato che una riga mancante, un duplicato aggiunto,
un campo cambiato e log vuoti o duplicati non vengano accettati come
confronti equivalenti.

Per preparare questa verifica è stata generata una fixture FST4W sintetica
di 120 s con `fst4_performance_fixture`, messaggio `K1ABC FN42 33`, 1500 Hz,
DT 0 e profondità 3 nel worker autonomo. Il controllo dei bit codificati e
del loro unpack passa, ma non dimostra il percorso audio. Nel test audio a
SNR simulato -10, tre decodifiche non hanno restituito righe; nel test senza
rumore (`snr=99` nel simulatore), tre decodifiche hanno restituito ciascuna
sette righe, nessuna contenente il messaggio generato. I tempi sono stati
circa 3,7–4,6 s e 20,5–21,1 s rispettivamente, ma queste esecuzioni non sono
un test valido di sensibilità o di sicurezza della cancellazione. Va prima
verificata la fixture contro una sorgente indipendente: il risultato non
identifica ancora se la causa sia generazione, parametri o decoder e non è
un confronto con la versione originale. `tests/tools/check_fst4w_synthetic.py`
conserva tutte le righe e fallisce se il messaggio atteso manca.
I risultati locali sono in `build-baseline/performance/results/fst4w-synthetic-*`.

Una seconda fixture pulita, costruita con forma d'onda reale e inserimento
lineare nel periodo, ha invece restituito `K1ABC FN42 33` in tre esecuzioni.
Il confronto dei campioni ha mostrato che il simulatore anticipava il segnale
di 24.000 campioni rispetto a questa fixture per DT=0 e periodo 120 s. Il
simulatore ora applica lo spostamento nel verso corretto; una sua nuova
fixture pulita ha restituito il messaggio atteso in tre esecuzioni. Una prova
separata verifica che aumentando DT di 0,5 s il segnale si sposti in avanti
di 6.000 campioni sia per FST4-15 sia per FST4W-120. Il risultato spiega
l'anomalia della fixture precedente, ma generatore e decoder condividono parti
del progetto: serve ancora un riferimento esterno per una verifica completa.
Il decoder ha prodotto anche altre righe nel caso pulito; non sono state
interpretate come messaggi corretti. Probe:
`tests/tools/fst4_sim_alignment_probe.py` e `tests/tools/check_fst4w_synthetic.py`.

Un confronto offline aggiuntivo usa il simulatore e `jt9` del pacchetto
Ubuntu WSJT-X 2.7.0, estratto localmente senza installazione. Sul suo segnale
pulito FST4W-120, periodo 120 s, depth 3, frequenza 1500 Hz e tolleranza
20 Hz, entrambi i decoder hanno trovato `K1ABC FN42 33`. Il worker autonomo
di Decodium ha restituito sette righe per passaggio, `jt9` una; le altre sei
righe non corrispondono al messaggio generato. Il worker ha richiesto circa
26,5–26,8 s per passaggio e `jt9` circa 3,1 s per processo. Interfacce e
implementazioni non sono identiche e il confronto riguarda un solo segnale,
non l'applicazione completa. Dettagli e hash sono in
`build-baseline/performance/results/fst4w-reference-compare.json`.

Una profilazione del worker ha individuato lavoro nelle metriche dei bit,
nella correlazione e nella ricerca OSD. Il calcolo del massimo per un bit
ora salta blocchi di indici che non corrispondono al bit richiesto, mantenendo
l'ordine dei confronti e senza cambiare le operazioni numeriche selezionate.
Il confronto delle funzioni reali con dati casuali, zeri, infiniti e NaN è
identico bit per bit con `-O2` e `-Os`; dodici array completi di metriche
FST4 su segnali sintetici sono anch'essi identici bit per bit. Nel confronto
autonomo con il segnale pulito WSJT-X tutte le sette righe sono rimaste
identiche e i tre passaggi sono scesi da 26,5–26,8 s a 22,6–22,7 s. Su un
segnale WSJT-X a SNR simulato -10 le tre righe sono rimaste identiche in una
prova per versione, da 19,0 a 17,1 s; il messaggio previsto è presente.
Un segnale FST4-15 sintetico ha mantenuto la stessa riga. La build e il test
di cancellazione FST4 passano. Queste misure sono del worker autonomo e non
quantificano il miglioramento con radio reale o con altri segnali.

La successiva riduzione delle metriche degli otto simboli riutilizza i massimi
intermedi: elabora lo stesso buffer in posizione e conserva l'ordine della
scelta in caso di valori uguali. I NaN vengono trattati come nel calcolo
precedente. Il probe `tests/tools/fst4_metric_tree_ab.py` confronta bit per
bit 32 array completi, inclusi zeri con segno, NaN e infiniti, sia a `-O2`
sia a `-Os`. Anche dodici array completi di metriche FST4 sono identici al
riferimento. Sono casi mirati, non tutte le possibili entrate.

Sul worker FST4W, quattro esecuzioni alternate del WAV WSJT-X a SNR simulato
-10 hanno mantenuto le stesse tre righe: 9,00/8,76 s prima e 7,22/7,33 s dopo.
Sul WAV pulito le sette righe sono rimaste identiche: 17,98/18,63 s prima e
16,06/15,71 s dopo. Un WAV FST4-15 ha conservato la stessa riga, in
9,27/9,20 s prima e 8,79/8,58 s dopo. Nel confronto applicativo FST4W LOW CPU
sul WAV rumoroso sono rimaste tre righe, tra cui il messaggio atteso, senza
righe mancanti; una singola prova per versione ha misurato circa 11,0 s prima
e 9,5 s dopo, inclusi 800 ms del timer del laboratorio. Le prove usano WAV e
non comprendono radio reale; le righe extra FST4W rimangono falsi candidati
non confermati. La build e il test di cancellazione sono passati. Risultati
locali: `fst4w-metric-tree-noisy.json`, `fst4w-metric-tree-clean.json`,
`fst4-metric-tree.json` e `fst4w-metric-tree-app.json` in
`build-baseline/performance/results/`.

Una prova applicativa FST4W-120 LOW CPU con l'eseguibile precedente non ha
terminato entro il limite del laboratorio: l'app usa tolleranza 250 Hz, non i
20 Hz del worker isolato. Alla chiusura forzata ha segnalato che il worker non
si era arrestato e la distruzione di un mutex bloccato. Questa prova non
stabilisce la parità o il tempo della versione modificata nell'app; il caso di
arresto e la ricerca a banda più ampia richiedono ulteriori verifiche.
Strumenti: `tests/tools/fst4_metric_max_ab.py`,
`tests/tools/fst4_bitmetrics_full_ab.py` e
`tests/tools/compare_fst4w_reference.py`.

Con il segnale WSJT-X a SNR simulato -10, in LOW CPU nell'app e con tolleranza
250 Hz, entrambe le versioni del worker hanno prodotto sei righe ma il filtro
semantico applicava le regole dei messaggi diretti anche ai beacon FST4W,
scartando `K1ABC FN42 33` perché `FN42` non è un secondo nominativo. Il
filtro ora riconosce, solo in FST4W, i formati beacon `call grid potenza` e
`call potenza`, inclusi hash e placeholder quando consentiti dal chiamante.
Sul WAV isolato la versione precedente non mostrava righe; quella modificata
mostra il messaggio atteso a 1500 Hz e altre due righe non contenute nel
segnale generato. Queste ultime sono falsi candidati già prodotti dal worker,
ora visibili grazie al filtro più adatto al protocollo: richiedono ulteriori
verifiche e non sono decode confermati. Il decoder WSJT-X sullo stesso file
ha restituito soltanto il messaggio generato.

Il confronto di 224 combinazioni FT8/FT2 dei filtri è identico; test dedicati
coprono beacon con locator, senza locator e con hash. La build e il cambio
FT8 → FT4 → FT8 LOW CPU sono passati. Sul file FST4W i tempi applicativi
rilevati (inclusi 800 ms del timer del laboratorio) sono stati circa 22,1 s
prima e 12,7 s dopo, una sola prova per versione: le build differiscono anche
per le altre ottimizzazioni e la misura non è un confronto con radio reale.
Il caso di arresto durante un FST4W lungo è stato affrontato separatamente.

Alla chiusura dell'app il worker FST4/FST4W riceve ora un segnale di arresto
indipendente dal ritiro della sessione. FST4W continua quindi a completare il
lavoro nei normali cambi di modo, come prima; può invece fermarsi alla chiusura
anche mentre attende il mutex DSP. I nominativi già appresi vengono salvati
anche all'uscita anticipata dal ciclo. La cancellazione viene osservata
ai punti di controllo del decoder, non interrompe immediatamente ogni singola
operazione DSP.

Il probe `tests/tools/fst4w_shutdown_calls_lab.py` ha interrotto il worker
dopo 10 s durante la decodifica di un segnale WSJT-X pulito, usando una
directory temporanea inizialmente vuota. Il file scritto conteneva `K1ABC
FN42`, la coppia attesa, e altre tre voci; tutte e quattro sono presenti anche
nel file prodotto da una seconda decodifica completata, durata 18,7 s. Le
altre voci non sono presenti nel segnale generato e sono falsi candidati già
prodotti dal decoder: anche la loro memorizzazione richiede un'indagine
separata. La prova conferma il salvataggio anticipato per questa fixture, non
la correttezza di tutte le chiamate apprese né la persistenza in ogni fase
dell'arresto. Il ritardo può richiedere regolazione su hardware diverso.
Risultato locale: `build-baseline/performance/results/fst4w-shutdown-calls.json`.

#### Indagine sui falsi candidati FST4W

La ricerca OSD corrente cerca un candidato che superi il CRC tra quelli
esaminati. Nel [codice WSJT-X 2.7.0](https://github.com/WSJTX/wsjtx/blob/v2.7.0/lib/fst4/fastosd240_74.f90)
la ricerca sceglie invece il candidato più vicino e poi controlla il CRC.
La differenza è una possibile causa dei falsi candidati, non prova che sia
l'unica causa nel ricevitore completo.

Il laboratorio `tests/tools/fst4w_crc_selection_lab.py` confronta l'OSD
attuale con una variante isolata che cerca prima il candidato più vicino tra
gli stessi tentativi; non è un'implementazione completa dell'algoritmo WSJT-X.
Su 840 vettori LLR gaussiani deterministici, con una sola codeword nota e
Keff=66, entrambe le varianti hanno recuperato 381 volte la codeword corretta;
quella attuale ha restituito 23 codeword errate, la variante zero. Non sono
WAV né un modello completo della ricezione, e le intensità dei vettori non
sono valori SNR in dB. Inoltre, sui 120 vettori più forti la variante ha
impiegato circa 812 ms contro 76 ms dell'attuale: una sola misura sequenziale
del solo OSD. I risultati non giustificano una modifica in produzione né
garantiscono la conservazione dei segnali deboli. Dati locali:
`build-baseline/performance/results/fst4w-crc-selection.json`.

Separatamente è stata provata una potatura della ricerca che preserva il
criterio CRC attuale. Il confronto con il codice precedente ha dato risultati
identici su 360 vettori e il WAV FST4W rumoroso ha mantenuto le stesse tre
righe. I tempi del worker alternato sono stati 13,06/10,18 s prima e
10,62/11,09 s dopo; si sovrappongono e non dimostrano un miglioramento.
La potatura è stata rimossa. Risultato locale:
`build-baseline/performance/results/fst4w-pruning-paired.json`.

Una successiva profilazione esplorativa del worker FST4W con simboli ha
indicato lavoro anche nel CRC della ricerca OSD. La versione locale del CRC
usa ora un registro a 24 bit invece di aggiornare un array di 25 elementi,
con lo stesso polinomio e la stessa gestione dei valori non nulli in ingresso.
Il test `tests/tools/fst4_crc_local_ab.py` confronta 11.500 vettori di diverse
lunghezze e ingressi nulli con il codice precedente; i risultati sono identici.
Il confronto OSD completo, su 360 vettori di FST4 e FST4W con maschere AP,
conserva messaggi, codeword, distanze ed errori bit per bit. In una misura
isolata i 360 vettori hanno richiesto 208 ms prima e 182 ms dopo; non è una
misura dell'intera applicazione.

Sul worker FST4W con il WAV WSJT-X a SNR simulato -10, quattro esecuzioni a
ordine alternato hanno mantenuto le stesse tre righe: 9,88/10,34 s prima e
9,37/8,79 s dopo. Le righe comprendono il segnale generato e due falsi
candidati già descritti. È una sola fixture su questa macchina; non quantifica
il risparmio in ricezione reale né garantisce parità per tutti i segnali.
La modifica non cambia la selezione dei candidati e non risolve l'indagine sui
falsi decode. Dati locali:
`build-baseline/performance/results/fst4w-crc-paired.json`.
Un confronto FST4-15 ha mantenuto la stessa riga, in 8,97 s prima e 8,85 s
dopo; una sola coppia non dimostra un vantaggio temporale. La build
dell'applicazione, il test di cancellazione e la prova applicativa LOW CPU
FT8 → FT4 → FT8 su WAV sono passati.

Il test `test_fst4_decode_cancellation` controlla le condizioni di arresto,
l'isolamento fra thread, il mantenimento della semantica FST4W al ritiro e
l'interruzione sia in attesa del mutex sia durante una richiesta attiva. In
`tests/tools/fst4w_shutdown_runtime_lab.py`, con WAV WSJT-X pulito, LOW CPU e
nessun collegamento radio, l'app ha ricevuto la chiusura 1,5 s dopo l'inizio
della decodifica ed è terminata regolarmente circa 0,38 s dopo la richiesta,
senza i messaggi di terminazione forzata o mutex bloccato. È una prova singola
e non stabilisce un limite per tutti i segnali e le durate FST4W.

Su due esecuzioni applicative complete del WAV a SNR simulato -10 sono state
prodotte le stesse tre righe, coincidenti con quelle registrate prima di questa
modifica, incluso `K1ABC FN42 33`. Le altre due righe restano falsi candidati
non confermati; questa verifica non dimostra la parità generale dei decode.
Risultati locali: `build-baseline/performance/results/fst4w-shutdown-parity.json`.

È stato anche provato a riutilizzare le correlazioni già calcolate per i
simboli, evitando un secondo calcolo identico. I dodici array completi di
metriche coincidevano bit per bit, ma quattro esecuzioni alternate sullo
stesso FST4W pulito hanno richiesto 22,61/23,16 s prima e 23,12/22,61 s dopo.
La variabilità non mostra un vantaggio affidabile: questa seconda modifica è
stata rimossa. La build ripristinata e il test di cancellazione FST4 passano;
il confronto è ripetibile con `tests/tools/fst4_worker_paired.py` conservando
due eseguibili compilati per le varianti da confrontare.

La ricerca OSD FST4/FST4W ora costruisce la codeword iniziale una volta e
applica o annulla con XOR le righe associate ai bit modificati durante la
ricerca. L'ordine dei candidati, i confronti delle distanze e il controllo CRC
rimangono invariati. Il confronto dei sorgenti precedenti e correnti su 360
vettori deterministici, con maschere AP, profondità fino a 4 e 243 decode
riusciti, ha prodotto identici messaggi, codeword, tipo, errori e distanza bit
per bit. Il test isolato ha misurato 378 ms prima e 237 ms dopo; è un totale
per quei vettori, non un tempo dell'app.

Nel worker autonomo, quattro esecuzioni alternate sul segnale FST4W pulito
WSJT-X hanno restituito le stesse sette righe, in 22,75/23,14 s prima e
19,34/18,94 s dopo. Su un segnale FST4W a SNR simulato -10 hanno conservato
le stesse tre righe, in 16,50/16,54 s prima e 9,91/9,87 s dopo. Un segnale
FST4-15 ha mantenuto la stessa riga, con tempi sovrapposti: 9,28/8,93 s prima
e 9,13/8,93 s dopo. Sono misure su due segnali FST4W e uno FST4, non una
garanzia su altri segnali o sul tempo dell'app. La build, i confronti completi
delle metriche e i test di cancellazione FST4/continuità Q65 passano.
Il confronto OSD è `tests/tools/fst4_osd_incremental_ab.py`; i risultati del
worker sono nei file `fst4w-osd-paired.json`, `fst4w-osd-noisy-paired.json`
e `fst4-osd-paired.json` in `build-baseline/performance/results/`.

Una prova aggiuntiva nello stesso processo, con cancellazione di un WAV
seguita da due input C2, ha riprodotto un arresto dovuto a piani FFT già
distrutti e non azzerati. La pulizia ora azzera i piani dopo averli liberati;
la prova passa, come i test WSPR di coda e risorse. Dopo la correzione,
l'app ha completato WSPR → FT8 → WSPR mostrando otto righe nel secondo WSPR;
la prova autonoma sul WAV di riferimento ha restituito le stesse otto righe
precedenti. La sequenza mista è una prova sulle risorse, con C2 sintetico
silenzioso, non una verifica della sensibilità C2.

La stessa sequenza mista è stata ripetuta con un C2 sintetico contenente
otto segnali: entrambe le esecuzioni successive alla cancellazione hanno
prodotto le stesse otto righe dell'esecuzione di riferimento. Il controllo
verifica anche che una tabella hash preesistente resti identica subito dopo
la cancellazione, oltre al conteggio di allocazioni e file monitorati. Il C2
con segnali è un campione sintetico locale, non una registrazione RF.

La cancellazione viene ora controllata anche prima di preparare i file e
subito dopo la lettura dell'input. I test cancellano separatamente in queste
due fasi e durante la ricerca, verificano la conservazione della tabella hash
e ripetono la decodifica; tutti i sette test WSPR mirati passano. Dopo la
modifica, il WAV WSPR di riferimento continua a produrre le stesse otto righe
e la prova applicativa WSPR → FT8 → WSPR completa con otto righe al ritorno.

### Cache QML e avvio

Con Qt 6.11.0 locale, due avvii controllati con stessa directory di cache,
QML invariato, Xvfb e rendering software hanno richiesto 9,75 e 9,95 s per
caricare il componente principale. La cache era abilitata, ma il secondo
avvio ha segnalato `Checksum mismatch` per `Main.qml` e altri componenti e
ne ha riscritto i file `.qmlc`. Il tempo non equivale all'avvio su desktop
reale; nei log i tempi di creazione della finestra sono riportati a parte.

Una riproduzione minimale con l'eseguibile `qml` della stessa Qt mostra il
mismatch anche fuori dall'app per `ToolTip.visible`/`ToolTip.text`; usando un
oggetto `ToolTip` esplicito la prova non mostra il mismatch. In `Main.qml`
ci sono numerosi tooltip condivisi: sostituirli tutti con oggetti separati
potrebbe cambiare il comportamento e aumentare il numero di oggetti e la
memoria, quindi questa osservazione non giustifica ancora una modifica di
produzione. Il probe ripetibile è `tests/tools/qml_disk_cache_probe.py`;
servono confronto visivo, consumo di memoria e prove di avvio prima di
scegliere un'alternativa. Il codice Qt verifica il checksum delle dipendenze
prima di riutilizzare un componente compilato ([Qt QQmlTypeData](https://github.com/qt/qtdeclarative/blob/6.11/src/qml/qml/qqmltypedata.cpp)).

Una soluzione successiva usa `QtQuick.Templates.ToolTip` per le proprietà
associate, mantenendo il tooltip condiviso di Qt senza creare un popup per
ogni controllo. Nel probe minimale le due forme leggono lo stesso testo,
ritardo e durata; il test QML verifica visibilità e passaggio tra due tooltip.
Tutti i 45 componenti modificati sono stati caricati nell'engine dell'app;
questo non equivale a un controllo visivo di ogni schermata.

Con la stessa build locale, cache isolata e rendering software, il secondo
avvio del riferimento ha impiegato 9,55 s per il caricamento del componente
principale e circa 14,93 s CPU nel processo; la versione modificata ha
impiegato 0,113 s e circa 6,58 s CPU. Il picco RSS è stato rispettivamente
circa 546 e 542 MiB. Il primo avvio della versione modificata ha richiesto
9,66 s di caricamento: il beneficio osservato richiede una cache già
popolata. Un prototipo precedente ha mostrato rispettivamente 9,55 s e
0,163 s per il secondo avvio. Il programma era arrestato dopo un tempo
prefissato, quindi i tempi CPU includono anche altre attività di avvio e non
vanno interpretati come CPU della sola compilazione QML.

Queste prove usano Qt 6.11 e Xvfb; non dimostrano lo stesso guadagno su ogni
piattaforma o backend grafico. All'epoca di questa misura l'AppImage disabilitava la cache QML per
precauzione; la successiva modifica e le sue verifiche sono riportate sotto. Nell'app sono passati il caricamento dei componenti,
LOW CPU FT8 → FT4 → FT8, WSPR → FT8 → WSPR e il confronto di una registrazione
FT8 di riferimento (13 messaggi uguali). Sono passati anche il test QML dei
tooltip e sette test mirati su cambio modo, thread e WSPR. I risultati sono
in `build-baseline/performance/results/tooltip-*`.

### Cache QML dell'AppImage: isolamento e montaggi

Un probe minimale (`tests/tools/qml_cache_mount_probe.py`) simula due
montaggi diversi con lo stesso percorso QML stabile. Il secondo avvio riusa
la cache. Modificando il contenuto mantenendo lo stesso timestamp, però, Qt
6.11 riutilizza il componente vecchio; usando una directory di cache diversa
carica quello nuovo. Il risultato spiega perché non basta riabilitare la
cache per l'AppImage: un'identità basata solo su percorso, versione dichiarata
o timestamp non garantisce che il contenuto sia quello corretto.

Un collegamento simbolico globale al montaggio corrente permette il riuso nel
probe ma non è una soluzione sufficiente per istanze simultanee: se il
montaggio indicato termina, l'altra istanza potrebbe dover caricare un
componente rimasto sul montaggio ormai sparito. Occorre valutare uno snapshot
QML stabile e immutabile per contenuto, pubblicazione atomica, integrità,
concorrenza e ripiego al percorso originale. A quel punto non era ancora stato abilitato alcun cambiamento della cache
AppImage; le verifiche della soluzione successiva sono riportate sotto. Le prime righe `Error loading ... No
such file or directory` nel probe indicano solo una cache inizialmente vuota.

### Snapshot QML dell'AppImage

Su Linux, quando la cache non è esplicitamente disabilitata, l'app prepara
una copia delle tre directory QML applicative indicizzata da SHA-256 del
contenuto, versione Qt, ABI e versione applicativa. Non copia file `.qmlc` o
`.jsc` adiacenti né segue collegamenti simbolici. La pubblicazione avviene
tramite rinomina atomica e una copia esistente viene verificata prima dell'uso.
Se la preparazione o la verifica fallisce, l'app usa i file montati e
disabilita la cache QML, mantenendo il comportamento precedente. Gli snapshot
vecchi non vengono eliminati automaticamente, perché altre istanze potrebbero
ancora usarli.

I test coprono contenuto diverso con timestamp identico, otto creatori
concorrenti, copia danneggiata, input non valido e collegamenti simbolici.
Con le librerie dell'AppImage estratta, una simulazione a due montaggi ha
verificato riuso, rimozione del primo montaggio, aggiornamento e ripiego per
copia danneggiata. Un'AppImage locale di laboratorio è stata anche montata
realmente: senza impostare un percorso cache speciale, il caricamento QML
è passato da 9,60 s al primo avvio a 0,161 s al secondo, in Xvfb con
rendering software. Un'altra AppImage di laboratorio con una sonda QML ha
decodificato un WAV FT8 in LOW CPU. Non sono prove con radio reale, né
una release pubblicata; i pacchetti e i log sono nella cartella locale
`build-baseline/performance/`.

La verifica di integrità rifiuta anche bytecode adiacente inatteso e moduli
aggiunti alla radice dello snapshot, per evitare che Qt carichi file non
compresi nel checksum o importi moduli estranei. In entrambi i casi l'avvio
ripiega sui file originali senza cache, come per una copia danneggiata.

Nell'ultima build, la simulazione con le librerie AppImage ha misurato la
preparazione/verifica dello snapshot: 317 ms alla prima copia e 92 ms al
riuso, contro 9,40 s e 114 ms rispettivamente per il caricamento QML. La
copia danneggiata è stata rifiutata e l'app ha caricato i file montati senza
cache. Una seconda AppImage locale, montata realmente con cache predefinita,
ha misurato 348/124 ms per preparazione/verifica e 9,90 s/163 ms per il
caricamento QML. Un'altra immagine di prova con la stessa build finale ha
completato un decode FT8 in LOW CPU. Tutti i tempi si riferiscono a questo
computer e al display virtuale.

Se l'albero applicativo contiene un plugin nativo o un componente solo
compilato senza sorgente, la copia viene rifiutata e l'app torna al percorso
montato; i test verificano anche questi casi. Gli snapshot sono conservati
nella cache dell'utente e le versioni precedenti non vengono cancellate
automaticamente mentre potrebbero essere usate da altre istanze.

Il calcolo del checksum usa l'overload `QByteArray` disponibile anche nelle
versioni Qt 6 precedenti alla 6.3, al posto dell'overload `QByteArrayView`.
Il test confronta il digest con un valore calcolato indipendentemente; la
build Qt 6.11 ha inoltre riutilizzato lo stesso snapshot già creato prima
della modifica. Questa è una verifica dell'API e del digest, non una build
completa su Qt 6.2, che non è installata nell'ambiente di prova.

### Pulsazioni del periodo in LOW CPU

Con qualità UI media, due animazioni dell'indicatore e della scritta ACTIVE
rimanevano attive anche in LOW CPU. Nel profilo LOW CPU ora l'indicazione
rimane statica; nel profilo normale le pulsazioni rimangono abilitate.
Attivando LOW CPU durante una pulsazione, l'opacità viene ripristinata: una
prova nell'app ha verificato indicatore e scritta visibili a opacità 1,
indicatore inattivo a 0,5, scritta inattiva nascosta e ripresa delle
animazioni disattivando LOW CPU.

Con periodo attivo e pannello visibile forzati nel laboratorio, senza
monitor audio, quattro esecuzioni alternate di 10 s in Xvfb e rendering
software hanno misurato 3,00/2,96 s CPU con le pulsazioni e 2,12/2,34 s
senza. Il tempo comprende il resto del processo: è una misura locale dello
scenario grafico, non una percentuale garantita di risparmio in ricezione.
Il test LOW CPU con WAV e passaggio FT8 → FT4 → FT8 è passato. Strumenti e
log: `tests/tools/period_indicator_runtime_bench.py`,
`tests/tools/period_indicator_toggle_lab.py` e
`build-baseline/performance/results/period-visible-*`.

### Ricezione FT8 tramite audio virtuale

Un laboratorio isolato avvia un server PipeWire/Pulse privato, un'uscita
virtuale e una sorgente virtuale collegata a tale uscita. In LOW CPU, con CAT
e trasmissione disattivati, riproduce il WAV FT8 di riferimento all'inizio di
un periodo e acquisisce l'audio attraverso l'ingresso dell'applicazione. Il
monitor Pulse diretto non è sufficiente su questa configurazione Qt: la
sorgente virtuale dedicata viene invece acquisita correttamente.

Sul computer di prova, sia `decodium-before` sia `decodium` hanno mostrato
`CQ K1ABC FN42` a 1500 Hz. In due esecuzioni per versione, alternate per ordine,
la CPU totale del processo durante circa 28,2 secondi è stata 4,85 e 5,88 s per
il riferimento, 5,06 e 5,32 s per la versione modificata. Entrambe le prove
usano lo stesso QML modificato e il rendering software offscreen: confrontano
quindi i binari nelle condizioni del laboratorio, non l'applicazione originale
completa. I campioni sono troppo pochi e variabili per dimostrare un risparmio
o una regressione della CPU in ricezione; non sono una prova con radio reale.

Lo strumento è `tests/tools/live_virtual_audio_lab.py`. Va eseguito dalla root
in una sessione D-Bus privata, ad esempio con:

```sh
QT_QUICK_BACKEND=software dbus-run-session -- python3 tests/tools/live_virtual_audio_lab.py decodium
```

È possibile inoltre configurare `LD_LIBRARY_PATH` per la build locale. Modifica
temporaneamente il QML nella directory di build e lo ripristina all'uscita:
non va eseguito in parallelo con altri test che usano quel file. I log e le
misure della sessione locale sono in `build-baseline/performance/results/`.

Campionando `/proc` ogni 250 ms durante un'ulteriore ricezione virtuale,
su circa 28,1 s il processo ha usato 5,47 s CPU: circa 4,49 s sono stati
attribuiti al thread principale e 0,53 s a `FT8DecodeWorker`. Gli altri
thread rilevati hanno consumato meno. Il campionamento può perdere thread
molto brevi e l'interfaccia offscreen usa rendering software: questi numeri
servono a scegliere dove indagare, non a stimare il carico su ogni desktop.
Disattivare sperimentalmente l'animazione della sola barra temporale ha dato
5,62 s contro 5,77 s nella prova con animazione, differenza troppo piccola
rispetto alla variabilità osservata per attribuirle un beneficio.

In un'altra prova diagnostica, lo stesso segnale è stato decodificato con il
waterfall temporaneamente nascosto: 4,70 s CPU totali e 3,99 s nel thread
principale, contro 5,47 e 4,49 s in una precedente prova con waterfall
visibile. Una seconda coppia ha dato 4,73 s con waterfall visibile e 4,75 s
con waterfall nascosto, quindi non conferma un risparmio stabile. Una terza
coppia con controlli più rigorosi ha dato rispettivamente 5,74 e 4,87 s. Queste
misure non permettono di quantificare il costo del pannello o attribuirgli la
variabilità; la visualizzazione rimane disponibile per l'utente.
Lo strumento permette di ripetere questa prova con la variabile
`DECODIUM_LAB_HIDE_WATERFALL=1`; verifica lo stato del pannello, LOW CPU,
ingresso virtuale, monitor, trasmissione e messaggio durante la misura.
Un tentativo con debugger collegato durante la riproduzione ha
interferito con l'audio e non ha prodotto una decodifica: non è valido per
valutare prestazioni o parità di ricezione.

### Indicatori di monitoraggio in LOW CPU

Anche il simbolo del pulsante Monitor e il LED nella finestra waterfall
separata pulsavano durante la ricezione in LOW CPU. Ora rimangono statici e
completamente visibili; nel profilo normale riprendono a pulsare. Il cambio
profilo durante la pulsazione ripristina l'opacità. Un test con audio virtuale
alterna due volte i profili, verifica entrambi gli indicatori e decodifica
`CQ K1ABC FN42` a 1500 Hz. Passa anche la prova applicativa LOW CPU FT8 → FT4
→ FT8, senza CAT né trasmissione.

Prima della modifica, in due confronti alternati con la stessa ricezione
FT8 virtuale e rendering software offscreen, disabilitare sperimentalmente
le pulsazioni ha dato 2,91 e 4,53 s CPU su circa 28,1 s, contro 4,71 e
5,09 s con le pulsazioni. Dopo la modifica e il test dei cambi profilo,
una prova ha dato 3,20 s. Il risultato mostra un beneficio in questo
laboratorio, con variazioni tra esecuzioni; non è una percentuale garantita
su altri sistemi o con radio reale. La riproduzione usa
`DECODIUM_LAB_MONITOR_TOGGLE=1` con `live_virtual_audio_lab.py`.

Anche l'indicatore CAT ora resta statico e visibile in LOW CPU, ripristinando
l'opacità se il profilo cambia durante la pulsazione; il colore continua a
indicare lo stato CAT. In due confronti alternati con ricezione FT8 virtuale,
la sola animazione CAT forzata a scopo diagnostico ha dato 4,51/5,03 s CPU
in circa 28 s, contro 4,32/3,73 s con l'animazione ferma. La simulazione
forzava l'animazione dell'indicatore senza collegare una radio: il beneficio
varia e non quantifica il costo di un collegamento CAT reale. Dopo la modifica,
la prova con `DECODIUM_LAB_MONITOR_TOGGLE=1 DECODIUM_LAB_CAT_TOGGLE=1` ha
verificato due cambi profilo, opacità e ripresa della pulsazione simulata,
oltre al decode FT8. La prova FT8 → FT4 → FT8 in LOW CPU è passata.

### Frequenza della barra temporale

In LOW CPU è stato provato un aggiornamento visuale ogni 200 ms invece degli
attuali 100 ms. In due confronti alternati con ricezione FT8 virtuale, i tempi
CPU del processo su circa 28 s sono stati 4,44/3,28 s a 100 ms e 2,54/4,36 s
a 200 ms; ogni prova ha decodificato il messaggio di riferimento. In un test
separato della barra attiva con rendering software e ricezione disabilitata,
le due esecuzioni di 10 s hanno dato 1,77/2,24 s CPU a 100 ms e 1,91/2,25 s
a 200 ms. La variabilità non mostra un miglioramento affidabile: l'intervallo
di produzione rimane 100 ms. Il laboratorio di ricezione consente la prova
con `DECODIUM_LAB_TIMING_INTERVAL=200`, senza cambiare il codice applicativo.

### Chiamata periodica agli indicatori

Il timer degli indicatori chiamava ogni 500 ms `refreshLedStatus()`, un
metodo vuoto. La chiamata è stata rimossa; il timer continua a eseguire
`resetLedStatus()` alla fine del periodo come prima. Il test applicativo
degli indicatori e la sequenza FT8 → FT4 → FT8 in LOW CPU sono passati.
Si tratta di una piccola eliminazione di lavoro inutile: non è stato
misurato un risparmio di CPU attribuibile a questa sola modifica.

### Pulsazioni dei pulsanti di ripristino

Dieci pulsanti per ripristinare finestre minimizzate continuavano a pulsare
con LOW CPU attivo e qualità UI media. Ora in LOW CPU sono statici e visibili;
tornando al profilo normale riprendono a pulsare. Una prova applicativa ha
verificato due cambi di profilo, l'arresto e la ripresa delle dieci animazioni
e l'opacità dei dieci pulsanti dopo l'arresto. La sequenza FT8 → FT4 → FT8
in LOW CPU su WAV è passata dopo la modifica.

Nel laboratorio `tests/tools/minimized_indicator_runtime_bench.py`, con tutte
e dieci le finestre forzate minimizzate, ricezione disabilitata, qualità UI
media e rendering software offscreen, due prove alternate di circa 10 s
hanno usato 4,27/4,06 s CPU con le pulsazioni e 2,15/2,13 s senza. Il tempo
CPU comprende l'intero processo. Questo scenario intenzionalmente intenso
non quantifica il risparmio durante la normale ricezione, con meno finestre
minimizzate o con un altro backend grafico.

La prova è stata ripetuta con ricezione FT8 tramite sorgente audio virtuale
privata, monitor attivo, CAT e trasmissione spenti, sempre con dieci finestre
forzate minimizzate e rendering software offscreen. In quattro esecuzioni
alternate, ciascuna di circa 28,2 s, il processo ha usato 10,12/9,81 s CPU
con le pulsazioni forzate e 3,44/2,99 s con le pulsazioni ferme. Tutte hanno
decodificato `CQ K1ABC FN42` a 1500 Hz; i campioni verificano che tutti i
dieci pulsanti restino visibili e che le animazioni siano nello stato atteso.
È uno scenario grafico intenzionalmente intenso con un unico segnale audio
virtuale: non quantifica il beneficio con meno finestre, un altro renderer o
una radio reale e non dimostra la parità per ogni segnale. Si ripete con
`DECODIUM_LAB_MINIMIZED=1` in `live_virtual_audio_lab.py`; aggiungere
`DECODIUM_LAB_MINIMIZED_ANIMATED=1` per forzare le pulsazioni nel laboratorio.

Con una sola finestra minimizzata (Log), nello stesso laboratorio di
ricezione virtuale e in quattro esecuzioni alternate di circa 28,1 s, il
processo ha usato 5,22/5,47 s CPU con la pulsazione forzata e 3,32/3,09 s
con il pulsante statico. Ogni esecuzione ha recuperato lo stesso segnale e il
probe ha verificato un solo pulsante visibile e lo stato dell'animazione.
Anche queste misure riguardano un singolo WAV, rendering software offscreen
e questa macchina; non sono una percentuale garantita su altri sistemi.
Per ripeterle usare anche `DECODIUM_LAB_MINIMIZED_COUNT=1`.

### Indicatori delle funzioni avanzate

Gli indicatori Coherent Averaging, Neural Sync e Turbo Feedback mantengono
colore e stato, ma in LOW CPU non pulsano; nel profilo normale le pulsazioni
rimangono attive. Il test applicativo abilita i tre indicatori in un profilo
isolato e verifica due cambi di profilo, la ripresa delle animazioni, la loro
successiva disattivazione, l'opacità e lo stato attivo. Dopo la modifica è
passata anche la sequenza FT8 → FT4 → FT8 in LOW CPU su WAV.

Con tutte e tre le spie forzate attive, monitor disabilitato, timer di reset
fermato a scopo di misura, qualità UI media e rendering software offscreen,
due confronti alternati di circa 10 s hanno usato 2,98/3,02 s CPU con le
pulsazioni e 2,24/2,02 s con le spie statiche. È CPU dell'intero processo
in uno scenario artificiale: non misura il costo delle funzioni di decode,
né il risparmio in ricezione. Le tre funzioni sono normalmente disabilitate
per default. Il laboratorio è `tests/tools/advanced_indicator_runtime_bench.py`.

### Normalizzazione delle righe legacy

La normalizzazione delle righe del decoder legacy ricostruiva a ogni riga
con intestazione la stessa espressione regolare per separare le annotazioni.
La usa ora come oggetto costante condiviso, come già avveniva per le altre
espressioni della funzione. Il probe `tests/tools/decode_annotation_regex_ab.py`
confronta 5.009 righe sintetiche, compresi casi di spaziatura, caratteri non
ASCII e righe senza intestazione: i testi coincidono. In quattro esecuzioni
alternate su 100.000 normalizzazioni, la funzione isolata ha richiesto
2,12–2,32 s prima e 0,10–0,11 s dopo. Sono circa 20 microsecondi risparmiati
per riga nel probe, non una misura di fluidità dell'app; le righe usate sono
sintetiche. Il confronto dei percorsi ALL.TXT e la build sono passati.
Nell'app LOW CPU una registrazione FT8 ha restituito le stesse 7 righe prima
e dopo; un'altra non ha prodotto righe in nessuna versione e non fornisce
evidenza di conservazione dei decode. La prova applicativa non copre tutte
le condizioni radio né dimostra il costo della sola normalizzazione legacy.

### Controllo di recupero JT/FST4 durante FT

Il timer dello spettro invoca anche il recupero delle decodifiche JT/FST4;
non viene rallentato perché questo potrebbe ritardare tale recupero. Nei modi
esattamente `FT8`, `FT4` e `FT2` la verifica ora termina subito: nessuno dei
due selettori JT/FST4 accetta questi modi. Gli altri nomi, inclusi FST4 e
JT65, seguono il percorso precedente. Il confronto di comportamento del
metodo è passato per modi FT, JT65 e FST4, stati di ricezione, finestre
temporali, audio insufficiente e richieste duplicate; un probe dei selettori
include anche varianti di maiuscole e impostazioni CAT. La build e la prova
applicativa FT8 → FT4 → FT8 LOW CPU con WAV sono passate.

Il probe isolato ha misurato 500.000 controlli FT8 in circa 423 ms prima e
5,5 ms dopo: il risparmio per chiamata è piccolo e questa misura non prova un
beneficio apprezzabile sull'intero processo. I test non coprono ricezione RF
né esecuzione reale della procedura JT/FST4 su ogni piattaforma. Probe:
`tests/jt_fallback_mode_probe.cpp` e `tests/tools/bridge_jtdecodefallback_ab.py`.

### Durata del buffer grafico waterfall

Il contatore del buffer circolare dello spettro cresceva di un'unità per
campione e, a 12.000 campioni/s senza reset, poteva superare il limite di un
intero a 32 bit dopo circa 49,7 ore. Ora viene ricondotto a un intervallo
limitato dopo ciascun blocco, conservando l'indice circolare e l'informazione
se il buffer è pieno. La modifica riguarda il buffer grafico; i campioni per
la decodifica e la frequenza del timer non vengono cambiati.

Un probe con sanitizer confronta contenuto, posizione e quantità disponibile
con un contatore di riferimento a 64 bit, incluso il superamento simulato
del vecchio limite e una posizione corrispondente a 30 giorni. Passano i
confronti dei percorsi audio legacy, la build e una ricezione FT8 virtuale
LOW CPU con waterfall visibile e messaggio atteso. Non è una prova continua
di 30 giorni né una verifica con radio reale. Il probe è
`tests/waterfall_ring_position_probe.cpp`.

### Copia della finestra audio FT2 asincrona

La finestra di 45.000 campioni viene ora copiata dal buffer circolare in uno
o due blocchi contigui, conservando la stessa posizione finale acquisita e
senza cambiare frequenza o profondità delle decodifiche. Il probe
`ft2_audio_ring_snapshot_probe.cpp` confronta 1.010 finestre, comprese le
posizioni ai bordi e contatori grandi, e passa anche con ASan/UBSan. In due
coppie di 2.000 copie sintetiche, il tempo trascorso era 88/136 ms con il
ciclo precedente e 9,8/10,3 ms con i blocchi; non è una misura dell'intera
app o una verifica delle condizioni concorrenti durante ogni cambio audio.

Con audio virtuale LOW CPU, waterfall visibile, CAT e TX spenti, il segnale
FT2 debole atteso è stato decodificato. In un confronto separato della scena
affollata tra le build prima e dopo la copia a blocchi sono apparse le stesse
otto righe con le stesse frequenze. È una prova per ciascuna build, non una
garanzia su tutti i segnali, i dispositivi audio o una ricezione RF. Le misure
CPU complessive di questa coppia non isolano un risparmio stabile.

### Dimensione del gruppo OpenMP FT8

Nella ricerca OSD FT8 è stato sperimentato il riuso della codeword quando un
successivo candidato differisce per un solo bit. Il confronto ha prodotto
messaggi, codeword ed errori identici su 600 vettori deterministici; in due
coppie alternate il tempo totale è stato 2,15/2,14 s prima e 2,37/2,25 s
dopo. Una seconda variante, che cerca da destra la transizione necessaria
al prossimo candidato, ha anch'essa mantenuto i 600 risultati ma i tempi si
sovrapponevano (2,26/2,12 s prima, 2,13/2,30 s dopo). Nessuna delle due
varianti ha dimostrato un risparmio e il codice precedente è stato
ripristinato. Sono prove del solo OSD, non del decoder o dell'app completi.

In precedenza il decoder impostava correttamente il budget FT8 richiesto,
ma la regione OpenMP preparatoria per assegnare la priorità usava sempre il
massimo di 24 thread. Ora la regione preparatoria usa il massimo budget
effettivamente richiesto finora dal worker, lasciando invariato il massimo
configurabile e il budget della decodifica. Così un avvio e un uso solo in
LOW CPU non creano subito il gruppo massimo; dopo un budget più alto il gruppo
già creato resta riutilizzabile anche tornando a un budget più basso.
Anche `threads_active` nella diagnostica riporta tale budget, invece del
limite massimo OpenMP.

Nel laboratorio `tests/tools/mode_resource_profile.py --decode`, LOW CPU,
con WAV per FT8 → FT4 → FT2 → WSPR → FT8, il numero di thread Linux con
nome FT8 osservati dopo la prima decodifica è passato da 25 a 3. La RSS del
processo alla fine della sequenza è stata circa 712 MiB prima e 637,5/665,2 MiB
in due esecuzioni successive; è una misura complessiva, non una stima isolata o
una garanzia per tutte le macchine. Entrambe le sequenze hanno prodotto righe
per ogni WAV, incluse otto righe WSPR e un risultato FT8 al ritorno.

I confronti applicativi in LOW CPU contro l'eseguibile di riferimento hanno
prodotto gli stessi messaggi e frequenze su due registrazioni FT8: 7/7 e
13/13 righe, senza differenze; anche il WAV FT8 sintetico è identico. Una
prova tramite ingresso audio virtuale ha decodificato il segnale atteso e
registrato `threads_req=2 threads_active=2`. Il test del budget thread è
passato. Queste prove non coprono tutte le condizioni radio né sistemi Windows.

Il laboratorio `tests/tools/ft8_thread_transition_lab.py` verifica inoltre,
nello stesso processo Linux, sei decodifiche successive dello stesso WAV con
richieste manuali alternate 1 → 3 → 1 → 3 → 1 → 3; la lista viene svuotata
prima di ogni decodifica. Con `--low-toggle` alterna invece LOW CPU e profilo
normale, osservando richieste effettive 2 → 3 → 2 → 3 → 2 → 3 su questa
macchina a quattro thread logici. Ogni passaggio ha prodotto nuovamente il
messaggio atteso ed entrambe le esecuzioni si sono concluse regolarmente.
Il laboratorio confronta frequenza e messaggio della fixture, senza provare
tutte le configurazioni OpenMP o il comportamento su Windows.

Campionando gli identificativi Linux dei thread con nome FT8 nei cambi
LOW CPU ↔ normale è emerso che dimensionare ogni volta la regione preparatoria
esattamente sul budget corrente ricreava un thread al passaggio 2 → 3. Dopo
l'introduzione del massimo già richiesto, gli identificativi osservati sono
rimasti stabili dopo il primo ampliamento nei sei passaggi; ciascun WAV ha
prodotto il messaggio atteso. Anche con questa variante le due registrazioni
FT8 di riferimento hanno restituito rispettivamente 7/7 e 13/13 righe
identiche. La stabilità osservata riguarda il runtime OpenMP Linux della prova,
non è una garanzia per altri runtime o Windows.

Un'ulteriore prova `--low-toggle --idle-window` ha atteso circa 5,98 s dopo
il ritorno a LOW CPU: i quattro thread Linux con nome FT8 osservati erano in
attesa all'inizio e alla fine e non hanno accumulato tick CPU nel periodo
campionato (risoluzione 10 ms). Con `DECODIUM_FT8_PRIORITY_PROFILE=1`, la
preparazione della priorità nelle sei decodifiche ha richiesto da 0,204 a
4,345 ms di tempo trascorso; dopo il ritorno a LOW CPU il gruppo preparatorio
era di tre thread e i due campioni hanno richiesto 0,248 e 0,204 ms. Sono
misure brevi di una singola esecuzione, senza monitoraggio audio, non un
limite massimo né una garanzia sugli altri runtime OpenMP. La diagnostica
aggiuntiva è disabilitata per default.

### Preparazione FT8 da ALL.TXT all'avvio

La lettura di ALL.TXT continua a raccogliere gli stessi nominativi per gli
hash FT8. La preparazione separata dei candidati CQ, usata soltanto per il
replay esplicitamente abilitato su sorgente JTDX, viene ora saltata quando
non può essere utilizzata. Non è stato spostato o ritardato il caricamento
hash rispetto all'avvio del decoder.

Il laboratorio `tests/tools/knowncq_optimization_lab.py` confronta conteggio,
ordine e digest degli hash e delle chiamate alla funzione di seed CQ. Con un
file sintetico di 8,82 MiB, quindi superiore alla coda di 8 MiB letta dal
programma, i risultati coincidono per ALL.TXT e JTDX-ALL.TXT con replay
spento e acceso; il replay abilitato su sorgente non JTDX resta disabilitato.
Per ALL.TXT senza replay, due prove alternate hanno richiesto 1,426/1,468 s
prima e 0,752/0,727 s dopo. Per JTDX con replay abilitato i tempi sono stati
1,575/1,537 s e 1,565/1,606 s. Sono misure del metodo su dati sintetici,
non tempi complessivi di avvio né una stima per ogni log dell'utente.

Il confronto del precedente harness verifica file assente, limite d'età,
percorso JTDX, hash e replay CQ. Una prova dell'app ha letto un ALL.TXT
sintetico isolato e inizializzato l'hash atteso. In LOW CPU, una registrazione
FT8 applicativa ha mantenuto 13/13 righe identiche all'eseguibile di
riferimento. Per ripetere il confronto del metodo, esportare la versione
precedente di `DecodiumBridgeKnownCq.cpp` e passarla con `--before`; il test
richiede Qt6Core e usa solo file temporanei sintetici.

La validazione dei token per gli hash usa ora un controllo diretto dei
caratteri ASCII ammessi e delle griglie, evitando tre espressioni regolari per
token. Un confronto con la versione immediatamente precedente, sullo stesso
file sintetico e con replay disattivato, ha misurato 0,735/0,709 s prima e
0,574/0,567 s dopo. Hash e seed CQ, inclusi ordine e conteggio, coincidono
nelle quattro combinazioni di sorgente/replay; coincidono anche su oltre
3.000 casi sintetici aggiuntivi, inclusi caratteri non ASCII, rapporti,
griglie, separatori e lunghezze limite. Il precedente harness e la prova di
avvio con hash isolato passano. La registrazione FT8 applicativa in LOW CPU
mantiene 13/13 righe rispetto al riferimento. I tempi riguardano il metodo,
non l'intero avvio; i dati sintetici non esauriscono tutti i log possibili.

È stato valutato anche un filtro preliminare delle righe che non contengono
`FT8`. Su un log sintetico con circa il 75% di righe di altri modi, con replay
disabilitato, due prove hanno misurato 0,285/0,301 s prima e 0,265/0,283 s
con il filtro. Con il 9% di righe di altri modi, invece, hanno misurato
0,536/0,563 s prima e 0,529/0,634 s con il filtro. Gli output sono identici,
ma i tempi non dimostrano un vantaggio affidabile per chi usa soprattutto FT8;
il filtro non è stato mantenuto. Il laboratorio accetta
`--other-mode-percent` per ripetere il confronto con altre proporzioni.

È stata valutata anche l'eliminazione di conversioni in maiuscolo successive
alla normalizzazione della riga. Gli output dei casi sintetici, compresi
spazi e caratteri Unicode, coincidevano; nel percorso predefinito i tempi
oscillavano però in entrambe le direzioni. Per esempio, su ALL.TXT con replay
spento, quattro misure sono state 0,535/0,585/0,534/0,533 s prima e
0,462/0,579/0,572/0,527 s dopo. Il beneficio non è dimostrato e la variante
non è stata mantenuta.

### Verifica della build locale

Il tag contiene le sezioni della guida ma non i file
`doc/user_guide/en/wsjtx-main.adoc` e `wsjtx_improved.adoc` richiesti da
`doc/CMakeLists.txt`. Perciò la build con documentazione attiva si interrompe.
La build locale è stata riconfigurata con l'opzione già prevista
`-DWSJT_GENERATE_DOCS=OFF`; il target applicativo `decodium_qml` compila.
La build generale senza documentazione è stata avviata ma interrotta durante
la compilazione dei numerosi target di test per limitare il carico su questo
computer; non è quindi una verifica completa della build generale.

Sono stati compilati ed eseguiti separatamente 10 test mirati, tutti passati:
budget thread FT, cancellazione FT4, coda WSPR, sei casi WSPR di risorse e
cancellazione, snapshot QML AppImage. Passa anche il test QML dei tooltip.
Questi controlli non sostituiscono l'intera suite di test.

### Sensibilità FT8 e costo di una ricerca più profonda

Nell'applicazione LOW CPU, tre WAV sintetici affollati con un segnale debole
nominale a -16, -18 e -20 dB hanno prodotto quattro righe ciascuno, identiche
per messaggio e frequenza all'eseguibile di riferimento. Il segnale debole a
1500 Hz non compare in nessuna delle due versioni: la parità di questi quattro
risultati non implica che tutti i segnali presenti siano stati recuperati.

Per esplorare il limite, `tests/tools/ft8_depth_tradeoff_probe.py` confronta
il decoder autonomo stage 4, budget di due thread esplicito, depth 2 e 3,
`candthin=20`, AP disabilitato e deadline di 2800 ms. In due ripetizioni per
file, depth 3 ha prodotto 5 righe contro 4 sui tre WAV sintetici, recuperando
`CQ K1ABC FN42` a 1500 Hz; sulle due registrazioni FT8 ha prodotto 9 contro 7
e 17 contro 13 righe. Considerando lo stesso messaggio entro ±2 Hz, nessuna
riga depth 2 è mancata nei confronti. Il rapporto conserva anche le differenze
esatte: una stima di frequenza differisce di 1 Hz tra i due passaggi.

Sul computer di prova, depth 2 ha usato circa 0,44–0,81 s CPU e depth 3 circa
2,72–2,84 s CPU per file. La deadline ha limitato il passaggio più profondo:
i risultati non sono un confronto senza limiti di tempo. Lo strumento è
separato dall'app e non riproduce tutte le scadenze, code, impostazioni e
condizioni RF della ricezione; i suoi risultati non dimostrano che sostituire
il profilo LOW CPU sarebbe sicuro o che non si perderebbero decodifiche su
altri segnali. Il profilo applicativo non è stato cambiato. Per valutare un
futuro secondo passaggio servono prove applicative additive e verifiche di
CPU, tempi, annullamento al cambio modo e assenza di interferenze con lo slot
successivo.

#### Esplorazione applicativa di due passaggi

`tests/tools/ft8_two_pass_exploratory_lab.py` esegue due aperture successive
sullo stesso WAV nello stesso processo isolato: prima LOW CPU, poi profilo
normale con Deep Search e due thread, tornando infine a LOW CPU. Il secondo
passaggio non è un nuovo scheduler LOW CPU: è una prova manuale con CAT e
monitor audio disabilitati. Le impostazioni non sono identiche tra i passaggi:
nei log osservati il secondo usa depth 4, deadline 6500 ms e un intervallo di
frequenze più ampio, quindi l'eventuale risultato aggiunto non può essere
attribuito alla sola profondità.

Sui tre WAV sintetici affollati, il primo passaggio ha mostrato quattro righe
e il secondo cinque, includendo `CQ K1ABC FN42`. Nella prova su registrazione
FT8, cancellando intenzionalmente la lista fra i passaggi, il primo ha mostrato
13 righe e il secondo 8: sostituire la prima lista con la seconda perderebbe
risultati. In due prove con `--preserve-first`, che non cancella la lista,
l'app ha mantenuto tutte le 13 righe iniziali della registrazione e ne ha
aggiunta una; sul WAV sintetico ha mantenuto le quattro righe e aggiunto il
segnale debole. Il passaggio profondo è durato circa 7,5–7,8 s nella misura
QML, inclusi circa 0,9 s di attesa del laboratorio dopo la fine del decoder.

Questa prova non verifica ascolto continuo, slot successivo, cambio modo o
trasmissione e non dimostra che una seconda decodifica sia sicura su hardware
lento. Evidenzia che un'eventuale implementazione deve mantenere i risultati
rapidi, gestire deduplicazione e tempi e non sostituire il primo passaggio.
Non è stato cambiato il comportamento LOW CPU dell'applicazione.

In un laboratorio separato (`tests/tools/ft8_deep_switch_exploratory_lab.py
--return-ft8`), una decodifica FT8 profonda del profilo normale a due thread
viene interrotta dopo 1,5 s passando a FT4 in LOW CPU; segue il ritorno a FT8
in LOW CPU. In due esecuzioni, il log ha confermato che il decoder FT8 era
entrato in depth 4 ed è stato invalidato. FT4 ha restituito il messaggio atteso
in 725/709 ms dopo il cambio; FT8 lo ha restituito al ritorno in 509/505 ms.
Il rilevamento QML avviene ogni 250 ms. Sono prove con WAV, monitor e CAT
spenti: verificano la cancellazione del percorso profondo già esistente, non
un secondo passaggio LOW CPU né la ricezione durante gli slot successivi.

#### Prototipo LOW CPU solo per laboratorio

Un prototipo di secondo passaggio depth 3 è attivabile esclusivamente con
`--test-mode` e `DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP=1`, con Deep Search attivo.
Non è attivo nell'uso normale. Mantiene il primo passaggio, usa il budget di
due thread e, prima di avviare il follow-up, controlla cambio modo, pressione
CPU, trasmissione attiva o armata e tempo disponibile. La durata
sperimentale può essere limitata con `DECODIUM_LAB_LOWCPU_FT8_FOLLOWUP_MS`.
Questi controlli sono una base di prova, non una certificazione per la radio.

Con un limite di 2800 ms, confronti applicativi LOW CPU hanno mantenuto tutte
le righe del riferimento e aggiunto il segnale debole nei tre WAV sintetici
(4 → 5) e quattro righe su una registrazione (13 → 17). Con limite 1400 ms,
il WAV sintetico a -20 dB è passato da 4 a 5 righe e la registrazione da 13 a
15, senza righe mancanti; i tempi effettivi osservati per il secondo passaggio
erano 1417 e 1400 ms. Le rilevazioni riguardano un file per esecuzione.

Con audio virtuale privato, due slot consecutivi e waterfall visibile, sia il
riferimento sia il prototipo a 1400 ms hanno decodificato entrambi i segnali.
In una coppia a ordine invertito, su circa 43 s, il processo ha usato 4,83 s
CPU senza follow-up e 8,18 s con follow-up; nei rispettivi log figurano tre
passaggi rapidi e, nel prototipo, tre passaggi aggiuntivi. Un'altra coppia ha
dato 4,90 e 7,98 s ma con un numero diverso di passaggi rapidi, quindi non è
un confronto perfettamente omogeneo. Nella prova audio virtuale con il WAV
affollato seguito da uno slot diverso, entrambe le versioni hanno mostrato gli
stessi sei messaggi, incluso il segnale debole: il prototipo ha consumato più
CPU senza aggiungere righe in quella ricezione.

Due cambi FT8 → FT4 → FT8 hanno invalidato il secondo passaggio sperimentale
con limite 1400 ms; FT4 ha restituito il risultato atteso in 484/469 ms e
FT8 al ritorno in 465/473 ms, con polling QML di 250 ms. In questa prova la
proprietà `decoding` era già falsa mentre il worker di follow-up era ancora
attivo: prima di qualsiasi utilizzo normale occorre verificare le interazioni
con stato UI, TX, arresto, cambi profilo e nuovi slot. Il costo CPU aggiuntivo,
il comportamento con radio reale, trasmissione e carichi prolungati non sono
ancora verificati. Non abilitare il prototipo come comportamento predefinito
sulla base di queste prove.

Il prototipo ora ricontrolla Deep Search e LOW CPU dopo il passaggio rapido:
in due prove separate, disattivando Deep Search oppure uscendo da LOW CPU
mentre il primo decoder lavorava, il secondo non è partito e tutte le 13
righe rapide sono rimaste. Prima del controllo, disattivare Deep Search in
quel momento lasciava partire comunque il follow-up. Un'altra prova ha
confermato un limite ancora aperto: disattivando Deep Search dopo l'inizio
del secondo passaggio, il worker ha continuato fino alla scadenza di circa
1,4 s. Prima di renderlo disponibile normalmente occorre poter annullare la
sola richiesta supplementare senza interferire con un successivo passaggio
rapido; i casi TX e pressione CPU richiedono ulteriori verifiche.

Con il prototipo disattivato, il confronto della build attuale con quella
immediatamente precedente ha mantenuto 4/4 righe sul WAV sintetico affollato
e 13/13 sulla registrazione di riferimento, senza differenze di messaggio o
frequenza. Questo controllo copre soltanto i due file e la modalità LOW CPU.

### Inizializzazione concorrente delle tabelle OSD FST4

Le tabelle OSD FST4 e FST4W sono inizializzate una sola volta per ciascun
valore di `keff` tramite `std::call_once`, in sostituzione di indicatori
booleani condivisi non sincronizzati. Questo elimina una possibile corsa
quando due chiamate concorrenti richiedono per la prima volta la stessa
tabella; è una correzione di sicurezza concorrente, non un'accelerazione
misurata né l'integrazione della cessione sperimentale.

Il confronto tra sorgente precedente e nuovo su 360 vettori, con 243 decode
riusciti, ha conservato messaggi, codeword, tipo, errori e distanza bit per
bit; i tempi isolati sono stati 182,2 e 182,5 ms. Un controllo con 16 thread
avviati insieme ha verificato la concordanza di tutte le tabelle per i valori
supportati di `keff`; non è una prova esaustiva di tutti gli interleaving.
La build `decodium_qml` e `test_fst4_decode_cancellation` sono passati.

### Rilascio dei piani FFT alla chiusura del thread

La registry FFT per thread era allocata con `new` senza distruzione: al
termine del thread i piani restavano allocati, contrariamente alla durata
indicata nel commento del codice. Su Linux la registry è ora un oggetto `thread_local`
che libera i propri piani nel distruttore, tramite la protezione già usata
per il planner FFTW; la pulizia esplicita rimane disponibile. Su Windows e
macOS resta la durata precedente, date le segnalazioni di problemi FFTW
durante la distruzione dei thread.

Il probe `tests/tools/fft_thread_lifetime_probe.cpp` crea 300 thread in
sequenza, verifica una trasformata per ognuno e misura RSS dopo `malloc_trim`.
Su Linux, due prove alternate per versione hanno mostrato circa 8470 KiB di
RSS finale prima e 6570 KiB dopo, partendo da circa 3950 KiB. È un test
sintetico con un solo tipo di piano: non quantifica il risparmio in una
sessione radio né copre tutti i backend FFT. La build dell'app e il test di
cancellazione FST4 passano. Nell'app LOW CPU, FST4W → FT8 affollato → FST4W
ha restituito le stesse quattro righe FT8 di un precedente riferimento di
laboratorio con cessione disattivata e il messaggio FST4W atteso al ritorno;
le build differiscono anche per le protezioni sperimentali del riferimento,
quindi questo controllo non è un confronto isolato della sola modifica FFT.

Il probe FFT è stato esteso con otto thread simultanei, ciascuno con 50
trasformate dirette e inverse di due lunghezze e pulizie esplicite
intercalate: i risultati attesi sono stati rispettati. Il dato RSS precedente
riguarda la versione iniziale del probe, con 300 soli thread sequenziali.
Un ulteriore confronto applicativo ha usato due build con la sola differenza
del rilascio FFT: sul WAV FT8 affollato entrambe hanno prodotto le stesse
quattro righe complete, esclusi gli orari, e hanno ritrovato il messaggio
FST4W atteso al ritorno. La ricerca FST4W ritirata non era catturata in
questa prova; non è una verifica di tutti i decode o delle altre piattaforme.
Risultato: `build-baseline/performance/results/fst4w-fft-cleanup-isolated-ab.json`.

### Rilascio dei workspace di downsampling su Linux

I quattro workspace FFT per FT8, FT8 variante, FT2 e FT4 vengono ora
distrutti alla chiusura dei thread su Linux. Windows e macOS mantengono la
durata precedente, per i problemi di distruzione descritti nel codice.
Nel probe `tests/tools/downsample_thread_lifetime_probe.cpp`, 25 thread
sequenziali e otto contemporanei hanno prodotto lo stesso hash dei buffer
prima e dopo; l'RSS finale è stato rispettivamente 146416 e 14216 KiB.
La prova crea intenzionalmente molti thread e usa input sintetico: non
stima il risparmio per una sessione normale né copre tutte le sequenze.
La build e il test di cancellazione FST4 passano; una prova applicativa
LOW CPU con FST4W → FT8 affollato → FST4W conserva le quattro righe FT8
e ritrova il messaggio FST4W atteso, senza catturare tutte le righe della
ricerca ritirata. Confronto: `build-baseline/performance/results/fst4w-downsample-cleanup-ft8-ab.json`.

Ulteriori prove applicative LOW CPU FST4W → FT4 e FST4W → FT2, usando
rispettivamente i WAV sintetici `ft4-0.wav` e `ft2-0.wav`, hanno prodotto
una riga per modo in entrambe le build; tutti i campi coincidono esclusi gli
orari di sessione. Le prove attendono il completamento del decoder
successivo, ma non acquisiscono le righe FST4W ritirate e ciascun WAV
contiene un solo messaggio. Risultati: `downsample-cleanup-ft4-ab.json` e
`downsample-cleanup-ft2-ab.json` in `build-baseline/performance/results/`.

Il probe verifica anche il riutilizzo dell'FFT già calcolata nello stesso
thread per FT8, FT2 e FT4: i campioni sono identici bit per bit al primo
passaggio sia prima sia dopo la modifica. In due coppie alternate della
versione iniziale del probe, la durata complessiva era 0,22/0,25 s prima e
0,25/0,26 s dopo; il tempo CPU totale variava rispettivamente tra 0,24–0,30
e 0,29–0,30 s. Il picco RSS è stato 147932/148092 KiB prima e
37664/36800 KiB dopo. Questi valori descrivono creazione artificiale di thread,
non il costo o la memoria di una normale sessione; non dimostrano una
variazione significativa di velocità dell'applicazione.

### Rilascio del workspace del filtro FT8 su Linux

Anche i due buffer e i due piani del filtro FT8 sono ora riuniti in un
workspace `thread_local` distrutto alla fine del thread su Linux; i piani
vengono liberati prima dei buffer. Windows e macOS conservano la durata
precedente. Il probe di downsampling, esteso al filtro, ha confrontato i
campioni bit per bit su 25 thread sequenziali e otto simultanei: hash
identico nelle due varianti. In due coppie alternate, l'RSS finale è stato
56236/56232 KiB prima e 26344/19360 KiB dopo; è un test artificiale di
ricambio thread, non una previsione per l'applicazione. Il test di stress
`thread_churn_downsample 40 12` ha completato 480 thread su Linux senza
arresti. La build e il test di cancellazione FST4 passano. Nel sorgente
applicativo esaminato non sono stati trovati chiamanti diretti del simbolo
del filtro oltre al wrapper esportato, quindi non si attribuisce un guadagno
misurato alla ricezione FT8 ordinaria.

### Rilascio del workspace di sottrazione FT8 su Linux

Il workspace contenente il segnale di riferimento e il buffer di sottrazione
FT8 viene ora distrutto alla fine del thread su Linux; sulle altre piattaforme
conserva la durata precedente. Il probe `tests/tools/subtract_thread_lifetime_probe.cpp`
ha confrontato l'intero audio dopo la sottrazione con raffinamento temporale:
l'hash è identico su 25 thread sequenziali e otto simultanei, prima e dopo.
L'RSS finale del test artificiale era 94072 KiB prima e 35720 KiB dopo; non
rappresenta il risparmio di una sessione radio normale. Nell'app LOW CPU,
FST4W → FT8 con WAV sintetico affollato ha restituito le stesse quattro righe
complete, a parte gli orari, in entrambe le build. Il test non cattura le
righe FST4W ritirate. Build e cancellazione FST4 passano. Confronto:
`build-baseline/performance/results/subtract-cleanup-ft8-ab.json`.

### Durata dello stato FT2 su Linux

Lo stato di ricerca e accumulo FT2, lo stato di emissione e lo snapshot degli
hash AP rimangono disponibili fra le chiamate dello stesso thread e vengono
rilasciati alla sua chiusura su Linux. La durata precedente è mantenuta sulle
altre piattaforme. La build, `test_ft2_qso_sim` e `test_ft2_legacy_cpu` sono
passati; una prova applicativa LOW CPU FST4W → FT2 ha prodotto la stessa
riga completa nelle due build, esclusi gli orari. La prova usa un solo WAV,
non cattura le righe FST4W ritirate e non misura il risparmio di memoria né
copre tutti i casi di accumulo tra slot. Risultato:
`build-baseline/performance/results/ft2-state-cleanup-ab.json`.

Il probe `ft2_thread_lifetime_probe` inizializza lo stato FT2 e una cache AP
sintetica di 4096 hash in 80 thread sequenziali, quindi misura RSS dopo
`malloc_trim`. In due coppie alternate, l'RSS finale è stato 27632/27636 KiB
prima e 8360/8364 KiB dopo, con circa 7764 KiB iniziali. È un caso artificiale
che forza 80 thread e una cache scelta per la misura; non stima il risparmio
nell'uso quotidiano e non esercita l'accumulo di segnali fra slot. Le due
build del probe differiscono solo nella gestione della durata dello stato FT2;
l'applicazione e il sorgente sono stati ripristinati alla versione corrente.

Il probe mirato `ft2_accumulation_lifetime_probe` accede alla stessa tabella
privata dello stage FT2: registra due slot sintetici sullo stesso thread e
verifica che entrambi restino presenti nelle chiamate successive; ripete la
sequenza su un secondo thread indipendente. Il controllo è passato. Verifica
la durata dello stato e l'isolamento fra thread, non la decodifica di una
trasmissione reale né tutti i casi di accumulo e sostituzione delle voci.

### Aggiornamento dell'orologio mondiale nascosto

Il timer dell'orologio mondiale ora funziona solo quando l'elemento è
visibile: per esempio si ferma nella modalità DX-pedition, che già nasconde
l'orologio. Quando ritorna visibile l'ora viene aggiornata immediatamente.
La prova applicativa `world_clock_visibility_lab.py` ha verificato timer
attivo, arresto per 1,5 s durante la modalità DX-pedition, aggiornamento
immediato e ripresa all'uscita; trasmissione e CAT erano disabilitati. La
build passa. È una piccola eliminazione di un aggiornamento al secondo nello
scenario indicato: non è stato misurato un risparmio CPU complessivo e il
cambiamento non interviene nel percorso di decodifica.

### Anteprima meteo nelle impostazioni nascoste

Quando la pagina Station rimane caricata dopo la chiusura delle impostazioni,
il timer dell'anteprima meteo non effettua più aggiornamenti se la pagina,
il dialogo o la finestra ospite non sono visibili. Riaprendo le impostazioni,
la ripresa del timer aggiorna subito l'anteprima. Questo riguarda solo la
visualizzazione e non cambia le impostazioni o la raccolta meteo del backend.
La prova applicativa `weather_preview_visibility_lab.py` ha verificato le
transizioni attivo → fermo → attivo aprendo, nascondendo e riaprendo la
finestra; la build passa. Si evita un aggiornamento dell'anteprima ogni 15 s
quando l'opzione meteo è attiva e le impostazioni sono nascoste: non è stato
misurato un risparmio CPU complessivo.

### Riutilizzo dei fasori nella demodulazione FT2 equalizzata

Nel ramo FT2 con equalizzazione del canale, la DFT a quattro toni riutilizza
ora 128 fasori calcolati una volta, mantenendo lo stesso ordine di somma. Il
probe `ft2_dft_phasor_ab.py` ha confrontato toni, moduli e potenze bit per
bit su 1024 ingressi; in due coppie alternate il ciclo isolato ha richiesto
760/767 ms prima e 275/265 ms dopo. Sono tempi del solo probe, non della
decodifica completa. Il confronto delle metriche complete
`ft2_equalized_metrics_ab.py` ha prodotto risultati identici bit per bit su
32 ingressi sintetici, di cui 25 nel ramo equalizzato. La build, la
simulazione QSO e il test FT2 per CPU meno recenti passano. Una prova
applicativa LOW CPU FST4W → FT2 ha conservato la stessa riga completa, salvo
l'orario; usa un solo WAV e non dimostra che il ramo equalizzato fosse attivo
in quel caso. Le prove non garantiscono parità per ogni segnale. Risultato
applicativo: `build-baseline/performance/results/ft2-dft-app-ab.json`.

Sul WAV sintetico `ft2-0.wav`, il banco `ft2_equalized_compare` ha trovato un
candidato e tre segmenti: tutti e tre attivano il ramo equalizzato nel banco.
Due eseguibili costruiti rispettivamente prima e dopo la tabella dei fasori
hanno prodotto diagnostica testuale identica per i tre segmenti e zero
segmenti segnalati come discordanti. La stampa non è un confronto bit per bit
di tutti gli array e il banco non dimostra che l'applicazione abbia percorso
gli stessi segmenti; completa, senza sostituire, i confronti sintetici delle
metriche e quello della riga applicativa. Log:
`ft2-equalized-wav-before.log` e `ft2-equalized-wav-after.log` in
`build-baseline/performance/results/`.

Il probe delle metriche complete accetta `--bench` e ripete 500 passaggi
sugli stessi 25 ingressi sintetici che attivano l'equalizzazione, in ordine
alternato. Su questa macchina il tempo CPU totale del processo Python è
stato 639/650 ms prima e 517/538 ms dopo; la misura include la preparazione e
la copia degli output diagnostici, e corrisponde a circa 0,2–0,3 ms per
passaggio in questo banco. È una misura dei calcoli diagnostici su ingressi
sintetici, non il tempo totale dell'app, né una previsione di quante volte
il ramo venga eseguito durante una ricezione.

### Copia dei campioni per il sincronismo FT8: tentativo non adottato

Un esperimento ha eliminato una copia locale prima dell'FFT di sincronismo
FT8. Il confronto `ft8_sync_copy_ab.py` ha restituito output completi
identici bit per bit su 32 ingressi sintetici e quattro forme di metrica.
In due coppie alternate di 320 passaggi completi, il tempo CPU del processo
è stato 189/202 ms prima e 191/191 ms dopo: i campioni non dimostrano un
beneficio ripetibile. La modifica è stata rimossa; il sorgente mantiene
l'implementazione precedente. Il banco non rappresenta tutti i segnali né
l'intera applicazione.

Il confronto dei fasori FT2 è stato esteso a zeri con segno, subnormali,
infiniti e NaN, oltre agli ingressi casuali, compilando sia con `-Os` sia con
`-O2`. In entrambe le build toni, moduli e potenze coincidono bit per bit su
1024 ingressi. Anche la prima inizializzazione concorrente, avviata da otto
thread, ha prodotto risultati identici nei casi verificati. Questi controlli
riguardano i valori e il compilatore usati, non ogni ambiente o segnale.

### Riutilizzo delle finestre di ricerca FT8

La finestra Nuttall usata dai due percorsi di ricerca FT8 e la finestra del
bordo usata dallo stage 4 sono ora calcolate una volta e riutilizzate. Il
confronto `ft8_search_window_ab.py` ha restituito candidati e spettro di
riferimento identici bit per bit su un WAV FT8 reale (300 candidati nel
percorso generale, 35 nello stage 4) e uno sintetico affollato (87 e 8).
In un probe isolato, 2000 costruzioni della finestra Nuttall hanno richiesto
circa 310 ms; l'accesso alla copia precalcolata circa 0,009 ms dopo la
prima inizializzazione. La ricerca completa mostra variabilità e non basta
per stimare un guadagno nell'app. Una prova applicativa LOW CPU FST4W → FT8
ha conservato le quattro righe complete del WAV sintetico affollato, a parte
gli orari; non cattura le righe FST4W ritirate. La build passa. Risultato:
`build-baseline/performance/results/ft8-window-app-ab.json`.

### Finestre dei candidati FT2/FT4: tentativo non adottato

È stato provato il riutilizzo delle finestre dei due percorsi di ricerca
candidati FT2 e FT4. Sul WAV sintetico di ciascun modo, il banco
`ft24_candidate_window_ab.py` ha confrontato tutti gli array di candidati,
punteggi e spettri: i quattro risultati coincidono bit per bit. I tempi di
tre ricerche per prova, in due coppie alternate, sono rimasti sovrapposti:
circa 1,9–2,0 ms per FT2 generale, 8,0–9,4 ms per FT2 sincronismo,
3,4–4,5 ms per FT4 generale e 12,1–13,6 ms per FT4 sincronismo. La
modifica è stata rimossa perché in questi campioni il beneficio non emerge;
i WAV non coprono ogni possibile ingresso e il banco non misura l'app.

### Riutilizzo dei logaritmi nella stima del fondo

La stima quartica del fondo riutilizza i logaritmi già calcolati per ciascun
segmento, evitando di ricalcolarli durante la selezione dei punti. La
funzione del percentile ordina una copia, quindi l'ordine e i valori dei punti
restano invariati. Su 250 spettri sintetici, compresi zeri e intervalli di
diverse dimensioni, gli output completi coincidono bit per bit. In due coppie
alternate di 2500 passaggi del probe Python, il tempo CPU totale è stato
69/65 ms prima e 56/56 ms dopo, inclusa la preparazione degli output: non è
una misura della decodifica completa. Le ricerche FT2/FT4 sui rispettivi WAV
e FT8 su una registrazione hanno restituito candidati e spettri identici;
nell'app LOW CPU il WAV FT8 affollato ha conservato le quattro righe complete
esclusi gli orari. Build e test FT2 legacy CPU passano. Il confronto
applicativo non cattura le righe FST4W ritirate e le prove non coprono ogni
segnale. Risultato: `build-baseline/performance/results/baseline-log-ft8-app-ab.json`.

### Selezione del percentile del fondo: tentativo non adottato

È stata provata la selezione del percentile senza ordinare l'intero segmento,
con ritorno all'ordinamento precedente per segmenti contenenti NaN. Su 250
spettri sintetici, inclusi zeri, valori ripetuti, infiniti e NaN, gli output
completi coincidevano bit per bit. Le prove alternate di 2500 stime davano
circa 59–64 ms prima e 52–57 ms dopo, compresa la preparazione degli output;
sui WAV FT2 e FT4 i candidati e gli spettri coincidevano, ma non emergeva
un guadagno chiaro nella ricerca completa. La modifica è stata rimossa:
il vantaggio isolato era piccolo rispetto alla complessità aggiuntiva e le
prove non coprivano tutti i possibili ingressi.

### Piani FFT persistenti per i candidati FT2/FT4: tentativo non adottato

È stata provata una cache dei buffer e piani FFT per thread nelle ricerche
candidati FT2 e FT4, con ritentativo dopo un eventuale errore di creazione.
Sui due WAV sintetici, i quattro percorsi hanno restituito candidati, punteggi
e spettri identici bit per bit. I tempi di tre ricerche per prova sono
rimasti sovrapposti: circa 1,8–1,9 ms FT2 generale, 7,6–8,1 ms FT2
sincronismo, 3,1–3,8 ms FT4 generale e 10,7–12,7 ms FT4 sincronismo.
La variante avrebbe trattenuto buffer per thread e aumentato la complessità
di gestione: senza un guadagno dimostrato è stata rimossa. Il confronto usa
`ft24_candidate_window_ab.py` e non copre ogni ingresso o l'applicazione.

### Intervallo dei denominatori nella ricerca sincronizzata FT2/FT4

La ricerca aggiuntiva per sincronismo conserva e calcola i denominatori solo
nelle celle della banda richiesta, mantenendo le operazioni e l'ordine dei
candidati. Non cambia l'attivazione della funzione, che resta opzionale.
Il probe `ft24_sync_range_ab.py` ha confrontato gli array completi su 72 casi
FT2 e 72 FT4: WAV sintetico, rumore e silenzio; otto bande e tre limiti di
candidati. Tutti coincidono bit per bit. Su dieci ricerche in banda stretta,
il tempo CPU del banco era 22,9/23,3 ms prima e 19,5/20,3 ms dopo per FT2;
34,4/34,1 e 32,5/34,3 ms per FT4. Sono misure isolate e variabili, non
l'intera decodifica. Per la banda 900–1100 Hz la matrice dei denominatori
usa circa un quinto delle celle precedenti, mentre le altre matrici restano.

Con `DECODIUM_FT2_SYNC2D=1`, la prova applicativa LOW CPU FST4W → FT2 ha
conservato la riga completa del WAV sintetico, escluso l'orario; non cattura
le righe FST4W ritirate. La build e il test FT2 legacy CPU passano. Risultato:
`build-baseline/performance/results/sync-range-ft2-ab.json`.

Con `DECODIUM_FT4_SYNC2D=1`, una seconda prova applicativa LOW CPU
FST4W → FT4 ha mantenuto la stessa riga completa nelle due build, escluso
l'orario. Come la prova FT2, usa un solo WAV sintetico e non cattura le
righe FST4W ritirate. Risultato:
`build-baseline/performance/results/sync-range-ft4-ab.json`.

La verifica del nuovo intervallo è stata estesa a 40 bande casuali oltre alle
otto fisse, su WAV, rumore e silenzio e tre limiti di candidati: 432 casi
FT2 e 432 FT4 identici bit per bit alle rispettive build precedenti. Le due
librerie sono state compilate con AddressSanitizer e UndefinedBehaviorSanitizer;
non sono stati segnalati errori negli accessi o comportamenti controllati. Il
controllo delle perdite era disabilitato perché il banco viene caricato in un
processo Python. Non sono prove esaustive di tutti gli ingressi.

### Inizializzazione delle matrici di sincronismo: tentativo non adottato

È stata provata l'eliminazione dell'azzeramento iniziale delle matrici che
vengono poi riempite dalla ricerca sincronizzata FT2/FT4. Gli output hanno
coinciso bit per bit nei 432 casi per modo del banco `ft24_sync_range_ab.py`.
Le due coppie di dieci ricerche su banda stretta hanno dato 20,1/20,5 ms
prima e 20,4/20,3 ms dopo per FT2, 33,5/36,1 e 33,3/37,2 ms per FT4.
Il beneficio non emerge e l'uso di memoria non inizializzata richiederebbe
ulteriori garanzie: l'esperimento è stato rimosso.

### Intervallo delle potenze nella ricerca sincronizzata FT2/FT4

La matrice delle potenze conserva ora soltanto la banda letta dalla ricerca,
includendo i 12 bin necessari ai quattro toni. Per la banda 900–1100 Hz,
la larghezza della matrice scende da 125 a 34 celle per frame in FT2 e da
231 a 54 in FT4; altre allocazioni del decoder restano invariate. Su 432 casi
per modo con bande fisse e casuali, WAV sintetici, rumore e silenzio, tutti
i candidati e punteggi coincidono bit per bit con il riferimento. La stessa
prova passa con AddressSanitizer e UndefinedBehaviorSanitizer, disabilitando
il controllo perdite nel processo Python. In due coppie di dieci ricerche
in banda stretta, FT2 ha richiesto 24,0/29,2 ms prima e 23,2/24,2 ms dopo;
FT4 33,8/33,6 e 31,7/31,7 ms. Le misure sono del solo banco e non stimano
la velocità dell'app. Con `DECODIUM_FT4_SYNC2D=1`, una prova applicativa
LOW CPU ha mantenuto la stessa riga FT4 completa del WAV sintetico, escluso
l'orario. Build e test FT2 legacy CPU passano. La prova non cattura tutte
le righe FST4W ritirate. Risultato:
`build-baseline/performance/results/sync-power-range-ft4-ab.json`.

Con `DECODIUM_FT2_SYNC2D=1`, la prova applicativa LOW CPU FST4W → FT2
ha conservato la stessa riga completa dopo la riduzione della matrice delle
potenze, escluso l'orario. Come per FT4, usa un solo WAV sintetico e non
cattura tutte le righe FST4W ritirate. Risultato:
`build-baseline/performance/results/sync-power-range-ft2-ab.json`.

### Verifica complessiva FT8 contro il riferimento conservato

Una profilazione offline (`ft8_stage_timing_lab.py`) su due scene sintetiche
FT8 affollate, profondità 2 e senza deadline, ha individuato la scansione di
sincronismo `sync8var` come costo principale delle passate misurate: circa
1,16/1,21 s nelle scansioni su 1,58/1,68 s complessivi delle passate principali.
Le sezioni sono annidate e i tempi non vanno sommati. Il contatore LDPC del
banco non intercetta il percorso batch, quindi uno zero non indica assenza di
lavoro LDPC. Il banco aggiunge strumentazione, non riproduce l'intera gestione
LOW CPU dell'app e non è una misura RF. I sorgenti e la build normali sono
stati ripristinati; log `build-baseline/performance/results/ft8-stage-timing.log`.

Un prototipo separato elabora quattro frequenze del sincronismo FT8 con SSE2,
mantenendo l'ordine delle somme e il calcolo finale scalare. Nel banco
`ft8_sync_simd_explore.py`, 100 matrici casuali e casi speciali hanno prodotto
matrici di punteggi e flag identiche; due coppie di otto scansioni hanno
richiesto circa 91–108 ms CPU prima e 29–50 ms dopo. Il test
`ft8_sync_simd_sanitize.py` confronta anche i punteggi e flag individuali con
ASan/UBSan. In una build solo sperimentale LOW CPU sono state conservate 7/7 e
13/13 righe su due registrazioni, 4/4 su una scena sintetica affollata e 1/1
su un segnale debole. I tempi del worker nelle quattro singole coppie sono
stati rispettivamente 951→481, 737→505, 508→292 e 464→204 ms: campioni
preliminari, non una misura di CPU complessiva. Log e risultati:
`ft8-simd-explore-*` in `build-baseline/performance/results/`.

La variante è stata inserita nella build locale per Linux x86-64 SSE2 senza
FMA o fast-math, con controlli su banda, dimensioni e intervalli; tutti gli
altri casi conservano il percorso scalare. Il banco confronta ora il codice
integrato con il fallback su 500 matrici casuali, casi speciali e opzioni
`-Os`/`-O2`; ASan/UBSan ha verificato i punteggi individuali. La compilazione
senza SSE2 e i test `test_ft2_legacy_cpu` e `test_ft4_decode_cancellation`
sono passati. La build integrata ha mantenuto 7/7 righe della registrazione
e 1/1 del segnale debole rispetto alla copia subito precedente; i tempi del
worker sono stati 703→562 ms e 482→238 ms rispettivamente, una sola coppia
per WAV. Rimangono da verificare ulteriori segnali, sistemi e ricezione RF;
queste prove non misurano la CPU complessiva. Risultati:
`ft8-simd-production-parity.json` e relativi log nella directory performance.

Un confronto successivo con audio virtuale continuo FT8, LOW CPU e due slot
identici ha alternato la copia scalare immediatamente precedente e la build
con SSE2, con CAT e trasmissione disattivati. Tutte le quattro esecuzioni
hanno ricevuto entrambi i messaggi attesi alle stesse frequenze. Su circa
43,17 s per prova, la CPU complessiva del processo era 5,54/6,13 s per il
riferimento e 5,76/5,56 s per SSE2: i valori si sovrappongono e non dimostrano
un risparmio complessivo stabile. Il campionamento del worker FT8 indicava
0,86/0,92 s contro 0,42/0,50 s, ma può perdere thread brevi; il thread
principale pesava molto nel totale. Sono solo due prove per versione, su
questo computer, con renderer software e segnali virtuali, non ricezione RF.
Script e dati: `tests/tools/ft8_live_scalar_ab.py` e
`build-baseline/performance/results/ft8-live-scalar-ab.json`.

Il confronto applicativo è stato ripetuto dopo le modifiche successive,
usando `decodium-before` e l'eseguibile corrente con lo stesso QML attuale,
LOW CPU, CAT e monitor spenti. Sulle registrazioni `181201_180245.wav` e
`210703_133430.wav` entrambe le build hanno prodotto rispettivamente 7/7 e
13/13 righe uguali per messaggio e frequenza, senza righe mancanti o
aggiunte. Il tempo osservato dalla richiesta al completamento era 1495 e
1527 ms nel riferimento, 1528 e 1617 ms nell'attuale: una prova per file,
comprendente il polling del laboratorio, non dimostra un miglioramento o un
peggioramento di velocità. Il riferimento è l'eseguibile locale conservato,
non una build appena ricreata dalla release; l'interfaccia è comune alle due
prove. Non sono prove RF o una garanzia per tutti i segnali. Risultato:
`build-baseline/performance/results/ft8-parity-latest-vs-before.json`.

### Verifica complessiva FT2 contro il riferimento conservato

Un'esplorazione locale del percorso FT2 (`ft2_stage_timing_lab.py`) ha
strumentato il decoder con profondità 2 sui WAV sintetici affollato e debole.
Nel primo caso il tempo trascorso totale era 1.356 ms, di cui 849 ms nelle
scansioni di sincronismo e 391 ms nei passaggi di decode; nel secondo era
2.000 ms, di cui 1.163 ms e 659 ms rispettivamente. La strumentazione aggiunge
overhead e il decoder può avere un limite temporale: non sono tempi CPU o
misure su tutte le condizioni. La build normale è stata ripristinata.

Un prototipo che precalcola la coniugazione dei template del sincronismo ha
prodotto risultati identici bit per bit su 180 scansioni casuali. Nel banco
isolato, tuttavia, i tempi CPU di 60 scansioni si sovrapponevano: 12,1/14,5 ms
prima e 13,6/13,9 ms dopo. Il prototipo non è stato adottato; script
`ft2_sync_conjugate_explore.py`.

Un secondo prototipo ha sostituito la moltiplicazione complessa nella
correlazione con componenti esplicite, conservando il fallback per risultati
non numerici. Su 500 scansioni sintetiche punteggi e posizione del massimo
coincidevano bit per bit, ma i tempi CPU di 120 scansioni si sovrapponevano:
16,2/23,7 ms prima e 20,0/20,7 ms dopo. Non è stato adottato; il confronto
non copre tutti i valori speciali o una decodifica completa. Script:
`ft2_sync_complex_explore.py`.

Un terzo prototipo sperimentale usa SSE2 per valutare quattro posizioni di
sincronismo contemporaneamente, mantenendo il percorso scalare ai bordi e
quando compaiono valori non finiti. In `ft2_sync_simd_explore.py`, 1.000
scansioni casuali e casi di silenzio, infinito, NaN, valori estremi e
subnormali hanno prodotto lo stesso massimo bit per bit; due coppie di 150
scansioni hanno richiesto 20,9/22,1 ms CPU prima e 10,6/12,6 ms dopo.

Una build solo sperimentale ha mantenuto le otto righe FT2 della scena
affollata e la riga del segnale debole. Il tempo di decode riportato dal
worker è passato da 1.314 a 1.064 ms nel primo caso e da 2.000 a 1.691 ms
nel secondo, una sola coppia per WAV; il limite temporale del decoder può
incidere sul confronto. Log e righe sono in
`build-baseline/performance/results/ft2-simd-explore-parity*`.

La verifica `ft2_sync_simd_sanitize.py` ha confrontato ogni punteggio
parallelo con quello scalare su 5.000 intervalli e valori speciali, con
AddressSanitizer e UndefinedBehaviorSanitizer attivi: nessuna differenza o
errore rilevato. Una seconda coppia di prove applicative, con ordine dei WAV
invertito, ha mantenuto 8/8 righe nella scena affollata e 1/1 nel segnale
debole. Le due coppie di tempi del worker sono 1.314/1.315 ms scalare contro
1.064/1.081 ms sperimentale per la scena affollata; 2.000/1.911 contro
1.691/1.727 ms per il segnale debole. I dati non stabiliscono la parità su
tutti i segnali o compilatori.

La variante è ora abilitata nella build su Linux x86-64 con SSE2, solo quando
non sono abilitate FMA o fast-math; sugli altri target si usa il percorso
scalare. Gli estremi della finestra e gli intermedi non finiti ricadono nel
percorso scalare. Il confronto numerico passa anche con `-Os` e
`-fcx-limited-range`, opzioni della build locale; la compilazione con SSE2
disabilitato passa, ma non sostituisce una build su altre piattaforme. Sono
passati i test `test_ft2_qso_sim`, `test_ft2_legacy_cpu` e la verifica dei
5.000 intervalli sul codice inserito con ASan/UBSan. Nel confronto LOW CPU
con il riferimento conservato, le otto righe FT2 affollate e la riga debole
sono rimaste uguali; risultati in `ft2-simd-production-parity.json`.
Resta necessaria una verifica più ampia su segnali reali e sistemi diversi.

Un confronto successivo (`ft2_live_scalar_ab.py`) usa l'ingresso audio
virtuale privato e due build locali identiche salvo SSE2, LOW CPU e waterfall
visibile. Quattro prove alternate sul WAV debole hanno decodificato tutte
`CQ K1ABC FN42`; le frequenze osservate erano 1500–1501 Hz. Su circa 16,9 s,
il processo scalare ha usato 8,70/8,97 s CPU e quello SSE2 8,76/8,22 s; il
thread FT2 campionato 5,97/6,49 e 6,42/5,77 s rispettivamente. I campioni si
sovrappongono e non dimostrano un risparmio CPU complessivo stabile durante
la ricezione continua. Una precedente esecuzione è stata scartata per timeout
della sorgente virtuale. Il rendering era software, CAT e TX spenti: non è
una prova RF. Risultati e log: `ft2-live-scalar-ab*`.

Il banco `ft2_sync_simd_explore.py` confronta ora direttamente il codice
integrato con lo stesso codice compilato disattivando SSE2. Con `-Os` e
`-fcx-limited-range` ha confrontato 5.000 intervalli su più ingressi e
template, più casi speciali, senza differenze nei massimi. Il controllo
ASan/UBSan dei singoli punteggi sul codice integrato è stato ripetuto con
esito positivo. Le misure di tempo eseguite insieme ad altri test non sono
usate per stimare le prestazioni.

Su un'ulteriore scena FT2 sintetica con otto stazioni simultanee e rumore,
le due build in LOW CPU hanno decodificato le stesse otto righe, senza
mancanze o aggiunte. Le frequenze sono entro 1 Hz dai valori generati. La
scena è riproducibile con `prepare_ft24_busy_fixture.py`; dettagli e hash in
`build-baseline/performance/results/ft2-busy-parity-20261115.json` e nel
manifest accanto al WAV. Si tratta di una scena sintetica, non di una misura
della soglia di sensibilità o di ricezione RF.

Due ulteriori segnali FT2 sintetici con rumore, generati con `--snr-db -16`
e `-19` e seed 1101/1102, hanno prodotto in entrambe le build lo stesso
`CQ K1ABC FN42`, rispettivamente a 1500 e 1499 Hz; i valori SNR visualizzati
erano −14 e −17 dB. Risultato con hash dei WAV:
`build-baseline/performance/results/ft2-weak-parity-20261115.json`.
L'SNR passato al generatore è relativo all'RMS del segnale, non è una soglia
calibrata su 2500 Hz; due semi non dimostrano parità generale.

La build corrente e `decodium-before` sono state confrontate nell'app con
LOW CPU, lo stesso QML corrente, CAT e monitor spenti e tre WAV FT2 sintetici
(`ft2--12`, `ft2--6`, `ft2-0`). Ciascun WAV ha prodotto una riga in entrambe
le build, uguale per messaggio e frequenza, senza righe mancanti o aggiunte.
Le misure di circa 2,8 s includono l'attesa del laboratorio e non stimano la
sola durata della decodifica. I tre file contengono un singolo segnale e non
coprono radio reale, altri profili o tutti i casi deboli. Risultato:
`build-baseline/performance/results/ft2-parity-latest-vs-before.json`.

### Collegamenti precalcolati nel decoder LDPC scalare

Una nuova profilazione FT2 dopo SSE2 ha misurato circa 340/664 ms nelle
scansioni di sincronismo e 391/809 ms nei tentativi di decode, su due WAV e
con strumentazione: i valori dipendono anche dai limiti temporali. Il decoder
LDPC scalare ora usa una tabella `constexpr` per ritrovare il primo arco del
grafo, prima cercato a ogni iterazione; `static_assert` verifica che ogni
arco esista. Ordine dei tentativi e operazioni in virgola mobile non cambiano.

Il banco `ldpc_bp_vector_probe` ha confrontato tutti i byte di messaggio e
parola, gli indicatori e il bit pattern della distanza su 450 ingressi
deterministici, con maschere AP e diversi percorsi BP/OSD: risultati identici.
In quattro esecuzioni alternate, i tempi CPU dell'intero banco erano
1,351/1,347 s prima e 1,257/1,229 s dopo; non sono tempi dell'applicazione.
La build e i test `test_ft2_legacy_cpu` e `test_ft4_decode_cancellation`
sono passati.

Le build identiche salvo questa ricerca hanno restituito le stesse 8/8 righe
su scene affollate FT2 e FT4 e le stesse 7/7 righe sulla registrazione FT8
`181201_180245.wav`, in LOW CPU. In quattro ricezioni FT2 alternate con audio
virtuale debole tutte hanno trovato il messaggio atteso a 1500–1501 Hz. Su
circa 16,9 s, il processo ha usato 8,25/8,64 s CPU prima e 8,47/9,06 s dopo:
questo campione non dimostra un risparmio complessivo e la maggiore velocità
di una sezione può anche permettere più lavoro entro i limiti di tempo. Sono
prove su un solo computer, con CAT e TX spenti; non garantiscono la parità
su tutti i segnali o la ricezione RF. Risultati:
`ldpc-edge-parity-ft2.json`, `ldpc-edge-parity-ft4.json`,
`ldpc-edge-parity-ft8.json` e `ft2-live-ldpc-edges-ab.json` nella directory
`build-baseline/performance/results/`.

### Min-sum LDPC per riga e temporizzazione del test FT2

Un prototipo calcolava una sola volta i due minimi e il prodotto dei segni
per ciascun controllo, invece di ripetere la ricerca per ogni arco. Il banco
`ldpc_minsum_row_probe.cpp` ha confrontato un milione di righe, anche con
valori speciali, e `ldpc_bp_vector_probe` ha prodotto risultati identici sui
suoi 450 casi. La build sperimentale ha anche mantenuto 8/8 righe nelle scene
FT2 e FT4 affollate, 7/7 in una registrazione FT8 e il segnale debole nelle
quattro prove FT2 con audio virtuale. In queste ultime la CPU del processo
era 8,12/8,58 s prima e 7,50/7,08 s sperimentale su circa 16,9 s.

Inizialmente `test_ft2_legacy_cpu` ha fallito due volte sul segnale debole
a -15 dB con il prototipo e la modifica è stata ritirata. L'indagine
successiva ha confrontato 128.773.224 valori intermedi sulle chiamate reali
del test senza differenze bit per bit. Una prova con il solo segnale debole,
senza storico, non lo ha decodificato né prima né dopo: tutte le 512 chiamate
LDPC condivise avevano ingressi e uscite identici.

Il test precedente eseguiva il segnale debole subito dopo più segnali forti,
ma la verifica FT2 del messaggio memorizzato accetta un'età fra 5,5 e 9,5 s.
La build più veloce arrivava prima di quell'intervallo: nel confronto immediato
il prototipo non ha effettuato verifiche AP e non ha recuperato il segnale;
con 3 s aggiuntivi ha effettuato verifiche AP e lo ha recuperato. Anche il
riferimento lo ha recuperato con l'attesa. Il test è stato corretto preparando
un nuovo messaggio forte, aspettando esplicitamente 6 s e verificando che il
recupero debole avvenga mediante AP tipo 8. Entrambe le varianti hanno passato
il test corretto. Log: `ldpc-minsum-intermediate.log`, `ldpc-trace-*.log` e
`ft2-history-*.log` sotto `build-baseline/performance/results/`.

La variante per riga è stata inizialmente reinserita nella build locale. Sono passati
`test_ft2_legacy_cpu`, `test_ft4_decode_cancellation` e il confronto dei 450
casi LDPC. La misura di CPU riguarda due prove per versione, con un solo
segnale virtuale e renderer software: non garantisce lo stesso vantaggio o la
conservazione di tutti i decode in altre condizioni o su radio reale. I dati
del confronto applicativo sono `ft2-live-ldpc-rows-ab.json` e
`ldpc-rows-parity-{ft2,ft4,ft8}.json` nella directory performance.

`ft2_history_live_ab.py` riproduce inoltre con audio virtuale un messaggio
forte e lo stesso messaggio debole due slot dopo, intervallati da uno slot
silenzioso. In quattro prove alternate LOW CPU, due per build, sono apparse
entrambe le trasmissioni previste con etichette UTC distinte e frequenze
entro 1 Hz da 1500 Hz; il log mostra anche verifiche della memoria AP. Ciò
non prova che il secondo messaggio sia stato recuperato grazie all'AP né
copre altri livelli di segnale o radio reale. In una sola prova della build
ottimizzata è comparsa una riga aggiuntiva dello stesso messaggio, marcata
con `?` e AP tipo 8, a 1504 Hz e con etichetta UTC successiva: non è stata
conteggiata come una terza trasmissione e la sua origine resta da verificare.
Confronto e log: `ft2-history-live-ab.json` e `ft2-history-live-ab-*.log`.
La diagnostica AP era attiva e aggiunge lavoro: i tempi CPU di questa prova
non vengono usati come misura di prestazioni.

Una seconda coppia alternata con lo stesso segnale ha nuovamente mantenuto
entrambe le trasmissioni. Nelle quattro prove complessive per build, però,
la build sperimentale ha mostrato righe supplementari `?` AP tipo 8 in tre
prove; la build di riferimento non ne ha mostrate in queste quattro prove.
Una prova per build, con il secondo segnale sostituito da silenzio, non ha
mostrato un secondo orario di decodifica, pur effettuando verifiche AP. Nel
controllo della build sperimentale sono comunque comparse due righe per il
primo orario; non dimostra assenza generale di duplicati o falsi. I segnali
e le finestre di ricezione si sovrappongono e questi dati non stabiliscono
se le righe AP supplementari siano la coda del secondo burst riconosciuta
di nuovo o altre conferme spurie. Non è stato cambiato alcun filtro AP.

Per prudenza il min-sum per riga è stato nuovamente **ritirato** dalla build
corrente. Il risparmio CPU del prototipo non va attribuito alla versione
attuale; restano invece le altre ottimizzazioni, inclusa la tabella degli
archi. Risultati: `ft2-history-ap-trace.json`,
`ft2-history-silence-control.json` e relativi log.

Un'ulteriore traccia temporanea del worker (`ft2_history_worker_trace.py`)
ha confrontato due esecuzioni per build e poi ripristinato sorgenti e build
ordinarie. In una prova sperimentale le due finestre asincrone che hanno
decodificato il burst debole stimavano l'inizio rispettivamente al campione
309.201 e 309.198; la riga AP tipo 8 aggiuntiva stimava 340.359, circa
2,60 s più tardi, ed era prodotta dal percorso asincrono. Il burst sintetico
dura circa 2,47 s: la stima cade quindi dopo l'inizio del burst, vicino alla
sua fine, e non dimostra una terza trasmissione. La seconda prova della
variante non ha mostrato righe AP aggiuntive. Nelle due prove di riferimento
entrambe le trasmissioni erano presenti: in una il secondo messaggio era
esso stesso marcato `?` e AP tipo 8. La traccia conferma che una riga AP 8
può essere l'unico risultato visibile per il burst debole: scartare o
limitare tutte le righe AP 8 perderebbe decodifiche. Non è stato introdotto
un filtro; l'ottimizzazione per riga rimane ritirata. Campioni, DT e orari
sono stime del decoder, non la prova definitiva dell'origine del segnale.
Risultati e log: `ft2-window-trace.json` e `ft2-window-trace-*.log`.

### Riutilizzo dei template di sincronismo FT4

FT4 ora usa la scansione già condivisa con FT2: prepara i quattro template
una volta per ciascuna frequenza anziché una volta per ogni posizione. Mantiene
l'ordine della ricerca e il primo massimo stretto; su Linux x86-64 con SSE2
usa inoltre il percorso vettoriale già verificato, con fallback scalare.
`ft4_sync_scan_probe.cpp` ha confrontato 1.005 intervalli e valori speciali
bit per bit, anche con ASan/UBSan e in una build di prova senza SSE2. Passa
il test `test_ft4_decode_cancellation`.

Due confronti applicativi LOW CPU con ordine delle build invertito hanno
mantenuto le stesse otto righe della scena FT4 affollata e la stessa riga
del segnale debole. Il worker ha riportato 230/232 ms prima e 137/147 ms
dopo per la scena affollata; 242/242 ms prima e 135/141 ms dopo per quella
debole. Sono tempi trascorsi di due WAV sintetici, non CPU complessiva o
una prova RF. Risultati: `ft4-sync-scan-parity*.json` e relative directory
di log in `build-baseline/performance/results/`.

In due coppie alternate di ricezione FT4 con audio virtuale privato e
waterfall visibile, entrambe le build hanno decodificato `CQ K1ABC FN42`
dal WAV debole; la frequenza osservata era 1500 Hz in tre prove e 1501 Hz
in una. Su circa 19,2 s, il processo ha usato 2,34/2,23 s CPU prima e
2,03/1,83 s dopo; il thread FT4 campionato 0,32/0,31 e 0,20/0,18 s. Le
build condividono il QML corrente ma non differiscono soltanto per il codice
FT4, e il campionamento dei thread può perdere attività brevi: i dati non
isolano il risparmio dell'ottimizzazione. CAT e TX spenti, renderer software;
non è una ricezione RF. Log: `ft4-live-sync-before-first.log`,
`ft4-live-sync-after-first.log` e `live-audio-*-visible-ft4-custom-first.log`.

Un confronto successivo isola la sola scansione FT4:
`tests/tools/ft4_live_sync_isolated_ab.py` ricompila lo stesso sorgente con
il ciclo precedente, conserva quell'eseguibile e ripristina subito codice e
build correnti. Su quattro ricezioni alternate dello stesso WAV debole, tutte
con LOW CPU, audio virtuale e waterfall visibile, è stato ricevuto lo stesso
`CQ K1ABC FN42` a 1500 Hz. In circa 19,2 s per prova la CPU complessiva è
stata 1,95/1,96 s con il ciclo precedente e 1,86/1,80 s con la scansione
attuale; il worker FT4 campionato 0,26/0,29 s contro 0,18/0,17 s. Sono due
prove per variante e un solo segnale, con CAT e TX spenti e renderer software;
il campionamento può perdere thread brevi. Non è una prova RF né garantisce
lo stesso risparmio o la stessa sensibilità su altri segnali. Risultati:
`build-baseline/performance/results/ft4-live-sync-isolated-ab.json`.

### Verifica complessiva FT4 contro il riferimento conservato

Anche nella scena FT4 sintetica con otto stazioni simultanee e rumore, le
due build LOW CPU hanno prodotto le stesse otto righe; le frequenze sono entro
1 Hz dai valori generati. Risultati e hash sono in
`build-baseline/performance/results/ft4-busy-parity-20261115.json` e nel
manifest accanto al WAV. Non dimostra la parità di tutti i segnali deboli o
di una ricezione RF.

Le due prove FT4 equivalenti, con `--snr-db -16` e `-19` e seed 1101/1102,
hanno prodotto in entrambe le build lo stesso `CQ K1ABC FN42` a 1500 Hz;
i valori SNR visualizzati erano −12 e −15 dB. Il risultato è in
`build-baseline/performance/results/ft4-weak-parity-20261115.json`. L'SNR
del generatore è relativo all'RMS del segnale e questi due casi non sono una
misura della soglia di sensibilità né una ricezione RF.

Con LOW CPU, lo stesso QML corrente, CAT e monitor spenti, le build
`decodium-before` e corrente hanno prodotto una riga ciascuna per i tre WAV
FT4 sintetici `ft4--12`, `ft4--6` e `ft4-0`. Messaggio e frequenza sono
identici senza righe mancanti o aggiunte. Ogni WAV contiene un singolo
segnale: la verifica non copre ricezione radio reale o tutte le condizioni
di segnale. Risultato:
`build-baseline/performance/results/ft4-parity-latest-vs-before.json`.

### Verifica complessiva FST4/FST4W contro il riferimento conservato

Con LOW CPU, stesso QML corrente, CAT e monitor spenti, il WAV sintetico
FST4-15 ha prodotto in entrambe le build `CQ K1ABC FN42` a 1331 Hz, senza
righe mancanti o aggiunte; il tempo osservato è stato 13,23 s nel riferimento
e 12,41 s nell'attuale, inclusi 800 ms del laboratorio. Una prova per build
non dimostra una variazione stabile di velocità.

Il WAV FST4W rumoroso ha prodotto zero righe nel riferimento conservato e
tre nell'attuale, tra cui il messaggio atteso `K1ABC FN42 33`. Le altre due
righe sono falsi candidati già segnalati: non vanno contate come ulteriori
segnali validi. Poiché il riferimento non mostra righe, questa prova non
verifica la conservazione di decode FST4W rispetto al riferimento. Tempi
osservati 22,26 e 9,41 s, inclusi 800 ms del laboratorio e differenze
funzionali già documentate; non sono una stima generale. Risultati:
`fst4-parity-latest-vs-before.json` e `fst4w-parity-latest-vs-before.json`
in `build-baseline/performance/results/`.

### Verifica complessiva WSPR contro il riferimento conservato

Il banco applicativo `bridge_wav_parity_lab.py` supporta anche WSPR. Con
LOW CPU, lo stesso QML corrente, CAT e monitor disattivati, il WAV di
riferimento `150426_0918.wav` ha prodotto otto righe per build, identiche
per messaggio e frequenza, senza righe mancanti o aggiunte. Il tempo dalla
richiesta al rilevamento del completamento è stato 17,99 s per
`decodium-before` e 11,61 s per la build corrente, inclusi 800 ms del timer;
è una prova per versione, non una stima generale né una misura in RF. Le
build incorporano più modifiche e il confronto non ne separa i contributi.
Risultato: `build-baseline/performance/results/wspr-parity-latest-vs-before.json`.

Ripetendo la coppia WSPR in ordine inverso, la build corrente ha richiesto
11,49 s e il riferimento 18,42 s, sempre inclusi 800 ms di attesa; entrambe
hanno restituito le stesse otto righe per messaggio e frequenza. Le due coppie
mostrano quindi tempi più brevi per la build corrente su questo WAV e questo
computer, senza isolare il contributo delle singole modifiche o garantire il
risultato su altri segnali. Il file `wspr-parity-latest-reverse.json` usa
`before` per la build corrente e `after` per il riferimento perché l'ordine
è stato invertito intenzionalmente.

### Verifica FT8 debole contro il riferimento conservato

Con LOW CPU e lo stesso QML, CAT e monitor spenti, due WAV sintetici FT8 a
−18 dB nominali e due a −21 dB sono stati confrontati fra `decodium-before`
e la build corrente. Entrambi i file a −18 dB e il primo a −21 dB hanno
prodotto una riga identica per messaggio e frequenza in entrambe le build;
il secondo a −21 dB non ha prodotto righe in nessuna delle due, quindi non
verifica la conservazione di un decode. Quattro realizzazioni non misurano
la soglia di sensibilità né rappresentano tutte le condizioni RF. Risultato:
`build-baseline/performance/results/ft8-weak-parity-latest-vs-before.json`.

Sul WAV FT8 isolato `snr-21-000000_000002.wav`, che non aveva prodotto
righe nel confronto LOW CPU, una prova applicativa con Deep Search attivo e
follow-up sperimentale limitato a 1400 ms non ha recuperato il messaggio:
zero righe sia senza sia con follow-up. Il log mostra che il passaggio
aggiuntivo è stato eseguito e ha impiegato circa 1489 ms. La prova usa un solo
WAV e nessuna ricezione RF; il follow-up resta confinato al laboratorio e non
viene attivato per il normale LOW CPU. Risultato:
`build-baseline/performance/results/ft8-weak21-followup-1400.json`.

### Ricezione FT8 continua: confronto corrente con riferimento conservato

Due coppie alternate dell'app in LOW CPU hanno riprodotto due slot FT8
consecutivi tramite sorgente audio virtuale privata, con waterfall visibile,
CAT e TX spenti e rendering software offscreen. Le due build usavano lo
stesso QML corrente. Su circa 43,2 s di ricezione, `decodium-before` ha usato
4,87/4,96 s CPU del processo e la build corrente 4,94/5,92 s. Il thread
principale campionato ha usato rispettivamente 3,45/3,47 e 3,37/4,39 s; il
campionamento può perdere thread brevi. I due messaggi attesi erano presenti
in tutte e quattro le prove, con frequenze 1500 e 1300 Hz. Questi campioni
non mostrano un risparmio CPU complessivo in questa configurazione; la
variabilità dell'interfaccia e il numero limitato di prove non bastano a
quantificare una regressione. Non confrontano il QML originale e non sono una
ricezione RF. Log nella directory `build-baseline/performance/results/`:
`live-audio-before-parity-first.log`, `live-audio-current-parity-first.log`
e `live-audio-decodium-before-visible-two-slots.log`; il log della build
corrente della seconda coppia è stato sovrascritto da una prova successiva.

### Indicatori TX del waterfall in LOW CPU

Le due animazioni della guida TX del waterfall si fermano in LOW CPU: la barra
rossa mantiene opacità piena e la linea centrale resta visibile; il profilo
normale conserva le animazioni. La prova `waterfall_tx_lowcpu_lab.py` forza
solo la visibilità grafica nel QML della build, senza attivare trasmissione o
tune, e verifica LOW CPU → normale → LOW CPU, opacità e stato delle
animazioni. Per esercitare entrambi gli overlay con rendering software il
laboratorio ne forza anche la visibilità. Una ricezione FT8 con audio virtuale
privato e waterfall visibile ha recuperato il messaggio atteso. La build è
passata; non è stata misurata la CPU durante una trasmissione reale e la prova
non sostituisce una verifica visiva con radio e backend grafici diversi.

Il banco `waterfall_tx_cpu_bench.py` ha confrontato le stesse due guide
simulate con animazioni forzate e con comportamento LOW CPU, mantenendo
TX e tune reali disattivati e forzando visibili entrambi gli overlay. In due
coppie alternate di nove secondi, il processo ha usato 0,82/0,82 s CPU con
animazioni e 0,41/0,30 s con indicatori statici. La misura include tutta
l'interfaccia, usa rendering software offscreen e monitor audio spento: non
è una misura durante TX reale né una percentuale garantita su altri backend.
Risultati `tx-visual-animated1.json`, `tx-visual-static1.json`,
`tx-visual-static2.json` e `tx-visual-animated2.json` sotto
`build-baseline/performance/results/`.

### Copia a blocchi del buffer grafico waterfall

I campioni per il solo buffer grafico vengono ora copiati in uno o due
segmenti contigui, invece di effettuare un modulo per ogni campione. Il
buffer del decoder non è modificato. Il probe `waterfall_ring_position_probe`
ha confrontato contenuto, posizione e quantità disponibili con il
riferimento, inclusi wrap, copie fino all'intera capacità e posizioni
equivalenti a 30 giorni, passando con AddressSanitizer e
UndefinedBehaviorSanitizer. Nel microbenchmark di 30.000 blocchi da 1200
campioni, la copia precedente ha richiesto 28–39 ms e quella a blocchi
2,6–4,6 ms: un test isolato, non il carico complessivo dell'app. La build
e una prova LOW CPU con audio virtuale, waterfall visibile e due slot FT8
attesi sono passate. In una singola esecuzione l'app ha usato 5,05 s CPU su
43,16 s: il valore non dimostra un risparmio rispetto alla variabilità delle
prove precedenti. Il log di quella singola esecuzione è stato sovrascritto da una prova
successiva.

### Frame dello spettro durante il worker occupato

Nel percorso FFT CPU dello spettro, il timer non prepara un nuovo buffer
mentre il worker è occupato e assegna un numero di sequenza solo a un frame
effettivamente programmato. Prima, un tick occupato poteva incrementare la
sequenza e far scartare il frame già in elaborazione. Il laboratorio
`spectrum_busy_frame_lab.py` riproduce l'interleaving nell'app: il controllo
negativo ha scartato il frame, la versione corretta lo ha consegnato e il
worker è tornato libero. Su 5000 tick forzati con worker occupato, una prova
ha misurato circa 35,1 ms prima e 26,9 ms dopo; non è una misura dell'uso
CPU complessivo né dimostra quante volte la condizione si verifichi in radio.
La build normale è stata ripristinata. Una prova LOW CPU con audio virtuale
e due slot FT8 ha recuperato entrambi i messaggi con waterfall visibile; il
log è `build-baseline/performance/results/live-audio-spectrum-busy-fixed.log`.
La prova usa rendering software, non radio reale o tutti i backend grafici.

Se l'intervallo di frequenze cambia mentre la FFT CPU è in corso, il callback
scarta ora il frame calcolato per l'intervallo precedente, libera il worker e
consente il frame successivo con la nuova banda. Il laboratorio forzato ha
verificato scarto del frame vecchio, riavvio e consegna del successivo. Una
seconda misura sintetica di 5000 tick occupati ha dato 13,3 ms prima e
17,2 ms dopo, invertendo il risultato della prima: la variabilità non permette
di attribuire un risparmio CPU stabile al controllo anticipato. Le prove sono
su renderer software, CAT e TX spenti, non su tutti i backend grafici.
Una successiva ricezione LOW CPU con audio virtuale e waterfall visibile ha
recuperato il messaggio FT8 atteso dopo questa verifica del cambio banda.

La stessa consegna asincrona controlla ora anche l'identificativo della
sessione di monitoraggio: se la ricezione viene fermata e riavviata durante
la FFT, il frame precedente viene scartato e il worker liberato. Il laboratorio
`spectrum_busy_frame_lab.py` simula l'avanzamento dell'identificativo senza
avviare acquisizione audio o radio e verifica lo scarto e la consegna del
frame successivo. È una verifica di correttezza grafica, non una misura di
risparmio CPU o una prova di stop e riavvio su una radio reale.

### Indicatori di decodifica in LOW CPU

Le pulsazioni degli indicatori DEC e dei thread FT sono disattivate in LOW
CPU durante la decodifica; gli indicatori rimangono accesi e leggibili. Il
laboratorio `statusbar_decode_lowcpu_lab.py`, con stato di decodifica simulato,
verifica LOW CPU → normale → LOW CPU, inclusi ripristino dell'opacità e ripresa
delle animazioni. Una ricezione FT8 LOW CPU con sorgente audio virtuale e
waterfall visibile ha mostrato il messaggio atteso dopo la modifica. Questa
prova non dimostra la parità di tutti i decode o la ricezione RF.

Il successivo banco `statusbar_decode_cpu_bench.py` ha confrontato gli stessi
indicatori in LOW CPU con decodifica simulata, monitor e CAT spenti, su renderer
software offscreen. In quattro passaggi alternati di nove secondi, il processo
ha usato 0,88 e 0,87 s CPU con pulsazioni forzate, 0,30 e 0,34 s con indicatori
statici. La misura riguarda l'intero processo in questo scenario sintetico: non
equivale al risparmio durante RX o decodifica reali e l'effetto dipende anche
dal renderer e dalle altre animazioni contemporanee. Risultati:
`build-baseline/performance/results/statusbar-decode-cpu.json`.

Per valutare la frequenza reale delle pulsazioni, una coppia di prove LOW CPU
ha riprodotto due slot FT8 tramite audio virtuale privato, waterfall visibile,
renderer software e CAT/TX spenti. Con pulsazioni forzate DEC era attivo in
2 campioni su 86 (ogni 500 ms); con indicatori statici era attivo in 2 su 86
e nessuna pulsazione era in funzione. Entrambe le prove hanno mostrato i due
messaggi attesi. Il processo ha usato rispettivamente 5,92 e 5,66 s CPU su
circa 43,2 s, ma una sola coppia non permette di attribuire la differenza alle
pulsazioni. Il campionamento può perdere attività brevi; il risultato non
descrive segnali più affollati, altri modi, GPU o ricezione RF. I log sono
`live-audio-decodium-visible-two-slots-status-animated.log` e
`live-audio-decodium-visible-two-slots-status-static.log` nella directory dei
risultati di performance.

### Ricostruzione dei colori del waterfall software

Quando il fallback software ricostruisce lo storico RGB dalla matrice di
intensità a 8 bit, calcola ora una tabella di 256 colori e la riutilizza per
ogni pixel. Il probe `tests/waterfall_intensity_palette_probe.cpp` confronta
tutti i 256 livelli per ciascuna coppia intera black/gain da 0 a 100:
2.611.456 colori coincidono con la formula precedente. Su dieci immagini
sintetiche da 1200×600, due passaggi alternati hanno richiesto 177–257 ms con
la formula per pixel e 8–11 ms con tabella, con checksum identico. Il banco
misura il calcolo dei colori, non la ricostruzione applicativa completa.
La build e una ricezione FT8 LOW CPU con audio virtuale e waterfall visibile
sono passate. La ricostruzione completa avviene solo quando lo storico RGB è
invalidato: questi numeri non dimostrano un risparmio CPU continuo o la parità
di tutti i decode e dei renderer.

Anche la colorazione CPU delle nuove righe usa ora una tabella di 256 valori,
conservata fino al cambio di palette, black level o color gain. Per mantenere
inalterate le palette SmartSDR e HPSDR, la tabella delle righe usa la stessa
funzione di colore continua precedente; non riusa la quantizzazione distinta
dello storico. Il probe confronta tutte le intensità e le coppie intere di
impostazioni anche con una funzione di colore continua: 5.222.912 confronti
complessivi tra righe e storico passati. La build e un FT8 LOW CPU con audio
virtuale e waterfall visibile sono passati. Non è stata ancora misurata la
CPU complessiva delle nuove righe rispetto alla versione precedente; la prova
non copre tutte le palette nell'app, tutti i segnali o radio reale.

Una prova applicativa successiva (`waterfall_row_palette_live_lab.py`) confronta
temporaneamente ogni pixel di ogni riga CPU con la formula precedente, mentre
alterna le 11 palette e modifica black level e color gain. Con due slot FT8
di audio virtuale, 122 righe e 94.672 pixel sono risultati identici, e sono
comparsi entrambi i messaggi attesi. L'app è stata poi ricompilata senza la
strumentazione. Il confronto riguarda questa sequenza di impostazioni e il
renderer software, non tutti i segnali o una radio reale; l'overhead della
verifica rende il tempo CPU del test inadatto a valutare le prestazioni.

Il banco `waterfall_row_cpu_live_bench.py` misura invece il tempo CPU del
solo ciclo di colorazione nell'app, con due slot FT8 virtuali e renderer
software; alterna formula precedente, tabella, tabella, formula precedente.
Per righe larghe 776 pixel e palette 0 sono state misurate medie di 84,8 e
86,0 microsecondi prima, 34,0 e 33,7 microsecondi con la tabella. Le quattro
prove hanno recuperato entrambi i messaggi attesi. Si tratta di circa 51–52
microsecondi per riga nel campione, non di una percentuale della CPU totale;
include l'overhead di misura e dipende da contenuto e palette. La build
ordinaria è stata ripristinata. Risultati e log sono in
`build-baseline/performance/results/waterfall-row-timing*`.

### Esplorazione degli spot DX sulla cascata

`waterfall_dx_refresh_profile.py` riempie il modello C++ con 200 spot
sintetici e richiama 1.000 volte il filtro della cascata, mantenendo 20 spot
nel passabanda. In due coppie alternate, il tempo trascorso totale era
1.114/921 ms con i log diagnostici e 1.058/815 ms senza. I valori si
sovrappongono; ogni chiamata ha quindi richiesto circa 0,8–1,1 ms in questo
test forzato, non nella normale frequenza degli aggiornamenti. La prova usa
renderer software, CAT e ricezione spenti e output catturato; non misura la
CPU completa o un cluster reale. Non è stata cambiata la gestione degli spot.
Risultati: `waterfall-dx-refresh-profile-paired.json`.

### Esplorazione della memoria e dell'allocatore Linux

`idle_memory_map_profile.py` campiona `/proc/<pid>/smaps` dopo 23 s di una
sessione FT8 isolata, LOW CPU e monitor spento: nelle due prime osservazioni
la RSS totale era 468.100 e 494.616 KiB; heap e mappature anonime costituivano
la maggior parte della memoria. Le categorie non identificano da sole il
componente responsabile e possono includere pagine condivise o avvio ancora
in corso; non sono una misura su Windows o con una radio collegata.

Un confronto esplorativo alternato (`idle_allocator_arena_ab.py`) ha avviato
il processo Linux con allocatore standard e con `MALLOC_ARENA_MAX=2`. Nelle
due prove per variante la RSS a 23 s era 545.272/537.724 KiB standard e
513.508/439.464 KiB con il limite; la CPU accumulata dall'avvio era
15,16/15,54 s e 14,89/15,81 s. La variabilità è ampia e il limite riguarda
glibc e l'intero processo, non soltanto LOW CPU.

Con la scena FT2 affollata tramite audio virtuale, quattro prove alternate
hanno recuperato tutte le otto stazioni attese: il picco RSS campionato ogni
250 ms era 550.780/527.124 KiB standard e 514.864/512.724 KiB con il limite;
CPU del processo 9,14/8,46 s e 8,51/8,11 s su circa 16,9 s. Una prova con
limite ha mostrato anche una riga ripetuta a 1 Hz di distanza. Una coppia FT8
ha restituito le stesse sette righe con picchi 543.900 e 537.696 KiB; una
coppia FT4 le stesse otto con picchi 554.944 e 508.952 KiB, rispettivamente
standard e limite. Per FT8 e FT4 c'è solo una prova per configurazione.

Sono dati preliminari con renderer software, CAT e TX spenti, non prove RF o
misure di tutti i modi e carichi concorrenti. Un limite globale potrebbe
causare contesa, modificare temporizzazioni o influire sugli altri modi; non
è stato impostato nell'applicazione. Risultati e log:
`idle-allocator-arena-ab.json`, `ft2-allocator-live-ab.json` e
`ft84-allocator-live-ab.json` in `build-baseline/performance/results/`.

### Aggiornamenti identici delle etichette waterfall

La cascata confronta ora i dati delle etichette prima di sostituire il
modello QML: se nominativo, frequenza, SNR, colori, flag e ordine non cambiano,
evita di notificare un nuovo modello e ricostruire la disposizione grafica.
La cancellazione rimane esplicita e l'overlay C++ riceve ancora la pulizia.
Il test applicativo `waterfall_label_stability_lab.py` verifica snapshot
identici, ciascun campo modificato, riordinamento e cancellazioni ripetute.

Il modello grafico viene inoltre aggiornato quando cambiano zoom, posizione
dello spettro, dimensione e spaziatura dei caratteri o colore: questi fattori
possono cambiare la disposizione anche se le etichette ricevute sono uguali.
La prova `waterfall_label_view_lab.py` verifica zoom, pan, caratteri e cambio
e ripristino del colore senza cambiare i dati delle etichette; gli eventi
continuano a essere aggregati dal timer grafico da 250 ms.

Nel banco applicativo `waterfall_label_cpu_ab.py`, con waterfall visibile,
LOW CPU, CAT e monitor spenti, 30 etichette sintetiche uguali forzate 10 volte
al secondo e renderer software, quattro prove alternate da 10 s hanno usato
1,51/0,91 s CPU del processo prima e 0,34/0,36 s dopo. Le notifiche erano
100 contro 1 per prova. È uno scenario intenzionalmente artificiale, non una
stima del risparmio in ricezione normale o su altri renderer.

In una coppia di prove FT2 con audio virtuale e scena affollata, entrambe le
varianti hanno mostrato tutti gli otto messaggi e le otto etichette attese,
con frequenze entro 1 Hz dai valori generati. I tempi CPU complessivi di una
prova per variante (8,89 e 9,33 s su circa 16,9 s) non dimostrano un
risparmio durante la ricezione. Una prova FT8 su due slot ha mostrato entrambi
i messaggi e le relative etichette. Non sono prove RF o su tutti i segnali.
Log e risultati: `waterfall-label-cpu-ab.json`,
`waterfall-label-ft2-live-ab.json` e relativi log.

Dopo il controllo delle impostazioni grafiche, un'ulteriore ricezione FT4
LOW CPU con audio virtuale e la scena affollata ha mostrato gli otto messaggi
e le otto etichette attese alle frequenze generate. È una singola esecuzione,
non un confronto di sensibilità o una prova RF; log
`live-audio-decodium-visible-ft4-custom-first.log`.

### Ridisegni della scala Decometer

Il timer del Decometer continua a gestire misure, decadimenti, allarmi e
transizioni; la scala Canvas viene ridisegnata solo se cambia uno degli
ingressi usati dal disegno. Il laboratorio grafico
`tests/tools/decometer_gauge_runtime_lab.py` confronta la versione precedente
e quella corrente: quattro immagini della scala sono identiche durante riposo,
telemetria simulata e cambio di portata. Nella sequenza di 15 tick a riposo,
più la cattura dell'immagine, il conteggio dei paint è passato da 2 a 18 nel
riferimento e da 2 a 4 con la modifica; la cattura può causare un paint anche
senza variazioni. La prova usa renderer software e telemetria simulata, non
quantifica la CPU dell'applicazione né copre tutti gli apparati. Una prova FT8
LOW CPU con due slot di audio virtuale ha recuperato entrambi i messaggi con
il Decometer chiuso: non verifica l'interazione con la telemetria reale.

Il confronto applicativo `tests/tools/decometer_live_cpu_ab.py` apre invece il
Decometer nella vera applicazione, con LOW CPU, ricezione e CAT spenti e senza
RF. In quattro prove alternate da 10 s (riferimento, modifica, modifica,
riferimento) la CPU totale del processo era rispettivamente 2,53, 1,74, 1,81
e 2,61 s; i paint della scala erano 50, 2, 2 e 50. In questo campione la
media scende da 2,57 a 1,775 s (circa 31%). È un risultato per la finestra
aperta e inattiva su questa macchina, con renderer software offscreen e
strumentazione: non rappresenta il risparmio con la finestra chiusa, durante
la ricezione o con telemetria reale. Risultati e log sono in
`build-baseline/performance/results/decometer-live-cpu-*`.

Una successiva ripetizione dello stesso banco ha confermato immagini identiche
e 50/50 ridisegni nella variante precedente contro 2/2 in quella corrente.
La CPU del processo nei quattro campioni alternati da 10 s è stata 2,22 e
1,70 s nel riferimento, 0,69 e 1,09 s nella variante corrente. Il risultato
resta limitato alla finestra aperta e inattiva con renderer software offscreen,
senza CAT, RX o RF; non rappresenta il consumo durante la ricezione.

Con `DECODIUM_LAB_OPEN_METER=1` il laboratorio audio apre il Decometer e ne
verifica la visibilità durante la ricezione. Una prova FT8 LOW CPU con due
slot virtuali, waterfall e Decometer visibili ha recuperato entrambi i
messaggi attesi, con CAT e TX spenti. Non è una prova con telemetria reale o
un confronto di CPU durante la ricezione. Log:
`live-audio-decodium-visible-two-slots-meter-open.log`.

Il banco `decometer_rx_cpu_ab.py` ha poi alternato riferimento, corrente,
corrente, riferimento durante due slot FT8 via audio virtuale, con stessa build,
LOW CPU, waterfall e Decometer visibili, CAT/TX spenti e renderer software.
Tutte le quattro prove hanno recuperato entrambi i messaggi attesi. Su circa
43,2 s, la CPU complessiva del processo era 7,33/7,03 s nel riferimento e
4,35/4,40 s con la modifica: media 7,18 contro 4,375 s, circa 39% in meno in
questo campione. Sono due prove per variante con lo strumento aperto e senza
telemetria reale: non generalizzare a RF, altre macchine o finestra chiusa.
Risultati `decometer-rx-cpu-ab.json` e log `decometer-rx-cpu-*`.

La stessa prova con `decometer_rx_cpu_ab.py FT2` usa una scena sintetica con
otto stazioni. Le quattro sessioni alternate hanno recuperato tutti gli otto
messaggi. Su circa 16,9 s, CPU totale 10,26/9,91 s nel riferimento e 8,68/8,74 s
con la modifica: medie 10,085 e 8,71 s, circa 13,6% in meno nel campione.
Stessa build, LOW CPU, Decometer e waterfall aperti, audio virtuale, CAT/TX
spenti e renderer software; due prove per variante non garantiscono risultati
su RF, con telemetria reale o con finestra chiusa. Risultati
`decometer-rx-cpu-ft2-ab.json` e relativi log.

Con `decometer_rx_cpu_ab.py FT4`, la scena sintetica a otto stazioni ha
prodotto tutti gli otto messaggi in quattro prove alternate. Su circa 19,2 s,
la CPU totale era 3,17/3,05 s con la finestra precedente e 1,82/1,88 s con
quella ottimizzata: medie 3,11 e 1,85 s, circa 40,5% in meno nel campione.
Stessa build, LOW CPU, Decometer e waterfall aperti, audio virtuale e renderer
software; non è una prova RF o con telemetria reale. Risultati
`decometer-rx-cpu-ft4-ab.json` e relativi log.

### Finestra DecoSyncTime nascosta

Il pannello rimane istanziato anche quando la sua finestra è chiusa: la sua
proprietà `visible` non riflette da sola quella della finestra. Ora la
richiesta di ridisegno del grafico e la costruzione del testo visibile sono
condizionate alla finestra effettivamente visibile e non minimizzata. Il
timer continua a raccogliere i campioni a 1 Hz anche quando la finestra è
nascosta, preservando lo storico precedente; alla riapertura testo e grafico
vengono aggiornati. `decosync_window_visibility_lab.py` verifica chiusura,
campionamento senza richieste grafiche, riapertura e aggiornamento immediato
nell'app. Non è stato misurato un risparmio della CPU complessiva e la prova
non verifica il comportamento di minimizzazione su tutti i gestori finestre.
Una ricezione FT2 LOW CPU con audio virtuale debole e waterfall visibile ha
recuperato il messaggio atteso; non verifica la ricezione RF.

### Esplorazione del filtro audio condiviso

Il banco `audio_fil4_simd_explore.py` ha confrontato la sola somma FIR scalare
e una variante SSE2 con lo stesso ordine di accumulo: 100.000 vettori casuali
di campioni a 16 bit hanno prodotto somme identiche bit per bit. Su un milione
di somme, a `-Os` i tempi trascorsi erano 53,4–54,2 ms scalare e 43,4–43,5 ms
SSE2, mentre a `-O2` si sovrapponevano. Non è un banco della pipeline completa
né una prova di tutti gli input o delle build con altre opzioni: il guadagno
stimato per il normale flusso da 12.000 campioni/s è piccolo, perciò non è
stata introdotta la variante nel percorso audio dei decoder.

### Notifiche periodiche della condivisione SSTV

Il worker di condivisione controlla e pubblica gli snapshot ogni 750 ms,
inclusi scadenze e handoff. Un tentativo di sopprimere `stateChanged` per
snapshot identici aveva superato il test del controller e una prova FT8 con
audio virtuale, ma è stato ritirato: il segnale aggiorna anche la diagnostica
SSTV, che raccoglie contatori di ricezione indipendenti dalle proprietà di
condivisione. Tali contatori non erano coperti dal test e sopprimere le
notifiche potrebbe lasciarli non aggiornati. Nessun risparmio CPU è attribuito
a questo tentativo; il comportamento originale resta in vigore.

### Cambio ripetuto tra FT8, FT4 e FT2

`tests/tools/mode_resource_profile.py --focus-ft` esegue due cicli
FT8 → FT4 → FT2 nello stesso processo, con LOW CPU e monitor e TX disattivati.
Nei sei file WAV di prova è stato decodificato `CQ K1ABC FN42` alla frequenza
attesa. Dopo il primo utilizzo, i thread di decodifica restano presenti e
vengono riutilizzati: quelli dei modi non selezionati hanno registrato incrementi
di CPU nulli o di 0,01 secondi tra i campioni. Il thread `Ft2LinkDecode`,
creato all'avvio, ha registrato 0,00 secondi in tutti i campioni. La RAM RSS
campionata è passata da circa 704 MiB dopo il primo FT8 a 749 MiB dopo FT2,
restando poi circa stabile. Sono osservazioni di una sola esecuzione, non un
confronto con la versione precedente né una misura dell'uso di FT2-Link attivo.
Non giustificano, da sole, la rimozione dei thread o una modifica al ciclo di
vita dei decoder. Risultati in `mode-resource-focus-ft-profile.json` nella
cartella `build-baseline/performance/results`.

### Controllo periodico della finestra Logbook

La finestra `LogWindow.qml` aggiorna l'elenco ogni 15 secondi quando è visibile
e ha il focus; `LogWindowContent.qml` contiene invece un timer separato di
3 secondi e non risulta istanziato dalle altre QML esaminate. L'aggiornamento
periodico della prima finestra serve anche a rilevare variazioni esterne al
logbook. Non è sufficiente saltarlo quando la cache condivisa è valida: altre
chiamate possono ricostruire la cache senza aggiornare la lista già mostrata
nella finestra. Il controllo confronta ora la versione del file catturata prima
della ricerca visualizzata con quella corrente, includendo percorso, dimensione,
data di modifica e generazione di invalidazione della cache. Ricerca, notifiche
e cambio logbook continuano a richiedere un aggiornamento immediato.

`log_revision_runtime_lab.py` ha verificato un controllo senza modifiche (zero
ricostruzioni) e una scrittura esterna dopo cui un altro componente ha
ricostruito la cache: la finestra ha aggiornato due righe e il filtro ha
selezionato il nuovo QSO. Il banco con 3.000 QSO ha misurato 2–3 ms per 100
controlli invariati contro 79–96 ms per singola ricostruzione, tempo trascorso,
non CPU. Una prova FT8 LOW CPU con audio virtuale, Logbook e waterfall visibili
ha recuperato il messaggio atteso; non verifica RF, tutti i filtri o filesystem
che conservano dimensione e data di modifica identiche dopo una scrittura.

Il laboratorio `tests/tools/log_refresh_cost_lab.py` apre la vera finestra
con un ADIF sintetico isolato di 3.000 QSO e LOW CPU. In due esecuzioni, cinque
chiamate consecutive a `refreshLog()` hanno impiegato rispettivamente
103/85/71/82/70 ms e 115/91/66/82/68 ms di tempo trascorso sul thread QML.
Nella seconda esecuzione, cinque sole ricerche hanno richiesto 13–32 ms e
cinque calcoli delle statistiche 4–5 ms. Le componenti non sono strettamente
additive e i tempi includono anche possibili pause di scheduling; non sono una
misura di CPU. I controlli hanno confermato 3.000 righe e il totale statistico.
Renderer software offscreen, CAT, monitor e TX spenti: la prova non quantifica
il carico durante la ricezione né la frequenza effettiva del timer su tutti i
gestori di finestre. I risultati dell'ultima esecuzione sono in
`build-baseline/performance/results/log-refresh-cost.json`.

### Controlli finali con finestra Logbook

La build `decodium_qml` è riuscita. Con audio virtuale privato, LOW CPU,
waterfall e Logbook visibili, FT8, FT4 e FT2 hanno ricevuto ciascuno il
messaggio sintetico atteso. I due cicli successivi FT8 → FT4 → FT2 con WAV
hanno recuperato tutte e sei le righe attese. Queste non sono prove RF.

Il profilo inattivo `runtime_thread_profile.py`, con LOW CPU richiesto, CAT e
monitor spenti, ha alternato riferimento, corrente, corrente, riferimento
sullo stesso desktop OpenGL/Wayland e QML corrente. Su 10 secondi circa, la
CPU di processo era 4,09 e 5,06 s nel riferimento, 5,92 e 5,77 s nella build
corrente; la RSS finale era rispettivamente 676/690 MiB e 704/700 MiB circa.
Il thread di rendering e quello del driver grafico pesano molto; il campione
non identifica la causa e può includere attività di avvio. I valori mostrano
una possibile regressione in questo scenario, non un risparmio complessivo:
servono indagini e confronti più ampi, compresa la ricezione. I quattro file
`final-idle-*.json` e i log relativi sono in `build-baseline/performance/results`.

Una coppia aggiuntiva con `QT_QPA_PLATFORM=offscreen` e renderer software ha
usato 6,65 s CPU nel riferimento e 5,98 s nella build corrente su circa 10 s;
il thread `QQmlThread` ha consumato rispettivamente 4,59 e 2,15 s, indicando
che le attività di caricamento influenzano ancora il campione. Una sola coppia
e condizioni grafiche diverse non spiegano né smentiscono il risultato OpenGL.
Occorre misurare uno stato stabile più tardi e verificare la ricezione. Dati:
`final-idle-software-before.json` e `final-idle-software-current.json`.

Per ridurre l'effetto dell'avvio, `runtime_thread_profile.py --settle-seconds
60` ha ripetuto il confronto sul desktop OpenGL in ordine riferimento, corrente,
corrente, riferimento: su 10 s circa la CPU totale è stata 1,32/1,01 s per il
riferimento e 0,86/1,45 s per la build corrente. Le misure si sovrappongono:
non dimostrano né il precedente aumento né un risparmio stabile. La sola
attesa non dimostra che ogni attività di avvio sia terminata; LOW CPU è
richiesto nel profilo, con CAT e monitor spenti e QML corrente per entrambe le
build. Risultati `final-settled-*.json` e log nella directory performance.

### Confronto finale FT2 in ricezione virtuale

Quattro esecuzioni alternate riferimento, corrente, corrente, riferimento,
stesso QML corrente, hanno riprodotto `wavs-ft2-ft4/ft2-0.wav` in LOW CPU con
waterfall visibile, CAT e TX spenti e renderer software offscreen. Tutte hanno
ricevuto `CQ K1ABC FN42` a 1000 Hz (tolleranza 2 Hz). Per circa 16,9 s di
ricezione la CPU totale del processo è stata 8,96/9,39 s nel riferimento e
8,42/8,06 s nella build corrente: le medie di questo piccolo campione sono
9,175 e 8,24 s, circa 10% in meno. Il picco RSS campionato ogni 250 ms era
550.140/547.220 KiB e 542.308/570.536 KiB rispettivamente. Due prove per
variante e un singolo segnale non dimostrano lo stesso guadagno su altre bande,
hardware o in RF; il QML originale non è incluso nel confronto. I log con lo
stesso nome sono stati sovrascritti dalla seconda esecuzione di ciascuna build.

Il confronto `ft2_busy_final_ab.py` ha ripetuto la sequenza alternata con la
scena sintetica affollata a otto stazioni. Tutte le prove hanno recuperato
8/8 messaggi attesi, con differenze di frequenza fino a 1 Hz tra osservazioni.
Su circa 16,9 s, CPU totale 10,27/10,20 s nel riferimento e 9,07/8,85 s nella
build corrente: media 10,235 contro 8,96 s, circa 12,5% in meno nel campione.
Picco RSS campionato 550.336/548.176 e 570.884/551.628 KiB. Due prove per
variante su un segnale sintetico non garantiscono lo stesso guadagno o la
parità di ogni decode su altre condizioni o in RF. Stesso QML corrente per
entrambe, LOW CPU, audio virtuale, waterfall visibile e CAT/TX spenti; log e
misure preservati in `ft2-busy-final-*` nella directory performance.

Con l'argomento `FT4`, lo stesso banco ha confrontato la scena FT4 sintetica
affollata nelle stesse condizioni. Tutte le quattro esecuzioni hanno recuperato
8/8 messaggi con le stesse frequenze. Su circa 19,2 s, CPU totale 2,04/2,30 s
nel riferimento e 1,87/1,86 s nella build corrente: media 2,17 contro 1,865 s,
circa 14% in meno in questo piccolo campione. Picco RSS campionato
552.652/550.544 e 554.372/531.312 KiB. Due prove per variante e un solo WAV
non dimostrano lo stesso risultato su tutti i segnali o in RF; QML corrente per
entrambe. Risultati in `ft4-busy-final-ab.json` e log `ft4-busy-final-*`.

Con argomento `FT8` lo stesso banco ha riprodotto la registrazione
`181201_180245.wav` tramite audio virtuale. Su quattro esecuzioni alternate,
la prima di ciascuna build ha recuperato tutti i sette messaggi del precedente
confronto WAV; nella seconda di entrambe manca `CQ UW5EJX/MM`. Le due prove
successive alla prima hanno anche mostrato `W1ATV F6DKQ 73`, oltre ai messaggi
restanti. La verifica degli attesi termina quindi con errore: il risultato non
dimostra una regressione specifica della build, ma nemmeno la stabilità della
ricezione virtuale. CPU di processo, su circa 28,1 s: riferimento 3,93/3,05 s,
corrente 4,05/3,07 s; campione troppo variabile per sostenere un risparmio.
LOW CPU, QML corrente, CAT/TX spenti, renderer software; non è ricezione RF.
Misure e log `ft8-busy-final-*` sono conservati nella directory performance.

L'ispezione dei log mostra una differenza di sequenza comune alle due build:
nelle due prove senza `CQ UW5EJX/MM` è stato avviato un decode vuoto circa
205–207 ms prima dell'inizio misurato della riproduzione; nelle altre due il
primo dispatch registrato era circa 14,8 s dopo l'inizio. In tutte, il dispatch
relativo allo slot riprodotto cade circa 14,8 s dopo l'inizio. Il decode vuoto
può influenzare lo stato o indicare una diversa temporizzazione d'ingresso, ma
questa associazione non ne dimostra la causa: non sono stati confrontati i
campioni audio acquisiti né isolati gli effetti dello storico. La riproduzione
parte circa 300 ms dopo il confine calcolato dal laboratorio; tale timestamp
precede l'avvio effettivo di `paplay`. Serve una prova controllata prima di
attribuire la mancata riga al decoder o alla sorgente virtuale.

Un controllo successivo con `DECODIUM_LAB_WARM_SLOTS=1` lascia trascorrere
uno slot supplementare di silenzio prima della riproduzione. Nella singola
prova della build corrente è avvenuto un dispatch vuoto circa 83 ms prima del
timestamp di riproduzione e sono comparsi tutti e sette i messaggi, incluso
`CQ UW5EJX/MM`, più `W1ATV F6DKQ 73`. Quindi la presenza del dispatch vuoto
non basta, da sola, a prevedere l'assenza della riga. L'attesa non isola
completamente la temporizzazione della sorgente virtuale, e una prova non
dimostra ripetibilità o parità RF. Log `ft8-warm-slot-control.log`.

Con lo stesso slot di riscaldamento e inizio programmato 700 ms dopo il
confine, un'unica esecuzione per build ha restituito 7 righe nel riferimento
e 6 nella corrente: `CQ UW5EJX/MM` compare solo nel riferimento. Sei messaggi
e frequenze coincidono, con tempi stimati analoghi. Alla partenza programmata
300 ms, una precedente prova corrente aveva trovato 8 righe. La coppia a
700 ms segnala una possibile perdita da indagare, ma non esclude variabilità
della sorgente virtuale: il comando `paplay` parte dopo il timestamp e i
campioni acquisiti non sono stati confrontati. Non è una prova RF né una
valutazione statistica. Log `ft8-offset-070-before.log` e `ft8-offset-070.log`.

Ripetendo a 700 ms, prima il riferimento e poi la build corrente, entrambi
hanno restituito le stesse 7 righe incluso `CQ UW5EJX/MM`. Sulle due prove per
build a questo offset, il riferimento ha recuperato la riga 2/2 e la build
corrente 1/2. Il singolo mancato recupero non è dunque deterministico; con
campioni audio non catturati e solo due esecuzioni per variante non si può
stabilire se derivi da variabilità dell'ingresso o dal decoder. Log aggiuntivi
`ft8-offset-070-before-2.log` e `ft8-offset-070-current-2.log`.

Per confrontare input esattamente uguali sono state create due copie mono a
12 kHz della registrazione, anteponendo rispettivamente 400 e 700 ms di
silenzio e tagliando la coda per mantenere 15 secondi. In LOW CPU, tramite
`bridge_wav_parity_lab.py`, entrambe le build hanno prodotto le stesse 7 righe
e frequenze in entrambi i file, compreso `CQ UW5EJX/MM`. È una prova per file
e build con apertura WAV, non replica il percorso audio in diretta né prova la
parità su tutte le condizioni; mostra però che questi stessi input ritardati
non riproducono la perdita osservata nella sorgente virtuale. Hash, risultati
e log in `ft8-shift-parity.json` e `ft8-shift-parity-logs`.

Il banco `ft8_audio_capture_build.py` crea una build diagnostica separata che,
solo in modalità test e con directory esplicita, salva gli snapshot FT8 prima
del decoder; ripristina poi sorgente e build normali. Una prova virtuale a
700 ms ha salvato tre snapshot e recuperato anche `CQ UW5EJX/MM`. Riaprendo
lo snapshot del segnale (`ft8-input-3.wav`) entrambe le build hanno restituito
le stesse 8 righe e frequenze. La cattura può influenzare la temporizzazione e
questa prova non contiene la perdita: occorre catturare un caso in cui la riga
manchi per verificare se l'input identico riproduce la differenza. Risultato
`ft8-capture-1-parity.json`; log e WAV nella directory performance.

Nella terza acquisizione a 700 ms la riga `CQ UW5EJX/MM` non è comparsa nella
ricezione virtuale. Lo snapshot dello slot interessato (`ft8-capture-3/
ft8-input-2.wav`) riaperto da entrambe le build ha prodotto le stesse 6 righe
e frequenze, senza la riga mancante in entrambe. È una prova sul medesimo
buffer catturato: indica che in questo caso il riferimento non la recupera
neppure dall'input della build corrente, ma non esclude differenze nel percorso
live, stato o temporizzazione e non dimostra parità universale. La seconda
acquisizione aveva invece recuperato la riga. Risultati
`ft8-capture-3-parity.json`, log `ft8-capture-2.log`/`ft8-capture-3.log`.

Il confronto diretto dei due buffer da 180.000 campioni mostra che il buffer
della prova senza la riga è ritardato di esattamente 256 campioni rispetto a
quello della prova con la riga: tutti i 179.744 campioni sovrapposti sono
identici bit per bit. A 12 kHz lo scarto è circa 21,3 ms; i 256 campioni non
sovrapposti alle estremità non rientrano nel confronto. Anche riaprendo il
buffer della prova con la riga, riferimento e build corrente hanno prodotto
le stesse 7 righe e frequenze, incluso `CQ UW5EJX/MM`. Sul buffer ritardato
entrambe hanno prodotto le stesse 6 righe, senza quel messaggio. Questa coppia
associa la differenza a input con allineamento diverso; non dimostra da sola
quale parte del segnale o quale fase del decoder ne sia responsabile, né
garantisce la parità in tutti i casi live. Risultati e hash in
`ft8-capture-2-parity.json` e `ft8-capture-3-parity.json`.

Un controllo aggiuntivo ha anticipato il buffer della prova senza la riga di
256 campioni, aggiungendo altrettanti campioni di silenzio in coda. Entrambe le
build hanno restituito le stesse 7 righe, inclusa `CQ UW5EJX/MM`, contro le 6
del buffer non modificato. È una prova singola e lo spostamento sostituisce
anche i campioni terminali: identifica sensibilità alla finestra temporale,
ma non individua la fase responsabile né giustifica uno spostamento generale
dei buffer live. Risultato `ft8-capture-advance256-parity.json` e relativi log.

Un controllo separato ha azzerato soltanto gli ultimi 256 campioni del buffer
originale, senza anticipare gli altri. Entrambe le build hanno conservato le
stesse 6 righe e la riga mancante non è ricomparsa. In questo esperimento il
solo azzeramento della coda non riproduce quindi il recupero osservato con lo
spostamento; resta possibile l'interazione con la posizione e altri dettagli
del buffer. Una prova per variante non giustifica modifiche al percorso live.
Risultato `ft8-capture-tailzero-parity.json` e relativi log.

Con `DECODIUM_FT8_CAND_LOG=1`, il buffer senza riga ha prodotto 15, 21 e 64
candidati nei tre passaggi; quello anticipato ne ha prodotti 17, 21 e 71,
con le stesse soglie rispettivamente 1,800, 1,800 e 1,584. I conteggi sono
identici tra riferimento e build corrente per ogni buffer, come le 6 e 7
righe risultanti. La diagnostica conta tutti i candidati, non identifica
quello della stazione mancante: non dimostra se sia escluso in ricerca o
fallisca dopo, e non è un'analisi delle prestazioni. Risultato
`ft8-candidate-count-parity.json` e log `ft8-candidate-count-logs`.

Una build diagnostica separata (`ft8_candidate_trace_build.py`) ha tracciato
i candidati tra 735 e 775 Hz e ripristinato il codice e la build normali. Nel
terzo passaggio entrambi i buffer presentano un candidato a 753,125 Hz: quello
senza la riga con dt 0,72 e sync 2,87086, quello anticipato con dt 0,68 e
sync 3,34216. La build diagnostica ha prodotto gli stessi 6 e 7 decode della
build corrente sui due file. La sola frequenza non identifica con certezza il
candidato con `CQ UW5EJX/MM`, quindi non si può ancora dire quale fase lo
perda; stampe e output possono modificare i tempi. Risultati e log:
`ft8-candidate-detail-parity.json` e `ft8-candidate-detail-logs`.

La successiva traccia diagnostica del risultato identifica il candidato:
nel buffer anticipato, il candidato a 753,125 Hz del terzo passaggio ha
prodotto `CQ UW5EJX/MM` con CRC valido; nel buffer originale lo stesso candidato
in frequenza è stato tentato, ma il risultato aveva CRC non valido e nessun
messaggio. L'esito mostrato è quello del tentativo principale prima di eventuali
recuperi. La diagnostica ha conservato 6/7 righe rispetto alla build normale;
non individua ancora la causa del fallimento interno e le stampe possono
influenzare i tempi. Log `ft8-candidate-result-logs` e confronto
`ft8-candidate-result-parity.json`.

Una traccia per target (`DECODIUM_FT8_EXPECTED_TARGETS`) chiarisce il caso:
entrambi i buffer hanno superato la soglia di sincronizzazione (20 contro 8).
Nel buffer originale un tentativo LDPC ha prodotto `CQ UW5EJX/MM` con
13 errori e dmin 25,8597, ma il controllo `false8` lo ha scartato; nel buffer
anticipato il messaggio è stato accettato con 14 errori e dmin 8,21313. In
questo percorso `false8` viene applicato se la somma errori+dmin supera 36,6
o se è presente AP: ciò descrive il comportamento osservato, non prova che
rilassare il filtro sia sicuro rispetto ai falsi decode. La prova tracciata
ha mantenuto 6/7 righe come la build normale. Nessuna modifica ai filtri o
alla modalità LOW CPU; log `ft8-target-trace-logs` e risultato
`ft8-target-trace-parity.json`.

L'ispezione di `ft8var_chkfalse8_impl` mostra che alcune verifiche dei
nominativi composti consultano un database di nominativi: se il database è
valido, la sola assenza di una chiamata può contribuire al rifiuto; se il
database non è valido, il controllo di appartenenza non rifiuta. Il ramo
esatto e il contenuto del database per questa prova non sono stati tracciati,
quindi non si può attribuire il rifiuto a quella singola condizione. I test
esistenti verificano un CQ ordinario valido e un nominativo non valido, non
coprono specificamente questo messaggio `/MM` e le condizioni di database.
Nessun filtro è stato allentato sulla base della sola ispezione.

Una build diagnostica ha registrato per il rifiuto `i3=4`, `n3=1`, AP=0,
13 errori e dmin 25,85974. Nel percorso di test avviato dalla radice del
repository il primo database disponibile è `resources/runtime/ALLCALL7.TXT`:
contiene 100.302 nominativi, tutte le 16 categorie richieste dalla verifica di
validità sono presenti e `UW5EJX` non risulta nell'elenco. Seguendo il ramo
`i3=4` per `CQ UW5EJX/MM`, il controllo estrae `UW5EJX` e verifica la sua
presenza nel database; questa è la causa coerente con il rifiuto osservato.
Il ramo interno non è stato strumentato direttamente, quindi questa
attribuzione è un'inferenza dal codice e dal database di test. Il filtro è
inalterato: un nominativo non incluso non è per sé prova sufficiente a
distinguere un CQ valido da un falso. Traccia in `ft8-false8-trace-logs`.

Un esperimento successivo ha copiato il database in una directory temporanea,
aggiungendo soltanto `UW5EJX` prima del terminatore, e avviato entrambe le
build da quella directory sullo stesso buffer catturato. Entrambe hanno
prodotto le stesse 7 righe, incluso `CQ UW5EJX/MM`, contro le 6 ottenute con
il database originale. Il database del programma è rimasto invariato. Questa
prova conferma che la presenza del nominativo nel database selezionato cambia
l'esito in questo caso, ma non dimostra sicurezza di un allentamento generale
del filtro né suggerisce di aggiungere automaticamente nominativi. Risultato
`ft8-allcall-isolated-parity.json` e log `ft8-allcall-isolated-logs`.

Il banco Linux `startup_ft_final_ab.py` ha confrontato quattro avvii in ordine
riferimento, corrente, corrente, riferimento su questo desktop, con impostazioni
isolate, LOW CPU richiesto, CAT e monitor disattivati e QML corrente per entrambe
le build. Dall'avvio del processo alla creazione della finestra principale i
tempi erano 10,492/11,084 s nel riferimento e 10,839/11,173 s nella build
corrente; alla fase grafica waterfall 12,092/13,906 e 12,436/12,989 s. Sono
milestone di log, non primo frame né ricezione pronta; due prove per variante
non dimostrano un vantaggio stabile sull'avvio. Dati `ft-startup-final-ab.json`
e log `ft-startup-final-*`.

Nel laboratorio audio privato, con monitor richiesto dal test dopo 9 s e
osservazione QML ogni 3 s, il primo campione che mostrava LOW CPU, monitor
attivo e l'ingresso virtuale selezionato è arrivato dopo 14,942 s nel
riferimento e 12,702 s nella build corrente. Entrambe hanno successivamente
decodificato il segnale FT8 di prova. È una sola coppia, con ritardo deliberato
e campionamento grossolano, quindi non misura l'istante esatto di disponibilità
né dimostra un guadagno stabile; il QML è corrente per entrambe. I log sono
`live-audio-decodium-before-visible.log` e `live-audio-decodium-visible.log`.

Con `DECODIUM_LAB_FT_SWITCH=1`, il laboratorio audio virtuale avvia il monitor
FT8 e passa a FT4, FT2 e di nuovo FT8 prima della riproduzione. Una prova LOW
CPU ha rilevato monitor attivo e TX spento a ogni passaggio e ha poi recuperato
`CQ K1ABC FN42` in FT8 al ritorno. Il segnale è stato riprodotto dopo la
sequenza, quindi la prova non verifica decode in corso durante il cambio né
ricezione FT4/FT2 in questa stessa sessione. Log
`live-audio-decodium-visible-ft-switch.log`.

Lo stesso banco è stato esteso al ritorno nei tre modi: in una prova ciascuna,
FT4 → FT2 → FT8 → FT4 e FT2 → FT4 → FT8 → FT2 hanno mantenuto monitor e LOW
CPU attivi e TX spento ai passaggi, recuperando il rispettivo `CQ K1ABC FN42`
dopo il ritorno. Il segnale è riprodotto soltanto alla fine: non verifica la
cancellazione di un decode contemporaneo al cambio, né RF. Log
`live-audio-decodium-visible-ft4-ft-switch-custom-first.log` e relativo FT2.

`bridge_active_decode_switch_ab.py --low-cpu` verifica separatamente FT8 →
FT4 mentre `bridge.decoding` è vero per un WAV, poi ritorno a FT8 e decode di
un secondo WAV. La build corrente e il riferimento hanno entrambi osservato
il cambio con stato di decodifica ripulito e recuperato `CQ 9H1SR JM75`; LOW
CPU era attivo. È una prova WAV con monitor spento, non verifica un cambio
durante audio live né la decodifica nel modo intermedio FT4.

Il banco è stato esteso a `--mode FT4` e `--mode FT2`: una prova LOW CPU per
ciascun modo sulla build corrente ha osservato rispettivamente il cambio verso
FT2 o FT8 mentre il decode WAV era attivo, seguito dal recupero del segnale nel
modo iniziale. Nel caso FT2 il controllo corretto ha rilevato stato inattivo
anche subito dopo il cambio. Una prima esecuzione FT2 è stata invalidata per
un errore di sostituzione dei nomi nel banco, corretto prima della prova valida.
Le stesse due prove sul riferimento sono passate: modo cambiato, stato
immediatamente inattivo e segnale recuperato. Monitor spento; non è una prova
live né un confronto completo delle prestazioni.

Una prova aggiuntiva `DECODIUM_LAB_ACTIVE_SWITCH=1` usa audio virtuale FT8,
attende audio e `bridge.decoding`, poi passa a FT4 in un evento successivo e
ritorna a FT8 dopo 1,8 s. Nella prova a due slot il secondo messaggio non è
comparso nella build corrente; un tentativo sul riferimento ha anch'esso avuto
righe vuote, ma la sua verifica automatica era invalida perché non ammetteva
il modo FT4 temporaneo. Il riarmo del timer nel codice attende il confine UTC
successivo prima di iniziare un periodo completo: è coerente con il possibile
salto dello slot già iniziato, ma non basta a dimostrarne la causa. Ripetendo
con un terzo slot identico al secondo, la build corrente ha recuperato il
messaggio `CQ 9H1SR JM75`, con monitor e LOW CPU attivi e TX spento. Il test
non stabilisce l'istante di ripresa esatto né garantisce che il secondo slot
venga decodificato; una prova comparativa corretta resta necessaria. Log
`live-audio-decodium-visible-two-slots-active-switch.log`.

La prova corretta a tre slot è passata anche sul riferimento: dopo il cambio
FT8 → FT4 durante il decode e il ritorno a FT8, il messaggio del terzo slot è
stato recuperato. Un precedente tentativo sul riferimento è stato invalidato
da timeout dello stream audio virtuale e non conta come risultato di decode.
Una prova riuscita per build non verifica tutti gli allineamenti o apparati.
Log `live-audio-decodium-before-visible-two-slots-active-switch.log`.

Nei log delle due prove riuscite, tra il dispatch interrotto dal cambio e il
successivo dispatch FT8 passano rispettivamente 30.001 ms nella build corrente
e 30.000 ms nel riferimento, cioè due periodi nominali. Il risultato recuperato
è presente in entrambi i casi. Questo conferma l'intervallo osservato in queste
due esecuzioni; il codice attende il successivo confine UTC prima di avviare
l'accumulo. Non dimostra che si possa decodificare in sicurezza lo slot parziale
saltato: non è stato cambiato il riarmo del timer.

In un controllo offline è stato sostituito con silenzio il primo 1,8 s del WAV
`ft8-9h1sr-1300.wav`, lasciando invariati posizione e durata degli altri
campioni. Entrambe le build hanno recuperato `CQ 9H1SR JM75` a 1300 Hz. Questo
singolo segnale dimostra che un input parziale può essere decodificabile, ma
non riproduce l'avvio dell'accumulo a metà slot, né dimostra che anticipare il
timer preservi gli slot successivi o tutti i decode. Risultato
`ft8-partial-1800-parity.json`; comportamento del timer invariato.

Il banco isolato `bridge_periodtimerarm_ab.py` è stato rieseguito con Qt 6.11:
il metodo corrente coincide con il riferimento del refactoring e sono passati
i controlli su avvio allineato, modi immediati e callback con sessione o modo
superati. Il banco usa dipendenze simulate e non verifica la ricezione live
né una variante di riarmo anticipato.

Due ulteriori WAV FT8 sintetici a -18 dB, anch'essi con i primi 1,8 s
sostituiti da silenzio, hanno restituito entrambi `K1ABC W9XYZ EN37` nelle due
build, a 1500–1501 Hz. Il confronto file preserva posizione e durata ma non
simula la riapertura dell'ingresso o il primo buffer parziale; due semi e un
livello non dimostrano sensibilità sufficiente per modificare il timer live.
Risultato `ft8-partial-weak18-parity.json` e relativi log.

La build sperimentale `ft8_immediate_arm_build.py`, attiva solo in test con
variabile esplicita, avvia il timer FT8 immediatamente dopo `setMode`; sorgente
e build normali sono ripristinati. In una prova di cambio durante audio virtuale
a tre slot, la variante ha aggiunto un dispatch nel secondo periodo (167.424
campioni), ma quel decode ha restituito zero righe; il messaggio è comparso nel
terzo slot completo. Il riarmo immediato ha quindi aggiunto lavoro senza
recuperare lo slot in questa prova. Il WAV con silenzio iniziale non rappresenta
l'allineamento del buffer live parziale. Una prova non dimostra l'esito per ogni
segnale; la variante non è stata integrata. Log
`live-audio-decodium-ft8-immediate-arm-lab-visible-two-slots-active-switch.log`.

Su una registrazione FT8 con più stazioni, una coppia di prove dopo cambio
durante decode ha mostrato 8 righe nello slot completo con il riarmo normale;
la variante immediata con padding ne ha mostrate 7 nello slot parziale e 7
nello slot completo successivo. La riga `W1ATV F6DKQ 73` presente nel normale
non è comparsa nella variante. Sono riproduzioni separate e un solo campione:
non provano che il padding causi la differenza, ma non soddisfano il requisito
di preservare tutti i decode. La variante resta esclusivamente sperimentale.
Log `ft8-partial-busy-standard.log` e `ft8-partial-busy-experimental.log`.

Un controllo offline ha anteposto 288 campioni nulli allo snapshot completo
`ft8-capture-1/ft8-input-3.wav`, tagliando altrettanti campioni finali per
mantenere la durata. Con o senza questo spostamento, entrambe le build hanno
restituito le stesse 8 righe e frequenze, inclusa `W1ATV F6DKQ 73`. È un solo
buffer e non replica i diversi campioni ricevuti nelle prove live; non dimostra
che il padding di uno slot successivo sia sempre innocuo. Risultato e hash
`ft8-capture-pad288-parity.json`.

Una seconda variante di laboratorio applica il padding soltanto quando
mancano almeno 6.000 campioni (500 ms), soglia scelta per l'esperimento e non
in base a un'analisi UTC. Nella singola prova affollata ha applicato il padding
una sola volta (12.576 campioni) e prodotto 7 righe nello slot parziale e 7 nel
successivo completo; `W1ATV F6DKQ 73`, osservato nella prova normale separata,
non è comparso. Le riproduzioni separate non dimostrano causalità; la soglia non
garantisce che il padding interessi solo il primo slot e non prova la parità.
Variante non integrata; log `ft8-partial-busy-threshold.log`.

Ripetendo una volta ciascuna configurazione, la riga `W1ATV F6DKQ 73` è
apparsa negli slot completi di entrambe le prove normali (8/8) e in nessuna
delle due prove con soglia sperimentale (7/8 sia nel parziale sia nel completo).
Le acquisizioni audio sono separate, quindi questo piccolo confronto non prova
la causa; rafforza però la decisione di non integrare la variante. Log
aggiuntivi `ft8-partial-busy-standard-2.log` e `ft8-partial-busy-threshold-2.log`.

Una successiva coppia con build diagnostiche ha acquisito lo snapshot dello
slot completo dopo il cambio. Quello normale ha 180.000 campioni, quello della
variante 179.968; il campione 0 della variante coincide con il campione 8.704
del normale e tutti i 171.296 campioni sovrapposti sono identici. L'inizio
della variante è quindi più avanti di circa 725 ms nel segnale. Riaprendo i
due snapshot separatamente, entrambe le build danno 8 righe sul normale e 7
sulla variante. La riproduzione live è stata separata, ma i campioni in comune
sono identici; il prelievo di campioni dal periodo successivo durante il decode
parziale è una possibile spiegazione, non ancora isolata. La variante rimane
esclusa. Dati `ft8-switch-capture-parity.json` e directory
`ft8-switch-capture-standard`/`ft8-switch-capture-experiment`.

Una variante ulteriore di sola prova evita di consumare dal buffer successivo
i campioni presi in prestito per il parziale. In una ricezione affollata ha
prodotto 7 righe nel parziale e 8 nello slot completo successivo, inclusa
`W1ATV F6DKQ 73`. Il buffer completo catturato contiene 180.000 campioni; una
parte iniziale coincide nella posizione con la cattura normale, ma i buffer
non sono complessivamente identici. È una singola prova con acquisizioni
separate: non verifica i successivi slot, tutti gli allineamenti, segnali
deboli o falsi decode; trattenere sempre i campioni potrebbe duplicare audio
usato in precedenza. La variante resta sperimentale. Log
`ft8-partial-busy-keep.log`, catture `ft8-switch-capture-keep`.

Una variante diagnostica limita il mantenimento dei campioni presi in prestito
ai buffer con almeno 6.000 campioni mancanti prima del prestito; il padding usa
la stessa soglia sulla lunghezza successiva. In una prova con la registrazione
affollata ha prodotto 7 righe nello slot parziale e 8 nel successivo, incluso
`W1ATV F6DKQ 73`; il padding è stato segnalato una sola volta. La soglia non
identifica con certezza il primo slot dopo il cambio, e la prova non copre i
successivi periodi, segnali deboli o tutti gli allineamenti. Rimane una
variante di laboratorio, non una modifica di produzione. Log
`ft8-partial-busy-keep-partial.log`, catture `ft8-switch-capture-keep-partial`.

Con quattro ripetizioni della registrazione dopo il cambio, una prova della
variante ha restituito 7 righe nel parziale, poi 8/7/7 negli slot completi; una
prova separata del comportamento normale ha restituito 8/7/7 negli slot completi
e non ha decodificato il parziale. I sette messaggi principali coincidono;
`W1ATV F6DKQ 73` compare nel primo slot completo e manca nei due successivi per
entrambe le prove. Non è una prova di assenza di duplicazioni interne né di
parità per ogni segnale: acquisizioni separate, una prova per configurazione e
un solo cambio. Log `ft8-partial-busy-four-slots.log` e
`ft8-partial-busy-four-slots-standard.log`. Variante non integrata.

Nel medesimo confronto di una sola coppia a quattro slot, la CPU complessiva
del processo durante circa 88,1 s è stata 10,54 s con il comportamento normale
e 10,41 s con la variante. La variante ha eseguito anche il decode parziale;
la differenza fra due sole misure non quantifica né esclude un costo aggiuntivo
e non dimostra un risparmio. Renderer software, audio virtuale, LOW CPU, CAT/TX
spenti. Le misure provengono dall'output del laboratorio, non dai log QML.

Il controllo `ft8_live_duplicate_audit.py` ha esaminato tutte le 30 e 34
istantanee QML dei due log, rispettivamente normale e sperimentale. Non ha
trovato righe con stesso messaggio e frequenza duplicate nello stesso periodo
in alcuna istantanea. Il campionamento ogni tre secondi può perdere righe
transitorie, non rileva duplicati a frequenze diverse e non verifica duplicati
interni o trasmissioni: la variante resta non integrata.

Una seconda prova sperimentale ha catturato il buffer parziale effettivamente
inviato al decoder: 167.424 campioni. Riaperto senza modifiche non produce
righe in entrambe le build. Anteponendo 12.576 campioni nulli per arrivare a
180.000, entrambe recuperano `CQ 9H1SR JM75` a 1300 Hz. Questo indica che,
per il buffer catturato, la posizione temporale può spiegare il mancato decode;
il padding stimato dalla sola lunghezza non è necessariamente l'allineamento
UTC corretto, soprattutto per campioni presi in prestito dopo il confine. Non
è stato modificato il percorso live. Risultati e hash in
`ft8-immediate-capture-pad-parity.json`, WAV in `ft8-immediate-capture`.

Una variante confinata alla build di laboratorio abbina il riarmo immediato a
un padding iniziale calcolato dalla lunghezza del buffer parziale. In una prova
virtuale a tre slot, dopo il cambio durante il decode ha recuperato il messaggio
già nel secondo slot e di nuovo nel terzo; il secondo buffer aveva 13.088
campioni di padding e il terzo 32. È una singola prova con un segnale forte:
la stessa regola è intervenuta anche sul buffer quasi completo successivo e
la lunghezza non prova l'allineamento UTC, specie in presenza di campioni
presi in prestito. Non dimostra assenza di decode persi o falsi, né un beneficio
CPU; la variante resta solo sperimentale. Log
`live-audio-decodium-ft8-immediate-arm-lab-visible-two-slots-active-switch.log`.

Rieseguiti dopo la build corrente i test `test_ft8_auto_tx_decode_gate`,
`test_ft2_legacy_cpu` e `test_ft4_decode_cancellation`: 3/3 superati. Coprono
rispettivamente il gate FT8, il caso FT2 legacy/LOW CPU e la cancellazione FT4;
non sono una suite completa di parità decode, avvio o ricezione RF. La verifica
`git diff --check` sui tre file della modifica Logbook è pulita; sull'intero
worktree segnala spazi/terminatori di riga in altri file modificati.

Un'ulteriore verifica delle finestre ha rieseguito `log_revision_runtime_lab.py`,
`waterfall_label_view_lab.py` e `decosync_window_visibility_lab.py`: tutti e
tre hanno superato rispettivamente aggiornamento Logbook, zoom/posizione/stile
delle etichette e chiusura/riapertura DecoSync con storico. L'ispezione dei
quattro sorgenti temporaneamente strumentati nei laboratori FT8 non ha trovato
le relative variabili o marcatori sperimentali; la build normale era stata
ricostruita dopo ciascun esperimento. Questi controlli non coprono ogni finestra
o piattaforma.

`log_search_debounce_lab.py` è stato rieseguito con la build corrente: la
sequenza rapida di ricerca produce un aggiornamento aggregato e quello
esplicito rimane immediato; entrambi i pannelli restituiscono i risultati
attesi. Il controllo su metadati del file usa gli stessi percorso, dimensione e
tempo di modifica della cache esistente; scritture esterne che conservino
identici dimensione e timestamp non sono distinguibili da tale controllo. La
notifica di completamento del caricamento asincrono richiede comunque un nuovo
aggiornamento della finestra quando visibile.

Il confronto A/B della finestra Decometer durante la ricezione usa la stessa
build, LOW CPU, waterfall visibile e audio virtuale; cambia soltanto il QML
del Decometer prima/dopo l'eliminazione dei repaint invariati. Con due prove
per variante, la CPU di processo è passata da 7,33/7,03 a 4,35/4,40 secondi
su 43,2 secondi in FT8 con due segnali, da 3,17/3,05 a 1,82/1,88 secondi
su 19,2 in FT4 con otto segnali e da 10,26/9,91 a 8,68/8,74 secondi su
16,9 in FT2 con otto segnali. In ciascun confronto sono state ricevute le
stesse righe attese. Un'ulteriore registrazione FT8 reale ha dato 4,75/4,70
contro 3,03/3,00 secondi di CPU su circa 28,1 secondi, con gli stessi otto
messaggi in tutte le prove. I risultati completi sono nei file
`decometer-rx-cpu-{ab,ft8-busy-ab,ft4-ab,ft2-ab}.json` sotto
`build-baseline/performance/results/`. Sono misure locali con Decometer
aperto, rendering software offscreen e telemetria inattiva; non dimostrano
un risparmio equivalente a finestra chiusa, su ogni computer o in RF reale.
