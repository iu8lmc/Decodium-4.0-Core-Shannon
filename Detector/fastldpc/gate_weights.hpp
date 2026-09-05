// gate_weights.hpp — pesi del gate appreso (strato 2, FASTLDPC-AI-SPEC-001 §2).
//
// Riaddestrati il 5 settembre 2026 su LLR REALI del decoder di produzione
// (alpha 0,578, ntau 13, span2 64, pair_span 64 — la configurazione attuale,
// non quella del pacchetto di ricerca originale), raccolti con
// tests/ft2_gate_dump.cpp: WAV FT2 con messaggio noto attraverso la catena
// di decodifica vera (sync, demod, LDPC), non il canale AWGN sintetico di
// train/ft2chan.py. Dataset: 8 messaggi (CQ, rapporto, R-report, RR73, 73,
// due nominativi diversi), SNR da -24 a -8 dB, 7350 prove, 317 202 candidati
// OSD esaminati, di cui 1 717 genuinamente veri (il resto rumore che supera
// comunque la CRC-14 — e' il problema per cui il gate esiste). Split 80/20,
// soglia scelta per <=0,5 falsi per mille sul training.
//
// Risultato misurato sul 20% tenuto da parte (mai visto in training):
//   solo nd<=0,065 (oggi, senza gate): 92,60% veri accettati, 57,89 per mille falsi
//   con questo gate:                   90,00% veri accettati,  0,65 per mille falsi
// Cioe' ~89 volte meno candidati fantasma per un costo di 2,6 punti di
// sensibilita'. Train e held-out sono coerenti (90,88%/0,50 per mille contro
// 90,00%/0,65 per mille): a differenza del primo tentativo con 218 esempi
// veri, qui la soglia non balla piu' fra le due meta' del dataset.
//
// Il meccanismo (gate.hpp) resta a comportamento invariato quando
// DECODIUM_LDPC_GATE non e' impostato: questi pesi valgono SOLO se qualcuno
// accende esplicitamente quella variabile d'ambiente (spenta di default).
// Non ancora provato in aria su traffico reale (solo su WAV sintetico con
// AWGN attraverso il demodulatore vero): la prova in aria resta il passo
// successivo prima di proporre DECODIUM_LDPC_GATE=1 come default.
// Per riaddestrare: Detector/fastldpc/lab/neural/gate/train_gate.py.
static const float GATE_W[10] = {-0.419673f, -0.581239f, -0.205885f, -0.043541f, 0.299545f, 0.000000f, -0.332447f, -0.486771f, 0.178712f, 0.000000f};
static const float GATE_B = -7.146039f;
static const float GATE_MU[10] = {0.102884f, 0.230003f, 0.025739f, 0.017413f, 1.949061f, 1.000000f, 0.264674f, 0.067119f, 0.970600f, 1.000000f};
static const float GATE_SD[10] = {0.028520f, 0.028952f, 0.010620f, 0.026249f, 0.494642f, 0.000001f, 0.066366f, 0.031916f, 0.063212f, 0.000001f};
static const float GATE_THRESHOLD = -4.125135f;
