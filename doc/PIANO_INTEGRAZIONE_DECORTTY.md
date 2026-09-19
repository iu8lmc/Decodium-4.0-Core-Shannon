# Integrare DecoRTTY in Decodium 4.0 — piano

*Redatto il 31 agosto 2026. Da leggere prima di toccare il codice.*

## Che cosa si porta dentro

DecoRTTY 0.7.2: RTTY su VITA-49, per FlexRadio in modo diretto e per l'FT-991A
attraverso un gateway che la mette in rete. Niente DAX, niente porta CAT, niente
cavo audio virtuale: l'audio arriva come pacchetti VITA-49 su UDP e il controllo
passa dall'API SmartSDR su TCP 4992.

Sono **30 file `.cpp`** e 20 file QML, circa **8.000 righe** di C++ più
l'interfaccia, in sei moduli:

| modulo | righe | che cosa fa |
|---|---|---|
| `dsp` | 1792 | demodulatore FSK a filtro adattato, AFC, recupero del timing, Baudot a decisione morbida con Viterbi e bigrammi |
| `flex` | 1888 | API SmartSDR, scoperta radio, flusso VITA-49, codec Opus |
| `gateway` | 1176 | gateway FT-991A: CAT e audio codificato verso la rete |
| `app` | 2142 | motore RTTY, waterfall, macro, log QSO, piano di banda |
| `link` | 786 | astrazione della sorgente radio: rete o scheda audio |
| `vita` | 294 | contesto dei pacchetti VITA-49 |

## Perché si può fare senza traumi

**Il precedente esiste ed è più grande.** SSTV è già dentro Decodium come
sottoprogetto (`src/sstv`, 89.000 righe) dietro l'opzione CMake
`DECODIUM_ENABLE_SSTV` e la macro `DECODIUM_HAS_SSTV`. DecoRTTY è dieci volte
più piccolo: la strada è tracciata e collaudata.

**Le collisioni sono due.** DecoRTTY vive già nel namespace `decortty`; gli unici
nomi in comune sono `RadioLink` e una dichiarazione anticipata di `QAudioSink`,
entrambi risolti dal namespace.

**Le dipendenze ci sono già.** Qt 6.5+ contro il 6.11 di Decodium; Opus è già
presente in MSYS2 (`libopus.a`, `libopus.dll.a`).

**Non si sovrappone a DecoPort.** DecoPort è il protocollo *nostro* per mettere
una radio in rete (1224 righe, zero riferimenti a VITA). Il modulo `flex` di
DecoRTTY parla invece VITA-49, cioè il protocollo *di FlexRadio*. Sono
complementari: uno non sostituisce l'altro.

---

## I passi

### 1. Innesto del sottoprogetto — mezza giornata

- `src/rtty/` accoglie i sei moduli, con il loro `CMakeLists.txt` adattato a
  produrre librerie statiche invece di un eseguibile.
- Opzione `DECODIUM_ENABLE_RTTY` (accesa per default) e macro
  `DECODIUM_HAS_RTTY`, sullo stesso schema di SSTV.
- Opus diventa dipendenza opzionale: senza, si compila escludendo `OpusCodec` e
  il gateway perde solo la compressione audio.

**Verifica**: Decodium compila con e senza l'opzione, e senza Opus.

### 2. La sorgente audio — il punto delicato, un giorno

DecoRTTY ha una propria astrazione (`RadioHub`, `RadioLink`, `SoundCardLink`)
che sceglie fra rete e scheda audio. Decodium ha la sua, con in più DecoPort e
il watchdog audio.

Due strade, e conviene decidere qui:

- **(a) DecoRTTY tiene la propria sorgente.** Il flusso VITA-49 arriva
  direttamente al demodulatore RTTY senza passare dal percorso audio di
  Decodium. Più semplice, isolato, nessun rischio per FT8/FT2. Ma l'audio Flex
  resta disponibile al solo RTTY.
- **(b) Il flusso VITA-49 alimenta il percorso audio di Decodium.** FlexRadio
  diventa una sorgente per *tutti* i modi, RTTY compreso. Molto più potente —
  significa FT8 da FlexRadio senza DAX — ma tocca il codice audio che regge la
  ricezione, e va fatto con la stessa prudenza usata per la fase profonda.

**Raccomandazione**: partire da **(a)**, che porta RTTY funzionante senza
rischiare nulla, e valutare **(b)** come passo successivo a sé stante — con il
suo banco di prova, perché quel percorso è quello che ha già dato un blocco
della ricezione quando la sorgente remota è rimasta attiva a monitor spento.

### 3. Il decodificatore come modo — un giorno

- `RttyEngine` va agganciato al ciclo di decodifica: RTTY è a flusso continuo,
  non a slot come FT8, quindi non passa dallo scheduler degli slot ma consegna
  righe man mano.
- Le righe decodificate entrano nel modello della lista con il modo `RTTY`.
- I filtri anti-fantasma **non** si applicano: sono tarati sui nominativi dei
  modi digitali strutturati, mentre RTTY porta testo libero.

**Verifica**: i segnali di prova in `testsignals/` del progetto originale
devono decodificare come prima dell'innesto.

### 4. L'interfaccia — un giorno

I 20 file QML sono già nello stile di Decodium (stessa tavolozza, stessi
pannelli di vetro). Due possibilità:

- finestra staccabile dedicata, come DecoPort e la Live Map;
- pannello nella finestra principale quando il modo attivo è RTTY.

**Raccomandazione**: finestra dedicata, coerente con come sono già trattati gli
strumenti grandi, e senza toccare il layout principale.

### 5. Traduzioni e documentazione — mezza giornata

Le stringhe nuove vanno nei tredici cataloghi esistenti. Attenzione al contesto
di traduzione: separare i file QML in sottocartelle rompe i contesti e fa
apparire «0 unfinished» in modo ingannevole.

---

## I rischi, in ordine di gravità

**Il percorso audio.** È il punto in cui un errore si paga con la ricezione
ferma su tutti i modi. La scelta (a) lo evita del tutto; la (b) va affrontata
separatamente e con un banco di prova.

**Il gateway FT-991A si sovrappone al CAT esistente.** Decodium già parla con
l'FT-991A via Hamlib. Il gateway di DecoRTTY apre la *sua* porta CAT per mettere
la radio in rete: se girano insieme si contendono la porta seriale. Serve un
mutuo interlock, come già esiste fra le due istanze che condividono la CAT.

**Il carico.** RTTY è a flusso continuo: gira sempre, non a slot. Va misurato
che cosa costa mentre FT8 fa due decodifiche profonde per slot — il sistema è
già al limite su un 32-core.

**La manutenzione a valle.** DecoRTTY è un repository a sé con la sua storia.
Una volta innestato, gli aggiornamenti vanno riportati a mano, come già succede
con gli allineamenti a upstream.

---

## Che cosa NON fare

- Non fondere DecoPort e il modulo `flex`: fanno cose diverse e la
  sovrapposizione apparente è solo «radio in rete».
- Non applicare a RTTY i filtri anti-fantasma dei modi strutturati.
- Non far girare il gateway FT-991A e il CAT di Decodium sulla stessa porta
  senza interlock.

---

## Stima

Quattro giorni di lavoro per la strada (a), più il tempo di prova sul campo.
La strada (b) — FlexRadio come sorgente per tutti i modi — è un progetto a sé,
da valutare dopo, e vale probabilmente più della parte RTTY.
