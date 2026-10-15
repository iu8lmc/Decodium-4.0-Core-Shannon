# Protocollo SPE Expert — lettura della telemetria

Fonte: **Application Programmer's Guide** rev. 1.1 di SPE, per Expert 1.3K-FA,
1.5K-FA e 2K-FA, scaricabile da
<https://www.spetlc.com/images/download/SPE_Application_Programmers_Guide.pdf>.
Estratto e verificato dal documento originale, non riassunto da terzi.

Serve a portare al DECØMETER la potenza all'uscita dell'amplificatore invece
di quella dell'eccitatrice (richiesta di PA3GYQ).

## Il punto che sblocca tutto

L'amplificatore **espone potenza in watt e ROS** su USB/RS-232, e lo stato
«può essere richiesto più volte al secondo» — cadenza più che sufficiente per
la balistica dello strumento.

⚠️ **Gli SPE Expert non parlano TCI.** Verso la radio usano il protocollo
**Kenwood**, e in quel verso si limitano a *chiedere* la frequenza per
commutare banda. Il ponte ESP32 che circola in rete
([forum Expert Electronics](https://eesdr.com/en/forum-en/connection-to-external-power-amplifiers/9400-esp32-tci-to-cat-bridge-for-spe-pa))
va in quella direzione: TCI → amplificatore, per la commutazione. Non legge la
telemetria. Per il DECØMETER serve il verso opposto, ed è quello descritto qui.

E attenzione all'omonimia: **SPE** (Roma, amplificatori) non è **Expert
Electronics** (SunSDR, autori del TCI). Nomi simili, aziende diverse.

## Richiesta di stato

```
0x55 0x55 0x55   tre byte di sincronismo
0x01             segue un solo byte
0x90             comando "Get Status"
0x90             somma di controllo (di un byte solo: il byte stesso)
```

## Risposta

```
0xAA 0xAA 0xAA   tre byte di sincronismo dall'amplificatore
0x43             67 = numero di caratteri che seguono
DATA0..DATA66    67 caratteri ASCII, valori separati da virgola
CHK0             SUM(DATA0..DATA66) % 256
CHK1             SUM(DATA0..DATA66) / 256
CR LF
```

Esempio dalla guida:

```
20K,S,R,x,1,00,1a,0r,L,0000, 0.00, 0.00, 0.0, 0.0, 33, 0, 0,N,N
```

## I diciannove campi

| # | Campo | Lung. | Contenuto |
|---|---|---|---|
| 1 | ID | 3 | `20K` per 2K-FA, `13K` per 1.3K-FA |
| 2 | Standby/Operate | 1 | `S` o `O` |
| 3 | RX/TX | 1 | `R` o `T` |
| 4 | Banco memoria | 1 | `A`/`B`, oppure `x` |
| 5 | Ingresso | 1 | `1` o `2` |
| 6 | Banda | 2 | da `00` (160 m) a `11` (4 m) |
| 7 | Antenna TX e ATU | 2 | `t` sintonizzabile, `b` ATU escluso, `a` ATU attivo |
| 8 | Antenna RX | 2 | |
| 9 | Livello potenza | 1 | `L`, `M`, `H` |
| **10** | **Potenza d'uscita** | **4** | **`0000` in RX, watt misurati in TX** |
| **11** | **ROS prima dell'ATU** | **5** | **`_0.00` in RX** |
| **12** | **ROS d'antenna** | **5** | **`_0.00` in RX** |
| 13 | V PA | 4 | tensione di alimentazione, `48.0` a piena potenza |
| 14 | I PA | 4 | corrente assorbita in TX |
| 15 | Temp. superiore | 3 | dissipatore |
| 16 | Temp. inferiore | 3 | `000` sul 1.3K-FA |
| 17 | Temp. combinatore | 3 | `000` sul 1.3K-FA |
| 18 | Avvisi | 1 | `N` = nessuno |
| 19 | Allarmi | 1 | `N` = nessuno |

Campi 10, 11 e 12 sono quelli che al DECØMETER servono. Il 3 (`R`/`T`) dice
quando la misura è valida: in ricezione la potenza è `0000` per definizione.

## Allarmi e avvisi

Vale la pena leggerli: sono la ragione per cui uno strumento va guardato.

**Allarmi** — `S` ROS oltre i limiti · `A` protezione amplificatore ·
`D` sovrapilotaggio in ingresso · `H` surriscaldamento eccessivo

**Avvisi** — `M` allarme amplificatore · `A` nessuna antenna selezionata ·
`S` ROS antenna · `B` banda non valida · `P` limite di potenza superato ·
`O` surriscaldamento · `Y` ATU non disponibile · `W` sintonia senza potenza ·
`K` ATU escluso · `R` interruttore tenuto da remoto · `T` surriscaldamento
combinatore · `C` guasto combinatore · `N` nessun avviso

## Il vincolo che resta

Una porta seriale la apre **un solo programma alla volta**. Finché il software
del costruttore tiene la USB, nessun altro può interrogare l'amplificatore.
L'apparato però ha **due porte**, USB e RS-232: quella è la via d'uscita, e va
verificata sul modello specifico.
