// Minimal FT8 all-zero-codeword smoke test for an AVX2/FMA-capable CPU.
// Link against the real SIMD TU and FtxLdpc.cpp for the real parity tables.
// ftx_ft2_rvec_c is unused for this FT8 input and supplied as a stub.
// Linux GCC example (run from the repository root):
// g++ -O2 -ffunction-sections -fdata-sections -c Detector/FtxLdpc.cpp -o /tmp/ftx-ldpc.o
// g++ -std=c++17 -O3 -mavx2 -mfma -ffunction-sections -fdata-sections -DSUPERLDPC_HAVE_GATE_WEIGHTS=1 -c Detector/superldpc/decodium_bridge.cpp -o /tmp/superldpc.o
// g++ -O2 -Wl,--gc-sections tests/test_superldpc_simd_smoke.cpp /tmp/ftx-ldpc.o /tmp/superldpc.o -o /tmp/superldpc-smoke
// qemu-x86_64 -cpu Haswell,-hle,-rtm /tmp/superldpc-smoke
#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" void ftx_ft2_rvec_c(signed char* out) { std::memset(out, 0, 77); }
extern "C" void superldpc_simd_set_ft8_mode_c(int);
extern "C" void superldpc_simd_decode174_91_c(float const*, int, int, int,
    signed char const*, signed char*, signed char*, int*, int*, float*);

int main()
{
    float llr[174];
    std::fill_n(llr, 174, -10.0f); // strong evidence for the valid all-zero codeword
    signed char mask[174] {}, message[91] {}, cw[174] {};
    int ntype = -2, nhard = -2;
    float dmin = -2.0f;
    superldpc_simd_set_ft8_mode_c(1);
    superldpc_simd_decode174_91_c(llr, 91, 3, 3, mask, message, cw, &ntype, &nhard, &dmin);
    unsigned sum = 0;
    for (auto bit : cw) sum += static_cast<unsigned char>(bit);
    std::printf("ntype=%d nhard=%d dmin=%.3f cw_sum=%u\n", ntype, nhard, dmin, sum);
    return ntype == 1 && nhard == 0 && dmin == 0.0f && sum == 0 ? 0 : 1;
}
