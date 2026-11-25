// decodium_bridge_sse2.cpp — backend superldpc per CPU x86-64 senza AVX.
//
// Stesso sorgente del backend AVX2 (decodium_bridge.cpp), compilato per la CPU
// x86-64 minima (SSE2, nessuna opzione -m) con il kernel min-sum a 8 corsie
// (minsum_sse2.hpp). Namespace ed entry point propri: vedi il commento su
// SUPERLDPC_BACKEND_NS in decodium_bridge.cpp. Il dispatcher lo chiama solo se
// la CPU ha SSE2 e manca il livello superiore.
#define SUPERLDPC_BACKEND_NS superldpc_backend_sse2
#define SUPERLDPC_EXPORT(name) superldpc_sse2_##name
#define SUPERLDPC_SSE2_KERNEL 1
#include "decodium_bridge.cpp"
