// gate.hpp — gate appreso contro le false decodifiche (FASTLDPC-AI-SPEC-001, strato 2).
// Sostituisce la sola soglia su nd con una regressione logistica su GATE_NF feature
// del candidato che ha passato la CRC-14. Pesi in gate_weights.hpp (appresi offline).
#pragma once
#include <cmath>
#include <cstdint>

static constexpr int GATE_NF = 10;

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
};

// FT2 e FT8 condividono il decoder (decodium_bridge.cpp, Ft2Decoder), ma NON
// il canale: rumore, tipi di messaggio ammessi e distribuzione delle feature
// sono diversi. I due modi hanno pesi separati (GATE_*_FT2 / GATE_*_FT8 in
// gate_weights.hpp); il parametro ft8 sceglie quale tabella usare, deciso a
// costruzione del decoder (Ft2Config::gate_is_ft8, vedi decodium_bridge.cpp).
#ifdef FASTLDPC_HAVE_GATE_WEIGHTS
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
    return gate_logit(g, ft8) > (ft8 ? GATE_THRESHOLD_FT8 : GATE_THRESHOLD_FT2);
}
#else
inline float gate_logit(const GateFeatures&, bool = false) { return 1.0f; }
inline bool gate_accept(const GateFeatures&, bool = false) { return true; }
#endif
