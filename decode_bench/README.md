# decode_bench — Banco di misura della sensibilità FT8 in dB

Misura **quanto in basso (in dB) Decodium decodifica un segnale FT8**, in modo
riproducibile e con verità di terra (segnale piantato a SNR noto). È il
prerequisito a qualsiasi lavoro sul decoder: senza misurare in dB non si
distingue un guadagno reale dal rumore di propagazione.

## La metrica che conta: la soglia in dB

L'unico numero che conta è la **soglia al 50%**: la SNR a cui decodifichi metà
dei segnali. Più bassa = più sensibile. Riferimenti tipici (AWGN):

| Decoder | Soglia FT8 |
|---|---|
| WSJT-X normale | ~ −20.5 dB |
| WSJT-X / JTDX deep | ~ −24 dB |

Il **conteggio totale dei decode NON è una metrica**: è gonfiato dai segnali
facili. Qui ogni slot contiene **un** segnale a SNR noto e si misura solo se lo
prendi o no → curva P(decode) vs SNR → soglia.

> La SNR di `ft8sim` è nella **banda di riferimento 2500 Hz**, la stessa
> convenzione che Decodium e jt9 usano per riportare la SNR. Quindi i numeri
> sono direttamente confrontabili.

## Catena di misura

```
ft8sim  →  FT8 a SNR calibrato + AWGN (N realizzazioni di rumore)
        →  decoder REALE di Decodium  (tests/ft8_stage_compare, stage4)
        →  [opzionale] jt9 MTft8       (riferimento JTDX-style)
        →  P(decode) vs SNR  →  soglia 50% in dB
```

Tutti i pezzi esistono già: `ft8sim.exe` e `jt9.exe` da WSJT-X
(`C:\WSJT\wsjtx\bin`), e `ft8_stage_compare` (target di test di Decodium che
gira lo **stage4 di produzione** su un .wav). Il banco è solo l'orchestratore.

## Prerequisiti

1. **WSJT-X** installato in `C:\WSJT\wsjtx\bin` (per `ft8sim.exe`, `jt9.exe`).
   Se è altrove, passa `-WsjtBin <path>` allo script o `--ft8sim/--jt9` a `bench.py`.
2. **Compila il decoder CLI** (una volta):
   ```
   cmake --build build_mingw64 --target ft8_stage_compare -j 6
   ```
   Produce `build_mingw64/tests/ft8_stage_compare.exe`.
3. **Python 3** (qualsiasi; usa solo la standard library).

## Uso rapido

PowerShell (consigliato — imposta da solo il PATH delle DLL MinGW/Qt):

```powershell
cd C:\decodium-4.0\decode_bench
# smoke veloce (~5 min) con confronto jt9
.\run_bench.ps1 -Quick -WithJt9 -Label smoke

# sweep serio: soglia deep su 30 realizzazioni per punto
.\run_bench.ps1 -Profile deep -Snr "-15:-25:-1" -Trials 30 -WithJt9 -Label baseline
```

Diretto con Python (ricorda il PATH delle DLL):

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
python bench.py --decodium ../build_mingw64/tests/ft8_stage_compare.exe \
  --profile deep --snr "-15:-25:-1" --trials 30 --with-jt9 --label baseline
```

## Output

Tabella per ogni SNR + soglia interpolata + delta vs jt9, e un CSV
`results_<label>.csv`. Esempio:

```
  SNR | D4 hits  P(D4)  | jt9 hits P(jt9)  |
 -15  | 30/30   1.00 ############ | 30/30   1.00 ############
 -21  | 23/30   0.77 #########... | 19/30   0.63 ########....
 -24  |  6/30   0.20 ##.......... |  9/30   0.30 ###.........
  SOGLIA Decodium (deep) : -22.40 dB
  SOGLIA jt9 (ref)       : -22.05 dB
  DELTA (jt9 - Decodium) : +0.35 dB  ->  Decodium PIU' sensibile di 0.35 dB
```

## Flusso PRIMA/DOPO una modifica al decoder

È l'uso principale: validare che una modifica al decoder **abbassi la soglia**.

```powershell
# 1) baseline sul codice attuale
.\run_bench.ps1 -Profile deep -Snr "-18:-25:-1" -Trials 40 -Label prima
# 2) applichi la modifica, ricompili ft8_stage_compare
# 3) stessa identica chiamata
.\run_bench.ps1 -Profile deep -Snr "-18:-25:-1" -Trials 40 -Label dopo
```

Confronta le due soglie. **Un guadagno reale = soglia più bassa di ≥ 0.5 dB**
in modo stabile (con 40 realizzazioni il rumore statistico sulla soglia è
~0.2–0.3 dB; sotto questa soglia non fidarti).

## Profili decoder

| Profilo | Cosa attiva | Quando |
|---|---|---|
| `prod` | depth 3, no AP, no harvest | sensibilità di base (~2 s/wav) |
| `deep` | depth 4 + AP + OSD ord.4 | **soglia di riferimento** (~8 s/wav) |
| `harvest` | deep + subpass + 3 cicli + soglie basse (il "GAL") | banda affollata (~8 s/wav) |

> ⚠️ **Su segnale singolo in AWGN, `harvest` ≈ `deep`.** Il vantaggio di harvest
> (cancellazione dell'interferenza co-canale) emerge **solo in banda affollata**,
> con più segnali sovrapposti. Per misurarlo serve la modalità **multi-segnale**
> (vedi sotto). Per la soglia pura usa `deep`.

## Le due leve e cosa misura questo banco

Dalla discussione sul "vero salto" del decoder:

- **Leva A — demod (quasi-)coerente / LLR pesati sul fading** → guadagno di
  **sensibilità su segnale singolo** → **questo banco lo misura direttamente**
  (soglia in dB, profilo `deep`). È lo strumento per validare la leva A.
- **Leva B — cancellazione interferenza / harvest** → guadagno in **banda
  affollata** → serve il banco **multi-segnale** (TODO, vedi sotto).

## Modalità multi-segnale (leva B / harvest) — `bench_multi.py`

Misura quello che il banco a segnale singolo non può: **l'apporto di harvest in
banda affollata**. Costruisce uno slot con 1 **vittima** debole sovrapposta a
1–2 **interferenti** forti a pochi Hz, e conta quanto spesso la vittima viene
recuperata da `deep` vs `harvest`. Il gap = valore della leva B.

```powershell
cd C:\decodium-4.0\decode_bench
.\run_bench.ps1 -Multi -Snr "-12:-22:-2" -Trials 15 -Label cluster
# oppure diretto:
python bench_multi.py --decodium ..\build_mingw64\tests\ft8_stage_compare.exe `
  --snr "-12:-22:-2" --trials 15 --profiles deep,harvest --with-jt9 --label cluster
```

**Come è costruita la scena (senza congetture di calibrazione):**
- vittima rumorosa = `ft8sim(msg, f0, snr_v)` → SNR assoluto + AWGN già calibrati
  da ft8sim (banda rif. 2500 Hz);
- ogni interferente = `ft8sim(msg_i, f0_i, snr=50)` → segnale quasi **pulito**
  all'ampiezza standard di ft8sim;
- combinato = vittima + Σ gᵢ·interferenteᵢ, con
  `gᵢ = 10^((snrᵢ − snr_v)/20) · (rms_vittima_pulita / rms_interfᵢ_pulito)` →
  porta ogni interferente al suo **SNR assoluto** rispetto allo stesso rumore
  della vittima (per FT8, rapporto di potenza in dB = rapporto SNR in dB);
- poi normalizza il picco per non clippare (guadagno comune su segnale+rumore →
  SNR invariati).

**Scena di default:** vittima `CQ IU8LMC JN70` @1500 Hz; **4 interferenti forti
ben separati** (1460/1525/1560/1590 Hz, +2…+5 dB, messaggi distinti auto). In
questa scena la vittima è **recuperabile** (osservato: P≈1.00 fino a −18 dB per
entrambi i profili) → è un *sanity baseline* di "banda affollata ma gestibile".
Il mascheramento dipende quasi solo dall'**offset in Hz** del vicino più stretto:
`--intf "1506:3,1517:3"` (+6 Hz, sovrappone ~44 dei 50 Hz) → vittima quasi sempre
**persa da entrambi**; offset ≥ +25 Hz → quasi sempre **recuperata da entrambi**.
La transizione è ripida e non lascia, sul sintetico, una finestra in cui deep
cede e harvest regge (vedi sotto).

**Output:** P(recupero vittima) vs SNR vittima per ogni profilo, soglia di
recupero, e l'**APPORTO HARVEST** = gap medio `P(harvest) − P(deep)` + di quanti
dB harvest abbassa la soglia. Personalizza con `--intf`, `--victim-f0`, `--intf-dt`.

### Cosa abbiamo trovato (stato attuale del decoder, 2026-06-17)

Su scene **sintetiche AWGN**, `deep` ≈ `harvest` ovunque: dove la vittima è
recuperabile la prendono entrambi, dove è mascherata la perdono entrambi
(verificato su interferente singolo, 4 ben separati, 6 fitti). Due ragioni:

1. il grosso della **qualità di sottrazione** (il DT-refine, `lrefinedt`, +8 dB
   di parità nella nota JTDX) è **già attivo in `deep`** perché gated a
   `ndepth≥4`; harvest aggiunge solo *più passate* + *soglie più basse*, che su
   segnali sintetici puliti non cambiano l'esito;
2. **caveat deadline:** con `--max-ms` stretto le passate extra di harvest
   (npass 9 vs 5) non completano → si comporta come deep. Per dare a harvest una
   chance reale, alza `--max-ms` (es. 20000+) **solo sul profilo harvest**.

→ L'apporto di harvest, se c'è, va cercato su **catture reali off-air** (bande
molto dense, fading, forme d'onda imperfette) o dopo un miglioramento della
**leva B** (cancellazione interferenza). Questo banco è **pronto a rilevare il
gap** appena un cambiamento del decoder lo apre: gap medio > 0 e soglia di
recupero più bassa per harvest = miglioria reale.

## Modalità catture reali (leva B sul campo) — `bench_real.py`

È dove l'eventuale vantaggio di harvest in **banda affollata vera** si vede:
niente sintetico, decodifica i **tuoi .wav off-air** con `deep` vs `harvest` (e
`jt9`) e confronta gli **insiemi** di decode sugli stessi file. È il metodo della
nota di parità JTDX (confronto slot-per-slot), automatizzato.

```powershell
cd C:\decodium-4.0\decode_bench
.\run_bench.ps1 -Real -Wavs "C:\percorso\save\*.wav" -Profiles deep,harvest -WithJt9 -Limit 50 -Label campo1
# diretto:
python bench_real.py --decodium ..\build_mingw64\tests\ft8_stage_compare.exe `
  --wavs "C:\percorso\save" --profiles deep,harvest --with-jt9 --limit 50 --label campo1
```

**Dove prendere i .wav.** Due strade:

*A) Registrare con Decodium* (`Record RX`):
- **Pulsante toolbar "Record RX"** (LED ●/○ rosso, tooltip "Recording: Ns") o
  **menu → "Record RX"**; oppure, non presidiato, la variabile d'ambiente
  **`DECODIUM_RECORD_RX_SECONDS=<secondi>`** (avvia 10 s dopo il lancio).
- Salva in **`Documents\Decodium\recordings\decodium_<YYYYMMDD_HHmmss-UTC>.wav`**,
  12 kHz mono 16-bit.
- ⚠️ **È UN wav CONTINUO**, non slot da 15 s. Va **affettato e allineato a UTC**
  prima del banco (i decoder assumono che il segnale inizi al confine di slot):
  ```
  py slice_recording.py --wav "C:/Users/.../Documents/Decodium/recordings/decodium_20260617_193012.wav" --out C:/tmp/slot
  py bench_real.py --decodium ../build_mingw64/tests/ft8_stage_compare.exe --wavs C:/tmp/slot --profiles deep,harvest --with-jt9
  ```
  `slice_recording.py` ricava lo start UTC dal nome, taglia fette da 180000
  campioni (15 s) allineate ai confini :00/:15/:30/:45, le nomina
  `YYMMDD_HHMMSS.wav` e scarta i parziali. (FT4: `--slot 7.5`.)

*B) Registrare con WSJT-X* (`File → Save → Save All`): scrive già slot da 15 s
allineati (`YYMMDD_HHMMSS.wav`) nella sua cartella `save` → si danno a
`bench_real.py` **senza affettare**. Comodo se vuoi confrontare sulla stessa
sessione audio.

In entrambi i casi servono `.wav` **12000 Hz mono 16-bit** da 15 s, timestamp nel nome.

**Niente verità di terra** (segnali veri, contenuto ignoto) → la misura è
**comparativa**, non una soglia in dB. Ma con CRC-14 i falsi decode sono ~1/10⁴:
i decode che harvest prende e deep no sono **quasi tutti stazioni vere**. Output:

```
  Decode unici (aggregato su tutti i file):
    deep     : 412
    harvest  : 437   (+25 vs deep)
    jt9      : 421
  harvest vs deep:  comuni=410  harvest-only=27  deep-only=2
  SNR dei harvest-only (marginali): min/mediana/max = -22/-20/-16 dB  (n=27)
  Decodium(unione) vs jt9:  comuni=400  decodium-only=37  jt9-only=21
```

- **harvest-only** = stazioni vere che harvest estrae e deep no → l'apporto reale.
- La **distribuzione SNR dei marginali** mostra se harvest scava sui deboli.
- **decodium-only / jt9-only** = parità vs il riferimento sugli stessi slot.
- CSV per drill-down: una riga per `(file, messaggio)` con chi l'ha preso e a che SNR.

> Banda piena `200..4000 Hz` di default (i segnali reali sono ovunque) → pesante:
> `--max-ms 20000` e `--limit 50` di default. Restringi `--nfa/--nfb` se conosci
> la sotto-banda attiva, alza `--limit 0` per tutti i file. Per dare a harvest la
> chance delle passate extra, prova `--max-ms 30000`.

## Parametri utili (`bench.py --help`)

- `--snr start:stop:step` (es. `-15:-25:-1`) o CSV (`-18,-20,-22`).
- `--trials N` realizzazioni di rumore per punto (più alto = soglia più stabile).
- `--fdop Hz` / `--delay ms`: 0/0 = AWGN puro (default, riproducibile);
  `--fdop 1.0 --delay 2.0` ≈ fading HF tipico.
- `--max-ms` deadline decode (default 8000; **0 = illimitata** ma molto lenta).
- `--band Hz` semi-banda attorno a f0 (default 250; non cambia la sensibilità
  sul segnale singolo, velocizza la ricerca).
- `--message` / `--f0`: messaggio piantato e frequenza audio.

## Note di accuratezza

- **Niente AP "barato":** `mycall`/`hiscall` restano vuoti, quindi l'AP usa solo
  priori generici CQ e **non** inietta il call piantato. Il decode resta cieco.
- **Stesso seed di rumore prima/dopo:** `ft8sim` rigenera rumore nuovo ad ogni
  run, quindi confronta sempre con lo **stesso `--trials`** (il rumore medio si
  annulla su N realizzazioni). Per un confronto a parità di rumore esatta,
  conserva i .wav con `--workdir <dir>` e ri-decodifica gli stessi file.
- Le passate early sintetiche 41/47 sono **saltate** (`--no-early`): non servono
  alla soglia e dimezzano i tempi. Aggiungi `--early` per includerle.
