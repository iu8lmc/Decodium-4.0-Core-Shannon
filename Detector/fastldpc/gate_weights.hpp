// gate_weights.hpp — pesi del gate appreso (strato 2, FASTLDPC-AI-SPEC-001 §2).
// Tabelle separate per FT2 e FT8: stesso decoder (Ft2Decoder), canali e tipi
// di messaggio diversi -- vedi gate.hpp per come vengono scelte.
//
// --- FT2: riaddestrati il 5 settembre 2026 su LLR REALI del decoder di
// produzione (alpha 0,578, ntau 13, span2 64, pair_span 64 — la
// configurazione attuale, non quella del pacchetto di ricerca originale),
// raccolti con tests/ft2_gate_dump.cpp: WAV FT2 con messaggio noto attraverso
// la catena di decodifica vera (sync, demod, LDPC), non il canale AWGN
// sintetico di train/ft2chan.py. Dataset: 8 messaggi (CQ, rapporto,
// R-report, RR73, 73, due nominativi diversi), SNR da -24 a -8 dB, 7350
// prove, 317 202 candidati OSD esaminati, di cui 1 717 genuinamente veri
// (il resto rumore che supera comunque la CRC-14 — e' il problema per cui il
// gate esiste). Split 80/20, soglia scelta per <=0,5 falsi per mille sul
// training.
//
// Risultato misurato sul 20% tenuto da parte (mai visto in training):
//   solo nd<=0,065 (oggi, senza gate): 92,60% veri accettati, 57,89 per mille falsi
//   con questo gate:                   90,00% veri accettati,  0,65 per mille falsi
// Cioe' ~89 volte meno candidati fantasma per un costo di 2,6 punti di
// sensibilita'. Train e held-out sono coerenti (90,88%/0,50 per mille contro
// 90,00%/0,65 per mille). Confermato su ~13 minuti di confronto controllato
// in aria (Decodium/decodifiche per ciclo invariato spegnendo/accendendo il
// gate, 5 settembre 2026 sera) -- ma quel confronto era su traffico FT8, non
// FT2 (vedi sotto perche' questo file adesso separa i due modi).
// Per riaddestrare: Detector/fastldpc/lab/neural/gate/train_gate.py.
static const float GATE_W_FT2[10] = {-0.419673f, -0.581239f, -0.205885f, -0.043541f, 0.299545f, 0.000000f, -0.332447f, -0.486771f, 0.178712f, 0.000000f};
static const float GATE_B_FT2 = -7.146039f;
static const float GATE_MU_FT2[10] = {0.102884f, 0.230003f, 0.025739f, 0.017413f, 1.949061f, 1.000000f, 0.264674f, 0.067119f, 0.970600f, 1.000000f};
static const float GATE_SD_FT2[10] = {0.028520f, 0.028952f, 0.010620f, 0.026249f, 0.494642f, 0.000001f, 0.066366f, 0.031916f, 0.063212f, 0.000001f};
static const float GATE_THRESHOLD_FT2 = -4.125135f;

// --- FT8: riaddestrati il 5 settembre 2026 su LLR REALI, stesso metodo di
// FT2 ma con tests/ft8_gate_dump.cpp: FT8 non scrambla il messaggio prima
// dell'LDPC, quindi la verita' viene direttamente da
// ftx_encode_ft8_candidate_c (niente vettore di scrambling da replicare).
// Dataset: stessi 8 messaggi di FT2, SNR da -22 a -8 dB, 3000 prove,
// 952 582 candidati OSD esaminati, di cui 4 395 genuinamente veri. Split
// 80/20, soglia scelta per <=0,5 falsi per mille sul training.
//
// Risultato misurato sul 20% tenuto da parte (mai visto in training):
//   solo nd<=0,065 (oggi, senza gate): 88,23% veri accettati,  7,57 per mille falsi
//   con questo gate:                   91,04% veri accettati,  0,46 per mille falsi
// Qui il gate non e' solo un compromesso: alza la sensibilita' (88,23% ->
// 91,04%) E abbassa i falsi di ~16x insieme, a differenza di FT2 dove il
// guadagno sui falsi costava un po' di sensibilita'. Train ed eval coerenti
// (92,55%/0,50 per mille contro 91,04%/0,46 per mille).
// Per riaddestrare: Detector/fastldpc/lab/neural/gate/train_gate.py.
static const float GATE_W_FT8[10] = {-0.346551f, -0.081185f, -0.079960f, 0.053433f, 0.493541f, 0.000000f, -0.332220f, -0.650949f, 0.000000f, 0.000000f};
static const float GATE_B_FT8 = -7.126093f;
static const float GATE_MU_FT8[10] = {0.108834f, 0.226586f, 0.028659f, 0.027788f, 2.227563f, 1.000000f, 0.257168f, 0.086401f, 1.000000f, 1.000000f};
static const float GATE_SD_FT8[10] = {0.022140f, 0.027959f, 0.011783f, 0.037801f, 0.184993f, 0.000001f, 0.061984f, 0.017733f, 0.000001f, 0.000001f};
static const float GATE_THRESHOLD_FT8 = -4.168874f;
