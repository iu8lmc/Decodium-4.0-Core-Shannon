// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only probe invoking the pinned QSSTV OFDM modulator.
#include "OFDM.h"
#include "logging.h"
#include <cstdio>
#include <cstdint>
logFile* logFilePtr = nullptr;
void logFile::add(const char*, const char*, int, QString, unsigned short) {}
class Probe : public COFDMModulation {
public:
    void setup(CParameter& p) { SetCarOffset(350.0); InitInternal(p); }
    int inputLength() const { return iInputBlockSize; }
    int outputLength() const { return iOutputBlockSize; }
    void run(CParameter& p, CVectorEx<_COMPLEX>& in, CVectorEx<_COMPLEX>& out) {
        pvecInputData = &in; pvecOutputData = &out; ProcessDataInternal(p);
    }
};
int main() {
    const ERobMode modes[] = {RM_ROBUSTNESS_MODE_A, RM_ROBUSTNESS_MODE_B, RM_ROBUSTNESS_MODE_E};
    for (int mode=0; mode<3; ++mode) for (int bandwidth=0; bandwidth<2; ++bandwidth) for (int variant=0; variant<2; ++variant) {
        CParameter p; p.CellMappingTable.MakeTable(modes[mode], bandwidth ? SO_1 : SO_0);
        Probe probe; probe.setup(p);
        CVectorEx<_COMPLEX> in, out; in.Init(probe.inputLength()); out.Init(probe.outputLength());
        std::uint32_t seed=0x4f46444dU + static_cast<std::uint32_t>(variant);
        for (int i=0;i<probe.inputLength();++i) {
            seed=seed*1664525U+1013904223U; const double re=(seed & 0x80000000U) ? 1.0 : -1.0;
            seed=seed*1664525U+1013904223U; const double im=(seed & 0x80000000U) ? 1.0 : -1.0;
            in[i]=_COMPLEX(re,im);
        }
        probe.run(p,in,out);
        std::printf("%d %d %d %d %d\n",mode,bandwidth,variant,probe.inputLength(),probe.outputLength());
        for(int i=0;i<probe.outputLength();++i)
            std::printf("%.17g %.17g\n",double(out[i].real()),double(out[i].imag()));
    }
}
