// SPDX-License-Identifier: GPL-3.0-or-later
// Offline analysis of a float32 stereo BlackHole capture from a lab TX.
#include "src/sstv/fax/SstvFax480Receiver.h"
#include "src/sstv/fax/SstvFax480AutoRx.h"
#include <QImage>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace decodium::sstv;
struct ToneReference {
    std::array<double,144> cosine {},sine {};
    explicit ToneReference(double hz) {
        constexpr double pi=3.14159265358979323846;
        for (std::size_t j=0;j<cosine.size();++j) {
            const double phase=2*pi*hz*double(j)/48000.0;
            cosine[j]=std::cos(phase); sine[j]=std::sin(phase);
        }
    }
};
static double power(const std::vector<float>& pcm, std::size_t at, const ToneReference& ref, std::size_t length=72U) {
    double real=0,imag=0;
    for (std::size_t j=0;j<length;++j) {
        real+=pcm[(at+j)*2U]*ref.cosine[j]; imag+=pcm[(at+j)*2U]*ref.sine[j];
    }
    return real*real+imag*imag;
}

int main(int argc,char** argv) {
    if (argc!=4 && argc!=5) return 1;
    const bool autoMode=argc==5 && std::string(argv[4])=="auto";
    if (argc==5 && !autoMode) return 1;
    const bool dayton=std::string(argv[1])=="dayton-480";
    if (!dayton && std::string(argv[1])!="qsstv-500") return 2;
    const auto profile=dayton ? SstvFax480Profile::Dayton480 : SstvFax480Profile::Qsstv500;
    std::ifstream file(argv[2],std::ios::binary|std::ios::ate);
    if (!file) return 3;
    const auto bytes=file.tellg(); if (bytes<=0 || bytes%8!=0) return 4;
    std::vector<float> pcm(std::size_t(bytes)/sizeof(float));
    file.seekg(0); file.read(reinterpret_cast<char*>(pcm.data()),bytes); if (!file) return 5;
    const std::size_t frames=pcm.size()/2U;
    std::size_t start=frames;
    const ToneReference ref15(1500),ref23(2300);
    // Coarse location of the alternating header; refine from the first
    // line-sync pulse so a VOX pre-key tone cannot shift the image anchor.
    for (std::size_t s=0;s+11000U<frames && s<20U*48000U;s+=12U) {
        int good=0;
        for (int j=0;j<24;++j) {
            const auto at=s+std::size_t(std::llround((double(j)*0.00205+0.00027)*48000.0));
            const double p15=power(pcm,at,ref15),p23=power(pcm,at,ref23);
            const bool wantWhite=dayton ? j%2==0 : j%2!=0;
            if ((wantWhite ? p23>p15*1.25 : p15>p23*1.25) && p15+p23>0.05) ++good;
        }
        if (good>=23) { start=s; break; }
    }
    if (start==frames) { std::cerr<<"header not found\n"; return 6; }
    const ToneReference ref12(1200);
    const auto searchBegin=start+4U*48000U+43200U;
    const auto searchEnd=std::min(frames-144U,start+6U*48000U);
    std::size_t runStart=0, runEnd=0, syncCenter=0;
    for (std::size_t at=searchBegin;at<searchEnd;at+=24U) {
        const double p12=power(pcm,at,ref12,144U);
        const double p15=power(pcm,at,ref15,144U);
        const double p23=power(pcm,at,ref23,144U);
        const bool match=p12>1.0 && p12>2.0*p15 && p12>2.0*p23;
        if (match) { if (!runStart) runStart=at; runEnd=at; }
        else if (runStart) {
            if (runEnd-runStart>=48U) { syncCenter=(runStart+runEnd)/2U+72U; break; }
            runStart=runEnd=0;
        }
    }
    if (!syncCenter) { std::cerr<<"first sync not found\n";return 10; }
    const double syncOffsetSeconds=5.002 + (dayton ? 0.00256 : 0.267266-0.00256);
    const auto syncOffset=std::uint64_t(std::llround(syncOffsetSeconds*48000.0));
    if (syncCenter<syncOffset) return 11;
    start=syncCenter-syncOffset;
    SstvFax480Mapper mapper(profile,48000);
    if (frames-start<mapper.totalSamples()) { std::cerr<<"capture truncated\n";return 7; }
    QImage reference(QString::fromLocal8Bit(argv[3]));
    reference=reference.convertToFormat(QImage::Format_Grayscale8);
    if (reference.width()!=512 || reference.height()!=int(mapper.height())) return 8;
    const auto cfg=SstvFrequencyDemodulatorConfig::sstvDefaults();
    SstvFrequencyDemodulator demod(cfg);
    SstvFax480Receiver receiver(profile,12000);
    SstvFax480AutoRx automatic(profile,12000);
    const SstvFax480Mapper rxMapper(profile,12000);
    const std::size_t first=autoMode ? 0U : start;
    const std::size_t count=autoMode ? (frames/4U) : rxMapper.totalSamples();
    std::vector<float> mono(4096);
    for (std::size_t offset=0;offset<count;offset+=mono.size()) {
        const auto n=std::min<std::size_t>(mono.size(),count-offset);
        for (std::size_t i=0;i<n;++i) mono[i]=pcm[(first+4U*(offset+i))*2U];
        const auto obs=demod.consume(mono.data(),n);
        if (autoMode) automatic.consume(obs.data(),obs.size());
        else receiver.consume(obs.data(),obs.size());
    }
    if (autoMode && (!automatic.acquired() || !automatic.complete())) {
        std::cerr<<"automatic acquisition incomplete\n"; return 12;
    }
    const auto image=autoMode ? automatic.image() : receiver.image();
    const auto coverage=autoMode ? automatic.coverage() : receiver.coverage();
    double error=0;std::size_t seen=0;
    for (std::size_t i=0;i<image.size();++i) if (coverage[i]) {
        const auto expected=reference.constScanLine(int(i/512U))[i%512U];
        error+=std::abs(int(image[i])-int(expected));++seen;
    }
    QImage decoded(512,int(mapper.height()),QImage::Format_Grayscale8);
    for (std::size_t i=0;i<image.size();++i) decoded.scanLine(int(i/512U))[i%512U]=image[i];
    decoded.save(QString::fromLocal8Bit(argv[2])+QStringLiteral(".png"));
    const double ratio=double(seen)/image.size(), mae=seen ? error/seen : 999;
    std::cout<<"header sample="<<start<<" coverage="<<ratio<<" MAE="<<mae<<"\n";
    return ratio>0.90 && mae<12.0 ? 0 : 9;
}
