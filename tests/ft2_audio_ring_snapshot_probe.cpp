// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/bridge/AudioRingSnapshot.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <vector>

int main()
{
    constexpr int capacity=90000, count=45000;
    std::mt19937 rng(20261115);
    std::vector<short> ring(capacity), old(count), current(count);
    for(auto& sample:ring)sample=static_cast<short>(rng());
    auto compare=[&](std::uint64_t end) {
        auto start=(end-count)%capacity;
        for(int i=0;i<count;++i)old[i]=ring[(start+i)%capacity];
        decodium::copyAudioRingSnapshot(ring.data(),capacity,end,current.data(),count);
        if(old!=current)std::abort();
    };
    for(std::uint64_t end:std::array<std::uint64_t,10>{45000ULL,89999ULL,90000ULL,90001ULL,134999ULL,
                           135000ULL,135001ULL,180000ULL,9000000000ULL,
                           std::numeric_limits<std::uint64_t>::max()})compare(end);
    for(int i=0;i<1000;++i)compare(45000ULL+(static_cast<std::uint64_t>(rng())<<20)+rng());
    volatile unsigned checksum=0;
    for(bool blocks:{false,true,true,false}) {
        auto start=std::chrono::steady_clock::now();
        for(unsigned i=0;i<2000;++i) {
            std::uint64_t end=45000ULL+i*97;
            if(blocks)decodium::copyAudioRingSnapshot(ring.data(),capacity,end,current.data(),count);
            else {auto pos=(end-count)%capacity;for(int j=0;j<count;++j)current[j]=ring[(pos+j)%capacity];}
            checksum=checksum+static_cast<unsigned short>(current[i%count]);
        }
        auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        std::printf("%s_us=%lld checksum=%u\n",blocks?"blocks":"old",static_cast<long long>(us),checksum);
    }
    std::puts("1010 ring snapshots identical");
}
