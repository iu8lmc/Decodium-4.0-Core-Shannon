#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <complex>
#include <vector>

#include "commons.h"
#include "Detector/LegacyDspIoHelpers.hpp"

#if __GNUC__ > 7 || defined(__clang__)
using fortran_charlen_t_local = size_t;
#else
using fortran_charlen_t_local = int;
#endif

extern "C"
{
  void refspectrum_ (short d2[], bool* bclear, bool* brefspec, bool* buseref,
                     char const* fname, fortran_charlen_t_local len);
  void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform, int);
  void symspec_ (dec_data_t* shared_data, int* k, double* trperiod, int* nsps, int* ingain,
                 bool* bLowSidelobes, int* minw, float* px, float s[], float* df3,
                 int* ihsym, int* npts8, float* pxmax, int* npct);
  void hspec_ (short d2[], int* k, int* nutc0, int* ntrperiod, int* nrxfreq, int* ntol,
               bool* bmsk144, bool* btrain, double const pcoeffs[], int* ingain,
               char const* mycall, char const* hiscall, bool* bshmsg, bool* bswl,
               char const* datadir, float green[], float s[], int* jh, float* pxmax,
               float* dbNoGain, char line[],
               fortran_charlen_t_local, fortran_charlen_t_local,
               fortran_charlen_t_local, fortran_charlen_t_local);
}

extern "C" void mskrtd_ (short[], int*, float*, int*, int*, int*, char*, char*, bool*, bool*,
                         double const[], bool*, char*, char* line,
                         fortran_charlen_t_local, fortran_charlen_t_local,
                         fortran_charlen_t_local, fortran_charlen_t_local)
{
  if (line)
    {
      line[0] = '\0';
    }
}

namespace
{
constexpr int kRefNfft = 6912;
constexpr int kRefNh = kRefNfft / 2;
constexpr int kHspecColumns = 703;
constexpr int kHspecRows = 64;
constexpr int kSampleCount = NTMAX * RX_SAMPLE_RATE;

bool close_enough (float lhs, float rhs, float abs_tol = 1.0e-3f, float rel_tol = 1.0e-4f)
{
  float const scale = std::max (1.0f, std::max (std::fabs (lhs), std::fabs (rhs)));
  return std::fabs (lhs - rhs) <= abs_tol + rel_tol * scale;
}

void four2a_forward_real_buffer (float* buffer, int nfft)
{
  int ndim = 1;
  int isign = -1;
  int iform = 0;
  four2a_ (reinterpret_cast<std::complex<float>*> (buffer), &nfft, &ndim, &isign, &iform, 0);
}

void four2a_inverse_real_buffer (float* buffer, int nfft)
{
  int ndim = 1;
  int isign = 1;
  int iform = -1;
  four2a_ (reinterpret_cast<std::complex<float>*> (buffer), &nfft, &ndim, &isign, &iform, 0);
}

std::vector<float> load_reference_column (QString const& path, int start)
{
  std::vector<float> values (static_cast<std::size_t> (kRefNh + 1), 0.0f);
  QFile file {path};
  if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
    {
      return {};
    }

  QTextStream in {&file};
  in.setLocale (QLocale::c ());
  bool header_consumed = false;
  int row = 1;
  while (!in.atEnd () && row <= kRefNh)
    {
      QString const raw_line = in.readLine ();
      QString const trimmed = raw_line.trimmed ();
      if (trimmed.isEmpty ())
        {
          continue;
        }
      if (!header_consumed)
        {
          header_consumed = true;
          if (trimmed.size () > 58)
            {
              continue;
            }
        }
      if (raw_line.size () < 58)
        {
          return {};
        }
      bool ok = false;
      double const field_value = raw_line.mid (start, 12).trimmed ().toDouble (&ok);
      if (!ok)
        {
          return {};
        }
      values[static_cast<std::size_t> (row)] = static_cast<float> (field_value);
      ++row;
    }

  return values;
}

std::vector<float> load_linear_filter (QString const& path)
{
  auto values = load_reference_column (path, 34);
  if (values.empty ())
    {
      return values;
    }
  values[0] = 1.0f;
  return values;
}

std::vector<std::complex<float>> build_causal_filter (std::vector<float> const& fil, int shift,
                                                      int zero_from = 800, int zero_to = kRefNh)
{
  std::vector<float> buffer (static_cast<std::size_t> (kRefNfft + 2), 0.0f);
  auto* cx = reinterpret_cast<std::complex<float>*> (buffer.data ());
  cx[0] = std::complex<float> {};
  for (int i = 1; i <= kRefNh; ++i)
    {
      cx[static_cast<std::size_t> (i)] = std::complex<float> {
          fil[static_cast<std::size_t> (i)] / kRefNfft, 0.0f};
  }
  four2a_inverse_real_buffer (buffer.data (), kRefNfft);
  std::rotate (buffer.begin (), buffer.begin () + (kRefNfft - shift), buffer.begin () + kRefNfft);
  std::fill (buffer.begin () + zero_from, buffer.begin () + zero_to + 1, 0.0f);
  four2a_forward_real_buffer (buffer.data (), kRefNfft);
  return {cx, cx + kRefNh + 1};
}

std::vector<short> apply_causal_filter (std::vector<short> const& input,
                                        std::vector<std::complex<float>> const& cfil)
{
  std::vector<float> xs (static_cast<std::size_t> (kRefNh), 0.0f);
  std::vector<float> buffer (static_cast<std::size_t> (kRefNfft + 2), 0.0f);
  auto* cx = reinterpret_cast<std::complex<float>*> (buffer.data ());
  for (int i = 0; i < kRefNh; ++i)
    {
      buffer[static_cast<std::size_t> (i)] = static_cast<float> (input[static_cast<std::size_t> (i)]) / kRefNfft;
    }
  four2a_forward_real_buffer (buffer.data (), kRefNfft);
  for (int i = 0; i <= kRefNh; ++i)
    {
      cx[static_cast<std::size_t> (i)] *= cfil[static_cast<std::size_t> (i)];
    }
  four2a_inverse_real_buffer (buffer.data (), kRefNfft);
  for (int i = 0; i < kRefNh; ++i)
    {
      buffer[static_cast<std::size_t> (i)] += xs[static_cast<std::size_t> (i)];
    }

  std::vector<short> output (static_cast<std::size_t> (kRefNh));
  for (int i = 0; i < kRefNh; ++i)
    {
      output[static_cast<std::size_t> (i)] = static_cast<short> (std::lround (buffer[static_cast<std::size_t> (i)]));
    }
  return output;
}

void clear_spectra_common ()
{
  std::fill_n (spectra_.syellow, NSMAX, 0.0f);
  std::fill_n (spectra_.ref, 3457, 0.0f);
  std::fill_n (spectra_.filter, 3457, 0.0f);
}

bool compare_refspectrum ()
{
  QTemporaryDir fortran_dir;
  QTemporaryDir cpp_dir;
  if (!fortran_dir.isValid () || !cpp_dir.isValid ())
    {
      return false;
    }
  bool const keep_debug = !qEnvironmentVariableIsEmpty ("KEEP_REFSPEC_DEBUG");
  if (keep_debug)
    {
      fortran_dir.setAutoRemove (false);
      cpp_dir.setAutoRemove (false);
      std::fprintf (stderr, "refspectrum debug files: fortran=%s cpp=%s\n",
                    fortran_dir.path ().toLocal8Bit ().constData (),
                    cpp_dir.path ().toLocal8Bit ().constData ());
    }

  QString const fortran_path = QDir {fortran_dir.path ()}.filePath ("refspec.dat");
  QString const cpp_path = QDir {cpp_dir.path ()}.filePath ("refspec.dat");
  std::vector<short> input (static_cast<std::size_t> (kRefNh), 0);
  std::vector<short> input_filtered_fortran;
  std::vector<short> input_filtered_cpp;
  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i < kRefNh; ++i)
    {
      double const sample = 9000.0 * std::sin (2.0 * pi * 1210.0 * i / 12000.0)
                          + 4500.0 * std::sin (2.0 * pi * 2030.0 * i / 12000.0);
      input[static_cast<std::size_t> (i)] = static_cast<short> (std::lround (sample));
    }
  input_filtered_fortran = input;
  input_filtered_cpp = input;

  clear_spectra_common ();
  bool clear = true;
  bool accumulate = true;
  bool use = false;
  QByteArray const fortran_native = QDir::toNativeSeparators (fortran_path).toLocal8Bit ();
  for (int i = 0; i < 4; ++i)
    {
      refspectrum_ (input.data (), &clear, &accumulate, &use, fortran_native.constData (),
                    static_cast<fortran_charlen_t_local> (fortran_native.size ()));
      clear = false;
    }
  std::array<float, 3457> fortran_ref {};
  std::array<float, 3457> fortran_filter {};
  std::copy_n (spectra_.ref, 3457, fortran_ref.begin ());
  std::copy_n (spectra_.filter, 3457, fortran_filter.begin ());

  use = true;
  accumulate = false;
  refspectrum_ (input_filtered_fortran.data (), &clear, &accumulate, &use,
                fortran_native.constData (),
                static_cast<fortran_charlen_t_local> (fortran_native.size ()));

  auto input_filtered_cpp_from_fortran_file = input;
  decodium::legacy::reset_refspectrum_state ();
  clear_spectra_common ();
  clear = false;
  accumulate = false;
  use = true;
  decodium::legacy::refspectrum_update (input_filtered_cpp_from_fortran_file.data (), clear, accumulate,
                                        use, fortran_path);

  decodium::legacy::reset_refspectrum_state ();
  clear_spectra_common ();
  clear = true;
  accumulate = true;
  use = false;
  for (int i = 0; i < 4; ++i)
    {
      decodium::legacy::refspectrum_update (input.data (), clear, accumulate, use, cpp_path);
      clear = false;
    }
  std::array<float, 3457> cpp_ref {};
  std::array<float, 3457> cpp_filter {};
  std::copy_n (spectra_.ref, 3457, cpp_ref.begin ());
  std::copy_n (spectra_.filter, 3457, cpp_filter.begin ());

  auto input_filtered_cpp_from_memory = input;
  use = true;
  accumulate = false;
  decodium::legacy::refspectrum_update (input_filtered_cpp_from_memory.data (), clear, accumulate,
                                        use, cpp_dir.filePath ("missing_refspec.dat"));

  decodium::legacy::reset_refspectrum_state ();
  clear_spectra_common ();
  clear = true;
  accumulate = true;
  use = false;
  for (int i = 0; i < 4; ++i)
    {
      decodium::legacy::refspectrum_update (input.data (), clear, accumulate, use, cpp_path);
      clear = false;
    }
  use = true;
  accumulate = false;
  decodium::legacy::refspectrum_update (input_filtered_cpp.data (), clear, accumulate, use, cpp_path);

  for (int i = 1; i <= kRefNh; ++i)
    {
      if (!close_enough (fortran_ref[static_cast<std::size_t> (i)],
                         cpp_ref[static_cast<std::size_t> (i)], 2.0e-2f, 1.0e-3f)
          || !close_enough (fortran_filter[static_cast<std::size_t> (i)],
                            cpp_filter[static_cast<std::size_t> (i)], 2.0e-2f, 1.0e-3f))
        {
          std::fprintf (stderr, "refspectrum mismatch at bin %d: ref %g/%g filter %g/%g\n",
                        i,
                        fortran_ref[static_cast<std::size_t> (i)],
                        cpp_ref[static_cast<std::size_t> (i)],
                        fortran_filter[static_cast<std::size_t> (i)],
                        cpp_filter[static_cast<std::size_t> (i)]);
          return false;
        }
    }

  for (int i = 0; i < kRefNh; ++i)
    {
      int const diff = std::abs (input_filtered_fortran[static_cast<std::size_t> (i)]
                               - input_filtered_cpp[static_cast<std::size_t> (i)]);
      if (diff > 1)
        {
          int best_lag = 0;
          double best_score = -1.0;
          for (int lag = -8; lag <= 8; ++lag)
            {
              double score = 0.0;
              for (int j = 0; j < 256; ++j)
                {
                  int const lhs = j;
                  int const rhs = j + lag;
                  if (rhs < 0 || rhs >= kRefNh)
                    {
                      continue;
                    }
                  score += static_cast<double> (input_filtered_fortran[static_cast<std::size_t> (lhs)])
                         * static_cast<double> (input_filtered_cpp[static_cast<std::size_t> (rhs)]);
                }
              if (score > best_score)
                {
                  best_score = score;
                  best_lag = lag;
                }
            }
          std::fprintf (stderr, "refspectrum best lag estimate: %d\n", best_lag);
          auto const fil = load_linear_filter (cpp_path);
          auto const fortran_s = load_reference_column (fortran_path, 10);
          auto const cpp_s = load_reference_column (cpp_path, 10);
          if (!fil.empty ())
            {
              int best_shift = 400;
              int best_zero_from = 800;
              int best_zero_to = kRefNh;
              long long best_abs = std::numeric_limits<long long>::max ();
              for (int shift = 392; shift <= 408; ++shift)
                {
                  for (int zero_from = 798; zero_from <= 802; ++zero_from)
                    {
                      for (int zero_to = kRefNh - 2; zero_to <= kRefNh; ++zero_to)
                        {
                          auto const cfil = build_causal_filter (fil, shift, zero_from, zero_to);
                          auto const probe = apply_causal_filter (input, cfil);
                          long long abs_sum = 0;
                          for (int t = 0; t < 64; ++t)
                            {
                              abs_sum += std::llabs (
                                  static_cast<long long> (input_filtered_fortran[static_cast<std::size_t> (t)])
                                - static_cast<long long> (probe[static_cast<std::size_t> (t)]));
                            }
                          if (abs_sum < best_abs)
                            {
                              best_abs = abs_sum;
                              best_shift = shift;
                              best_zero_from = zero_from;
                              best_zero_to = zero_to;
                            }
                        }
                    }
                }
              std::fprintf (stderr,
                            "refspectrum best causal probe: shift=%d zero=%d:%d (abs64=%lld)\n",
                            best_shift, best_zero_from, best_zero_to, best_abs);
            }
              auto const fortran_fil = load_linear_filter (fortran_path);
              if (!fortran_fil.empty () && !fil.empty ())
                {
                  int worst_s_bin = 0;
                  float worst_s_abs = 0.0f;
                  for (int b = 1; b <= kRefNh && !fortran_s.empty () && !cpp_s.empty (); ++b)
                    {
                      float const diff_abs = std::fabs (fortran_s[static_cast<std::size_t> (b)]
                                                      - cpp_s[static_cast<std::size_t> (b)]);
                      if (diff_abs > worst_s_abs)
                        {
                          worst_s_abs = diff_abs;
                          worst_s_bin = b;
                        }
                    }
                  int worst_bin = 0;
                  float worst_abs = 0.0f;
                  for (int b = 1; b <= kRefNh; ++b)
                    {
                  float const diff_abs = std::fabs (fortran_fil[static_cast<std::size_t> (b)]
                                                  - fil[static_cast<std::size_t> (b)]);
                  if (diff_abs > worst_abs)
                    {
                      worst_abs = diff_abs;
                      worst_bin = b;
                    }
                }
                  auto const cpp_from_fortran = apply_causal_filter (input, build_causal_filter (fortran_fil, 400));
                  auto const cpp_from_cpp = apply_causal_filter (input, build_causal_filter (fil, 400));
                  std::fprintf (stderr,
                            "refspectrum pure-cpp file probes sample0: from fortran file=%d from cpp file=%d actual C++ use with fortran file=%d from C++ memory=%d, worst s bin=%d f=%g c=%g diff=%g, worst fil bin=%d f=%g c=%g diff=%g\n",
                            cpp_from_fortran.front (), cpp_from_cpp.front (),
                            input_filtered_cpp_from_fortran_file.front (),
                            input_filtered_cpp_from_memory.front (),
                            worst_s_bin,
                            worst_s_bin > 0 && !fortran_s.empty () ? fortran_s[static_cast<std::size_t> (worst_s_bin)] : 0.0f,
                            worst_s_bin > 0 && !cpp_s.empty () ? cpp_s[static_cast<std::size_t> (worst_s_bin)] : 0.0f,
                            worst_s_abs,
                            worst_bin,
                            worst_bin > 0 ? fortran_fil[static_cast<std::size_t> (worst_bin)] : 0.0f,
                            worst_bin > 0 ? fil[static_cast<std::size_t> (worst_bin)] : 0.0f,
                            worst_abs);
            }
          for (int j = 0; j < 8; ++j)
            {
              std::fprintf (stderr, "  sample[%d] %d vs %d\n", j,
                            input_filtered_fortran[static_cast<std::size_t> (j)],
                            input_filtered_cpp[static_cast<std::size_t> (j)]);
            }
          std::fprintf (stderr, "refspectrum filtered sample mismatch at %d: %d vs %d\n",
                        i,
                        input_filtered_fortran[static_cast<std::size_t> (i)],
                        input_filtered_cpp[static_cast<std::size_t> (i)]);
          return false;
        }
    }

  if (!QFile::exists (fortran_path) || !QFile::exists (cpp_path))
    {
      std::fprintf (stderr, "refspectrum output file missing\n");
      return false;
    }

  return true;
}

bool compare_symspec ()
{
  clear_spectra_common ();

  std::unique_ptr<dec_data_t> fortran_data {new dec_data_t {}};
  std::unique_ptr<dec_data_t> cpp_data {new dec_data_t {}};
  fortran_data->params.ndiskdat = false;
  cpp_data->params.ndiskdat = false;

  double const pi = 4.0 * std::atan (1.0);
  for (int i = 0; i < kSampleCount; ++i)
    {
      double const value = 9000.0 * std::sin (2.0 * pi * 1200.0 * i / 12000.0)
                         + 4000.0 * std::sin (2.0 * pi * 1620.0 * i / 12000.0);
      short const sample = static_cast<short> (std::lround (value));
      fortran_data->d2[i] = sample;
      cpp_data->d2[i] = sample;
    }

  int nsps = 6912;
  int ingain = 6;
  bool low_sidelobes = true;
  int minw = 2;
  double trperiod = 15.0;
  int npct = 0;
  float px_fortran = 0.0f;
  float px_cpp = 0.0f;
  float pxmax_fortran = 0.0f;
  float pxmax_cpp = 0.0f;
  float df3_fortran = 0.0f;
  float df3_cpp = 0.0f;
  int ihsym_fortran = 0;
  int ihsym_cpp = 0;
  int npts8_fortran = 0;
  int npts8_cpp = 0;
  std::array<float, NSMAX> s_fortran {};
  std::array<float, NSMAX> s_cpp {};

  int k = 8192;
  for (int step = 0; step < 10; ++step, k += nsps / 2)
    {
      int k_fortran = k;
      symspec_ (fortran_data.get (), &k_fortran, &trperiod, &nsps, &ingain, &low_sidelobes,
                &minw, &px_fortran, s_fortran.data (), &df3_fortran, &ihsym_fortran,
                &npts8_fortran, &pxmax_fortran, &npct);
    }
  std::array<float, NSMAX> syellow_fortran {};
  std::copy_n (spectra_.syellow, NSMAX, syellow_fortran.begin ());

  clear_spectra_common ();
  k = 8192;
  for (int step = 0; step < 10; ++step, k += nsps / 2)
    {
      decodium::legacy::symspec_update (cpp_data.get (), k, nsps, ingain, low_sidelobes, minw,
                                        &px_cpp, s_cpp.data (), &df3_cpp, &ihsym_cpp,
                                        &npts8_cpp, &pxmax_cpp);
    }
  std::array<float, NSMAX> syellow_cpp {};
  std::copy_n (spectra_.syellow, NSMAX, syellow_cpp.begin ());

  if (!close_enough (px_fortran, px_cpp)
      || !close_enough (pxmax_fortran, pxmax_cpp)
      || !close_enough (df3_fortran, df3_cpp)
      || ihsym_fortran != ihsym_cpp
      || npts8_fortran != npts8_cpp)
    {
      std::fprintf (stderr, "symspec scalar mismatch\n");
      return false;
    }

  int const iz = std::min (NSMAX, static_cast<int> (std::lround (5000.0 / df3_fortran)));
  for (int i = 0; i < iz; ++i)
    {
      if (!close_enough (s_fortran[static_cast<std::size_t> (i)],
                         s_cpp[static_cast<std::size_t> (i)], 2.0e-2f, 2.0e-4f)
          || !close_enough (fortran_data->savg[i], cpp_data->savg[i], 2.0e-2f, 2.0e-4f)
          || !close_enough (syellow_fortran[static_cast<std::size_t> (i)],
                            syellow_cpp[static_cast<std::size_t> (i)], 2.0e-2f, 2.0e-4f))
        {
          std::fprintf (stderr, "symspec mismatch at bin %d\n", i);
          return false;
        }
    }

  for (int sym = 0; sym < 10; ++sym)
    {
      for (int i = 0; i < iz; ++i)
        {
          int const idx = i * 184 + sym;
          if (!close_enough (fortran_data->ss[idx], cpp_data->ss[idx], 2.0e-2f, 2.0e-4f))
            {
              std::fprintf (stderr, "symspec ss mismatch at sym=%d bin=%d\n", sym, i);
              return false;
            }
        }
    }

  return true;
}

bool compare_hspec ()
{
  std::vector<short> input (static_cast<std::size_t> (30 * 12000), 0);
  double const pi = 4.0 * std::atan (1.0);
  for (size_t i = 0; i < input.size (); ++i)
    {
      double const value = 7000.0 * std::sin (2.0 * pi * 950.0 * i / 12000.0)
                         + 2500.0 * std::sin (2.0 * pi * 1680.0 * i / 12000.0);
      input[i] = static_cast<short> (std::lround (value));
    }

  std::vector<short> input_fortran = input;
  std::vector<short> input_cpp = input;
  std::array<float, kHspecColumns> green_fortran {};
  std::array<float, kHspecColumns> green_cpp {};
  std::array<float, kHspecColumns * kHspecRows> spec_fortran {};
  std::array<float, kHspecColumns * kHspecRows> spec_cpp {};
  int k = 4096;
  int nutc0 = 123456;
  int ntrpdepth = 30000;
  int nrxfreq = 1500;
  int ntol = 80;
  bool bmsk144 = false;
  bool btrain = false;
  std::array<double, 5> pcoeffs {};
  int ingain = 3;
  bool bshmsg = false;
  bool bswl = false;
  QTemporaryDir tmp;
  if (!tmp.isValid ())
    {
      return false;
    }
  QByteArray const datadir = QDir::toNativeSeparators (tmp.path ()).toLocal8Bit ();
  char mycall[12] {};
  char hiscall[12] {};
  std::memcpy (mycall, "9H1TEST     ", 12);
  std::memcpy (hiscall, "CQDX        ", 12);
  float pxmax_fortran = 0.0f;
  float pxmax_cpp = 0.0f;
  float rms_fortran = 0.0f;
  float rms_cpp = 0.0f;
  int jh_fortran = 0;
  int jh_cpp = 0;
  std::array<char, 81> line_fortran {};

  hspec_ (input_fortran.data (), &k, &nutc0, &ntrpdepth, &nrxfreq, &ntol, &bmsk144,
          &btrain, pcoeffs.data (), &ingain, mycall, hiscall, &bshmsg, &bswl,
          datadir.constData (), green_fortran.data (), spec_fortran.data (), &jh_fortran,
          &pxmax_fortran, &rms_fortran, line_fortran.data (),
          static_cast<fortran_charlen_t_local> (12),
          static_cast<fortran_charlen_t_local> (12),
          static_cast<fortran_charlen_t_local> (datadir.size ()),
          static_cast<fortran_charlen_t_local> (80));

  decodium::legacy::hspec_update (input_cpp.data (), k, ntrpdepth, ingain, green_cpp.data (),
                                  spec_cpp.data (), &jh_cpp, &pxmax_cpp, &rms_cpp);

  if (jh_fortran != jh_cpp
      || !close_enough (pxmax_fortran, pxmax_cpp)
      || !close_enough (rms_fortran, rms_cpp))
    {
      std::fprintf (stderr, "hspec scalar mismatch\n");
      return false;
    }

  for (int i = 0; i <= jh_fortran; ++i)
    {
      if (!close_enough (green_fortran[static_cast<std::size_t> (i)],
                         green_cpp[static_cast<std::size_t> (i)], 2.0e-2f, 2.0e-4f))
        {
          std::fprintf (stderr, "hspec green mismatch at %d\n", i);
          return false;
        }
    }

  for (int i = 0; i < kHspecRows * std::max (0, jh_fortran); ++i)
    {
      if (!close_enough (spec_fortran[static_cast<std::size_t> (i)],
                         spec_cpp[static_cast<std::size_t> (i)], 2.0e-2f, 2.0e-4f))
        {
          std::fprintf (stderr, "hspec spectrum mismatch at %d\n", i);
          return false;
        }
    }

  return true;
}

}

int main (int argc, char** argv)
{
  QCoreApplication app {argc, argv};
  bool const refspectrum_ok = compare_refspectrum ();
  bool const symspec_ok = compare_symspec ();
  bool const hspec_ok = compare_hspec ();
  std::printf ("refspectrum=%s symspec=%s hspec=%s\n",
               refspectrum_ok ? "ok" : "fail",
               symspec_ok ? "ok" : "fail",
               hspec_ok ? "ok" : "fail");
  if (!(refspectrum_ok && symspec_ok && hspec_ok))
    {
      return 1;
    }

  std::printf ("Legacy spectrum compare passed for refspectrum, symspec and hspec\n");
  return 0;
}
