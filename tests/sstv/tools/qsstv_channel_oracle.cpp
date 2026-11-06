// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone oracle linked to pinned QSSTV sources; not linked into Decodium.
#include "ConvEncoder.h"
#include "BitInterleaver.h"
#include <cstdio>
#include <cstdint>
int main() {
    CConvEncoder encoder;
    encoder.Init(CS_1_SM, CT_FAC, 0, 45, 0, 48, 0, 6, 0);
    CVector<_DECISION> input(48), output(90);
    std::uint32_t state = 0xC0DEC0DEU;
    for (int i=0; i<48; ++i) {
        state = state * 1664525U + 1013904223U;
        input[i] = BitToSoft((state >> 31U) & 1U);
    }
    const auto count = encoder.Encode(input, output);
    std::printf("%d ", count);
    for (int i=0; i<count; ++i) std::putchar(ExtractBit(output[i]) ? '1' : '0');
    std::puts("");
    CBitInterleaver interleaver;
    interleaver.Init(0, 254, 13);
    CVector<_DECISION> bits(254);
    state = 0x11223344U;
    for (int i=0; i<254; ++i) {
        state = state * 1664525U + 1013904223U;
        bits[i] = BitToSoft((state >> 31U) & 1U);
    }
    interleaver.Interleave(bits);
    for (int i=0; i<254; ++i) std::putchar(ExtractBit(bits[i]) ? '1' : '0');
    std::puts("");
}
