#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

// Backend stubs: every superldpc tier only counts its calls, so the dispatcher
// can be checked on any CPU, including ARM CI, with emulated x86 CPUID data.
namespace {
int genericCalls = 0;
struct Counters {
    int decode = 0, mode = 0, gate = 0, extrinsic = 0, cap = 0;
    int lastMode = -1, lastCap = -1;
};
Counters top;    // superldpc_simd_*: AVX2/FMA (or NEON)
Counters sse2;   // superldpc_sse2_*
unsigned cpuFeatures = 31;
}

extern "C" unsigned superldpc_test_x86_features_c () { return cpuFeatures; }
extern "C" void ftx_decode174_91_c (float const*, int, int, int,
                                      signed char const*, signed char*,
                                      signed char*, int* ntype, int* nhard, float* dmin)
{
    ++genericCalls;
    if (ntype) *ntype = 1;
    if (nhard) *nhard = 0;
    if (dmin) *dmin = 0.0f;
}

#define TIER_STUBS(prefix, counters, extrinsicMark)                                     \
    extern "C" void prefix##set_ft8_mode_c (int mode)                                   \
    { ++counters.mode; counters.lastMode = mode; }                                      \
    extern "C" void prefix##set_ft2_max_hard_c (int cap)                                \
    { ++counters.cap; counters.lastCap = cap; }                                         \
    extern "C" void prefix##decode174_91_c (float const*, int, int, int,                \
        signed char const*, signed char*, signed char*, int*, int*, float*)             \
    { ++counters.decode; }                                                              \
    extern "C" void prefix##decode174_91_batch_c (int, float const*,                    \
        signed char const*, int, int, int, signed char*, signed char*, int*, int*,      \
        float*)                                                                         \
    { ++counters.decode; }                                                              \
    extern "C" int prefix##extrinsic174_91_c (float const*, int, float, float* out)     \
    { ++counters.extrinsic; out[0] = extrinsicMark; return 1; }                         \
    extern "C" int prefix##extrinsic174_91_v2_c (float const*, int, int, int, float,    \
                                                 float* out)                            \
    { ++counters.extrinsic; out[0] = 0.5f; return 0; }                                  \
    extern "C" void prefix##gate_dump_open_c (char const*) { ++counters.gate; }         \
    extern "C" void prefix##gate_dump_close_c () { ++counters.gate; }                   \
    extern "C" void prefix##gate_truth_set_c (signed char const*) { ++counters.gate; }  \
    extern "C" void prefix##gate_truth_clear_c () { ++counters.gate; }

TIER_STUBS (superldpc_simd_, top, 0.25f)
TIER_STUBS (superldpc_sse2_, sse2, 0.125f)

extern "C" void superldpc_set_enabled_c (int);
extern "C" int superldpc_is_enabled_c ();
extern "C" int superldpc_is_available_c ();
extern "C" int superldpc_active_tier_c ();
extern "C" void superldpc_set_ft8_mode_c (int);
extern "C" void superldpc_set_ft2_max_hard_c (int);
extern "C" int superldpc_extrinsic174_91_c (float const*, int, float, float*);
extern "C" void superldpc_gate_dump_open_c (char const*);
extern "C" void superldpc_gate_dump_close_c ();
extern "C" void superldpc_gate_truth_set_c (signed char const*);
extern "C" void superldpc_gate_truth_clear_c ();
extern "C" void superldpc_decode174_91_c (
    float const*, int, int, int, signed char const*, signed char*,
    signed char*, int*, int*, float*);
extern "C" void superldpc_decode174_91_batch_c (
    int, float const*, signed char const*, int, int, int, signed char*,
    signed char*, int*, int*, float*);

namespace {
void setEnv (char const* name, char const* value)
{
#if defined(_WIN32)
    _putenv_s (name, value);
#else
    setenv (name, value, 1);
#endif
}

bool untouched (Counters const& c)
{
    return c.decode == 0 && c.mode == 0 && c.extrinsic == 0 && c.cap == 0;
}
}

int main (int argc, char** argv)
{
    std::string const scenario = argc > 1 ? argv[1] : "environment";
    bool const emergency = scenario == "environment";
    // Tier the emulated hardware can run (0 generic, 1 SSE2, 2 AVX2/FMA),
    // independent of the LDPC button and of the emergency switch.
    int hardwareTier = 2;
    char const* maxTier = "";
    if (scenario == "ivybridge") { cpuFeatures &= ~(2u | 4u); hardwareTier = 1; } // AVX, no AVX2/FMA
    else if (scenario == "no-avx") { cpuFeatures &= ~1u; hardwareTier = 1; }
    else if (scenario == "no-avx2") { cpuFeatures &= ~2u; hardwareTier = 1; }
    else if (scenario == "no-fma") { cpuFeatures &= ~4u; hardwareTier = 1; }
    else if (scenario == "no-osxsave") { cpuFeatures &= ~8u; hardwareTier = 1; }
    else if (scenario == "no-ymm-state") { cpuFeatures &= ~16u; hardwareTier = 1; }
    else if (scenario == "no-sse2") { cpuFeatures = (cpuFeatures & ~2u) | 32u; hardwareTier = 0; }
    else if (scenario == "max-sse2") { maxTier = "sse2"; hardwareTier = 1; }
    else if (scenario == "max-generic") { maxTier = "generic"; hardwareTier = 0; }
    else if (scenario != "environment" && scenario != "supported" && scenario != "ui-disabled")
        return 1;
#if !defined(DECODIUM_SUPERLDPC_SSE2_BUILT)
    if (hardwareTier == 1) hardwareTier = 0;
#endif
    setEnv ("DECODIUM_FT2_DISABLE_SUPERLDPC", emergency ? "1" : "0");
    setEnv ("DECODIUM_SUPERLDPC_MAX_TIER", maxTier);

    // An enabled saved setting must not bypass CPU/OS or emergency guards.
    superldpc_set_enabled_c (scenario != "ui-disabled");
    bool const runtimeOff = emergency || scenario == "ui-disabled";
#if defined(DECODIUM_SUPERLDPC_TESTING)
    int const expectedTier = runtimeOff ? 0 : hardwareTier;
    // Only the AVX2/FMA tier counts as "available": an SSE2-only CPU keeps the
    // FT2 compatibility deadline.
    if (superldpc_is_available_c () != int (hardwareTier == 2)) {
        std::cerr << "incorrect hardware availability: " << scenario << '\n';
        return 1;
    }
#else
    // Real CPU detection: only the emergency scenario has a known answer.
    if (!runtimeOff) return 1;
    (void) hardwareTier;
    int const expectedTier = 0;
#endif
    if (superldpc_is_enabled_c () != int (expectedTier > 0)
        || superldpc_active_tier_c () != expectedTier) {
        std::cerr << "incorrect runtime selection: " << scenario << '\n';
        return 1;
    }
    Counters const& active = expectedTier == 2 ? top : sse2;
    Counters const& other = expectedTier == 2 ? sse2 : top;
    float const mark = expectedTier == 2 ? 0.25f : 0.125f;
    bool const simd = expectedTier > 0;

    std::array<float, 2 * 174> llr {};
    std::array<signed char, 2 * 174> mask {};
    std::array<signed char, 2 * 91> message {};
    std::array<signed char, 2 * 174> codeword {};
    std::array<int, 2> ntype {};
    std::array<int, 2> nhard {};
    std::array<float, 2> dmin {};
    std::array<float, 174> extrinsic;
    extrinsic.fill (99.0f); // Never leak the preceding probe's data on fallback.

    superldpc_set_ft8_mode_c (1);
    superldpc_decode174_91_c (llr.data (), 91, 3, 3, mask.data (),
                             message.data (), codeword.data (), ntype.data (),
                             nhard.data (), dmin.data ());
    superldpc_decode174_91_batch_c (
        2, llr.data (), mask.data (), 91, 3, 3, message.data (),
        codeword.data (), ntype.data (), nhard.data (), dmin.data ());
    int const accepted = superldpc_extrinsic174_91_c (llr.data (), 2, 2.0f, extrinsic.data ());
    bool dispatchOk = genericCalls == (simd ? 0 : 3) && accepted == int (simd)
        && std::all_of (extrinsic.begin () + (simd ? 1 : 0), extrinsic.end (),
                        [] (float value) { return value == 0.0f; });
    if (simd)
        dispatchOk = dispatchOk && active.decode == 2 && active.extrinsic == 1
            && active.mode == 3 && active.lastMode == 1 && extrinsic[0] == mark
            && untouched (other);
    else
        dispatchOk = dispatchOk && untouched (top) && untouched (sse2);
    if (!dispatchOk) {
        std::cerr << "scalar/batch/BICM dispatch regression: " << scenario << '\n';
        return 1;
    }
    // The FT2 flipped-bit cap never reaches the SIMD unit in FT8 mode, is
    // forwarded with every FT2 decode call, and a request of 0 clears it.
    superldpc_set_ft2_max_hard_c (30);
    superldpc_decode174_91_batch_c (
        2, llr.data (), mask.data (), 91, 3, 3, message.data (),
        codeword.data (), ntype.data (), nhard.data (), dmin.data ());
    bool capOk = active.cap == 0;
    superldpc_set_ft8_mode_c (0);
    superldpc_decode174_91_batch_c (
        2, llr.data (), mask.data (), 91, 3, 3, message.data (),
        codeword.data (), ntype.data (), nhard.data (), dmin.data ());
    capOk = capOk && active.cap == int (simd) && (!simd || active.lastCap == 30);
    superldpc_decode174_91_c (llr.data (), 91, 3, 3, mask.data (),
                             message.data (), codeword.data (), ntype.data (),
                             nhard.data (), dmin.data ());
    capOk = capOk && active.cap == (simd ? 2 : 0) && (!simd || active.lastCap == 30);
    superldpc_set_ft2_max_hard_c (0);
    superldpc_decode174_91_batch_c (
        2, llr.data (), mask.data (), 91, 3, 3, message.data (),
        codeword.data (), ntype.data (), nhard.data (), dmin.data ());
    capOk = capOk && active.cap == (simd ? 3 : 0) && (!simd || active.lastCap == 0)
        && other.cap == 0;
    if (!capOk) {
        std::cerr << "FT2 flipped-bit cap dispatch regression: " << scenario << '\n';
        return 1;
    }
    superldpc_set_ft8_mode_c (1);
    // A UI change between checking availability and probing is also safe.
    superldpc_set_enabled_c (0);
#if defined(DECODIUM_SUPERLDPC_TESTING)
    if (superldpc_is_available_c () != int (hardwareTier == 2)) return 1;
#endif
    if (superldpc_active_tier_c () != 0) return 1;
    extrinsic.fill (99.0f);
    int const extrinsicBefore = top.extrinsic + sse2.extrinsic;
    if (superldpc_extrinsic174_91_c (llr.data (), 2, 2.0f, extrinsic.data ()) != 0
        || !std::all_of (extrinsic.begin (), extrinsic.end (), [] (float v) { return v == 0; })
        || top.extrinsic + sse2.extrinsic != extrinsicBefore) return 1;
    superldpc_extrinsic174_91_c (nullptr, 2, 2.0f, extrinsic.data ());
    superldpc_extrinsic174_91_c (llr.data (), 2, 2.0f, nullptr);
    if (top.extrinsic + sse2.extrinsic != extrinsicBefore) return 1;

    // These may close/clear an existing dump after the UI toggle is off, but
    // only in the backend the CPU can run, and never on an unsupported CPU.
    superldpc_gate_dump_open_c ("unused-test-path");
    superldpc_gate_truth_set_c (codeword.data ());
    superldpc_gate_truth_clear_c ();
    superldpc_gate_dump_close_c ();
#if defined(DECODIUM_SUPERLDPC_TESTING)
    if (top.gate != (hardwareTier == 2 ? 4 : 0) || sse2.gate != (hardwareTier == 1 ? 4 : 0)) {
        std::cerr << "gate utilities bypass CPU guard\n";
        return 1;
    }
#endif
    std::cout << "safe scalar/batch/BICM/gate dispatch: " << scenario
              << " tier=" << expectedTier << '\n';
    return 0;
}
