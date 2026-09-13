// bits.hpp — popcount / count-trailing-zeros portabili (gcc, clang, MSVC).
// Serve perche' il resto del progetto e' header-only e deve compilare anche
// dentro DECODIUM, che su Windows puo' essere costruito con MSVC.
#pragma once
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
static inline int fl_popcount64(uint64_t x) { return (int)__popcnt64(x); }
static inline int fl_ctz64(uint64_t x) { unsigned long i; _BitScanForward64(&i, x); return (int)i; }
#else
static inline int fl_popcount64(uint64_t x) { return __builtin_popcountll(x); }
static inline int fl_ctz64(uint64_t x) { return __builtin_ctzll(x); }
#endif
