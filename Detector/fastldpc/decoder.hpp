// decoder.hpp — catena a due stadi per LDPC(174,91) di FT2.
//
//   stadio 1: min-sum normalizzato layered, int16, batch.
//             MinSumV2 qui dentro e' la versione SCALARE di riferimento: il
//             loop interno sul batch doveva auto-vettorizzarsi, ma ne' gcc 15
//             ne' clang 22 ci riescono ("unsupported control flow in loop"),
//             139 us/parola contro i 4,7 della versione a intrinsics.
//             In produzione si usa MinSumV3 di cpp/minsum_avx2.hpp, di cui
//             cpp/verify.cpp dimostra l'equivalenza bit per bit.
//   stadio 2: OSD (ordered statistics decoding) di ordine 0/1/2 SOLO sulle
//             parole che lo stadio 1 non chiude, a partire dai suoi LLR
//             a posteriori.
//   gate:     CRC-14 (poly 0x2757, come WSJT-X) su ogni parola accettata, piu'
//             la soglia sulla distanza soft normalizzata (OsdDecoder::nd_max).
//
// Stesso formato di ingresso di minsum.hpp: LLR float [B][N], + = bit 0.
#pragma once
#include "minsum.hpp"   // Code
#include <array>
#include <climits>

// ---------------------------------------------------------------- CRC-14
// Sindrome della CRC-14: 0 se e solo se la parola e' coerente.
//
// L'LFSR parte da zero e non c'e' XOR finale, quindi il resto e' una funzione
// LINEARE su GF(2) dei bit 0..76, e il campo CRC lo e' dei bit 77..90. Ne segue
// che crc14_syn e' lineare sull'intera parola:
//
//     crc14_syn(x ^ y) == crc14_syn(x) ^ crc14_syn(y)
//
// OsdFast sfrutta questa proprieta' per testare la CRC di un candidato con un
// solo XOR invece di ricostruire la parola e rifare l'LFSR.
inline uint16_t crc14_syn(const uint8_t* a) {      // a = 174 bit hard, usa 0..90
    uint16_t r = 0;
    auto step = [&](int bit) {
        r ^= (uint16_t)(bit << 13);
        r = (r & 0x2000) ? (uint16_t)(((r << 1) ^ 0x2757) & 0x3FFF) : (uint16_t)((r << 1) & 0x3FFF);
    };
    for (int i = 0; i < 77; ++i) step(a[i]);
    for (int i = 0; i < 5; ++i) step(0);
    uint16_t c = 0;
    for (int i = 0; i < 14; ++i) c = (uint16_t)((c << 1) | a[77 + i]);
    return (uint16_t)(c ^ r);
}

inline bool crc14_ok(const uint8_t* a) { return crc14_syn(a) == 0; }

// ------------------------------------------------------ stadio 1: min-sum v2
class MinSumV2 {
public:
    static constexpr int LLR_FIX = 8;
    static constexpr int LLR_MAX = 2047;

    MinSumV2(const Code& code, int batch, int max_iter)
        : c_(code), B_(batch), max_iter_(max_iter),
          L_((size_t)code.N * batch), Lout_((size_t)code.N * batch),
          R_((size_t)code.edges() * batch), q_(8 * (size_t)batch),
          m1_(batch), m2_(batch), i1_(batch), sgn_(batch), done_(batch), unsat_(batch) {}

    int batch() const { return B_; }
    int N() const { return c_.N; }

    // Dopo decode(): LLR a posteriori (int16, Q=1/8) della parola b.
    const int16_t* posterior(int b) const { return &Lout_[(size_t)b * c_.N]; }

    // Check non soddisfatti dalla parola b: 0 se convergente. Stessa semantica
    // di MinSumV3::unsat, cosi' le due classi restano interscambiabili.
    int unsat(int b) const { return unsat_[b]; }

    void decode(const float* llr, uint8_t* out_bits, int* out_iters, uint8_t* out_ok) {
        const int N = c_.N, M = c_.M, B = B_;
        for (int v = 0; v < N; ++v) {
            int16_t* Lv = &L_[(size_t)v * B];
            for (int b = 0; b < B; ++b) {
                float x = llr[(size_t)b * N + v] * LLR_FIX;
                x = std::max(-(float)LLR_MAX, std::min((float)LLR_MAX, x));
                Lv[b] = (int16_t)std::lrint(x);
            }
        }
        std::fill(R_.begin(), R_.end(), (int16_t)0);
        std::fill(done_.begin(), done_.end(), (uint8_t)0);
        for (int b = 0; b < B; ++b) { out_iters[b] = max_iter_; out_ok[b] = 0; }

        int active = B;
        for (int it = 1; it <= max_iter_ && active > 0; ++it) {
            for (int m = 0; m < M; ++m) {
                const int e0 = c_.row_ptr[m], deg = c_.row_ptr[m + 1] - e0;
                int16_t* __restrict__ q = q_.data();
                int16_t* __restrict__ m1 = m1_.data(); int16_t* __restrict__ m2 = m2_.data();
                int16_t* __restrict__ i1 = i1_.data(); int16_t* __restrict__ sg = sgn_.data();
                for (int b = 0; b < B; ++b) { m1[b] = 32767; m2[b] = 32767; i1[b] = -1; sg[b] = 0; }
                // passo 1+2: q = L - R, min1/min2/argmin/segno  (loop interno su b)
                for (int j = 0; j < deg; ++j) {
                    const int16_t* __restrict__ Lv = &L_[(size_t)c_.col_idx[e0 + j] * B];
                    const int16_t* __restrict__ Re = &R_[(size_t)(e0 + j) * B];
                    int16_t* __restrict__ qj = q + (size_t)j * B;
#if defined(__GNUC__) && !defined(__clang__)
                    #pragma GCC ivdep
#endif
                    for (int b = 0; b < B; ++b) {
                        int x = Lv[b] - Re[b];
                        x = x > LLR_MAX ? LLR_MAX : (x < -LLR_MAX ? -LLR_MAX : x);
                        qj[b] = (int16_t)x;
                        int16_t a = (int16_t)(x < 0 ? -x : x);
                        bool lt1 = a < m1[b];
                        bool lt2 = a < m2[b];
                        m2[b] = lt1 ? m1[b] : (lt2 ? a : m2[b]);
                        m1[b] = lt1 ? a : m1[b];
                        i1[b] = lt1 ? (int16_t)j : i1[b];
                        sg[b] ^= (int16_t)(x < 0);
                    }
                }
                // passo 3: R nuovo, L aggiornato
                for (int j = 0; j < deg; ++j) {
                    int16_t* __restrict__ Lv = &L_[(size_t)c_.col_idx[e0 + j] * B];
                    int16_t* __restrict__ Re = &R_[(size_t)(e0 + j) * B];
                    const int16_t* __restrict__ qj = q + (size_t)j * B;
#if defined(__GNUC__) && !defined(__clang__)
                    #pragma GCC ivdep
#endif
                    for (int b = 0; b < B; ++b) {
                        int16_t x = qj[b];
                        int16_t mag = (i1[b] == (int16_t)j) ? m2[b] : m1[b];
                        mag = (int16_t)((mag * 3) >> 2);
                        int s = sg[b] ^ (x < 0);
                        int16_t r = s ? (int16_t)-mag : mag;
                        Re[b] = r;
                        int y = x + r;
                        Lv[b] = (int16_t)(y > LLR_MAX ? LLR_MAX : (y < -LLR_MAX ? -LLR_MAX : y));
                    }
                }
            }
            // sindrome; le parole convergenti vengono copiate in Lout_ e congelate
            for (int b = 0; b < B; ++b) {
                if (done_[b]) { unsat_[b] = 0; continue; }
                int nbad = 0;
                for (int m = 0; m < M; ++m) {
                    int par = 0;
                    for (int e = c_.row_ptr[m]; e < c_.row_ptr[m + 1]; ++e)
                        par ^= (L_[(size_t)c_.col_idx[e] * B + b] < 0);
                    nbad += par;
                }
                unsat_[b] = nbad;
                if (nbad == 0) { done_[b] = 1; out_ok[b] = 1; out_iters[b] = it; --active; snapshot(b); }
            }
        }
        for (int b = 0; b < B; ++b) {
            if (!done_[b]) snapshot(b);
            const int16_t* P = posterior(b);
            for (int v = 0; v < N; ++v) out_bits[(size_t)b * N + v] = (P[v] < 0);
        }
    }

private:
    void snapshot(int b) {
        for (int v = 0; v < c_.N; ++v) Lout_[(size_t)b * c_.N + v] = L_[(size_t)v * B_ + b];
    }
    const Code& c_;
    int B_, max_iter_;
    std::vector<int16_t> L_, Lout_, R_, q_, m1_, m2_;
    std::vector<int16_t> i1_, sgn_;
    std::vector<uint8_t> done_;
    std::vector<int> unsat_;
};

// ------------------------------------------------------------ stadio 2: OSD
// Ordered Statistics Decoding su H (M x N) con eliminazione di Gauss sulle
// colonne meno affidabili. Ordine 0, 1 o 2 (2 limitato ai `span2` bit
// d'informazione meno affidabili). Ritorna true se ha trovato una parola
// che passa la CRC-14.
class OsdDecoder {
public:
    static constexpr int NW = 3;                          // 174 bit -> 3 x uint64

    // Gate anti-false-decode: un candidato che passa la CRC-14 viene comunque
    // rifiutato se la sua distanza soft normalizzata nd supera questa soglia.
    // Tarato su 20 000 parole/punto a 0,5-3 dB (vedi README): a 0,085 elimina
    // il 90-95% delle false decodifiche e perde 4 decodifiche corrette su 9014
    // nel punto peggiore (0,5 dB), zero agli altri SNR. Mettere a 1.0 per
    // disattivarlo. Ha effetto solo se a decode() vengono passati gli LLR di
    // canale.
    float nd_max = 0.085f;

    OsdDecoder(const Code& code, int order = 1, int span2 = 32)
        : c_(code), order_(order), span2_(span2), rows_(code.M),
          absL_(code.N), perm_(code.N), hard_(code.N), best_(code.N), tmp_(code.N) {
        // H in bitset
        base_.resize(code.M);
        for (int m = 0; m < code.M; ++m) {
            base_[m] = {0, 0, 0};
            for (int e = code.row_ptr[m]; e < code.row_ptr[m + 1]; ++e) setb(base_[m], code.col_idx[e]);
        }
    }

    // L: LLR a posteriori (int16). out: 174 bit. Ritorna true se CRC ok.
    //
    // llr_ch (opzionale): LLR di CANALE della stessa parola, come dati in
    // ingresso al min-sum. Se presente, *nd riceve la distanza soft
    // normalizzata del candidato accettato:
    //
    //     nd = somma|llr_ch[v]| sui v dove il candidato smentisce la
    //          decisione hard di canale   /   somma|llr_ch[v]| su tutti i v
    //
    // e' adimensionale (in [0,1]) e quindi indipendente dalla scala degli LLR,
    // che dipende da sigma^2: la stessa soglia vale a ogni SNR. Serve come gate
    // anti-false-decode (roadmap #1): la CRC-14 da sola accetta 1 candidato su
    // 16384, e l'OSD-2 ne prova ~500 per parola. La distanza si misura sugli
    // LLR di canale e non sui posterior perche' questi sono saturati a +-2047
    // dal min-sum e non conservano il rapporto con il rumore.
    bool decode(const int16_t* L, uint8_t* out, const float* llr_ch = nullptr, float* nd = nullptr) {
        const int N = c_.N, M = c_.M;
        for (int v = 0; v < N; ++v) { absL_[v] = L[v] < 0 ? -L[v] : L[v]; hard_[v] = L[v] < 0; perm_[v] = v; }
        // Tie-break esplicito sull'indice: gli |LLR| sono interi quantizzati e
        // le parita' sono frequenti. Senza, l'ordine a parita' dipende
        // dall'implementazione di std::sort e due decoder equivalenti possono
        // scegliere candidati diversi.
        std::sort(perm_.begin(), perm_.end(), [&](int a, int b) {
            return absL_[a] != absL_[b] ? absL_[a] < absL_[b] : a < b;
        });

        // Gauss: pivot sulle colonne meno affidabili
        rows_ = base_;
        pivcol_.clear(); pivrow_.clear();
        std::vector<uint8_t> used(M, 0);
        for (int k = 0; k < N && (int)pivcol_.size() < M; ++k) {
            int col = perm_[k]; int pr = -1;
            for (int m = 0; m < M; ++m) if (!used[m] && getb(rows_[m], col)) { pr = m; break; }
            if (pr < 0) continue;
            used[pr] = 1;
            for (int m = 0; m < M; ++m) if (m != pr && getb(rows_[m], col)) xorrow(rows_[m], rows_[pr]);
            pivcol_.push_back(col); pivrow_.push_back(pr);
        }
        if ((int)pivcol_.size() < M) return false;   // H singolare (non succede per questo codice)
        std::vector<uint8_t> ispiv(N, 0);
        for (int c : pivcol_) ispiv[c] = 1;
        info_.clear();
        for (int k = N - 1; k >= 0; --k) if (!ispiv[perm_[k]]) info_.push_back(perm_[k]); // + affidabili prima
        std::reverse(info_.begin(), info_.end());   // info_[0] = meno affidabile del set d'informazione

        // parità di base con x_I = hard_I
        std::array<uint64_t, NW> hx = {0, 0, 0};
        for (int v : info_) if (hard_[v]) setb(hx, v);
        std::vector<uint8_t> basepar(M);
        for (int r = 0; r < M; ++r) basepar[r] = parity(rows_[pivrow_[r]], hx);
        // per ogni bit d'informazione: quali righe tocca
        auto rowsof = [&](int v, std::vector<uint8_t>& dst) {
            for (int r = 0; r < M; ++r) dst[r] = getb(rows_[pivrow_[r]], v);
        };

        long best = LONG_MAX; bool found = false;
        auto eval = [&](const std::vector<uint8_t>& par, const std::vector<int>& flips) {
            long score = 0;
            for (int f : flips) score += absL_[f];
            for (int r = 0; r < M; ++r) { int v = pivcol_[r]; if (par[r] != hard_[v]) score += absL_[v]; }
            if (score >= best) return;
            // costruisci parola e verifica CRC
            for (int v = 0; v < N; ++v) tmp_[v] = hard_[v];
            for (int f : flips) tmp_[f] ^= 1;
            for (int r = 0; r < M; ++r) tmp_[pivcol_[r]] = par[r];
            if (!crc14_ok(tmp_.data())) return;
            best = score; found = true; best_ = tmp_;
        };

        std::vector<uint8_t> par(M), col_i(M), col_j(M);
        eval(basepar, {});                                          // OSD-0
        if (order_ >= 1) {
            for (int a = 0; a < (int)info_.size(); ++a) {
                int i = info_[a]; rowsof(i, col_i);
                for (int r = 0; r < M; ++r) par[r] = basepar[r] ^ col_i[r];
                eval(par, {i});
                if (order_ >= 2 && a < span2_) {
                    for (int bb = a + 1; bb < span2_ && bb < (int)info_.size(); ++bb) {
                        int j = info_[bb]; rowsof(j, col_j);
                        std::vector<uint8_t> par2(M);
                        for (int r = 0; r < M; ++r) par2[r] = par[r] ^ col_j[r];
                        eval(par2, {i, j});
                    }
                }
            }
        }
        if (!found) return false;
        if (llr_ch) {
            double num = 0, den = 0;
            for (int v = 0; v < N; ++v) {
                double a = llr_ch[v] < 0 ? -llr_ch[v] : llr_ch[v];
                den += a;
                if (best_[v] != (uint8_t)(llr_ch[v] < 0)) num += a;
            }
            float d = den > 0 ? (float)(num / den) : 1.0f;
            if (nd) *nd = d;
            if (d > nd_max) return false;              // gate anti-false-decode
        }
        for (int v = 0; v < N; ++v) out[v] = best_[v];
        return true;
    }

private:
    using Row = std::array<uint64_t, NW>;
    static void setb(Row& r, int i) { r[i >> 6] |= 1ull << (i & 63); }
    static bool getb(const Row& r, int i) { return (r[i >> 6] >> (i & 63)) & 1; }
    static void xorrow(Row& a, const Row& b) { for (int w = 0; w < NW; ++w) a[w] ^= b[w]; }
    static uint8_t parity(const Row& a, const Row& b) {
        uint64_t x = 0; for (int w = 0; w < NW; ++w) x ^= a[w] & b[w];
        return (uint8_t)(__builtin_popcountll(x) & 1);
    }
    const Code& c_;
    int order_, span2_;
    std::vector<Row> base_, rows_;
    std::vector<int> absL_;
    std::vector<int> pivcol_, pivrow_, info_, perm_;
    std::vector<uint8_t> hard_, best_, tmp_;
};
