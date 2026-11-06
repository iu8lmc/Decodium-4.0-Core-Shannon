// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic non-RF stress audio: matching FAC/pilots, conflicting MSC data.
#include "src/sstv/digital/HamDrmMotCodec.h"
#include "src/sstv/digital/waveform/HamDrmWaveformCodec.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace decodium::sstv::hamdrm;
using namespace decodium::sstv::hamdrm::waveform;

static std::vector<double> makeWaveform(const HamDrmWaveformConfig& config, bool alternate)
{
    HamDrmWaveformTransmitter tx(config);
    for (std::uint16_t i = 0; i < 60U; ++i) {
        HamDrmMotDataGroup group;
        group.kind = HamDrmMotGroupKind::Body;
        group.continuityIndex = i % 8U;
        group.segmentNumber = i;
        group.lastSegment = i == 59U;
        group.transportId = alternate ? 0x7001U : 0x1001U;
        group.payload.resize(64U);
        std::uint32_t state = 0x31415926U + i * 97U;
        for (auto& byte : group.payload) {
            state = state * 1'664'525U + 1'013'904'223U;
            byte = static_cast<std::uint8_t>((state >> 24U) ^ (alternate ? 0xffU : 0U));
        }
        const auto encoded = encodeHamDrmMotDataGroup(group);
        if (!encoded.ok() || !tx.enqueueMotDataGroup(*encoded.value).ok())
            throw std::runtime_error("could not enqueue stress group");
    }
    if (!tx.finish().ok()) throw std::runtime_error("could not finish stress input");
    std::vector<double> pcm;
    while (!tx.done()) {
        auto part = tx.pullPcm(4800U);
        if (part.empty() || pcm.size() + part.size() > 48000U * 60U)
            throw std::runtime_error("stress waveform stalled or exceeded bound");
        pcm.insert(pcm.end(), part.begin(), part.end());
    }
    return pcm;
}

int main(int argc, char** argv)
{
    if (argc != 3 || (std::string(argv[1]) != "short" && std::string(argv[1]) != "long")) {
        std::cerr << "usage: export_hamdrm_crc_stress short|long output.s16\n";
        return 2;
    }
    try {
        HamDrmWaveformConfig config;
        config.constellation = HamDrmConstellation::Qam64;
        config.protection = HamDrmProtection::Normal;
        config.interleaver = std::string(argv[1]) == "short"
            ? HamDrmInterleaver::Short : HamDrmInterleaver::Long;
        config.pcmSampleRateHz = 48000U;
        const auto a = makeWaveform(config, false), b = makeWaveform(config, true);
        if (a.size() != b.size()) throw std::runtime_error("different waveform lengths");
        std::ifstream existing(argv[2], std::ios::binary);
        if (existing.good()) throw std::runtime_error("output already exists");
        std::ofstream out(argv[2], std::ios::binary);
        if (!out) throw std::runtime_error("cannot open output");
        for (std::size_t i = 0; i < a.size(); ++i) {
            const double value = std::clamp((a[i] + b[i]) * 0.25, -1.0, 1.0);
            const auto word = static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(value * 32767.0)));
            out.put(static_cast<char>(word & 0xffU));
            out.put(static_cast<char>(word >> 8U));
        }
        if (!out) throw std::runtime_error("cannot write output");
        std::cout << "wrote " << a.size() << " samples\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
