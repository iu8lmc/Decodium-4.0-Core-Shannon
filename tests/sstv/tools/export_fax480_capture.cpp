// SPDX-License-Identifier: GPL-3.0-or-later
// Synthesizes a known test capture with silence and a VOX-like tone.
#include "src/sstv/fax/SstvFax480.h"
#include <QImage>
#include <array>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
using namespace decodium::sstv;
int main(int argc,char** argv) {
    if (argc!=4) return 1;
    const bool dayton=std::string(argv[1])=="dayton-480";
    if (!dayton && std::string(argv[1])!="qsstv-500") return 2;
    const auto profile=dayton ? SstvFax480Profile::Dayton480 : SstvFax480Profile::Qsstv500;
    SstvFax480Mapper mapper(profile,48000);
    std::vector<std::uint8_t> pixels(std::size_t(mapper.height())*512U);
    QImage reference(512,int(mapper.height()),QImage::Format_Grayscale8);
    for (std::size_t i=0;i<pixels.size();++i) {
        const unsigned x=i%512U, y=i/512U;
        pixels[i]=static_cast<std::uint8_t>((x/64U+y/80U+(dayton ? 0U : 2U))%4U*70U+20U);
        reference.scanLine(int(y))[x]=pixels[i];
    }
    if (!reference.save(QString::fromLocal8Bit(argv[3]),"PNG")) return 3;
    std::ofstream out(argv[2],std::ios::binary);
    if (!out) return 4;
    constexpr double pi=3.14159265358979323846;
    for (unsigned i=0;i<96000U;++i) {
        const float v=i<48000U ? 0.0F : float(0.28*std::sin(2*pi*1700.0*i/48000.0));
        const std::array<float,2> stereo {{v,v}};
        out.write(reinterpret_cast<const char*>(stereo.data()),sizeof(stereo));
    }
    SstvFax480Encoder encoder(profile,48000,std::move(pixels));
    std::array<std::int16_t,4096> pcm {};
    while (!encoder.complete()) {
        const auto n=encoder.pull(pcm.data(),pcm.size());
        if (!n) return 5;
        for (std::size_t i=0;i<n;++i) {
            const float v=float(pcm[i])/32768.0F;
            const std::array<float,2> stereo {{v,v}};
            out.write(reinterpret_cast<const char*>(stereo.data()),sizeof(stereo));
        }
    }
    for (unsigned i=0;i<4800U;++i) {
        const std::array<float,2> zero {};
        out.write(reinterpret_cast<const char*>(zero.data()),sizeof(zero));
    }
    return out ? 0 : 6;
}
