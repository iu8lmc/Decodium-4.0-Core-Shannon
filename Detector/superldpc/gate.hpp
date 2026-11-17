// gate.hpp — gate appreso contro le false decodifiche (SUPERLDPC-AI-SPEC-001, strato 2).
// Sostituisce la sola soglia su nd con una regressione logistica su GATE_NF feature
// del candidato che ha passato la CRC-14. Pesi in gate_weights.hpp (appresi offline).
#pragma once
#include <cmath>
#include <cstdint>

static constexpr int GATE_NF = 12;

struct GateFeatures {
    float f[GATE_NF];
    // 0 nd           distanza soft normalizzata (num/den su bit liberi)
    // 1 nhard        frazione di bit del candidato diversi dalla decisione hard di canale
    // 2 nhard_top    idem ma solo sulla meta' di bit con |LLR| piu' alto (i piu' affidabili)
    // 3 min_rel      min|LLR| / media|LLR|
    // 4 mean_abs     media |LLR| di canale (scala)
    // 5 iters        iterazioni min-sum / max_iter
    // 6 unsat        check non soddisfatti all'uscita del min-sum / M
    // 7 score        score OSD (somma |L16| dei flip) / somma |L16|
    // 8 free_ratio   bit liberi (non AP) / N
    // 9 by_osd       1 se il candidato viene dall'OSD, 0 dal min-sum
    // 10 rango       log2(1 + candidati sottoposti alla CRC-14 prima di questo)
    // 11 passaggi    log2(1 + candidati che avevano gia' passato la CRC-14)
    //
    // 10 e 11 sono il "giudice consapevole del rango" (idea di SO-GRAND): un
    // candidato trovato dopo 18000 tentativi, con altri che avevano gia'
    // passato la CRC, e' statisticamente un'altra cosa da uno trovato al terzo.
    // La CRC ammette un falso ogni 16384 candidati: il rango dice quanti
    // dadi si sono tirati. E' la grandezza strutturale del README §5, che
    // finora il gate non vedeva. Entrambi zero per i candidati del min-sum.
};

// FT2 e FT8 condividono il decoder (decodium_bridge.cpp, Ft2Decoder), ma NON
// il canale: rumore, tipi di messaggio ammessi e distribuzione delle feature
// sono diversi. I due modi hanno pesi separati (GATE_*_FT2 / GATE_*_FT8 in
// gate_weights.hpp); il parametro ft8 sceglie quale tabella usare, deciso a
// costruzione del decoder (Ft2Config::gate_is_ft8, vedi decodium_bridge.cpp).
// Scostamento della soglia del classificatore a runtime, impostato da
// decodium_bridge.cpp (DECODIUM_LDPC_GATE_DELTA). 0 = soglia appresa,
// bit-identico a prima; negativo accetta di piu' (recupera candidati veri
// deboli, lascia passare piu' falsi), positivo il contrario. Manopola di
// misura (lab/misure/20260908_fantasmi_ft8.md), non un default.
inline float& gate_threshold_delta() {
    static float v = 0.0f;
    return v;
}

// ---------------------------------------------------------------------------
// Gate a margine, per la ricerca sul codice concatenato (174,77)
// (OsdFast::crc_in_base). Li' ogni candidato e' una parola valida, quindi la
// CRC non dice piu' niente e "ha passato la CRC" non e' piu' un evento raro:
// l'OSD restituisce SEMPRE la parola plausibile piu' vicina fra le migliaia
// provate. La domanda giusta diventa quanto quella parola e' piu' vicina
// della sua concorrente, cioe' della migliore fra tutte le altre.
//
// Sul rumore le parole di codice piu' vicine stanno tutte alla stessa
// distanza (nd ~ 0,10 la prima, ~ 0,10 la seconda, e una volta su due la
// concorrente e' addirittura piu' vicina, perche' la migliore in assoluto non
// era un messaggio plausibile); su un segnale vero la prima si stacca
// (nd ~ 0,05 contro ~ 0,10). La statistica e'
//
//     z = nd - c_gap * clip(nd2 - nd, +-gap_cap)
//            + c_cnt * (log2(1 + provati) - 11) - w_logit * logit
//
// e si accetta se z <= thr. `logit` e' l'uscita del gate appreso qui sopra
// sullo stesso candidato (gate_logit): con w_logit = 0 il gate a margine fa da
// solo, con w_logit > 0 il margine CORREGGE il gate appreso invece di
// sostituirlo. c_cnt renderebbe la soglia consapevole del numero di candidati
// provati (stessa idea delle feature 10 e 11); misurato, non aiuta -- a
// ricerca fissa il numero di candidati varia troppo poco da parola a parola --
// e nei parametri di gate_weights.hpp vale 0.
//
// Con bit noti per ipotesi a priori la soglia diventa thr * (N/liberi)^ap_pow,
// come Ft2Decoder fa per nd_max; ap_pow = 0 la lascia ferma. Se tutti i bit
// d'informazione sono noti la parola possibile e' una sola e la concorrente non
// esiste: qui il termine di margine vale zero, ma Ft2Decoder in quel caso non
// chiama margin_accept() e lascia decidere la regola di prima.
struct MarginFeatures {
    float nd = 0;          // distanza soft normalizzata del candidato
    float nd2 = 1;         // idem per il concorrente (OsdFast::last_nd2)
    bool  has2 = false;    // il concorrente esiste
    float log2_tested = 0; // log2(1 + candidati valutati dall'OSD)
    float free_ratio = 1;  // bit liberi (non AP) / N
    bool  valid = false;   // la parola ha un candidato dell'OSD (174,77)
};

struct MarginGate {
    float c_gap   = 0.0f;
    float gap_cap = 1.0f;
    float c_cnt   = 0.0f;
    float w_logit = 0.0f;
    float thr     = 1.0f;
    float ap_pow  = 0.0f;
};

inline float margin_stat(const MarginFeatures& m, const MarginGate& g, float logit) {
    float gap = m.has2 ? m.nd2 - m.nd : 0.0f;
    if (gap > g.gap_cap) gap = g.gap_cap;
    if (gap < -g.gap_cap) gap = -g.gap_cap;
    return m.nd - g.c_gap * gap + g.c_cnt * (m.log2_tested - 11.0f) - g.w_logit * logit;
}

inline bool margin_accept(const MarginFeatures& m, const MarginGate& g, float logit) {
    float thr = g.thr;
    if (g.ap_pow != 0.0f && m.free_ratio > 0.0f && m.free_ratio < 1.0f)
        thr *= std::pow(1.0f / m.free_ratio, g.ap_pow);
    return margin_stat(m, g, logit) <= thr;
}

#ifdef SUPERLDPC_HAVE_GATE_WEIGHTS
#include "gate_weights.hpp"
inline float gate_logit(const GateFeatures& g, bool ft8 = false) {
    const float* W  = ft8 ? GATE_W_FT8  : GATE_W_FT2;
    const float* MU = ft8 ? GATE_MU_FT8 : GATE_MU_FT2;
    const float* SD = ft8 ? GATE_SD_FT8 : GATE_SD_FT2;
    float z = ft8 ? GATE_B_FT8 : GATE_B_FT2;
    for (int k = 0; k < GATE_NF; ++k) z += W[k] * (g.f[k] - MU[k]) / SD[k];
    return z;
}
inline bool gate_accept(const GateFeatures& g, bool ft8 = false) {
    return gate_logit(g, ft8) > (ft8 ? GATE_THRESHOLD_FT8 : GATE_THRESHOLD_FT2)
                                 + gate_threshold_delta();
}
#else
// Senza i pesi non c'e' logit e non ci sono nemmeno i parametri del gate a
// margine (MARGIN_GATE_FT8, MARGIN_GATE_FT2: stanno in gate_weights.hpp e sono
// tarati insieme al logit). Chi li chiede senza SUPERLDPC_HAVE_GATE_WEIGHTS non
// compila, apposta: una soglia per il solo termine di margine non e' mai stata
// misurata su semi diversi da quelli di taratura ne' con bit AP.
inline float gate_logit(const GateFeatures&, bool = false) { return 1.0f; }
inline bool gate_accept(const GateFeatures&, bool = false) { return true; }
#endif
