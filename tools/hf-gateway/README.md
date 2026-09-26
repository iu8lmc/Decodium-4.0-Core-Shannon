# HF Gateway — radio HF via internet per FT2/FT2-Link

Tool standalone che **emula il percorso audio di una radio HF via internet**:
due stazioni Decodium (es. IU8LMC ↔ elisir80) si collegano punto-punto in UDP
e sperimentano FT2/FT2-Link **senza dipendere dalla propagazione**, con
simulazione opzionale del canale HF (rumore, QSB, attenuazione, offset).

```
[Decodium A]                                          [Decodium B]
 TX → cavo virt. 1 → gateway ═══ UDP/internet ═══ gateway → cavo virt. 2 → RX
 RX ← cavo virt. 2 ← gateway ═══ UDP/internet ═══ gateway ← cavo virt. 1 ← TX
```

- **PCM raw 48 kHz int16 mono** (~790 kbit/s per direzione): niente codec —
  Discord/Zoom/Skype hanno AGC e compressione che distruggono i modi digitali.
- 48 kHz = stesso sample rate del path audio di Decodium (richiesto per
  W500/W2300 dal 1.0.473).
- Latenza internet tipica 30–100 ms: irrilevante per i DT di FT2/FT2-Link.
- Il canale HF simulato è **spento di default** (pass-through pulito).

## Installazione

Serve Python ≥ 3.10 su entrambi i lati.

```bash
pip install -r requirements.txt        # sounddevice + numpy (scipy opzionale)
```

### Cavi audio virtuali

- **Windows**: [VB-Cable](https://vb-audio.com/Cable/) — servono **due** cavi
  (A+B, es. il pacchetto VB-Cable A+B). Chi ha già i cavi del loopback
  Decodium (ALPHA/BRAVO) può riusarli.
- **macOS**: [BlackHole 2ch](https://existential.audio/blackhole/) — installare
  **due istanze** (il sito spiega come clonare con nome diverso), oppure usare
  la configurazione già validata per il loopback 1.0.473.

Configurazione Decodium (profilo dedicato consigliato, es. "GATEWAY"):
- **Output audio (TX)** → cavo virtuale 1
- **Input audio (RX)** ← cavo virtuale 2
- CAT: **None** (non serve una radio; la frequenza è solo cosmetica)

### Collegamento tra i due gateway (NAT)

Consigliato: **[Tailscale](https://tailscale.com/)** (gratuito) su entrambi i PC.
Crea una VPN privata con IP fissi (100.x.y.z) **senza aprire porte sul router**,
cifrata. In alternativa: port-forwarding UDP manuale sul router di uno dei due.

## Uso

1. Elenca i dispositivi audio e individua i nomi dei cavi:
   ```bash
   python hf_gateway.py --list-devices
   ```

2. Avvia il gateway su **entrambi** i lati (IP Tailscale del peer):

   **IU8LMC (Windows):**
   ```bash
   python hf_gateway.py --in-device "CABLE-A Output" --out-device "CABLE-B Input" ^
                        --peer 100.x.y.z:5550 --listen 5550
   ```

   **elisir80 (macOS):**
   ```bash
   python3 hf_gateway.py --in-device "BlackHole 2ch" --out-device "BlackHole2 2ch" \
                         --peer 100.a.b.c:5550 --listen 5550
   ```

3. **Primo collaudo senza Decodium**: un lato aggiunge `--tone-test`
   (trasmette un tono a 1500 Hz); l'altro lato deve vedere `out` salire di
   livello nelle statistiche e sentire/vedere il tono sul waterfall di
   Decodium. Poi si toglie il flag e si fa il primo QSO FT2.

Le statistiche (ogni 2 s) mostrano: frame tx/rx, persi, ritardo, underrun,
**RTT**, e livelli **in/out in dBFS** (per regolare i livelli audio).

## Simulazione canale HF (opzionale)

Applicata al segnale **ricevuto**, prima di consegnarlo a Decodium.
Ogni lato controlla il "suo" canale in ricezione.

| Flag | Effetto |
|------|---------|
| `--noise-dbfs -30` | pavimento di rumore AWGN a −30 dBFS |
| `--attenuate-db 20` | attenua il segnale ricevuto di 20 dB (abbassa l'SNR) |
| `--qsb-depth-db 6 --qsb-period 20` | fading sinusoidale 6 dB picco-picco ogni 20 s |
| `--freq-offset 15` | sposta il segnale di +15 Hz (richiede `pip install scipy`) |

Esempio "banda 20 m in cattive condizioni" per testare W2300 WEAK/DEEP:

```bash
python hf_gateway.py ... --noise-dbfs -25 --attenuate-db 25 --qsb-depth-db 10 --qsb-period 15
```

L'SNR effettivo si regola combinando attenuazione del segnale e livello del
rumore; i livelli misurati sono nelle statistiche. Per prove *pulite* di
protocollo (QSY, BBS, mail) lasciare tutto spento.

## Risoluzione problemi

- **Nessun audio ricevuto**: controlla firewall (consenti UDP sulla porta di
  `--listen`), che i due `--peer` puntino l'uno all'altro, e l'RTT nelle
  statistiche (se resta `-1` i pacchetti non passano).
- **Audio a scatti / underrun**: alza `--jitter-ms` (150–250 su collegamenti
  instabili).
- **`frame supera l'MTU`**: lascia `--frame-ms 10` (default) a 48 kHz.
- **Decodium non decodifica**: verifica 48 kHz su tutta la catena (cavi
  virtuali configurabili a 48000 Hz nel pannello audio di Windows) e i
  livelli: `in` del lato TX intorno a −20…−10 dBFS senza clipping.
- **Entrambi trasmettono insieme**: collisione — realistico, come in HF
  simplex. FT2-Link ritenta da solo (HELLO retry).

## Alternativa zero-code (solo trasporto, senza canale HF)

Per un test rapido senza questo tool: **VBAN/Voicemeeter** (solo Windows↔
Windows) oppure **JackTrip** (Windows↔macOS) trasportano audio PCM via rete.
Nessuna simulazione canale e più configurazione manuale; con Tailscale
funzionano entrambi. Questo gateway resta la via consigliata perché è un
comando solo, cross-platform, con statistiche e canale HF integrati.
