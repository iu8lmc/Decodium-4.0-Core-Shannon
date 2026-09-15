#include <array>
#include <cstdlib>
#include <iostream>

namespace {

int genericCalls = 0;
int simdCalls = 0;

} // namespace

extern "C" void ftx_decode174_91_c (float const*, int, int, int,
                                      signed char const*, signed char*,
                                      signed char*, int* ntype, int* nhard,
                                      float* dmin)
{
    ++genericCalls;
    if (ntype) *ntype = 1;
    if (nhard) *nhard = 0;
    if (dmin) *dmin = 0.0f;
}

extern "C" void fastldpc_simd_set_ft8_mode_c (int) {}

extern "C" void fastldpc_simd_decode174_91_c (
    float const*, int, int, int, signed char const*, signed char*,
    signed char*, int*, int*, float*)
{
    ++simdCalls;
}

extern "C" void fastldpc_simd_decode174_91_batch_c (
    int, float const*, signed char const*, int, int, int, signed char*,
    signed char*, int*, int*, float*)
{
    ++simdCalls;
}

extern "C" void fastldpc_set_enabled_c (int);
extern "C" int fastldpc_is_enabled_c ();
extern "C" void fastldpc_decode174_91_c (
    float const*, int, int, int, signed char const*, signed char*,
    signed char*, int*, int*, float*);
extern "C" void fastldpc_decode174_91_batch_c (
    int, float const*, signed char const*, int, int, int, signed char*,
    signed char*, int*, int*, float*);

int main ()
{
#if defined(_WIN32)
    _putenv_s ("DECODIUM_FT2_DISABLE_FASTLDPC", "1");
#else
    setenv ("DECODIUM_FT2_DISABLE_FASTLDPC", "1", 1);
#endif

    // Simula il caricamento di una configurazione con il toggle acceso.
    fastldpc_set_enabled_c (1);
    if (fastldpc_is_enabled_c () != 0) {
        std::cerr << "the saved UI setting overrode the emergency environment switch\n";
        return 1;
    }

    std::array<float, 2 * 174> llr {};
    std::array<signed char, 2 * 174> mask {};
    std::array<signed char, 2 * 91> message {};
    std::array<signed char, 2 * 174> codeword {};
    std::array<int, 2> ntype {};
    std::array<int, 2> nhard {};
    std::array<float, 2> dmin {};

    fastldpc_decode174_91_c (llr.data (), 91, 3, 3, mask.data (),
                             message.data (), codeword.data (), ntype.data (),
                             nhard.data (), dmin.data ());
    fastldpc_decode174_91_batch_c (
        2, llr.data (), mask.data (), 91, 3, 3, message.data (),
        codeword.data (), ntype.data (), nhard.data (), dmin.data ());

    if (simdCalls != 0 || genericCalls != 3) {
        std::cerr << "emergency fallback did not route all decodes to the generic backend"
                  << " (SIMD=" << simdCalls << ", generic=" << genericCalls << ")\n";
        return 1;
    }

    std::cout << "environment disable overrides the saved LDPC toggle\n";
    return 0;
}
