// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

using Complex=std::complex<float>;
extern "C" void ftx_sync4d_c(Complex const*,int,int,Complex const*,int,float*);
extern "C" void ftx_sync2d_scan_c(Complex const*,int,int,int,int,Complex const*,int,float*,int*);
struct Result { float score; int position; };
Result previous(Complex const* data,int n,int lo,int hi,int step,Complex const* tweak,int enabled)
{
    Result r{-1.0e30f,-1};
    for(int pos=lo;pos<=hi;pos+=step) {
        float score=0;ftx_sync4d_c(data,n,pos,tweak,enabled,&score);
        if(score>r.score)r={score,pos};
    }
    return r;
}
Result scanned(Complex const* data,int n,int lo,int hi,int step,Complex const* tweak,int enabled)
{
    Result r{};ftx_sync2d_scan_c(data,n,lo,hi,step,tweak,enabled,&r.score,&r.position);return r;
}
int main()
{
    constexpr int n=4032;std::mt19937 rng(20261115);std::uniform_real_distribution<float> value(-3,3);
    std::vector<Complex> data(n);std::array<Complex,64> tweak{};
    for(int i=0;i<1000;++i) {
        if(i%100==0){for(auto& v:data)v={value(rng),value(rng)};for(auto& v:tweak)v={value(rng),value(rng)};}
        int lo=static_cast<int>(rng()%1500)-400;int hi=lo+static_cast<int>(rng()%200);
        int const step=(i%2==0)?1:4;int enabled=(i/2)%2;
        Result a=previous(data.data(),n,lo,hi,step,tweak.data(),enabled);
        Result b=scanned(data.data(),n,lo,hi,step,tweak.data(),enabled);
        if(std::memcmp(&a.score,&b.score,sizeof(float))!=0 || a.position!=b.position)std::abort();
    }
    std::fill(data.begin(),data.end(),Complex{});
    for(float special:{0.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),1e38f,1e-40f}) {
        data[500]={special,0};
        Result a=previous(data.data(),n,-344,1012,4,tweak.data(),1);
        Result b=scanned(data.data(),n,-344,1012,4,tweak.data(),1);
        if(std::memcmp(&a.score,&b.score,sizeof(float))!=0 || a.position!=b.position)std::abort();
        data[500]={0,0};
    }
    for(auto& v:data)v={value(rng),value(rng)};
    volatile float checksum=0;
    for(bool fast:{false,true,true,false}) {
        auto const t=std::chrono::steady_clock::now();
        for(int i=0;i<120;++i) {
            auto r=fast?scanned(data.data(),n,108,560,4,tweak.data(),1)
                       :previous(data.data(),n,108,560,4,tweak.data(),1);
            checksum+=r.score;
        }
        auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t).count();
        std::printf("%s_us=%lld checksum=%.4f\n",fast?"scan":"old",static_cast<long long>(us),checksum);
    }
    std::puts("1005 FT4 sync scans agree bit for bit");
}
