// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only native MOT/packet vectors for the independent QSSTV RX probe.
#include "src/sstv/digital/HamDrmMotCodec.h"
#include "src/sstv/digital/channel/HamDrmChannelCrc.h"
#include "src/sstv/digital/waveform/HamDrmPacketCodec.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::waveform;
static void hex(const std::vector<std::uint8_t>& bytes) {
    for (auto b : bytes) std::printf("%02x", b);
}
int main(int argc, char** argv) {
    if (argc != 3) return 4;
    for (int variant=0; variant<2; ++variant) {
        HamDrmMotObjectMetadata meta;
        meta.transportId=0x2345U+variant;
        meta.filename=variant ? "image-b.jpg" : "image-a.jpg";
        std::ifstream input(argv[variant+1], std::ios::binary);
        if (!input) return 5;
        const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(input)), {});
        if (image.empty()) return 6;
        const auto encoded=encodeHamDrmObject(meta,image,50U);
        if (!encoded.ok()) return 1;
        for (std::size_t size : {16U, 64U}) {
            std::uint8_t continuity=0;
            auto groups=encoded.value->headerGroups;
            groups.insert(groups.end(), encoded.value->bodyGroups.begin(), encoded.value->bodyGroups.end());
            for (const auto& raw : groups) {
                const auto group=parseHamDrmMotDataGroup(raw.data(),raw.size());
                const auto packets=hamDrmPacketizeDataUnit(raw,{size,1U,16384U},continuity);
                if (!group.ok() || !packets.ok()) return 2;
                continuity=packets.value->nextContinuityIndex;
                auto corrupted=raw;
                corrupted.back() ^= 1U;
                const auto damaged=hamDrmPacketizeDataUnit(corrupted,{size,1U,16384U},0U);
                if (!damaged.ok()) return 3;
                for (int bad=0;bad<2;++bad) {
                    const auto& selected = bad ? *damaged.value : *packets.value;
                    std::printf("%d %u %u %u %u ",variant+2*bad,unsigned(group.value->kind),
                        unsigned(group.value->segmentNumber),unsigned(group.value->lastSegment),
                        unsigned(group.value->transportId));
                    hex(group.value->payload);
                    std::printf(" %zu",selected.packets.size());
                    for (const auto& packet:selected.packets) { std::putchar(' ');hex(packet); }
                    std::putchar('\n');
                }
                if (packets.value->packets.size() > 1U) {
                    auto broken=packets.value->packets;
                    auto fragment=broken[1];
                    fragment.resize(fragment.size()-2U);
                    fragment[0] ^= 1U;
                    broken[1]=channel::hamDrmAppendChannelCrc16(fragment);
                    std::printf("%d %u %u %u %u ",variant+4,unsigned(group.value->kind),
                        unsigned(group.value->segmentNumber),unsigned(group.value->lastSegment),
                        unsigned(group.value->transportId));
                    hex(group.value->payload);
                    std::printf(" %zu",broken.size());
                    for (const auto& packet:broken) { std::putchar(' '); hex(packet); }
                    std::putchar('\n');
                }
            }
        }
    }
    // Exercise wide PPI data units that require a padded non-final packet.
    // These are MOT groups rather than full JPEG objects; the independent
    // QSSTV methods must return their exact payload and metadata.
    for (int variant=0; variant<2; ++variant) {
        HamDrmMotDataGroup group;
        group.kind=HamDrmMotGroupKind::Body;
        group.transportId=static_cast<std::uint16_t>(0x3344U+variant);
        group.segmentNumber=static_cast<std::uint16_t>(variant);
        group.lastSegment=true;
        const std::size_t wanted=variant ? 300U : 256U;
        group.payload.resize(wanted-11U);
        for (std::size_t i=0;i<group.payload.size();++i)
            group.payload[i]=static_cast<std::uint8_t>(i*31U+variant);
        const auto raw=encodeHamDrmMotDataGroup(group);
        if (!raw.ok() || raw.value->size()!=wanted) return 7;
        const auto packets=hamDrmPacketizeDataUnit(*raw.value,{311U,1U,16384U},7U);
        if (!packets.ok() || packets.value->packets.size()!=2U
            || (packets.value->packets[0][0] & 0x48U)!=0x08U) return 8;
        auto corruptGroup=*raw.value;
        corruptGroup.back() ^= 1U;
        const auto damaged=hamDrmPacketizeDataUnit(corruptGroup,{311U,1U,16384U},7U);
        if (!damaged.ok()) return 9;
        auto broken=packets.value->packets;
        auto fragment=broken[1];
        fragment.resize(fragment.size()-2U);
        fragment[0] ^= 1U;
        broken[1]=channel::hamDrmAppendChannelCrc16(fragment);
        for (int mode=0; mode<3; ++mode) {
            const auto& selected = mode==0 ? packets.value->packets
                : mode==1 ? damaged.value->packets : broken;
            std::printf("%d %u %u %u %u ",variant+6+mode*2,unsigned(group.kind),
                        unsigned(group.segmentNumber),unsigned(group.lastSegment),
                        unsigned(group.transportId));
            hex(group.payload);
            std::printf(" %zu",selected.size());
            for (const auto& packet:selected) { std::putchar(' ');hex(packet); }
            std::putchar('\n');
        }
    }
}
