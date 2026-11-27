#include "../src/bridge/WaterfallRingPosition.hpp"
#include <array>
#include <chrono>
#include <iostream>

int main()
{
    constexpr int capacity=16384, count=1200, repeats=30000;
    std::array<short,capacity> before{}, after{};
    std::array<short,count> samples{};
    for(int i=0;i<count;++i) samples[i]=static_cast<short>((i*37)%32768);
    int oldPos=capacity-23,newPos=oldPos;
    auto t0=std::chrono::steady_clock::now();
    for(int n=0;n<repeats;++n) {
        for(int i=0;i<count;++i) before[oldPos++%capacity]=samples[i];
        decodium::normalizeWaterfallRingPosition(oldPos,capacity);
    }
    auto t1=std::chrono::steady_clock::now();
    for(int n=0;n<repeats;++n)
        decodium::appendWaterfallRing(after.data(),capacity,newPos,samples.data(),count);
    auto t2=std::chrono::steady_clock::now();
    if(before!=after || oldPos!=newPos) return 1;
    std::cout<<"batches="<<repeats<<" samples="<<count
             <<" before_ms="<<std::chrono::duration<double,std::milli>(t1-t0).count()
             <<" after_ms="<<std::chrono::duration<double,std::milli>(t2-t1).count()<<'\n';
}
