#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QTemporaryDir>
#include <QTextStream>

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include "Detector/LegacyDspIoHelpers.hpp"

#if __GNUC__ > 7 || defined(__clang__)
using fortran_charlen_t_local = size_t;
#else
using fortran_charlen_t_local = int;
#endif

extern "C"
{
  void wav12_ (short d2[], short d1[], int* npts, short* nbitsam2);
  void freqcal_ (short d2[], int* k, int* nkhz, int* noffset, int* ntol, char line[], int);
  void calibrate_ (char const* data_dir, int* iz, double* a, double* b, double* rms,
                   double* sigmaa, double* sigmab, int* irc, fortran_charlen_t_local);
}

namespace
{

bool compare_wav12_case (std::vector<unsigned char> const& raw8,
                         std::vector<short> const& raw16, short bits)
{
  constexpr int kNz12 = 60 * 12000;
  std::vector<short> fortran_data (static_cast<size_t> (kNz12), 0);
  std::vector<short> cpp_data (static_cast<size_t> (kNz12), 0);
  int npts = 0;

  if (bits == 8)
    {
      npts = static_cast<int> (raw8.size ());
      std::copy (raw8.begin (), raw8.end (),
                 reinterpret_cast<unsigned char*> (fortran_data.data ()));
      std::copy (raw8.begin (), raw8.end (),
                 reinterpret_cast<unsigned char*> (cpp_data.data ()));
    }
  else
    {
      npts = static_cast<int> (raw16.size ());
      std::copy (raw16.begin (), raw16.end (), fortran_data.begin ());
      std::copy (raw16.begin (), raw16.end (), cpp_data.begin ());
    }

  int npts_fortran = npts;
  int npts_cpp = npts;
  short sample_bits = bits;
  wav12_ (fortran_data.data (), fortran_data.data (), &npts_fortran, &sample_bits);
  decodium::legacy::wav12_inplace (cpp_data.data (), &npts_cpp, sample_bits);

  if (npts_fortran != npts_cpp)
    {
      std::fprintf (stderr, "wav12 npts mismatch bits=%d: fortran=%d cxx=%d\n",
                    bits, npts_fortran, npts_cpp);
      return false;
    }

  for (int i = 0; i < npts_fortran; ++i)
    {
      int const diff = std::abs (fortran_data[static_cast<size_t> (i)]
                               - cpp_data[static_cast<size_t> (i)]);
      if (diff > 1)
        {
          std::fprintf (stderr, "wav12 mismatch bits=%d index=%d: fortran=%d cxx=%d diff=%d\n",
                        bits, i, fortran_data[static_cast<size_t> (i)],
                        cpp_data[static_cast<size_t> (i)], diff);
          return false;
        }
    }

  return true;
}

QString numeric_suffix (QString const& line)
{
  return line.mid (8).trimmed ();
}

bool compare_freqcal ()
{
  constexpr int kNz = 30 * 12000;
  constexpr int kNfft = 55296;
  std::vector<short> samples (static_cast<size_t> (kNz), 0);
  double const frequency = 1523.75;
  double const fs = 12000.0;
  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i < kNfft; ++i)
    {
      samples[static_cast<size_t> (i)] =
          static_cast<short> (std::lround (12000.0 * std::sin (2.0 * pi * frequency * i / fs)));
    }

  int k = kNfft;
  int nkhz = 14084;
  int noffset = 1500;
  int ntol = 80;
  std::array<char, 81> line {};
  freqcal_ (samples.data (), &k, &nkhz, &noffset, &ntol, line.data (), 80);
  QString const fortran_line = QString::fromLatin1 (line.data ());
  QString const cpp_line = decodium::legacy::freqcal_line (samples.data (), k, nkhz, noffset, ntol);

  if (numeric_suffix (fortran_line) != numeric_suffix (cpp_line))
    {
      std::fprintf (stderr, "freqcal mismatch:\nfortran=%s\ncxx=%s\n",
                    qPrintable (fortran_line), qPrintable (cpp_line));
      return false;
    }

  return true;
}

bool write_sample_fmt_all (QString const& dir_path)
{
  QFile file {QDir {dir_path}.filePath ("fmt.all")};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Text))
    {
      return false;
    }

  QTextStream out {&file};
  out.setLocale (QLocale::c ());
  out << "12:00:00 14074  1  1500  1498.7  -1.3  35.0  28.0\n";
  out << "12:00:10 14074  1  1500  1499.1  -0.9  35.2  27.0\n";
  out << "12:01:00 21074  1  1500  1502.5   2.5  33.0  24.0\n";
  out << "12:01:10 21074  1  1500  1502.1   2.1  33.4  25.0\n";
  out << "12:02:00 28074  1  1500  1505.8   5.8  30.0  21.0\n";
  out << "12:02:10 28074  1  1500  1506.0   6.0  29.8  20.0\n";
  return true;
}

bool compare_calibrate ()
{
  QTemporaryDir fortran_dir;
  QTemporaryDir cpp_dir;
  if (!fortran_dir.isValid () || !cpp_dir.isValid ())
    {
      return false;
    }
  if (!write_sample_fmt_all (fortran_dir.path ()) || !write_sample_fmt_all (cpp_dir.path ()))
    {
      return false;
    }

  QByteArray path = QDir::toNativeSeparators (fortran_dir.path ()).toLocal8Bit ();
  int iz_fortran = 0;
  double a_fortran = 0.0;
  double b_fortran = 0.0;
  double rms_fortran = 0.0;
  double sigmaa_fortran = 0.0;
  double sigmab_fortran = 0.0;
  int irc_fortran = 0;
  calibrate_ (path.constData (), &iz_fortran, &a_fortran, &b_fortran, &rms_fortran,
              &sigmaa_fortran, &sigmab_fortran, &irc_fortran,
              static_cast<fortran_charlen_t_local> (path.size ()));

  auto const cpp_solution = decodium::legacy::calibrate_freqcal_directory (cpp_dir.path ());

  auto close_enough = [] (double lhs, double rhs) {
    return std::fabs (lhs - rhs) <= 1.0e-9;
  };

  if (iz_fortran != cpp_solution.iz || irc_fortran != cpp_solution.irc
      || !close_enough (a_fortran, cpp_solution.a)
      || !close_enough (b_fortran, cpp_solution.b)
      || !close_enough (rms_fortran, cpp_solution.rms)
      || !close_enough (sigmaa_fortran, cpp_solution.sigmaa)
      || !close_enough (sigmab_fortran, cpp_solution.sigmab))
    {
      std::fprintf (stderr,
                    "calibrate mismatch:\nfortran iz=%d irc=%d a=%g b=%g rms=%g sa=%g sb=%g\n"
                    "cxx     iz=%d irc=%d a=%g b=%g rms=%g sa=%g sb=%g\n",
                    iz_fortran, irc_fortran, a_fortran, b_fortran, rms_fortran,
                    sigmaa_fortran, sigmab_fortran,
                    cpp_solution.iz, cpp_solution.irc, cpp_solution.a, cpp_solution.b,
                    cpp_solution.rms, cpp_solution.sigmaa, cpp_solution.sigmab);
      return false;
    }

  if (!QFileInfo::exists (QDir {cpp_dir.path ()}.filePath ("fcal2.out")))
    {
      std::fprintf (stderr, "calibrate output file missing\n");
      return false;
    }

  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};

  std::vector<unsigned char> raw8 (4096);
  for (size_t i = 0; i < raw8.size (); ++i)
    {
      raw8[i] = static_cast<unsigned char> ((37 * i + 53) & 0xFF);
    }
  std::vector<short> raw16 (4096);
  for (size_t i = 0; i < raw16.size (); ++i)
    {
      raw16[i] = static_cast<short> (std::lround (14000.0 * std::sin (0.013 * i)));
    }

  if (!compare_wav12_case (raw8, {}, 8)
      || !compare_wav12_case ({}, raw16, 16)
      || !compare_freqcal ()
      || !compare_calibrate ())
    {
      return 1;
    }

  std::printf ("Legacy DSP/IO compare passed for wav12, freqcal and calibrate\n");
  return 0;
}
