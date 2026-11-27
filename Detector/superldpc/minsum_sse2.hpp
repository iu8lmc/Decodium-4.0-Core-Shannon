// minsum_sse2.hpp — stadio 1 con intrinsics SSE2, per le CPU x86-64 senza AVX2.
//
// Stesso algoritmo di MinSumV3 AVX2 (minsum_avx2.hpp) e di MinSumV2, con 8
// parole int16 per registro XMM invece di 16. Usa SOLO istruzioni SSE2, che
// ogni CPU x86-64 possiede: le tre che AVX2 usa e SSE2 non ha sono sostituite
// con equivalenti esatti sugli stessi valori.
//   _mm256_abs_epi16    (SSSE3)  -> max(x, 0 - x): |x| <= LLR_MAX, nessun trabocco
//   _mm256_blendv_epi8  (SSE4.1) -> (m & b) | (~m & a): le maschere sono 0 o -1
//                                  su tutta la corsia int16, quindi identico
//   movemask a 32 bit           -> movemask a 16 bit, sempre 2 bit per corsia
// Il risultato e' bit-identico al backend AVX2: tests/test_superldpc_tiers.cpp
// lo verifica parola per parola.
//
// Una variante compilata con -mavx (codifica VEX, per Sandy/Ivy Bridge) e'
// stata provata e scartata: AVX senza AVX2 non ha operazioni intere a 256 bit,
// e sul banco era alla pari o piu' lenta della SSE2 (25/11/2026).
#pragma once
#include "decoder.hpp"   // Code, MinSumV2, crc14_ok
#include <emmintrin.h>

class MinSumV3 {
public:
    static constexpr int LLR_FIX = 8;
    static constexpr int LLR_MAX = 2047;
    static constexpr int LANES   = 8;           // int16 per registro XMM

    // batch DEVE essere multiplo di 8.
    MinSumV3(const Code& code, int batch, int max_iter)
        : c_(code), B_(batch), G_(batch / LANES), max_iter_(max_iter),
          L_((size_t)code.N * batch), Lout_((size_t)code.N * batch),
          R_((size_t)code.edges() * batch),
          done_(batch), gactive_(batch / LANES), unsat_(batch) {
        if (batch % LANES) throw std::runtime_error("MinSumV3: batch deve essere multiplo di 8");
    }

    void set_alpha(unsigned w) { alpha_w_ = (uint16_t)(w & ~1u); }
    unsigned alpha() const { return alpha_w_; }

    int batch() const { return B_; }
    int N() const { return c_.N; }
    const int16_t* posterior(int b) const { return &Lout_[(size_t)b * c_.N]; }
    int unsat(int b) const { return unsat_[b]; }

    void decode(const float* llr, uint8_t* out_bits, int* out_iters, uint8_t* out_ok,
                int occupate = -1) {
        const int N = c_.N, M = c_.M, B = B_;
        const int gusati = (occupate < 0 || occupate >= B)
                               ? G_
                               : std::max(1, (occupate + LANES - 1) / LANES);
        const int busati = gusati * LANES;
        for (int v = 0; v < N; ++v) {
            int16_t* Lv = &L_[(size_t)v * B];
            for (int b = 0; b < busati; ++b) {
                float x = llr[(size_t)b * N + v] * LLR_FIX;
                x = std::max(-(float)LLR_MAX, std::min((float)LLR_MAX, x));
                Lv[b] = (int16_t)std::lrint(x);
            }
        }
        std::fill(R_.begin(), R_.end(), (int16_t)0);
        std::fill(done_.begin(), done_.end(), (uint8_t)0);
        std::fill(gactive_.begin(), gactive_.end(), (uint8_t)1);
        for (int b = 0; b < B; ++b) { out_iters[b] = max_iter_; out_ok[b] = 0; }

        int gleft = gusati;
        for (int it = 1; it <= max_iter_ && gleft > 0; ++it) {
            for (int g = 0; g < gusati; ++g) {
                if (!gactive_[g]) continue;
                const int g0 = g * LANES;
                for (int m = 0; m < M; ++m) {
                    const int e0 = c_.row_ptr[m], deg = c_.row_ptr[m + 1] - e0;
                    const int* cols = &c_.col_idx[e0];
                    if      (deg == 6) check<6>(cols, e0, g0);
                    else if (deg == 7) check<7>(cols, e0, g0);
                    else               check_gen(cols, e0, g0, deg);
                }
                __m128i bad = _mm_setzero_si128();
                __m128i nbad = _mm_setzero_si128();
                for (int m = 0; m < M; ++m) {
                    __m128i par = _mm_setzero_si128();
                    for (int e = c_.row_ptr[m]; e < c_.row_ptr[m + 1]; ++e)
                        par = _mm_xor_si128(par, sign_mask(ld(&L_[(size_t)c_.col_idx[e] * B + g0])));
                    bad = _mm_or_si128(bad, par);
                    nbad = _mm_sub_epi16(nbad, par);
                }
                st(&unsat_[g0], nbad);
                // 2 bit di maschera per ogni lane int16
                unsigned conv = (unsigned)_mm_movemask_epi8(
                        _mm_cmpeq_epi16(bad, _mm_setzero_si128()));
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
        for (int b = 0; b < busati; ++b) {
            if (!done_[b]) snapshot(b);
            const int16_t* P = posterior(b);
            for (int v = 0; v < N; ++v) out_bits[(size_t)b * N + v] = (P[v] < 0);
        }
    }

private:
    static inline __m128i ld(const int16_t* p) { return _mm_loadu_si128((const __m128i*)p); }
    static inline void st(int16_t* p, __m128i v) { _mm_storeu_si128((__m128i*)p, v); }
    static inline __m128i sign_mask(__m128i x) { return _mm_srai_epi16(x, 15); }  // 0 oppure -1
    // |x| con sole istruzioni SSE2 (pabsw e' SSSE3).
    static inline __m128i abs16(__m128i x) { return _mm_max_epi16(x, _mm_sub_epi16(_mm_setzero_si128(), x)); }
    // m ? b : a, con m = 0 oppure -1 per corsia (pblendvb e' SSE4.1).
    static inline __m128i sel(__m128i a, __m128i b, __m128i m) {
        return _mm_or_si128(_mm_and_si128(m, b), _mm_andnot_si128(m, a));
    }

    template <int DEG>
    inline void check(const int* cols, int e0, int g0) {
        const __m128i VMAX = _mm_set1_epi16(LLR_MAX), VMIN = _mm_set1_epi16(-LLR_MAX);
        __m128i q[DEG];
        __m128i m1 = _mm_set1_epi16(LLR_MAX + 1), m2 = m1;
        __m128i i1 = _mm_set1_epi16(-1), sgn = _mm_setzero_si128();
        for (int j = 0; j < DEG; ++j) {
            __m128i x = _mm_sub_epi16(ld(&L_[(size_t)cols[j] * B_ + g0]),
                                      ld(&R_[(size_t)(e0 + j) * B_ + g0]));
            x = _mm_max_epi16(_mm_min_epi16(x, VMAX), VMIN);
            q[j] = x;
            __m128i a   = abs16(x);
            __m128i lt1 = _mm_cmpgt_epi16(m1, a);              // a < m1
            m2  = sel(_mm_min_epi16(m2, a), m1, lt1);
            m1  = _mm_min_epi16(m1, a);
            i1  = sel(i1, _mm_set1_epi16((short)j), lt1);
            sgn = _mm_xor_si128(sgn, sign_mask(x));
        }
        const __m128i AW = _mm_set1_epi16((short)alpha_w_);
        const __m128i n1 = _mm_mulhi_epu16(m1, AW);
        const __m128i n2 = _mm_mulhi_epu16(m2, AW);
        for (int j = 0; j < DEG; ++j) {
            __m128i eq  = _mm_cmpeq_epi16(i1, _mm_set1_epi16((short)j));
            __m128i mag = sel(n1, n2, eq);
            __m128i s   = _mm_xor_si128(sgn, sign_mask(q[j]));
            __m128i r   = _mm_sub_epi16(_mm_xor_si128(mag, s), s);  // s ? -mag : mag
            st(&R_[(size_t)(e0 + j) * B_ + g0], r);
            __m128i y = _mm_max_epi16(_mm_min_epi16(_mm_add_epi16(q[j], r), VMAX), VMIN);
            st(&L_[(size_t)cols[j] * B_ + g0], y);
        }
    }

    inline void check_gen(const int* cols, int e0, int g0, int deg) {
        const __m128i VMAX = _mm_set1_epi16(LLR_MAX), VMIN = _mm_set1_epi16(-LLR_MAX);
        __m128i q[32];
        __m128i m1 = _mm_set1_epi16(LLR_MAX + 1), m2 = m1;
        __m128i i1 = _mm_set1_epi16(-1), sgn = _mm_setzero_si128();
        for (int j = 0; j < deg; ++j) {
            __m128i x = _mm_sub_epi16(ld(&L_[(size_t)cols[j] * B_ + g0]),
                                      ld(&R_[(size_t)(e0 + j) * B_ + g0]));
            x = _mm_max_epi16(_mm_min_epi16(x, VMAX), VMIN);
            q[j] = x;
            __m128i a   = abs16(x);
            __m128i lt1 = _mm_cmpgt_epi16(m1, a);
            m2  = sel(_mm_min_epi16(m2, a), m1, lt1);
            m1  = _mm_min_epi16(m1, a);
            i1  = sel(i1, _mm_set1_epi16((short)j), lt1);
            sgn = _mm_xor_si128(sgn, sign_mask(x));
        }
        const __m128i AW = _mm_set1_epi16((short)alpha_w_);
        const __m128i n1 = _mm_mulhi_epu16(m1, AW);
        const __m128i n2 = _mm_mulhi_epu16(m2, AW);
        for (int j = 0; j < deg; ++j) {
            __m128i eq  = _mm_cmpeq_epi16(i1, _mm_set1_epi16((short)j));
            __m128i mag = sel(n1, n2, eq);
            __m128i s   = _mm_xor_si128(sgn, sign_mask(q[j]));
            __m128i r   = _mm_sub_epi16(_mm_xor_si128(mag, s), s);
            st(&R_[(size_t)(e0 + j) * B_ + g0], r);
            __m128i y = _mm_max_epi16(_mm_min_epi16(_mm_add_epi16(q[j], r), VMAX), VMIN);
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
