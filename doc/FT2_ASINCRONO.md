# Progetto: ricevitore asincrono FT2 (ASYMX / Async L2) con le tecniche di JTTY

Documento di lavoro per Claude Code. Repo: `iu8lmc/Decodium-4.0-Core-Shannon`.
Analisi fatta sul commit `dae9872` (Release 1.0.649, 22/09/2026): i numeri di riga si riferiscono a quel commit, verificali prima di modificare.

---

## 0. Obiettivo e regole

**Obiettivo.** Rendere il decoder asincrono FT2 più sensibile, più affidabile e meno costoso in CPU, riusando le soluzioni che il modo JTTY di WSJT-X 3.2.0-rc1 adotta per un problema identico: decodificare frame con sync che possono iniziare in qualsiasi istante.

**Regole non negoziabili**
1. **Nessuna modifica al protocollo FT2** (forma d'onda, LDPC, 77 bit, sync). Si tocca solo il ricevitore e la logica di visualizzazione.
2. **Prima si misura, poi si cambia.** La fase F0 (banco) è prerequisito di tutto. Ogni modifica deve riportare: soglia 50% in dB, falsi decode per ora di rumore, latenza, CPU per tick. Stile e metodo di `decode_bench/README.md`: il conteggio dei decode non è una metrica.
3. **Tutto dietro flag** `DECODIUM_FT2_ASYNC_*` (convenzione già usata: `DECODIUM_FT2_ACCUMULO`, `DECODIUM_FT2_AP_MSG`, …), default spento, finché il banco non dimostra il guadagno. Il path di fine slot (`decode()` sincrono) non si tocca se non serve.
4. Commit piccoli, uno per compito, con i numeri del banco nel messaggio.
5. Commenti in italiano come nel resto di `FtxFt2Stage7.cpp`.

---

## 1. Come funziona oggi l'asincrono (mappa del codice)

| Cosa | Dove |
|---|---|
| Riempimento ring buffer 90 000 campioni (7.5 s a 12 kHz) | `widgets/mainwindow.cpp` ~7260, dentro `dataSink()` |
| Timer 100 ms che lancia il decode async | `widgets/mainwindow.cpp` ~4248 (`m_asyncDecodeTimer`) |
| Finestra decodificata | sempre gli **ultimi 45 000 campioni** (3.75 s) del ring |
| Profondità live limitata a 2 (niente OSD) | `embedded_ft2_async_live_depth()` ~1515; `DECODIUM_FT2_ASYNC_DEEP=1` la sblocca |
| Worker | `Detector/FT2DecodeWorker.cpp::decodeAsync()` ~264, sotto `fortran::runtime_mutex()` |
| Decoder | `Detector/FtxFt2Stage7.cpp::decode_ft2_stage7()` ~3039 (stesso decoder dello slot) |
| Ricerca DT per candidato | ~3417: `ibest` da −688 a 2024 campioni a 1333 Hz (−0.52 … +1.52 s), in 3 segmenti |
| Candidati "attesi" esenti dai gate di sync | ~3190-3330: memoria messaggi (`AP_MSG`), mittenti (`STORICO`), `ACCUMULO`, `GENIE` (solo banco) |
| Conferma dei decode deboli (snr < −19: serve vederli 2 volte in 4.5 s) | `MainWindow::asyncConfirmDecode()` ~12846 |
| Dedup testuale 5 s, "vince l'SNR migliore" | `MainWindow::isDuplicateDecode()` ~12807 |
| Visualizzazione righe async | `MainWindow::processFt2AsyncDecodedRows()` ~30340 |

Parametri FT2 (da `FtxFt2Stage7.cpp`): nsps 288 a 12 kHz (41.667 baud), 16 simboli sync + 87 dati = 103 simboli ≈ 2.47 s, downsample ×9 → 1333.33 Hz, 3 insiemi di metriche (a, b, c, cioè blocchi da 1, 2 e 4 simboli, come FT4). Le metriche coerenti multi-simbolo di JTTY **ci sono già**: non vanno reimplementate.

### Cosa si deduce

- Ogni trasmissione resta interamente dentro la finestra per circa 3.75 − 2.47 ≈ 1.3 s, quindi viene **ridecodificata da zero ~13 volte** (un tick ogni 100 ms, se il worker non è occupato). Ogni volta si ripete la ricerca DT su ~2 s di ritardi possibili.
- Per stare nei tempi la profondità live è tagliata a 2, quindi **niente OSD** nel path async: la sensibilità si perde proprio lì.
- La "conferma doppia" dei deboli nasce dalla stessa ridondanza: le due conferme sono spesso **lo stesso audio** visto da due finestre sovrapposte, quindi con lo stesso rumore. Non sono prove indipendenti; la conferma ritarda la visualizzazione senza ridurre davvero i falsi.
- La dedup per testo su 5 s sopprime anche una **ripetizione legittima** dello stesso messaggio entro 5 s (in ASYMX i giri sono di ~3 s: il corrispondente che ripete il rapporto perché non ci ha copiato viene nascosto nella finestra decodifiche). Da verificare se il sequencer usa un percorso diverso.

---

## F0 — Banco di misura asincrono (prerequisito)

**Scopo.** Riprodurre fuori dall'app esattamente ciò che fa l'app: audio continuo → ring → tick ogni 100 ms → `decode_ft2_stage7` → conferma/dedup → righe mostrate.

Compiti:
1. Generatore di WAV lunghi (60-300 s) con trasmissioni FT2 a **istanti casuali non allineati agli slot**, SNR noto (banda 2500 Hz), frequenze casuali, casi con sovrapposizione parziale nel tempo e in frequenza, deriva opzionale. Partire da `tests/ft2_make_test_wav.cpp`. Scrivere un file di verità (t_inizio, f, messaggio, SNR).
2. Harness `tests/ft2_async_bench.cpp` che simula `dataSink` a blocchi realistici e il timer a 100 ms, chiama lo stesso codice di decodifica e le stesse funzioni di conferma e dedup (estrarle da `MainWindow` in funzioni libere o in una piccola classe, senza cambiarne il comportamento).
3. Metriche in uscita:
   - P(decode) vs SNR → soglia 50%;
   - latenza = istante di visualizzazione − fine della trasmissione;
   - CPU media e massima per tick, tick saltati perché il worker era occupato;
   - falsi decode per ora su solo rumore e su registrazioni reali;
   - duplicati mostrati e ripetizioni legittime perse.
4. Script in `decode_bench/` (`bench_async.py`) sul modello di `bench.py`.

**Accettazione.** Il banco riproduce, sulla linea di base attuale, gli stessi decode che l'app mostra con lo stesso WAV (confronto su almeno 3 registrazioni reali).

---

## F1 — Correttezza del ring buffer (sospetto bug, priorità massima)

Codice attuale (`dataSink`, ~7260):

```cpp
if (m_mode == "FT2" && ui->cbAsyncDecode->isChecked() && k > 0) {
  int nsamples = qMin(k, 90000);
  int src_start = qMax(0, k - nsamples);
  for (int i = 0; i < nsamples; i++) {
    m_asyncAudio[m_asyncAudioPos % 90000] = dec_data.d2[src_start + i];
    m_asyncAudioPos++;
  }
}
```

`k` è il numero **cumulativo** di campioni in `dec_data.d2` dall'inizio del periodo (lo conferma il codice dello snapshot FT8 poco sopra, ~7236-7253, che copia solo i campioni nuovi). Qui invece a **ogni** callback si riaccodano **tutti** i campioni del periodo fin lì. Il ring diventa una sequenza di prefissi ripetuti `[0..k1][0..k2][0..k3]…`, e la finestra degli ultimi 45 000 campioni contiene l'audio del periodo corrente più un pezzo di copia precedente, con una discontinuità. Una trasmissione a cavallo del confine di periodo verrebbe spezzata.

Compiti:
1. **Prima dimostrarlo** con un test: alimentare `dataSink` (o la logica estratta) con una rampa nota a blocchi della dimensione reale e verificare che il ring sia una copia contigua dell'ingresso. Se il test passa, il sospetto è sbagliato: documentare perché e chiudere il compito.
2. Se il bug c'è: copia incrementale con `m_asyncLastK`, sul modello dello snapshot FT8; al cambio di periodo (k che riparte) copiare prima la coda del periodo precedente fino alla sua fine reale, poi i nuovi campioni.
3. Tenere il **tempo assoluto** di ogni campione nel ring (indice campione globale e UTC di riferimento): serve a F2-F5.

**Accettazione.** Test del ring verde; sul banco F0, nessuna perdita di trasmissioni a cavallo dei confini di periodo; latenza invariata o migliore.

> Attenzione: se F1 cambia davvero l'audio visto dal decoder, cambia anche il comportamento di `asyncConfirmDecode` e della dedup. Rimisurare la linea di base dopo F1 e usare quella come riferimento per le fasi successive.

---

## F2 — Registro delle trasmissioni al posto della dedup testuale

Tecnica JTTY: ogni frame decodificato ha un'impronta **(frequenza, istante assoluto della sync)**. Due decode sono lo stesso frame se differiscono di meno di ~3 Hz e ~50 ms; il testo non basta e non serve.

Compiti:
1. `Ft2AsyncRegistry`: voci `{t_sync_abs, f, bits77, tones, snr, qualità, prima_visualizzazione}`, scadenza dopo ~2 finestre.
2. Il decoder restituisce anche `ibest` convertito in **tempo assoluto** (oggi `xdt` è relativo alla finestra).
3. Dedup: stesso frame se |Δf| < 3 Hz e |Δt| < 50 ms (tolleranze da ritarare sul banco per FT2). Una trasmissione nuova con lo stesso testo 3 s dopo **non** è un duplicato.
4. Sostituire la conferma doppia con una conferma basata su **evidenza indipendente**, dietro `DECODIUM_FT2_ASYNC_REGISTRO=1`:
   - decode con qualità sufficiente (`qual`, `nharderror`, `dmin`, già calcolati ~4046) → mostrato subito;
   - decode debole → mostrato se confermato da una **trasmissione diversa** (altro `t_sync`) oppure dal contesto (è la risposta attesa al nostro messaggio, vedi F5).

**Accettazione.** Sul banco: zero ripetizioni legittime perse, duplicati ≤ linea di base, falsi per ora ≤ linea di base, latenza dei decode deboli ridotta.

---

## F3 — Finestre incrementali: cercare solo il tempo nuovo

Tecnica JTTY: la finestra avanza di una frazione di frame e in ogni finestra si cercano **solo i ritardi nuovi**; ciò che è già stato decodificato non si ricerca, si sottrae.

Compiti:
1. Tenere un **watermark** in tempo assoluto: fino a quale istante di inizio frame la ricerca è già stata fatta.
2. A ogni tick cercare solo gli inizi frame i cui 103 simboli sono appena diventati completi, cioè `t_inizio ∈ (watermark, adesso − 2.47 s]`, più un piccolo margine di sovrapposizione (±20 ms) contro gli errori di stima. Tradotto in `ibmin/ibmax` per il path async, lasciando intatto il path di slot.
3. Prima della ricerca, **sottrarre dalla finestra tutte le trasmissioni del registro** che vi cadono, usando i toni ricodificati e la stima del guadagno complesso con passa-basso (come `subtract_jtty` di WSJT-X o la sottrazione FT2 già presente nel decoder).
4. Con la CPU liberata, provare `ndepth` 3 (OSD) nel path async: `embedded_ft2_async_live_depth()`, flag `DECODIUM_FT2_ASYNC_INCREMENTALE=1`.

**Accettazione.** CPU per tick ridotta di almeno 5 volte a parità di configurazione; con OSD attivo, soglia 50% migliore della linea di base; nessun tick saltato; latenza ≤ linea di base + 100 ms.

---

## F4 — Retro-sweep dopo la sottrazione

Tecnica JTTY: quando si trova e sottrae un segnale, si rifanno le finestre precedenti che lo contenevano, perché un segnale più debole sovrapposto può emergere solo dopo la sottrazione. Non ricorsivo, con un tetto.

Compiti:
1. A ogni nuova voce del registro, rieseguire la ricerca sull'intervallo di inizi frame che si sovrapponeva a essa (al massimo quello dei ~2.47 s precedenti), con il segnale sottratto.
2. Tetto di CPU configurabile (numero massimo di retro-sweep per tick, priorità ai segnali più forti).
3. I decode trovati così passano dalle stesse regole di F2.

**Accettazione.** Sul banco con trasmissioni sovrapposte (Δf < 50 Hz, Δt < 1 s): più segnali deboli recuperati, falsi invariati, CPU entro il tetto.

---

## F5 — Candidato atteso **nel tempo**, non solo in frequenza

Oggi (~3190-3330) i candidati attesi sono forzati a una **frequenza** nota e poi si cerca il DT su tutto l'intervallo. In ASYMX però si sa **quando** il corrispondente risponderà: dopo la fine della nostra trasmissione, più la sua latenza di decodifica, più ~300 ms di latenza TX di ASYMX.

Tecnica JTTY corrispondente: lo "sticky retry", cioè provare la decodifica direttamente nel punto di tempo e frequenza atteso, senza gate di sync.

Compiti:
1. Registrare l'istante di fine di ogni nostra TX (`t_fine_tx`).
2. Stimare per ogni corrispondente la latenza di risposta `Δ = t_inizio_risposta − t_fine_tx` (media e dispersione, con un valore iniziale misurato sul banco e sulle registrazioni reali).
3. Per il corrispondente del QSO in corso: candidato atteso a f nota con `ibest` ristretto a `t_fine_tx + Δ ± 3σ`, esente dai gate di sync, combinato con l'AP del messaggio atteso (hiscall/mycall, già in `build_ap_setup`).
4. Stringere la finestra temporale riduce le ipotesi, quindi rende più sicura l'esenzione dai gate. Misurare i falsi con il corrispondente **assente** (rumore puro nella finestra attesa).

**Accettazione.** Nel QSO simulato (`tests/test_ft2_qso_sim.cpp` esteso): soglia 50% delle risposte attese migliore di almeno 1 dB rispetto alla linea di base; falsi con corrispondente assente ≤ 1 ogni 1000 finestre attese.

---

## F6 — Gate di qualità e contabilità dei falsi

JTTY non ha soglie oltre CRC e grammatica, e con molte ipotesi per candidato produce ~1.6·10⁻³ falsi per candidato di rumore. In JTTY un gate sulla metrica normalizzata li ha ridotti di 5-26 volte con perdite dello 0.2-3% a soglia (misura preliminare). Decodium FT2 ha già `qual`, `nharderror`, `dmin` e soglie diverse per tipo AP (~4050), ma le esenzioni di F2, F3 ed F5 aumentano le ipotesi.

Compiti:
1. Contatore di "budget falsi" per ogni sorgente di ipotesi (normale, OSD, atteso in frequenza, atteso nel tempo, retro-sweep, accumulo), misurato sul banco con solo rumore.
2. Soglie per sorgente, ritarate in modo che il totale dei falsi per ora non superi la linea di base.
3. Riportare una tabella: sorgente → decode corretti in più → falsi in più.

**Accettazione.** Falsi totali per ora ≤ linea di base con tutte le fasi attive.

---

## F7 — Concorrenza (dopo, se serve)

`decodeAsync()` e il decode di slot girano entrambi sotto `fortran::runtime_mutex()`, quindi sono serializzati. Verificare quali chiamate nel path di `decode_ft2_stage7` siano ancora Fortran (`ftx_getcandidates2_c`, `ftx_sync2d_c`, …). Se il path async diventa interamente C++ senza stato globale (oggi c'è `stage7_state()`), può girare in parallelo al path di slot. Da fare solo se il banco mostra tick saltati per attesa sul mutex.

---

## Ordine e deliverable

| Fase | Deliverable | Dipende da |
|---|---|---|
| F0 | banco async + linea di base (tabella numeri) | — |
| F1 | test ring + fix se necessario + nuova linea di base | F0 |
| F2 | registro + nuova dedup/conferma | F1 |
| F3 | ricerca incrementale + sottrazione + OSD live | F2 |
| F4 | retro-sweep con tetto | F3 |
| F5 | atteso nel tempo per ASYMX | F2 |
| F6 | budget falsi e soglie | F3, F4, F5 |
| F7 | parallelismo | facoltativa |

Alla fine di ogni fase: aggiornare questo file con i numeri misurati (sezione "Risultati"), e solo allora proporre di accendere il flag di default.

---

## Riferimenti JTTY (WSJT-X 3.2.0-rc1, `lib/jtty/`)

| Tecnica | File | Parametri JTTY (frame 1.888 s) |
|---|---|---|
| Finestre scorrevoli | `rjtty_sub.f90`, `jtty_mdecode.f90` | finestra 1.25 frame, passo ¼ frame, ricerca solo ¼ frame di ritardi |
| Registro e dedup | `jtty_mdecode.f90` (`same_frame`, `is_recent_frame`) | 3 Hz / 50 ms stesso frame; 12 Hz / 50 ms nella cronologia |
| Sottrazione | `subtract_jtty.f90` | toni ricodificati, guadagno complesso con passa-basso cos² di ~2 simboli |
| Retro-sweep | `jtty_mdecode_step` | 3 finestre precedenti, fino a 16 segnali, non ricorsivo |
| Sticky retry | `process_channel` | un periodo di frame dopo ±0.1 s, senza gate di sync |
| Continuità dei messaggi | `classify_active_candidate` | Δt ≈ k·frame (k ≤ 3) ±0.1 s, \|Δf\| < 10 + 3(k−1) Hz |

Documentazione completa nel pacchetto `jtty_decodium` (file `JTTY_03_ricevitore.md`).

---

## Correzione alla mappa del codice (25/09/2026)

La tabella della sezione 1 descrive il **backend legacy** (`widgets/mainwindow.cpp`). Nell'app QML
l'FT2 asincrono gira invece nel **bridge** — il log lo conferma: `onFt2AsyncDecodeReady summary`,
`legacyTap=0`, `legacy_pcm=0`:

| Cosa | Percorso vivo |
|---|---|
| Ring 90 000 campioni | `DecodiumBridge` (`m_asyncAudio`), scritto **campione per campione** dal callback audio: contiguo |
| Timer 100 ms, finestra 45 000 campioni | `DecodiumBridge` (dispatch vicino a `m_ft2AsyncDispatchSlotStartMs`) |
| Profondita' | quella dell'utente (`effectiveDecodeDepth`), non limitata a 2; 2 solo in TX |
| Doppioni | chiave (slot di 3,75 s calcolato al dispatch, frequenza a gruppi di 20 Hz, testo) |
| Conferma doppia dei deboli | **non esiste** nel bridge (solo nel legacy) |
| Scadenza | cancellazione cooperativa a 2,5 s (`Ft2AsyncDecodeDeadlineMs`) |

Il sospetto di F1 e' fondato ma riguarda solo il legacy: vedi sotto.

## Strumenti

- `tests/ft2_async_bench.cpp` (`ft2_async_bench gen|run`): scene lunghe con trasmissioni a istanti
  casuali, SNR in 2500 Hz, ripetizioni legittime; simulazione del percorso vivo con i tempi di CPU
  misurati (i tick che cadono mentre il worker lavora si perdono, come nell'app). Opzioni
  `--registro=1` (F2), `--incr=1` (F3), `--depth=N`, `--rows=file`.
- `tests/ft2_async_ring_test.cpp` (ctest): il ring del legacy deve essere contiguo.

## Risultati

Scena L1: 600 s, 240 trasmissioni da -22 a -6 dB (15-26 per dB), 37 ripetizioni legittime dello
stesso messaggio 2,6-4 s dopo. Scena N1: 600 s di solo rumore. Ryzen 9 5950X, `mycall=IU8LMC`,
banda 200-4000 Hz. Soglia = interpolazione al 50% della curva P(decode).

| Variante | Decodificate | Soglia 50% | Doppioni mostrati | Ripetizioni perse | Latenza media / p95 | CPU per giro media / p95 | Tick saltati | Falsi (N1) |
|---|---|---|---|---|---|---|---|---|
| base (depth 3) | 120 | -14,4 dB | 26 | 7 | 1,25 / 2,16 s | 663 / 1121 ms | 85 % | 0 |
| F2 registro | 124 | -14,4 dB | **0** | 3 | 1,17 / 1,79 s | 654 / 1081 ms | 85 % | — |
| F2 + depth 4 | 124 | -14,4 dB | 0 | 1 | 1,12 / 1,56 s | 640 / 1075 ms | 85 % | — |
| F2+F3 incrementale | 118 | -14,2 dB | 0 | 1 | **0,51 / 0,78 s** | **203** / 336 ms | 59 % | 0 |
| F2+F3 + depth 4 | 119 | -14,2 dB | 0 | 1 | 0,48 / 0,60 s | 200 / 327 ms | 59 % | — |

Letture:

- **F1**: nel legacy il vecchio ciclo copiava nel ring 13,5 volte l'audio vero, con fino a 6
  discontinuita' nella finestra; corretto (`widgets/Ft2AsyncRing.hpp`). Nel bridge il problema non c'e'.
- **F2**: la chiave per slot mostrava due volte il 22 % delle trasmissioni (la stessa finestra cade
  in due slot, o la frequenza stimata attraversa un gruppo di 20 Hz). Il registro per inizio assoluto
  (+/-0,6 s) e frequenza (+/-10 Hz) li azzera, e lascia visibili piu' ripetizioni legittime.
- **F3**: CPU per giro divisa per 3,3 (non 5), latenza divisa per 2,5, stessi falsi; costa 0,2 dB e
  ~2 % delle decodifiche vicino alla soglia: ogni frame viene provato una sola volta, da completo,
  invece che 2-3 volte da finestre diverse.
- **depth 4** non cambia niente: la curva ha un gradino fra -15 dB (0-1 su 12) e -14 dB (9-12 su 15)
  che non viene dall'OSD ma dai cancelli di sincronismo/candidati. E' li' che si guadagnano dB:
  F5 (atteso nel tempo) e F6 (soglie per sorgente).

### F5 e F6 (25/09/2026 sera)

Scena Q1: 900 s di QSO ASYMX simulati, 132 scambi. Dopo ogni nostra trasmissione
il corrispondente risponde con una latenza fra 0,3 e 0,9 s, a una frequenza nota,
con un messaggio diretto a noi (`IU8LMC <call> -NN / R-NN / RR73`). Le SNR vanno
da -24 a -10 dB; 108 risposte presenti, 24 assenti. Scena Q0: un'ora, 528 attese
**senza** risposta, per contare i falsi. In QSO il banco fa come l'app: RX sulla
frequenza del corrispondente, hiscall impostato, progresso del QSO 3 (AP con
mycall+hiscall).

| Variante | Risposte decodificate | Soglia 50% | Falsi (Q1) | Falsi su 528 attese vuote (Q0) |
|---|---|---|---|---|
| senza AP del QSO (progresso 0, RX a 1500 Hz) | 28 | -14,8 dB | 0 | — |
| base come l'app (AP mycall+hiscall) | 45 | -16,0 dB | 0 | — |
| **F5** risposta attesa nel tempo | **47** | **-16,6 dB** | 0 | **0** |
| F5 + F6 (limite errori duri 60 per l'atteso che nomina noi e lui) | 47 | -16,6 dB | 0 | — |

Letture:

- **F5** guadagna 0,6 dB e nessun falso in un'ora di attese vuote: il candidato
  forzato a frequenza e tempo noti, esente dai cancelli, recupera le risposte che
  la ricerca normale non aggancia. Resta sotto il +1 dB chiesto dall'accettazione,
  quindi e' nel codice ma **spento** (`DECODIUM_FT2_ASYNC_ATTESO=1` lo accende nel
  bridge, dopo la fine di ogni nostra TX: 0,2-1,0 s, sulla frequenza RX).
- **F6** misurato sul punto dove ci si aspettava un guadagno: alzare il limite
  degli errori duri per la risposta attesa non cambia nulla, perche' sotto
  -16 dB non e' quel cancello a scartare, e' l'LDPC a non convergere. Tolto.
  Contabilita' dei falsi per sorgente: nessuna sorgente (normale, OSD, AP del QSO,
  atteso nel tempo) ha prodotto falsi in 1 ora e 25 minuti di attese vuote e 10
  minuti di rumore puro.
- Il gradino fra -16 e -14 dB e' vicino al limite fisico della forma d'onda:
  FT2 va a 41,7 baud, quasi sette volte l'FT8, cioe' circa 8 dB in meno di
  sensibilita' a parita' di codice. Da qui in poi i dB si prendono solo con piu'
  energia per bit (accumulo fra ripetizioni, AP del messaggio intero), non con i
  cancelli.

Stato: F2 e' **acceso di default** (spegnibile con `DECODIUM_FT2_ASYNC_REGISTRO=0`); F3 resta
dietro `DECODIUM_FT2_ASYNC_INCREMENTALE=1` e F5 dietro `DECODIUM_FT2_ASYNC_ATTESO=1`, spenti.

### F4: sovrapposizioni e sottrazione (26/09/2026)

Scena genpair (`ft2_async_bench genpair`, seme 41): 149 coppie in 900 s, un
debole a -16..-10 dB e un forte a -8..-2 dB, a 5-50 Hz e fino a +-1 s. Lo stesso
rumore e gli stessi deboli sono anche in un file senza i forti; il controllo
(`--dfmin=250 --dfmax=400`) mette i forti lontani.

| Percorso vivo, registro acceso | Deboli presi (su 149) | Soglia 50% |
|---|---|---|
| deboli da soli | 111 | -14,5 dB |
| forte lontano 250-400 Hz (controllo) | 110 | -14,2 dB |
| forte accanto, sottrazione classica (filtro 700) | 78 | -12,9 dB |
| + F4 avanti (toglie prima i noti tagliati dalla finestra) | 76 | -12,8 dB |
| + F4 avanti e retro sweep | 76 | -12,8 dB |
| filtro di sottrazione 1400 | 90 | -13,6 dB |
| **filtro di sottrazione 2000 (nuovo default)** | **95** | **-13,8 dB** |
| filtro 2000 + F4 avanti | 97 | -13,7 dB |
| filtro 2800 | 98 | -14,0 dB |
| filtro 2800 + F4 avanti | 103 | — |

I forti sono presi 147-149 su 149 in ogni variante; nessun falso, nessun doppione.

Letture:

- La perdita e' della sovrapposizione in frequenza (FT2 e' largo ~167 Hz): col
  forte lontano non si perde niente.
- Il retro sweep di JTTY non serve all'FT2 asincrono: 149 finestre rifatte, 1
  riga nuova. Il debole ha di solito una sola finestra viva che lo contiene
  intero (un giro ogni ~0,7 s, 1,28 s utili) e in 18 casi persi su 35 in quella
  finestra il forte era intero: il decoder lo decodificava e lo sottraeva, ma la
  sottrazione si portava via anche il debole.
- La sottrazione stima ampiezza e fase del forte con un filtro di 700 campioni
  (58 ms, ~17 Hz di banda). Sottraendo un forte 10 dB sopra, il debole a 5-50 Hz
  resta sporco a -8,7 dB della sua energia (`ft2_async_bench danno`); con 2000
  campioni a -12,9 dB, con 2800 a -14,1. Il residuo del forte da solo passa da
  -28,6 a -30,0 dB.
- Il prezzo del filtro lungo e' la deriva del forte (`ft2_async_bench deriva`):
  residuo -27,9 dB a 1 Hz/s (-28,6 col filtro classico), -23,1 dB a 2 Hz/s
  (-28,3), -14,6 dB a 4 Hz/s (-26,4). 2000 e' il compromesso: 2800 prende 3
  deboli in piu' ma a 2 Hz/s scende a -19 dB.
- Scena L1 (240 trasmissioni sparse): 123-124 prese con ogni variante, soglia
  -14,4 dB invariata. Scena QSO Q1 con l'AP: 45 risposte, -16,0 dB, identica.
  Rumore puro: 0 righe.

Stato: filtro di sottrazione FT2 a 2000 campioni **di default** (con la
correzione ai bordi del frame, come FT8); `DECODIUM_FT2_SUB_NFILT=700` torna al
classico. F4 avanti nel codice ma **spento** (`DECODIUM_FT2_ASYNC_AVANTI=1`):
+2 deboli col filtro 2000 e +5 col 2800, al limite della variabilita' dei
tempi del banco: da riprovare in aria o su una scena piu' lunga. Retro sweep tolto.
