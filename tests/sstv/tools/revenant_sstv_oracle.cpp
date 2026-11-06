// SPDX-License-Identifier: GPL-3.0-or-later
// Narrow file adapter for the separately checked-out Revenant SSTV codec.
// Audio is mono 48 kHz little-endian float; images are raw RGB or PPM.
#include "core/decode/sstv.h"
#include "core/dsp/synth/sstv_mod.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

using namespace revenant;

int main(int argc, char** argv)
{
    if (argc != 5) {
        std::cerr << "usage: decode|render INPUT OUTPUT MODE\n";
        return 2;
    }
    const std::string action = argv[1];
    if (action == "render") {
        const auto* mode = decode::sstv_mode_by_name(argv[4]);
        if (!mode) return 3;
        std::ifstream input(argv[2], std::ios::binary);
        if (!input) return 7;
        const std::vector<std::uint8_t> rgb((std::istreambuf_iterator<char>(input)), {});
        auto samples = siggen::sstv_render(siggen::SstvModConfig{}, *mode, rgb);
        if (!samples) {
            std::cerr << samples.error().message << '\n';
            return 4;
        }
        std::ofstream output(argv[3], std::ios::binary);
        output.write(reinterpret_cast<const char*>(samples->data()),
                     static_cast<std::streamsize>(samples->size() * sizeof(float)));
        return output ? 0 : 5;
    }
    if (action != "decode") return 2;
    auto decoder = decode::SstvDecoder::create({48'000});
    if (!decoder) return 6;
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) return 7;

    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0, rows = 0;
    std::size_t components = 0;
    int opened = 0;
    bool complete = false;
    std::string label;
    decode::ImageSink sink;
    sink.on_open = [&](const auto& image) {
        ++opened;
        width = image.width;
        components = image.model == decode::ImageRow::Gray8 ? 1U : 3U;
        label = image.label;
        pixels.clear();
    };
    sink.on_row = [&](const auto& row) {
        if ((row.model == decode::ImageRow::Gray8 ? 1U : 3U) != components
            || row.width != width || row.bytes.size() != width * components) return;
        const auto end = static_cast<std::size_t>(row.row + 1U) * width * components;
        if (pixels.size() < end) pixels.resize(end);
        std::copy(row.bytes.begin(), row.bytes.end(), pixels.begin() + row.row * width * components);
    };
    sink.on_close = [&](const auto& image) {
        complete = image.complete;
        rows = image.rows;
    };
    std::vector<float> block(8192);
    while (input.read(reinterpret_cast<char*>(block.data()),
                      static_cast<std::streamsize>(block.size() * sizeof(float))) || input.gcount()) {
        const auto bytes = input.gcount();
        if (bytes % sizeof(float)) return 10;
        decoder->process(std::span<const float>(block.data(),
                         static_cast<std::size_t>(bytes) / sizeof(float)), sink);
    }
    decoder->flush(sink);
    const auto stats = decoder->stats();
    std::cerr << "mode=" << label << " opened=" << opened << " complete=" << complete
              << " rows=" << rows << " width=" << width << " vis=" << stats.last_vis
              << " ratio=" << std::setprecision(12) << stats.last_period_ratio
              << " offset_hz=" << stats.last_offset_hz << '\n';
    if (!complete || opened != 1 || label != argv[4]
        || pixels.size() != static_cast<std::size_t>(width) * rows * components) return 8;
    std::ofstream output(argv[3], std::ios::binary);
    output << (components == 1U ? "P5\n" : "P6\n") << width << ' ' << rows << "\n255\n";
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    return output ? 0 : 9;
}
