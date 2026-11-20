// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded diagnostic A/B waveform; output is mono s16le, not RF.
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

static std::vector<std::uint8_t> group(std::size_t size, std::uint16_t id)
{
    HamDrmMotDataGroup value;
    value.kind = HamDrmMotGroupKind::Body;
    value.continuityIndex = 3U;
    value.segmentNumber = 7U;
    value.lastSegment = true;
    value.transportId = id;
    value.payload.resize(size);
    std::uint32_t state = 0x13579BDFU;
    for (auto& byte : value.payload) {
        state = state * 1'664'525U + 1'013'904'223U;
        byte = static_cast<std::uint8_t>(state >> 24U);
    }
    const auto encoded = encodeHamDrmMotDataGroup(value);
    if (!encoded.ok()) throw std::runtime_error("cannot encode test group");
    return *encoded.value;
}

int main(int argc, char** argv)
{
    if ((argc < 3 || argc > 5) || (std::string(argv[1]) != "short" && std::string(argv[1]) != "long")
        || (argc >= 4 && std::string(argv[3]) != "48000")) {
        std::cerr << "expected short|long output.s16 [48000 [echo-gain]]\n";
        return 2;
    }
    try {
        HamDrmWaveformConfig config;
        config.robustness = HamDrmRobustness::A;
        config.occupiedBandwidth = HamDrmOccupiedBandwidth::Hz2500;
        config.constellation = HamDrmConstellation::Qam16;
        config.protection = HamDrmProtection::Normal;
        config.interleaver = std::string(argv[1]) == "short"
            ? HamDrmInterleaver::Short : HamDrmInterleaver::Long;
        if (argc >= 4) config.pcmSampleRateHz = 48000U;
        const double echo = argc == 5 ? std::stod(argv[4]) : 0.3;
        if (!std::isfinite(echo) || echo < 0.0 || echo > 0.5)
            throw std::runtime_error("echo gain out of range");
        HamDrmWaveformTransmitter tx(config);
        if (!tx.enqueueMotDataGroup(group(17U, 0x7001U)).ok()
            || !tx.enqueueMotDataGroup(group(29U, 0x7101U)).ok() || !tx.finish().ok())
            throw std::runtime_error("transmitter setup failed");
        std::vector<double> samples;
        while (!tx.done()) {
            const auto block = tx.pullPcm(4800U);
            if (block.empty()) throw std::runtime_error("transmitter stalled");
            samples.insert(samples.end(), block.begin(), block.end());
            if (samples.size() > 48000U * 60U) throw std::runtime_error("waveform bound exceeded");
        }
        std::ifstream existing(argv[2], std::ios::binary);
        if (existing.good()) throw std::runtime_error("output already exists");
        std::ofstream output(argv[2], std::ios::binary);
        if (!output) throw std::runtime_error("cannot open output");
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const std::size_t delay = config.pcmSampleRateHz / 1000U;
            const double value = 0.5 * (samples[i] + (i >= delay ? echo * samples[i-delay] : 0.0));
            const auto signedSample = static_cast<std::int16_t>(std::lround(std::clamp(value, -1.0, 1.0) * 32767.0));
            const auto encoded = static_cast<std::uint16_t>(signedSample);
            output.put(static_cast<char>(encoded & 0xffU));
            output.put(static_cast<char>(encoded >> 8U));
        }
        if (!output) throw std::runtime_error("output write failed");
        std::cout << "wrote " << samples.size() << " samples; groups 0x7001 and 0x7101\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
