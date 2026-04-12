#include <QCoreApplication>

#include <algorithm>
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
  void extract_ (float s3[], int* nadd, int* mode65, int* ntrials, int* naggressive, int* ndepth,
                 int* nflip, char const* mycall_12, char const* hiscall_12, char const* hisgrid,
                 int* nQSOProgress, int* ljt65apon, int* ncount, int* nhist, char decoded[],
                 int* ltext, int* nft, float* qual, fortran_charlen_t_local,
                 fortran_charlen_t_local, fortran_charlen_t_local, fortran_charlen_t_local);
  extern int __jt65_mod_MOD_param[10];
  extern int __jt65_mod_MOD_mrs[63];
  extern int __jt65_mod_MOD_mrs2[63];
  extern float __jt65_mod_MOD_s3a[64 * 63];
  extern struct {
    int correct[63];
  } chansyms65_;
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

template <typename T, std::size_t N>
void compare_array_exact (std::array<T, N> const& lhs, std::array<T, N> const& rhs, char const* label)
{
  for (std::size_t i = 0; i < N; ++i)
    {
      if (lhs[i] != rhs[i])
        {
          std::fprintf (stderr, "%s mismatch at %zu: lhs=%d rhs=%d\n", label, i,
                        static_cast<int> (lhs[i]), static_cast<int> (rhs[i]));
          fail (label);
        }
    }
}

template <std::size_t N>
void compare_array_float (std::array<float, N> const& lhs, std::array<float, N> const& rhs,
                          char const* label, double tolerance)
{
  for (std::size_t i = 0; i < N; ++i)
    {
      if (std::fabs (static_cast<double> (lhs[i] - rhs[i])) > tolerance)
        {
          std::fprintf (stderr, "%s mismatch at %zu: lhs=%g rhs=%g\n", label, i,
                        static_cast<double> (lhs[i]), static_cast<double> (rhs[i]));
          fail (label);
        }
    }
}

void run_case (std::mt19937& rng, int trial)
{
  std::uniform_real_distribution<float> dist (0.05f, 10.0f);
  std::array<float, 64 * 63> s3_wrapped {};
  for (float& value : s3_wrapped)
    {
      value = dist (rng);
    }
  std::array<float, 64 * 63> s3_direct = s3_wrapped;

  int nadd = 1 + (trial % 8);
  int mode65 = 1;
  int ntrials = 80 + trial;
  int naggressive = (trial % 3 == 0) ? 10 : 0;
  int ndepth = (trial % 4 == 0) ? 32 : 1;
  int nflip = (trial % 2 == 0) ? 1 : -1;
  int nQSOProgress = trial % 6;
  int ljt65apon = (trial % 2) == 0 ? 1 : 0;
  int ncount_wrapped = -99;
  int nhist_wrapped = -99;
  QByteArray decoded_wrapped (22, ' ');
  int ltext_wrapped = 0;
  int nft_wrapped = -99;
  float qual_wrapped = 0.0f;

  QByteArray const mycall = fixed_field (QByteArray ("K1ABC       ", 12), 12);
  QByteArray const hiscall = fixed_field (QByteArray ("W9XYZ       ", 12), 12);
  QByteArray const hisgrid = fixed_field (QByteArray ("EN34  ", 6), 6);

  extract_ (s3_wrapped.data (), &nadd, &mode65, &ntrials, &naggressive, &ndepth, &nflip,
            mycall.constData (), hiscall.constData (), hisgrid.constData (), &nQSOProgress,
            &ljt65apon, &ncount_wrapped, &nhist_wrapped, decoded_wrapped.data (), &ltext_wrapped,
            &nft_wrapped, &qual_wrapped, static_cast<fortran_charlen_t_local> (mycall.size ()),
            static_cast<fortran_charlen_t_local> (hiscall.size ()),
            static_cast<fortran_charlen_t_local> (hisgrid.size ()),
            static_cast<fortran_charlen_t_local> (decoded_wrapped.size ()));

  std::array<int, 10> param_wrapped {};
  std::copy_n (__jt65_mod_MOD_param, 10, param_wrapped.begin ());
  std::array<int, 63> mrs_wrapped {};
  std::copy_n (__jt65_mod_MOD_mrs, 63, mrs_wrapped.begin ());
  std::array<int, 63> mrs2_wrapped {};
  std::copy_n (__jt65_mod_MOD_mrs2, 63, mrs2_wrapped.begin ());
  std::array<float, 64 * 63> s3a_wrapped {};
  std::copy_n (__jt65_mod_MOD_s3a, 64 * 63, s3a_wrapped.begin ());
  std::array<int, 63> correct_wrapped {};
  std::copy_n (chansyms65_.correct, 63, correct_wrapped.begin ());

  auto const direct = decodium::legacy::extract_compute (s3_direct.data (), nadd, mode65, ntrials,
                                                         naggressive, ndepth, nflip, mycall,
                                                         hiscall, hisgrid, nQSOProgress,
                                                         ljt65apon != 0);

  if (direct.ncount != ncount_wrapped || direct.nhist != nhist_wrapped
      || static_cast<int> (direct.ltext) != ltext_wrapped || direct.nft != nft_wrapped
      || std::fabs (static_cast<double> (direct.qual - qual_wrapped)) > 1.0e-6
      || fixed_field (direct.decoded, 22) != decoded_wrapped)
    {
      std::fprintf (stderr,
                    "scalar mismatch trial=%d wrapped(ncount=%d nhist=%d ltext=%d nft=%d qual=%g decoded='%.*s') "
                    "direct(ncount=%d nhist=%d ltext=%d nft=%d qual=%g decoded='%.*s')\n",
                    trial, ncount_wrapped, nhist_wrapped, ltext_wrapped, nft_wrapped,
                    static_cast<double> (qual_wrapped), static_cast<int> (decoded_wrapped.size ()),
                    decoded_wrapped.constData (), direct.ncount, direct.nhist,
                    static_cast<int> (direct.ltext), direct.nft,
                    static_cast<double> (direct.qual), static_cast<int> (direct.decoded.size ()),
                    direct.decoded.constData ());
      fail ("extract scalar");
    }

  compare_array_exact (direct.mrs, mrs_wrapped, "mrs");
  compare_array_exact (direct.mrs2, mrs2_wrapped, "mrs2");
  compare_array_exact (direct.correct, correct_wrapped, "correct");
  compare_array_float (direct.s3a, s3a_wrapped, "s3a", 1.0e-6);
  compare_array_exact (direct.param, param_wrapped, "param");
}

}  // namespace

int main (int argc, char** argv)
{
  try
    {
      QCoreApplication app {argc, argv};
      std::mt19937 rng {0x65E123u};
      for (int trial = 0; trial < 12; ++trial)
        {
          run_case (rng, trial);
        }
      std::printf ("JT65 extract compare passed\n");
      return 0;
    }
  catch (std::exception const& e)
    {
      std::fprintf (stderr, "jt65_extract_compare failed: %s\n", e.what ());
      return 1;
    }
}
