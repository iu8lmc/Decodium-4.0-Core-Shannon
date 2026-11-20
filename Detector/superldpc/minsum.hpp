// minsum.hpp — decoder LDPC min-sum normalizzato, layered, a virgola fissa,
// che decodifica un BATCH di candidati in parallelo (SIMD sul batch).
//
// Idea: in un ciclo FT2 il decoder viene chiamato su centinaia di candidati
// (offset di tempo, bin di frequenza, ipotesi AP). Invece di decodificarli uno
// alla volta, li mettiamo in un batch di B parole e ogni operazione del
// min-sum lavora su B valori contigui: il compilatore vettorizza il loop
// interno (AVX2 = 16 int16 per istruzione).
//
// Formato fisso: LLR in int16 con Q = 1/8 (LLR_FIX = 8), saturato a ±LLR_MAX.
// Fattore di normalizzazione alpha = 0.75 (x*3 >> 2), standard per min-sum.
//
// Uso:
//   Code code = Code::load("ldpc_174_91.h.txt");
//   MinSumDecoder dec(code, /*batch=*/64, /*max_iter=*/30);
//   dec.decode(llr_float /* [batch][N] */, out_bits, out_iters, out_ok);
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

struct Code {
    int M = 0, N = 0;
    std::vector<int> row_ptr;   // M+1
    std::vector<int> col_idx;   // colonne per ogni check, in ordine

    static Code load(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "r");
        if (!f) throw std::runtime_error("cannot open " + path);
        Code c;
        if (std::fscanf(f, "%d %d", &c.M, &c.N) != 2) throw std::runtime_error("bad header");
        c.row_ptr.push_back(0);
        for (int i = 0; i < c.M; ++i) {
            int deg; if (std::fscanf(f, "%d", &deg) != 1) throw std::runtime_error("bad row");
            for (int j = 0; j < deg; ++j) { int v; std::fscanf(f, "%d", &v); c.col_idx.push_back(v); }
            c.row_ptr.push_back((int)c.col_idx.size());
        }
        std::fclose(f);
        return c;
    }
    int edges() const { return (int)col_idx.size(); }
};

class MinSumDecoder {
public:
    static constexpr int   LLR_FIX = 8;       // 1 LLR = 8 unita' fisse
    static constexpr int16_t LLR_MAX = 2047;  // saturazione (12 bit utili)

    MinSumDecoder(const Code& code, int batch, int max_iter)
        : c_(code), B_(batch), max_iter_(max_iter),
          L_((size_t)code.N * batch), R_((size_t)code.edges() * batch),
          q_(7 * batch), done_(batch) {}

    int batch() const { return B_; }

    // llr: [B][N] float (positivo = bit 0). out_bits: [B][N] uint8.
    // out_iters: iterazioni usate per parola. out_ok: 1 se sindrome nulla.
    void decode(const float* llr, uint8_t* out_bits, int* out_iters, uint8_t* out_ok) {
        const int N = c_.N, M = c_.M, B = B_;

        // quantizzazione ingresso
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
        for (int b = 0; b < B; ++b) { out_iters[b] = 0; out_ok[b] = 0; }

        int active = B;
        for (int it = 1; it <= max_iter_ && active > 0; ++it) {
            // ---- una passata layered su tutti i check ----
            for (int m = 0; m < M; ++m) {
                const int e0 = c_.row_ptr[m], e1 = c_.row_ptr[m + 1], deg = e1 - e0;
                int16_t* q = q_.data();

                // 1) q_e = L_v - R_e  per ogni arco del check (salvato in q_)
                for (int j = 0; j < deg; ++j) {
                    const int16_t* Lv = &L_[(size_t)c_.col_idx[e0 + j] * B];
                    const int16_t* Re = &R_[(size_t)(e0 + j) * B];
                    int16_t* qj = q + (size_t)j * B;
                    for (int b = 0; b < B; ++b) qj[b] = sat(Lv[b] - Re[b]);
                }
                // 2) min1, min2, indice del min1, prodotto dei segni
                for (int b = 0; b < B; ++b) {
                    if (done_[b]) continue;            // parola gia' convergente: congelata
                    int16_t m1 = 32767, m2 = 32767; int i1 = -1; int sgn = 0;
                    for (int j = 0; j < deg; ++j) {
                        int16_t x = q[(size_t)j * B + b];
                        int16_t a = x < 0 ? -x : x;
                        sgn ^= (x < 0);
                        if (a < m1) { m2 = m1; m1 = a; i1 = j; }
                        else if (a < m2) { m2 = a; }
                    }
                    // 3) nuovo R_e e aggiornamento L_v
                    const int16_t n1 = (int16_t)((m1 * 3) >> 2), n2 = (int16_t)((m2 * 3) >> 2);
                    for (int j = 0; j < deg; ++j) {
                        int16_t x = q[(size_t)j * B + b];
                        int s = sgn ^ (x < 0);          // segno escluso self
                        int16_t mag = (j == i1) ? n2 : n1;
                        int16_t r = s ? (int16_t)-mag : mag;
                        R_[(size_t)(e0 + j) * B + b] = r;
                        L_[(size_t)c_.col_idx[e0 + j] * B + b] = sat(x + r);
                    }
                }
            }
            // ---- sindrome / early stop ----
            for (int b = 0; b < B; ++b) {
                if (done_[b]) continue;
                bool ok = true;
                for (int m = 0; m < M && ok; ++m) {
                    int par = 0;
                    for (int e = c_.row_ptr[m]; e < c_.row_ptr[m + 1]; ++e)
                        par ^= (L_[(size_t)c_.col_idx[e] * B + b] < 0);
                    ok = (par == 0);
                }
                if (ok) { done_[b] = 1; out_ok[b] = 1; out_iters[b] = it; --active; }
            }
        }
        for (int b = 0; b < B; ++b) {
            if (!done_[b]) out_iters[b] = max_iter_;
            for (int v = 0; v < N; ++v) out_bits[(size_t)b * N + v] = (L_[(size_t)v * B + b] < 0);
        }
    }

private:
    static inline int16_t sat(int x) {
        return (int16_t)(x > LLR_MAX ? LLR_MAX : (x < -LLR_MAX ? -LLR_MAX : x));
    }
    const Code& c_;
    int B_, max_iter_;
    std::vector<int16_t> L_, R_, q_;
    std::vector<uint8_t> done_;
};
