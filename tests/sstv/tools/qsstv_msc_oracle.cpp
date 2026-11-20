// SPDX-License-Identifier: GPL-3.0-or-later
// Developer oracle invokes QSSTV's MSC encoder; not linked into Decodium.
#include "MLC.h"
#include "logging.h"
// Unused QSSTV buffer-interface logging, supplied only to link its vtable.
logFile* logFilePtr = nullptr;
void logFile::add(const char*, const char*, int, QString, unsigned short) {}
#include <cmath>
#include <cstdint>
#include <cstdio>
class Probe : public CMSCMLCEncoder {
public:
    void setup(CParameter& parameters) { InitInternal(parameters); }
    int inputLength() const { return iInputBlockSize; }
    int outputLength() const { return iOutputBlockSize; }
    void run(CParameter& parameters, CVectorEx<_BINARY>& input, CVectorEx<_COMPLEX>& output) {
        pvecInputData = &input;
        pvecOutputData = &output;
        ProcessDataInternal(parameters);
    }
};
int main() {
    CParameter parameters;
    const int counts[] = {339,369,455,530,647,704};
    for (int cells : counts) for (int constellation=0; constellation<3; ++constellation)
    for (int protection=0; protection<2; ++protection) {
        parameters.CellMappingTable.iNumUsefMSCCellsPerFrame = cells;
        parameters.eMSCCodingScheme = constellation == 0 ? CS_1_SM : (constellation == 1 ? CS_2_SM : CS_3_SM);
        parameters.MSCPrLe.iPartB = protection;
        Probe probe; probe.setup(parameters);
        CVectorEx<_BINARY> input;
        CVectorEx<_COMPLEX> output;
        input.Init(probe.inputLength()); output.Init(probe.outputLength());
        std::uint32_t seed = 0x44524d31U;
        for (int i=0; i<probe.inputLength(); ++i) {
            seed = seed * 1664525U + 1013904223U;
            input[i] = static_cast<_BINARY>((seed >> 31U) & 1U);
        }
        probe.run(parameters,input,output);
        const double scale = std::sqrt(constellation == 0 ? 2.0 : (constellation == 1 ? 10.0 : 42.0));
        std::printf("%d %d %d %d ",cells,constellation,protection,probe.inputLength());
        for (int i=0; i<probe.outputLength(); ++i) {
            std::putchar('A' + static_cast<int>(std::lround(output[i].real() * scale)) + 7);
            std::putchar('A' + static_cast<int>(std::lround(output[i].imag() * scale)) + 7);
        }
        std::puts("");
    }
}
