#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>

#include "Detector/LegacyDspIoHelpers.hpp"

extern "C"
{
  void graycode65_ (int dat[], int* n, int* idir);
  void interleave63_ (int d1[], int* idir);
  void demod64a_ (float s3[], int* nadd, float* afac1, int mrsym[], int mrprob[],
                  int mr2sym[], int mr2prob[], int* ntest, int* nlow);
}

namespace
{

[[noreturn]] void fail (std::string const& message)
{
  throw std::runtime_error {message};
}

void fill_symbols (std::array<int, 63>& data, std::mt19937& rng)
{
  std::uniform_int_distribution<int> dist (0, 63);
  for (int& value : data)
    {
      value = dist (rng);
    }
}

void compare_graycode (std::mt19937& rng)
{
  for (int trial = 0; trial < 16; ++trial)
    {
      std::array<int, 63> wrapped {};
      std::array<int, 63> direct {};
      fill_symbols (wrapped, rng);
      direct = wrapped;

      int n = 63;
      int idir = (trial % 2 == 0) ? 1 : -1;
      graycode65_ (wrapped.data (), &n, &idir);
      decodium::legacy::graycode65_inplace (direct.data (), n, idir);
      if (wrapped != direct)
        {
          fail ("graycode65 compare failed");
        }
    }
}

void compare_interleave (std::mt19937& rng)
{
  for (int trial = 0; trial < 16; ++trial)
    {
      std::array<int, 63> wrapped {};
      std::array<int, 63> direct {};
      std::uniform_int_distribution<int> dist (0, 255);
      for (int& value : wrapped)
        {
          value = dist (rng);
        }
      direct = wrapped;

      int idir = (trial % 2 == 0) ? 1 : -1;
      interleave63_ (wrapped.data (), &idir);
      decodium::legacy::interleave63_inplace (direct.data (), idir);
      if (wrapped != direct)
        {
          fail ("interleave63 compare failed");
        }
    }
}

void compare_demod (std::mt19937& rng)
{
  std::uniform_real_distribution<float> dist (0.0f, 10.0f);
  for (int trial = 0; trial < 8; ++trial)
    {
      std::array<float, 64 * 63> s3 {};
      for (float& value : s3)
        {
          value = dist (rng);
        }

      int nadd = 1 + (trial % 5);
      float afac1 = 1.1f + 0.1f * trial;
      std::array<int, 63> mrsym_wrapped {};
      std::array<int, 63> mrprob_wrapped {};
      std::array<int, 63> mr2sym_wrapped {};
      std::array<int, 63> mr2prob_wrapped {};
      int ntest_wrapped = 0;
      int nlow_wrapped = 0;
      demod64a_ (s3.data (), &nadd, &afac1, mrsym_wrapped.data (), mrprob_wrapped.data (),
                 mr2sym_wrapped.data (), mr2prob_wrapped.data (), &ntest_wrapped, &nlow_wrapped);

      decodium::legacy::Jt65Demod64aResult const direct =
          decodium::legacy::demod64a_compute (s3.data (), nadd, afac1);

      if (direct.mrsym != mrsym_wrapped || direct.mrprob != mrprob_wrapped
          || direct.mr2sym != mr2sym_wrapped || direct.mr2prob != mr2prob_wrapped
          || direct.ntest != ntest_wrapped || direct.nlow != nlow_wrapped)
        {
          fail ("demod64a compare failed");
        }
    }
}

}  // namespace

int main (int argc, char** argv)
{
  try
    {
      QCoreApplication app {argc, argv};
      std::mt19937 rng {0x65A65A};
      compare_graycode (rng);
      compare_interleave (rng);
      compare_demod (rng);
      std::printf ("JT65 extract primitive compare passed\n");
      return 0;
    }
  catch (std::exception const& e)
    {
      std::fprintf (stderr, "jt65_extract_primitives_compare failed: %s\n", e.what ());
      return 1;
    }
}
