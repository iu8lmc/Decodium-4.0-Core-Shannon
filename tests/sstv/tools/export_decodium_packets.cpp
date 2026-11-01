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
}
