// minsum_neon.hpp — stadio min-sum vettorizzato ARM NEON.
//
// Mantiene la stessa semantica bit-per-bit di MinSumV2 e del backend AVX2,
// elaborando otto LLR int16 per registro NEON da 128 bit. AArch64 rende
// Advanced SIMD parte dell'architettura, ma il dispatcher verifica comunque
// la capacita' runtime del sistema prima di entrare nella translation unit.
#pragma once

#include "decoder.hpp"

#if defined(_MSC_VER) && defined(_M_ARM64)
#  include <arm64_neon.h>
#else
#  include <arm_neon.h>
#endif

class MinSumV3 {
public:
    static constexpr int LLR_FIX = 8;
    static constexpr int LLR_MAX = 2047;
    static constexpr int LANES = 8;

    MinSumV3(const Code& code, int batch, int max_iter)
        : c_(code), B_(batch), G_(batch / LANES), max_iter_(max_iter),
          L_((size_t)code.N * batch), Lout_((size_t)code.N * batch),
          R_((size_t)code.edges() * batch), done_(batch),
          gactive_(batch / LANES), unsat_(batch)
    {
        if (batch % LANES)
            throw std::runtime_error("MinSumV3 NEON: batch deve essere multiplo di 8");
    }

    // Vedi minsum_avx2.hpp: stessa semantica, stesso risultato bit per bit.
    // vqdmulhq_s16(m, W/2) = (2*m*(W/2)) >> 16 = (m*W) >> 16 per W pari.
    void set_alpha(unsigned w) { alpha_h_ = (int16_t)((w & ~1u) >> 1); }
    unsigned alpha() const { return (unsigned)alpha_h_ << 1; }

    int batch() const { return B_; }
    int N() const { return c_.N; }
    const int16_t* posterior(int b) const { return &Lout_[(size_t)b * c_.N]; }
    int unsat(int b) const { return unsat_[b]; }

    void decode(const float* llr, uint8_t* out_bits, int* out_iters, uint8_t* out_ok)
    {
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
        for (int b = 0; b < B; ++b) {
            out_iters[b] = max_iter_;
            out_ok[b] = 0;
        }

        int groupsLeft = G_;
        for (int iteration = 1; iteration <= max_iter_ && groupsLeft > 0; ++iteration) {
            for (int group = 0; group < G_; ++group) {
                if (!gactive_[group]) continue;
                const int firstLane = group * LANES;
                for (int checkIndex = 0; checkIndex < M; ++checkIndex) {
                    const int edge0 = c_.row_ptr[checkIndex];
                    const int degree = c_.row_ptr[checkIndex + 1] - edge0;
                    const int* columns = &c_.col_idx[edge0];
                    if (degree == 6)
                        check<6>(columns, edge0, firstLane);
                    else if (degree == 7)
                        check<7>(columns, edge0, firstLane);
                    else
                        checkGeneric(columns, edge0, firstLane, degree);
                }

                int16x8_t bad = vdupq_n_s16(0);
                int16x8_t badCount = vdupq_n_s16(0);
                for (int checkIndex = 0; checkIndex < M; ++checkIndex) {
                    int16x8_t parity = vdupq_n_s16(0);
                    for (int edge = c_.row_ptr[checkIndex];
                         edge < c_.row_ptr[checkIndex + 1]; ++edge) {
                        parity = veorq_s16(
                            parity,
                            signMask(load(&L_[(size_t)c_.col_idx[edge] * B + firstLane])));
                    }
                    bad = vorrq_s16(bad, parity);
                    badCount = vsubq_s16(badCount, parity);
                }
                store(&unsat_[firstLane], badCount);

                int16_t badLanes[LANES];
                store(badLanes, bad);
                bool allDone = true;
                for (int lane = 0; lane < LANES; ++lane) {
                    const int word = firstLane + lane;
                    if (done_[word]) {
                        unsat_[word] = 0;
                        continue;
                    }
                    if (badLanes[lane] == 0) {
                        done_[word] = 1;
                        out_ok[word] = 1;
                        out_iters[word] = iteration;
                        snapshot(word);
                    } else {
                        allDone = false;
                    }
                }
                if (allDone) {
                    gactive_[group] = 0;
                    --groupsLeft;
                }
            }
        }

        for (int word = 0; word < B; ++word) {
            if (!done_[word]) snapshot(word);
            const int16_t* posteriorValues = posterior(word);
            for (int variable = 0; variable < N; ++variable)
                out_bits[(size_t)word * N + variable] = posteriorValues[variable] < 0;
        }
    }

private:
    static inline int16x8_t load(const int16_t* values) { return vld1q_s16(values); }
    static inline void store(int16_t* values, int16x8_t vector) { vst1q_s16(values, vector); }
    static inline int16x8_t signMask(int16x8_t value) { return vshrq_n_s16(value, 15); }

    template <int Degree>
    inline void check(const int* columns, int edge0, int firstLane)
    {
        const int16x8_t maximum = vdupq_n_s16(LLR_MAX);
        const int16x8_t minimum = vdupq_n_s16(-LLR_MAX);
        int16x8_t extrinsic[Degree];
        int16x8_t min1 = vdupq_n_s16(LLR_MAX + 1);
        int16x8_t min2 = min1;
        int16x8_t minIndex = vdupq_n_s16(-1);
        int16x8_t signs = vdupq_n_s16(0);

        for (int edge = 0; edge < Degree; ++edge) {
            int16x8_t value = vsubq_s16(
                load(&L_[(size_t)columns[edge] * B_ + firstLane]),
                load(&R_[(size_t)(edge0 + edge) * B_ + firstLane]));
            value = vmaxq_s16(vminq_s16(value, maximum), minimum);
            extrinsic[edge] = value;

            int16x8_t magnitude = vabsq_s16(value);
            uint16x8_t lessThanMin1 = vcltq_s16(magnitude, min1);
            min2 = vbslq_s16(lessThanMin1, min1, vminq_s16(min2, magnitude));
            min1 = vminq_s16(min1, magnitude);
            minIndex = vbslq_s16(lessThanMin1,
                                  vdupq_n_s16((int16_t)edge), minIndex);
            signs = veorq_s16(signs, signMask(value));
        }

        const int16x8_t normalisedMin1 = vqdmulhq_n_s16(min1, alpha_h_);
        const int16x8_t normalisedMin2 = vqdmulhq_n_s16(min2, alpha_h_);
        for (int edge = 0; edge < Degree; ++edge) {
            uint16x8_t isMinimum = vceqq_s16(minIndex, vdupq_n_s16((int16_t)edge));
            int16x8_t magnitude = vbslq_s16(isMinimum, normalisedMin2, normalisedMin1);
            int16x8_t sign = veorq_s16(signs, signMask(extrinsic[edge]));
            int16x8_t response = vsubq_s16(veorq_s16(magnitude, sign), sign);
            store(&R_[(size_t)(edge0 + edge) * B_ + firstLane], response);
            int16x8_t posteriorValue = vaddq_s16(extrinsic[edge], response);
            posteriorValue = vmaxq_s16(vminq_s16(posteriorValue, maximum), minimum);
            store(&L_[(size_t)columns[edge] * B_ + firstLane], posteriorValue);
        }
    }

    inline void checkGeneric(const int* columns, int edge0, int firstLane, int degree)
    {
        const int16x8_t maximum = vdupq_n_s16(LLR_MAX);
        const int16x8_t minimum = vdupq_n_s16(-LLR_MAX);
        int16x8_t extrinsic[32];
        int16x8_t min1 = vdupq_n_s16(LLR_MAX + 1);
        int16x8_t min2 = min1;
        int16x8_t minIndex = vdupq_n_s16(-1);
        int16x8_t signs = vdupq_n_s16(0);

        for (int edge = 0; edge < degree; ++edge) {
            int16x8_t value = vsubq_s16(
                load(&L_[(size_t)columns[edge] * B_ + firstLane]),
                load(&R_[(size_t)(edge0 + edge) * B_ + firstLane]));
            value = vmaxq_s16(vminq_s16(value, maximum), minimum);
            extrinsic[edge] = value;
            int16x8_t magnitude = vabsq_s16(value);
            uint16x8_t lessThanMin1 = vcltq_s16(magnitude, min1);
            min2 = vbslq_s16(lessThanMin1, min1, vminq_s16(min2, magnitude));
            min1 = vminq_s16(min1, magnitude);
            minIndex = vbslq_s16(lessThanMin1,
                                  vdupq_n_s16((int16_t)edge), minIndex);
            signs = veorq_s16(signs, signMask(value));
        }

        const int16x8_t normalisedMin1 = vqdmulhq_n_s16(min1, alpha_h_);
        const int16x8_t normalisedMin2 = vqdmulhq_n_s16(min2, alpha_h_);
        for (int edge = 0; edge < degree; ++edge) {
            uint16x8_t isMinimum = vceqq_s16(minIndex, vdupq_n_s16((int16_t)edge));
            int16x8_t magnitude = vbslq_s16(isMinimum, normalisedMin2, normalisedMin1);
            int16x8_t sign = veorq_s16(signs, signMask(extrinsic[edge]));
            int16x8_t response = vsubq_s16(veorq_s16(magnitude, sign), sign);
            store(&R_[(size_t)(edge0 + edge) * B_ + firstLane], response);
            int16x8_t posteriorValue = vaddq_s16(extrinsic[edge], response);
            posteriorValue = vmaxq_s16(vminq_s16(posteriorValue, maximum), minimum);
            store(&L_[(size_t)columns[edge] * B_ + firstLane], posteriorValue);
        }
    }

    void snapshot(int word)
    {
        for (int variable = 0; variable < c_.N; ++variable)
            Lout_[(size_t)word * c_.N + variable] = L_[(size_t)variable * B_ + word];
    }

    const Code& c_;
    int B_;
    int G_;
    int max_iter_;
    std::vector<int16_t> L_;
    std::vector<int16_t> Lout_;
    std::vector<int16_t> R_;
    std::vector<uint8_t> done_;
    std::vector<uint8_t> gactive_;
    std::vector<int16_t> unsat_;
    int16_t alpha_h_ = 24576;       // 49152/2, cioe' 3/4
};
