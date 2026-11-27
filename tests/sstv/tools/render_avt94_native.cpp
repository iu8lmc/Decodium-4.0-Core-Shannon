// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded file adapter for comparing the Decodium AVT encoder with peers.
#include "src/sstv/analog/SstvAvt.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    using namespace decodium::sstv;
    if (argc != 4 || (std::string(argv[3]) != "qrm" && std::string(argv[3]) != "narrow-qrm"
                      && std::string(argv[3]) != "narrow")) {
        std::cerr << "usage: render_avt94_native RGB OUTPUT.f32 qrm|narrow-qrm|narrow\n";
        return 2;
    }
    if (std::filesystem::exists(argv[2]) || std::filesystem::is_symlink(argv[2])) return 3;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<std::uint8_t> rgb(320U * 200U * 3U);
    if (!input.read(reinterpret_cast<char*>(rgb.data()), std::streamsize(rgb.size()))
        || input.peek() != std::char_traits<char>::eof()) return 4;
    std::vector<SstvRgbPixel> pixels;
    pixels.reserve(320U * 200U);
    for (std::size_t i = 0; i < rgb.size(); i += 3U)
        pixels.push_back({rgb[i], rgb[i + 1U], rgb[i + 2U]});
    SstvAvtEncoderConfig config;
    config.mode = SstvAvtMode::Avt94;
    config.sampleRate = 48000U;
    const std::string variant(argv[3]);
    config.variant = variant == "narrow-qrm" ? SstvAvtVariant::NarrowQrm
        : variant == "narrow" ? SstvAvtVariant::Narrow : SstvAvtVariant::Qrm;
    config.terminalHoldMs = 10U;
    SstvAvtEncoder encoder(pixels, config);
    std::ofstream output(argv[2], std::ios::binary);
    std::array<float, 8192U> samples {};
    const std::vector<float> silence(24000U);
    output.write(reinterpret_cast<const char*>(silence.data()), std::streamsize(silence.size() * sizeof(float)));
    while (!encoder.complete()) {
        const auto count = encoder.pullFloat(samples.data(), samples.size());
        if (!count) return 5;
        output.write(reinterpret_cast<const char*>(samples.data()), std::streamsize(count * sizeof(float)));
    }
    output.write(reinterpret_cast<const char*>(silence.data()), std::streamsize(silence.size() * sizeof(float)));
    return output ? 0 : 6;
}
