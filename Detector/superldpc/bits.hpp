// bits.hpp — popcount / count-trailing-zeros portabili (gcc, clang, MSVC).
// Serve perche' il resto del progetto e' header-only e deve compilare anche
// dentro DECODIUM, che su Windows puo' essere costruito con MSVC.
#pragma once
#include <cstdint>

#if defined(_MSC_VER) && defined(SUPERLDPC_SSE2_KERNEL)
#include <intrin.h>
// Backend SSE2: __popcnt64 emette POPCNT senza controlli, assente sulle CPU
// x86-64 piu' vecchie. Conteggio portabile (GCC/Clang usano gia' il builtin,
// che senza -mpopcnt non genera POPCNT).
static inline int fl_popcount64(uint64_t x) {
    x = x - ((x >> 1) & 0x5555555555555555ull);
    x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full;
    return (int)((x * 0x0101010101010101ull) >> 56);
}
static inline int fl_ctz64(uint64_t x) { unsigned long i; _BitScanForward64(&i, x); return (int)i; }
#elif defined(_MSC_VER)
#include <intrin.h>
static inline int fl_popcount64(uint64_t x) { return (int)__popcnt64(x); }
static inline int fl_ctz64(uint64_t x) { unsigned long i; _BitScanForward64(&i, x); return (int)i; }
#else
static inline int fl_popcount64(uint64_t x) { return __builtin_popcountll(x); }
static inline int fl_ctz64(uint64_t x) { return __builtin_ctzll(x); }
#endif
