#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int genericCalls = 0;
int simdCalls = 0;
int modeCalls = 0;
int gateCalls = 0;
int extrinsicCalls = 0;
int lastMode = -1;
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
extern "C" void superldpc_simd_set_ft8_mode_c (int mode)
{
    ++modeCalls;
    lastMode = mode;
}
extern "C" void superldpc_simd_decode174_91_c (
    float const*, int, int, int, signed char const*, signed char*,
    signed char*, int*, int*, float*) { ++simdCalls; }
extern "C" void superldpc_simd_decode174_91_batch_c (
    int, float const*, signed char const*, int, int, int, signed char*,
    signed char*, int*, int*, float*) { ++simdCalls; }
extern "C" int superldpc_simd_extrinsic174_91_c (float const*, int, float, float* out)
{
    ++extrinsicCalls;
    out[0] = 0.25f;
    return 1;
}
extern "C" void superldpc_simd_gate_dump_open_c (char const*) { ++gateCalls; }
extern "C" void superldpc_simd_gate_dump_close_c () { ++gateCalls; }
extern "C" void superldpc_simd_gate_truth_set_c (signed char const*) { ++gateCalls; }
extern "C" void superldpc_simd_gate_truth_clear_c () { ++gateCalls; }

extern "C" void superldpc_set_enabled_c (int);
extern "C" int superldpc_is_enabled_c ();
extern "C" void superldpc_set_ft8_mode_c (int);
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

int main (int argc, char** argv)
{
    std::string const scenario = argc > 1 ? argv[1] : "environment";
    bool const emergency = scenario == "environment";
    if (scenario == "ivybridge") cpuFeatures &= ~(2u | 4u); // AVX, no AVX2/FMA
    else if (scenario == "no-avx") cpuFeatures &= ~1u;
    else if (scenario == "no-avx2") cpuFeatures &= ~2u;
    else if (scenario == "no-fma") cpuFeatures &= ~4u;
    else if (scenario == "no-osxsave") cpuFeatures &= ~8u;
    else if (scenario == "no-ymm-state") cpuFeatures &= ~16u;
    else if (scenario != "environment" && scenario != "supported" && scenario != "ui-disabled")
        return 1;
#if defined(_WIN32)
    _putenv_s ("DECODIUM_FT2_DISABLE_SUPERLDPC", emergency ? "1" : "0");
#else
    setenv ("DECODIUM_FT2_DISABLE_SUPERLDPC", emergency ? "1" : "0", 1);
#endif

    // An enabled saved setting must not bypass CPU/OS or emergency guards.
    superldpc_set_enabled_c (scenario != "ui-disabled");
    bool const expectedSimd = scenario == "supported";
    if (superldpc_is_enabled_c () != int (expectedSimd)) {
        std::cerr << "incorrect runtime selection: " << scenario << '\n';
        return 1;
    }
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
    if (simdCalls != (expectedSimd ? 2 : 0) || genericCalls != (expectedSimd ? 0 : 3)
        || extrinsicCalls != int (expectedSimd) || accepted != int (expectedSimd)
        || modeCalls != (expectedSimd ? 3 : 0)
        || (expectedSimd && (lastMode != 1 || extrinsic[0] != 0.25f))
        || !std::all_of (extrinsic.begin () + (expectedSimd ? 1 : 0), extrinsic.end (),
                        [] (float value) { return value == 0.0f; })) {
        std::cerr << "scalar/batch/BICM dispatch regression: " << scenario << '\n';
        return 1;
    }
    // A UI change between checking availability and probing is also safe.
    superldpc_set_enabled_c (0);
    extrinsic.fill (99.0f);
    if (superldpc_extrinsic174_91_c (llr.data (), 2, 2.0f, extrinsic.data ()) != 0
        || !std::all_of (extrinsic.begin (), extrinsic.end (), [] (float v) { return v == 0; })
        || extrinsicCalls != int (expectedSimd)) return 1;
    superldpc_extrinsic174_91_c (nullptr, 2, 2.0f, extrinsic.data ());
    superldpc_extrinsic174_91_c (llr.data (), 2, 2.0f, nullptr);
    if (extrinsicCalls != int (expectedSimd)) return 1;

    // These may close/clear an existing dump after the UI toggle is off,
    // but may never enter the SIMD translation unit on an unsupported CPU.
    superldpc_gate_dump_open_c ("unused-test-path");
    superldpc_gate_truth_set_c (codeword.data ());
    superldpc_gate_truth_clear_c ();
    superldpc_gate_dump_close_c ();
#if defined(DECODIUM_SUPERLDPC_TESTING)
    if (gateCalls != (cpuFeatures == 31 ? 4 : 0)) {
        std::cerr << "gate utilities bypass CPU guard\n";
        return 1;
    }
#endif
    std::cout << "safe scalar/batch/BICM/gate dispatch: " << scenario << '\n';
    return 0;
}
