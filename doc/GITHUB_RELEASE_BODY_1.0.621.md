# Decodium 4 FT2 v1.0.621

## English (UK)

### Changes since v1.0.620

#### FT2: the plausibility filter was reading the scrambled payload

The decoder's plausibility check sits inside the OSD acceptance loop and reads
the 77-bit payload to verify its structure: message type, callsign field
shapes, ranges of the length-limited fields. In FT2 those bits are **not** the
message — they are the message scrambled with `rvec`. FT2 scrambles before
encoding and unscrambles only after the decoder returns, so the filter was
reading a pseudo-random number and its structural checks carried no meaning:
it was discarding **correct** decodes.

This went unnoticed because of an accident of the scrambling vector: a standard
message (type 1) appears as type 4 in the scrambled domain, and type 4 is in
the mask FT2 uses, so FT2 decoded and everything looked fine. The loss was
spread thinly across all decodes rather than showing up as an obvious failure.

The filter now removes the scrambling before checking. Measured on the real
decode chain, 400 trials, identical noise seeds:

| | decodes | phantoms (per mille) |
|---|---:|---:|
| before | 275 | 0.036 |
| after | **292 (+6.2%)** | 0.034 |

The gain concentrates at the decoding threshold, where it matters most: at
−19 dB, 59 → 73 decodes (+24%). Phantom rejection is unchanged (34 against 36
on a million pure-noise words, identical within counting noise; without the
filter it is 102).

**FT8 is not affected and not changed.** FT8 does not scramble, so its filter
has always read the true message bits and worked as designed. Verified: 60
slots of real recorded traffic, 1724 decoded lines, byte-for-byte identical
before and after. The decoder is deterministic (two runs of the same binary
differ in zero lines), so that comparison is conclusive.

Measurements are on a synthetic AWGN channel with the real demodulator: no
fading, no interference, a single signal. On-air confirmation is still to come.

#### Research tooling (no effect on decoding)

- Iterative demodulation (BICM-ID) test bench for FT2, with a genie arm as
  the upper bound. Measured +6.3% decodes; not adopted, because it costs two
  extra demodulate-and-decode rounds per failed candidate.
- Benches that generate `.llr` vectors from the real 4-GFSK demodulator (every
  vector in the lab was previously synthetic), and that count true and false
  decodes on the real chain.

This release is published with the source code and platform packages built by
the GitHub Actions runners: Windows x64 executable, macOS Apple Silicon and
Intel DMGs, and Linux x86_64 and aarch64 AppImages.

## Italiano

### Modifiche dalla v1.0.620

#### FT2: il filtro di plausibilità leggeva il payload mescolato

Il controllo di plausibilità del decoder sta dentro il ciclo di accettazione
dell'OSD e legge i 77 bit del payload per verificarne la struttura: tipo di
messaggio, forma dei campi dei nominativi, intervalli dei campi a lunghezza
limitata. In FT2 quei bit **non sono** il messaggio: sono il messaggio
mescolato con `rvec`. FT2 mescola prima di codificare e rimette in chiaro solo
dopo che il decoder ha restituito la parola, quindi il filtro leggeva un numero
pseudo-casuale e i suoi controlli non avevano alcun significato: scartava
decodifiche **corrette**.

Non si era mai visto per una coincidenza del vettore di scrambling: un
messaggio standard (tipo 1) nel dominio mescolato appare come tipo 4, che è
nella lista ammessa da FT2. Il decoder funzionava e tutto sembrava a posto, e
la perdita era distribuita su tutte le decodifiche invece di presentarsi come
un guasto evidente.

Ora il filtro toglie lo scrambling prima del controllo. Misurato sulla catena
di decodifica vera, 400 prove, semi di rumore identici:

| | decodifiche | fantasmi (per mille) |
|---|---:|---:|
| prima | 275 | 0,036 |
| dopo | **292 (+6,2%)** | 0,034 |

Il guadagno si concentra alla soglia di decodifica, dove serve di più: a
−19 dB si passa da 59 a 73 decodifiche (+24%). Il potere filtrante sui
fantasmi resta invariato (34 contro 36 su un milione di parole di puro rumore,
identici entro il rumore di conteggio; senza filtro sono 102).

**FT8 non è interessato e non è cambiato.** FT8 non mescola, quindi il suo
filtro ha sempre letto i bit veri del messaggio e ha sempre fatto il suo
mestiere. Verificato: 60 slot di traffico reale registrato, 1724 righe
decodificate, identiche byte per byte prima e dopo. Il decoder è deterministico
(due esecuzioni dello stesso binario danno zero differenze), quindi il
confronto è conclusivo.

Le misure sono su canale AWGN sintetico con il demodulatore vero: niente
fading, niente interferenza, un solo segnale. La conferma in aria resta da
fare.

#### Strumenti di ricerca (nessun effetto sulla decodifica)

- Banco per la demodulazione iterativa (BICM-ID) di FT2, con un braccio
  "genio" come limite superiore. Misurato +6,3% di decodifiche; non adottato,
  perché costa due giri in più di demodulazione e decodifica per ogni
  candidato fallito.
- Banchi che generano vettori `.llr` dal demodulatore 4-GFSK vero (quelli del
  laboratorio erano tutti sintetici) e che contano decodifiche giuste e false
  sulla catena vera.

Questa release viene pubblicata con il codice sorgente e i pacchetti prodotti
dai runner GitHub Actions: eseguibile Windows x64, DMG macOS Apple Silicon e
Intel, AppImage Linux x86_64 e aarch64.
