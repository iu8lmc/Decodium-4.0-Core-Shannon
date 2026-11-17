# superldpc — decoder SIMD a due stadi per LDPC(174,91) di FT2 e FT8

    stadio 1  min-sum normalizzato, layered, int16, AVX2 su x86 o NEON su ARM64
    stadio 2  OSD di ordine 0..3 SOLO sulle parole che lo stadio 1 non chiude
    gate      CRC-14 (poly 0x2757, identica a WSJT-X) + distanza soft normalizzata

Header-only, GPL-3.0, si integra in DECODIUM includendo `ft2_decoder.hpp`; il
programma lo chiama attraverso `decodium_bridge.cpp`, sia in FT2 sia in FT8.
Scritto ex novo: non e' un adattamento del decoder di WSJT-X (che in Decodium
esiste ancora, portato in C++, in `Detector/FtxLdpc.cpp`). Opera pero' sul
codice LDPC(174,91) e sulla CRC-14 del protocollo FT8, usati **senza
modifiche** (vedi "Provenienza e attribuzione" in fondo).

Rispetto alla prima versione del progetto: **+0,35 dB** rispetto alla
configurazione OSD-2/span32 di partenza e **+1,3 dB** rispetto al solo min-sum,
a parità di false decodifiche, con la catena **16x più veloce** a 2 dB. I due
risultati sono legati: il guadagno di sensibilità viene dall'usare un ordine OSD
molto più alto, cosa diventata praticabile solo perché il costo per candidato è
crollato.

**Come leggere i numeri di questo README.** Ci sono due famiglie di misure, e
non vanno mescolate:

- le tabelle di "Risultati misurati" e di "Da dove viene il guadagno"
  riguardano i preset della libreria autonoma (`veloce`, `conservativo`,
  `sensibile`) su AWGN/BPSK con LLR ideali. **Decodium non usa nessuno dei tre
  nel profilo di esercizio**;
- quello che il programma fa girare davvero, con i suoi numeri su metriche FSK,
  sta in "Cosa gira davvero in Decodium".

In tutte le tabelle **Eb/N0 è riferito ai 91 bit** che entrano nel codice LDPC
(77 di messaggio + 14 di CRC), cioè al tasso R = 91/174, non ai 77 bit del
messaggio.

## Risultati misurati (preset della libreria autonoma)

AWGN/BPSK, Eb/N0 per 91 bit (R = 91/174), 20 000 parole per punto, 1 thread,
batch 64, 30 iterazioni.
Ryzen Zen 3, gcc 15.2 (MSYS2), `-O3 -march=native`. FER = tasso di parole non
decodificate, fd = false decodifiche su 20 000.

| Eb/N0 | `veloce` FER | fd | µs | `conservativo` FER | fd | µs | `sensibile` FER | fd | µs |
|------:|-----:|--:|---:|-----:|--:|---:|-----:|--:|---:|
| 0.5 dB | 0.851 | 0 | 6.3 | 0.401 | 20 | 19.0 | **0.265** | 15 | 39.6 |
| 1.0 dB | 0.674 | 0 | 5.5 | 0.209 |  8 | 15.3 | **0.114** |  5 | 31.8 |
| 1.5 dB | 0.443 | 0 | 5.5 | 0.082 |  1 | 12.2 | **0.036** |  2 | 21.8 |
| 2.0 dB | 0.223 | 0 | 5.3 | 0.024 |  1 |  8.9 | **0.0082** |  0 | 14.3 |
| 2.5 dB | 0.083 | 0 | 4.6 | 0.0050 | 1 |  6.1 | **0.0018** |  0 |  7.5 |
| 3.0 dB | 0.021 | 0 | 2.9 | 0.00095 | 0 |  3.2 | **0.00010** | 0 |  4.1 |
| 3.5 dB | 0.0036 | 0 | 1.8 | 0.00010 | 0 | 1.9 | **0 su 20000** | 0 | 1.9 |

`veloce` = solo min-sum. `conservativo` = OSD-2 su span 32, il compromesso
classico tipo WSJT-X. `sensibile` = OSD-3, span2 91 e span3 48.

Eb/N0 necessario per raggiungere un dato FER, interpolando fra 14 punti
misurati fra 0,25 e 3,5 dB:

| FER | `veloce` | `conservativo` | `sensibile` | guadagno di `sensibile` |
|---|---:|---:|---:|---:|
| 0.20 | 2.06 dB | 1.03 dB | **0.68 dB** | +0.34 su conservativo, +1.38 su min-sum |
| 0.10 | 2.41 dB | 1.40 dB | **1.06 dB** | +0.34 / +1.36 |
| 0.05 | 2.70 dB | 1.70 dB | **1.36 dB** | +0.34 / +1.34 |
| 0.01 | 3.22 dB | 2.29 dB | **1.94 dB** | +0.36 / +1.28 |
| 0.001 | — | 2.98 dB | **2.61 dB** | +0.38 |

Il guadagno è **+0,35 dB costante** su tutta la curva rispetto a OSD-2/span32,
e **+1,3 dB** rispetto al solo min-sum. (Una stima precedente dava +0,6 dB su
OSD-2: era sbagliata, confrontava con la riga OSD-1 della prima tabella.)

Throughput (preset `sensibile`, 1 dB, OpenMP, Ryzen a 16 core):

| thread | 1 | 2 | 4 | 8 | 16 |
|---|---:|---:|---:|---:|---:|
| kparole/s | 32 | 62 | 124 | 241 | 337 |

337 000 parole/s sono **1,26 milioni di candidati per ciclo da 3,75 s**, con il
decoder più sensibile. Il FER è identico a ogni numero di thread: il decoder è
deterministico e ogni thread ha la sua istanza, senza stato condiviso.

### Quanto manca al limite: blocco finito, non Shannon

Il teorema di Shannon è asintotico: dice cosa si può ottenere con blocchi di
lunghezza arbitraria, e a 174 bit non è il termine di paragone giusto. Per
n = 174 il limite dell'approssimazione normale a FER 0,1 / 0,01 è
**1,02 / 1,77 dB per K = 91** e **-0,01 / 0,81 dB per K = 77**. Il ~1 dB fra i
due è il prezzo della rivelazione d'errore: i 14 bit di CRC viaggiano ma non
portano messaggio. Il preset `sensibile` sta a 1,06 e 1,94 dB (tabella sopra,
BPSK con LLR ideali), cioè 0,04 e 0,17 dB sopra la riga K = 91: lì non resta
quasi niente da prendere con il solo decoder. Quello che resta
sta nella differenza fra K = 91 e K = 77, cioè nell'usare la CRC come parte del
codice (sezione "Ricerca su (174,77) e gate a margine") senza pagarlo in false
decodifiche.

## Da dove viene il guadagno

### 1. Il min-sum non si vettorizzava (30x)

`decoder.hpp` (MinSumV2) organizza i dati perché il loop interno sul batch si
auto-vettorizzi. **Non funziona**: i kernel contengono min1/min2/argmin e
aggiornamenti condizionati, e sia gcc 15 sia clang 22 li rifiutano con
`unsupported control flow in loop` (`-fopt-info-vec-missed` lo mostra sulle
righe 76, 95 e 108). Misura a 2 dB, solo stadio 1:

| stadio 1 | µs/parola |
|---|---:|
| MinSumV2, gcc 15.2 `-O3 -march=native` | 139,8 |
| MinSumV2, clang 22.1 idem | 94,3 |
| **MinSumV3 (`minsum_avx2.hpp`), intrinsics AVX2** | **4,7** |

`MinSumV3` scrive le stesse operazioni a mano su 16 int16 per registro YMM,
tiene i messaggi di un check node nei registri e calcola la sindrome
vettorialmente. Include l'early-exit per gruppo di 16 parole.

### 2. La CRC-14 è lineare, quindi l'ordine OSD è quasi gratis

È il punto che sblocca tutto il resto. L'LFSR della CRC-14 parte da zero e non
ha XOR finale, quindi la sua sindrome è una **funzione lineare su GF(2)** dei
91 bit:

    crc14_syn(x ^ y) == crc14_syn(x) ^ crc14_syn(y)

Il contributo alla sindrome del flip di un bit d'informazione si può allora
precalcolare, e testare la CRC di un candidato costa **uno XOR su un uint16**
invece di ricostruire i 174 bit della parola e ripercorrere l'LFSR. I
contributi dei 91 bit si ricavano in blocco per bit-slicing: 14 accumulatori da
256 bit, uno per bit di CRC.

L'effetto è che il filtro più selettivo (la CRC accetta 1 candidato su 16 384)
diventa anche il più economico, e va messo per primo: score e costruzione della
parola si pagano solo sui pochissimi candidati che lo superano. Il costo
dell'OSD smette di dipendere dal numero di candidati:

| a 1 dB, µs per parola tentata | prima | dopo |
|---|---:|---:|
| OSD-0 (nessun candidato oltre il primo) | 34,4 | 15,2 |
| OSD-1 (91 candidati) | 95,9 | 15,3 |
| OSD-2 span 32 (~590 candidati) | 456 | 15,6 |
| OSD-2 span 91 (~4 100 candidati) | — | 16,9 |
| OSD-3 span3 48 (~21 000 candidati) | — | 31,2 |

Prima, passare da OSD-0 a OSD-2 costava 13x. Ora costa il 3 %. Da qui la scelta
di progetto: **usare ordini e span molto più alti di quanto sia sensato con
un'implementazione tradizionale**, che è esattamente da dove arrivano i
+0,35 dB su OSD-2/span32.

Contribuiscono, in ordine di peso: Gauss branchless su righe da 256 bit (una
riga di H è 174 bit, sta in un registro YMM), parità dei candidati come
bitmask, pruning per lower bound sullo score, `colmask` calcolate pigramente,
radix sort al posto di `std::sort`, e test CRC vettorizzato a 16 candidati per
volta. `cpp/verify.cpp` confronta il risultato con l'implementazione di
riferimento parola per parola: su 21 492 parole passate all'OSD, **zero
differenze**.

### 3. Il gate anti-false-decode è ciò che rende usabile l'ordine alto

Più candidati si provano, più falsi positivi passano la CRC. Senza gate, OSD-3
sarebbe inutilizzabile: a 0,5 dB produce **3542 false decodifiche su 20 000**
(17,7 %). Il gate misura quanto il candidato accettato smentisce il canale:

    nd = somma |llr_ch[v]| sui v dove il candidato differisce dalla
         decisione hard di canale   /   somma |llr_ch[v]| su tutti i v

`nd` è adimensionale, quindi la stessa soglia vale a ogni SNR (gli LLR scalano
con 1/σ²). Si misura sugli **LLR di canale**, non sui posterior del min-sum,
che sono saturati a ±2047 e non conservano il rapporto con il rumore.
Distribuzione misurata sui candidati OSD (gate spento):

| Eb/N0 | nd corrette p99,9 | nd corrette max | nd false min | nd false p50 |
|------:|-----:|-----:|-----:|-----:|
| 0.5 dB | 0.083 | 0.087 | 0.057 | 0.106 |
| 1.0 dB | 0.078 | 0.083 | 0.057 | 0.108 |
| 2.0 dB | 0.070 | 0.074 | 0.079 | 0.115 |
| 3.0 dB | 0.062 | 0.062 | 0.127 | 0.127 |

Le due popolazioni si sovrappongono appena. Effetto della soglia su OSD-3
span3=48 a 0,5 dB:

| nd_max | 1.0 (spento) | 0.085 | 0.080 | 0.075 | 0.070 |
|---|---:|---:|---:|---:|---:|
| false | 3542 | 114 | 51 | **17** | 8 |
| FER | 0.2446 | 0.2461 | 0.2504 | **0.2627** | 0.2948 |

Più candidati si provano, più stretta va la soglia: `conservativo` usa 0,085,
`sensibile` 0,075. Le parole chiuse dal solo BP+CRC non hanno mai prodotto false
decodifiche in nessun punto — devono soddisfare tutte le 83 equazioni di parità
— quindi il gate si applica ai soli candidati OSD.

Ritarare: `make gate` scrive `data/nd*.csv` (colonne `src,ok,nd`).

### 4. Robustezza: cosa succede se gli LLR non sono quelli ideali

Tutte le misure sopra sono su AWGN/BPSK con LLR perfettamente calibrati. Il
demodulatore reale non li produce così, quindi conviene sapere a cosa la catena
è sensibile e a cosa no. (Le misure sulle metriche FSK sono più sotto, in "Cosa
gira davvero in Decodium".)

**Scala degli LLR: irrilevante.** Moltiplicando tutti gli LLR per un fattore da
0,25 a 8 (32x), a 1 dB il FER passa da 0,1109 a 0,1185 e le false restano 5-9.
Non è fortuna: `nd` è un rapporto, quindi è *esattamente* invariante di scala, e
il min-sum lavora su minimi e segni — a differenza del sum-product non ha
bisogno di LLR calibrati. Un demodulatore che sbaglia la stima di sigma non è
un problema.

**Interferenza impulsiva: molto sensibile.** Sostituendo una frazione di LLR con
un valore grande di segno casuale (un LLR *sicuro e sbagliato*, cioè quello che
produce un static crash o del QRM), a 2 dB col preset `sensibile`:

| frazione di bit impulsivi | 0 | 0,5 % | 1 % | 2 % | 5 % |
|---|---:|---:|---:|---:|---:|
| FER, senza clipping | 0.0081 | 0.0766 | 0.183 | 0.414 | 0.868 |
| false, senza clipping | 0 | 5 | 7 | 29 | **91** |

Lo 0,5 % è meno di un bit su 174, e già moltiplica il FER per 9,5. Peggio: da
circa il 5 % in su il preset `sensibile` produce **più** false del
`conservativo` (91 contro 70). Ha senso — provare più candidati è un vantaggio
solo finché il canale non mente; quando mente, aumenta solo le occasioni di
sbagliare.

**Mitigazione: `llr_clip`.** Un |LLR| molto più grande della media della parola
è quasi sempre un artefatto, non informazione, e un solo LLR sicuro e sbagliato
avvelena tutti i check che lo toccano. Limitarlo a `k` volte la media della
parola (relativa, quindi invariante di scala come `nd`) recupera parecchio:

| a 2 dB, impulsi 1 % | k=0 (spento) | k=2,5 (default) | k=2 |
|---|---:|---:|---:|
| FER | 0.183 | 0.125 | **0.090** |
| false | 7 | 2 | **2** |
| costo su canale pulito (FER a 1 dB) | 0.1126 | 0.1136 (+0,9 %) | 0.1186 (+5,3 %) |

Il default è **k = 2,5**: costo trascurabile su canale pulito, un terzo del
danno da impulsi tolto. **k = 2** dimezza il danno da impulsi e taglia le false
dell'86 % nel caso peggiore, ma costa il 5 % di FER su canale pulito. La scelta
fra i due è stata rifatta sulle metriche FSK con il preset di esercizio
("Taglio degli LLR sulle metriche FSK", più sotto): **resta 2,5**.

### Ipotesi a priori (AP) e innesto in Decodium

FT2 non decodifica alla cieca: fa piu' passate in cui alcuni bit del messaggio
sono gia' noti (il proprio nominativo, quello del corrispondente, CQ), e li
passa al decoder come `apmask` con gli LLR gia' portati al valore giusto e
magnitudine grande. Senza supportarlo, superldpc non e' utilizzabile li'.

`decode_batch` accetta ora una `apmask` opzionale: i bit noti vengono saturati
al massimo rappresentabile, cosi' il min-sum non li ribalta e l'OSD, che ordina
per affidabilita', li lascia in fondo al set d'informazione e non li flippa.

**La soglia del gate va pero' adattata, e non di poco.** Con K bit noti lo
spazio dei candidati compatibili si riduce di 2^K, quindi i falsi positivi
crollano; e allo stesso tempo l'AP fa decodificare parole con piu' errori di
canale, che hanno `nd` intrinsecamente piu' alto. Tenendo la soglia tarata
senza AP si buttano via decodifiche buone. Il fattore misurato e'
**(N / N_liberi)²**, e predice il ginocchio della curva in tutti i punti
provati (1 dB, 2000 parole, soglia oltre la quale compaiono false):

| bit noti | liberi | (N/N_liberi)² × 0,075 | ginocchio misurato | corrette a quella soglia |
|---:|---:|---:|---:|---:|
| 0  | 174 | 0.075 | 0.075 | 1791 |
| 14 | 160 | 0.089 | 0.090 | 1937 |
| 29 | 145 | 0.108 | 0.110 | 1992 |
| 58 | 116 | 0.169 | ≥0.130 | 2000 |

Con 29 bit noti e la soglia fissa a 0,075 si decodificano 1892 parole su 2000;
con la soglia adattiva, 1992. Il gate si regola da solo: basta passare
`apmask`.

### Innesto: `decodium_bridge.cpp`

Espone `superldpc_decode174_91_c` con la **stessa firma** di
`ftx_decode174_91_c` di Decodium 4, conversione di segno inclusa (li' LLR
positivo = bit 1, qui = bit 0), e la variante a blocchi
`superldpc_simd_decode174_91_batch_c`, che e' quella che chiamano FT2
(`Detector/FtxFt2Stage7.cpp`) e FT8 (`Detector/FtxFt8Stage4.cpp`). Un'istanza
per thread e per modo; `FtxLdpc.cpp` resta al suo posto, serve ancora per
l'encoder, le tabelle e le CRC.

Misura a 1 dB, 1000 parole, contro il decoder di Decodium (con il fix del segno
del min-sum gia' applicato — vedi sotto). E' la misura fatta al momento
dell'innesto, su AWGN/BPSK e con il preset `sensibile`: non e' la
configurazione di esercizio di oggi, descritta nella sezione che segue.

| bit noti | Decodium | superldpc | false | velocita' |
|---:|---:|---:|---:|---:|
| 0  |  834 | **893** | 0 | 718x |
| 14 |  952 | **977** | 0 | 270x |
| 29 |  994 | **997** | 0 |  71x |
| 58 | 1000 | 1000    | 0 |   3x |

Il vantaggio si assottiglia man mano che l'AP fa il lavoro al posto del
decoder: con mezzo messaggio noto il problema e' facile per entrambi. Resta il
tempo, ed e' li' che cambia il quadro: una passata FT2 prova fino a 300
candidati (`kFt2MaxCand`), e a 60 ms l'uno non si sta in un ciclo da 3,75 s.

**Nota sul batch.** Il drop-in riceve una parola per chiamata e il min-sum ne
lavora 16 per registro: 15 corsie su 16 vanno sprecate. Se il chiamante puo'
raccogliere i candidati e passarli insieme a `decode_batch`, il costo per
parola scende di circa un ordine di grandezza (da ~85 µs a ~14). E' la
modifica che vale di piu' dopo l'innesto.

### Cosa gira davvero in Decodium (preset del bridge)

I tre preset delle tabelle in cima **non sono** quello che il programma usa
nel profilo di esercizio. `decodium_bridge.cpp` sceglie la configurazione
dall'indice di preset `norder` che riceve dal chiamante (non e' l'ordine
dell'OSD):

| `norder` | chi lo usa | configurazione |
|---|---|---|
| <= 3 | profilo di esercizio di FT2 e FT8 | **preset di esercizio**, descritto qui sotto |
| 4-5 | profili profondi di FT8 (`--norder 4`) | `conservativo` (OSD-2, span 32, `nd_max` 0,085) con alpha, iterazioni e gate del bridge |
| >= 6 | — | `sensibile` (OSD-3, span2 91, span3 48, `nd_max` 0,075) con alpha, iterazioni e gate del bridge |

Preset di esercizio, uguale nei due modi salvo dove indicato:

| voce | valore |
|---|---|
| min-sum | 10 iterazioni, alpha 0,578 (37888/65536), una parola per chiamata in un batch da 16 |
| OSD | ordine 2, span2 64, nessuna terna, ricerca a coppie con chiave di 13 bit sui primi 64 bit |
| `llr_clip` | 2,5 |
| accettazione dei candidati OSD | FT2: soglia sulla distanza, `nd_max` 0,065. FT8: gate appreso (l'OSD accetta fino a `nd` 0,25 e decide il classificatore) |
| bit ribaltati ammessi | FT8: 58. FT2: 22 (58 quando tutti i 77 bit sono imposti dall'AP) |
| plausibilita' del messaggio | FT8: tutti i tipi definiti. FT2: solo i tipi usati, dopo aver tolto lo scrambling |
| coerenza con i bit AP | accesa |
| ricerca su (174,77), gate a margine | spenti |

**Misure del preset di esercizio.** Banco `tests/tools/superldpc_k77_bench.cpp`
con `chan=chain`: le parole passano da `superldpc_simd_decode174_91_batch_c`,
la funzione che chiama il programma, e gli LLR non sono BPSK ma le metriche di
bit di una catena FSK non coerente a livello di simbolo, calcolate come fa il
demodulatore: per FT8 79 simboli 8-FSK e i cinque insiemi di metriche di
`run_ft8_bitmetrics` (gruppi di 1, 2 e 3 simboli, l'insieme normalizzato al
massimo e il massimo dei tre), per FT2 103 simboli 4-FSK e tre insiemi (gruppi
di 1, 2 e 4 simboli). Si conta **a trame**, come il decoder vero: una trama e'
decodificata se almeno un insieme restituisce la parola trasmessa, ed e' una
falsa decodifica se almeno un insieme ne restituisce un'altra. Limiti del
banco: AWGN, fase costante sulla trama, tempo e frequenza ideali, nessuna
interferenza fra simboli; per FT2 il banco usa il max-log sulle ampiezze dove
il demodulatore usa una log-somma di esponenziali, quindi li' e'
un'approssimazione. Senza bit AP.

Semi 41, 42, 43, mai usati per scegliere niente. Eb/N0 per 91 bit. Trame
decodificate su 24 000 per punto; false decodifiche su trame di solo rumore;
tempo del solo LDPC per trama di rumore, un thread, macchina condivisa con
altri lavori (indicativo).

FT8, 600 000 trame di rumore:

| | esercizio (`llr_clip` 2,5) | `llr_clip` 2,0 | `llr_clip` spento | K77 + margine |
|---|---:|---:|---:|---:|
| 2,0 dB | 6462 | 5770 | 6753 | 6582 |
| 2,5 dB | 12755 | 11913 | 13051 | 12853 |
| 3,0 dB | 19119 | 18541 | 19323 | 19178 |
| 3,5 dB | 22722 | 22499 | 22797 | 22768 |
| Eb/N0 a FER 0,5 | 2,43 dB | 2,50 dB | 2,40 dB | 2,42 dB |
| Eb/N0 a FER 0,1 | 3,26 dB | 3,32 dB | 3,25 dB | 3,26 dB |
| false su rumore (per seme) | 75 (23, 25, 27) | 33 (12, 9, 12) | 101 (30, 33, 38) | 24 (8, 7, 9) |
| false su trame con segnale (4 punti, 96 000 trame) | 31 | 13 | 44 | 6 |
| µs per trama di rumore | 258 | 252 | 250 | 426 |

FT2, 1 500 000 trame di rumore:

| | esercizio (`llr_clip` 2,5) | `llr_clip` 2,0 | `llr_clip` spento | K77 + margine |
|---|---:|---:|---:|---:|
| 3,0 dB | 9478 | 9409 | 9504 | 9754 |
| 3,5 dB | 15649 | 15588 | 15658 | 15839 |
| 4,0 dB | 20482 | 20455 | 20487 | 20578 |
| 4,5 dB | 22966 | 22964 | 22966 | 22995 |
| Eb/N0 a FER 0,5 | 3,17 dB | 3,18 dB | 3,17 dB | 3,15 dB |
| Eb/N0 a FER 0,1 | 4,16 dB | 4,16 dB | 4,16 dB | 4,14 dB |
| false su rumore (per seme) | 1 (0, 1, 0) | 1 (0, 1, 0) | 1 (0, 1, 0) | 0 |
| false su trame con segnale (4 punti, 96 000 trame) | 0 | 0 | 1 | 1 |
| µs per trama di rumore | 139 | 141 | 137 | 312 |

Gli Eb/N0 a FER 0,5 e 0,1 sono interpolati (log-lineare) fra i quattro punti
misurati. La colonna "esercizio" e' il punto di lavoro di default, prima e
dopo questo giro di misure: **nessun default e' cambiato**.

#### Taglio degli LLR sulle metriche FSK

`llr_clip` era stato scelto su BPSK con impulsi artificiali, con la decisione
fra 2 e 2,5 rimandata agli LLR veri. Rifatta sulle metriche FSK, con il preset
di esercizio:

- **FT8.** Alla soglia di esercizio il taglio a 2 dimezza le false decodifiche
  (33 contro 75) ma costa decodifiche: dal 10,7% in meno a 2,0 dB all'1,0% in
  meno a 3,5 dB, cioe' 0,06-0,07 dB. Non sono due cose separate: e' lo stesso
  spostamento lungo la curva che si ottiene stringendo la soglia del gate. A
  **pari false decodifiche** il taglio a 2 non decodifica di piu'. La verifica:
  soglia del gate appreso spostata, sui semi 1-4, fino ad avere con il taglio a
  2 le stesse false del taglio a 2,5 (da -4,1689 a -4,3763), poi applicata ai
  semi 31, 32, 33 mai visti (24 000 trame per punto, 600 000 di rumore):

  | | false su rumore | 2,0 dB | 2,5 dB | 3,0 dB | 3,5 dB |
  |---|---:|---:|---:|---:|---:|
  | taglio 2,5, soglia di esercizio | 72 | 6540 | 12925 | 19095 | 22675 |
  | taglio 2,0, soglia di esercizio | 29 | 5866 | 12129 | 18484 | 22432 |
  | taglio 2,0, soglia allentata | 89 | 6415 | 12813 | 19016 | 22642 |
  | senza taglio, soglia di esercizio | 100 | 6828 | 13236 | 19298 | 22753 |
  | senza taglio, soglia stretta (-4,0856) | 69 | 6572 | 12943 | 19094 | 22669 |

  Il taglio a 2 con la soglia allentata e' peggio su tutti e due gli assi;
  togliere il taglio e stringere la soglia restituisce il punto di esercizio.
  Quindi non c'e' un margine di false decodifiche da spendere allentando il
  gate, e la soglia non e' stata toccata.
- **FT2.** Il taglio non sposta niente di misurabile: le false su 1,5 milioni
  di trame di rumore sono 1 in tutti e tre i casi, e le decodifiche cambiano di
  meno dell'1% (9409 / 9478 / 9504 a 3,0 dB). In FT2 decidono `nd_max` e il
  limite di 22 bit ribaltati.
- **Decoder vero, sui WAV sintetici** (`ft8_stage_compare`,
  `ft2_stage_compare`, variabile `DECODIUM_LDPC_LLR_CLIP`). FT8, 8 WAV,
  decodifiche con taglio spento / 2,5 / 2,0: profondita' 1 69 / 67 / 67,
  profondita' 2 70 / 69 / 69, profondita' 3 (esercizio) 86 / 86 / 84, profilo
  profondo 88 / 87 / 85, profilo di raccolta 84 / 83 / 81. Su 40 WAV di solo
  rumore, profondita' 2 e 3: 0 decodifiche in tutti e tre i casi. FT2, 8 WAV
  con verita' nota, tre profondita': 174 decodifiche giuste e 0 fuori dalla
  verita' in tutti e tre i casi, stessi messaggi. Il corpus FT8 non ha file di
  verita' ed e' piccolo: conferma il verso (2 toglie decodifiche), non misura
  le false.

**Decisione: `llr_clip` resta 2,5 in entrambi i modi.** Il valore 2 non e' un
guadagno, e toglierlo del tutto non lo e' nemmeno: a pari false decodifiche i
tre valori si equivalgono su AWGN, e 2,5 e' quello che protegge dagli impulsi
(tabella della sezione "Robustezza") al costo minore.

La misura ha trovato un difetto: `DECODIUM_LDPC_LLR_CLIP` veniva letta solo in
modalita' FT8, quindi in FT2 era ignorata in silenzio. Ora vale in entrambi i
modi.

#### Ricontrollo delle soglie del gate a margine sulle metriche FSK

Le soglie di `MARGIN_GATE_FT8` e `MARGIN_GATE_FT2` erano state tarate su
canali a una parola per volta (BPSK e M-FSK simbolo per simbolo). Sulle trame
della catena FSK, senza ritararle (colonna "K77 + margine" delle due tabelle
sopra, semi mai visti):

- FT8: 24 false su 600 000 trame di rumore contro 75, con decodifiche uguali o
  di poco superiori (dallo 0,2% all'1,9% in piu'; FER 0,5 a 2,42 contro
  2,43 dB). Le soglie reggono, e lasciano un margine di false che non e' stato
  speso: allentarle richiede una taratura sulle trame che non e' stata fatta.
- FT2: 0 false contro 1 su 1,5 milioni di trame (troppo poche per distinguere),
  decodifiche dallo 0,1% al 2,9% in piu' (0,02 dB).
- Il tempo dell'LDPC cresce del 65% in FT8 e piu' che raddoppia in FT2. Nel
  decoder vero, rispetto allo stesso binario a opzione spenta, sono circa il
  16% sul profilo di esercizio di FT8 e il 22% in FT2 (sezione "Ricerca su
  (174,77) e gate a margine"): e' il motivo per cui l'opzione resta spenta.

#### Opzioni da ambiente

Si leggono alla costruzione del decoder (una per thread e per modo). Senza
variabili il comportamento e' quello di default.

| variabile | effetto | default |
|---|---|---|
| `DECODIUM_LDPC_LLR_CLIP` | taglio degli LLR in multipli della media dei moduli della parola; 0 lo spegne. Tutti i preset, entrambi i modi. Vale solo un numero fra 0 e 10 scritto con il punto; qualunque altra cosa viene ignorata | 2,5 |
| `DECODIUM_LDPC_K77` | 1: OSD sul codice (174,77) **e** gate a margine, con i bit AP come vincolo se `DECODIUM_LDPC_AP_CHECK` e' acceso. Solo preset di esercizio, entrambi i modi (in FT2 la soglia non e' validata, vedi sotto) | 0 |
| `DECODIUM_LDPC_K77_NOGATE` | 1 (solo con K77): la ricerca (174,77) con i gate di prima al posto del gate a margine. **Solo per misurare**: con bit AP in FT8 lascia passare circa il 9% delle parole di solo rumore | 0 |
| `DECODIUM_LDPC_MARGIN_THR` | sposta la soglia del gate a margine; manopola di misura | 0,2000 FT8, 0,040 FT2 |
| `DECODIUM_LDPC_GATE` | gate appreso acceso o spento, in entrambi i modi | acceso in FT8, spento in FT2 |
| `DECODIUM_LDPC_GATE_DELTA`, `DECODIUM_LDPC_GATE_RELAX` | soglia del gate appreso e allargamento dell'OSD a gate acceso; manopole di misura | 0 e 0,25 |
| `DECODIUM_LDPC_MAX_HARD` | limite sui bit ribaltati, per entrambi i modi | 58 FT8, 22 FT2 |
| `DECODIUM_LDPC_AP_CHECK` | 0 spegne il controllo di coerenza con i bit AP | acceso |
| `DECODIUM_LDPC_ALPHA`, `DECODIUM_LDPC_MAX_ITER` | normalizzazione (in 1/65536) e iterazioni del min-sum | 37888 e 10 |
| `DECODIUM_LDPC_NTAU`, `DECODIUM_LDPC_PAIR_SPAN`, `DECODIUM_LDPC_SPAN2_BASE` | ampiezza della ricerca del preset di esercizio | 13, 64 e 64 |

#### Riprodurre le misure

Il banco non fa parte della build normale. Con
`-DDECODIUM_BUILD_SUPERLDPC_K77_BENCH=ON` (x86-64, GCC o Clang) CMake
costruisce il target `superldpc_k77_bench` e registra in ctest
`superldpc_k77_selftest`, che e' `selftest=1`. A mano, dalla radice dei
sorgenti:

    g++ -O3 -DNDEBUG -std=gnu++17 -mavx2 -mfma -DSUPERLDPC_HAVE_GATE_WEIGHTS=1 -I. \
        -o superldpc_k77_bench tests/tools/superldpc_k77_bench.cpp \
        Detector/superldpc/decodium_bridge.cpp Detector/FtxLdpc.cpp \
        lib/crc14.cpp lib/crc13.cpp -lpthread

    ./superldpc_k77_bench selftest=1

    # le due tabelle di questa sezione (un seme per volta, poi si sommano le righe "T")
    ./superldpc_k77_bench mode=ft8 chan=chain variants=base,clip2.0,clip0,new \
        ebn0=2,2.5,3,3.5 n=8000 noise=200000 seeds=41
    ./superldpc_k77_bench mode=ft2 chan=chain variants=base,clip2.0,clip0,new \
        ebn0=3,3.5,4,4.5 n=8000 noise=500000 seeds=41

    # tabelle della sezione "Ricerca su (174,77) e gate a margine"
    ./superldpc_k77_bench mode=ft8 chan=fsk variants=base,new \
        ebn0=3,3.5,4,4.5 n=20000 noise=400000 seeds=21,22,23

`base` e' il default, `clipX` e' il default con `DECODIUM_LDPC_LLR_CLIP=X`,
`new` e' `DECODIUM_LDPC_K77=1`, `k77` aggiunge `DECODIUM_LDPC_K77_NOGATE=1`.
Un seme FT8 della
catena dura una decina di minuti su un core.

### Un bug trovato nel decoder di Decodium

Misurando il confronto e' emerso che il ramo min-sum di
`Detector/FtxLdpc.cpp` non negava il segno del messaggio check->variabile,
mentre il ramo esatto (`use_exact_bp`, attivo solo con norder>=4) calcola
`2*platanh(-tmn)`. I due rami producevano segni opposti per ogni grado di
riga, quindi il BP non convergeva mai e tutto il carico ricadeva sull'OSD.
Corretto con una negazione; su 1000 parole a 1 dB le parole chiuse dal BP
passano da 0 a 305 e le decodifiche totali da 713 a 834. Il ramo e' condiviso
con FT8 e FT4.

### Quello che NON ha funzionato

L'ipotesi che i check rimasti insoddisfatti dal min-sum predicano il successo
dell'OSD, e permettano quindi di saltarlo sulle parole senza speranza, **è
falsa**. Misurata su 17 027 parole a 0,5 dB (`cpp/triage_stats.cpp`), P(l'OSD
riesce) resta piatta:

| check insoddisfatti | 1-2 | 3-4 | 5-6 | 7-8 | 9-12 | 13-16 | 17-24 |
|---|---:|---:|---:|---:|---:|---:|---:|
| P(OSD riesce) | 58% | 65% | 63% | 68% | 62% | 57% | 48% |

Non c'è nessuna soglia utile. Ha senso a posteriori: l'OSD riesce o fallisce in
base a quanti errori cadono nel *set d'informazione* (i 91 bit più affidabili),
non a quanti check restano aperti. Il conteggio resta esposto come
`unsat()` per diagnostica. Va aggiunto che a 0,5 dB l'OSD recupera comunque il
60 % delle parole che riceve: non c'è molto lavoro sprecato da tagliare.

**La Gauss incrementale non conviene**, e il motivo è misurabile
(`cpp/pivot_stats.cpp`). L'idea era di non ricalcolare la forma ridotta da zero
per ogni parola — H è fissa — ma di partire da una base precalcolata e fare solo
le sostituzioni di colonna necessarie. Il guadagno è però proporzionale a quanto
il set di pivot voluto si sovrappone a quello di riferimento, e su 13 473 parole
la frequenza con cui una colonna finisce fra i pivot è **quasi uniforme: 0,45
minimo, 0,48 mediana, 0,50 massimo**. Non c'è struttura da sfruttare: il set di
pivot lo decide il rumore. Le 83 colonne migliori possibili coprono in media
40,5 pivot su 83 (49 %), il che mette un tetto di ~1,97x sulla Gauss, cioè
~1,3x sul totale OSD, in cambio di gestire lo stato di conferma dei pivot e del
rischio di divergere dal riferimento. Non vale il cambio.

### Ricerca su (174,77) e gate a margine (17 novembre 2026: nel codice, spenti)

Due cose nuove, tutte e due solo in ricezione e tutte e due **spente di
default**: `DECODIUM_LDPC_K77=1` le accende insieme, per il solo preset di
esercizio (`norder <= 3`). Senza variabili il decoder decide come prima
(verificato: uscite del bridge identiche a quelle dell'albero di partenza su
24 configurazioni con e senza bit AP, e liste di decodifica identiche a un
binario costruito dall'albero di partenza con le stesse opzioni su tutti i
WAV: 40 file FT8, 24 FT2, 24 FT4).

La ricerca non si accende senza il gate. `DECODIUM_LDPC_K77_NOGATE=1` la lascia
ai gate di prima ed esiste solo per misurare quanto e' peggio (piu' sotto).

**(a) OSD sul codice concatenato.** La CRC-14 e' lineare, quindi i suoi 14
vincoli sono 14 righe di parita' in piu': `OsdFast::crc_in_base` le ricava da
`crc14_syn` (la stessa funzione che fa il controllo), le aggiunge alle 83 di H
e fa l'eliminazione di Gauss su 97 righe. La base piu' affidabile ha 77
posizioni d'informazione e ogni candidato passa la CRC per costruzione. Con
`ap_vincolo` i bit noti per ipotesi a priori sono un vincolo della ricerca: un
candidato che ne contraddice uno non viene generato, invece di essere scelto e
poi scartato dal controllo di coerenza a valle.

**(b) Gate a margine.** Sul codice (174,77) l'OSD trova sempre una parola, e
"ha passato la CRC" non dice piu' niente. La statistica e' quanto il candidato
restituito e' piu' vicino del suo concorrente (il migliore fra gli altri):
`z = nd - c_gap*clip(nd2 - nd) - w_logit*logit <= thr*(N/liberi)^ap_pow`
(`gate.hpp`, `margin_accept`; parametri in `gate_weights.hpp`). Se il
concorrente non esiste (77 bit noti: la parola possibile e' una sola) decide la
regola di prima.

**Misure.** Banco `tests/tools/superldpc_k77_bench.cpp`, che passa dal bridge
(`superldpc_simd_decode174_91_batch_c`) e quindi misura la configurazione che
il programma usa, non i preset autonomi di questo README. Eb/N0 riferito a 91
bit; canale M-FSK non coerente simbolo per simbolo (8 toni FT8, 4 FT2, un solo
insieme di metriche per parola) oppure BPSK; soglie tarate sui semi 1, 2, 3 e
riportate qui sui semi 21, 22, 23 mai visti; 60 000 parole per punto, parole
decodificate giuste / false decodifiche su solo rumore. Le soglie sono state
poi ricontrollate sulla catena di metriche FSK del demodulatore, a trame: vedi
"Ricontrollo delle soglie del gate a margine" in "Cosa gira davvero in
Decodium".

FT8, senza AP (1,2 milioni di parole di rumore):

| canale | punto | prima | K77 + margine |
|---|---|---|---|
| 8-FSK | 3,0 dB | 8254 | 8637 |
| 8-FSK | 3,5 dB | 21677 | 22447 |
| 8-FSK | 4,0 dB | 39939 | 40762 |
| 8-FSK | 4,5 dB | 53558 | 54043 |
| 8-FSK | rumore, falsi | 21 | 13 |
| BPSK | 0,5 dB | 37736 | 38552 |
| BPSK | 1,0 dB | 49791 | 50567 |
| BPSK | 1,5 dB | 56709 | 57264 |
| BPSK | rumore, falsi | 17 | 13 |

In dB e' poco: letto sulla pendenza delle curve, circa 0,02 dB su 8-FSK (da
+383 a +823 parole dove la curva ne sale 27 000-36 000 per dB), 0,03-0,04 dB su
BPSK e 0,01 dB sulle trame della catena FSK (FER 0,5 a 2,42 contro 2,43 dB,
tabella in "Cosa gira davvero in Decodium"). Una seconda misura indipendente
sul canale 8-FSK (semi 201-203) da' lo stesso quadro: 22652 contro 22027
parole a 3,5 dB e 40864 contro 40105 a 4,0 dB su 60 000, 13 falsi contro 17 su
1,2 milioni di parole di rumore.

Con bit AP il guadagno e' soprattutto sui falsi,
perche' la regola di prima ne lascia passare molti (FT8, 8-FSK):

| bit AP | prima: giuste a meta' curva / falsi su rumore / falsi con ipotesi sbagliata su segnale (600 000 parole) | K77 + margine |
|---|---|---|
| 32 | 40970 (3 dB) / 10602 / 8609 | 45054 / 1824 / 1183 |
| 61 | 38183 (2 dB) / 52 / 52 | 39809 / 6 / 8 |
| 77 | 11029 (0 dB) / 0 / 1 | 11072 / 0 / 1 |

FT2, senza AP (6 milioni di parole di rumore): 12375 -> 12926 a 4,5 dB,
27501 -> 28073 a 5,0, 44746 -> 45010 a 5,5, 55284 -> 55351 a 6,0; falsi sul
rumore 3 -> 5. Non e' un guadagno a pari false decodifiche: in FT2 decide gia'
la soglia a 22 bit ribaltati. La seconda misura (semi 201-203) non cambia il
giudizio: 1 falso contro 2 su 2,1 milioni di parole di rumore senza AP, ma 1
contro 0 su 900 000 con 32 bit AP; 28082 contro 27532 parole a 5,0 dB. I
conteggi dei falsi sono troppo piccoli per dire qualcosa, quindi
`MARGIN_GATE_FT2` **non e' validato** a pari o meno false decodifiche, e in
FT2 `DECODIUM_LDPC_K77=1` e' da considerare sperimentale.

Altre due cose misurate, che conviene sapere:

- La sola ricerca (174,77) con i gate di prima (`DECODIUM_LDPC_K77_NOGATE=1`)
  NON paga, e con bit AP e' molto peggio del default. Senza AP decodifica di
  piu' (FT8 4,0 dB: 41117 contro 40086, semi 11-13) ma i falsi sul rumore
  salgono da 24 a 31. Con 32 bit AP in FT8 accetta 13 342 parole di solo rumore
  su 150 000 (8,9%; semi 201-203), contro 2611 su 300 000 del default (0,87%) e
  442 su 300 000 con il gate a margine: il gate appreso e' tarato sui candidati
  della ricerca (174,91) e su quelli della (174,77) non filtra piu'. Per questo
  `DECODIUM_LDPC_K77=1` accende sempre anche il gate a margine.
- In FT8 il margine da solo (`w_logit = 0`) non batte il gate appreso: a 22
  falsi su 1,2 milioni (semi di taratura), 40035 parole contro 40045 a 4,0 dB. Il guadagno viene dalla
  somma di margine e logit del gate appreso.

**Perche' restano spenti.** Nel decoder vero (`ft8_stage_compare`,
`ft2_stage_compare`), tre binari costruiti con le stesse opzioni: l'albero di
partenza, questo con l'opzione spenta, questo con `DECODIUM_LDPC_K77=1`. Tempo
di CPU sommato sugli 8 WAV, macchina condivisa con altri lavori:

| | decodifiche: partenza / spenta / accesa | tempo di CPU: partenza / spenta / accesa |
|---|---|---|
| FT8, profondita' 1 (una corsa) | 67 / 67 / 68 | 1,16 / 1,27 / 1,28 s |
| FT8, profondita' 2 (due corse) | 69 / 69 / 69 | 2,37 / 2,35 / 2,41 s |
| FT8, profondita' 3, esercizio (tre corse) | 86 / 86 / 84 | 22,9 / 21,3 / 24,7 s (22,7-23,1 / 21,0-21,7 / 24,0-25,4) |
| FT2, profondita' 1 (cinque corse) | 49 / 49 / 49 | 0,34 / 0,30 / 0,35 s |
| FT2, profondita' 2 | 62 / 62 / 62 | 1,16 / 1,11 / 1,36 s |
| FT2, profondita' 3 | 63 / 63 / 63 | 1,30 / 1,24 / 1,51 s |

Sul profilo di esercizio di FT8 l'opzione accesa costa l'8% rispetto
all'albero di partenza e il 16% rispetto allo stesso binario a opzione spenta,
e perde due decodifiche ("K1JT HA5WA 73" e "N1API F2VX 73") senza guadagnarne.
In FT2 costa il 16-17% e il 22-23%, con gli stessi 174 messaggi. La differenza
fra le due basi e' `colmask()` senza salti (paragrafo seguente), che c'e' in
entrambi i casi.

Nessuna falsa decodifica in piu' (FT2: 174 decodifiche giuste e 0 fuori dalla
verita' in tutti e tre i casi; FT8: 0 decodifiche su 40 WAV di solo rumore),
ma nemmeno un guadagno visibile, e il tempo cresce oltre il 10%: ogni candidato
del codice (174,77) va pesato, mentre prima la CRC ne
scartava 16383 su 16384 con uno XOR. Il corpus e' piccolo (una novantina di
decodifiche) e non puo' vedere uno 0,02 dB; quello che dice e' che il costo c'e'
e il guadagno non si vede.

**Una modifica che invece e' sempre attiva: `colmask()` senza salti.** La
maschera di colonna di un bit d'informazione si costruiva con un `if` per riga
di parita' su un bit che vale 0 o 1 con la stessa probabilita'; ora e' uno
shift e un OR, con lo stesso risultato (uscite del bridge identiche bit per
bit). Tempo dell'LDPC per parola, microbanco sul bridge, 20 000 parole, minimo
di cinque corse alternate:

| | partenza | ora, default | ora, `DECODIUM_LDPC_K77=1` |
|---|---|---|---|
| FT8, solo rumore | 64,6 us | 50,5 us | 85,2 us |
| FT8, segnale a 3,5 dB | 85,1 us | 69,9 us | 87,3 us |
| FT2, solo rumore | 64,8 us | 49,5 us | 103,9 us |
| FT2, segnale a 4,5 dB | 77,0 us | 63,8 us | 75,5 us |

Nel decoder vero l'LDPC e' una parte del totale: il default e' piu' veloce
dell'albero di partenza del 7% sul profilo di esercizio di FT8 e del 4-5% in
FT2 alle profondita' 2 e 3 (tabella sopra), con liste di decodifica identiche.

## Uso

    #include "ft2_decoder.hpp"

    Code code = Code::load("data/ldpc_174_91.h.txt");
    Ft2Decoder dec(code, Ft2Decoder::sensibile());

    std::vector<uint8_t> bits(n * 174), ok(n);
    int accettate = dec.decode_batch(llr, n, bits.data(), ok.data());
    for (int i = 0; i < n; ++i)
        if (ok[i]) usa_messaggio(&bits[i * 174]);       // 77 bit + 14 di CRC

`llr` è `[n][174]` float, positivo = bit 0, nel formato del demodulatore. `n` è
libero: il riempimento del batch lo gestisce il decoder. Un'istanza per thread,
nessuno stato condiviso.

Preset: `veloce()` (solo min-sum, nessuna falsa decodifica), `conservativo()`
(OSD-2/span32, come WSJT-X), `sensibile()` (OSD-3, il default della libreria).
Oppure una `Ft2Config` esplicita — ordine, span2, span3 e `nd_max` sono
indipendenti. Il programma usa una `Ft2Config` sua per il profilo di esercizio
(vedi "Cosa gira davvero in Decodium") e i preset `conservativo` e `sensibile`
solo per i profili profondi.

In Decodium il dispatcher seleziona AVX2/FMA su x86, NEON su ARM64 e usa il
decoder originale generico quando il backend richiesto non è disponibile. Il
fallback resta compilato per la CPU minima, quindi l'assenza di una estensione
SIMD non impedisce l'avvio. `bits.hpp` copre anche MSVC.

I comandi `make` qui sotto e i file `cpp/*.cpp`, `tools/` e `data/` citati in
questo README appartengono al pacchetto autonomo da cui la libreria proviene e
**non sono in questo albero**: qui ci sono gli header, il bridge e il banco
`tests/tools/superldpc_k77_bench.cpp`.

    make            # tutti i binari
    make test       # CRC, equivalenza con l'implementazione di riferimento, i tre preset
    make sweep      # tabella completa
    make gate       # ritaratura della soglia del gate
    make profile    # ripartizione del tempo dell'OSD per fase

## Roadmap

- [x] Gate anti-false-decode sulla distanza soft normalizzata.
- [x] Early-exit per sotto-batch nel min-sum (granularità: gruppo di 16).
- [x] Interfaccia unica per DECODIUM: `Ft2Decoder::decode(llr, n, ...)`.
- [x] `span2`/`span3` come parametri: con il costo per candidato quasi nullo la
      scelta è diventata una decisione di progetto, non un vincolo.
- [x] `llr_clip`: limite sui |LLR| relativo alla parola, contro l'interferenza
      impulsiva.
- [x] Decidere `llr_clip` (2 o 2,5) sulle metriche FSK: resta 2,5 in entrambi
      i modi (vedi "Taglio degli LLR sulle metriche FSK").
- [x] Verifica del preset di esercizio e delle soglie dei gate su metriche FSK
      sintetiche a livello di simbolo (non AWGN/BPSK), FT8 e FT2.
- [ ] La stessa verifica su traffico vero: la catena del banco ha tempo e
      frequenza ideali e nessuna interferenza.
- [ ] Soglia del gate derivata dal numero di candidati provati invece che
      tarata a mano per preset.
- [ ] Codici quasi-ciclici alternativi (bivariate-bicycle) a parità di 174 bit.

## File

In questo albero (`Detector/superldpc/`):

    ft2_decoder.hpp       API pubblica: Ft2Decoder + preset. E' l'unico da includere.
    minsum_avx2.hpp       selezione MinSumV3 e stadio 1 con intrinsics AVX2
    minsum_neon.hpp       stadio 1 MinSumV3 con intrinsics ARM NEON
    osd_fast.hpp          OsdFast, stadio 2: CRC incrementale, Gauss branchless, pruning, ricerca (174,77)
    gate.hpp              gate appreso e gate a margine
    gate_weights.hpp      pesi del gate appreso e parametri del gate a margine, per modo
    plausible.hpp         controllo di plausibilita' del messaggio dentro l'OSD
    decoder.hpp           CRC-14 e sindrome, MinSumV2 e OsdDecoder (riferimento scalare)
    minsum.hpp            Code (lettura di H) + MinSumDecoder, prima versione
    bits.hpp              popcount/ctz portabili (gcc, clang, MSVC)
    decodium_bridge.cpp   innesto in Decodium: preset di esercizio, opzioni da ambiente
    decodium_dispatch.cpp scelta del backend (AVX2, NEON, generico) all'avvio
    tests/tools/superldpc_k77_bench.cpp   (radice di Decodium) banco e selftest: preset del bridge, catena FSK, (174,77), gate a margine

Nel pacchetto autonomo, non in questo albero:

    cpp/bench3.cpp        banco dell'API pubblica, con OpenMP
    cpp/bench2.cpp        banco a basso livello, per variare i singoli parametri
    cpp/verify.cpp        equivalenza bit-per-bit ottimizzato vs riferimento
    cpp/gate_stats.cpp    distribuzione di nd per corrette e false (taratura del gate)
    cpp/triage_stats.cpp  P(successo OSD | check insoddisfatti) — l'esperimento negativo
    cpp/osd_profile.cpp   ripartizione del tempo dell'OSD per fase
    tools/gen_test.py     vettori AWGN/BPSK + riferimento BP float
    tools/crc14.py        CRC-14, confrontata con la versione a byte di ft8_lib

## Provenienza e attribuzione

Quattro livelli, di cui solo l'ultimo e' opera di questo progetto. Vale la pena
tenerli distinti, perche' confonderli e' un errore che in questa comunita' viene
notato subito.

**La classe di codici.** I codici LDPC sono di Robert Gallager, tesi di dottorato
al MIT, 1962, riscoperti da MacKay e Neal negli anni Novanta. Non sono di
WSJT-X, non sono di nessuno di noi.

**Gli algoritmi di decodifica.** Il min-sum normalizzato e' di Chen e Fossorier,
l'ordered statistics decoding di Fossorier e Lin: letteratura consolidata degli
anni Novanta. La ricerca a coppie (`pair_search`) e' invece modellata sui passi
npre1/npre2 di `osd174_91` di WSJT-X — l'idea e' loro, l'implementazione e la
chiave lineare che la rende praticabile sono nostre.

**Il codice specifico.** Le 83 righe di parita' del LDPC(174,91), la matrice
generatrice e la CRC-14 con polinomio 0x2757 appartengono al protocollo FT8,
progettato da Steve Franke K9AN e Joe Taylor K1JT e pubblicato su QEX
("The FT4 and FT8 Communication Protocols"). Sono estratte da `constants.c` di
ft8_lib e verificate bit per bit. **Non vanno cambiate**: cambiarle romperebbe
la compatibilita' con qualunque altra stazione.

**Il decodificatore.** `superldpc` e' scritto da zero. Sono originali il min-sum
vettorizzato AVX2 a sedici parole per registro, l'eliminazione di Gauss senza
salti condizionati, la sindrome CRC incrementale ottenuta sfruttando la
linearita' della CRC-14 con bit-slicing, la potatura per limite inferiore, il
gate `nd` anti-fantasma, il controllo di plausibilita' del messaggio dentro il
ciclo di accettazione e l'interfaccia a blocco — e con essi tutte le misure di
questo README.

Formula breve, se serve citarlo:

> `superldpc` e' un decodificatore scritto ex novo per Decodium 4.0 Core Shannon.
> Implementa algoritmi noti — codici LDPC (Gallager, 1962), min-sum normalizzato,
> ordered statistics decoding — con vettorizzazione AVX2 e ottimizzazioni
> originali. Opera sul codice LDPC(174,91) e sulla CRC-14 del protocollo FT8
> (Franke K9AN, Taylor K1JT), usati senza modifiche per garantire compatibilita'
> bit-a-bit. GPL-3.0; tabelle del codice verificate contro ft8_lib (MIT,
> Karlis Goba YL3JG).

**Licenze.** `superldpc` e tutto Decodium sono GPL-3.0. Le tabelle del codice
vengono da `constants.c` di **ft8_lib, che e' sotto licenza MIT** (Karlis Goba
YL3JG) — codice MIT si puo' includere in un progetto GPL, ma la licenza e'
quella, non la GPL. WSJT-X, da cui viene l'idea dei passi npre1/npre2 della
ricerca a coppie, e' GPL-3.0.

**Dove stanno davvero le tabelle nell'integrazione.** Dentro Decodium
`superldpc` non porta con se' le proprie tabelle: le chiede al programma con
`ftx_ldpc174_91_tables_c` (`Detector/FtxLdpc.cpp`). La verifica bit per bit
contro `constants.c` di ft8_lib e' servita a garantire che siano le stesse,
ma la copia usata a runtime e' quella di Decodium.

**Una precisazione che conviene fare per primi.** "Scritto da zero" vale per
`superldpc`, non per tutto Decodium: il decodificatore precedente,
`Detector/FtxLdpc.cpp`, e' un porting del decodificatore di WSJT-X — si vede
dalle tabelle `Mn`/`Nm`/`nrw` indicizzate da 1 e da `platanh` con le stesse
costanti del Fortran. Sono due cose diverse e vanno dette separate.
