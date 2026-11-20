# OSD: prima misura su hardware meno recente

Misura locale su Intel Core i7-3520M (2 core fisici, 4 thread logici), Linux
x86-64, GCC 15.2, build `MinSizeRel` (`-Os`, architettura generica). Il
riferimento è il commit `2d43b6fc` della release 1.0.1003. La variante usa
generatori OSD compattati, evita una codifica ripetuta e, su GCC con `-Os`,
compila la sola ricerca OSD con `-O3`, senza `fast-math` o istruzioni
aggiuntive specifiche della CPU.

Su 600 vettori deterministici OSD (K=77, 87, 91; ordini 0–4 e maschere AP)
i byte dei messaggi, le codeword, gli errori e i bit delle distanze sono
identici. Tre esecuzioni alternate per versione, con compilazione e altri
benchmark sospesi, hanno dato mediane di **8,49 s** prima e **2,14 s** dopo.
La misura comprende l'avvio del processo e riguarda il banco OSD, non la
velocità complessiva dell'app. Altri 12 vettori con ordini 5 e 6 hanno dato
output identico; le rispettive durate non sono confrontabili perché eseguite
con carichi concorrenti differenti.

Sono stati confrontati anche due WAV FT8 presenti nel repository, sei slot
FT8 sintetici a diversi livelli di rumore, quattro WAV FT2 e tre FT4
sintetici. Non sono comparsi decode mancanti nel confronto, per messaggio e
frequenza arrotondata. Nei sei FT8 sintetici entrambe le build hanno
decodificato tre segnali; negli altri tre nessuna delle due ha prodotto un
decode. Le prove FT2 a banda standard hanno decodificato quattro segnali su
quattro per entrambe le build; FT4 tre su tre.

Un'ulteriore prova FT8 `deep` usa tre slot sintetici con cinque segnali
sovrapposti e una scadenza di decode richiesta di 8 s (il tempo effettivo può
superarla). In due esecuzioni, l'originale ha
prodotto rispettivamente 5, 0 e 1 decode; la variante ne ha prodotti 5, 5 e 5,
senza perdere quelli dell'originale. I messaggi aggiuntivi corrispondono ai
segnali introdotti nella simulazione. La scadenza influenza il risultato:
questo esperimento mostra il comportamento su tre slot di prova, non una
garanzia di guadagno o di identità su ogni banda reale.

Questi risultati non dimostrano equivalenza per tutti i segnali o le altre
piattaforme. La banda affollata reale, l'app completa, LOW CPU, i cambi
modo con acquisizione audio e il funzionamento con una radio reale richiedono
prove ulteriori. I WAV generati e i risultati di questa sessione sono nella
cartella locale `build-baseline/performance/`; le istruzioni per rigenerare
confronti e vettori sono in `decode_bench/README.md`.

Tre [WAV FT8 di riferimento del progetto WSJT](https://sourceforge.net/projects/wsjt/files/samples/FT8/)
sono stati confrontati a banda 200–4000 Hz, profilo `deep`, con scadenza
richiesta di 8 s. In entrambe le prove il file `170709_135615.wav` non ha
prodotto decode; `181201_180245.wav` ne ha prodotti 3 prima e 5 dopo;
`210703_133430.wav` ne ha prodotti 1 prima e rispettivamente 4 e 3 dopo.
Nessun messaggio presente nel riferimento è mancato nella variante. I numeri
dipendono dalla scadenza, dalla configurazione del banco e dalle registrazioni;
non stabiliscono un risultato per ogni slot o configurazione dell'app.

## Riferimento applicazione completa

L'AppImage ufficiale 1.0.1003, verificata con il checksum della release, è
stata eseguita su questo computer in Xvfb con rendering software, senza CAT
né monitor audio. Il laboratorio riproduce WAV locali e verifica il passaggio
FT8 → FT4 → FT8 e la ripresa della decodifica. In una singola esecuzione per
profilo entrambe le prove sono riuscite, inclusa la verifica dell'impostazione
LOW CPU all'avvio e al termine. Il picco RSS osservato è stato circa 543 MiB
in LOW CPU e 608 MiB nel profilo normale; il tempo CPU complessivo è stato
rispettivamente 14,35 e 15,47 secondi. La durata comprende avvio e attese
intenzionali del laboratorio: non è una misura della sola decodifica. Una
singola prova non permette di attribuire con sicurezza la differenza al
profilo. Log e risultati completi sono in `build-baseline/performance/results/`.

Questi valori sono un riferimento iniziale, non un confronto prima/dopo:
servono prove ripetute con la build modificata, tenendo conto delle differenze
tra AppImage ufficiale e build locale. Il rendering virtuale e i WAV non
sostituiscono una prova con interfaccia grafica e radio reali.

Una seconda esecuzione LOW CPU isolata ha completato la stessa prova con
14,55 s di CPU e picco RSS circa 574 MiB. Nel suo log, il caricamento del componente `Main.qml` ha richiesto circa 8,65 s in Xvfb; il laboratorio
modifica temporaneamente il QML e disattiva la relativa cache, quindi il dato
non rappresenta necessariamente l'avvio normale con cache calda. Il log completo
è `build-baseline/performance/results/appimage-lowcpu-output.log`.

Per verificare l'effetto della strumentazione, due ulteriori avvii della
AppImage hanno usato il QML originale, senza `DECODIUM_DISABLE_QML_CACHE`,
con la stessa cartella di impostazioni isolata, Xvfb e rendering software.
I log indicano rispettivamente 8,65 e 8,55 s per caricare il componente
`Main.qml`;
il costruttore del bridge ha richiesto 424 e 341 ms. Sono misure dello stesso
ambiente virtuale, non una previsione per l'interfaccia grafica reale; non
provano da sole quale parte della creazione QML sia responsabile del tempo.

Il valore originale stampato come «created» è calcolato prima di `createObject`:
misura il caricamento del componente, non la creazione della finestra. La
strumentazione locale ora riporta separatamente anche la durata di creazione
e configurazione della finestra.
Una prova aggiuntiva con la sola strumentazione del BootLoader applicata
all'AppImage ha separato 8,55 s di caricamento del componente da 715 ms per
creare e configurare la finestra; il bridge ha richiesto 339 ms. In questo
avvio la cache su disco QML era abilitata. Sono tempi indicativi di questa
macchina e dell'ambiente virtuale, non identificano ancora la causa specifica.

## Confronto locale dell'applicazione

È stata creata anche una build di confronto nella stessa directory e con lo
stesso collegamento della build modificata, sostituendo gli oggetti del bridge,
l'OSD WSPR e la libreria FT con quelli originali. Nel laboratorio a WAV,
LOW CPU e rendering software, una prova per versione ha completato la
sequenza FT8 → FT4 → FT8 e ripreso la decodifica. Originale: 15,72 s di CPU,
672,6 MiB di picco RSS; modificata: 16,21 s e 671,0 MiB. Il carico include
avvio, interfaccia e attese, mentre i WAV semplici non sollecitano il decoder
come i casi deboli e affollati: questi numeri non dimostrano una riduzione del
carico complessivo o della memoria. Servono ripetizioni e scenari più
rappresentativi. Il QML utilizzato dalle due build è lo stesso e include la
strumentazione dei tempi di avvio. Log e JSON sono nella cartella locale
`build-baseline/performance/results/local-paired-*`.

Anche una prova per versione con LOW CPU disabilitato ha verificato pulizia e
ripresa dopo il cambio modo: originale 17,36 s CPU e circa 652 MiB di picco
RSS, modificata 16,48 s e circa 676 MiB. Come sopra, si tratta di singole
esecuzioni con audio simulato e non dimostrano una differenza di prestazioni
generali. L'ordine delle due versioni è stato invertito rispetto alla prova
LOW CPU.

## Barra temporale in LOW CPU

La barra grafica di RX/TX aggiornava il proprio stato ogni 50 ms anche in
LOW CPU. Ora in quel profilo si aggiorna ogni 100 ms; il profilo normale
rimane a 50 ms. La posizione resta calcolata dall'ora UTC, mentre acquisizione,
decodifica e pianificazione TX rimangono nel backend. Le due prove QML con
WAV e cambio FT8 → FT4 → FT8 hanno verificato rispettivamente intervallo
100/50 ms e recupero della decodifica. Poiché il laboratorio disabilita il
monitoraggio audio, non misura il risparmio di CPU durante l'uso della barra.

È stata poi forzata l'attività della sola barra, con CAT e monitor audio
spenti, misurando il processo per circa 10 s dopo l'avvio. Con 50 ms sono
stati osservati 200 aggiornamenti e 2,70/2,36 s di CPU; con 100 ms, 100
aggiornamenti e 2,28/2,15 s. Le due coppie sono state eseguite in ordine
inverso. Il tempo comprende anche il resto del processo e del rendering
software; queste poche misure indicano un possibile risparmio nello scenario
simulato, non quantificano il beneficio durante la ricezione reale. Lo script
riproducibile è `tests/tools/timing_bar_runtime_bench.py`; forza l'attività
grafica e ripristina il QML della build al termine.

## Selettore città all'avvio

La costruzione dell'orologio non richiede più due richieste anticipate della
lista città: la lista viene generata quando si apre il selettore. La prova
QML ha verificato lista inizialmente vuota, apertura, elenco predefinito,
ricerca di Roma e ora UTC; una successiva prova LOW CPU ha verificato anche
la decodifica e il cambio FT8 → FT4 → FT8. Un singolo confronto in Xvfb con
cache QML disattivata non mostra un miglioramento dell'avvio: i tempi di
caricamento/creazione sono 8,40 s/706 ms prima e 8,70 s/775 ms dopo. Questa
variazione non consente di attribuire un effetto alla modifica; è documentata
come eliminazione di lavoro non necessario, non come accelerazione dimostrata.

## Ricerca fusi orari

La ricerca ora confronta i nomi prima di creare gli oggetti dei fusi orari;
le zone non corrispondenti non vengono più caricate inutilmente. Il confronto
ha prodotto JSON identico per 11 ricerche, tra cui stringa vuota, maiuscole,
alias, accenti e nessun risultato, ciascuna con cinque limiti. In un
microbenchmark di 100 ricerche su questa macchina, due prove alternate hanno
richiesto 1646/1715 ms prima e 505/406 ms dopo; non è un tempo di avvio né
una misura del programma completo. `tests/world_clock_performance_probe.cpp`
e gli eseguibili/dati in `build-baseline/performance/world-clock-*` documentano
la prova. La build completa è stata ricompilata e il test del selettore e
quello LOW CPU con cambio modo e decode sono passati.

## Parità FT8 nell'app completa con LOW CPU

Due WAV FT8 di riferimento WSJT sono stati aperti nella build locale
originale e nella build modificata, con LOW CPU, stesso QML, CAT e monitor
spenti, una nuova istanza isolata per ogni prova. Dopo il segnale di fine
decodifica e un'attesa di 800 ms sono stati confrontati messaggio normalizzato
e frequenza, con molteplicità: `181201_180245.wav` ha restituito 7 decode in
entrambe, `210703_133430.wav` 13 in entrambe; nessun decode mancante o
aggiunto nei due confronti. L'ordine di esecuzione è stato invertito tra file.
Le durate includono l'attesa e non sono un benchmark del decoder. I risultati
sono in `build-baseline/performance/results/app-lowcpu-reference-parity.json`;
lo strumento è `tests/tools/bridge_wav_parity_lab.py`. Due registrazioni e un
profilo non garantiscono la parità per tutti i segnali o per la radio reale.

Tre WAV FT8 sintetici deboli sono stati confrontati anche attraverso l'app
in LOW CPU. Nel caso a SNR nominale −18 dB entrambe le build hanno mostrato
`K1ABC W9XYZ EN37` a 1501 Hz; nei casi a −21 e −24 dB nessuna ha mostrato
decode. Una ripetizione a −21 dB ha confermato il risultato. La prova
separata del decoder rilevava quel campione a −21 dB, ma usava banda
1200–1800 Hz e opzioni differenti; i log dell'app riportano banda 300–2700 Hz,
profondità effettiva 2 e configurazione adattiva LOW CPU. Non è quindi un
confronto equivalente e non identifica da solo la causa. Nei log dell'app
il worker segnala `nout=1` per il caso −21 dB, mentre la lista mostrata è
vuota: occorre esaminare il percorso di validazione/presentazione prima di
trarre conclusioni. Risultati e log sono in
`build-baseline/performance/results/app-lowcpu-weak-*.json` e
`build-baseline/performance/results/weak-app-logs/`.

L'indagine successiva ha chiarito il caso −21 dB: il worker di entrambe le
build decodificava `K1ABC W9XYZ EN37`, ma il filtro ghost lo nascondeva.
Il laboratorio precedente aveva lasciato il locator predefinito `AA00`:
la distanza verso `EN37` era circa 15.285 km, oltre la soglia del filtro per
segnali a −19 dB o inferiori. Disabilitando il filtro solo nel laboratorio,
entrambe le build mostrano il messaggio. Ripetendo con il locator di prova
esplicito `JM75` e il filtro attivo, entrambe lo mostrano. Il laboratorio
ora accetta `--grid` e usa `JM75` per default; non modifica le impostazioni
dell'utente. La regola geografica può comunque nascondere stazioni reali
lontane anche con un locator corretto: questo caso non misura il tasso di
falsi positivi e non basta a decidere una modifica al filtro.

## FT4 e FT2 nell'app con LOW CPU

Il laboratorio di confronto accetta ora anche `--mode FT4` e `--mode FT2`.
In due istanze isolate per modo, la versione originale e quella modificata
hanno decodificato lo stesso messaggio sintetico `CQ K1ABC FN42` a 1000 Hz:
un risultato per build in FT4 e uno per build in FT2. I log confermano LOW CPU,
profondità effettiva 2 e due thread richiesti per i campioni provati. I file
sono sintetici e la prova non copre tutti i livelli di segnale o la radio;
JSON e log si trovano in `build-baseline/performance/results/app-lowcpu-ft[24]-*`.

Un'ulteriore coppia di campioni sintetici, con SNR nominale −12 dB, è stata
confrontata nell'app in LOW CPU. In FT4 entrambe le build hanno mostrato
`CQ K1ABC FN42` a 1000 Hz, con SNR visualizzato −10 dB; in FT2 lo stesso
messaggio a 999 Hz, con SNR visualizzato −12 dB. La differenza fra SNR
nominale del generatore e valore mostrato è riportata esplicitamente. Non
sono comparsi decode mancanti nei due confronti. I risultati non stabiliscono
la sensibilità generale; JSON e log sono in
`build-baseline/performance/results/app-lowcpu-FT[24]-weak-*`.
