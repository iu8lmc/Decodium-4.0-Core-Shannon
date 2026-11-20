// Compare complete scalar LDPC outputs for deterministic LLRs and AP masks.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

extern "C" void ftx_decode174_91_c (float const*, int, int, int, signed char const*,
                                     signed char*, signed char*, int*, int*, float*);
int main ()
{
  std::mt19937 rng {0x71c0951u};
  for (int trial=0; trial<450; ++trial)
    {
      std::array<float,174> llr {};
      std::array<signed char,174> mask {}, cw {};
      std::array<signed char,91> message {};
      for (int i=0; i<174; ++i)
        {
          llr[i]=static_cast<float>(static_cast<int>(rng()%16001)-8000)/1000.0f;
          if (trial%11==0 && i%13==0) llr[i]=0.0f;
          if (trial%3==0 && rng()%10==0) mask[i]=1;
        }
      int ntype=0, nhard=0;float dmin=0;
      int const maxosd=trial<300 ? -1 : (trial<440 ? 0 : 3);
      int const order=trial<440 ? 1 : 4;
      ftx_decode174_91_c(llr.data(),91,maxosd,order,mask.data(),message.data(),cw.data(),&ntype,&nhard,&dmin);
      std::uint32_t bits;std::memcpy(&bits,&dmin,sizeof bits);
      std::printf("%d %d %d %08x ",trial,ntype,nhard,bits);
      for (auto value:message) std::printf("%02x",static_cast<unsigned char>(value));
      std::putchar(' ');
      for (auto value:cw) std::printf("%02x",static_cast<unsigned char>(value));
      std::putchar('\n');
    }
}
