// SPDX-License-Identifier: GPL-3.0-or-later
// Generate Decodium FAC cells and expected bits for an external RX oracle.
#include "src/sstv/digital/channel/HamDrmFacCodec.h"
#include <cmath>
#include <cstdio>
using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::channel;
int main() {
    for (int frame=0;frame<3;++frame) for(int bandwidth=0;bandwidth<2;++bandwidth)
    for(int depth=0;depth<2;++depth) for(int constellation=0;constellation<3;++constellation)
    for(int protection=0;protection<2;++protection) for(int packet=0;packet<4;++packet) {
        HamDrmFacParameters p; p.frameIdentity=frame;
        p.occupiedBandwidth=bandwidth ? HamDrmOccupiedBandwidth::Hz2500 : HamDrmOccupiedBandwidth::Hz2300;
        p.interleaver=depth ? HamDrmInterleaver::Short : HamDrmInterleaver::Long;
        p.mscConstellation=constellation==0 ? HamDrmConstellation::Qam4 : (constellation==1 ? HamDrmConstellation::Qam16 : HamDrmConstellation::Qam64);
        p.protection=protection ? HamDrmProtection::Normal : HamDrmProtection::High;
        p.packetId=packet; p.callsign="TEST123";
        const auto bits=hamDrmEncodeFacPayloadBits(p); const auto cells=hamDrmEncodeFacCells(p);
        if (!bits.ok() || !cells.ok()) return 1;
        for(auto bit:*bits.value) std::putchar(bit ? '1' : '0');
        for(const auto& cell:*cells.value) std::printf(" %.17g %.17g",cell.real(),cell.imag());
        std::putchar('\n');
    }
}
