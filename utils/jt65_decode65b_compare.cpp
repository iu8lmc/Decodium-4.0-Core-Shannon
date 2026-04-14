#include <QByteArray>
#include <QCoreApplication>

#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>

#include "Detector/LegacyDspIoHelpers.hpp"

#if __GNUC__ > 7 || defined(__clang__)
using fortran_charlen_t_local = size_t;
#else
using fortran_charlen_t_local = int;
#endif

extern "C"
{
  void jt65_init_tables_ ();
  void decode65b_ (float s2[], int* nflip, int* nadd, int* mode65, int* ntrials,
                   int* naggressive, int* ndepth, char const* mycall, char const* hiscall,
                   char const* hisgrid, int* nQSOProgress, int* ljt65apon, int* nqd, int* nft,
                   float* qual, int* nhist, char decoded[], fortran_charlen_t_local,
                   fortran_charlen_t_local, fortran_charlen_t_local, fortran_charlen_t_local);
}

namespace
{

[[noreturn]] void fail (char const* message)
{
  throw std::runtime_error {message};
}

QByteArray fixed_field (QByteArray value, int width)
{
  value = value.left (width);
  if (value.size () < width)
    {
      value.append (QByteArray (width - value.size (), ' '));
    }
  return value;
}

void run_case (std::mt19937& rng, int trial)
{
  std::uniform_real_distribution<float> dist (-5.0f, 25.0f);
  std::array<float, 66 * 126> s2_wrapped {};
  for (float& value : s2_wrapped)
    {
      value = dist (rng);
    }
  std::array<float, 66 * 126> s2_direct = s2_wrapped;

  int nflip = (trial % 2 == 0) ? 1 : -1;
  int nadd = 1 + (trial % 6);
  int mode65 = 1;
  int ntrials = 80 + trial;
  int naggressive = (trial % 3 == 0) ? 10 : 0;
  int ndepth = (trial % 4 == 0) ? 32 : 1;
  int nQSOProgress = trial % 6;
  int ljt65apon = (trial % 2 == 0) ? 1 : 0;
  int nqd = 0;
  int nft_wrapped = -1;
  float qual_wrapped = 0.0f;
  int nhist_wrapped = -1;
  QByteArray decoded_wrapped (22, ' ');
  QByteArray const mycall = fixed_field ("K1ABC       ", 12);
  QByteArray const hiscall = fixed_field ("W9XYZ       ", 12);
  QByteArray const hisgrid = fixed_field ("EN34  ", 6);

  decode65b_ (s2_wrapped.data (), &nflip, &nadd, &mode65, &ntrials, &naggressive, &ndepth,
              mycall.constData (), hiscall.constData (), hisgrid.constData (), &nQSOProgress,
              &ljt65apon, &nqd, &nft_wrapped, &qual_wrapped, &nhist_wrapped,
              decoded_wrapped.data (), static_cast<fortran_charlen_t_local> (mycall.size ()),
              static_cast<fortran_charlen_t_local> (hiscall.size ()),
              static_cast<fortran_charlen_t_local> (hisgrid.size ()),
              static_cast<fortran_charlen_t_local> (decoded_wrapped.size ()));

  auto const direct = decodium::legacy::decode65b_compute (
      s2_direct.data (), nflip, nadd, mode65, ntrials, naggressive, ndepth, mycall, hiscall,
      hisgrid, nQSOProgress, ljt65apon != 0);

  if (nft_wrapped != direct.nft || nhist_wrapped != direct.nhist
      || std::fabs (static_cast<double> (qual_wrapped - direct.qual)) > 1.0e-6
      || decoded_wrapped != fixed_field (direct.decoded, 22))
    {
      std::fprintf (stderr,
                    "decode65b mismatch trial=%d wrapped(nft=%d qual=%g nhist=%d decoded='%.*s') "
                    "direct(nft=%d qual=%g nhist=%d decoded='%.*s')\n",
                    trial, nft_wrapped, static_cast<double> (qual_wrapped), nhist_wrapped,
                    decoded_wrapped.size (), decoded_wrapped.constData (), direct.nft,
                    static_cast<double> (direct.qual), direct.nhist,
                    direct.decoded.size (), direct.decoded.constData ());
      fail ("decode65b");
    }
}

}  // namespace

int main (int argc, char** argv)
{
  try
    {
      QCoreApplication app {argc, argv};
      jt65_init_tables_ ();
      std::mt19937 rng {0x65BEEF};
      for (int trial = 0; trial < 16; ++trial)
        {
          run_case (rng, trial);
        }
      std::printf ("JT65 decode65b compare passed\n");
      return 0;
    }
  catch (std::exception const& e)
    {
      std::fprintf (stderr, "jt65_decode65b_compare failed: %s\n", e.what ());
      return 1;
    }
}
