// SPDX-License-Identifier: GPL-3.0-or-later
// Export deterministic native MSC bit patterns and cells for an external RX.
#include "src/sstv/digital/channel/HamDrmMlcCodec.h"
#include <cstdint>
#include <cstdio>
using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::channel;
int main() {
    for(int count : {339,369,455,530,647,704}) for(int mode=0;mode<3;++mode)
    for(int protection=0;protection<2;++protection) for(int variant=0;variant<2;++variant) {
        const auto constellation=mode==0 ? HamDrmConstellation::Qam4 : (mode==1 ? HamDrmConstellation::Qam16 : HamDrmConstellation::Qam64);
        const auto profile=hamDrmMlcProfile(count, constellation, protection ? HamDrmProtection::Normal : HamDrmProtection::High);
        if(!profile.ok()) return 1;
        std::vector<std::uint8_t> bits(profile.value->inputBits);
        std::uint32_t seed=0x44524d31U + static_cast<std::uint32_t>(variant);
        for(auto& bit:bits) { seed=seed*1664525U+1013904223U; bit=static_cast<std::uint8_t>(seed>>31U); }
        const auto cells=hamDrmEncodeMscCells(bits,*profile.value); if(!cells.ok()) return 2;
        std::printf("%d %d %d %d ",count,mode,protection,variant);
        for(auto bit:bits) std::putchar(bit ? '1' : '0');
        for(const auto& cell:*cells.value) std::printf(" %.17g %.17g",cell.real(),cell.imag());
        std::putchar('\n');
    }
}
