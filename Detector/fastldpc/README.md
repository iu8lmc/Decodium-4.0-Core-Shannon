# fastldpc — decoder SIMD a due stadi per LDPC(174,91) di FT2

    stadio 1  min-sum normalizzato, layered, int16, AVX2 su x86 o NEON su ARM64
    stadio 2  OSD di ordine 0..3 SOLO sulle parole che lo stadio 1 non chiude
    gate      CRC-14 (poly 0x2757, identica a WSJT-X) + distanza soft normalizzata

Header-only, GPL-3.0, si integra in DECODIUM includendo `cpp/ft2_decoder.hpp`.
Scritto ex novo: non e' un adattamento del decoder di WSJT-X. Opera pero' sul
codice LDPC(174,91) e sulla CRC-14 del protocollo FT8, usati **senza
modifiche** (vedi "Provenienza e attribuzione" in fondo).

Rispetto alla prima versione del progetto: **+0,35 dB** rispetto alla
configurazione OSD-2/span32 di partenza e **+1,3 dB** rispetto al solo min-sum,
a parità di false decodifiche, con la catena **16x più veloce** a 2 dB. I due
risultati sono legati: il guadagno di sensibilità viene dall'usare un ordine OSD
molto più alto, cosa diventata praticabile solo perché il costo per candidato è
crollato.

## Risultati misurati

AWGN/BPSK, 20 000 parole per punto, 1 thread, batch 64, 30 iterazioni.
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
un'implementazione tradizionale**, che è esattamente da dove arrivano i +0,6 dB.

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
demodulatore 4-GFSK reale non li produrrà così, quindi conviene sapere in
anticipo a cosa la catena è sensibile e a cosa no.

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
dell'86 % nel caso peggiore, ma costa il 5 % di FER su canale pulito. Quale dei
due sia giusto lo dirà il canale reale — è la prima cosa da decidere quando
arriveranno gli LLR del 4-GFSK.

### Ipotesi a priori (AP) e innesto in Decodium

FT2 non decodifica alla cieca: fa piu' passate in cui alcuni bit del messaggio
sono gia' noti (il proprio nominativo, quello del corrispondente, CQ), e li
passa al decoder come `apmask` con gli LLR gia' portati al valore giusto e
magnitudine grande. Senza supportarlo, fastldpc non e' utilizzabile li'.

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

### Innesto: `cpp/decodium_bridge.cpp`

Espone `fastldpc_decode174_91_c` con la **stessa firma** di
`ftx_decode174_91_c` di Decodium 4, conversione di segno inclusa (li' LLR
positivo = bit 1, qui = bit 0). Sostituirlo e' una riga in
`Detector/FtxFt2Stage7.cpp`. Un'istanza per thread; `FtxLdpc.cpp` resta al suo
posto, serve ancora per l'encoder, le tabelle e le CRC.

Misura a 1 dB, 1000 parole, contro il decoder di Decodium (con il fix del segno
del min-sum gia' applicato — vedi sotto):

| bit noti | Decodium | fastldpc | false | velocita' |
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
(OSD-2/span32, come WSJT-X), `sensibile()` (OSD-3, il default). Oppure una
`Ft2Config` esplicita — ordine, span2, span3 e `nd_max` sono indipendenti.

In Decodium il dispatcher seleziona AVX2/FMA su x86, NEON su ARM64 e usa il
decoder originale generico quando il backend richiesto non è disponibile. Il
fallback resta compilato per la CPU minima, quindi l'assenza di una estensione
SIMD non impedisce l'avvio. `bits.hpp` copre anche MSVC.

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
- [ ] Decidere `llr_clip` (2 o 2,5) e rifare la taratura di `nd_max` sugli LLR
      reali del 4-GFSK.
- [ ] Test su LLR reali dal demodulatore 4-GFSK di FT2 (non AWGN/BPSK). La
      soglia del gate va riverificata lì: `nd` assume LLR di canale calibrati.
- [ ] Soglia del gate derivata dal numero di candidati provati invece che
      tarata a mano per preset.
- [ ] Codici quasi-ciclici alternativi (bivariate-bicycle) a parità di 174 bit.

## File

    cpp/ft2_decoder.hpp   API pubblica: Ft2Decoder + preset. E' l'unico da includere.
    minsum_avx2.hpp       selezione MinSumV3 e stadio 1 con intrinsics AVX2
    minsum_neon.hpp       stadio 1 MinSumV3 con intrinsics ARM NEON
    cpp/osd_fast.hpp      OsdFast, stadio 2: CRC incrementale, Gauss branchless, pruning
    cpp/decoder.hpp       CRC-14 e sindrome, MinSumV2 e OsdDecoder (riferimento scalare)
    cpp/minsum.hpp        Code (lettura di H) + MinSumDecoder, prima versione
    cpp/bits.hpp          popcount/ctz portabili (gcc, clang, MSVC)
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

**Il decodificatore.** `fastldpc` e' scritto da zero. Sono originali il min-sum
vettorizzato AVX2 a sedici parole per registro, l'eliminazione di Gauss senza
salti condizionati, la sindrome CRC incrementale ottenuta sfruttando la
linearita' della CRC-14 con bit-slicing, la potatura per limite inferiore, il
gate `nd` anti-fantasma, il controllo di plausibilita' del messaggio dentro il
ciclo di accettazione e l'interfaccia a blocco — e con essi tutte le misure di
questo README.

Formula breve, se serve citarlo:

> `fastldpc` e' un decodificatore scritto ex novo per Decodium 4.0 Core Shannon.
> Implementa algoritmi noti — codici LDPC (Gallager, 1962), min-sum normalizzato,
> ordered statistics decoding — con vettorizzazione AVX2 e ottimizzazioni
> originali. Opera sul codice LDPC(174,91) e sulla CRC-14 del protocollo FT8
> (Franke K9AN, Taylor K1JT), usati senza modifiche per garantire compatibilita'
> bit-a-bit. GPL-3.0.

Tutto il progetto e' GPL-3.0, come WSJT-X e ft8_lib da cui provengono le
tabelle.
