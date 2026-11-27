// Compare OSD results between two builds on the same machine.  The output
// includes every message/codeword byte and the exact distance bit pattern.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

extern "C" void ftx_osd174_91_c (float const*, int, signed char const*, int,
                                   signed char*, signed char*, int*, float*);

int main (int argc, char** argv)
{
  std::setvbuf (stdout, nullptr, _IOLBF, 0);
  bool const deep = argc > 1 && std::string {argv[1]} == "--deep";
  int const count = deep ? 12 : 600;
  std::mt19937 rng {19780317u};
  for (int trial = 0; trial < count; ++trial)
    {
      std::array<float, 174> llr {};
      std::array<signed char, 174> mask {}, codeword {};
      std::array<signed char, 91> message {};
      for (int i = 0; i < 174; ++i)
        {
          llr[i] = (static_cast<int> (rng () % 20001) - 10000) / 1000.0f;
          mask[i] = deep ? (rng () % 5 != 0 ? 1 : 0)
                         : (trial % 3 == 0 && rng () % 20 == 0 ? 1 : 0);
        }
      int const k = trial % 3 == 0 ? 77 : (trial % 3 == 1 ? 87 : 91);
      int const order = deep ? 5 + trial % 2 : trial % 5;
      int nhard = 0;
      float dmin = 0.0f;
      ftx_osd174_91_c (llr.data (), k, mask.data (), order, message.data (),
                       codeword.data (), &nhard, &dmin);
      std::uint32_t distanceBits;
      std::memcpy (&distanceBits, &dmin, sizeof distanceBits);
      std::printf ("%d %d %d %08x ", trial, k, nhard, distanceBits);
      for (auto value : message) std::printf ("%02x", static_cast<unsigned char> (value));
      std::putchar (' ');
      for (auto value : codeword) std::printf ("%02x", static_cast<unsigned char> (value));
      std::putchar ('\n');
    }
}
