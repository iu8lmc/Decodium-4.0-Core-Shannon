// SPDX-License-Identifier: GPL-3.0-or-later
// Decode a 12 kHz libsstv-generated FAX480 PCM stream with native RX.
#include "src/sstv/fax/SstvFax480AutoRx.h"
#include "src/sstv/dsp/SstvFrequencyDemodulator.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace decodium::sstv;
int main(int argc,char** argv) {
    if (argc!=3) return 1;
    const int variant=std::stoi(argv[2]); if (variant<0 || variant>1) return 2;
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<unsigned char> raw((std::istreambuf_iterator<char>(file)),{});
    if (raw.empty() || raw.size()%2) return 3;
    SstvFrequencyDemodulator demod(SstvFrequencyDemodulatorConfig::sstvDefaults());
    SstvFax480AutoRx rx(SstvFax480Profile::LibsstvVis480,12000);
    std::vector<float> samples(4096);
    for (std::size_t offset=0;offset<raw.size()/2;offset+=samples.size()) {
        const auto n=std::min(samples.size(),raw.size()/2-offset);
        for (std::size_t i=0;i<n;++i) {
            const std::uint16_t bits=raw[2*(offset+i)] | (std::uint16_t(raw[2*(offset+i)+1])<<8U);
            samples[i]=float(bits<32768U ? int(bits) : int(bits)-65536)/32768.0F;
        }
        const auto obs=demod.consume(samples.data(),n);
        rx.consume(obs.data(),obs.size());
    }
    if (!rx.acquired()) { std::cerr<<"VIS 85 not acquired\n";return 4; }
    const auto image=rx.image(), coverage=rx.coverage();
    std::size_t seen=0;double sum=0;
    for (std::size_t i=0;i<image.size();++i) if (coverage[i]) {
        const unsigned x=i%512U,y=i/512U;
        const int expected=(((x/64U)+(y/80U)+unsigned(variant)*2U)%4U)*60U+35U;
        sum+=std::abs(int(image[i])-expected);++seen;
    }
    std::cout<<"anchor="<<*rx.anchor()<<" coverage="<<double(seen)/image.size()
             <<" MAE="<<sum/seen<<"\n";
    return seen>image.size()*9U/10U && sum/seen<12.0 ? 0 : 5;
}
