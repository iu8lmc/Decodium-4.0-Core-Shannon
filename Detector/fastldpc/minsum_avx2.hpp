// minsum_avx2.hpp — stadio 1 con intrinsics AVX2 espliciti.
//
// Perche' non basta l'auto-vettorizzazione (che minsum.hpp e decoder.hpp si
// aspettavano): i kernel del min-sum contengono min1/min2/argmin e
// aggiornamenti condizionati che gcc 15 e clang 22 rifiutano entrambi con
// "unsupported control flow in loop". Misurato su Zen 3: 139 us/parola con
// gcc, 94 con clang, contro i ~17 attesi. Qui le stesse operazioni sono
// scritte a mano: min/abs/blend/xor su 16 int16 per registro YMM.
//
// Semantica IDENTICA a MinSumV2 (stessa quantizzazione, stesso alpha = 3/4,
// stesso layout L[v*B+b] e posterior[b*N+v]): cpp/verify.cpp la verifica bit
// per bit. In piu': early-exit per gruppo di 16 parole (roadmap #2).
#pragma once
#include "decoder.hpp"   // Code, MinSumV2, crc14_ok

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)

#include "minsum_neon.hpp"

#elif defined(__AVX2__)
#include <immintrin.h>

class MinSumV3 {
public:
    static constexpr int LLR_FIX = 8;
    static constexpr int LLR_MAX = 2047;
    static constexpr int LANES   = 16;          // int16 per registro YMM

    // batch DEVE essere multiplo di 16.
    MinSumV3(const Code& code, int batch, int max_iter)
        : c_(code), B_(batch), G_(batch / LANES), max_iter_(max_iter),
          L_((size_t)code.N * batch), Lout_((size_t)code.N * batch),
          R_((size_t)code.edges() * batch),
          done_(batch), gactive_(batch / LANES), unsat_(batch) {
        if (batch % LANES) throw std::runtime_error("MinSumV3: batch deve essere multiplo di 16");
    }

    // Fattore di normalizzazione del min-sum, in unita' di 1/65536.
    // 49152 = 3/4, la costante storica di Chen-Fossorier: con quella il
    // comportamento e' bit-identico a prima. Deve essere PARI, perche' il
    // backend NEON lo dimezza per usare vqdmulhq_s16 e i due devono coincidere.
    void set_alpha(unsigned w) { alpha_w_ = (uint16_t)(w & ~1u); }
    unsigned alpha() const { return alpha_w_; }

    int batch() const { return B_; }
    int N() const { return c_.N; }
    const int16_t* posterior(int b) const { return &Lout_[(size_t)b * c_.N]; }

    // Check non soddisfatti dalla parola b all'ultima iterazione utile: 0 se e'
    // convergente. E' una misura di quanto lontano e' rimasto il min-sum, e
    // quindi di quanta speranza ha l'OSD (vedi cpp/triage_stats.cpp).
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
        std::fill(gactive_.begin(), gactive_.end(), (uint8_t)1);
        for (int b = 0; b < B; ++b) { out_iters[b] = max_iter_; out_ok[b] = 0; }

        int gleft = G_;
        for (int it = 1; it <= max_iter_ && gleft > 0; ++it) {
            for (int g = 0; g < G_; ++g) {
                if (!gactive_[g]) continue;
                const int g0 = g * LANES;
                for (int m = 0; m < M; ++m) {
                    const int e0 = c_.row_ptr[m], deg = c_.row_ptr[m + 1] - e0;
                    const int* cols = &c_.col_idx[e0];
                    if      (deg == 6) check<6>(cols, e0, g0);
                    else if (deg == 7) check<7>(cols, e0, g0);
                    else               check_gen(cols, e0, g0, deg);
                }
                // sindrome dell'intero gruppo, vettorizzata. Oltre a sapere SE
                // la parola e' chiusa, si conta QUANTI check restano
                // insoddisfatti: par vale 0 o -1 per lane, quindi sottrarlo
                // accumula il conteggio. Costa una vpsubw per check ed e' il
                // criterio con cui si decide se vale la pena chiamare l'OSD.
                __m256i bad = _mm256_setzero_si256();
                __m256i nbad = _mm256_setzero_si256();
                for (int m = 0; m < M; ++m) {
                    __m256i par = _mm256_setzero_si256();
                    for (int e = c_.row_ptr[m]; e < c_.row_ptr[m + 1]; ++e)
                        par = _mm256_xor_si256(par, sign_mask(ld(&L_[(size_t)c_.col_idx[e] * B + g0])));
                    bad = _mm256_or_si256(bad, par);
                    nbad = _mm256_sub_epi16(nbad, par);
                }
                st(&unsat_[g0], nbad);
                // 2 bit di maschera per ogni lane int16
                unsigned conv = (unsigned)_mm256_movemask_epi8(
                        _mm256_cmpeq_epi16(bad, _mm256_setzero_si256()));
                bool all_done = true;
                for (int k = 0; k < LANES; ++k) {
                    const int b = g0 + k;
                    if (done_[b]) { unsat_[b] = 0; continue; }
                    if (conv & (1u << (2 * k))) {
                        done_[b] = 1; out_ok[b] = 1; out_iters[b] = it; snapshot(b);
                    } else all_done = false;
                }
                if (all_done) { gactive_[g] = 0; --gleft; }
            }
        }
        for (int b = 0; b < B; ++b) {
            if (!done_[b]) snapshot(b);
            const int16_t* P = posterior(b);
            for (int v = 0; v < N; ++v) out_bits[(size_t)b * N + v] = (P[v] < 0);
        }
    }

private:
    static inline __m256i ld(const int16_t* p) { return _mm256_loadu_si256((const __m256i*)p); }
    static inline void st(int16_t* p, __m256i v) { _mm256_storeu_si256((__m256i*)p, v); }
    static inline __m256i sign_mask(__m256i x) { return _mm256_srai_epi16(x, 15); }  // 0 oppure -1

    // Un check node, 16 parole in parallelo. DEG noto -> q[] resta nei registri.
    template <int DEG>
    inline void check(const int* cols, int e0, int g0) {
        const __m256i VMAX = _mm256_set1_epi16(LLR_MAX), VMIN = _mm256_set1_epi16(-LLR_MAX);
        __m256i q[DEG];
        // INIT = LLR_MAX+1: irraggiungibile da |q| (<= LLR_MAX) come il 32767
        // dello scalare, ma moltiplicato per 3 non trabocca in int16.
        __m256i m1 = _mm256_set1_epi16(LLR_MAX + 1), m2 = m1;
        __m256i i1 = _mm256_set1_epi16(-1), sgn = _mm256_setzero_si256();
        for (int j = 0; j < DEG; ++j) {
            __m256i x = _mm256_sub_epi16(ld(&L_[(size_t)cols[j] * B_ + g0]),
                                         ld(&R_[(size_t)(e0 + j) * B_ + g0]));
            x = _mm256_max_epi16(_mm256_min_epi16(x, VMAX), VMIN);
            q[j] = x;
            __m256i a   = _mm256_abs_epi16(x);
            __m256i lt1 = _mm256_cmpgt_epi16(m1, a);              // a < m1
            m2  = _mm256_blendv_epi8(_mm256_min_epi16(m2, a), m1, lt1);
            m1  = _mm256_min_epi16(m1, a);
            i1  = _mm256_blendv_epi8(i1, _mm256_set1_epi16((short)j), lt1);
            sgn = _mm256_xor_si256(sgn, sign_mask(x));
        }
        // mulhi prende i 16 bit alti: (m * W) / 65536, senza traboccare e con
        // un'istruzione in meno di mullo+srai. W = 49152 e' esattamente 3/4.
        const __m256i AW = _mm256_set1_epi16((short)alpha_w_);
        const __m256i n1 = _mm256_mulhi_epu16(m1, AW);
        const __m256i n2 = _mm256_mulhi_epu16(m2, AW);
        for (int j = 0; j < DEG; ++j) {
            __m256i eq  = _mm256_cmpeq_epi16(i1, _mm256_set1_epi16((short)j));
            __m256i mag = _mm256_blendv_epi8(n1, n2, eq);
            __m256i s   = _mm256_xor_si256(sgn, sign_mask(q[j]));         // segno escluso self
            __m256i r   = _mm256_sub_epi16(_mm256_xor_si256(mag, s), s);  // s ? -mag : mag
            st(&R_[(size_t)(e0 + j) * B_ + g0], r);
            __m256i y = _mm256_max_epi16(_mm256_min_epi16(_mm256_add_epi16(q[j], r), VMAX), VMIN);
            st(&L_[(size_t)cols[j] * B_ + g0], y);
        }
    }

    // Fallback per gradi diversi da 6 e 7 (LDPC(174,91) non li usa).
    inline void check_gen(const int* cols, int e0, int g0, int deg) {
        const __m256i VMAX = _mm256_set1_epi16(LLR_MAX), VMIN = _mm256_set1_epi16(-LLR_MAX);
        __m256i q[32];
        __m256i m1 = _mm256_set1_epi16(LLR_MAX + 1), m2 = m1;
        __m256i i1 = _mm256_set1_epi16(-1), sgn = _mm256_setzero_si256();
        for (int j = 0; j < deg; ++j) {
            __m256i x = _mm256_sub_epi16(ld(&L_[(size_t)cols[j] * B_ + g0]),
                                         ld(&R_[(size_t)(e0 + j) * B_ + g0]));
            x = _mm256_max_epi16(_mm256_min_epi16(x, VMAX), VMIN);
            q[j] = x;
            __m256i a   = _mm256_abs_epi16(x);
            __m256i lt1 = _mm256_cmpgt_epi16(m1, a);
            m2  = _mm256_blendv_epi8(_mm256_min_epi16(m2, a), m1, lt1);
            m1  = _mm256_min_epi16(m1, a);
            i1  = _mm256_blendv_epi8(i1, _mm256_set1_epi16((short)j), lt1);
            sgn = _mm256_xor_si256(sgn, sign_mask(x));
        }
        const __m256i AW = _mm256_set1_epi16((short)alpha_w_);
        const __m256i n1 = _mm256_mulhi_epu16(m1, AW);
        const __m256i n2 = _mm256_mulhi_epu16(m2, AW);
        for (int j = 0; j < deg; ++j) {
            __m256i eq  = _mm256_cmpeq_epi16(i1, _mm256_set1_epi16((short)j));
            __m256i mag = _mm256_blendv_epi8(n1, n2, eq);
            __m256i s   = _mm256_xor_si256(sgn, sign_mask(q[j]));
            __m256i r   = _mm256_sub_epi16(_mm256_xor_si256(mag, s), s);
            st(&R_[(size_t)(e0 + j) * B_ + g0], r);
            __m256i y = _mm256_max_epi16(_mm256_min_epi16(_mm256_add_epi16(q[j], r), VMAX), VMIN);
            st(&L_[(size_t)cols[j] * B_ + g0], y);
        }
    }

    void snapshot(int b) {
        for (int v = 0; v < c_.N; ++v) Lout_[(size_t)b * c_.N + v] = L_[(size_t)v * B_ + b];
    }

    const Code& c_;
    int B_, G_, max_iter_;
    std::vector<int16_t> L_, Lout_, R_;
    std::vector<uint8_t> done_, gactive_;
    std::vector<int16_t> unsat_;
    uint16_t alpha_w_ = 49152;      // 3/4 in 1/65536
};

#else   // niente AVX2: si ricade sulla versione portabile
using MinSumV3 = MinSumV2;
#endif
