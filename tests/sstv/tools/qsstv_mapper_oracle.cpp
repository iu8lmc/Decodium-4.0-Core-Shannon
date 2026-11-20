// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only harness calling QSSTV's symbol cell mapper.
#include "ofdmcellmapping/OFDMCellMapping.h"
#include "logging.h"
#include <cstdio>
logFile* logFilePtr = nullptr;
void logFile::add(const char*, const char*, int, QString, unsigned short) {}
class Probe : public COFDMCellMapping {
public:
    void setup(CParameter& p) { InitInternal(p); }
    int msc() const { return iInputBlockSize; }
    int fac() const { return iInputBlockSize2; }
    int carriers() const { return iOutputBlockSize; }
    void run(CParameter& p, CVectorEx<_COMPLEX>& a, CVectorEx<_COMPLEX>& b, CVectorEx<_COMPLEX>& out) {
        pvecInputData = &a; pvecInputData2 = &b; pvecOutputData = &out; ProcessDataInternal(p);
    }
};
int main() {
    const ERobMode modes[] = {RM_ROBUSTNESS_MODE_A, RM_ROBUSTNESS_MODE_B, RM_ROBUSTNESS_MODE_E};
    for(int mode=0;mode<3;++mode) for(int width=0;width<2;++width) for(int variant=0;variant<2;++variant) {
        CParameter p; p.CellMappingTable.MakeTable(modes[mode], width ? SO_1 : SO_0); p.eMSCCodingScheme=CS_3_SM;
        Probe probe; probe.setup(p);
        const int symbols=p.CellMappingTable.iNumSymbolsPerSuperframe;
        std::printf("%d %d %d %d %d\n",mode,width,variant,symbols,probe.carriers());
        for(int symbol=0;symbol<symbols;++symbol) {
            CVectorEx<_COMPLEX> a,b,out; a.Init(probe.msc()); b.Init(probe.fac()); out.Init(probe.carriers());
            for(int i=0;i<probe.msc();++i) a[i]=_COMPLEX(double(i+1)/100.0,double(symbol+variant+1)/-100.0);
            for(int i=0;i<probe.fac();++i) b[i]=_COMPLEX(double(i+variant+1)/-100.0,double(symbol+1)/100.0);
            std::printf("%d %d\n",probe.msc(),probe.fac());
            probe.run(p,a,b,out);
            for(int i=0;i<probe.carriers();++i) std::printf("%.17g %.17g\n",double(out[i].real()),double(out[i].imag()));
        }
    }
}
