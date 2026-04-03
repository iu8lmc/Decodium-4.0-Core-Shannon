#include "LegacyDspIoHelpers.hpp"
#include "Modulator/LegacyJtEncoder.hpp"
#include "widgets/PlotLegacyHelpers.hpp"

#include <QByteArray>
#include <QFile>
#include <QLocale>
#include <QTextStream>
#include <QTime>

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <thread>
#include <vector>

#if __GNUC__ > 7 || defined(__clang__)
using fortran_charlen_t_local = size_t;
#else
using fortran_charlen_t_local = int;
#endif

extern "C"
{
  void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform, int);
  void filbig_ (float dd[], int* npts, float* f0, int* newdat, std::complex<float> c4a[],
                int* n4, float* sq0);
  void sh65_ (std::complex<float> cx[], int* n5, int* mode65, int* ntol, float* xdf,
              int* nspecial, float* snrdb);
  void fil6521_ (std::complex<float> c1[], int* n1, std::complex<float> c2[], int* n2);
  void afc65b_ (std::complex<float> cx[], int* npts, float* fsample, int* nflip, int* mode65,
                float a[], float* ccfbest, float* dtbest);
  void twkfreq65_ (std::complex<float> c4aa[], int* n5, float a[]);
  void smo121_ (float x[], int* nz);
  void pctile_ (float x[], int* npts, int* npct, float* xpct);
  void polyfit_ (double x[], double y[], double sigmay[], int* npts, int* nterms, int* mode,
                 double a[], double* chisqr);
  void ftrsdap_ (int mrsym[], int mrprob[], int mr2sym[], int mr2prob[], int ap[],
                 int* ntrials, int correct[], int param[], int ntry[]);
  void hint65_ (float s3[], int mrs[], int mrs2[], int* nadd, int* nflip, char const* mycall,
                char const* hiscall, char const* hisgrid, float* qual, char decoded[],
                fortran_charlen_t_local, fortran_charlen_t_local, fortran_charlen_t_local,
                fortran_charlen_t_local);
  float gran_ ();
  extern int __jt65_mod_MOD_param[10];
  extern int __jt65_mod_MOD_mrs[63];
  extern int __jt65_mod_MOD_mrs2[63];
  extern int __jt65_mod_MOD_mdat[126];
  extern int __jt65_mod_MOD_mref[126 * 2];
  extern int __jt65_mod_MOD_mdat2[126];
  extern int __jt65_mod_MOD_mref2[126 * 2];
  extern float __jt65_mod_MOD_pr[126];
  extern float __jt65_mod_MOD_s3a[64 * 63];
  extern float __jt65_mod_MOD_width;
  extern struct {
    int correct[63];
  } chansyms65_;
  extern struct {
    float fspread_self;
    float fspread_dx;
  } echocom2_;
  struct {
    float thresh0;
  } steve_;
  extern struct {
    std::complex<float> c0[120 * 1500];
  } c0com_;
  extern struct {
    float dfref;
    float ref[3413];
  } refspec_;
  struct {
    float ss[552 * 3413];
  } sync_;
}

namespace
{
constexpr int kRefNfft = 6912;
constexpr int kRefNh = kRefNfft / 2;
constexpr int kRefPolyLow = 400;
constexpr int kRefPolyHigh = 2600;
constexpr int kJt65Nfft = 8192;
constexpr int kJt65Nsz = 3413;
constexpr int kJt65MaxQsym = 552;
constexpr int kJt65MaxCand = 300;
constexpr int kJt65WindowCutoff = 4458;
constexpr int kJt65DecodeNmax = 60 * 12000;
constexpr int kJt65DecodeFft = 512;
constexpr int kJt65DecodeRows = 512;
constexpr int kJt65DecodeCols = 126;
constexpr int kSymspecNfft3 = 16384;
constexpr int kSymspecMaxHalfSymbols = 184;
constexpr int kSymspecMaxFreqBins = 6827;
constexpr int kHspecColumns = 703;
constexpr int kHspecNfft = 512;
constexpr int kEchoNsps = 4096;
constexpr int kEchoTxSamples = 6 * kEchoNsps;
constexpr int kEchoNfft = 32768;
constexpr int kEchoNz = 4096;
constexpr int kWsprNmax = 120 * 12000;
constexpr int kWsprNdmax = 120 * 1500;
constexpr int kWsprBlockSamples = 512;
constexpr int kWsprMixTail = 105;
constexpr int kWsprMixOutput = 64;
constexpr int kWsprSaveMaxFft = 256 * 1024;
std::array<float, static_cast<std::size_t> (kJt65DecodeRows * kJt65DecodeCols)> g_jt65_last_s1 {};

double db_value (double x)
{
  if (x <= 1.259e-10)
    {
      return -99.0;
    }
  return 10.0 * std::log10 (x);
}

QString format_fortran_e12_3 (double value)
{
  if (value == 0.0)
    {
      return QStringLiteral ("   0.000E+00");
    }

  int exponent = static_cast<int> (std::floor (std::log10 (std::fabs (value)))) + 1;
  double mantissa = value / std::pow (10.0, exponent);
  QString mantissa_text = QString::asprintf ("%.3f", mantissa);
  bool ok = false;
  double rounded = mantissa_text.toDouble (&ok);
  if (ok && std::fabs (rounded) >= 1.0)
    {
      rounded /= 10.0;
      ++exponent;
      mantissa_text = QString::asprintf ("%.3f", rounded);
    }

  QString const field =
      QStringLiteral ("%1E%2").arg (mantissa_text).arg (QString::asprintf ("%+03d", exponent));
  return field.rightJustified (12, QLatin1Char {' '});
}

float quantize_fortran_e12_3 (float value)
{
  bool ok = false;
  double const quantized = format_fortran_e12_3 (value).trimmed ().toDouble (&ok);
  return ok ? static_cast<float> (quantized) : value;
}

double peakup_offset (double ym, double y0, double yp)
{
  double const b = yp - ym;
  double const c = yp + ym - 2.0 * y0;
  if (std::fabs (c) < 1.0e-30)
    {
      return 0.0;
    }
  return -b / (2.0 * c);
}

void four2a_forward_real (std::vector<std::complex<float>>& buffer, int nfft)
{
  int ndim = 1;
  int isign = -1;
  int iform = 0;
  four2a_ (buffer.data (), &nfft, &ndim, &isign, &iform, 0);
}

void four2a_inverse_real (std::vector<std::complex<float>>& buffer, int nfft)
{
  int ndim = 1;
  int isign = 1;
  int iform = -1;
  four2a_ (buffer.data (), &nfft, &ndim, &isign, &iform, 0);
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

void four2a_forward_complex (std::complex<float>* buffer, int nfft)
{
  int ndim = 1;
  int isign = -1;
  int iform = 1;
  four2a_ (buffer, &nfft, &ndim, &isign, &iform, 0);
}

void four2a_inverse_complex (std::complex<float>* buffer, int nfft)
{
  int ndim = 1;
  int isign = 1;
  int iform = 1;
  four2a_ (buffer, &nfft, &ndim, &isign, &iform, 0);
}

void four2a_complex_sign (std::complex<float>* buffer, int nfft, int isign)
{
  int ndim = 1;
  int iform = 1;
  four2a_ (buffer, &nfft, &ndim, &isign, &iform, 0);
}

std::size_t jt65_ss_index (int qsym_1based, int freq_1based)
{
  return static_cast<std::size_t> (qsym_1based - 1)
         + static_cast<std::size_t> (kJt65MaxQsym) * static_cast<std::size_t> (freq_1based - 1);
}

std::size_t jt65_s1_index (int row_fortran, int column_fortran)
{
  return static_cast<std::size_t> (row_fortran + 255)
         + static_cast<std::size_t> (kJt65DecodeRows)
               * static_cast<std::size_t> (column_fortran - 1);
}

std::array<float, 126> const& jt65_pr_sequence ()
{
  static std::array<float, 126> pr = [] {
    std::array<int, 126> const nprc {
        1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1,
        0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0,
        1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};
    std::array<float, 126> values {};
    for (std::size_t i = 0; i < values.size (); ++i)
      {
        values[i] = 2.0f * nprc[i] - 1.0f;
      }
    return values;
  } ();
  return pr;
}

void initialize_jt65_tables ()
{
  static bool initialized = false;
  if (initialized)
    {
      return;
    }

  auto const& pr = jt65_pr_sequence ();
  for (int i = 0; i < 126; ++i)
    {
      __jt65_mod_MOD_pr[static_cast<std::size_t> (i)] = pr[static_cast<std::size_t> (i)];
    }

  int k = 0;
  int mr1 = 0;
  int mr2 = 0;
  for (int i = 1; i <= 126; ++i)
    {
      if (pr[static_cast<std::size_t> (i - 1)] < 0.0f)
        {
          __jt65_mod_MOD_mdat[static_cast<std::size_t> (k++)] = i;
        }
      else
        {
          mr2 = i;
          if (mr1 == 0)
            {
              mr1 = i;
            }
        }
    }

  auto const set_mref = [] (int slot, int which, int value) {
    __jt65_mod_MOD_mref[static_cast<std::size_t> ((slot - 1) + 126 * (which - 1))] = value;
  };
  for (int slot = 1; slot <= k; ++slot)
    {
      int const m = __jt65_mod_MOD_mdat[static_cast<std::size_t> (slot - 1)];
      set_mref (slot, 1, mr1);
      for (int n = 1; n <= 10; ++n)
        {
          if ((m - n) > 0 && pr[static_cast<std::size_t> (m - n - 1)] > 0.0f)
            {
              set_mref (slot, 1, m - n);
              break;
            }
        }
      set_mref (slot, 2, mr2);
      for (int n = 1; n <= 10; ++n)
        {
          if ((m + n) <= 126 && pr[static_cast<std::size_t> (m + n - 1)] > 0.0f)
            {
              set_mref (slot, 2, m + n);
              break;
            }
        }
    }

  k = 0;
  mr1 = 0;
  mr2 = 0;
  for (int i = 1; i <= 126; ++i)
    {
      if (pr[static_cast<std::size_t> (i - 1)] > 0.0f)
        {
          __jt65_mod_MOD_mdat2[static_cast<std::size_t> (k++)] = i;
        }
      else
        {
          mr2 = i;
          if (mr1 == 0)
            {
              mr1 = i;
            }
        }
    }

  auto const set_mref2 = [] (int slot, int which, int value) {
    __jt65_mod_MOD_mref2[static_cast<std::size_t> ((slot - 1) + 126 * (which - 1))] = value;
  };
  for (int slot = 1; slot <= k; ++slot)
    {
      int const m = __jt65_mod_MOD_mdat2[static_cast<std::size_t> (slot - 1)];
      set_mref2 (slot, 1, mr1);
      for (int n = 1; n <= 10; ++n)
        {
          if ((m - n) > 0 && pr[static_cast<std::size_t> (m - n - 1)] < 0.0f)
            {
              set_mref2 (slot, 1, m - n);
              break;
            }
        }
      set_mref2 (slot, 2, mr2);
      for (int n = 1; n <= 10; ++n)
        {
          if ((m + n) <= 126 && pr[static_cast<std::size_t> (m + n - 1)] < 0.0f)
            {
              set_mref2 (slot, 2, m + n);
              break;
            }
        }
    }

  initialized = true;
}

std::array<float, kJt65Nfft> const& jt65_window ()
{
  static std::array<float, kJt65Nfft> window = [] {
    std::array<float, kJt65Nfft> w {};
    for (int i = 0; i < kJt65Nfft; ++i)
      {
        w[static_cast<std::size_t> (i)] = (i + 1) <= kJt65WindowCutoff ? 1.0f : 0.0f;
      }
    return w;
  } ();
  return window;
}

void flat65_compute (std::vector<float> const& ss, int nqsym, std::vector<float>& ref)
{
  ref.assign (kJt65Nsz, 1.0f);
  if (nqsym <= 0 || ss.empty ())
    {
      return;
    }

  int constexpr npct = 28;
  int constexpr nsmo = 33;
  std::vector<float> stmp (kJt65Nsz, 0.0f);
  for (int i = 0; i < kJt65Nsz; ++i)
    {
      float pct = 0.0f;
      int npts = nqsym;
      int pct_rank = npct;
      pctile_ (const_cast<float*> (ss.data () + static_cast<std::size_t> (i) * kJt65MaxQsym),
               &npts, &pct_rank, &pct);
      stmp[static_cast<std::size_t> (i)] = pct;
    }

  int const ia = nsmo / 2 + 1;
  int const ib = kJt65Nsz - nsmo / 2 - 1;
  for (int i = ia; i <= ib; ++i)
    {
      float pct = 0.0f;
      int npts = nsmo;
      int pct_rank = npct;
      pctile_ (stmp.data () + (i - nsmo / 2 - 1), &npts, &pct_rank, &pct);
      ref[static_cast<std::size_t> (i - 1)] = pct;
    }

  std::fill_n (ref.begin (), ia - 1, ref[static_cast<std::size_t> (ia - 1)]);
  std::fill (ref.begin () + ib, ref.end (), ref[static_cast<std::size_t> (ib - 1)]);
  for (float& value : ref)
    {
      value *= 4.0f;
      if (!(value > 0.0f))
        {
          value = 1.0f;
        }
    }
}

float percentile_value (float const* data, int npts, int rank)
{
  if (!data || npts <= 0)
    {
      return 0.0f;
    }
  std::vector<float> tmp (data, data + npts);
  float pct = 0.0f;
  pctile_ (tmp.data (), &npts, &rank, &pct);
  return pct;
}

void slope_normalize (std::vector<float>& y, double xpk)
{
  if (y.empty ())
    {
      return;
    }

  double sumw = 0.0;
  double sumx = 0.0;
  double sumy = 0.0;
  double sumx2 = 0.0;
  double sumxy = 0.0;
  for (std::size_t i = 0; i < y.size (); ++i)
    {
      double const x = static_cast<double> (i + 1);
      if (std::abs (x - xpk) <= 4.0)
        {
          continue;
        }
      sumw += 1.0;
      sumx += x;
      sumy += y[i];
      sumx2 += x * x;
      sumxy += x * y[i];
    }

  double const delta = sumw * sumx2 - sumx * sumx;
  if (std::fabs (delta) < 1.0e-30)
    {
      return;
    }

  double const a = (sumx2 * sumy - sumx * sumxy) / delta;
  double const b = (sumw * sumxy - sumx * sumy) / delta;
  double sq = 0.0;
  for (std::size_t i = 0; i < y.size (); ++i)
    {
      double const x = static_cast<double> (i + 1);
      y[i] = static_cast<float> (y[i] - (a + b * x));
      if (std::abs (x - xpk) > 4.0)
        {
          sq += y[i] * y[i];
        }
    }

  double const denom = std::max (1.0, sumw - 4.0);
  double const rms = std::sqrt (std::max (0.0, sq / denom));
  if (rms <= 1.0e-30)
    {
      return;
    }
  for (float& value : y)
    {
      value = static_cast<float> (value / rms);
    }
}

struct Jt65XcorResult
{
  std::vector<float> ccf;
  float ccf0 {0.0f};
  int lagpk {0};
  float flip {1.0f};
};

Jt65XcorResult jt65_xcor (int ipk, int nsteps, int nsym, int lag1, int lag2, float fdot,
                          bool robust, float const* ss_data)
{
  Jt65XcorResult result;
  result.ccf.assign (static_cast<std::size_t> (lag2 - lag1 + 1), 0.0f);
  if (!ss_data || nsteps <= 0 || nsym <= 0)
    {
      return result;
    }

  double const df = 12000.0 / kJt65Nfft;
  double const dtstep = 0.25 / df;
  double const fac = dtstep / (60.0 * df);
  std::vector<float> a (static_cast<std::size_t> (nsteps), 0.0f);
  for (int j = 1; j <= nsteps; ++j)
    {
      int const ii = static_cast<int> (std::lround ((j - nsteps / 2.0) * fdot * fac)) + ipk;
      if (ii >= 1 && ii <= kJt65Nsz)
        {
          a[static_cast<std::size_t> (j - 1)] = ss_data[jt65_ss_index (j, ii)];
        }
    }

  if (robust)
    {
      float const xmed = percentile_value (a.data (), nsteps, 50);
      for (float& value : a)
        {
          value = value >= xmed ? 1.0f : -1.0f;
        }
    }

  auto const& pr = jt65_pr_sequence ();
  float ccfmax = 0.0f;
  float ccfmin = 0.0f;
  int lagmax = lag1;
  int lagmin = lag1;
  for (int lag = lag1; lag <= lag2; ++lag)
    {
      float x = 0.0f;
      for (int i = 1; i <= nsym; ++i)
        {
          int const j = 4 * i - 3 + lag;
          if (j >= 1 && j <= nsteps)
            {
              x += a[static_cast<std::size_t> (j - 1)] * pr[static_cast<std::size_t> (i - 1)];
            }
        }
      float const ccf = 2.0f * x;
      result.ccf[static_cast<std::size_t> (lag - lag1)] = ccf;
      if (ccf > ccfmax)
        {
          ccfmax = ccf;
          lagmax = lag;
        }
      if (ccf < ccfmin)
        {
          ccfmin = ccf;
          lagmin = lag;
        }
    }

  result.ccf0 = ccfmax;
  result.lagpk = lagmax;
  result.flip = 1.0f;
  if (-ccfmin > ccfmax)
    {
      for (float& value : result.ccf)
        {
          value = -value;
        }
      result.lagpk = lagmin;
      result.ccf0 = -ccfmin;
      result.flip = -1.0f;
    }
  return result;
}

struct LegacyJt65CandidateRaw
{
  float freq;
  float dt;
  float sync;
  float flip;
};

static_assert (sizeof (LegacyJt65CandidateRaw) == sizeof (float) * 4,
               "JT65 candidate ABI changed");

int gray_transform (int value, int idir)
{
  if (idir > 0)
    {
      return value ^ (value >> 1);
    }

  int n = value;
  unsigned long sh = 1;
  unsigned long nn = static_cast<unsigned long> (n) >> sh;
  while (nn > 0)
    {
      n ^= static_cast<int> (nn);
      sh <<= 1;
      nn = static_cast<unsigned long> (n) >> sh;
    }
  return n;
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

int trimmed_length (QByteArray const& value)
{
  return decodium::legacy_jt::detail::trim_right_ascii (value).size ();
}

void chkhist_compute (std::array<int, 63> const& mrsym, int* nmax_out, int* ipk_out)
{
  int hist[64] {};
  for (int value : mrsym)
    {
      if (value >= 0 && value < 64)
        {
          ++hist[value];
        }
    }

  int nmax = 0;
  int ipk = 1;
  for (int i = 0; i < 64; ++i)
    {
      if (hist[i] > nmax)
        {
          nmax = hist[i];
          ipk = i + 1;
        }
    }

  if (nmax_out) *nmax_out = nmax;
  if (ipk_out) *ipk_out = ipk;
}

std::array<int, 12> pack_ap_words (QByteArray const& message)
{
  std::array<int, 12> ap {};
  ap.fill (-1);
  auto const packed = decodium::legacy_jt::detail::packmsg (
      decodium::legacy_jt::detail::fixed_ascii (message, 22));
  if (packed.itype != 1)
    {
      return ap;
    }
  ap = packed.dat;
  return ap;
}

void compute_averms (float const* x, int n, int nskip, float* ave, float* rms)
{
  if (!x || n <= 0 || !ave || !rms)
    {
      if (ave)
        {
          *ave = 0.0f;
        }
      if (rms)
        {
          *rms = 0.0f;
        }
      return;
    }

  int ipk = 0;
  for (int i = 1; i < n; ++i)
    {
      if (x[i] > x[ipk])
        {
          ipk = i;
        }
    }

  int ns = 0;
  double s = 0.0;
  double sq = 0.0;
  for (int i = 0; i < n; ++i)
    {
      if (nskip < 0 || std::abs (i - ipk) > nskip)
        {
          s += x[i];
          sq += x[i] * x[i];
          ++ns;
        }
    }
  if (ns <= 0)
    {
      *ave = 0.0f;
      *rms = 0.0f;
      return;
    }

  *ave = static_cast<float> (s / ns);
  double const variance = std::max (0.0, sq / ns - (*ave) * (*ave));
  *rms = static_cast<float> (std::sqrt (variance));
}

void gen_echocall_impl (QString const& callsign, int itone[6])
{
  std::fill (itone, itone + 6, 0);
  QByteArray const latin = callsign.left (6).toLatin1 ();
  for (int i = 0; i < latin.size () && i < 6; ++i)
    {
      int const ch = static_cast<unsigned char> (latin.at (i));
      if (ch >= '0' && ch <= '9')
        {
          itone[i] = ch - 47;
        }
      else if (ch >= 'A' && ch <= 'Z')
        {
          itone[i] = ch - 54;
        }
      else if (ch >= 'a' && ch <= 'z')
        {
          itone[i] = ch - 86;
        }
    }
}

void twkfreq_apply (std::complex<float> const* c3, std::complex<float>* c4, int npts,
                    float fsample, std::array<float, 3> const& a)
{
  if (!c3 || !c4 || npts <= 0 || fsample <= 0.0f)
    {
      return;
    }

  constexpr float twopi = 6.283185307f;
  std::complex<float> w {1.0f, 0.0f};
  float const x0 = 0.5f * (npts + 1);
  float const s = 2.0f / static_cast<float> (npts);
  for (int i = 1; i <= npts; ++i)
    {
      float const x = s * (i - x0);
      float const p2 = 1.5f * x * x - 0.5f;
      float const dphi = (a[0] + x * a[1] + p2 * a[2]) * (twopi / fsample);
      std::complex<float> const wstep {std::cos (dphi), std::sin (dphi)};
      w *= wstep;
      c4[static_cast<std::size_t> (i - 1)] = w * c3[static_cast<std::size_t> (i - 1)];
    }
}

void echo_snr_impl (float const* sa, float const* sb, float fspread, float* blue, float* red,
                    float* snrdb, float* db_err, float* fpeak, float* snr_detect)
{
  if (!sa || !sb || !blue || !red || !snrdb || !db_err || !fpeak || !snr_detect)
    {
      return;
    }

  float const df = 12000.0f / kEchoNfft;
  float const wh = 0.5f * fspread + 10.0f;
  int const i1 = static_cast<int> (std::lround ((1500.0f - 2.0f * wh) / df)) - 2048;
  int const i2 = static_cast<int> (std::lround ((1500.0f - wh) / df)) - 2048;
  int const i3 = static_cast<int> (std::lround ((1500.0f + wh) / df)) - 2048;
  int const i4 = static_cast<int> (std::lround ((1500.0f + 2.0f * wh) / df)) - 2048;

  auto sum_range = [] (float const* data, int begin_1based, int end_1based) -> double
  {
    double sum = 0.0;
    for (int i = begin_1based - 1; i < end_1based; ++i)
      {
        sum += data[i];
      }
    return sum;
  };

  float const baseline = static_cast<float> (
      (sum_range (sb, i1, i2 - 1) + sum_range (sb, i3 + 1, i4)) / (i2 + i4 - i1 - i3));

  for (int i = 0; i < kEchoNz; ++i)
    {
      blue[i] = sa[i] / baseline;
      red[i] = sb[i] / baseline;
    }

  double psig = 0.0;
  for (int i = i2 - 1; i < i3; ++i)
    {
      psig += red[i] - 1.0f;
    }
  float const pnoise_2500 = 2500.0f / df;
  *snrdb = static_cast<float> (db_value (psig / pnoise_2500));

  float smax = 0.0f;
  int const mh = std::max (1, static_cast<int> (std::lround (0.2f * fspread / df)));
  int ipk = i2;
  for (int i = i2; i <= i3; ++i)
    {
      float ssum = 0.0f;
      for (int j = i - mh; j <= i + mh; ++j)
        {
          ssum += red[j - 1];
        }
      if (ssum > smax)
        {
          smax = ssum;
          ipk = i;
        }
    }
  *fpeak = ipk * df - 750.0f;

  float ave1 = 0.0f;
  float rms1 = 0.0f;
  float ave2 = 0.0f;
  float rms2 = 0.0f;
  compute_averms (red + (i1 - 1), i2 - i1, -1, &ave1, &rms1);
  compute_averms (red + i3, i4 - i3, -1, &ave2, &rms2);
  float const perr =
      0.707f * (rms1 + rms2) * std::sqrt (static_cast<float> (i2 - i1 + i4 - i3));
  *snr_detect = psig / perr;
  *db_err = 99.0f;
  if (psig > perr)
    {
      *db_err = *snrdb - static_cast<float> (db_value ((psig - perr) / pnoise_2500));
    }
  if (*db_err < 0.5f)
    {
      *db_err = 0.5f;
    }
}

void decode_echo_inplace (short* id2, bool searching, QString* rxcall_out)
{
  if (!id2 || !rxcall_out)
    {
      return;
    }

  int ndop_total = 0;
  int ndop_audio = 0;
  int nfrit = 0;
  float f1 = 0.0f;
  float fspread = 0.0f;
  int ndf = 0;
  int itone[6] {};
  decodium::legacy::load_echo_params (id2, &ndop_total, &ndop_audio, &nfrit, &f1, &fspread, &ndf,
                                      itone);

  if (ndf < 10 || ndf > 30)
    {
      *rxcall_out = QStringLiteral ("      ");
      return;
    }
  if (f1 == 0.0f)
    {
      f1 = 1500.0f;
    }

  float const df = 12000.0f / kEchoNsps;
  int const i1 = static_cast<int> (std::lround ((f1 - 5.0f * ndf) / df));
  int const i2 = static_cast<int> (std::lround ((f1 + 42.0f * ndf) / df));
  if (i1 <= 0 || i1 > 4096 || i2 <= 0 || i2 > 4096)
    {
      *rxcall_out = QStringLiteral ("      ");
      return;
    }

  std::vector<std::complex<float>> c0 (static_cast<std::size_t> (kEchoTxSamples));
  std::vector<std::complex<float>> c1 (static_cast<std::size_t> (kEchoNsps));
  std::vector<std::complex<float>> c2 (static_cast<std::size_t> (kEchoTxSamples));
  std::vector<float> s (static_cast<std::size_t> (kEchoNsps), 0.0f);
  std::vector<float> p (static_cast<std::size_t> (kEchoNsps) * 6, 0.0f);

  float const fac = 2.0f / (32767.0f * kEchoTxSamples);
  for (int i = 0; i < kEchoTxSamples; ++i)
    {
      c0[static_cast<std::size_t> (i)] = std::complex<float> {fac * id2[i], 0.0f};
    }
  for (int i = 0; i < 15; ++i)
    {
      c0[static_cast<std::size_t> (i)] = std::complex<float> {};
    }
  four2a_forward_complex (c0.data (), kEchoTxSamples);
  for (int i = kEchoTxSamples / 2 + 1; i < kEchoTxSamples; ++i)
    {
      c0[static_cast<std::size_t> (i)] = std::complex<float> {};
    }
  c0[0] *= 0.5f;
  four2a_inverse_complex (c0.data (), kEchoTxSamples);

  QString rxcall (QStringLiteral ("      "));
  static char const alphabet[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (int j = 0; j < 6; ++j)
    {
      int const ia = j * kEchoNsps;
      std::copy_n (c0.data () + ia, kEchoNsps, c1.begin ());
      four2a_forward_complex (c1.data (), kEchoNsps);
      for (int i = 0; i <= kEchoNsps / 2; ++i)
        {
          auto const value = c1[static_cast<std::size_t> (i)];
          s[static_cast<std::size_t> (i)] = value.real () * value.real () + value.imag () * value.imag ();
        }
      if (!searching)
        {
          float* col = p.data () + j * kEchoNsps;
          for (int i = 0; i < kEchoNsps; ++i)
            {
              col[i] += s[static_cast<std::size_t> (i)];
            }
          auto const* begin = col + i1;
          auto const* end = col + i2 + 1;
          auto const it = std::max_element (begin, end);
          int const pos0 = static_cast<int> (it - col);
          int const k = static_cast<int> (std::lround (((pos0 * df) - f1) / ndf));
          if (k >= 1 && k <= 37)
            {
              rxcall[j] = QLatin1Char {alphabet[k]};
            }
        }
    }

  for (int j = 0; j < 6; ++j)
    {
      int const ia = j * kEchoNsps;
      std::array<float, 3> a {};
      a[0] = -itone[j] * ndf;
      twkfreq_apply (c0.data () + ia, c2.data () + ia, kEchoNsps, 12000.0f, a);
    }

  for (int i = 15; i < kEchoTxSamples; ++i)
    {
      id2[i] = static_cast<short> (std::lround (32767.0f * c2[static_cast<std::size_t> (i)].real ()));
    }

  *rxcall_out = rxcall;
}

struct CalibrationRow
{
  double fd {0.0};
  double deltaf {0.0};
  double residual {0.0};
  double rmsd {0.0};
  int count {0};
};

struct RefspectrumState
{
  bool initialized {false};
  bool last_use {false};
  int saved_count {0};
  std::vector<float> saved_tail;
  std::vector<float> spectrum_sum;
  std::vector<float> linear_filter;
  std::vector<float> fft_real_work;
  std::vector<std::complex<float>> causal_filter;
};

struct SymspecState
{
  int k0 {99999999};
  int ja {0};
  int nfft3z {0};
  std::vector<float> window;
  std::vector<float> spectrum_sum;
};

struct HspecState
{
  int k0 {99999999};
  int ja {0};
};

struct WsprDownsampleState
{
  int k0 {99999999};
  int k1 {0};
  int k8 {0};
  double phi {0.0};
  std::array<std::complex<float>, kWsprMixTail> tail {};
};

struct AvechoState
{
  float dop0 {0.0f};
  int navg0 {-1};
  std::vector<float> sax;
  std::vector<float> sbx;
};

RefspectrumState& refspectrum_state ()
{
  static RefspectrumState state;
  return state;
}

SymspecState& symspec_state ()
{
  static SymspecState state;
  return state;
}

HspecState& hspec_state ()
{
  static HspecState state;
  return state;
}

WsprDownsampleState& wspr_downsample_state ()
{
  static WsprDownsampleState state;
  return state;
}

AvechoState& avecho_state ()
{
  static AvechoState state;
  return state;
}

void smooth_sum_inplace (float* x, int npts, int nadd)
{
  if (!x || npts <= 0 || nadd <= 0)
    {
      return;
    }

  int const nh = nadd / 2;
  if (nh <= 0 || npts <= 2 * nh)
    {
      std::fill_n (x, npts, 0.0f);
      return;
    }

  std::vector<float> y (static_cast<std::size_t> (npts), 0.0f);
  for (int i = nh; i < npts - nh; ++i)
    {
      float sum = 0.0f;
      for (int j = -nh; j <= nh; ++j)
        {
          sum += x[i + j];
        }
      y[static_cast<std::size_t> (i)] = sum;
    }

  std::copy (y.begin (), y.end (), x);
  std::fill_n (x, nh, 0.0f);
  std::fill_n (x + (npts - nh), nh, 0.0f);
}

void make_nuttall_window (std::vector<float>& win)
{
  if (win.empty ())
    {
      return;
    }

  double const pi = 4.0 * std::atan (1.0);
  double constexpr a0 = 0.3635819;
  double constexpr a1 = -0.4891775;
  double constexpr a2 = 0.1365995;
  double constexpr a3 = -0.0106411;
  int const n = static_cast<int> (win.size ());
  for (int i = 0; i < n; ++i)
    {
      double const phase = 2.0 * pi * i / n;
      win[static_cast<std::size_t> (i)] = static_cast<float> (
          a0
        + a1 * std::cos (phase)
        + a2 * std::cos (2.0 * phase)
        + a3 * std::cos (3.0 * phase));
    }
}

void flat1_relative_baseline (float const* savg, int iz, int nsmo, float* syellow)
{
  if (!savg || !syellow || iz <= 0 || nsmo <= 0)
    {
      return;
    }

  std::vector<float> x (static_cast<std::size_t> (std::max (iz, 8192)), 0.0f);
  int const ia = nsmo / 2 + 1;
  int const ib = iz - nsmo / 2 - 1;
  int constexpr nstep = 20;
  int constexpr nh = nstep / 2;
  int npct = 50;

  for (int i = ia; i <= ib; i += nstep)
    {
      int const start = i - nsmo / 2 - 1;
      float pct = 0.0f;
      pctile_ (const_cast<float*> (savg + start), &nsmo, &npct, &pct);
      for (int j = i - nh - 1; j <= i + nh - 2; ++j)
        {
          if (j >= 0 && j < iz)
            {
              x[static_cast<std::size_t> (j)] = pct;
            }
        }
    }

  if (ia - 1 >= 0 && ia - 1 < iz)
    {
      std::fill_n (x.begin (), ia - 1, x[static_cast<std::size_t> (ia - 1)]);
    }
  if (ib - 1 >= 0 && ib - 1 < iz)
    {
      std::fill (x.begin () + ib, x.begin () + iz, x[static_cast<std::size_t> (ib - 1)]);
    }

  int const mid_lo = std::max (0, iz / 10 - 1);
  int const mid_hi = std::min (iz - 1, (9 * iz) / 10 - 1);
  float x0 = 0.0f;
  if (mid_lo <= mid_hi)
    {
      x0 = 0.001f * *std::max_element (x.begin () + mid_lo, x.begin () + mid_hi + 1);
    }

  for (int i = 0; i < iz; ++i)
    {
      syellow[i] = savg[i] / (x[static_cast<std::size_t> (i)] + x0);
    }
}

void fitcal_cpp (std::vector<CalibrationRow>& rows,
                 double* a_out, double* b_out, double* sigmaa_out,
                 double* sigmab_out, double* rms_out)
{
  int const iz = static_cast<int> (rows.size ());
  double sx = 0.0;
  double sy = 0.0;
  double sxy = 0.0;
  double sx2 = 0.0;
  for (CalibrationRow const& row : rows)
    {
      sx += row.fd;
      sy += row.deltaf;
      sxy += row.fd * row.deltaf;
      sx2 += row.fd * row.fd;
    }

  double const delta = iz * sx2 - sx * sx;
  double a = 0.0;
  double b = 0.0;
  if (std::fabs (delta) > 0.0)
    {
      a = (sx2 * sy - sx * sxy) / delta;
      b = (iz * sxy - sx * sy) / delta;
    }

  double sq = 0.0;
  for (CalibrationRow& row : rows)
    {
      row.residual = row.deltaf - (a + b * row.fd);
      sq += row.residual * row.residual;
    }

  double rms = 0.0;
  double sigmaa = 0.0;
  double sigmab = 0.0;
  if (iz >= 3 && std::fabs (delta) > 0.0)
    {
      rms = std::sqrt (sq / (iz - 2));
      sigmaa = std::sqrt (rms * rms * sx2 / delta);
      sigmab = std::sqrt (iz * rms * rms / delta);
    }

  if (a_out) *a_out = a;
  if (b_out) *b_out = b;
  if (sigmaa_out) *sigmaa_out = sigmaa;
  if (sigmab_out) *sigmab_out = sigmab;
  if (rms_out) *rms_out = rms;
}

}

namespace decodium
{
namespace legacy
{

void jt65_store_last_s1 (float const* src)
{
  if (!src)
    {
      std::fill (g_jt65_last_s1.begin (), g_jt65_last_s1.end (), 0.0f);
      return;
    }
  std::copy_n (src, g_jt65_last_s1.size (), g_jt65_last_s1.begin ());
}

void jt65_load_last_s1 (float* dst)
{
  if (!dst)
    {
      return;
    }
  std::copy (g_jt65_last_s1.begin (), g_jt65_last_s1.end (), dst);
}

void wav12_inplace (short* d2_io, int* npts_io, short sample_bits)
{
  if (!d2_io || !npts_io || *npts_io <= 0)
    {
      return;
    }

  constexpr int kNz11 = 60 * 11025;
  constexpr int kNz12 = 60 * 12000;
  constexpr int kNfft1 = 64 * 11025;
  constexpr int kNfft2 = 64 * 12000;

  int const jz = std::min (kNz11, *npts_io);
  std::vector<short> input (static_cast<size_t> (jz), 0);

  if (sample_bits == 8)
    {
      unsigned char const* bytes = reinterpret_cast<unsigned char const*> (d2_io);
      for (int i = 0; i < jz; ++i)
        {
          input[static_cast<size_t> (i)] = static_cast<short> (10 * (static_cast<int> (bytes[i]) - 128));
        }
    }
  else
    {
      std::copy_n (d2_io, jz, input.begin ());
    }

  std::vector<std::complex<float>> spectrum1 (static_cast<size_t> (kNfft1 / 2 + 1));
  std::vector<std::complex<float>> spectrum2 (static_cast<size_t> (kNfft2 / 2 + 1));
  float* real1 = reinterpret_cast<float*> (spectrum1.data ());
  float* real2 = reinterpret_cast<float*> (spectrum2.data ());

  std::fill_n (real1, kNfft1 + 2, 0.0f);
  std::fill_n (real2, kNfft2 + 2, 0.0f);
  for (int i = 0; i < jz; ++i)
    {
      real1[i] = static_cast<float> (input[static_cast<size_t> (i)]);
    }

  four2a_forward_real (spectrum1, kNfft1);
  std::fill (spectrum1.begin () + (kNfft1 / 2), spectrum1.end (), std::complex<float> {});
  std::copy (spectrum1.begin (), spectrum1.begin () + (kNfft1 / 2), spectrum2.begin ());
  four2a_inverse_real (spectrum2, kNfft2);

  int const output_count = static_cast<int> ((static_cast<double> (jz) * 12000.0) / 11025.0);
  *npts_io = output_count;

  constexpr float scale = 1.0e-6f;
  for (int i = 0; i < output_count; ++i)
    {
      d2_io[i] = static_cast<short> (std::lround (scale * real2[i]));
    }
  for (int i = output_count; i < kNz12; ++i)
    {
      d2_io[i] = 0;
    }
}

QString freqcal_line (short const* id2, int k, int nkhz, int noffset, int ntol)
{
  constexpr int kNfft = 55296;
  constexpr int kNh = kNfft / 2;
  if (!id2 || k < kNfft)
    {
      return {};
    }

  static std::vector<float> const window = [] {
    std::vector<float> values (static_cast<size_t> (kNfft));
    double const pi = 4.0 * std::atan (1.0);
    for (int i = 0; i < kNfft; ++i)
      {
        double const ww = std::sin (i * pi / kNfft);
        values[static_cast<size_t> (i)] = static_cast<float> (ww * ww / kNfft);
      }
    return values;
  }();
  static std::vector<float> const xi = [] {
    std::vector<float> values (static_cast<size_t> (kNfft));
    double const pi = 4.0 * std::atan (1.0);
    for (int i = 0; i < kNfft; ++i)
      {
        values[static_cast<size_t> (i)] = static_cast<float> (2.0 * pi * i);
      }
    return values;
  }();

  std::vector<std::complex<float>> buffer (static_cast<size_t> (kNh + 1));
  float* real = reinterpret_cast<float*> (buffer.data ());
  for (int i = 0; i < kNfft; ++i)
    {
      real[i] = window[static_cast<size_t> (i)] * id2[k - kNfft + i];
    }
  real[kNfft] = 0.0f;
  real[kNfft + 1] = 0.0f;
  four2a_forward_real (buffer, kNfft);

  double const fs = 12000.0;
  double const df = fs / kNfft;
  int ia = 0;
  int ib = 0;
  if (ntol > noffset)
    {
      ia = 0;
      ib = static_cast<int> (std::lround ((noffset * 2.0) / df));
    }
  else
    {
      ia = static_cast<int> (std::lround ((noffset - ntol) / df));
      ib = static_cast<int> (std::lround ((noffset + ntol) / df));
    }
  ia = std::max (0, ia);
  ib = std::min (kNh, ib);

  std::vector<float> spectrum (static_cast<size_t> (kNh + 1), 0.0f);
  double smax = 0.0;
  int ipk = -99;
  for (int i = ia; i <= ib; ++i)
    {
      std::complex<float> const value = buffer[static_cast<size_t> (i)];
      spectrum[static_cast<size_t> (i)] =
          value.real () * value.real () + value.imag () * value.imag ();
      if (spectrum[static_cast<size_t> (i)] > smax)
        {
          smax = spectrum[static_cast<size_t> (i)];
          ipk = i;
        }
    }

  double snr = -99.9;
  double pave = -99.9;
  double fpeak = -99.9;
  double ferr = -99.9;
  if (ipk >= 1 && ipk + 1 <= kNh)
    {
      double const dx = peakup_offset (spectrum[static_cast<size_t> (ipk - 1)],
                                       spectrum[static_cast<size_t> (ipk)],
                                       spectrum[static_cast<size_t> (ipk + 1)]);
      fpeak = df * (ipk + dx);
      double const ap = (fpeak / fs + 1.0 / (2.0 * kNfft));
      double const an = (fpeak / fs - 1.0 / (2.0 * kNfft));
      std::complex<double> sp {0.0, 0.0};
      std::complex<double> sn {0.0, 0.0};
      for (int i = 0; i < kNfft; ++i)
        {
          double const phase_p = xi[static_cast<size_t> (i)] * ap;
          double const phase_n = xi[static_cast<size_t> (i)] * an;
          double const sample = id2[k - kNfft + i];
          sp += sample * std::complex<double> (std::cos (phase_p), -std::sin (phase_p));
          sn += sample * std::complex<double> (std::cos (phase_n), -std::sin (phase_n));
        }
      double const sp_abs = std::abs (sp);
      double const sn_abs = std::abs (sn);
      if (sp_abs + sn_abs > 0.0)
        {
          fpeak += fs * (sp_abs - sn_abs) / (sp_abs + sn_abs) / (2.0 * kNfft);
        }

      double xsum = 0.0;
      int nsum = 0;
      for (int i = ia; i <= ib; ++i)
        {
          if (std::abs (i - ipk) > 10)
            {
              xsum += spectrum[static_cast<size_t> (i)];
              ++nsum;
            }
        }
      double const ave = nsum > 0 ? xsum / nsum : 0.0;
      snr = db_value (ave > 0.0 ? smax / ave : 0.0);
      pave = db_value (ave) + 8.0;
      ferr = fpeak - noffset;
    }

  QTime const now = QTime::currentTime ();
  QChar const cflag = snr < 20.0 ? QLatin1Char {'*'} : QLatin1Char {' '};
  QString suffix = QString::asprintf ("%7d%3d%6d%10.3f%10.3f%7.1f%7.1f  %c",
                                      nkhz, 1, noffset, fpeak, ferr, pave, snr,
                                      cflag.toLatin1 ());
  return QStringLiteral ("%1:%2:%3")
      .arg (now.hour (), 2, 10, QLatin1Char {'0'})
      .arg (now.minute (), 2, 10, QLatin1Char {'0'})
      .arg (now.second (), 2, 10, QLatin1Char {'0'})
      + suffix;
}

CalibrationSolution calibrate_freqcal_directory (QString const& data_dir)
{
  CalibrationSolution solution;
  QString const input_path = data_dir + QStringLiteral ("/fmt.all");
  QString const output_path = data_dir + QStringLiteral ("/fcal2.out");

  QFile input {input_path};
  if (!input.open (QIODevice::ReadOnly | QIODevice::Text))
    {
      solution.irc = -1;
      return solution;
    }

  QFile output {output_path};
  if (!output.open (QIODevice::WriteOnly | QIODevice::Text))
    {
      solution.irc = -2;
      return solution;
    }

  QTextStream in {&input};
  QTextStream out {&output};
  in.setLocale (QLocale::c ());
  out.setLocale (QLocale::c ());

  int current_khz = std::numeric_limits<int>::min ();
  double sum = 0.0;
  double sumsq = 0.0;
  int count = 0;
  std::vector<CalibrationRow> rows;
  int line_number = 0;

  auto flush_group = [&] {
    if (count <= 0 || current_khz == std::numeric_limits<int>::min ())
      {
        return;
      }
    CalibrationRow row;
    row.fd = 0.001 * current_khz;
    row.deltaf = sum / count;
    if (count > 1)
      {
        row.rmsd = std::sqrt (std::abs (sumsq - sum * sum / count) / (count - 1.0));
      }
    row.count = count;
    rows.push_back (row);
    sum = 0.0;
    sumsq = 0.0;
    count = 0;
  };

  while (!in.atEnd ())
    {
      QString const raw_line = in.readLine ();
      ++line_number;
      if (raw_line.trimmed ().isEmpty ())
        {
          continue;
        }

      QStringList const parts = raw_line.simplified ().split (QLatin1Char {' '}, Qt::SkipEmptyParts);
      if (parts.size () < 8)
        {
          solution.irc = -4;
          solution.iz = line_number;
          return solution;
        }

      bool ok = false;
      int const nkhz = parts[1].toInt (&ok);
      if (!ok)
        {
          solution.irc = -4;
          solution.iz = line_number;
          return solution;
        }
      double const noffset = parts[3].toDouble (&ok);
      if (!ok)
        {
          solution.irc = -4;
          solution.iz = line_number;
          return solution;
        }
      double const faudio = parts[4].toDouble (&ok);
      if (!ok)
        {
          solution.irc = -4;
          solution.iz = line_number;
          return solution;
        }

      if (nkhz != current_khz && current_khz != std::numeric_limits<int>::min ())
        {
          flush_group ();
        }
      current_khz = nkhz;

      double const dial_error = faudio - noffset;
      sum += dial_error;
      sumsq += dial_error * dial_error;
      ++count;
    }

  flush_group ();
  solution.iz = static_cast<int> (rows.size ());
  if (solution.iz < 2)
    {
      solution.irc = -3;
      return solution;
    }

  fitcal_cpp (rows, &solution.a, &solution.b, &solution.sigmaa, &solution.sigmab, &solution.rms);

  out << "    Freq      DF     Meas Freq    N    rms    Resid\n"
         "   (MHz)     (Hz)      (MHz)          (Hz)     (Hz)\n"
         "----------------------------------------------------\n";
  for (CalibrationRow const& row : rows)
    {
      double const fm = row.fd + 1.0e-6 * row.deltaf;
      QChar const flag = row.rmsd > 1.0 ? QLatin1Char {'*'} : QLatin1Char {' '};
      out << QString::asprintf ("%8.3f%9.3f%14.9f%4d%7.2f%9.3f %c\n",
                                row.fd, row.deltaf, fm, row.count,
                                row.rmsd, row.residual, flag.toLatin1 ());
    }

  solution.irc = 0;
  return solution;
}

void reset_refspectrum_state ()
{
  auto& state = refspectrum_state ();
  state = RefspectrumState {};
  std::fill_n (spectra_.syellow, NSMAX, 0.0f);
  std::fill_n (spectra_.ref, kRefNh + 1, 0.0f);
  std::fill_n (spectra_.filter, kRefNh + 1, 1.0f);
}

void refspectrum_update (short* id2, bool clear_reference, bool accumulate_reference,
                         bool use_reference, QString const& file_path)
{
  auto& state = refspectrum_state ();
  if (!state.initialized)
    {
      state.saved_tail.assign (static_cast<std::size_t> (kRefNh), 0.0f);
      state.spectrum_sum.assign (static_cast<std::size_t> (kRefNh + 1), 0.0f);
      state.fft_real_work.assign (static_cast<std::size_t> (kRefNfft + 2), 0.0f);
      state.causal_filter.assign (static_cast<std::size_t> (kRefNh + 1), std::complex<float> {});
      std::fill_n (spectra_.filter, kRefNh + 1, 1.0f);
      std::fill_n (spectra_.ref, kRefNh + 1, 0.0f);
      std::fill_n (spectra_.syellow, NSMAX, 0.0f);
      state.initialized = true;
    }

  if (!id2)
    {
      return;
    }

  if (clear_reference)
    {
      std::fill (state.spectrum_sum.begin (), state.spectrum_sum.end (), 0.0f);
    }

  if (accumulate_reference)
    {
      auto& real = state.fft_real_work;
      std::fill (real.begin (), real.end (), 0.0f);
      for (int i = 0; i < kRefNh; ++i)
        {
          real[i] = 0.001f * id2[i];
        }
      four2a_forward_real_buffer (real.data (), kRefNfft);
      auto const* spectrum = reinterpret_cast<std::complex<float> const*> (real.data ());

      for (int i = 1; i <= kRefNh; ++i)
        {
          auto const value = spectrum[i];
          state.spectrum_sum[static_cast<std::size_t> (i)] +=
              value.real () * value.real () + value.imag () * value.imag ();
        }
      ++state.saved_count;

      constexpr float fac0 = 0.9f;
      if ((state.saved_count % 4) == 0)
        {
          double const df = 12000.0 / kRefNfft;
          int ia = static_cast<int> (std::lround (1000.0 / df));
          int ib = static_cast<int> (std::lround (2000.0 / df));
          float const avemid = std::accumulate (state.spectrum_sum.begin () + ia,
                                                state.spectrum_sum.begin () + ib + 1,
                                                0.0f) / (ib - ia + 1);

          std::vector<float> fil (static_cast<std::size_t> (kRefNh + 1), 0.0f);
          for (int i = 0; i <= kRefNh; ++i)
            {
              float const sum = state.spectrum_sum[static_cast<std::size_t> (i)];
              if (sum > 0.0f)
                {
                  fil[static_cast<std::size_t> (i)] = std::sqrt (avemid / sum);
                }
            }

          ia = static_cast<int> (std::lround (240.0 / df));
          ib = static_cast<int> (std::lround (4000.0 / df));
          int const i0 = static_cast<int> (std::lround (1500.0 / df));
          int const ia0 = ia;
          int i = i0;
          for (; i >= ia0; --i)
            {
              if (state.spectrum_sum[static_cast<std::size_t> (i0)] > 0.0f
                  && state.spectrum_sum[static_cast<std::size_t> (i)]
                         / state.spectrum_sum[static_cast<std::size_t> (i0)] < 0.01f)
                {
                  break;
                }
            }
          ia = i;
          int const ib0 = ib;
          i = i0;
          for (; i <= ib0; ++i)
            {
              if (state.spectrum_sum[static_cast<std::size_t> (i0)] > 0.0f
                  && state.spectrum_sum[static_cast<std::size_t> (i)]
                         / state.spectrum_sum[static_cast<std::size_t> (i0)] < 0.01f)
                {
                  break;
                }
            }
          ib = i;

          float fac = fac0;
          for (int i = ia; i >= 1; --i)
            {
              fac *= fac0;
              fil[static_cast<std::size_t> (i)] *= fac;
            }

          fac = fac0;
          for (int i = ib; i <= kRefNh; ++i)
            {
              fac *= fac0;
              fil[static_cast<std::size_t> (i)] *= fac;
            }

          for (int iter = 0; iter < 100; ++iter)
            {
              decodium::plot::smooth121_inplace (fil.data (), kRefNh);
            }

          for (int i = 0; i <= kRefNh; ++i)
            {
              spectra_.filter[i] = -60.0f;
              if (state.spectrum_sum[static_cast<std::size_t> (i)] > 0.0f)
                {
                  spectra_.filter[i] = static_cast<float> (20.0 * std::log10 (fil[static_cast<std::size_t> (i)]));
                }
            }
          std::vector<float> linear_filter_quantized (static_cast<std::size_t> (kRefNh + 1), 0.0f);
          for (int i = 0; i <= kRefNh; ++i)
            {
              linear_filter_quantized[static_cast<std::size_t> (i)] =
                  quantize_fortran_e12_3 (fil[static_cast<std::size_t> (i)]);
            }
          state.linear_filter = linear_filter_quantized;

          int const il = static_cast<int> (std::lround (kRefPolyLow / df));
          int const ih = static_cast<int> (std::lround (kRefPolyHigh / df));
          int const nfit = ih - il + 1;
          int mode = 0;
          int nterms = 5;
          std::vector<double> xfit (static_cast<std::size_t> (nfit));
          std::vector<double> yfit (static_cast<std::size_t> (nfit));
          std::vector<double> sigmay (static_cast<std::size_t> (nfit), 1.0);
          std::array<double, 5> a {};
          double chisqr = 0.0;
          for (int i = 0; i < nfit; ++i)
            {
              xfit[static_cast<std::size_t> (i)] = (((i + il) * df) - 1500.0) / 1000.0;
              yfit[static_cast<std::size_t> (i)] = fil[static_cast<std::size_t> (i + il)];
            }
          polyfit_ (xfit.data (), yfit.data (), sigmay.data (), const_cast<int*> (&nfit),
                    &nterms, &mode, a.data (), &chisqr);

          QFile file {file_path};
          if (file.open (QIODevice::WriteOnly | QIODevice::Text))
            {
              QTextStream out {&file};
              out.setLocale (QLocale::c ());
              out << QString::asprintf ("%5d%5d%5d", kRefPolyLow, kRefPolyHigh, nterms);
              for (double coeff : a)
                {
                  out << QString::asprintf ("%25.16e", coeff);
                }
              out << '\n';
              for (int i = 1; i <= kRefNh; ++i)
                {
                  double const freq = i * df;
                  spectra_.ref[i] = static_cast<float> (db_value (state.spectrum_sum[static_cast<std::size_t> (i)] / avemid));
                  out << QString::asprintf ("%10.3f", freq)
                      << format_fortran_e12_3 (state.spectrum_sum[static_cast<std::size_t> (i)])
                      << QString::asprintf ("%12.6f",
                                            static_cast<double> (spectra_.ref[i]))
                      << format_fortran_e12_3 (
                             linear_filter_quantized[static_cast<std::size_t> (i)])
                      << QString::asprintf ("%12.6f\n",
                                            static_cast<double> (spectra_.filter[i]));
                }
            }
        }
      return;
    }

  if (use_reference)
    {
      if (state.last_use != use_reference)
        {
          std::vector<float> fil (static_cast<std::size_t> (kRefNh + 1), 1.0f);
          QFile file {file_path};
          if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
            {
              if (state.linear_filter.size () == static_cast<std::size_t> (kRefNh + 1))
                {
                  fil = state.linear_filter;
                }
              else
                {
                  return;
                }
            }
          else
            {
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
                      return;
                    }
                  auto const field = [&raw_line] (int start, int width) -> QString
                  {
                    return raw_line.mid (start, width).trimmed ();
                  };
                  bool ok = false;
                  double const s_value = field (10, 12).toDouble (&ok);
                  if (!ok)
                    {
                      return;
                    }
                  double const ref_value = field (22, 12).toDouble (&ok);
                  if (!ok)
                    {
                      return;
                    }
                  double const fil_value = field (34, 12).toDouble (&ok);
                  if (!ok)
                    {
                      return;
                    }
                  double const filter_value = field (46, 12).toDouble (&ok);
                  if (!ok)
                    {
                      return;
                    }
                  state.spectrum_sum[static_cast<std::size_t> (row)] = static_cast<float> (s_value);
                  spectra_.ref[row] = static_cast<float> (ref_value);
                  fil[static_cast<std::size_t> (row)] = static_cast<float> (fil_value);
                  spectra_.filter[row] = static_cast<float> (filter_value);
                  ++row;
                }
            }

          auto& real = state.fft_real_work;
          std::fill (real.begin (), real.end (), 0.0f);
          auto* work = reinterpret_cast<std::complex<float>*> (real.data ());
          work[0] = std::complex<float> {};
          for (int i = 1; i <= kRefNh; ++i)
            {
              float const value = fil[static_cast<std::size_t> (i)] / kRefNfft;
              work[i] = std::complex<float> {value, 0.0f};
            }
          four2a_inverse_real_buffer (real.data (), kRefNfft);
          std::rotate (real.begin (), real.begin () + (kRefNfft - 400), real.begin () + kRefNfft);
          std::fill (real.begin () + 800, real.begin () + kRefNh + 1, 0.0f);
          four2a_forward_real_buffer (real.data (), kRefNfft);
          state.causal_filter.assign (work, work + kRefNh + 1);
        }

      auto& real = state.fft_real_work;
      std::fill (real.begin (), real.end (), 0.0f);
      for (int i = 0; i < kRefNh; ++i)
        {
          real[i] = static_cast<float> (id2[i]);
        }
      for (int i = 0; i < kRefNfft; ++i)
        {
          real[i] /= kRefNfft;
        }
      four2a_forward_real_buffer (real.data (), kRefNfft);
      auto* filtered = reinterpret_cast<std::complex<float>*> (real.data ());
      for (int i = 0; i <= kRefNh; ++i)
        {
          filtered[i] *= state.causal_filter[static_cast<std::size_t> (i)];
        }
      four2a_inverse_real_buffer (real.data (), kRefNfft);
      for (int i = 0; i < kRefNh; ++i)
        {
          real[i] += state.saved_tail[static_cast<std::size_t> (i)];
        }
      std::copy_n (real.begin () + kRefNh, kRefNh, state.saved_tail.begin ());
      for (int i = 0; i < kRefNh; ++i)
        {
          id2[i] = static_cast<short> (std::lround (real[i]));
        }
    }

  state.last_use = use_reference;
}

void wspr_downsample_update (short const* id2, int k)
{
  if (!id2 || k > kWsprNmax)
    {
      return;
    }

  double const twopi = 8.0 * std::atan (1.0);
  constexpr std::array<float, 113> fir {
    -0.001818142144f,-0.000939132050f,-0.001044063556f,-0.001042685542f,
    -0.000908957610f,-0.000628132309f,-0.000202701465f, 0.000346307629f,
     0.000978154552f, 0.001634336295f, 0.002243121592f, 0.002726064379f,
     0.003006201675f, 0.003018055983f, 0.002717699575f, 0.002091546534f,
     0.001162489032f,-0.000007904811f,-0.001321554806f,-0.002649908053f,
    -0.003843608784f,-0.004747338068f,-0.005218967042f,-0.005148229529f,
    -0.004470167307f,-0.003177923811f,-0.001335998901f, 0.000915924193f,
     0.003386100636f, 0.005818719744f, 0.007939147967f, 0.009465071347f,
     0.010145641899f, 0.009787447819f, 0.008285915754f, 0.005645995244f,
     0.001995842303f,-0.002410369720f,-0.007202515555f,-0.011916811719f,
    -0.016028350845f,-0.018993391440f,-0.020297455955f,-0.019503792208f,
    -0.016298136197f,-0.010526834635f,-0.002223837363f, 0.008378305829f,
     0.020854478160f, 0.034608532659f, 0.048909701463f, 0.062944127288f,
     0.075874892030f, 0.086903764340f, 0.095332017649f, 0.100619428175f,
     0.102420526192f, 0.100619428175f, 0.095332017649f, 0.086903764340f,
     0.075874892030f, 0.062944127288f, 0.048909701463f, 0.034608532659f,
     0.020854478160f, 0.008378305829f,-0.002223837363f,-0.010526834635f,
    -0.016298136197f,-0.019503792208f,-0.020297455955f,-0.018993391440f,
    -0.016028350845f,-0.011916811719f,-0.007202515555f,-0.002410369720f,
     0.001995842303f, 0.005645995244f, 0.008285915754f, 0.009787447819f,
     0.010145641899f, 0.009465071347f, 0.007939147967f, 0.005818719744f,
     0.003386100636f, 0.000915924193f,-0.001335998901f,-0.003177923811f,
    -0.004470167307f,-0.005148229529f,-0.005218967042f,-0.004747338068f,
    -0.003843608784f,-0.002649908053f,-0.001321554806f,-0.000007904811f,
     0.001162489032f, 0.002091546534f, 0.002717699575f, 0.003018055983f,
     0.003006201675f, 0.002726064379f, 0.002243121592f, 0.001634336295f,
     0.000978154552f, 0.000346307629f,-0.000202701465f,-0.000628132309f,
    -0.000908957610f,-0.001042685542f,-0.001044063556f,-0.000939132050f,
    -0.001818142144f
  };

  auto& state = wspr_downsample_state ();
  int constexpr nfft3 = 8192 / 4;
  if (k < nfft3)
    {
      return;
    }
  if (k < state.k0)
    {
      state.k1 = 0;
      state.k8 = 0;
    }
  state.k0 = k;

  int const nblocks = (k - state.k1) / kWsprBlockSamples;
  double const dphi = twopi * 1500.0 / 12000.0;

  for (int block = 0; block < nblocks; ++block)
    {
      std::array<std::complex<float>, kWsprMixTail + kWsprBlockSamples> history {};
      std::copy (state.tail.begin (), state.tail.end (), history.begin ());

      std::array<std::complex<float>, kWsprBlockSamples> mixed {};
      for (int i = 0; i < kWsprBlockSamples; ++i)
        {
          state.phi += dphi;
          if (state.phi > twopi)
            {
              state.phi -= twopi;
            }
          float const xphi = static_cast<float> (state.phi);
          mixed[static_cast<std::size_t> (i)] =
              static_cast<float> (id2[state.k1 + i])
              * std::complex<float> {std::cos (xphi), std::sin (xphi)};
        }

      std::copy (mixed.begin (), mixed.end (), history.begin () + kWsprMixTail);
      for (int out = 0; out < kWsprMixOutput; ++out)
        {
          std::complex<float> z {};
          int const base = out * 8;
          for (int tap = 0; tap < int (fir.size ()); ++tap)
            {
              z += history[static_cast<std::size_t> (base + tap)] * fir[static_cast<std::size_t> (tap)];
            }
          c0com_.c0[state.k8 + out] = z;
        }

      std::copy (mixed.end () - kWsprMixTail, mixed.end (), state.tail.begin ());
      state.k1 += kWsprBlockSamples;
      state.k8 += kWsprMixOutput;
    }
}

int savec2_file (QString const& file_path, int tr_seconds, double f0m1500)
{
  int const ntrminutes = tr_seconds / 60;
  int npts = 114 * 1500;
  int nfft1 = 262144;
  if (ntrminutes == 15)
    {
      npts = 890 * 1500;
      nfft1 = kWsprSaveMaxFft;
    }
  if (npts > kWsprNdmax)
    {
      return 1;
    }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
  std::vector<std::complex<float>> c1 (static_cast<std::size_t> (nfft1));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
  float const fac = 1.0f / nfft1;
  for (int i = 0; i < npts; ++i)
    {
      c1[static_cast<std::size_t> (i)] = fac * c0com_.c0[i];
    }

  four2a_forward_complex (c1.data (), nfft1);

  int constexpr nfft2 = 65536;
  int constexpr nh2 = nfft2 / 2;
  std::vector<std::complex<float>> c2 (nfft2, std::complex<float> {});
  if (ntrminutes == 2)
    {
      std::copy_n (c1.begin (), nh2 + 1, c2.begin ());
      std::copy_n (c1.begin () + (nfft1 - nh2 + 1), nh2 - 1, c2.begin () + nh2 + 1);
    }
  else
    {
      double const df1 = 1500.0 / nfft1;
      int const i0 = static_cast<int> (std::lround (112.5 / df1));
      std::copy_n (c1.begin () + i0, nh2 + 1, c2.begin ());
      std::copy_n (c1.begin () + (i0 - nh2 + 1), nh2 - 1, c2.begin () + nh2 + 1);
    }

  four2a_inverse_complex (c2.data (), nfft2);

  QByteArray native = file_path.toLocal8Bit ();
  int i1 = native.indexOf (".c2");
  if (i1 < 0)
    {
      i1 = native.size ();
    }
  QByteArray outfile = native.mid (qMax (0, i1 - 11), 14);
  if (outfile.size () < 14)
    {
      outfile = outfile.rightJustified (14, ' ');
    }
  else if (outfile.size () > 14)
    {
      outfile.truncate (14);
    }

  QFile file {file_path};
  if (!file.open (QIODevice::WriteOnly))
    {
      return int (file.error ());
    }

  qint32 minutes = ntrminutes;
  file.write (outfile.constData (), outfile.size ());
  file.write (reinterpret_cast<char const*> (&minutes), sizeof minutes);
  file.write (reinterpret_cast<char const*> (&f0m1500), sizeof f0m1500);
  file.write (reinterpret_cast<char const*> (c2.data ()), 45000 * sizeof (std::complex<float>));
  return file.error () == QFileDevice::NoError ? 0 : int (file.error ());
}

void degrade_snr_inplace (short* d2, int npts, float db, float bandwidth)
{
  if (!d2 || npts <= 0)
    {
      return;
    }

  float p0 = 0.0f;
  for (int i = 0; i < npts; ++i)
    {
      float const x = d2[i];
      p0 += x * x;
    }
  p0 /= npts;
  if (bandwidth > 0.0f)
    {
      p0 *= 6000.0f / bandwidth;
    }

  float const s = std::sqrt (p0 * (std::pow (10.0f, 0.1f * db) - 1.0f));
  float const fac = std::sqrt (p0 / (p0 + s * s));
  for (int i = 0; i < npts; ++i)
    {
      d2[i] = static_cast<short> (std::lround (fac * (d2[i] + s * gran_ ())));
  }
}

Jt65SymspecResult symspec65_compute (float const* dd, int npts)
{
  Jt65SymspecResult result;
  result.ss.assign (static_cast<std::size_t> (kJt65MaxQsym) * kJt65Nsz, 0.0f);
  result.savg.assign (kJt65Nsz, 0.0f);
  result.ref.assign (kJt65Nsz, 1.0f);
  if (!dd || npts <= kJt65Nfft)
    {
      return result;
    }

  double const hstep = 2048.0 * 12000.0 / 11025.0;
  float const qstep = static_cast<float> (hstep / 2.0);
  result.nhsym = static_cast<int> ((npts - kJt65Nfft) / hstep);
  result.nqsym = static_cast<int> ((npts - kJt65Nfft) / qstep);
  if (result.nqsym < 0)
    {
      result.nqsym = 0;
    }
  else if (result.nqsym > kJt65MaxQsym)
    {
      result.nqsym = kJt65MaxQsym;
    }
  if (result.nqsym <= 0)
    {
      return result;
    }

  float constexpr fac1 = 1.0e-3f;
  auto const& window = jt65_window ();
  for (int j = 1; j <= result.nqsym; ++j)
    {
      std::vector<std::complex<float>> spectrum (static_cast<std::size_t> (kJt65Nfft / 2 + 1));
      float* real = reinterpret_cast<float*> (spectrum.data ());
      std::fill_n (real, kJt65Nfft + 2, 0.0f);
      int const i0 = static_cast<int> ((j - 1) * qstep);
      for (int i = 0; i < kJt65Nfft; ++i)
        {
          real[i] = fac1 * window[static_cast<std::size_t> (i)] * dd[i0 + i];
        }
      four2a_forward_real (spectrum, kJt65Nfft);
      for (int i = 1; i <= kJt65Nsz; ++i)
        {
          auto const value = spectrum[static_cast<std::size_t> (i)];
          float const s = value.real () * value.real () + value.imag () * value.imag ();
          result.ss[jt65_ss_index (j, i)] = s;
          result.savg[static_cast<std::size_t> (i - 1)] += s;
        }
    }

  int const nhsym_divisor = std::max (1, result.nhsym);
  for (float& value : result.savg)
    {
      value /= nhsym_divisor;
    }

  flat65_compute (result.ss, result.nqsym, result.ref);
  for (int i = 1; i <= kJt65Nsz; ++i)
    {
      float const denom = result.ref[static_cast<std::size_t> (i - 1)];
      result.savg[static_cast<std::size_t> (i - 1)] /= denom;
      for (int j = 1; j <= result.nqsym; ++j)
        {
          result.ss[jt65_ss_index (j, i)] /= denom;
        }
    }

  result.ok = true;
  return result;
}

std::vector<Jt65SyncCandidate> sync65_compute (int nfa, int nfb, int ntol, int nqsym,
                                               bool robust, bool vhf, float thresh0)
{
  (void) ntol;
  std::vector<Jt65SyncCandidate> candidates;
  if (nqsym <= 0)
    {
      return candidates;
    }

  float const df = 12000.0f / kJt65Nfft;
  int const ia = std::max (2, static_cast<int> (std::lround (nfa / df)));
  int const ib = std::min (kJt65Nsz - 1, static_cast<int> (std::lround (nfb / df)));
  if (ia > ib)
    {
      return candidates;
    }

  int constexpr lag1 = -32;
  int constexpr lag2 = 82;
  int constexpr nsym = 126;
  float ccfmax = 0.0f;
  int ipk = 0;
  std::vector<float> ccfred (static_cast<std::size_t> (kJt65Nsz + 2), 0.0f);
  for (int i = ia; i <= ib; ++i)
    {
      auto xcor = jt65_xcor (i, nqsym, nsym, lag1, lag2, 0.0f, robust, sync_.ss);
      if (!vhf)
        {
          slope_normalize (xcor.ccf, xcor.lagpk - lag1 + 1.0);
        }
      ccfred[static_cast<std::size_t> (i)] =
          xcor.ccf[static_cast<std::size_t> (xcor.lagpk - lag1)];
      if (ccfred[static_cast<std::size_t> (i)] > ccfmax)
        {
          ccfmax = ccfred[static_cast<std::size_t> (i)];
          ipk = i;
        }
    }

  float const xmed = percentile_value (ccfred.data () + ia, ib - ia + 1, 35);
  for (int i = ia; i <= ib; ++i)
    {
      ccfred[static_cast<std::size_t> (i)] -= xmed;
    }
  ccfred[static_cast<std::size_t> (ia - 1)] = ccfred[static_cast<std::size_t> (ia)];
  ccfred[static_cast<std::size_t> (ib + 1)] = ccfred[static_cast<std::size_t> (ib)];

  candidates.reserve (kJt65MaxCand);
  for (int i = ia; i <= ib; ++i)
    {
      bool itry = false;
      if (vhf)
        {
          if (i == ipk && ccfmax >= thresh0)
            {
              itry = true;
            }
        }
      else if (ccfred[static_cast<std::size_t> (i)] >= thresh0
               && ccfred[static_cast<std::size_t> (i)]
                      > ccfred[static_cast<std::size_t> (i - 1)]
               && ccfred[static_cast<std::size_t> (i)]
                      > ccfred[static_cast<std::size_t> (i + 1)])
        {
          itry = true;
        }

      if (!itry)
        {
          continue;
        }

      auto xcor = jt65_xcor (i, nqsym, nsym, lag1, lag2, 0.0f, robust, sync_.ss);
      if (!vhf)
        {
          slope_normalize (xcor.ccf, xcor.lagpk - lag1 + 1.0);
        }

      double xlag = xcor.lagpk;
      if (xcor.lagpk > lag1 && xcor.lagpk < lag2)
        {
          int const idx = xcor.lagpk - lag1;
          double const dx2 =
              peakup_offset (xcor.ccf[static_cast<std::size_t> (idx - 1)], ccfmax,
                             xcor.ccf[static_cast<std::size_t> (idx + 1)]);
          xlag += dx2;
        }

      Jt65SyncCandidate candidate;
      candidate.freq = i * df;
      candidate.dt = static_cast<float> (xlag * 1024.0 / 11025.0);
      candidate.flip = xcor.flip;
      if (vhf)
        {
          candidate.sync =
              static_cast<float> (db_value (ccfred[static_cast<std::size_t> (i)]) - 16.0);
        }
      else
        {
          candidate.sync = ccfred[static_cast<std::size_t> (i)];
        }
      candidates.push_back (candidate);
      if (static_cast<int> (candidates.size ()) == kJt65MaxCand)
        {
          break;
        }
    }

  return candidates;
}

void graycode65_inplace (int* dat, int n, int idir)
{
  if (!dat || n <= 0)
    {
      return;
    }
  for (int i = 0; i < n; ++i)
    {
      dat[i] = gray_transform (dat[i], idir);
    }
}

void interleave63_inplace (int* data, int idir)
{
  if (!data)
    {
      return;
    }

  int transposed[63] {};
  if (idir >= 0)
    {
      for (int l = 0; l < 63; ++l)
        {
          transposed[l] = data[(l / 9) + 7 * (l % 9)];
        }
      std::copy (std::begin (transposed), std::end (transposed), data);
    }
  else
    {
      for (int l = 0; l < 63; ++l)
        {
          transposed[l] = data[(l / 7) + 9 * (l % 7)];
        }
      std::copy (std::begin (transposed), std::end (transposed), data);
    }
}

Jt65Demod64aResult demod64a_compute (float const* s3, int nadd, float afac1)
{
  Jt65Demod64aResult result {};
  result.mrsym.fill (0);
  result.mrprob.fill (0);
  result.mr2sym.fill (0);
  result.mr2prob.fill (0);

  if (!s3 || nadd == -999)
    {
      return result;
    }

  double const afac = afac1 * std::pow (static_cast<double> (nadd), 0.64);
  double const scale = 255.999;
  (void) afac;

  double ave = 0.0;
  for (int j = 0; j < 63; ++j)
    {
      for (int i = 0; i < 64; ++i)
        {
          ave += s3[i + 64 * j];
        }
    }
  ave /= (64.0 * 63.0);
  if (ave == 0.0)
    {
      ave = 1.0e-6;
    }

  for (int j = 0; j < 63; ++j)
    {
      float s1 = -1.0e30f;
      double psum = 0.0;
      int i1 = 0;
      for (int i = 0; i < 64; ++i)
        {
          float const value = s3[i + 64 * j];
          double const x = std::min (afac * value / ave, 50.0);
          (void) x;
          psum += value;
          if (value > s1)
            {
              s1 = value;
              i1 = i;
            }
        }
      if (psum == 0.0)
        {
          psum = 1.0e-6;
        }

      float s2 = -1.0e30f;
      int i2 = 0;
      for (int i = 0; i < 64; ++i)
        {
          float const value = s3[i + 64 * j];
          if (i != i1 && value > s2)
            {
              s2 = value;
              i2 = i;
            }
        }

      double const p1 = s1 / psum;
      double const p2 = s2 / psum;
      result.mrsym[static_cast<std::size_t> (j)] = i1;
      result.mr2sym[static_cast<std::size_t> (j)] = i2;
      result.mrprob[static_cast<std::size_t> (j)] = static_cast<int> (scale * p1);
      result.mr2prob[static_cast<std::size_t> (j)] = static_cast<int> (scale * p2);
    }

  result.nlow = 0;
  result.ntest = 0;
  for (int j = 0; j < 63; ++j)
    {
      if (result.mrprob[static_cast<std::size_t> (j)] <= 5)
        {
          ++result.nlow;
        }
      result.ntest += result.mrprob[static_cast<std::size_t> (j)];
    }

  return result;
}

Jt65ExtractResult extract_compute (float const* s3, int nadd, int /*mode65*/, int ntrials,
                                   int naggressive, int ndepth, int nflip,
                                   QByteArray const& mycall_12, QByteArray const& hiscall_12,
                                   QByteArray const& hisgrid_6, int nQSOProgress,
                                   bool ljt65apon)
{
  Jt65ExtractResult result {};
  result.decoded = QByteArray (22, ' ');
  result.correct.fill (0);
  result.param.fill (0);
  result.mrs.fill (0);
  result.mrs2.fill (0);
  result.s3a.fill (0.0f);

  if (!s3)
    {
      return result;
    }

  std::array<float, 64 * 63> work {};
  std::copy_n (s3, work.size (), work.data ());

  int npct = 50;
  int npts = static_cast<int> (work.size ());
  float base = 1.0f;
  pctile_ (work.data (), &npts, &npct, &base);
  if (!(base > 0.0f))
    {
      base = 1.0f;
    }
  for (float& sample : work)
    {
      sample /= base;
    }
  result.s3a = work;
  std::copy (result.s3a.begin (), result.s3a.end (), __jt65_mod_MOD_s3a);

  float const afac1 = 1.1f;
  int nfail = 0;
  std::array<int, 63> mrsym {};
  std::array<int, 63> mrprob {};
  std::array<int, 63> mr2sym {};
  std::array<int, 63> mr2prob {};
  std::array<int, 63> raw_mrs {};
  std::array<int, 63> raw_mrs2 {};
  int ntest = 0;
  int nlow = 0;

  while (true)
    {
      auto const demod = demod64a_compute (work.data (), nadd, afac1);
      mrsym = demod.mrsym;
      mrprob = demod.mrprob;
      mr2sym = demod.mr2sym;
      mr2prob = demod.mr2prob;
      ntest = demod.ntest;
      nlow = demod.nlow;
      (void) ntest;
      (void) nlow;

      int ipk = 1;
      chkhist_compute (mrsym, &result.nhist, &ipk);
      if (result.nhist < 20)
        {
          break;
        }

      ++nfail;
      for (int j = 0; j < 63; ++j)
        {
          work[static_cast<std::size_t> ((ipk - 1) + 64 * j)] = base;
        }
      if (nfail > 30)
        {
          result.ncount = -1;
          return result;
        }
    }

  raw_mrs = mrsym;
  raw_mrs2 = mr2sym;
  result.mrs = raw_mrs;
  result.mrs2 = raw_mrs2;

  graycode65_inplace (mrsym.data (), 63, -1);
  interleave63_inplace (mrsym.data (), -1);
  interleave63_inplace (mrprob.data (), -1);

  graycode65_inplace (mr2sym.data (), 63, -1);
  interleave63_inplace (mr2sym.data (), -1);
  interleave63_inplace (mr2prob.data (), -1);

  std::array<int, 6> const nappasses {{3, 4, 2, 3, 3, 4}};
  std::array<std::array<int, 4>, 6> const naptypes {{
      {{1, 2, 6, 0}},
      {{2, 3, 6, 7}},
      {{2, 3, 0, 0}},
      {{3, 4, 5, 0}},
      {{3, 4, 5, 0}},
      {{2, 3, 4, 5}},
  }};

  std::array<std::array<int, 12>, 7> apsymbols {};
  for (auto& row : apsymbols)
    {
      row.fill (-1);
    }
  apsymbols[0][0] = 62;
  apsymbols[0][1] = 32;
  apsymbols[0][2] = 32;
  apsymbols[0][3] = 49;

  QByteArray const mycall = fixed_field (mycall_12.left (6), 6);
  QByteArray const hiscall = fixed_field (hiscall_12.left (6), 6);
  QByteArray const hisgrid = fixed_field (hisgrid_6.left (6), 6);

  if (ljt65apon && trimmed_length (mycall) > 0)
    {
      {
        auto const ap =
            pack_ap_words (mycall + QByteArray (" ", 1) + mycall + QByteArray (" RRR", 4));
        std::copy_n (ap.begin (), 4, apsymbols[1].begin ());
      }
      if (trimmed_length (hiscall) > 0)
        {
          {
            auto const ap =
                pack_ap_words (mycall + QByteArray (" ", 1) + hiscall + QByteArray (" RRR", 4));
            std::copy_n (ap.begin (), 9, apsymbols[2].begin ());
            apsymbols[3] = ap;
          }
          apsymbols[4] =
              pack_ap_words (mycall + QByteArray (" ", 1) + hiscall + QByteArray (" 73", 3));
          if (trimmed_length (hisgrid.left (4)) > 0)
            {
              apsymbols[5] = pack_ap_words (mycall + QByteArray (" ", 1) + hiscall + QByteArray (" ", 1)
                                            + hisgrid.left (4));
              apsymbols[6] = pack_ap_words (QByteArray ("CQ ", 3) + hiscall + QByteArray (" ", 1)
                                            + hisgrid.left (4));
            }
        }
    }

  result.qual = 0.0f;
  result.nft = 0;
  int last_nhard = -1;
  std::array<int, 63> correct {};
  int npass = 1;
  if (ljt65apon && trimmed_length (mycall) > 0 && nQSOProgress >= 0 && nQSOProgress <= 5)
    {
      npass = 1 + nappasses[static_cast<std::size_t> (nQSOProgress)];
    }

  for (int ipass = 1; ipass <= npass; ++ipass)
    {
      std::array<int, 12> ap {};
      ap.fill (-1);
      int ntype = 0;
      if (ipass > 1 && nQSOProgress >= 0 && nQSOProgress <= 5)
        {
          ntype = naptypes[static_cast<std::size_t> (nQSOProgress)][static_cast<std::size_t> (ipass - 2)];
          if (ntype == 0)
            {
              continue;
            }
          ap = apsymbols[static_cast<std::size_t> (ntype - 1)];
          if (std::count_if (ap.begin (), ap.end (), [] (int value) { return value >= 0; }) == 0)
            {
              continue;
            }
        }

      int ntry = 0;
      std::array<int, 10> param {};
      ftrsdap_ (mrsym.data (), mrprob.data (), mr2sym.data (), mr2prob.data (), ap.data (),
                &ntrials, correct.data (), param.data (), &ntry);
      result.correct = correct;
      result.param = param;

      int const nhard = param[1];
      int const ntotal = param[5];
      float const rtt = 0.001f * static_cast<float> (param[4]);
      result.qual = 0.001f * static_cast<float> (param[7]);
      last_nhard = nhard;

      int nd0 = 81;
      float r0 = 0.87f;
      if (naggressive == 10)
        {
          nd0 = 83;
          r0 = 0.90f;
        }
      if (ntotal <= nd0 && rtt <= r0)
        {
          result.nft = 1 + (ntype << 2);
          break;
        }
    }

  if (result.nft == 0 && (ndepth & 32) == 32)
    {
      float const qmin = 2.0f - 0.1f * static_cast<float> (naggressive);
      QByteArray decoded_hint (22, ' ');
      int nadd_mut = nadd;
      int nflip_mut = nflip;
      float qual = result.qual;
      hint65_ (work.data (), raw_mrs.data (), raw_mrs2.data (), &nadd_mut, &nflip_mut,
               mycall.constData (), hiscall.constData (), hisgrid.constData (),
               &qual, decoded_hint.data (),
               static_cast<fortran_charlen_t_local> (mycall.size ()),
               static_cast<fortran_charlen_t_local> (hiscall.size ()),
               static_cast<fortran_charlen_t_local> (hisgrid.size ()),
               static_cast<fortran_charlen_t_local> (decoded_hint.size ()));
      result.qual = qual;
      if (result.qual >= qmin)
        {
          result.nft = 2;
          result.ncount = 0;
          result.decoded = decoded_hint;
          return result;
        }
      result.decoded.fill (' ');
      return result;
    }

  result.ncount = -1;
  result.decoded.fill (' ');
  result.ltext = false;
  if (result.nft > 0)
    {
      std::array<int, 12> dat4 {};
      for (int i = 0; i < 12; ++i)
        {
          dat4[static_cast<std::size_t> (i)] = result.correct[static_cast<std::size_t> (11 - i)];
        }

      std::array<int, 63> forward_correct {};
      for (int i = 0; i < 63; ++i)
        {
          forward_correct[static_cast<std::size_t> (i)] =
              result.correct[static_cast<std::size_t> (62 - i)];
        }
      interleave63_inplace (forward_correct.data (), 1);
      graycode65_inplace (forward_correct.data (), 63, 1);
      result.correct = forward_correct;

      result.decoded = decodium::legacy_jt::detail::fixed_ascii (
          decodium::legacy_jt::detail::unpackmsg (dat4), 22);
      result.ncount = 0;
      result.ltext = (dat4[9] & 8) != 0;
    }

  if (result.nft == 1 && last_nhard < 0)
    {
      result.decoded.fill (' ');
    }

  return result;
}

extern "C" void extract_ (float s3[], int* nadd, int* mode65, int* ntrials, int* naggressive,
                          int* ndepth, int* nflip, char const* mycall_12,
                          char const* hiscall_12, char const* hisgrid, int* nQSOProgress,
                          int* ljt65apon, int* ncount, int* nhist, char decoded[],
                          int* ltext, int* nft, float* qual,
                          fortran_charlen_t_local mycall_len,
                          fortran_charlen_t_local hiscall_len,
                          fortran_charlen_t_local hisgrid_len,
                          fortran_charlen_t_local decoded_len)
{
  if (!s3 || !nadd || !mode65 || !ntrials || !naggressive || !ndepth || !nflip || !mycall_12
      || !hiscall_12 || !hisgrid || !nQSOProgress || !ljt65apon || !nhist || !nft || !qual)
    {
      return;
    }

  auto const mycall = fixed_field (QByteArray (mycall_12, static_cast<int> (mycall_len)), 12);
  auto const hiscall = fixed_field (QByteArray (hiscall_12, static_cast<int> (hiscall_len)), 12);
  auto const grid = fixed_field (QByteArray (hisgrid, static_cast<int> (hisgrid_len)), 6);

  auto const result =
      extract_compute (s3, *nadd, *mode65, *ntrials, *naggressive, *ndepth, *nflip, mycall,
                       hiscall, grid, *nQSOProgress, *ljt65apon != 0);

  std::copy (result.param.begin (), result.param.end (), __jt65_mod_MOD_param);
  std::copy (result.mrs.begin (), result.mrs.end (), __jt65_mod_MOD_mrs);
  std::copy (result.mrs2.begin (), result.mrs2.end (), __jt65_mod_MOD_mrs2);
  std::copy (result.s3a.begin (), result.s3a.end (), __jt65_mod_MOD_s3a);
  std::copy (result.correct.begin (), result.correct.end (), chansyms65_.correct);

  if (ncount && result.ncount != -99)
    {
      *ncount = result.ncount;
    }
  *nhist = result.nhist;
  *nft = result.nft;
  *qual = result.qual;
  if (ltext)
    {
      *ltext = result.ltext ? 1 : 0;
    }

  if (decoded && decoded_len > 0)
    {
      std::fill_n (decoded, static_cast<std::size_t> (decoded_len), ' ');
      auto const bytes = std::min<std::size_t> (static_cast<std::size_t> (decoded_len),
                                                static_cast<std::size_t> (result.decoded.size ()));
      if (bytes > 0)
        {
          std::memcpy (decoded, result.decoded.constData (), bytes);
        }
    }
}

extern "C" void getpp_ (int workdat[], float* p)
{
  if (!workdat || !p)
    {
      return;
    }

  std::array<int, 63> a {};
  for (int i = 0; i < 63; ++i)
    {
      a[static_cast<std::size_t> (i)] = workdat[62 - i];
    }
  interleave63_inplace (a.data (), 1);
  graycode65_inplace (a.data (), 63, 1);

  float psum = 0.0f;
  for (int j = 0; j < 63; ++j)
    {
      int const i = a[static_cast<std::size_t> (j)] + 1;
      if (i >= 1 && i <= 64)
        {
          psum += __jt65_mod_MOD_s3a[static_cast<std::size_t> ((i - 1) + 64 * j)];
        }
    }
  *p = psum / 63.0f;
}

Jt65Decode65bResult decode65b_compute (float const* s2, int nflip, int nadd, int mode65,
                                       int ntrials, int naggressive, int ndepth,
                                       QByteArray const& mycall_12, QByteArray const& hiscall_12,
                                       QByteArray const& hisgrid_6, int nQSOProgress,
                                       bool ljt65apon)
{
  Jt65Decode65bResult result;
  result.decoded = QByteArray (22, ' ');

  std::array<float, 64 * 63> s3 {};
  for (int j = 1; j <= 63; ++j)
    {
      int k = __jt65_mod_MOD_mdat[static_cast<std::size_t> (j - 1)];
      if (nflip < 0)
        {
          k = __jt65_mod_MOD_mdat2[static_cast<std::size_t> (j - 1)];
        }
      for (int i = 1; i <= 64; ++i)
        {
          s3[static_cast<std::size_t> ((i - 1) + 64 * (j - 1))] =
              s2[static_cast<std::size_t> ((i + 1) + 66 * (k - 1))];
        }
    }

  auto const extract = extract_compute (s3.data (), nadd, mode65, ntrials, naggressive, ndepth,
                                        nflip, fixed_field (mycall_12, 12),
                                        fixed_field (hiscall_12, 12), fixed_field (hisgrid_6, 6),
                                        nQSOProgress, ljt65apon);
  result.nft = extract.nft;
  result.qual = extract.qual;
  result.nhist = extract.nhist;
  result.decoded = fixed_field (extract.decoded, 22);

  int ncount = extract.ncount;
  if (result.decoded.left (7) == "000AAA " || result.decoded.left (7) == "0L6MWK ")
    {
      ncount = -1;
    }
  if (nflip < 0 && extract.ltext)
    {
      ncount = -1;
    }
  if (ncount < 0)
    {
      result.nft = 0;
      result.decoded.fill (' ');
    }

  return result;
}

extern "C" void decode65b_ (float s2[], int* nflip, int* nadd, int* mode65, int* ntrials,
                            int* naggressive, int* ndepth, char const* mycall,
                            char const* hiscall, char const* hisgrid, int* nQSOProgress,
                            int* ljt65apon, int* nqd, int* nft, float* qual, int* nhist,
                            char decoded[], fortran_charlen_t_local mycall_len,
                            fortran_charlen_t_local hiscall_len,
                            fortran_charlen_t_local hisgrid_len,
                            fortran_charlen_t_local decoded_len)
{
  if (!s2 || !nflip || !nadd || !mode65 || !ntrials || !naggressive || !ndepth || !mycall
      || !hiscall || !hisgrid || !nQSOProgress || !ljt65apon || !nft || !qual || !nhist
      || !decoded)
    {
      return;
    }
  (void) nqd;

  auto const result = decode65b_compute (
      s2, *nflip, *nadd, *mode65, *ntrials, *naggressive, *ndepth,
      QByteArray (mycall, static_cast<int> (mycall_len)),
      QByteArray (hiscall, static_cast<int> (hiscall_len)),
      QByteArray (hisgrid, static_cast<int> (hisgrid_len)),
      *nQSOProgress, *ljt65apon != 0);

  *nhist = result.nhist;
  *nft = result.nft;
  *qual = result.qual;
  std::fill_n (decoded, static_cast<std::size_t> (decoded_len), ' ');
  auto const bytes = std::min<std::size_t> (static_cast<std::size_t> (decoded_len),
                                            static_cast<std::size_t> (result.decoded.size ()));
  if (bytes > 0)
    {
      std::memcpy (decoded, result.decoded.constData (), bytes);
    }
}

Jt65Decode65aResult decode65a_compute (float const* dd, int npts, int newdat, int nqd, float f0,
                                       int nflip, int mode65, int ntrials, int naggressive,
                                       int ndepth, int ntol, QByteArray const& mycall_12,
                                       QByteArray const& hiscall_12, QByteArray const& hisgrid_6,
                                       int nQSOProgress, bool ljt65apon, bool vhf,
                                       float dt_hint)
{
  (void) nqd;
  initialize_jt65_tables ();

  Jt65Decode65aResult result;
  result.decoded = QByteArray (22, ' ');
  result.dt = dt_hint;

  std::array<float, 5> a_local {};
  float sync2_local = result.sync2;
  float dt_local = result.dt;
  int nft_local = result.nft;
  int nspecial_local = result.nspecial;
  float qual_local = result.qual;
  int nhist_local = result.nhist;
  int nsmo_local = result.nsmo;

  int const max_complex = kJt65DecodeNmax / 8;
  std::vector<std::complex<float>> cx (static_cast<std::size_t> (max_complex), std::complex<float> {});
  std::vector<std::complex<float>> cx1 (static_cast<std::size_t> (max_complex), std::complex<float> {});
  std::vector<std::complex<float>> c5x (static_cast<std::size_t> (kJt65DecodeNmax / 32),
                                        std::complex<float> {});
  std::array<std::complex<float>, kJt65DecodeFft> c5a {};
  std::vector<float> s1_local (static_cast<std::size_t> (kJt65DecodeRows * kJt65DecodeCols), 0.0f);
  std::vector<float> s2 (static_cast<std::size_t> (66 * 126), 0.0f);

  int n5 = 0;
  float sq0 = 0.0f;
  filbig_ (const_cast<float*> (dd), &npts, &f0, &newdat, cx.data (), &n5, &sq0);
  if (mode65 == 4)
    {
      float f1 = f0 + 355.297852f;
      int newdat_secondary = newdat;
      filbig_ (const_cast<float*> (dd), &npts, &f1, &newdat_secondary, cx1.data (), &n5, &sq0);
    }

  if (vhf && mode65 != 101)
    {
      float xdf = 0.0f;
      float shorthand_sync = 0.0f;
      sh65_ (cx.data (), &n5, &mode65, &ntol, &xdf, &nspecial_local, &shorthand_sync);
      if (nspecial_local > 0)
        {
          a_local.fill (0.0f);
          a_local[0] = xdf;
          nflip = 0;
        }
    }

  if (nflip != 0)
    {
      int n6 = 0;
      fil6521_ (cx.data (), &n5, c5x.data (), &n6);

      float const fsample = 1378.125f / 4.0f;
      float ccfbest = 0.0f;
      float dtbest = dt_local;
      afc65b_ (c5x.data (), &n6, const_cast<float*> (&fsample), &nflip, &mode65, a_local.data (),
               &ccfbest, &dtbest);
      dtbest += 0.003628f;
      dt_local = dtbest;
      sync2_local = 3.7e-4f * ccfbest / sq0;
      if (mode65 == 4)
        {
          cx = cx1;
        }

      a_local[2] = 0.0f;
      twkfreq65_ (cx.data (), &n5, a_local.data ());

      float const df = 1378.125f / kJt65DecodeFft;
      int j = static_cast<int> (dtbest * 1378.125f);
      for (int k = 1; k <= 126; ++k)
        {
          c5a.fill (std::complex<float> {});
          for (int i = 1; i <= kJt65DecodeFft; ++i)
            {
              ++j;
              if (j >= 1 && j <= max_complex)
                {
                  c5a[static_cast<std::size_t> (i - 1)] =
                      cx[static_cast<std::size_t> (j - 1)];
                }
            }
          four2a_complex_sign (c5a.data (), kJt65DecodeFft, 1);
          for (int i = 1; i <= kJt65DecodeFft; ++i)
            {
              int jj = i;
              if (i > 256)
                {
                  jj = i - 512;
                }
              s1_local[jt65_s1_index (jj, k)] =
                  std::norm (c5a[static_cast<std::size_t> (i - 1)]);
            }
        }

      float qualbest = 0.0f;
      int nftbest = 0;
      float qual0 = -1.0e30f;
      int minsmo = 0;
      int maxsmo = 0;
      if (mode65 >= 2 && mode65 != 101)
        {
          minsmo = static_cast<int> (std::lround (__jt65_mod_MOD_width / df)) - 1;
          maxsmo = 2 * static_cast<int> (std::lround (__jt65_mod_MOD_width / df));
        }
      int nsmobest = 0;
      QByteArray decoded_best = result.decoded;

      auto const mycall_field = fixed_field (mycall_12, 12);
      auto const hiscall_field = fixed_field (hiscall_12, 12);
      auto const hisgrid_field = fixed_field (hisgrid_6, 6);

      for (int ismo = minsmo; ismo <= maxsmo; ++ismo)
        {
          if (ismo > 0)
            {
              int nz = kJt65DecodeRows;
              for (int col = 1; col <= 126; ++col)
                {
                  float* column = s1_local.data ()
                                  + static_cast<std::ptrdiff_t> ((col - 1) * kJt65DecodeRows);
                  smo121_ (column, &nz);
                }
            }

          for (int row = 1; row <= 66; ++row)
            {
              int jj = row;
              if (mode65 == 2)
                {
                  jj = 2 * row - 1;
                }
              if (mode65 == 4)
                {
                  float const ff = 4.0f * (row - 1) * df - 355.297852f;
                  jj = static_cast<int> (std::lround (ff / df)) + 1;
                }
              std::size_t const source_row = static_cast<std::size_t> (jj + 255);
              for (int col = 1; col <= 126; ++col)
                {
                  s2[static_cast<std::size_t> ((row - 1) + 66 * (col - 1))] =
                      s1_local[source_row
                               + static_cast<std::size_t> (kJt65DecodeRows * (col - 1))];
                }
            }

          auto const decode = decode65b_compute (s2.data (), nflip, ismo, mode65, ntrials,
                                                 naggressive, ndepth, mycall_field, hiscall_field,
                                                 hisgrid_field, nQSOProgress, ljt65apon);
          nhist_local = decode.nhist;
          qual_local = decode.qual;

          if (decode.nft == 1)
            {
              nsmo_local = ismo;
              __jt65_mod_MOD_param[9] = nsmo_local;
              nft_local = decode.nft;
              qual_local = decode.qual;
              result.decoded = decode.decoded;
              break;
            }
          if (decode.nft == 2 && decode.qual > qualbest)
            {
              decoded_best = decode.decoded;
              qualbest = decode.qual;
              nsmobest = ismo;
              nftbest = decode.nft;
            }
          if (decode.qual < qual0)
            {
              break;
            }
          qual0 = decode.qual;
        }

      if (nftbest == 2)
        {
          result.decoded = decoded_best;
          qual_local = qualbest;
          nsmo_local = nsmobest;
          __jt65_mod_MOD_param[9] = nsmo_local;
          nft_local = nftbest;
        }

      jt65_store_last_s1 (s1_local.data ());
    }
  else
    {
      jt65_store_last_s1 (nullptr);
    }

  result.sync2 = sync2_local;
  result.a = a_local;
  result.dt = dt_local;
  result.nft = nft_local;
  result.nspecial = nspecial_local;
  result.qual = qual_local;
  result.nhist = nhist_local;
  result.nsmo = nsmo_local;
  if (result.decoded.isEmpty ())
    {
      result.decoded = QByteArray (22, ' ');
    }
  return result;
}

extern "C" void decode65a_ (float dd[], int* npts, int* newdat, int* nqd, float* f0,
                            int* nflip, int* mode65, int* ntrials, int* naggressive,
                            int* ndepth, int* ntol, char const* mycall, char const* hiscall,
                            char const* hisgrid, int* nQSOProgress, int* ljt65apon,
                            int* bVHF, float* sync2, float a[], float* dt, int* nft,
                            int* nspecial, float* qual, int* nhist, int* nsmo,
                            char decoded[], fortran_charlen_t_local mycall_len,
                            fortran_charlen_t_local hiscall_len,
                            fortran_charlen_t_local hisgrid_len,
                            fortran_charlen_t_local decoded_len)
{
  if (!dd || !npts || !newdat || !f0 || !nflip || !mode65 || !ntrials || !naggressive
      || !ndepth || !ntol || !mycall || !hiscall || !hisgrid || !nQSOProgress || !ljt65apon
      || !bVHF || !sync2 || !a || !dt || !nft || !nspecial || !qual || !nhist || !nsmo
      || !decoded)
    {
      return;
    }

  auto const result = decode65a_compute (
      dd, *npts, *newdat, nqd ? *nqd : 0, *f0, *nflip, *mode65, *ntrials, *naggressive, *ndepth,
      *ntol, QByteArray (mycall, static_cast<int> (mycall_len)),
      QByteArray (hiscall, static_cast<int> (hiscall_len)),
      QByteArray (hisgrid, static_cast<int> (hisgrid_len)),
      *nQSOProgress, *ljt65apon != 0, *bVHF != 0, *dt);

  *sync2 = result.sync2;
  std::copy_n (result.a.begin (), 5, a);
  *dt = result.dt;
  *nft = result.nft;
  *nspecial = result.nspecial;
  *qual = result.qual;
  *nhist = result.nhist;
  *nsmo = result.nsmo;
  std::fill_n (decoded, static_cast<std::size_t> (decoded_len), ' ');
  auto const bytes =
      std::min<std::size_t> (static_cast<std::size_t> (decoded_len),
                             static_cast<std::size_t> (result.decoded.size ()));
  if (bytes > 0)
    {
      std::memcpy (decoded, result.decoded.constData (), bytes);
    }
}

extern "C" void symspec65_ (float dd[], int* npts, int* nqsym, float savg[])
{
  if (!npts || !nqsym || !savg)
    {
      return;
    }

  decodium::legacy::Jt65SymspecResult const result =
      decodium::legacy::symspec65_compute (dd, *npts);
  *nqsym = result.nqsym;
  if (!result.ok)
    {
      std::fill_n (savg, kJt65Nsz, 0.0f);
      std::fill_n (refspec_.ref, kJt65Nsz, 1.0f);
      refspec_.dfref = 12000.0f / kJt65Nfft;
      std::fill_n (sync_.ss, kJt65MaxQsym * kJt65Nsz, 0.0f);
      return;
    }

  std::copy_n (result.savg.data (), kJt65Nsz, savg);
  std::copy_n (result.ref.data (), kJt65Nsz, refspec_.ref);
  refspec_.dfref = 12000.0f / kJt65Nfft;
  std::copy_n (result.ss.data (), kJt65MaxQsym * kJt65Nsz, sync_.ss);
}

extern "C" void sync65_ (int* nfa, int* nfb, int* ntol, int* nqsym, LegacyJt65CandidateRaw ca[],
                         int* ncand, int* nrobust, int* bVHF)
{
  if (!nfa || !nfb || !ntol || !nqsym || !ca || !ncand)
    {
      return;
    }

  auto const candidates = decodium::legacy::sync65_compute (
      *nfa, *nfb, *ntol, *nqsym, nrobust && *nrobust != 0, bVHF && *bVHF != 0,
      steve_.thresh0);
  *ncand = static_cast<int> (candidates.size ());
  for (int i = 0; i < *ncand; ++i)
    {
      ca[i].freq = candidates[static_cast<std::size_t> (i)].freq;
      ca[i].dt = candidates[static_cast<std::size_t> (i)].dt;
      ca[i].sync = candidates[static_cast<std::size_t> (i)].sync;
      ca[i].flip = candidates[static_cast<std::size_t> (i)].flip;
    }
}

extern "C" void graycode65_ (int dat[], int* n, int* idir)
{
  if (!n || !idir)
    {
      return;
    }
  decodium::legacy::graycode65_inplace (dat, *n, *idir);
}

extern "C" void jt65_init_tables_ ()
{
  initialize_jt65_tables ();
  jt65_store_last_s1 (nullptr);
}

extern "C" void interleave63_ (int d1[], int* idir)
{
  if (!idir)
    {
      return;
    }
  decodium::legacy::interleave63_inplace (d1, *idir);
}

extern "C" void demod64a_ (float s3[], int* nadd, float* afac1, int mrsym[], int mrprob[],
                           int mr2sym[], int mr2prob[], int* ntest, int* nlow)
{
  if (!nadd || !afac1 || !mrsym || !mrprob || !mr2sym || !mr2prob || !ntest || !nlow)
    {
      return;
    }

  decodium::legacy::Jt65Demod64aResult const result =
      decodium::legacy::demod64a_compute (s3, *nadd, *afac1);
  std::copy (result.mrsym.begin (), result.mrsym.end (), mrsym);
  std::copy (result.mrprob.begin (), result.mrprob.end (), mrprob);
  std::copy (result.mr2sym.begin (), result.mr2sym.end (), mr2sym);
  std::copy (result.mr2prob.begin (), result.mr2prob.end (), mr2prob);
  *ntest = result.ntest;
  *nlow = result.nlow;
}

void symspec_update (dec_data_t* shared_data, int k, int nsps, int ingain,
                     bool low_sidelobes, int minw, float* pxdb, float* s, float* df3,
                     int* ihsym, int* npts8, float* pxdbmax)
{
  if (!shared_data || !pxdb || !s || !df3 || !ihsym || !npts8 || !pxdbmax)
    {
      return;
    }

  auto& state = symspec_state ();
  if (state.window.empty ())
    {
      state.window.resize (static_cast<std::size_t> (kSymspecNfft3));
      make_nuttall_window (state.window);
      state.nfft3z = kSymspecNfft3;
    }
  if (state.spectrum_sum.empty ())
    {
      state.spectrum_sum.assign (kSymspecMaxFreqBins, 0.0f);
    }

  int const jstep = nsps / 2;
  int constexpr max_samples = NTMAX * RX_SAMPLE_RATE;
  if (k > max_samples)
    {
      return;
    }
  if (k < 2048)
    {
      *ihsym = 0;
      *npts8 = k / 8;
      return;
    }

  if (k < state.k0)
    {
      state.k0 = 0;
      state.ja = 0;
      std::fill (state.spectrum_sum.begin (), state.spectrum_sum.end (), 0.0f);
      *ihsym = 0;
      if (!shared_data->params.ndiskdat)
        {
          std::fill (shared_data->d2 + k, shared_data->d2 + max_samples, 0);
        }
    }

  double const gain = std::pow (10.0, 0.1 * ingain);
  double sq = 0.0;
  float pxmax = 0.0f;
  for (int i = state.k0 + 1; i <= k; ++i)
    {
      if (state.k0 == 0 && i <= 10)
        {
          continue;
        }
      float const x1 = static_cast<float> (shared_data->d2[i - 1]);
      pxmax = std::max (pxmax, std::fabs (x1));
      sq += x1 * x1;
    }

  *pxdb = 0.0f;
  if (k > state.k0 && sq > 0.0)
    {
      *pxdb = static_cast<float> (10.0 * std::log10 (sq / (k - state.k0)));
    }
  *pxdbmax = 0.0f;
  if (pxmax > 0.0f)
    {
      *pxdbmax = static_cast<float> (20.0 * std::log10 (pxmax));
    }

  state.k0 = k;
  state.ja += jstep;

  std::vector<std::complex<float>> spectrum (static_cast<std::size_t> (kSymspecNfft3 / 2 + 1));
  float* real = reinterpret_cast<float*> (spectrum.data ());
  std::fill_n (real, kSymspecNfft3 + 2, 0.0f);
  constexpr float fac0 = 0.1f;
  for (int i = 0; i < kSymspecNfft3; ++i)
    {
      int const j = state.ja + i - (kSymspecNfft3 - 1);
      if (j >= 1 && j <= max_samples)
        {
          real[i] = fac0 * shared_data->d2[j - 1];
        }
    }
  ++(*ihsym);

  if (low_sidelobes)
    {
      for (int i = 0; i < kSymspecNfft3; ++i)
        {
          real[i] *= state.window[static_cast<std::size_t> (i)];
        }
    }
  four2a_forward_real (spectrum, kSymspecNfft3);

  *df3 = static_cast<float> (12000.0 / kSymspecNfft3);
  int const iz = std::min (kSymspecMaxFreqBins, static_cast<int> (std::lround (5000.0 / *df3)));
  double const fac = std::pow (1.0 / kSymspecNfft3, 2);
  for (int i = 1; i <= iz; ++i)
    {
      int const j = i - 1;
      auto const value = spectrum[static_cast<std::size_t> (j)];
      float const sx = static_cast<float> (fac * (value.real () * value.real () + value.imag () * value.imag ()));
      if (*ihsym <= kSymspecMaxHalfSymbols)
        {
          shared_data->ss[(i - 1) * kSymspecMaxHalfSymbols + (*ihsym - 1)] = sx;
        }
      state.spectrum_sum[static_cast<std::size_t> (i - 1)] += sx;
      s[i - 1] = static_cast<float> (1000.0 * gain * sx);
      shared_data->savg[i - 1] = state.spectrum_sum[static_cast<std::size_t> (i - 1)] / *ihsym;
    }

  if ((*ihsym % 10) == 0)
    {
      static int const nch[7] {1, 2, 4, 9, 18, 36, 72};
      int const mode4 = nch[minw];
      int nsmo = std::min (10 * mode4, 150);
      nsmo = 4 * nsmo;
      flat1_relative_baseline (shared_data->savg, iz, nsmo, spectra_.syellow);
      if (mode4 >= 2)
        {
          smooth_sum_inplace (spectra_.syellow, iz, mode4);
          smooth_sum_inplace (spectra_.syellow, iz, mode4);
        }
      std::fill_n (spectra_.syellow, std::min (250, iz), 0.0f);
      int const ia = static_cast<int> (500.0 / *df3);
      int const ib = static_cast<int> (2700.0 / *df3);
      float const smin = *std::min_element (spectra_.syellow + std::max (0, ia - 1),
                                            spectra_.syellow + std::min (iz, ib));
      float const smax = *std::max_element (spectra_.syellow, spectra_.syellow + iz);
      if (smax > smin)
        {
          for (int i = 0; i < iz; ++i)
            {
              spectra_.syellow[i] = (50.0f / (smax - smin)) * (spectra_.syellow[i] - smin);
              if (spectra_.syellow[i] < 0.0f)
                {
                  spectra_.syellow[i] = 0.0f;
                }
            }
        }
    }

  *npts8 = k / 8;
}

void hspec_update (short const* id2, int k, int ntrpdepth, int ingain,
                   float* green, float* s, int* jh, float* pxmax, float* db_no_gain)
{
  if (!id2 || !green || !s || !jh || !pxmax || !db_no_gain)
    {
      return;
    }

  auto& state = hspec_state ();
  int const ndepth = ntrpdepth / 1000;
  int const ntrperiod = ntrpdepth - 1000 * ndepth;
  (void) ndepth;

  double const gain = std::pow (10.0, 0.1 * ingain);
  int nstep = kHspecNfft;
  int nblks = 7;
  if (ntrperiod < 30)
    {
      nstep = 256;
      nblks = 14;
    }

  if (k > 30 * 12000)
    {
      return;
    }
  if (k < kHspecNfft)
    {
      *jh = 0;
      return;
    }

  if (k < state.k0)
    {
      state.ja = -nstep;
      *jh = -1;
    }

  *pxmax = 0.0f;
  for (int iblk = 0; iblk < nblks; ++iblk)
    {
      if (*jh < kHspecColumns - 1)
        {
          ++(*jh);
        }
      state.ja += nstep;
      int const jb = state.ja + kHspecNfft - 1;
      std::vector<std::complex<float>> spectrum (static_cast<std::size_t> (kHspecNfft / 2 + 1));
      float* real = reinterpret_cast<float*> (spectrum.data ());
      for (int i = 0; i < kHspecNfft; ++i)
        {
          real[i] = id2[state.ja + i];
        }
      double const sq = std::inner_product (real, real + kHspecNfft, real, 0.0);
      float xmax = *std::max_element (real, real + kHspecNfft);
      float xmin = std::fabs (*std::min_element (real, real + kHspecNfft));
      if (xmin > xmax)
        {
          xmax = xmin;
        }
      if (xmax > 0.0f)
        {
          *pxmax = static_cast<float> (20.0 * std::log10 (xmax));
        }
      double const rms = std::sqrt (gain * sq / kHspecNfft);
      double const rms2 = std::sqrt (sq / kHspecNfft);
      green[*jh] = 0.0f;
      if (rms > 0.0)
        {
          green[*jh] = static_cast<float> (20.0 * std::log10 (rms));
          *db_no_gain = static_cast<float> (20.0 * std::log10 (rms2));
        }
      four2a_forward_real (spectrum, kHspecNfft);
      double const fac = std::pow (1.0 / kHspecNfft, 2);
      for (int i = 1; i <= 64; ++i)
        {
          int const j = 2 * i;
          auto const hi = spectrum[static_cast<std::size_t> (j)];
          auto const lo = spectrum[static_cast<std::size_t> (j - 1)];
          float const sx = static_cast<float> (
              fac * (hi.real () * hi.real () + hi.imag () * hi.imag ()
                   + lo.real () * lo.real () + lo.imag () * lo.imag ()));
          s[64 * (*jh) + (i - 1)] = static_cast<float> (gain * sx);
        }
      if (state.ja + 2 * kHspecNfft > k)
        {
          break;
        }
      (void) jb;
    }
  state.k0 = k;
}

void save_echo_params_inplace (int ndop_total, int ndop_audio, int nfrit, float f1, float fspread,
                               int tone_spacing, int const itone[6], short id2[15])
{
  if (!id2 || !itone)
    {
      return;
    }

  short header[15] {};
  std::int32_t ndop_total0 = ndop_total;
  std::int32_t ndop_audio0 = ndop_audio;
  std::int32_t nfrit0 = nfrit;
  float f10 = f1;
  float fspread0 = fspread;
  std::int32_t tone_spacing0 = tone_spacing;
  std::int8_t tone_bytes[6] {};

  for (int i = 0; i < 6; ++i)
    {
      tone_bytes[i] = static_cast<std::int8_t> (itone[i]);
    }

  std::memcpy (&header[0], &ndop_total0, sizeof (ndop_total0));
  std::memcpy (&header[2], &ndop_audio0, sizeof (ndop_audio0));
  std::memcpy (&header[4], &nfrit0, sizeof (nfrit0));
  std::memcpy (&header[6], &f10, sizeof (f10));
  std::memcpy (&header[8], &fspread0, sizeof (fspread0));
  std::memcpy (&header[10], &tone_spacing0, sizeof (tone_spacing0));
  std::memcpy (&header[12], tone_bytes, sizeof (tone_bytes));
  std::copy (std::begin (header), std::end (header), id2);
}

void load_echo_params (short const id2[15], int* ndop_total, int* ndop_audio, int* nfrit,
                       float* f1, float* fspread, int* tone_spacing, int itone[6])
{
  if (!id2 || !ndop_total || !ndop_audio || !nfrit || !f1 || !fspread || !tone_spacing || !itone)
    {
      return;
    }

  std::int32_t ndop_total0 = 0;
  std::int32_t ndop_audio0 = 0;
  std::int32_t nfrit0 = 0;
  float f10 = 0.0f;
  float fspread0 = 0.0f;
  std::int32_t tone_spacing0 = 0;
  std::int8_t tone_bytes[6] {};

  std::memcpy (&ndop_total0, &id2[0], sizeof (ndop_total0));
  std::memcpy (&ndop_audio0, &id2[2], sizeof (ndop_audio0));
  std::memcpy (&nfrit0, &id2[4], sizeof (nfrit0));
  std::memcpy (&f10, &id2[6], sizeof (f10));
  std::memcpy (&fspread0, &id2[8], sizeof (fspread0));
  std::memcpy (&tone_spacing0, &id2[10], sizeof (tone_spacing0));
  std::memcpy (tone_bytes, &id2[12], sizeof (tone_bytes));

  *ndop_total = ndop_total0;
  *ndop_audio = ndop_audio0;
  *nfrit = nfrit0;
  *f1 = f10;
  *fspread = fspread0;
  *tone_spacing = tone_spacing0;
  for (int i = 0; i < 6; ++i)
    {
      int value = tone_bytes[i];
      if (value < 0 || value > 36)
        {
          value = 0;
        }
      itone[i] = value;
    }
}

EchoAnalysisResult avecho_update (short const* id2_0, int ndop, int nfrit, int nauto, int ndf,
                                  int navg, float f1, float width, bool disk_data,
                                  bool echo_call, QString const& txcall)
{
  EchoAnalysisResult result;
  result.width = width;
  result.rxcall = QStringLiteral ("      ");

  if (!id2_0 || navg <= 0)
    {
      return result;
    }

  auto& state = avecho_state ();
  std::vector<short> original (static_cast<std::size_t> (kEchoTxSamples + kEchoNsps), 0);
  std::copy_n (id2_0, kEchoTxSamples + kEchoNsps, original.begin ());

  if (echo_call && !disk_data)
    {
      int ndop_total0 = 0;
      int ndop_audio0 = 0;
      int nfrit0 = 0;
      float f10 = 0.0f;
      float fspread0 = 0.0f;
      int tone_spacing0 = 0;
      int itone0[6] {};
      load_echo_params (original.data (), &ndop_total0, &ndop_audio0, &nfrit0, &f10, &fspread0,
                        &tone_spacing0, itone0);
      gen_echocall_impl (txcall, itone0);
      save_echo_params_inplace (ndop_total0, ndop_audio0, nfrit0, f10, fspread0, tone_spacing0,
                                itone0, original.data ());
    }

  float fspread = echocom2_.fspread_dx;
  if (disk_data)
    {
      fspread = width;
    }
  if (nauto == 1)
    {
      fspread = echocom2_.fspread_self;
    }
  fspread = std::min (std::max (0.04f, fspread), 700.0f);
  result.width = fspread;

  if (navg != state.navg0)
    {
      state.sax.assign (static_cast<std::size_t> (navg) * kEchoNz, 0.0f);
      state.sbx.assign (static_cast<std::size_t> (navg) * kEchoNz, 0.0f);
      echocom_.nsum = 0;
      state.navg0 = navg;
    }

  double sq = 0.0;
  for (int i = 15; i < kEchoTxSamples; ++i)
    {
      float const sample = original[static_cast<std::size_t> (i)];
      sq += sample * sample;
    }
  result.xlevel = static_cast<float> (10.0 * std::log10 (sq / (kEchoTxSamples - 15)));

  if (navg == 1)
    {
      echocom_.nclearave = 1;
    }
  if (echocom_.nclearave != 0)
    {
      echocom_.nsum = 0;
    }
  if (echocom_.nsum == 0)
    {
      state.dop0 = static_cast<float> (ndop);
      std::fill (state.sax.begin (), state.sax.end (), 0.0f);
      std::fill (state.sbx.begin (), state.sbx.end (), 0.0f);
    }

  float const df = 12000.0f / kEchoNfft;
  int const ia = static_cast<int> (std::lround ((1500.0f + state.dop0 - nfrit) / df));
  int const ib = static_cast<int> (std::lround ((f1 + ndop - nfrit) / df));
  result.sigdb = 0.0f;
  result.db_err = 0.0f;
  result.dfreq = 0.0f;

  if (ia < 2048 || ib < 2048 || ia > 6144 || ib > 6144)
    {
      std::this_thread::sleep_for (std::chrono::milliseconds (10));
      return result;
    }

  constexpr float dtime = 0.05f;
  int idtbest = 0;
  float snrbest = -9999.0f;
  int idt1 = -10;
  int idt2 = 10;
  std::array<float, kEchoNz> sa {};
  std::array<float, kEchoNz> sb {};

  for (;;)
    {
      bool const searching = idt1 != idt2;
      for (int idt = idt1; idt <= idt2; ++idt)
        {
          int const i0 = static_cast<int> (12000.0f * dtime * idt);
          std::vector<short> shifted (static_cast<std::size_t> (kEchoTxSamples + kEchoNsps), 0);
          std::copy_n (original.begin (), 15, shifted.begin ());
          for (int i = 15; i < kEchoTxSamples; ++i)
            {
              int const j = i + i0;
              if (j >= 15 && j < kEchoTxSamples)
                {
                  shifted[static_cast<std::size_t> (i)] = original[static_cast<std::size_t> (j)];
                }
            }

          QString rxcall;
          decode_echo_inplace (shifted.data (), searching, &rxcall);

          std::vector<float> real_buffer (static_cast<std::size_t> (kEchoNfft + 2), 0.0f);
          for (int i = 0; i < kEchoTxSamples; ++i)
            {
              real_buffer[static_cast<std::size_t> (i)] =
                  shifted[static_cast<std::size_t> (i)] / static_cast<float> (kEchoTxSamples);
            }
          four2a_forward_real_buffer (real_buffer.data (), kEchoNfft);
          auto* c = reinterpret_cast<std::complex<float>*> (real_buffer.data ());
          std::array<float, 8192> s {};
          for (int i = 1; i <= 8192; ++i)
            {
              auto const value = c[i];
              s[static_cast<std::size_t> (i - 1)] =
                  value.real () * value.real () + value.imag () * value.imag ();
            }

          if (searching)
            {
              for (int i = 0; i < kEchoNz; ++i)
                {
                  sa[static_cast<std::size_t> (i)] = s[static_cast<std::size_t> (ia + i - 2048)];
                  sb[static_cast<std::size_t> (i)] = s[static_cast<std::size_t> (ib + i - 2048)];
                }
            }
          else
            {
              ++echocom_.nsum;
              result.rxcall = rxcall;
            }

          float snr_detect = 0.0f;
          echo_snr_impl (sa.data (), sb.data (), fspread, echocom_.blue, echocom_.red,
                         &result.sigdb, &result.db_err, &result.dfreq, &snr_detect);

          if (searching && result.sigdb > snrbest
              && (ndf == 0 || result.dfreq <= 0.5f * ndf))
            {
              snrbest = result.sigdb;
              idtbest = idt;
              int const slot = echocom_.nsum % navg;
              for (int i = 0; i < kEchoNz; ++i)
                {
                  state.sax[static_cast<std::size_t> (slot * kEchoNz + i)] =
                      s[static_cast<std::size_t> (ia + i - 2048)];
                  state.sbx[static_cast<std::size_t> (slot * kEchoNz + i)] =
                      s[static_cast<std::size_t> (ib + i - 2048)];
                }
            }
        }

      if (!searching)
        {
          break;
        }
      idt1 = idtbest;
      idt2 = idtbest;
    }

  result.xdt = idtbest * dtime;
  for (int i = 0; i < kEchoNz; ++i)
    {
      double sum_a = 0.0;
      double sum_b = 0.0;
      for (int j = 0; j < navg; ++j)
        {
          sum_a += state.sax[static_cast<std::size_t> (j * kEchoNz + i)];
          sum_b += state.sbx[static_cast<std::size_t> (j * kEchoNz + i)];
        }
      sa[static_cast<std::size_t> (i)] = static_cast<float> (sum_a);
      sb[static_cast<std::size_t> (i)] = static_cast<float> (sum_b);
    }

  float snr_detect = 0.0f;
  echo_snr_impl (sa.data (), sb.data (), fspread, echocom_.blue, echocom_.red,
                 &result.sigdb, &result.db_err, &result.dfreq, &snr_detect);
  result.nqual = std::max (0, std::min (10, static_cast<int> (snr_detect) - 2));

  float redmax = 0.0f;
  for (float value : echocom_.red)
    {
      redmax = std::max (redmax, value);
    }
  float const fac = 10.0f / std::max (redmax, 10.0f);
  for (int i = 0; i < kEchoNz; ++i)
    {
      echocom_.blue[i] *= fac;
      echocom_.red[i] *= fac;
    }

  int const nsmo = std::max (0, static_cast<int> (0.25f * fspread / df));
  for (int i = 0; i < nsmo; ++i)
    {
      decodium::plot::smooth121_inplace (echocom_.red, kEchoNz);
      decodium::plot::smooth121_inplace (echocom_.blue, kEchoNz);
    }

  int ia1 = static_cast<int> (50.0f / df);
  int ib1 = static_cast<int> (250.0f / df);
  int npct = 50;
  int npts = ib1 - ia1 + 1;
  float bred1 = 0.0f;
  float bblue1 = 0.0f;
  pctile_ (echocom_.red + (ia1 - 1), &npts, &npct, &bred1);
  pctile_ (echocom_.blue + (ia1 - 1), &npts, &npct, &bblue1);

  ia1 = static_cast<int> (1250.0f / df);
  ib1 = static_cast<int> (1450.0f / df);
  npts = ib1 - ia1 + 1;
  float bred2 = 0.0f;
  float bblue2 = 0.0f;
  pctile_ (echocom_.red + (ia1 - 1), &npts, &npct, &bred2);
  pctile_ (echocom_.blue + (ia1 - 1), &npts, &npct, &bblue2);

  for (int i = 0; i < kEchoNz; ++i)
    {
      echocom_.red[i] -= 0.5f * (bred1 + bred2);
      echocom_.blue[i] -= 0.5f * (bblue1 + bblue2);
    }

  std::this_thread::sleep_for (std::chrono::milliseconds (10));
  if (std::fabs (result.dfreq) >= 1000.0f)
    {
      result.dfreq = 0.0f;
    }

  return result;
}

}
}
