#include "Detector/JT9WideDecoder.hpp"

#include "Detector/JT9FastDecoder.hpp"
#include "Detector/JT9FastHelpers.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "commons.h"

#include <QByteArray>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numeric>
#include <vector>

extern "C"
{
  void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform, int);
  void pctile_ (float x[], int* npts, int* npct, float* xpct);
}

namespace
{

constexpr int kWideRows {184};
constexpr int kWideNfft {6912};
constexpr int kWideNh {kWideNfft / 2};
constexpr int kWideNq {kWideNh / 2};
constexpr int kWideLimit {10000};

constexpr std::array<int, 16> kJt9SyncHalfSymbols {{
  1, 3, 9, 19, 31, 45, 65, 69, 101, 103, 109, 119, 131, 145, 165, 169
}};

constexpr std::array<int, 16> kJt9SyncSymbols {{
  1, 2, 5, 10, 16, 23, 33, 35, 51, 52, 55, 60, 66, 73, 83, 85
}};

constexpr std::array<int, 85> kJt9Isync {{
  1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,
  0,0,1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,1,
  0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
  0,0,1,0,1
}};

inline std::size_t ss_index (int half_symbol_1based, int freq_bin_1based)
{
  return static_cast<std::size_t> ((freq_bin_1based - 1) * kWideRows + (half_symbol_1based - 1));
}

inline std::size_t ss2_index (int tone, int symbol_1based)
{
  return static_cast<std::size_t> (tone + 9 * (symbol_1based - 1));
}

inline std::size_t ss3_index (int tone, int symbol_1based)
{
  return static_cast<std::size_t> (tone + 8 * (symbol_1based - 1));
}

QByteArray trim_trailing_spaces (QByteArray bytes)
{
  while (!bytes.isEmpty ())
    {
      char const c = bytes.back ();
      if (c == ' ' || c == '\0' || c == '\r' || c == '\n' || c == '\t')
        {
          bytes.chop (1);
          continue;
        }
      break;
    }
  return bytes;
}

void four2a_forward_real_buffer (float* buffer, int nfft)
{
  int ndim = 1;
  int isign = -1;
  int iform = 0;
  four2a_ (reinterpret_cast<std::complex<float>*> (buffer), &nfft, &ndim, &isign, &iform, 0);
}

float percentile_value (float const* values, int count, int pct)
{
  if (!values || count <= 0 || pct < 0 || pct > 100)
    {
      return 1.0f;
    }

  std::vector<float> work (values, values + count);
  float xpct = 1.0f;
  int npts = count;
  int npct = pct;
  pctile_ (work.data (), &npts, &npct, &xpct);
  return xpct;
}

float db_value (float ratio)
{
  if (!(ratio > 0.0f))
    {
      return -99.0f;
    }
  return static_cast<float> (10.0 * std::log10 (ratio));
}

bool nearly_same_chisq (float lhs, float rhs)
{
  float const scale = std::max ({1.0f, std::fabs (lhs), std::fabs (rhs)});
  return std::fabs (lhs - rhs) <= 1.0e-6f * scale;
}

void smo_fortran_like (std::vector<float>& x, std::vector<float>& y, int nadd)
{
  int const npts = static_cast<int> (x.size ());
  int const nh = nadd / 2;
  if (y.size () != x.size ())
    {
      y.assign (x.size (), 0.0f);
    }
  for (int i = nh; i < npts - nh; ++i)
    {
      float sum = 0.0f;
      for (int j = -nh; j <= nh; ++j)
        {
          sum += x[static_cast<std::size_t> (i + j)];
        }
      y[static_cast<std::size_t> (i)] = sum;
    }
  x = y;
  for (int i = 0; i < std::min (nh, npts); ++i)
    {
      x[static_cast<std::size_t> (i)] = 0.0f;
    }
  for (int i = std::max (0, npts - nh); i < npts; ++i)
    {
      x[static_cast<std::size_t> (i)] = 0.0f;
    }
}

float fchisq0 (float const* y, int npts, std::array<float, 5> const& a)
{
  float chisq = 0.0f;
  float const width = std::max (a[3], 0.001f);
  for (int i = 0; i < npts; ++i)
    {
      float const x = static_cast<float> (i + 1);
      float const z = (x - a[2]) / (0.5f * width);
      float yfit = a[0];
      if (std::fabs (z) < 3.0f)
        {
          float const d = 1.0f + z * z;
          yfit = a[0] + a[1] * (1.0f / d - 0.1f);
        }
      float const diff = y[i] - yfit;
      chisq += diff * diff;
    }
  return chisq;
}

void lorentzian_fit (float const* y, int npts, std::array<float, 5>& a)
{
  a.fill (0.0f);
  if (!y || npts <= 0)
    {
      return;
    }

  int ipk = 0;
  float ymax = -1.0e30f;
  for (int i = 0; i < npts; ++i)
    {
      if (y[i] > ymax)
        {
          ymax = y[i];
          ipk = i;
        }
    }

  int const edge_count = std::min (20, npts);
  float base = 0.0f;
  for (int i = 0; i < edge_count; ++i)
    {
      base += y[i];
      base += y[npts - edge_count + i];
    }
  base /= std::max (1, 2 * edge_count);

  float const stest = ymax - 0.5f * (ymax - base);
  float ssum = y[ipk];
  for (int i = 1; i <= 50; ++i)
    {
      if (ipk + i >= npts || y[ipk + i] < stest)
        {
          break;
        }
      ssum += y[ipk + i];
    }
  for (int i = 1; i <= 50; ++i)
    {
      if (ipk - i < 0 || y[ipk - i] < stest)
        {
          break;
        }
      ssum += y[ipk - i];
    }

  float const peak = std::max (y[ipk], 1.0e-6f);
  float const ww = ssum / peak;
  float width = 2.0f;
  float const t = ww * ww - 5.67f;
  if (t > 0.0f)
    {
      width = std::sqrt (t);
    }

  a[0] = base;
  a[1] = ymax - base;
  a[2] = static_cast<float> (ipk + 1);
  a[3] = width;

  std::array<float, 4> deltaa {{0.1f, 0.1f, 1.0f, 1.0f}};
  float chisqr0 = 1.0e6f;
  for (int iter = 0; iter < 5; ++iter)
    {
      for (int j = 0; j < 4; ++j)
        {
          constexpr int kLorentzianSeekLimit {256};
          constexpr int kLorentzianSlideLimit {4096};
          float chisq1 = fchisq0 (y, npts, a);
          float fn = 0.0f;
          float delta = deltaa[static_cast<std::size_t> (j)];
          float const original = a[static_cast<std::size_t> (j)];
          bool advanced = false;

          for (int seek = 0; seek < kLorentzianSeekLimit; ++seek)
            {
              a[static_cast<std::size_t> (j)] += delta;
              float const chisq2_try = fchisq0 (y, npts, a);
              if (nearly_same_chisq (chisq2_try, chisq1))
                {
                  continue;
                }

              float chisq2 = chisq2_try;
              if (chisq2 > chisq1)
                {
                  delta = -delta;
                  a[static_cast<std::size_t> (j)] += delta;
                  std::swap (chisq1, chisq2);
                }

              float chisq3 = chisq2;
              for (int slide = 0; slide < kLorentzianSlideLimit; ++slide)
                {
                  fn += 1.0f;
                  a[static_cast<std::size_t> (j)] += delta;
                  chisq3 = fchisq0 (y, npts, a);
                  if (chisq3 < chisq2)
                    {
                      chisq1 = chisq2;
                      chisq2 = chisq3;
                        continue;
                    }
                  break;
                }

              float const denom = chisq3 - chisq2;
              if (std::fabs (denom) > 1.0e-12f)
                {
                  delta *= (1.0f / (1.0f + (chisq1 - chisq2) / denom) + 0.5f);
                }
              a[static_cast<std::size_t> (j)] -= delta;
              deltaa[static_cast<std::size_t> (j)] = deltaa[static_cast<std::size_t> (j)] * fn / 3.0f;
              advanced = true;
              break;
            }
          if (!advanced)
            {
              a[static_cast<std::size_t> (j)] = original;
              deltaa[static_cast<std::size_t> (j)] *= 0.5f;
            }
        }

      float const chisqr = fchisq0 (y, npts, a);
      a[4] = chisqr;
      if (chisqr0 > 0.0f && chisqr / chisqr0 > 0.99f)
        {
          break;
        }
      chisqr0 = chisqr;
    }
}

void sync9w_pass (std::vector<float>& ss, int nzhsym, int lag1, int lag2, int ia, int ib, int nadd,
                  std::vector<float>& ccfred, std::array<float, 28>& ccfblue, int& lagpk)
{
  for (int j = 1; j <= nzhsym; ++j)
    {
      std::vector<float> sa (NSMAX, 0.0f);
      std::vector<float> sb (NSMAX, 0.0f);
      for (int i = 1; i <= NSMAX; ++i)
        {
          sa[static_cast<std::size_t> (i - 1)] = ss[ss_index (j, i)];
        }
      smo_fortran_like (sa, sb, nadd);
      smo_fortran_like (sb, sa, nadd);
      for (int i = 1; i <= NSMAX; ++i)
        {
          ss[ss_index (j, i)] = sa[static_cast<std::size_t> (i - 1)];
        }
    }

  std::vector<float> ss1save (static_cast<std::size_t> (nzhsym), 0.0f);
  float sbest = 0.0f;
  ccfred.assign (static_cast<std::size_t> (NSMAX + 1), 0.0f);

  for (int i = ia; i <= ib; ++i)
    {
      std::vector<float> ss1 (static_cast<std::size_t> (nzhsym));
      for (int j = 1; j <= nzhsym; ++j)
        {
          ss1[static_cast<std::size_t> (j - 1)] = ss[ss_index (j, i)];
        }

      float xmed = percentile_value (ss1.data (), nzhsym, 50);
      if (!(xmed > 0.0f))
        {
          xmed = 1.0f;
        }
      for (float& value : ss1)
        {
          value = value / xmed - 1.0f;
        }

      float smax = 0.0f;
      for (int lag = lag1; lag <= lag2; ++lag)
        {
          float sum1 = 0.0f;
          for (int j = 0; j < 16; ++j)
            {
              int const k = kJt9SyncHalfSymbols[static_cast<std::size_t> (j)] + lag;
              if (k >= 1 && k <= nzhsym)
                {
                  sum1 += ss1[static_cast<std::size_t> (k - 1)];
                }
            }
          if (sum1 > smax)
            {
              smax = sum1;
            }
        }

      ccfred[static_cast<std::size_t> (i)] = smax;
      if (smax > sbest)
        {
          sbest = smax;
          ss1save = ss1;
        }
    }

  float xmed = percentile_value (ccfred.data () + ia, ib - ia + 1, 50);
  if (!(xmed > 0.0f))
    {
      xmed = 1.0f;
    }
  for (float& value : ccfred)
    {
      value /= xmed;
    }

  float smax = 0.0f;
  lagpk = lag1;
  ccfblue.fill (0.0f);
  for (int lag = lag1; lag <= lag2; ++lag)
    {
      float sum1 = 0.0f;
      for (int j = 0; j < 16; ++j)
        {
          int const k = kJt9SyncHalfSymbols[static_cast<std::size_t> (j)] + lag;
          if (k >= 1 && k <= nzhsym)
            {
              sum1 += ss1save[static_cast<std::size_t> (k - 1)];
            }
        }
      ccfblue[static_cast<std::size_t> (lag + 9)] = sum1;
      if (sum1 > smax)
        {
          smax = sum1;
          lagpk = lag;
        }
    }

  if (lagpk == -9)
    {
      lagpk = -8;
    }
  if (lagpk == 18)
    {
      lagpk = 17;
    }
}

decodium::jt9wide::detail::SoftsymResult softsym9w_compute (QVector<short> const& audio, float xdt0,
                                                            float f0, float width, int nsubmode)
{
  decodium::jt9wide::detail::SoftsymResult result;
  int const npts = std::min (52 * RX_SAMPLE_RATE, audio.size ());
  if (npts < kWideNfft)
    {
      return result;
    }

  float const df = static_cast<float> (RX_SAMPLE_RATE) / kWideNfft;
  int const i0a = std::max (1, static_cast<int> ((xdt0 - 1.0f) * RX_SAMPLE_RATE));
  int const i0b = static_cast<int> ((xdt0 + 1.0f) * RX_SAMPLE_RATE);
  int const k1 = std::max (1, static_cast<int> (std::lround ((f0 - 0.5f * width) / df)));
  int const k2 = std::min (kWideNq, static_cast<int> (std::lround ((f0 + 0.5f * width) / df)));
  if (k1 > k2)
    {
      return result;
    }

  std::array<float, 9 * 85> ss2 {};
  std::array<float, 8 * 69> ss3 {};
  std::vector<float> power (static_cast<std::size_t> (kWideNq), 0.0f);
  std::vector<float> real (static_cast<std::size_t> (kWideNfft + 2), 0.0f);
  auto const* spectrum = reinterpret_cast<std::complex<float> const*> (real.data ());

  auto accumulate_fft_power = [&audio, &real, spectrum] (int start_1based, std::vector<float>& out_power) -> bool
  {
    int const start = start_1based - 1;
    if (start < 0 || start + kWideNfft > audio.size ())
      {
        return false;
      }
    std::fill (real.begin (), real.end (), 0.0f);
    for (int i = 0; i < kWideNfft; ++i)
      {
        real[static_cast<std::size_t> (i)] = 1.0e-6f * audio.at (start + i);
      }
    four2a_forward_real_buffer (real.data (), kWideNfft);
    for (int k = 1; k <= kWideNq; ++k)
      {
        auto const value = spectrum[k];
        out_power[static_cast<std::size_t> (k - 1)] =
            value.real () * value.real () + value.imag () * value.imag ();
      }
    return true;
  };

  float smax = 0.0f;
  int i0pk = 1;
  for (int i0 = i0a; i0 <= i0b; i0 += 432)
    {
      std::fill (power.begin (), power.end (), 0.0f);
      for (int j = 0; j < 16; ++j)
        {
          int const ia = i0 + (kJt9SyncSymbols[static_cast<std::size_t> (j)] - 1) * kWideNfft;
          std::vector<float> sync_power (static_cast<std::size_t> (kWideNq), 0.0f);
          if (!accumulate_fft_power (ia, sync_power))
            {
              return result;
            }
          for (int k = 0; k < kWideNq; ++k)
            {
              power[static_cast<std::size_t> (k)] += sync_power[static_cast<std::size_t> (k)];
            }
        }

      float ssum = 0.0f;
      for (int k = k1; k <= k2; ++k)
        {
          ssum += power[static_cast<std::size_t> (k - 1)];
        }
      if (ssum > smax)
        {
          smax = ssum;
          i0pk = i0;
        }
      else if (ssum < 0.7f * smax)
        {
          break;
        }
    }

  result.xdt1 = static_cast<float> (i0pk - 1) / RX_SAMPLE_RATE;
  if (i0pk <= 0)
    {
      return result;
    }

  int m = 0;
  float const dtone = df * static_cast<float> (1 << nsubmode);
  std::vector<float> s2_flat;
  s2_flat.reserve (9 * 85);
  for (int j = 1; j <= 85; ++j)
    {
      int const ia = i0pk + (j - 1) * kWideNfft;
      if (!accumulate_fft_power (ia, power))
        {
          return result;
        }

      for (int i = 0; i <= 8; ++i)
        {
          float const f = f0 + i * dtone;
          int const kk1 = std::max (1, static_cast<int> (std::lround ((f - 0.5f * width) / df)));
          int const kk2 = std::min (kWideNq, static_cast<int> (std::lround ((f + 0.5f * width) / df)));
          float sum = 0.0f;
          for (int k = kk1; k <= kk2; ++k)
            {
              sum += power[static_cast<std::size_t> (k - 1)];
            }
          ss2[ss2_index (i, j)] = sum;
          s2_flat.push_back (sum);
        }

      if (kJt9Isync[static_cast<std::size_t> (j - 1)] == 0)
        {
          ++m;
          for (int i = 0; i <= 7; ++i)
            {
              ss3[ss3_index (i, m)] = ss2[ss2_index (i + 1, j)];
            }
        }
    }

  float ss = 0.0f;
  float sig = 0.0f;
  for (int j = 1; j <= 69; ++j)
    {
      float row_max = 0.0f;
      for (int i = 0; i <= 7; ++i)
        {
          float const value = ss3[ss3_index (i, j)];
          row_max = std::max (row_max, value);
          ss += value;
        }
      sig += row_max;
      ss -= row_max;
    }
  float const ave = ss / (69.0f * 7.0f);
  float const xmed = percentile_value (s2_flat.data (), static_cast<int> (s2_flat.size ()), 35);
  sig /= 69.0f;
  result.nsnr = static_cast<int> (std::lround (db_value (sig / std::max (xmed, 1.0e-12f)) - 28.0f));

  if (!(ave > 0.0f) || !(xmed > 0.0f))
    {
      return result;
    }

  decodium::jt9fast::softsym9f_compute (ss2, ss3, result.soft);
  result.ok = true;
  return result;
}

QString format_jt9_row (int nutc, int nsnr, float dt, float freq, QByteArray const& msg)
{
  QByteArray const fixed = msg.leftJustified (22, ' ', true).left (22);
  char prefix[64] {};
  std::snprintf (prefix, sizeof prefix, "%04d%4d%5.1f%5d @  ",
                 nutc, nsnr, double (dt), int (std::lround (freq)));
  QByteArray row {prefix};
  row.append (fixed);
  return QString::fromLatin1 (trim_trailing_spaces (row));
}

}

namespace decodium
{
namespace jt9wide
{

namespace detail
{

SyncPassResult run_sync_pass (QVector<float> const& ss_in, int nzhsym, int lag1, int lag2, int ia, int ib, int nadd)
{
  SyncPassResult result;
  if (ss_in.size () < kWideRows * NSMAX || nzhsym <= 0 || ia > ib)
    {
      return result;
    }

  std::vector<float> ss (ss_in.constBegin (), ss_in.constEnd ());
  result.ccfred.assign (static_cast<std::size_t> (NSMAX + 1), 0.0f);
  sync9w_pass (ss, nzhsym, lag1, lag2, ia, ib, nadd, result.ccfred, result.ccfblue, result.lagpk);
  return result;
}

DetectionEstimate estimate_wide_jt9 (legacyjt::DecodeRequest const& request)
{
  DetectionEstimate estimate;
  if (request.mode != "JT9" || request.nsubmode < 1 || request.ss.size () < kWideRows * NSMAX
      || request.audio.isEmpty ())
    {
      return estimate;
    }

  float const df = static_cast<float> (RX_SAMPLE_RATE) / 16384.0f;
  int const nzhsym = kWideRows;
  int const ia = std::max (1, static_cast<int> (std::lround ((request.nfqso - request.ntol) / df)));
  int const ib = std::min (NSMAX, static_cast<int> (std::lround ((request.nfqso + request.ntol) / df)));
  if (ia > ib)
    {
      return estimate;
    }

  int const lag1 = -static_cast<int> (2.5 / (0.5 * kWideNfft / static_cast<double> (RX_SAMPLE_RATE)) + 0.9999);
  int const lag2 = static_cast<int> (5.0 / (0.5 * kWideNfft / static_cast<double> (RX_SAMPLE_RATE)) + 0.9999);

  std::vector<float> ss (request.ss.constBegin (), request.ss.constEnd ());
  std::array<float, 5> a {};
  estimate.ccfred.assign (static_cast<std::size_t> (NSMAX + 1), 0.0f);

  for (int iter = 0; iter < 2; ++iter)
    {
      int nadd = 3;
      if (iter == 1)
        {
          nadd = 2 * static_cast<int> (std::lround (0.375f * a[3])) + 1;
          nadd = std::max (1, nadd);
        }

      std::array<float, 28> ccfblue {};
      int lagpk = 0;
      sync9w_pass (ss, nzhsym, lag1, lag2, ia, ib, nadd, estimate.ccfred, ccfblue, lagpk);

      double s = 0.0;
      double sq = 0.0;
      int ns = 0;
      for (int lag = -9; lag <= 18; ++lag)
        {
          if (std::abs (lag - lagpk) > 3)
            {
              float const value = ccfblue[static_cast<std::size_t> (lag + 9)];
              s += value;
              sq += value * value;
              ++ns;
            }
        }
      estimate.base = ns > 0 ? static_cast<float> (s / ns) : 0.0f;
      float const variance = ns > 0 ? static_cast<float> (sq / ns - estimate.base * estimate.base) : 0.0f;
      estimate.rms = variance > 0.0f ? std::sqrt (variance) : 1.0f;
      estimate.sync =
          (ccfblue[static_cast<std::size_t> (lagpk + 9)] - estimate.base) / estimate.rms;
      estimate.xdt0 = static_cast<float> (lagpk * (0.5 * kWideNfft / static_cast<double> (RX_SAMPLE_RATE)));
      estimate.ccfblue = ccfblue;
      estimate.lagpk = lagpk;

      lorentzian_fit (estimate.ccfred.data () + ia, ib - ia + 1, a);
      estimate.f0 = (ia + a[2]) * df;
      estimate.width = a[3] * df;
    }

  estimate.ok = true;
  return estimate;
}

SoftsymResult compute_softsym (QVector<short> const& audio, float xdt0, float f0, float width, int nsubmode)
{
  return softsym9w_compute (audio, xdt0, f0, width, nsubmode);
}

}

QStringList decode_wide_jt9 (legacyjt::DecodeRequest const& request)
{
  QStringList rows;
  detail::DetectionEstimate const estimate = detail::estimate_wide_jt9 (request);
  if (!estimate.ok)
    {
      return rows;
    }

  detail::SoftsymResult const soft =
      detail::compute_softsym (request.audio, estimate.xdt0, estimate.f0, estimate.width, request.nsubmode);
  if (!soft.ok)
    {
      return rows;
    }

  int nlim = 0;
  QByteArray const decoded = decodium::jt9fast::decode_soft_symbols (soft.soft, kWideLimit, &nlim);
  rows << format_jt9_row (request.nutc, soft.nsnr, soft.xdt1, estimate.f0, decoded);
  (void) nlim;
  return rows;
}

}
}
