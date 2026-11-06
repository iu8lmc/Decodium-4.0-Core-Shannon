// SPDX-License-Identifier: GPL-3.0-or-later
// Encode an independently produced BSR as a laboratory HAMDRM audio signal.
#include "src/sstv/digital/HamDrmMotCodec.h"
#include "src/sstv/digital/waveform/HamDrmWaveformCodec.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::waveform;
int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<std::uint8_t> body((std::istreambuf_iterator<char>(input)), {});
    if (!input.eof() && !input.good()) return 1;
    if (body.empty() || body.size() > 131072) return 1;
    HamDrmMotObjectMetadata metadata;
    metadata.filename = "bsr.bin";
    metadata.transportId = qsstvCompatibleTransportId(metadata.filename);
    const auto object = encodeHamDrmObject(metadata, body, 1024);
    if (!object.ok()) return 1;
    HamDrmWaveformConfig config;
    config.robustness = HamDrmRobustness::B;
    config.occupiedBandwidth = HamDrmOccupiedBandwidth::Hz2500;
    config.constellation = HamDrmConstellation::Qam16;
    config.protection = HamDrmProtection::High;
    config.interleaver = HamDrmInterleaver::Long;
    config.pcmSampleRateHz = 48000;
    HamDrmWaveformTransmitter tx(config);
    for (const auto& group : object.value->headerGroups)
        if (!tx.enqueueMotDataGroup(group).ok()) return 1;
    for (const auto& group : object.value->bodyGroups)
        if (!tx.enqueueMotDataGroup(group).ok()) return 1;
    if (!tx.finish().ok()) return 1;
    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    std::size_t count = 0;
    while (!tx.done()) {
        const auto block = tx.pullPcm(4800);
        if (block.empty() || (count += block.size()) > 48000U * 120U) return 1;
        for (const auto sample : block) {
            const auto value = static_cast<std::uint16_t>(static_cast<std::int16_t>(
                std::lround(std::clamp(sample * 0.5, -1.0, 1.0) * 32767.0)));
            output.put(static_cast<char>(value & 255U));
            output.put(static_cast<char>(value >> 8U));
        }
    }
    std::cout << count << " samples\n";
    return output ? 0 : 1;
}
