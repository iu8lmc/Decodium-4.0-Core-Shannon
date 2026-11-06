// SPDX-License-Identifier: GPL-3.0-or-later
// Export a bounded, synthetic HAMDRM RS waveform for independent receivers.
#include "src/sstv/digital/HamDrmRsCodec.h"
#include "src/sstv/digital/HamDrmMotCodec.h"
#include "src/sstv/digital/waveform/HamDrmWaveformCodec.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::waveform;

int main(int argc, char** argv)
{
    if (argc != 4 || std::string(argv[1]).size() != 1U || argv[1][0] < '1' || argv[1][0] > '4') return 2;
    const auto level = static_cast<unsigned>(argv[1][0] - '0');
    std::ifstream input(argv[2], std::ios::binary);
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(input)), {});
    if (!input.is_open() || image.empty()) return 3;
    const auto rs = encodeHamDrmRs(image, "jpg", level);
    if (!rs.ok()) return 3;
    HamDrmMotObjectMetadata metadata;
    metadata.filename = "boundary.rs" + std::to_string(level);
    metadata.transportId = qsstvCompatibleTransportId(metadata.filename);
    const auto object = encodeHamDrmObject(metadata, *rs.value, 1024);
    if (!object.ok()) return 4;
    HamDrmWaveformConfig config;
    config.robustness = HamDrmRobustness::B;
    config.occupiedBandwidth = HamDrmOccupiedBandwidth::Hz2500;
    config.constellation = HamDrmConstellation::Qam16;
    config.protection = HamDrmProtection::High;
    config.interleaver = HamDrmInterleaver::Long;
    config.pcmSampleRateHz = 48000;
    HamDrmWaveformTransmitter tx(config);
    const auto headers = [&] {
        for (int repeat = 0; repeat < 8; ++repeat)
            for (const auto& group : object.value->headerGroups)
                if (!tx.enqueueMotDataGroup(group).ok()) return false;
        return true;
    };
    if (!headers()) return 5;
    for (const auto& group : object.value->bodyGroups)
        if (!tx.enqueueMotDataGroup(group).ok()) return 5;
    if (!headers() || !tx.finish().ok()) return 5;
    std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
    for (int i = 0; i < 48000; ++i) { output.put(0); output.put(0); }
    std::size_t count = 0;
    while (!tx.done()) {
        const auto block = tx.pullPcm(4800);
        if (block.empty() || (count += block.size()) > 48000U * 180U) return 6;
        for (double sample : block) {
            const auto value = static_cast<std::uint16_t>(static_cast<std::int16_t>(
                std::lround(std::clamp(sample * 0.5, -1.0, 1.0) * 32767.0)));
            output.put(static_cast<char>(value & 255U));
            output.put(static_cast<char>(value >> 8U));
        }
    }
    return output ? 0 : 7;
}
