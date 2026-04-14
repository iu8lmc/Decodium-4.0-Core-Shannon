#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Detector/LegacyDspIoHelpers.hpp"

#if __GNUC__ > 7 || defined(__clang__)
using fortran_charlen_t_local = size_t;
#else
using fortran_charlen_t_local = int;
#endif

extern "C"
{
  void wspr_downsample_ (short d2[], int* k);
  int savec2_ (char const* fname, int* tr_seconds, double* dial_freq, fortran_charlen_t_local len);
  void degrade_snr_ (short d2[], int* npts, float* db, float* bandwidth);
  extern struct {
    std::complex<float> c0[120 * 1500];
  } c0com_;
}

namespace
{
constexpr int kWsprNmax = 120 * 12000;
constexpr int kWsprNdmax = 120 * 1500;

bool close_complex (std::complex<float> lhs, std::complex<float> rhs,
                    float abs_tol = 1.0e-4f, float rel_tol = 1.0e-4f)
{
  auto close_part = [abs_tol, rel_tol] (float a, float b)
  {
    float const scale = std::max (1.0f, std::max (std::fabs (a), std::fabs (b)));
    return std::fabs (a - b) <= abs_tol + rel_tol * scale;
  };
  return close_part (lhs.real (), rhs.real ()) && close_part (lhs.imag (), rhs.imag ());
}

bool compare_wspr_downsample ()
{
  std::vector<short> input (kWsprNmax, 0);
  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i < 8192; ++i)
    {
      double const sample = 12000.0 * std::sin (2.0 * pi * 1500.0 * i / 12000.0)
                          + 2500.0 * std::sin (2.0 * pi * 1700.0 * i / 12000.0);
      input[static_cast<std::size_t> (i)] = static_cast<short> (std::lround (sample));
    }

  std::fill_n (c0com_.c0, kWsprNdmax, std::complex<float> {});
  for (int k = 2048; k <= 4096; k += 512)
    {
      int current = k;
      wspr_downsample_ (input.data (), &current);
    }
  std::array<std::complex<float>, 512> fortran {};
  std::copy_n (c0com_.c0, int (fortran.size ()), fortran.begin ());

  std::fill_n (c0com_.c0, kWsprNdmax, std::complex<float> {});
  for (int k = 2048; k <= 4096; k += 512)
    {
      decodium::legacy::wspr_downsample_update (input.data (), k);
    }
  for (int i = 0; i < int (fortran.size ()); ++i)
    {
      if (!close_complex (fortran[static_cast<std::size_t> (i)], c0com_.c0[i]))
        {
          std::fprintf (stderr, "wspr_downsample mismatch at %d: (%g,%g) vs (%g,%g)\n",
                        i,
                        fortran[static_cast<std::size_t> (i)].real (),
                        fortran[static_cast<std::size_t> (i)].imag (),
                        c0com_.c0[i].real (), c0com_.c0[i].imag ());
          return false;
        }
    }
  return true;
}

bool compare_savec2 ()
{
  float const pi = 4.0f * std::atan (1.0f);
  for (int i = 0; i < 114 * 1500; ++i)
    {
      float const phase = 2.0f * pi * (i % 375) / 375.0f;
      c0com_.c0[i] = std::complex<float> {std::cos (phase), std::sin (phase)};
    }
  for (int i = 114 * 1500; i < kWsprNdmax; ++i)
    {
      c0com_.c0[i] = std::complex<float> {};
    }

  QTemporaryDir dir;
  if (!dir.isValid ())
    {
      return false;
    }
  QString const fortran_path = dir.filePath ("aa240101_123456.c2");
  QString const cpp_path = dir.filePath ("bb240101_123456.c2");

  QByteArray fortran_name = fortran_path.toLocal8Bit ();
  int tr_seconds = 120;
  double dial = 14.0956;
  int const fortran_err =
      savec2_ (fortran_name.constData (), &tr_seconds, &dial,
               static_cast<fortran_charlen_t_local> (fortran_name.size ()));
  if (fortran_err != 0)
    {
      std::fprintf (stderr, "Fortran savec2 failed with %d\n", fortran_err);
      return false;
    }

  for (int i = 0; i < 114 * 1500; ++i)
    {
      float const phase = 2.0f * pi * (i % 375) / 375.0f;
      c0com_.c0[i] = std::complex<float> {std::cos (phase), std::sin (phase)};
    }
  int const cpp_err = decodium::legacy::savec2_file (cpp_path, 120, 14.0956);
  if (cpp_err != 0)
    {
      std::fprintf (stderr, "C++ savec2 failed with %d\n", cpp_err);
      return false;
    }

  QFile fortran_file {fortran_path};
  QFile cpp_file {cpp_path};
  if (!fortran_file.open (QIODevice::ReadOnly) || !cpp_file.open (QIODevice::ReadOnly))
    {
      std::fprintf (stderr, "savec2 output open failed\n");
      return false;
    }
  QByteArray const fortran_bytes = fortran_file.readAll ();
  QByteArray const cpp_bytes = cpp_file.readAll ();
  if (fortran_bytes.size () != cpp_bytes.size () || fortran_bytes.size () < 26)
    {
      std::fprintf (stderr, "savec2 file size mismatch: fortran=%lld cpp=%lld\n",
                    qlonglong (fortran_bytes.size ()), qlonglong (cpp_bytes.size ()));
      return false;
    }

  if (!std::equal (fortran_bytes.constBegin (), fortran_bytes.constBegin () + 26,
                   cpp_bytes.constBegin ()))
    {
      int mismatch = -1;
      for (int i = 0; i < 26; ++i)
        {
          if (fortran_bytes.at (i) != cpp_bytes.at (i))
            {
              mismatch = i;
              break;
            }
        }
      std::fprintf (stderr, "savec2 header mismatch at byte %d\n", mismatch);
      return false;
    }

  auto const* fortran_c2 =
      reinterpret_cast<std::complex<float> const*> (fortran_bytes.constData () + 26);
  auto const* cpp_c2 =
      reinterpret_cast<std::complex<float> const*> (cpp_bytes.constData () + 26);
  for (int i = 0; i < 45000; ++i)
    {
      if (!close_complex (fortran_c2[i], cpp_c2[i], 2.0e-6f, 1.0e-6f))
        {
          std::fprintf (stderr, "savec2 payload mismatch at %d: (%g,%g) vs (%g,%g)\n",
                        i,
                        fortran_c2[i].real (), fortran_c2[i].imag (),
                        cpp_c2[i].real (), cpp_c2[i].imag ());
          return false;
        }
    }
  return true;
}

bool compare_degrade_snr ()
{
  std::vector<short> source (4096, 0);
  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i < int (source.size ()); ++i)
    {
      double const sample = 10000.0 * std::sin (2.0 * pi * 920.0 * i / 12000.0)
                          + 2000.0 * std::sin (2.0 * pi * 1410.0 * i / 12000.0);
      source[static_cast<std::size_t> (i)] = static_cast<short> (std::lround (sample));
    }

  auto fortran = source;
  auto cpp = source;
  int npts = int (source.size ());
  float db = 3.5f;
  float bw = 2500.0f;

  std::srand (1);
  degrade_snr_ (fortran.data (), &npts, &db, &bw);
  std::srand (1);
  decodium::legacy::degrade_snr_inplace (cpp.data (), npts, db, bw);

  for (int i = 0; i < npts; ++i)
    {
      if (fortran[static_cast<std::size_t> (i)] != cpp[static_cast<std::size_t> (i)])
        {
          std::fprintf (stderr, "degrade_snr mismatch at %d: %d vs %d\n",
                        i, fortran[static_cast<std::size_t> (i)], cpp[static_cast<std::size_t> (i)]);
          return false;
        }
    }
  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  bool const wspr_ok = compare_wspr_downsample ();
  bool const savec2_ok = compare_savec2 ();
  bool const degrade_ok = compare_degrade_snr ();

  if (!wspr_ok || !savec2_ok || !degrade_ok)
    {
      std::fprintf (stderr, "wspr_downsample=%s savec2=%s degrade_snr=%s\n",
                    wspr_ok ? "ok" : "fail",
                    savec2_ok ? "ok" : "fail",
                    degrade_ok ? "ok" : "fail");
      return 1;
    }

  std::printf ("Legacy WSPR/IO compare passed for wspr_downsample, savec2 and degrade_snr\n");
  return 0;
}
