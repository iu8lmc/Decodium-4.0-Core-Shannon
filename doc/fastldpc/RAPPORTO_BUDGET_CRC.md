# Il budget della CRC

**Italiano** · [English](CRC_BUDGET_REPORT.md) · [Español](INFORME_PRESUPUESTO_CRC.md)

> Misure sul decodificatore LDPC(174,91) di FT2 e FT8: perché ad allargare la
> ricerca non si guadagna, e che cosa si guadagna invece rafforzando il test di
> accettazione.
>
> *Autore: **IU8LMC**. Implementazione e misure svolte con l'assistenza di
> Claude (Anthropic) su indicazione dell'autore. GPL-3.0 — 29 agosto 2026.*

---

## 00 · In breve

> Il decodificatore di FT8 e FT2 non è limitato da quanto cerca, ma da come
> accetta. La CRC-14 lascia passare un candidato sbagliato ogni 16 384:
> allargare la ricerca compra candidati giusti e falsi nella stessa proporzione,
> e non paga. Aggiungendo al test di accettazione due bit di struttura del
> messaggio, i nominativi fantasma si dimezzano a decodifiche invariate.

Questo rapporto raccoglie quello che è stato misurato, comprese **tre volte in
cui la misura ha smentito la previsione**. Le smentite sono riportate per
intero: sono la parte più utile, perché ognuna avrebbe portato in banda un
peggioramento presentato come miglioramento.

Tutto è riproducibile. Il codice è header-only sotto GPL-3.0 dentro
`Detector/fastldpc/`, i banchi di misura in `lab/cpp/`, e ogni numero qui
riportato viene da un comando che si può rilanciare.

---

## 01 · Il contesto in tre paragrafi

FT8, FT4 e FT2 usano lo stesso codice a correzione d'errore: l'LDPC(174,91) del
protocollo FT8, con 77 bit di messaggio più 14 di CRC nei 91 bit d'informazione
e 83 di parità. FT2 è un modo a ciclo breve, 3,75 secondi, dove il tempo di
calcolo per ciclo è stretto.

Vale la pena separare subito quattro livelli, perché nel seguito solo l'ultimo è
opera di questo lavoro. La **classe dei codici LDPC** è di Robert Gallager,
1962, riscoperta da MacKay e Neal negli anni Novanta. Gli **algoritmi di
decodifica** — min-sum normalizzato, ordered statistics decoding — sono
letteratura consolidata, di Chen e Fossorier il primo, di Fossorier e Lin il
secondo. Il **codice specifico** (174,91), cioè quelle 83 righe di parità, e la
CRC-14 con polinomio `0x2757` appartengono al protocollo FT8, progettato da
Steve Franke K9AN e Joe Taylor K1JT e pubblicato su QEX. Il **decodificatore**
descritto qui è invece scritto da zero, e usa quel codice e quella CRC senza
modifiche, per scelta: cambiarli romperebbe la compatibilità bit-a-bit con le
altre stazioni.

Il decodificatore lavora in due stadi. Il primo è un *min-sum* normalizzato e
stratificato, che chiude la maggior parte delle parole a buon segnale. Il
secondo è un OSD, *ordered statistics decoding*, che rimette in gioco solo le
parole che il primo non ha chiuso: rimescola le colonne mettendo davanti i bit
meno affidabili, risolve, e prova un certo numero di varianti attorno alla
soluzione. La quantità che decide quanto costa e quanto rende il secondo stadio
è **il numero di candidati che prova**. Ed è lì che sta il risultato.

---

## 02 · Velocità: min-sum vettoriale e decodifica a blocco

Il primo stadio è stato riscritto con intrinseche AVX2, sedici parole per
registro a interi da 16 bit, in virgola fissa Q=1/8 con saturazione. Le parole
già convergenti escono dal ciclo. Il secondo stadio usa eliminazione di Gauss
senza salti condizionati su righe da 256 bit, potatura per limite inferiore,
ordinamento radix e sindrome CRC incrementale.

Il guadagno che rende il resto possibile è quello del primo stadio: da **139,8 a
4,7 microsecondi** per parola, ventinove volte e mezzo. Non è un fine in sé —
serve a rendere praticabile un ordine OSD più alto, ed è da lì che viene la
sensibilità.

LDPC(174,91) su AWGN/BPSK · 20 000 parole per punto · un thread ·
Ryzen Zen 3, gcc 15.2, `-O3 -march=native`:

| Eb/N0 | FER veloce | µs | FER conservativo | µs | FER sensibile | µs |
|---:|---:|---:|---:|---:|---:|---:|
| 0,5 dB | 0,851 | 6,3 | 0,401 | 19,0 | **0,265** | 39,6 |
| 1,0 dB | 0,674 | 5,5 | 0,209 | 15,3 | **0,114** | 31,8 |
| 1,5 dB | 0,443 | 5,5 | 0,082 | 12,2 | **0,036** | 21,8 |
| 2,0 dB | 0,223 | 5,3 | 0,024 | 8,9 | **0,0082** | 14,3 |
| 2,5 dB | 0,083 | 4,6 | 0,0050 | 6,1 | **0,0018** | 7,5 |
| 3,0 dB | 0,021 | 2,9 | 0,00095 | 3,2 | **0,00010** | 4,1 |

Rispetto alla configurazione di partenza il guadagno di sensibilità è di
**+0,35 dB**, e di **+1,3 dB** rispetto al solo min-sum, a parità di false
decodifiche, con la catena sedici volte più veloce a 2 dB.

### Atteso contro misurato — la decodifica a blocco

| Previsione | Misura |
|---|---|
| Sei volte più veloce: il min-sum passa da una parola per corsia a sedici. | **1,8 volte.** Il conto teneva solo il min-sum; l'OSD resta per parola e diventa la quota dominante. |

Il guadagno c'è ed è reale, ma un terzo di quello annunciato. La previsione era
sbagliata perché ottimizzava mentalmente la parte già veloce.

---

## 03 · FT8: lo stesso decodificatore, lo stesso guadagno

FT8 e FT2 condividono il codice, quindi condividono il decodificatore. In
Decodium 4 il percorso FT8 usa `fastldpc` comprensivo della decodifica a blocco
delle passate, con un interruttore d'ambiente
(`DECODIUM_FT8_FASTLDPC=0`) per tornare al decodificatore originale e un
meccanismo di recupero che, per un numero limitato di candidati per ciclo,
riprova con quello classico.

Una differenza rispetto a FT2 va sottolineata perché è sostanziale: il filtro di
plausibilità descritto più avanti gira in FT8 con **tutti i tipi di messaggio
ammessi**. I formati da contest che in FT2 non si vedono, in FT8 esistono
davvero, e filtrarli renderebbe il decodificatore cieco proprio nelle giornate
di gara.

Stadio FT8 di produzione su uno slot generato con `ft8sim` a −18 dB,
profondità 3, due giri per configurazione:

| Decodificatore LDPC | Giro 1 | Giro 2 | Decodifiche |
|---|---:|---:|---:|
| Originale `ftx_decode174_91_c` | 71 648 ms | 71 373 ms | 3 |
| **`fastldpc`** | **9 316 ms** | **9 319 ms** | 3 |

**7,7 volte più veloce, a decodifiche identiche.** La ripetibilità è entro lo
0,4%, e le stesse tre righe con il nominativo escono da entrambe le
configurazioni: il guadagno è tutto tempo, nessuna sensibilità barattata.

Il numero va letto per quello che è. `ft8_stage_compare` esegue lo stadio di
produzione confrontando più configurazioni sullo stesso file, quindi è un carico
sbilanciato verso il decodificatore: è il rapporto giusto per la parte LDPC, non
il tempo dell'applicazione intera. Ed è comunque il pezzo che nei cicli
affollati decide se il ciclo chiude in tempo.

### La soglia in dB, che è la metrica vera

Il numero che conta davvero per FT8 non è il tempo ma la **soglia al 50%**: la
SNR a cui si decodifica metà dei segnali, con segnale piantato a SNR nota nella
banda di riferimento di 2500 Hz. Il conteggio totale dei decode non è una
metrica — è gonfiato dai segnali facili.

Misurata con `decode_bench/`, che genera i segnali con `ft8sim` di WSJT-X e
quindi ha verità di terra. Sette punti da −19 a −25 dB, 25 realizzazioni di
rumore ciascuno, profilo deep, messaggio `K1ABC W9XYZ EN37` a 1500 Hz:

| SNR | con `fastldpc` | decoder originale | `jt9` deep |
|---:|---:|---:|---:|
| −19 dB | 25/25 | 25/25 | 25/25 |
| −20 dB | 24/25 | 23/25 | 23/25 |
| −21 dB | **11/25** | 7/25 | 9/25 |
| −22 dB | **6/25** | 3/25 | 7/25 |
| −23 dB | 0/25 | 0/25 | 0/25 |

| | soglia al 50% |
|---|---:|
| Decodium con **`fastldpc`** | **−20,88 dB** |
| Decodium con decoder originale | −20,66 dB |
| `jt9` di WSJT-X, profilo deep | −20,75 dB |

**Il fattore 7,7 di velocità non costa sensibilità.** `fastldpc` risulta 0,22 dB
più sensibile del decodificatore originale e 0,13 dB più di `jt9`.

Sulla forza statistica va detto il vero: i due punti informativi sono −21 dB
(11/25 contro 7/25) e −22 dB (6/25 contro 3/25), ciascuno a circa 1,2 sigma, che
combinati fanno circa 1,7. Suggestivo, non conclusivo. **Quello che si può
affermare senza riserve è che fastldpc non costa sensibilità**; per stabilire il
+0,2 dB servirebbero un centinaio di realizzazioni per punto invece di
venticinque.

### Il controllo: non è la scadenza

Il banco impone una scadenza per decodifica, e il decodificatore originale è 7,7
volte più lento: il sospetto ovvio è che il divario non sia qualità ma tempo
scaduto. È un'ipotesi che si verifica, e va verificata, perché cambia
completamente che cosa si sta misurando.

Due punti (−21 e −22 dB), 40 realizzazioni, profilo deep:

| | soglia | −21 dB | −22 dB | tempo totale |
|---|---:|---:|---:|---:|
| `fastldpc`, scadenza 8 s | **−21,29 dB** | 24/40 | 10/40 | 547 s |
| originale, scadenza 8 s | −21,00 dB | 20/40 | 5/40 | 647 s |
| originale, scadenza **40 s** | −21,05 dB | 21/40 | 1/40 | **3208 s** |

**Dando al decodificatore originale cinque volte più tempo non cambia niente**:
−21,05 contro −21,00. La scadenza non era il vincolo, e l'ipotesi era sbagliata.
Il divario è qualità del decodificatore, non tempo esaurito — coerente con la
catena descritta sopra: la velocità non regala decibel da sola, permette di
*permettersi* un ordine di ricerca più alto, e sono quelli a darli.

Due osservazioni sulla solidità. Questa corsa dà un divario di 0,29 dB, la
precedente 0,22: due campioni indipendenti concordi in direzione e ampiezza, che
insieme portano il segnale a circa 2,4 sigma. E la soglia assoluta oscilla di
0,4 dB fra due corse della stessa configurazione (−20,88 e −21,29), il che
ricorda quanto poco pesino venticinque o quaranta realizzazioni per punto: sono
i confronti appaiati a reggere, non i valori assoluti.

C'è infine un dettaglio che vale la pena notare senza forzarlo: a −22 dB il
decodificatore originale con più tempo fa **peggio**, 1/40 contro 5/40. I numeri
sono piccoli e la differenza sta a 1,7 sigma, quindi non se ne può concludere
molto — ma la direzione è esattamente quella della tesi di questo rapporto: più
tempo significa più candidati provati, e più candidati significa più falsi
positivi della CRC che soffocano quello giusto.

Vale anche la lettura opposta, ed è la più utile: Decodium sta **alla pari con
`jt9` in profilo deep**. I decibel, su FT8, non stanno più nel decodificatore.

---

## 04 · Quanti decibel restano nel decodificatore

Ottimizzare un decodificatore assume implicitamente che cercando meglio si
decodifichi di più. Non è detto, ed è una cosa che si può misurare invece di
supporre.

Un fallimento ha due cause opposte. O la parola vera era *più verosimile* di
quella scelta e il decodificatore non l'ha trovata — allora cercare di più paga.
Oppure la parola vera era *meno verosimile* di un'altra parola di codice valida:
lì ha sbagliato la massima verosimiglianza stessa, nessun decodificatore può
fare meglio, e i decibel vanno cercati nel demodulatore o nel sync.

Su canale AWGN la verosimiglianza di una parola è la somma dei valori assoluti
degli LLR nei bit in cui contraddice la decisione hard. Confrontando quella
della parola vera con quella scelta, a Eb/N0 = 1 dB su 5000 parole, con la
configurazione quasi ottima (ordine 3, span 91/48, gate spento):

| Causa | Casi | Quota |
|---|---:|---:|
| Limite del **codice** — la parola vera era meno verosimile | 5 | 1,1% |
| Limite di **ricerca** — parola sbagliata accettata | 370 | 79,4% |
| Limite di **ricerca** — nessuna parola trovata | 91 | 19,5% |

Il **98,9% dei fallimenti è un limite di ricerca**. Sulla carta c'era molto da
prendere: allargando l'OSD si passava dall'84,96% al 90,68% di decodifiche
corrette, a 1,7 volte il costo.

La stessa misura, applicata a un codice LDPC quantistico in regime di *code
capacity*, dà la risposta opposta: **zero fallimenti recuperabili**, cioè un
decodificatore già al soffitto della decodifica a peso minimo. È un criterio
semplice per decidere se conviene lavorare sul decodificatore o smettere, e vale
la pena eseguirlo *prima* di qualunque ottimizzazione.

---

## 05 · Il risultato: è il test di accettazione a limitare

I 5,7 punti percentuali di decodifiche in più non sono incassabili così, perché
arrivano insieme a un mucchio di false decodifiche. La domanda giusta non è
quanti candidati si provano, ma quanti se ne accettano per sbaglio.

Il solo test che decide se un candidato è valido è la CRC-14, che ammette un
candidato sbagliato ogni 2¹⁴ = 16 384. La ricerca stretta prova circa 600
candidati per parola, quella larga circa 21 400. Ne segue un numero atteso di
falsi positivi di CRC per parola di **0,04 contro 1,3**.

> Allargare la ricerca compra candidati giusti e falsi nella stessa proporzione.
> Non è la ricerca il collo di bottiglia: è il test di accettazione.

La verifica sta nel mettere le due larghezze sulla stessa curva, spazzando la
soglia del gate anti-fantasma, con 20 000 parole a segnale e 100 000 candidati
di puro rumore. Il confronto va fatto **a parità di fantasmi**, non a parità di
soglia — la soglia non è la grandezza che interessa a nessuno.

Eb/N0 = 1 dB:

| Configurazione | Soglia | Decodifiche | Fantasmi |
|---|---:|---:|---:|
| **Stretta, senza filtro** *(esercizio)* | 0,065 | **16 168** | **10** |
| Stretta, senza filtro | 0,070 | 16 718 | 43 |
| Stretta, senza filtro | 0,075 | 16 924 | 141 |
| Larga, senza filtro | 0,065 | 16 810 | 20 |
| Larga, senza filtro | 0,070 | 17 564 | 81 |
| Larga, senza filtro | 0,075 | 17 912 | 311 |

A parità di fantasmi le due larghezze si equivalgono, e sotto il pareggio la
stretta è migliore. Il punto di lavoro di esercizio sta proprio là sotto.
**Allargare la ricerca, da sola, non conviene.**

---

## 06 · Due bit di struttura del messaggio

Se il vincolo è il test di accettazione, si rafforza il test. E l'informazione
per farlo c'è già: i 77 bit del payload non sono un numero qualunque, sono un
messaggio con una struttura. Un payload sorteggiato a caso quasi sempre non
descrive nominativi possibili.

Il controllo verifica soltanto vincoli strutturali certi:

- che il tipo di messaggio `i3` sia fra quelli definiti e ammessi, e per `i3=0`
  anche il sottotipo `n3`;
- che i campi a lunghezza limitata stiano nel loro intervallo — il testo libero
  è 42¹³ dentro 71 bit, il nominativo non standard 38¹¹ dentro 58, lo scambio
  ARRL 1..8000 oppure un moltiplicatore valido;
- che i nominativi abbiano struttura possibile: suffisso allineato a sinistra,
  almeno una lettera di suffisso, prefisso che non sia due cifre né una cifra
  sola;
- che un token (CQ, DE, QRZ) non compaia in seconda posizione.

Nessun controllo geografico o statistico: quelli rifiuterebbero collegamenti
veri.

Sta **dentro** il ciclo di accettazione dell'OSD, non dopo. La distinzione è
sostanziale: un candidato falso che passa la CRC ma non è un messaggio non ferma
l'enumerazione, che può ancora trovare quello giusto. Applicato dopo, si
limiterebbe a buttare via la parola lasciando il buco. Costa quasi niente perché
lo vedono solo i candidati che hanno già passato la CRC, cioè uno su 16 384.

Forza del filtro, su 2 milioni di payload sorteggiati a caso:

| Impostazione | Accettati | Bit di filtro | Fattore |
|---|---:|---:|---:|
| Tutti i tipi definiti *(usata in FT8)* | 0,573 | **0,80** | 1,74× |
| Soli tipi usati *(usata in FT2)* | 0,268 | **1,90** | 3,73× |

### Il guadagno, e la sua sicurezza

A parità di ricerca e di soglia, il filtro **dimezza i nominativi fantasma
lasciando le decodifiche identiche**. È la parte adottabile senza contropartite:
non cambia né ricerca né soglia, non può togliere decodifiche.

| Filtro | Decodifiche | Fantasmi / 100 000 |
|---|---:|---:|
| Nessuno | 16 168 | 10 |
| Tutti i tipi definiti | 16 168 | **7** |
| **Soli tipi usati** | **16 170** | **4** |

La sicurezza è verificata su due fronti. Su 20 000 messaggi realistici il filtro
non ne scarta nessuno. E su 404 nominativi presi dai log ADIF reali, dopo la
correzione qui sotto, nemmeno uno.

### Atteso contro misurato — la regola sul prefisso

| Regola scritta | Validata sui log |
|---|---|
| Prima della cifra del nominativo ci va una lettera. Sembra ovvia. | **Scarta 12 nominativi veri su 404**: S53MJ, S50XX, A61OK, S51RU, S56EPX, S51DM, Z31B, N25BRX, G56KAY, A65DF, Z62NS, E75AA. |

Sono prefissi lettera+cifra: **S5** Slovenia, **A6** Emirati, **Z3** Macedonia,
**E7** Bosnia, **Z6** Kosovo. Con quella regola quelle nazioni non sarebbero mai
più state decodificate. Il vincolo vero è più debole: un prefisso può essere
lettera, lettera+lettera, lettera+cifra o cifra+lettera, mai due cifre né una
cifra sola.

> Un filtro di plausibilità si valida contro dati reali, non contro il proprio
> ragionamento. Il banco che l'ha trovato è
> `lab/tools/valida_nominativi.py`, e va rieseguito ogni volta che si tocca la
> regola.

---

## 07 · Quando il laboratorio sbaglia e la banda corregge

Con due bit di filtro in più, la ricerca larga tornava conveniente sulla carta.
La misura in laboratorio dava, a parità di fantasmi, **+4,0% di decodifiche e
−40% di fantasmi insieme**: un miglioramento netto su entrambi gli assi, senza
compromessi. È stata portata in produzione.

### Atteso contro misurato — la ricerca larga in aria

| Laboratorio | In banda |
|---|---|
| Su rumore gaussiano sintetico: più decodifiche *e* meno fantasmi. Provata due volte. | **Ritirata due volte.** Sei minuti a zero fantasmi sembravano assolverla, ma su una banda senza traffico FT2 sei minuti non dimostrano niente: con più tempo i fantasmi sono tornati copiosi. |

Il rumore vero non è gaussiano bianco. Portanti, altri modi e QRM producono LLR
correlati su cui l'OSD si aggrappa, e una ricerca larga trova strutture dove il
modello sintetico non ne aveva. **Un banco su rumore sintetico può sovrastimare,
e in questo caso lo ha fatto.**

La configurazione in produzione resta a ricerca stretta con il filtro attivo: il
guadagno che sopravvive è il dimezzamento dei fantasmi.

---

## 08 · La digressione quantistica, e perché si ferma qui

Lo stesso nucleo — min-sum vettoriale più OSD su base più affidabile — è stato
portato a decodificare sindromi di codici LDPC quantistici: *bivariate bicycle*
di Bravyi et al., compreso il [[144,12,12]], sia in regime di code capacity sia
con rumore da circuito ricavato con `stim`.

Tre differenze rispetto al caso classico. La sindrome non è zero, e nel min-sum
diventa una riga: l'accumulatore dei segni parte da *s<sub>m</sub>* invece che da
zero. Non c'è nessuna CRC, e non serve, perché ogni candidato soddisfa la
sindrome per costruzione. Il successo non è ritrovare l'errore: un codice
quantistico è degenere, e la decodifica è corretta purché il residuo non
contenga operatori logici.

Il lavoro ha prodotto risultati verificabili — fra cui la dimostrazione, per
enumerazione esaustiva, che nessuno schedule uniforme a sei strati può estrarre
la sindrome dei codici BB in modo deterministico, mentre a sette ne esistono 236,
e quello adottato conserva la distanza del codice.

### Atteso contro misurato — il confronto con la letteratura

| Convinzione iniziale | Bibliografia |
|---|---|
| Terreno poco battuto, e da 100 a 600 volte più veloce del riferimento. | Campo affollato e in movimento. **La libreria usata come riferimento contiene già il decodificatore veloce**, *Localized Statistics Decoding*, nato proprio per questo problema: si era misurato contro quello lento. |

Il confronto è stato allora rifatto su **`sinter`**, il banco standard del
settore: batch, multi-processo, stessa infrastruttura per tutti i decoder, e
riproducibile da chiunque senza avere il nostro codice. Surface code d=5, 50 000
shot per punto, sei processi:

| p | fastldpc | BP+LSD | BP+OSD-7 | pymatching |
|---:|---:|---:|---:|---:|
| 0,001 | 3 err · 190 µs | 4 · 1107 µs | 1 · 1039 µs | 7 · 0,9 µs |
| 0,002 | **17** · 119 µs | 30 · 2001 µs | 29 · 2570 µs | 59 · 1,5 µs |
| 0,003 | **86** · 141 µs | 124 · 3268 µs | 97 · 4485 µs | 153 · 2,1 µs |

**Da 8 a 32 volte più veloce di BP+LSD e BP+OSD, con meno errori di entrambi**,
e il vantaggio di velocità è sottostimato: il nostro tempo include l'intero giro
— analisi del modello in Python, scrittura file, avvio processo — mentre gli
altri girano nello stesso processo.

Sull'accuratezza va detta la forza statistica: a p=0,002 il 17 contro 29 sta a
1,8 sigma, a p=0,003 l'86 contro 97 a 0,8. Presi uno per uno non sono
conclusivi; il fatto di stare sotto a tutti e tre i punti lo è di più. Alla pari
o leggermente meglio, con la velocità come vantaggio solido.

> **Il banco standard ha trovato subito un baco che tre giorni di misure interne
> non avevano visto.** La prima misura via sinter dava 768 errori dove il banco
> interno ne dava 98: fattore otto. sinter passa ai decoder il modello
> **scomposto**, dove un'istruzione è spezzata da `^`, e un osservabile presente
> in due componenti **si annulla** in GF(2). Raccogliendo i target in una lista
> invece che per parità, 86 istruzioni su 1953 venivano segnate come se
> ribaltassero l'osservabile. Un errore silenzioso: pesi, priori e massa di
> probabilità restavano identici. Si è trovato solo perché l'integrazione nuova
> non riproduceva un numero già noto — ed è quel controllo che andava fatto per
> primo.

La finestra scorrevole implementata per rendere lineare il costo nel numero di
round è tecnica standard, e la profondità 7 dello schedule era già nel lavoro di
Bravyi et al.: è stata ritrovata, non trovata.

---

## 09 · Che cosa è nuovo e che cosa no

**Non è nuovo il concetto.** Sfruttare la ridondanza residua della sorgente
dentro il decodificatore di canale è *source-controlled channel decoding*, un
filone che risale ad Hagenauer negli anni Novanta. E che il budget di falsi
positivi della CRC limiti l'ampiezza della lista è teoria dei codici standard,
ben documentata nella letteratura su CRC-aided list decoding di codici polari e
convoluzionali.

**È nuova l'applicazione, e soprattutto la misura.** Nella comunità FT8 e
WSJT-X non risulta che qualcuno abbia costruito la curva
decodifiche-contro-fantasmi e mostrato che a limitare è il test di accettazione
e non la ricerca; né il risultato operativo che ne segue, cioè il dimezzamento
dei fantasmi a decodifiche invariate. È anche nuovo, per quanto risulta, l'uso
della diagnosi «limite di ricerca o limite del codice» come criterio per decidere
*dove* spendere lavoro prima di spenderlo.

---

## 10 · Limiti dichiarati

- Le misure su FT2 usano LLR sintetici AWGN, non l'uscita del demodulatore
  4-GFSK reale. Il modello di rumore **ha già sovrastimato una volta**, come
  descritto alla sezione 07.
- I tempi sono su una macchina sola, un thread solo, un compilatore solo.
- La restrizione ai soli tipi di messaggio usati è una **politica**, non un test
  di formato: un messaggio di un tipo escluso non verrebbe mai decodificato. In
  FT8 non è applicata proprio per questo.
- Il confronto quantistico è ora su `sinter`, ma il vantaggio di accuratezza
  resta a 1-2 sigma: servono più shot per renderlo solido. Sui codici BB il
  confronto non è ancora stato rifatto sul banco standard.
- La soglia FT8 in dB è ora misurata (sezione 03), ma su 25 realizzazioni per
  punto: il vantaggio di 0,2 dB sta a 1,7 sigma e non è stabilito. Serve un
  campione quattro volte più grande.

---

## 11 · Riprodurre le misure

Ogni tabella di questo rapporto viene da un comando. I banchi sono header-only e
si compilano con `g++ -O3 -march=native -std=c++17`.

```
lab/cpp/ml_gap.cpp        limite di ricerca o limite del codice
lab/cpp/pareto.cpp        curva decodifiche contro fantasmi
lab/cpp/noise_test.cpp    accettazioni su puro rumore, con i tempi
lab/cpp/plausible.hpp     il controllo di struttura del messaggio
lab/tools/gen_test.py     parole di prova; --reali per messaggi veri
lab/tools/valida_nominativi.py
                          verifica il filtro sui nominativi dei log ADIF
decode_bench/             soglia FT8 in dB con verita' di terra
```

Un avvertimento che è costato tempo: i dati di prova generati **senza**
`--reali` contengono payload casuali, non messaggi. Misurare un filtro di
plausibilità su quelli lo fa sembrare distruttivo, perché scarta anche le parole
«giuste» — che messaggi non sono.

---

## 12 · Bibliografia

- R. G. Gallager, **«Low-Density Parity-Check Codes»**, MIT, 1962.
  L'origine della classe di codici su cui tutto questo poggia.
  *IRE Trans. Inf. Theory, IT-8, pp. 21–28.*
- S. Franke K9AN, B. Somerville AE6NZ, J. Taylor K1JT, **«The FT4 and FT8
  Communication Protocols»**, QEX. La specifica del codice LDPC(174,91) e della
  CRC-14 usati senza modifiche in questo lavoro.
  <https://wsjt.sourceforge.io/FT4_FT8_QEX.pdf>
- **Joint Source–Channel Decoding**, Wiley. Il filone del *source-controlled
  channel decoding*: sfruttare la ridondanza residua della sorgente dentro il
  decodificatore di canale.
  <https://onlinelibrary.wiley.com/doi/10.1002/9781118693803.ch10>
- **Design, Performance, and Complexity of CRC-Aided List Decoding of
  Convolutional and Polar Codes for Short Messages**. Il compromesso fra
  ampiezza della lista e probabilità di errore non rilevato.
  <https://arxiv.org/pdf/2302.07513>
- **Pre-configured Error Pattern Ordered Statistics Decoding for CRC-Polar
  Codes**. <https://arxiv.org/pdf/2309.11836>
- **Linear-Equation Ordered-Statistics Decoding**. Varianti a bassa complessità
  dell'OSD. <https://arxiv.org/pdf/2110.11574>
- **Localized statistics decoding for quantum low-density parity-check codes**,
  Nature Communications, 2025. Attacca il costo dell'eliminazione di Gauss
  dell'OSD; distribuito nella libreria `ldpc`.
  <https://arxiv.org/pdf/2406.18655>
- **Ambiguity Clustering: an accurate and efficient decoder for qLDPC codes**.
  <https://arxiv.org/pdf/2406.14527>
- **Fully Parallelized BP Decoding for Quantum LDPC Codes Can Outperform
  BP-OSD**. <https://arxiv.org/abs/2507.00254>
- **An almost-linear time decoding algorithm for quantum LDPC codes under
  circuit-level noise**. La decodifica a finestra scorrevole come tecnica
  consolidata. <https://arxiv.org/pdf/2409.01440>
- **BP+OSD**, la libreria di riferimento usata nei confronti.
  <https://github.com/quantumgizmos/bp_osd>

---

*Rapporto tecnico su Decodium 4.0 Core Shannon · `fastldpc` · GPL-3.0.*

**Attribuzione.** `fastldpc` è un decodificatore scritto ex novo per Decodium
4.0 Core Shannon. Implementa algoritmi noti — codici LDPC (Gallager, 1962),
min-sum normalizzato, ordered statistics decoding — con vettorizzazione AVX2 e
ottimizzazioni originali. La ricerca a coppie è modellata sui passi
`npre1`/`npre2` di `osd174_91` di WSJT-X. Opera sul codice LDPC(174,91) e sulla
CRC-14 (`0x2757`) del protocollo FT8, progettati da Steve Franke K9AN e Joe
Taylor K1JT, usati senza modifiche per garantire compatibilità bit-a-bit. Il
decodificatore originale resta disponibile e selezionabile.
