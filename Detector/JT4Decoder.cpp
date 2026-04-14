// JT4 RX decoder - C++ implementation replacing Fortran legacy_jt_async_decode path
#include "Detector/JT4Decoder.hpp"
#include "Detector/FanoSequentialDecoder.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "Modulator/LegacyJtEncoder.hpp"
#include "wsjtx_config.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <string>
#include <vector>

#include <fftw3.h>

namespace
{

// nch(1..7): channel widths for submodes A-G
constexpr std::array<int, 7> kJt4Nch {{1, 2, 4, 9, 18, 36, 72}};

constexpr int kMaxSamples12k = 52 * 12000;  // 624000 input samples
constexpr int kNsymMax = decodium::jt4::AverageState::kNsymMax;  // 207
constexpr int kNsubMax = decodium::jt4::AverageState::kNsubMax;  // 7
constexpr int kMaxAve  = decodium::jt4::AverageState::kMaxAve;   // 64

// sync4 spectrum constants
constexpr int kSync4Nfft  = 2520;
constexpr int kSync4Nh    = kSync4Nfft / 2;   // 1260  (NHMAX)
constexpr int kSync4Nq    = kSync4Nfft / 4;   // 630
// constexpr int kSync4Nsmax = 525;            // max time steps (NSMAX) — reserved

// Pseudo-random sequence from xcor4.f90 / kJt4SyncPattern in LegacyJtEncoder.hpp
constexpr std::array<int, 207> kNpr2 {{
  0,0,0,0,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,0,0,0,0,0,0,0,1,1,0,0,
  0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,1,1,0,1,0,1,1,1,1,1,0,1,0,0,0,
  1,0,0,1,0,0,1,1,1,1,1,0,0,0,1,0,1,0,0,0,1,1,1,1,0,1,1,0,0,1,
  0,0,0,1,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,0,1,0,1,
  0,1,1,1,0,0,1,0,1,1,0,1,1,1,1,0,0,0,0,1,1,0,1,1,0,0,0,1,1,1,
  0,1,1,1,0,1,1,1,0,0,1,0,0,0,1,1,0,1,1,0,0,1,0,0,0,1,1,1,1,1,
  1,0,0,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,0,1,1,1,1,0,1,0,1
}};

// Fortran column-major index for sym(i, j): i and j are 1-based
inline std::size_t sym_idx (int i, int j)
{
  return static_cast<std::size_t> ((i - 1) + kNsymMax * (j - 1));
}

// Fortran column-major index for avg_ppsave(i, j, k): all 1-based
inline std::size_t ppsave_idx (int i, int j, int k)
{
  return static_cast<std::size_t> ((i - 1) + kNsymMax * (j - 1)
                                   + kNsymMax * kNsubMax * (k - 1));
}

QByteArray blank_message ()
{
  return QByteArray (22, ' ');
}

float db_value (float x)
{
  if (x <= 1.259e-10f)
    return -99.0f;
  return 10.0f * std::log10 (x);
}

// ── FFTW helpers ─────────────────────────────────────────────────────────────

// In-place real-to-complex forward FFT.  buf must be ≥ nfft+2 floats.
void fftw_r2c_inplace (float* buf, int nfft)
{
  auto* cx = reinterpret_cast<fftwf_complex*> (buf);
  fftwf_plan p = fftwf_plan_dft_r2c_1d (nfft, buf, cx, FFTW_ESTIMATE);
  fftwf_execute (p);
  fftwf_destroy_plan (p);
}

// In-place complex-to-real inverse FFT of size n2, reading from cx (which must
// be the start of a buffer ≥ n2+2 floats).  FFTW does NOT normalise.
void fftw_c2r_inplace (fftwf_complex* cx, float* out_buf, int n2)
{
  fftwf_plan p = fftwf_plan_dft_c2r_1d (n2, cx, out_buf, FFTW_ESTIMATE);
  fftwf_execute (p);
  fftwf_destroy_plan (p);
}

// Forward r2c for a pre-allocated buffer (used in compute_jt4_snr and ps4).
void fftw_forward_real_buffer (float* buf, int nfft)
{
  fftw_r2c_inplace (buf, nfft);
}

// ── Trivial DSP helpers ───────────────────────────────────────────────────────

// Port of averms.f90: mean/rms excluding nskip samples around the peak.
void averms_cpp (float const* x, int n, int nskip, float& ave, float& rms)
{
  int ipk = 0;
  for (int i = 1; i < n; ++i)
    if (x[i] > x[ipk])
      ipk = i;

  float s = 0.0f, sq = 0.0f;
  int ns = 0;
  for (int i = 0; i < n; ++i)
    {
      if (nskip < 0 || std::abs (i - ipk) > nskip)
        {
          s += x[i];
          sq += x[i] * x[i];
          ++ns;
        }
    }
  ave = s / ns;
  rms = std::sqrt (sq / ns - ave * ave);
}

// Port of smo121.f90: 1-2-1 smoother in-place.
void smo121_cpp (float* x, int nz)
{
  float x0 = x[0];
  for (int i = 1; i < nz - 1; ++i)
    {
      float x1 = x[i];
      x[i] = 0.5f * x[i] + 0.25f * (x0 + x[i + 1]);
      x0 = x1;
    }
}

// Port of pctile.f90: p-th percentile of x[0..n-1].
float pctile_cpp (float const* x, int n, int p)
{
  if (n <= 0 || p < 0 || p > 100)
    return 1.0f;
  std::vector<float> tmp (x, x + n);
  std::sort (tmp.begin (), tmp.end ());
  int j = static_cast<int> (std::lround (n * 0.01 * p));
  if (j < 1)
    j = 1;
  if (j > n)
    j = n;
  return tmp[static_cast<std::size_t> (j - 1)];
}

// Port of slope.f90: remove best-fit slope from y[0..npts-1], ignoring ±4 bins
// around xpk (1-based position).  Array is then divided by residual rms.
void slope_cpp (float* y, int npts, float xpk)
{
  float sumw = 0, sumx = 0, sumy = 0, sumx2 = 0, sumxy = 0;
  for (int i = 1; i <= npts; ++i)
    {
      if (std::fabs (static_cast<float> (i) - xpk) > 4.0f)
        {
          float const xf = static_cast<float> (i);
          sumw  += 1.0f;
          sumx  += xf;
          sumy  += y[i - 1];
          sumx2 += xf * xf;
          sumxy += xf * y[i - 1];
        }
    }
  if (sumw < 5.0f)
    return;
  float const delta = sumw * sumx2 - sumx * sumx;
  if (std::fabs (delta) < 1.0e-12f)
    return;
  float const a = (sumx2 * sumy - sumx * sumxy) / delta;
  float const b = (sumw * sumxy - sumx * sumy) / delta;

  float sq = 0.0f;
  for (int i = 1; i <= npts; ++i)
    {
      y[i - 1] -= (a + b * static_cast<float> (i));
      if (std::fabs (static_cast<float> (i) - xpk) > 4.0f)
        sq += y[i - 1] * y[i - 1];
    }
  float const rms = std::sqrt (sq / (sumw - 4.0f));
  if (rms > 0.0f)
    for (int i = 0; i < npts; ++i)
      y[i] /= rms;
}

// Port of peakup.f90: sub-sample peak interpolation via parabola.
float peakup_cpp (float ym, float y0, float yp)
{
  float const b = yp - ym;
  float const c = yp + ym - 2.0f * y0;
  if (std::fabs (c) < 1.0e-12f)
    return 0.0f;
  return -b / (2.0f * c);
}

// ── Resampling (wav11 / lpf1) ─────────────────────────────────────────────────

// Port of wav11.f90: resample 12000 Hz int16 → 11025 Hz float.
// Writes npts_out resampled samples to dd.
void wav11_cpp (short const* d2, int npts, float* dd, int& npts_out)
{
  constexpr int NFFT1 = 64 * 12000;  // 768000
  constexpr int NFFT2 = 64 * 11025;  // 705600
  // Cutoff: ia = 5000 / (12000/NFFT1) = 5000 * NFFT1 / 12000 = 320000
  constexpr int ia   = 320000;
  constexpr float fac = 1.0e-6f;

  int const jz = std::min (60 * 12000, npts);  // NZ12 limit

  // One large buffer: NFFT1+2 floats covers both r2c (NFFT1) and c2r (NFFT2).
  std::vector<float> buf (static_cast<std::size_t> (NFFT1 + 2), 0.0f);
  for (int i = 0; i < jz; ++i)
    buf[static_cast<std::size_t> (i)] = static_cast<float> (d2[i]);

  auto* cx = reinterpret_cast<fftwf_complex*> (buf.data ());

  // Forward r2c, size NFFT1 (in-place)
  fftw_r2c_inplace (buf.data (), NFFT1);

  // Zero above ia
  for (int i = ia; i <= NFFT1 / 2; ++i)
    cx[i][0] = cx[i][1] = 0.0f;

  // Inverse c2r, size NFFT2 (reads first NFFT2/2+1 complex values; in-place)
  fftw_c2r_inplace (cx, buf.data (), NFFT2);

  npts_out = static_cast<int> (static_cast<long long> (jz) * 11025 / 12000);
  for (int i = 0; i < npts_out; ++i)
    dd[i] = fac * buf[static_cast<std::size_t> (i)];
}

// Port of lpf1.f90: lowpass filter + decimate by 2 (11025 → 5512.5 Hz).
void lpf1_cpp (float const* dd, int jz, float* dat, int& jz2)
{
  constexpr int NFFT1 = 64 * 11025;  // 705600
  constexpr int NFFT2 = 32 * 11025;  // 352800
  constexpr int cutoff = NFFT2 / 2;  // 176400

  float const fac = 1.0f / static_cast<float> (NFFT1);

  std::vector<float> buf (static_cast<std::size_t> (NFFT1 + 2), 0.0f);
  int const copy = std::min (jz, NFFT1);
  for (int i = 0; i < copy; ++i)
    buf[static_cast<std::size_t> (i)] = fac * dd[i];

  auto* cx = reinterpret_cast<fftwf_complex*> (buf.data ());

  // Forward r2c, size NFFT1 (in-place)
  fftw_r2c_inplace (buf.data (), NFFT1);

  // Zero at and above NFFT2/2
  for (int i = cutoff; i <= NFFT1 / 2; ++i)
    cx[i][0] = cx[i][1] = 0.0f;

  // Inverse c2r, size NFFT2 (in-place)
  fftw_c2r_inplace (cx, buf.data (), NFFT2);

  jz2 = jz / 2;
  for (int i = 0; i < jz2; ++i)
    dat[i] = buf[static_cast<std::size_t> (i)];
}

// ── sync4 helper: power spectrum ─────────────────────────────────────────────

// Port of ps4.f90: half-symbol power spectrum.
// Fills s[0..nh-1] where nh = nfft/2.
// Uses a pre-planned FFTW plan (plan must be for size nfft, in-place r2c on buf).
void ps4_cpp (float const* dat, int nh, fftwf_plan plan, float* buf_work, float* s)
{
  int const nfft = 2 * nh;
  std::fill (buf_work, buf_work + nfft + 2, 0.0f);
  for (int i = 0; i < nh; ++i)
    buf_work[i] = dat[i] / 128.0f;

  fftwf_execute_dft_r2c (plan,
                          buf_work,
                          reinterpret_cast<fftwf_complex*> (buf_work));

  auto const* cx = reinterpret_cast<fftwf_complex const*> (buf_work);
  float const fac = 1.0f / static_cast<float> (nfft);
  // Fortran s(i) for i=1..nh → cx[i] (1-based); C++ s[i-1] = fac*|cx[i]|^2
  for (int i = 0; i < nh; ++i)
    {
      float const re = cx[i + 1][0];
      float const im = cx[i + 1][1];
      s[i] = fac * (re * re + im * im);
    }
}

// ── sync4 helper: cross-correlation ──────────────────────────────────────────

// Port of xcor4.f90.
// s2: flat array [nh * nsteps], column-major: s2[(freq-1) + nh*(step-1)]
// ccf_out[0..nlag-1]: CCF values for lag = lag1..lag2
// Returns: ccf0 (peak), lagpk (lag of peak), flip (+1/-1)
void xcor4_cpp (float const* s2, int ipk, int nsteps, int nsym,
                int lag1, int lag2, int ich, int mode4, int nh,
                float* ccf_out, float& ccf0, int& lagpk, float& flip)
{
  int const nw = kJt4Nch[static_cast<std::size_t> (std::min (std::max (ich, 1), 7) - 1)];
  int const n  = 2 * mode4;
  int const nlag = lag2 - lag1 + 1;

  // Build differential array a[1..nsteps]
  std::vector<float> a (static_cast<std::size_t> (nsteps + 1), 0.0f);

  auto s2_at = [&] (int freq_1based, int step_1based) -> float
  {
    int const fi = freq_1based - 1;
    if (fi < 0 || fi >= nh || step_1based < 1 || step_1based > nsteps)
      return 0.0f;
    return s2[static_cast<std::size_t> (fi + nh * (step_1based - 1))];
  };

  for (int j = 1; j <= nsteps; ++j)
    {
      if (mode4 == 1)
        {
          a[static_cast<std::size_t> (j)] =
              std::max (s2_at (ipk + n, j), s2_at (ipk + 3 * n, j))
              - std::max (s2_at (ipk, j), s2_at (ipk + 2 * n, j));
        }
      else
        {
          int const kz = std::max (1, nw / 2);
          float ss0 = 0, ss1 = 0, ss2 = 0, ss3 = 0, wsum = 0;
          for (int k = -kz + 1; k <= kz - 1; ++k)
            {
              float const w = static_cast<float> (kz - std::abs (k))
                              / static_cast<float> (nw);
              wsum += w;
              ss0 += w * s2_at (ipk + k,         j);
              ss1 += w * s2_at (ipk + n + k,     j);
              ss2 += w * s2_at (ipk + 2 * n + k, j);
              ss3 += w * s2_at (ipk + 3 * n + k, j);
            }
          if (wsum > 0.0f)
            a[static_cast<std::size_t> (j)] =
                (std::max (ss1, ss3) - std::max (ss0, ss2)) / std::sqrt (wsum);
        }
    }

  // Compute CCF for lag = lag1..lag2
  float ccfmax = 0.0f, ccfmin = 0.0f;
  int lagpk_pos = lag1, lagmin_pos = lag1;

  for (int lag = lag1; lag <= lag2; ++lag)
    {
      float x = 0.0f;
      for (int i = 1; i <= nsym; ++i)
        {
          int const j = 2 * i - 1 + lag;
          if (j >= 1 && j <= nsteps)
            x += a[static_cast<std::size_t> (j)]
                 * static_cast<float> (2 * kNpr2[static_cast<std::size_t> (i - 1)] - 1);
        }
      float const val = 2.0f * x;
      ccf_out[static_cast<std::size_t> (lag - lag1)] = val;
      if (val > ccfmax) { ccfmax = val; lagpk_pos = lag; }
      if (val < ccfmin) { ccfmin = val; lagmin_pos = lag; }
    }

  ccf0  = ccfmax;
  lagpk = lagpk_pos;
  flip  = 1.0f;

  if (-ccfmin > ccfmax)
    {
      for (int k = 0; k < nlag; ++k)
        ccf_out[static_cast<std::size_t> (k)] = -ccf_out[static_cast<std::size_t> (k)];
      lagpk = lagmin_pos;
      ccf0  = -ccfmin;
      flip  = -1.0f;
    }
}

// ── sync4 main ────────────────────────────────────────────────────────────────

// Port of sync4.f90.
void sync4_cpp (float const* dat, int jz, int ntol, int nfqso, int mode4, int minwidth,
                float& dtx, float& dfx, float& snrx, float& snrsync,
                float& flip, float& width)
{
  constexpr int nfft  = kSync4Nfft;   // 2520
  constexpr int nh    = kSync4Nh;     // 1260
  constexpr int nq    = kSync4Nq;     // 630
  constexpr int nsym  = 207;          // pseudo-random sequence length

  float const df = 0.5f * 11025.0f / static_cast<float> (nfft);  // 2.1875 Hz

  // Initialise outputs
  dtx = 0.0f; dfx = 0.0f; snrx = -26.0f;
  snrsync = 0.0f; flip = 1.0f; width = 0.0f;

  // Sanity: highest expected frequency must fit
  float const ftop = static_cast<float> (nfqso) + 7.0f * static_cast<float> (mode4) * df;
  if (ftop > 11025.0f / 4.0f)
    return;

  int const nsteps = jz / nq - 1;
  if (nsteps < 1)
    return;

  // ── Build s2(nh, nsteps) spectrum array ──────────────────────────────────
  std::vector<float> s2 (static_cast<std::size_t> (nh * nsteps), 0.0f);

  // Allocate reusable buffer + plan for ps4
  std::vector<float> ps4_buf (static_cast<std::size_t> (nfft + 2), 0.0f);
  fftwf_plan ps4_plan = fftwf_plan_dft_r2c_1d (
      nfft, ps4_buf.data (),
      reinterpret_cast<fftwf_complex*> (ps4_buf.data ()), FFTW_ESTIMATE);

  for (int j = 1; j <= nsteps; ++j)
    {
      int const k0 = (j - 1) * nq;  // 0-based start
      ps4_cpp (dat + k0, nh, ps4_plan, ps4_buf.data (),
               s2.data () + static_cast<std::size_t> (nh * (j - 1)));
    }
  fftwf_destroy_plan (ps4_plan);

  // ── Frequency search range ────────────────────────────────────────────────
  int ia = static_cast<int> (static_cast<float> (nfqso - ntol) / df);
  int ib = static_cast<int> (static_cast<float> (nfqso + ntol) / df);
  int const iamin = static_cast<int> (std::lround (100.0f / df));
  if (ia < iamin) ia = iamin;
  int const ibmax = static_cast<int> (std::lround (2700.0f / df)) - 6 * mode4;
  if (ib > ibmax) ib = ibmax;
  if (ia > ib) return;

  constexpr int lag1 = -5;
  constexpr int lag2 = 59;
  constexpr int nlag = lag2 - lag1 + 1;  // 65

  // Working CCF vector (reused per frequency)
  std::vector<float> ccf_work (nlag);

  // ccfred[i-1] = best ccf0 at frequency i (1-based)
  std::vector<float> ccfred (static_cast<std::size_t> (nh), 0.0f);
  std::vector<float> red    (static_cast<std::size_t> (nh), 0.0f);

  int  ipk      = static_cast<int> (std::lround (static_cast<float> (nfqso) / df));
  int  lagpk    = lag1;
  int  ichpk    = minwidth;
  float syncbest = -1.0e30f;
  float flip_best = 1.0f;

  // ── Main search loop ──────────────────────────────────────────────────────
  for (int ich = minwidth; ich <= 7; ++ich)
    {
      int const kz  = kJt4Nch[static_cast<std::size_t> (ich - 1)] / 2;
      int const iaa = ia + kz;
      int const ibb = ib - kz;
      bool savered  = false;

      for (int i = iaa; i <= ibb; ++i)
        {
          float ccf0_i = 0.0f;
          int   lagpk_i = lag1;
          float flip_i  = 1.0f;

          xcor4_cpp (s2.data (), i, nsteps, nsym, lag1, lag2, ich, mode4, nh,
                     ccf_work.data (), ccf0_i, lagpk_i, flip_i);

          ccfred[static_cast<std::size_t> (i - 1)] = ccf0_i;

          // Detrend CCF around peak (modifies ccf_work in-place)
          float const xpk = static_cast<float> (lagpk_i - lag1 + 1);
          slope_cpp (ccf_work.data (), nlag, xpk);

          float const sync_val =
              std::fabs (ccf_work[static_cast<std::size_t> (lagpk_i - lag1)]);

          if (sync_val > syncbest * 1.03f)
            {
              ipk       = i;
              lagpk     = lagpk_i;
              ichpk     = ich;
              syncbest  = sync_val;
              flip_best = flip_i;
              savered   = true;
            }
        }
      if (savered)
        red = ccfred;
    }

  if (syncbest < -1.0e29f)
    return;

  ccfred = red;

  // Subtract 45th-percentile baseline from ccfred within search range
  {
    int const kz_best = kJt4Nch[static_cast<std::size_t> (ichpk - 1)] / 2;
    int const iaa_b = ia + kz_best;
    int const ibb_b = ib - kz_best;
    if (iaa_b <= ibb_b)
      {
        float const base = pctile_cpp (
            ccfred.data () + static_cast<std::size_t> (iaa_b - 1),
            ibb_b - iaa_b + 1, 45);
        for (float& v : ccfred)
          v -= base;
      }
  }

  dfx = static_cast<float> (ipk) * df;
  flip = flip_best;

  // ── Final xcor4 at best frequency; sub-sample peak-up ────────────────────
  float ccfmax_final = 0.0f;
  int   lagpk_final  = lagpk;
  float flip_final   = flip_best;

  xcor4_cpp (s2.data (), ipk, nsteps, nsym, lag1, lag2, ichpk, mode4, nh,
             ccf_work.data (), ccfmax_final, lagpk_final, flip_final);
  flip = flip_final;

  float xlag = static_cast<float> (lagpk_final);
  if (lagpk_final > lag1 && lagpk_final < lag2)
    {
      int const pk = lagpk_final - lag1;
      float dx2 = peakup_cpp (ccf_work[static_cast<std::size_t> (pk - 1)],
                              ccfmax_final,
                              ccf_work[static_cast<std::size_t> (pk + 1)]);
      xlag = static_cast<float> (lagpk_final) + dx2;
    }

  // Detrend final CCF, compute rms excluding the peak ±2
  slope_cpp (ccf_work.data (), nlag, xlag - static_cast<float> (lag1) + 1.0f);

  float sq = 0.0f;
  int   nsq = 0;
  for (int lag = lag1; lag <= lag2; ++lag)
    {
      if (std::fabs (static_cast<float> (lag) - xlag) > 2.0f)
        {
          float const v = ccf_work[static_cast<std::size_t> (lag - lag1)];
          sq += v * v;
          ++nsq;
        }
    }
  float rms = (nsq > 0) ? std::sqrt (sq / nsq) : 1.0f;
  if (rms <= 0.0f)
    rms = 1.0f;

  float const ccf_pk = ccf_work[static_cast<std::size_t> (lagpk_final - lag1)];
  snrsync = std::max (0.0f, db_value (std::fabs (ccf_pk / rms) - 1.0f) - 4.5f);

  constexpr float dt = 2.0f / 11025.0f;
  int const istart = static_cast<int> (std::lround (xlag * static_cast<float> (nq)));
  dtx = static_cast<float> (istart) * dt;

  // ── Width (half-max of ccfred) and snrx ───────────────────────────────────
  {
    int const ipk1 = static_cast<int> (
        std::max_element (ccfred.begin (), ccfred.end ()) - ccfred.begin ());
    // ipk1a (1-based) = ipk1 + 1
    float const ccf10 = 0.5f * ccfred[static_cast<std::size_t> (ipk1)];

    // Search downward from peak
    int i1 = ipk1;
    for (int i = ipk1; i >= ia - 1; --i)
      {
        if (ccfred[static_cast<std::size_t> (i)] <= ccf10)
          {
            i1 = i;
            break;
          }
      }
    // Search upward from peak
    int i2 = ipk1;
    for (int i = ipk1; i <= ib - 1; ++i)
      {
        if (ccfred[static_cast<std::size_t> (i)] <= ccf10)
          {
            i2 = i;
            break;
          }
      }
    int const nw_width = i2 - i1;
    width = static_cast<float> (nw_width) * df;

    // snrx: rms of ccfred in annular region around peak
    float sq2 = 0.0f;
    int   ns2  = 0;
    int const iaa2 = std::max (ipk1 - 10 * nw_width, ia - 1);
    int const ibb2 = std::min (ipk1 + 10 * nw_width, ib - 1);
    int const jmax = 2 * mode4 / 3;
    for (int i = iaa2; i <= ibb2; ++i)
      {
        int const j = std::abs (i - ipk1);
        if (j > nw_width && j < jmax)
          {
            // Replicate Fortran: ccfred(j) uses j (distance) as 1-based index
            if (j >= 1 && j <= static_cast<int> (ccfred.size ()))
              {
                float const v = ccfred[static_cast<std::size_t> (j - 1)];
                sq2 += v * v;
                ++ns2;
              }
          }
      }
    if (ns2 > 0 && sq2 / ns2 > 0.0f)
      {
        float const rms2 = std::sqrt (sq2 / ns2);
        if (rms2 > 0.0f)
          snrx = 10.0f * std::log10 (
                     ccfred[static_cast<std::size_t> (ipk1)] / rms2) - 41.2f;
      }
  }
}

// ── JT4 metric table (getmet4) ────────────────────────────────────────────────

using MetricTable = std::array<std::array<int, 2>, 256>;

MetricTable const& jt4_metric_table ()
{
  static MetricTable const table = []
  {
    // xx0 data from getmet4.f90 (identical to JT9 table for indices 0-255)
    static constexpr std::array<float, 256> xx0 {{
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,1.000f,
      0.988f,1.000f,0.991f,0.993f,1.000f,0.995f,1.000f,0.991f,
      1.000f,0.991f,0.992f,0.991f,0.990f,0.990f,0.992f,0.996f,
      0.990f,0.994f,0.993f,0.991f,0.992f,0.989f,0.991f,0.987f,
      0.985f,0.989f,0.984f,0.983f,0.979f,0.977f,0.971f,0.975f,
      0.974f,0.970f,0.970f,0.970f,0.967f,0.962f,0.960f,0.957f,
      0.956f,0.953f,0.942f,0.946f,0.937f,0.933f,0.929f,0.920f,
      0.917f,0.911f,0.903f,0.895f,0.884f,0.877f,0.869f,0.858f,
      0.846f,0.834f,0.821f,0.806f,0.790f,0.775f,0.755f,0.737f,
      0.713f,0.691f,0.667f,0.640f,0.612f,0.581f,0.548f,0.510f,
      0.472f,0.425f,0.378f,0.328f,0.274f,0.212f,0.146f,0.075f,
      0.000f,-0.079f,-0.163f,-0.249f,-0.338f,-0.425f,-0.514f,-0.606f,
      -0.706f,-0.796f,-0.895f,-0.987f,-1.084f,-1.181f,-1.280f,-1.376f,
      -1.473f,-1.587f,-1.678f,-1.790f,-1.882f,-1.992f,-2.096f,-2.201f,
      -2.301f,-2.411f,-2.531f,-2.608f,-2.690f,-2.829f,-2.939f,-3.058f,
      -3.164f,-3.212f,-3.377f,-3.463f,-3.550f,-3.768f,-3.677f,-3.975f,
      -4.062f,-4.098f,-4.186f,-4.261f,-4.472f,-4.621f,-4.623f,-4.608f,
      -4.822f,-4.870f,-4.652f,-4.954f,-5.108f,-5.377f,-5.544f,-5.995f,
      -5.632f,-5.826f,-6.304f,-6.002f,-6.559f,-6.369f,-6.658f,-7.016f,
      -6.184f,-7.332f,-6.534f,-6.152f,-6.113f,-6.288f,-6.426f,-6.313f,
      -9.966f,-6.371f,-9.966f,-7.055f,-9.966f,-6.629f,-6.313f,-9.966f,
      -5.858f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,
      -9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f,-9.966f
    }};

    MetricTable result {};
    // JT4 uses slope 6.822/65.3 per step beyond i=160 (differs from JT9's slope=2)
    for (int i = 0; i <= 255; ++i)
      {
        float xx = xx0[static_cast<std::size_t> (i)];
        if (i >= 160)
          xx = xx0[160] - static_cast<float> (i - 160) * 6.822f / 65.3f;
        int const value = static_cast<int> (std::lround (50.0f * (xx - 0.5f)));
        result[static_cast<std::size_t> (i)][0] = value;
        if (i >= 1)
          result[static_cast<std::size_t> (256 - i)][1] = value;
      }
    result[0][1] = result[1][1];
    return result;
  } ();
  return table;
}

// ── interleave4 de-interleaver ────────────────────────────────────────────────

// Build the 8-bit bit-reversal permutation table (same as interleave4.f90's j0).
std::array<int, 206> build_interleave4_j0 ()
{
  std::array<int, 206> j0 {};
  int k = -1;
  for (int i = 0; i <= 255; ++i)
    {
      int m = i;
      int n = 0;
      for (int b = 0; b < 8; ++b)
        {
          n = (n << 1) | (m & 1);
          m >>= 1;
        }
      if (n <= 205)
        {
          ++k;
          j0[static_cast<std::size_t> (k)] = n;
        }
    }
  return j0;
}

// De-interleave id[0..205] in-place (ndir = -1 in Fortran terms).
void interleave4_deinterleave (std::int8_t* id)
{
  static std::array<int, 206> const j0 = build_interleave4_j0 ();
  std::array<std::int8_t, 206> tmp {};
  for (int i = 0; i < 206; ++i)
    tmp[static_cast<std::size_t> (i)] = id[j0[static_cast<std::size_t> (i)]];
  for (int i = 0; i < 206; ++i)
    id[i] = tmp[static_cast<std::size_t> (i)];
}

// ── extract4 ─────────────────────────────────────────────────────────────────

// Port of extract4.f90.
// sym0_fortran[0..206]: 207 soft symbols (Fortran sym0(207)).
// decoded_out: filled with decoded 22-char message on success.
// Returns 0 on success, -1 on failure.
int extract4_cpp (float const* sym0_fortran, QByteArray& decoded_out)
{
  constexpr float amp   = 30.0f;
  constexpr int   limit = 10000;          // Fano cycles per bit
  constexpr int   nbits = 72 + 31;        // 103 encoded bits

  decoded_out = blank_message ();

  // Normalise: subtract mean, divide by rms
  float ave0 = 0.0f;
  for (int i = 0; i < 207; ++i)
    ave0 += sym0_fortran[i];
  ave0 /= 207.0f;

  std::array<float, 207> sym {};
  for (int i = 0; i < 207; ++i)
    sym[i] = sym0_fortran[i] - ave0;

  float sq = 0.0f;
  for (float v : sym)
    sq += v * v;
  float const rms0 = std::sqrt (sq / 206.0f);
  if (!(rms0 > 0.0f))
    return -1;
  for (float& v : sym)
    v /= rms0;

  // Quantise to [-127, 127]
  std::array<std::int8_t, 207> symbol {};
  for (int j = 0; j < 207; ++j)
    {
      int n = static_cast<int> (std::lround (amp * sym[j]));
      if (n < -127) n = -127;
      if (n >  127) n =  127;
      symbol[static_cast<std::size_t> (j)] = static_cast<std::int8_t> (n);
    }

  // De-interleave symbol[1..206] (Fortran symbol(2..207))
  interleave4_deinterleave (&symbol[1]);

  // Build mettab_c[tx_bit][unsigned_sym] from the signed table
  MetricTable const& mettab_signed = jt4_metric_table ();
  int mettab_c[2][256] {};
  for (int u = 0; u < 256; ++u)
    {
      int const sv  = static_cast<int> (static_cast<std::int8_t> (static_cast<unsigned char> (u)));
      int const idx = sv + 128;
      mettab_c[0][u] = mettab_signed[static_cast<std::size_t> (idx)][0];
      mettab_c[1][u] = mettab_signed[static_cast<std::size_t> (idx)][1];
    }

  // Fano decode: 206 symbols starting at symbol[1], 103 encoded bits
  constexpr int kNdelta = 170;  // nint(3.4 * 50)
  std::array<unsigned char, 13> data1 {};
  unsigned int metric = 0, ncycles = 0, maxnp = 0;

  // Pass symbol[1..206] as unsigned char (bit-pattern reinterpret; matches JT9FastDecoder)
  auto* syms_u8 = reinterpret_cast<unsigned char*> (&symbol[1]);
  int const ierr = decodium::fano::sequential_decode (&metric, &ncycles, &maxnp,
                                                      data1.data (), syms_u8,
                                                      static_cast<unsigned int> (nbits),
                                                      mettab_c, kNdelta,
                                                      static_cast<unsigned int> (limit));

  if (ierr != 0 || ncycles >= static_cast<unsigned int> (nbits * limit))
    return -1;

  // Unpack 72 bits (9 bytes) → 12 × 6-bit words → message string
  std::array<int, 12> words {};
  int bit_index = 0;
  for (int i = 0; i < 12; ++i)
    {
      int value = 0;
      for (int j = 0; j < 6; ++j)
        {
          int const byte_idx    = bit_index / 8;
          int const bit_in_byte = 7 - (bit_index % 8);
          unsigned char const byte =
              data1[static_cast<std::size_t> (byte_idx)];
          value = (value << 1) | ((byte >> bit_in_byte) & 1u);
          ++bit_index;
        }
      words[static_cast<std::size_t> (i)] = value;
    }

  decoded_out = decodium::legacy_jt::detail::unpackmsg (words);
  if (decoded_out.left (6) == QByteArray ("000AAA"))
    {
      decoded_out = blank_message ();
      return -1;
    }
  return 0;
}

// ── deep4 (C++ replacement for deep4.f90) ────────────────────────────────────

// Encode a 22-char message as ±1 bits (encode4.f90 algorithm: packmsg → entail
// → encode232 → interleave9, same permutation as interleave4 forward).
bool encode4_bits (std::string const& msg22,
                   std::array<float, 206>& code_out)
{
  using namespace decodium::legacy_jt::detail;
  QByteArray ba = fixed_ascii (QString::fromStdString (msg22).toLatin1 (), 22);
  QByteArray const normalized = fmtmsg (ba);
  PackedJtMessage const packed = packmsg (normalized);

  std::array<signed char, 13>    tail_bytes {};
  std::array<unsigned char, 206> enc_bits   {};
  std::array<unsigned char, 206> interleaved {};

  entail (packed.dat, tail_bytes);
  encode232 (tail_bytes, enc_bits);
  interleave9 (enc_bits, interleaved);

  for (int i = 0; i < 206; ++i)
    code_out[i] = interleaved[i] ? 1.0f : -1.0f;
  return true;
}

// Compute the Fortran-style "effective call length": first-space position minus 1,
// clamped to [6, maxlen].
int effective_len (std::string const& s, int maxlen)
{
  auto it = std::find (s.begin (), s.end (), ' ');
  int j = static_cast<int> (it - s.begin ()) - 1;
  if (j <= -1) j = maxlen;
  if (j < 3)   j = 6;
  if (j > maxlen) j = maxlen;
  return j;
}

// Build or reuse the template cache.  Mirrors the save-state rebuild logic in deep4.f90.
void build_deep4_cache (decodium::jt4::Deep4Cache& cache,
                        std::string const& mycall12, std::string const& hiscall12,
                        std::string const& hisgrid6, int neme,
                        QString const& dataDir)
{
  if (mycall12 == cache.mycall0 && hiscall12 == cache.hiscall0
      && hisgrid6 == cache.hisgrid0 && neme == cache.neme0)
    return;

  cache.entries.clear ();
  cache.mycall0  = mycall12;
  cache.hiscall0 = hiscall12;
  cache.hisgrid0 = hisgrid6;
  cache.neme0    = neme;

  constexpr int MAXCALLS = 7000;
  constexpr int MAXRPT   = 63;

  static const std::array<char const*, MAXRPT> rpt {{
    "-01","-02","-03","-04","-05","-06","-07","-08","-09","-10",
    "-11","-12","-13","-14","-15","-16","-17","-18","-19","-20",
    "-21","-22","-23","-24","-25","-26","-27","-28","-29","-30",
    "R-01","R-02","R-03","R-04","R-05","R-06","R-07","R-08","R-09","R-10",
    "R-11","R-12","R-13","R-14","R-15","R-16","R-17","R-18","R-19","R-20",
    "R-21","R-22","R-23","R-24","R-25","R-26","R-27","R-28","R-29","R-30",
    "RO","RRR","73"
  }};

  auto add_msg = [&] (std::string const& msg) {
    decodium::jt4::Deep4Entry entry;
    entry.msg = msg;
    if (encode4_bits (msg, entry.code))
      cache.entries.push_back (std::move (entry));
  };

  // Compute effective call strings (trim to first space, min length 6)
  int const j1 = effective_len (mycall12, 12);
  int const j2 = effective_len (hiscall12, 12);
  std::string const my  = mycall12.substr  (0, static_cast<std::size_t> (j1));
  std::string const his = hiscall12.substr (0, static_cast<std::size_t> (j2));

  bool const j3 = (mycall12.find ('/') != std::string::npos);
  bool const j4 = (hiscall12.find ('/') != std::string::npos);

  // grid4 from hisgrid6: first 4 chars, NUL → space
  std::string grid4 = hisgrid6.substr (0, 4);
  for (char& c : grid4) if (c == '\0') c = ' ';

  // ── n=1: hiscall entry ───────────────────────────────────────────────────
  // mz = MAXRPT+1 if no compound calls and hiscall is not all spaces; else 1
  bool const his_not_blank = (his.find_first_not_of (' ') != std::string::npos);
  int const mz = (!j3 && !j4 && his_not_blank) ? MAXRPT + 1 : 1;

  for (int m = 1; m <= mz; ++m)
    {
      std::string const grid = (m == 1) ? grid4 : std::string (rpt[m - 2]);
      std::string callgrid;
      if (!j3 && !j4)
        callgrid = his + " " + grid;
      else
        callgrid = his;
      add_msg (my + " " + callgrid);
      // No CQ messages for n=1
    }

  // ── n>=2: CALL3.TXT entries ─────────────────────────────────────────────
  QString const call3path = QDir (dataDir).filePath (QStringLiteral ("CALL3.TXT"));
  QFile file (call3path);
  if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in (&file);
  int ncalls = 0;

  while (ncalls < MAXCALLS - 1 && !in.atEnd ())
    {
      QString const line = in.readLine ();
      if (line.startsWith (QStringLiteral ("ZZZZ")))
        break;
      if (line.startsWith (QStringLiteral ("//")))
        continue;

      // Format: CALLSIGN,GRID,CEME[,...] (commas mandatory, first field ≥3 chars)
      int const i1 = line.indexOf (QLatin1Char (','));
      if (i1 < 3)
        continue;
      QString const rest1 = line.mid (i1 + 1);
      int const i2rel = rest1.indexOf (QLatin1Char (','));
      if (i2rel < 4)
        continue;  // grid must be ≥ 4 chars

      std::string callsign = line.left (i1).toStdString ();
      std::string grid     = rest1.left (i2rel).toStdString ();
      QString const rest2  = rest1.mid (i2rel + 1);
      // ceme is everything up to the next comma or space
      int ci = rest2.indexOf (QLatin1Char (','));
      int si = rest2.indexOf (QLatin1Char (' '));
      int ceme_end = (ci >= 0 && (si < 0 || ci < si)) ? ci : si;
      std::string const ceme = (ceme_end >= 0 ? rest2.left (ceme_end) : rest2).toStdString ();

      if (neme == 1 && ceme != "EME")
        continue;

      ++ncalls;

      int const j2k = effective_len (callsign, 12);
      bool const j4k = (callsign.find ('/') != std::string::npos);
      std::string const call_eff = callsign.substr (0, static_cast<std::size_t> (j2k));

      // mz=1 for CALL3.TXT entries
      std::string callgrid;
      if (!j3 && !j4k)
        callgrid = call_eff + " " + grid;
      else
        callgrid = call_eff;

      add_msg (my + " " + callgrid);          // "MY CALLSIGN GRID"
      add_msg (std::string ("CQ ") + callgrid);  // "CQ CALLSIGN GRID"
    }
}

// C++ replacement for deep4_.
// sym0[0..205]: normalized soft symbols from decode4 (sym(2,ich) in Fortran).
// flip: >0 normal, ≤0 inverted sync (skip CQ candidates).
void deep4_cpp (float const* sym0, int neme, float flip,
                QByteArray const& mycall, QByteArray const& hiscall,
                QByteArray const& hisgrid,
                QByteArray& decoded, float& qual,
                QString const& dataDir,
                decodium::jt4::Deep4Cache& cache)
{
  std::string const mc (mycall.constData  (), static_cast<std::size_t> (std::min ((int)mycall.size  (), 12)));
  std::string const hc (hiscall.constData (), static_cast<std::size_t> (std::min ((int)hiscall.size (), 12)));
  std::string const hg (hisgrid.constData (), static_cast<std::size_t> (std::min ((int)hisgrid.size (), 6)));
  build_deep4_cache (cache, mc, hc, hg, neme, dataDir);

  decoded = blank_message ();
  qual    = 0.0f;
  if (cache.entries.empty ())
    return;

  // Normalize sym0 by RMS
  double sq = 0.0;
  for (int j = 0; j < 206; ++j)
    sq += static_cast<double> (sym0[j]) * sym0[j];
  float const rms = static_cast<float> (std::sqrt (sq / 206.0));
  if (rms < 1e-12f)
    return;

  std::vector<float> sym (206);
  for (int j = 0; j < 206; ++j)
    sym[j] = sym0[j] / rms;

  // Find best (p1) and second-best (p2) dot-product
  float p1 = -1.0e30f, p2 = -1.0e30f;
  int ip1 = -1;
  std::vector<float> pp (cache.entries.size (), 0.0f);

  for (int k = 0; k < static_cast<int> (cache.entries.size ()); ++k)
    {
      auto const& entry = cache.entries[static_cast<std::size_t> (k)];
      bool const is_cq = entry.msg.size () >= 3 && entry.msg.substr (0, 3) == "CQ ";
      if (flip <= 0.0f && is_cq)
        continue;

      float p = 0.0f;
      for (int j = 0; j < 206; ++j)
        p += entry.code[j] * sym[j];
      pp[static_cast<std::size_t> (k)] = p;

      if (p > p1)
        {
          p1  = p;
          ip1 = k;
        }
    }

  if (ip1 < 0)
    return;

  for (int k = 0; k < static_cast<int> (cache.entries.size ()); ++k)
    {
      float const pk = pp[static_cast<std::size_t> (k)];
      if (pk > p2 && cache.entries[static_cast<std::size_t> (k)].msg
                       != cache.entries[static_cast<std::size_t> (ip1)].msg)
        p2 = pk;
    }

  qual = p1 - std::max (1.15f * p2, 70.0f);

  if (qual > 1.0f)
    {
      std::string const& best = cache.entries[static_cast<std::size_t> (ip1)].msg;
      QByteArray msg_ba = QByteArray::fromStdString (best)
                              .leftJustified (22, ' ', true).left (22);
      for (char& c : msg_ba)
        if (c >= 'a' && c <= 'z')
          c = static_cast<char> (c - 32);
      decoded = msg_ba;
    }
  else
    {
      qual = 0.0f;
    }
}

// ── decode4 ──────────────────────────────────────────────────────────────────

// Port of decode4.f90: FSK coherent detection + Fano/deep decode.
// rsymbol_out layout: Fortran rsymbol_out(207,7), column-major flat array.
void decode4_cpp (float const* dat, int npts, float dtx, int nfreq, float flip,
                  int mode4, int ndepth, int neme, int minw,
                  QByteArray const& mycall, QByteArray const& hiscall,
                  QByteArray const& hisgrid,
                  QString const& dataDir, decodium::jt4::Deep4Cache& deep4_cache,
                  QByteArray& decoded, int& nfano, QByteArray& deepbest,
                  float& qbest, int& ichbest, float* rsymbol_out,
                  int& ich1_out, int& ich2_out)
{
  constexpr double twopi = 6.283185307179586;
  constexpr double dt    = 2.0 / 11025.0;    // sample interval at 5512.5 Hz
  constexpr double df    = 11025.0 / 2520.0; // JT4A tone separation
  constexpr int    nsym  = 206;
  constexpr float  amp   = 15.0f;

  decoded = blank_message ();
  deepbest = blank_message ();
  ichbest = -1;
  qbest   = 0.0f;
  nfano   = 0;
  float qbest_local = 0.0f;

  // Zero rsymbol_out
  for (int i = 0; i < kNsymMax * kNsubMax; ++i)
    rsymbol_out[i] = 0.0f;

  ich1_out = minw + 1;
  ich2_out = ich1_out;
  for (int ich = 1; ich <= 7; ++ich)
    if (kJt4Nch[static_cast<std::size_t> (ich - 1)] <= mode4)
      ich2_out = ich;

  int const istart = [&]
  {
    int v = static_cast<int> (std::lround ((dtx + 0.8) / dt));
    return v < 0 ? 0 : v;
  } ();

  float const fac2_base = 1.0e-8f * std::sqrt (static_cast<float> (mode4));

  for (int ich = ich1_out; ich <= ich2_out; ++ich)
    {
      int const nchips  = std::min (kJt4Nch[static_cast<std::size_t> (ich - 1)], 70);
      int const nspchip = 1260 / nchips;

      int k = istart;

      for (int j = 1; j <= nsym + 1; ++j)
        {
          // Carrier frequencies for this symbol, depending on pseudo-random bit and flip
          int const npr_j = kNpr2[static_cast<std::size_t> (j - 1)];
          double f0, f1;
          if (flip > 0.0f)
            {
              f0 = nfreq + npr_j * mode4 * df;
              f1 = nfreq + (2 + npr_j) * mode4 * df;
            }
          else
            {
              f0 = nfreq + (1 - npr_j) * mode4 * df;
              f1 = nfreq + (3 - npr_j) * mode4 * df;
            }
          double const dphi  = twopi * dt * f0;
          double const dphi1 = twopi * dt * f1;

          // Non-coherent sum over nchips coherent sub-integrations
          float sq0 = 0.0f, sq1 = 0.0f;
          for (int nc = 1; nc <= nchips; ++nc)
            {
              double phi = 0.0, phi1 = 0.0;
              double c0_re = 0.0, c0_im = 0.0;
              double c1_re = 0.0, c1_im = 0.0;
              for (int ii = 1; ii <= nspchip; ++ii)
                {
                  ++k;
                  phi  += dphi;
                  phi1 += dphi1;
                  if (k >= 1 && k <= npts)
                    {
                      float const d = dat[static_cast<std::size_t> (k - 1)];
                      c0_re += d * std::cos (phi);
                      c0_im -= d * std::sin (phi);
                      c1_re += d * std::cos (phi1);
                      c1_im -= d * std::sin (phi1);
                    }
                }
              sq0 += static_cast<float> (c0_re * c0_re + c0_im * c0_im);
              sq1 += static_cast<float> (c1_re * c1_re + c1_im * c1_im);
            }

          float const rsym = amp * fac2_base * (sq1 - sq0);
          rsymbol_out[sym_idx (j, ich)] = rsym;
        }

      // Try Fano decode for this channel width
      float const* sym_col = rsymbol_out + sym_idx (1, ich);
      QByteArray decoded_k = blank_message ();
      int const ncount = extract4_cpp (sym_col, decoded_k);

      if (ncount >= 0)
        {
          nfano   = 1;
          ichbest = ich;
          decoded = decoded_k;
          return;
        }

      // Try deep decode (bit 5 of ndepth)
      if ((ndepth & 32) == 32)
        {
          float qual_k  = 0.0f;
          QByteArray deepmsg_k = blank_message ();
          float* sym2 = rsymbol_out + sym_idx (2, ich);
          deep4_cpp (sym2, neme, flip,
                     mycall, hiscall, hisgrid,
                     deepmsg_k, qual_k, dataDir, deep4_cache);
          if (qual_k > qbest_local)
            {
              qbest_local = qual_k;
              deepbest    = deepmsg_k;
              ichbest     = ich;
            }
        }
    }

  qbest = qbest_local;
}

// ── compute_jt4_snr ──────────────────────────────────────────────────────────

int compute_jt4_snr (float const* dat, int npts, int mode4, float dtx,
                     QByteArray const& decoded)
{
  constexpr int kNsps = 1260;
  constexpr int kNh   = kNsps / 2;

  if (!dat || npts <= 0 || mode4 <= 0)
    return -99;

  // Generate expected tones via C++ JT4 encoder
  QString const msg_str =
      QString::fromLatin1 (decoded.leftJustified (22, ' ', true).left (22));
  decodium::txmsg::EncodedMessage const enc =
      decodium::legacy_jt::encodeJt4 (msg_str);
  if (!enc.ok || enc.tones.size () < 206)
    return -99;

  float constexpr dt = 1.0f / 5512.5f;
  float constexpr df = 5512.5f / static_cast<float> (kNsps);
  int const istart = static_cast<int> ((dtx + 1.08f) / dt);

  std::vector<float> savg (static_cast<std::size_t> (kNh), 0.0f);
  std::vector<float> fft_buffer (static_cast<std::size_t> (kNsps + 2), 0.0f);
  auto* cx = reinterpret_cast<std::complex<float>*> (fft_buffer.data ());

  for (int symbol = 0; symbol < static_cast<int> (enc.tones.size ()); ++symbol)
    {
      std::fill (fft_buffer.begin (), fft_buffer.end (), 0.0f);
      for (int sample = 0; sample < kNsps; ++sample)
        {
          int const k1 = symbol * kNsps + (sample + 1) + istart;
          if (k1 >= 1 && k1 <= npts)
            fft_buffer[static_cast<std::size_t> (sample)] =
                dat[static_cast<std::size_t> (k1 - 1)];
        }

      fftw_forward_real_buffer (fft_buffer.data (), kNsps);

      std::array<float, kNh> s {};
      for (int i = 0; i < kNh; ++i)
        {
          std::complex<float> const value = cx[static_cast<std::size_t> (i + 1)];
          s[static_cast<std::size_t> (i)] =
              value.real () * value.real () + value.imag () * value.imag ();
        }

      int const nshift = mode4 * enc.tones[symbol];
      for (int i = 0; i < kNh; ++i)
        {
          int shifted = i + nshift;
          while (shifted >= kNh)
            shifted -= kNh;
          savg[static_cast<std::size_t> (i)] += s[static_cast<std::size_t> (shifted)];
        }
    }

  int nskip = mode4 / 2;
  int nh    = kNh;
  float ave = 0.0f, rms = 0.0f;
  averms_cpp (savg.data (), nh, nskip, ave, rms);
  if (rms <= 0.0f)
    return -99;

  for (float& value : savg)
    value = (value - ave) / rms;

  for (int i = 0; i < mode4 / 4; ++i)
    smo121_cpp (savg.data (), nh);

  auto const peak = std::max_element (savg.begin (), savg.end ());
  if (peak == savg.end ())
    return -99;

  int const ipk = static_cast<int> (std::distance (savg.begin (), peak));
  float psig = 0.0f;

  int left = ipk;
  while (left >= 0 && savg[static_cast<std::size_t> (left)] >= 0.0f)
    {
      psig += savg[static_cast<std::size_t> (left)];
      --left;
    }

  int right = ipk + 1;
  while (right < kNh && savg[static_cast<std::size_t> (right)] >= 0.0f)
    {
      psig += savg[static_cast<std::size_t> (right)];
      ++right;
    }

  float const pnoise = 2500.0f * std::sqrt (206.0f) / df;
  return static_cast<int> (std::lround (db_value (psig / pnoise)));
}

// ── Formatting helpers ────────────────────────────────────────────────────────

QString format_sync_line (int nutc, int snr, float dt, int freq, char sync_char,
                           QByteArray const& decoded, QByteArray const& cflags)
{
  char buf[81] {};
  std::snprintf (buf, sizeof buf, "%04d%4d%5.1f%5d $%c %22.22s %3.3s",
                 nutc, snr, dt, freq, sync_char,
                 decoded.constData (), cflags.constData ());
  return QString::fromLatin1 (buf);
}

QString format_nosync_line (int nutc, int snr, float dt, int freq)
{
  char buf[32] {};
  std::snprintf (buf, sizeof buf, "%04d%4d%5.1f%5d", nutc, snr, dt, freq);
  return QString::fromLatin1 (buf);
}

std::string format_avg_line (char used, int utc, float sync, float dt,
                             int freq, char csync)
{
  char buf[40] {};
  std::snprintf (buf, sizeof buf, "%c%5.4d%6.1f%6.2f%6d %c",
                 used, utc, sync, dt, freq, csync);
  return std::string {buf};
}

void write_avg_lines (QString const& tempDirPath, std::vector<std::string> const& lines)
{
  if (tempDirPath.isEmpty ())
    return;
  QFile file {QDir {tempDirPath}.filePath (QStringLiteral ("avemsg.txt"))};
  if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return;
  QTextStream stream {&file};
  for (std::string const& line : lines)
    stream << QString::fromLatin1 (line.data (), static_cast<int> (line.size ())) << '\n';
}

QByteArray compute_cflags (bool is_decoded, bool is_deep, float qual,
                            bool is_average, int ave, QByteArray& decoded)
{
  QByteArray cflags {"   ", 3};
  if (!is_decoded)
    return cflags;

  cflags[0] = 'f';
  if (is_deep)
    {
      cflags[0] = 'd';
      int q = std::min (static_cast<int> (qual), 9);
      cflags[1] = (qual >= 10.0f) ? '*' : static_cast<char> ('0' + q);
      if (qual < 3.0f && !decoded.isEmpty ())
        decoded[decoded.size () - 1] = '?';
    }
  if (is_average)
    {
      cflags[2] = (ave >= 10) ? '*' : static_cast<char> ('0' + std::min (ave, 9));
      if (cflags[0] == 'f')
        {
          cflags[1] = cflags[2];
          cflags[2] = ' ';
        }
    }
  return cflags;
}

QString trim_and_format (QString const& line)
{
  int end = line.size ();
  while (end > 0)
    {
      QChar c = line[end - 1];
      if (c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
          --end;
          continue;
        }
      break;
    }
  return line.left (end);
}

// ── avg4 ─────────────────────────────────────────────────────────────────────

struct Avg4Result
{
  int nfanoave {0};
  QByteArray avemsg;
  float qave {0.0f};
  QByteArray deepave;
  int ichbest {1};
  int ndeepave {0};
  float dtave {0.0f};
  int nfreqave {0};
  std::vector<std::string> avg_lines;
};

Avg4Result avg4_compute (decodium::jt4::AverageState& state,
                         int nutc, float snrsync, float dtxx, float flip, int nfreq,
                         int mode4, int ntol, int ndepth, int neme,
                         QByteArray const& mycall, QByteArray const& hiscall,
                         QByteArray const& hisgrid,
                         QString const& dataDir)
{
  Avg4Result result;
  result.avemsg   = blank_message ();
  result.deepave  = blank_message ();
  result.dtave    = dtxx;
  result.nfreqave = nfreq;

  constexpr float kDtdiff = 0.2f;

  bool found_existing = false;
  for (int i = 0; i < kMaxAve; ++i)
    {
      if (nutc == state.avg_iutc[i]
          && std::abs (nfreq - state.avg_nfsave[i]) <= ntol)
        {
          found_existing = true;
          break;
        }
    }

  if (!found_existing)
    {
      int const s0 = state.nsave - 1;
      state.avg_iutc[s0]    = nutc;
      state.avg_syncsave[s0] = snrsync;
      state.avg_dtsave[s0]  = dtxx;
      state.avg_nfsave[s0]  = nfreq;
      state.avg_flipsave[s0] = flip;
      for (int i = 1; i <= kNsymMax; ++i)
        for (int j = 1; j <= kNsubMax; ++j)
          state.avg_ppsave[ppsave_idx (i, j, state.nsave)] =
              state.rsymbol[sym_idx (i, j)];
    }

  std::array<float, kNsymMax * kNsubMax> sym {};
  sym.fill (0.0f);
  std::array<char, kMaxAve> cused {};
  cused.fill ('.');
  int nsum = 0;

  for (int i = 0; i < kMaxAve; ++i)
    {
      if (state.avg_iutc[i] < 0)
        continue;
      if ((state.avg_iutc[i] % 2) != (nutc % 2))
        continue;
      if (std::fabs (dtxx - state.avg_dtsave[i]) > kDtdiff)
        continue;
      if (std::abs (nfreq - state.avg_nfsave[i]) > ntol)
        continue;
      if (flip != state.avg_flipsave[i])
        continue;

      for (int ii = 1; ii <= kNsymMax; ++ii)
        for (int jj = 1; jj <= kNsubMax; ++jj)
          sym[sym_idx (ii, jj)] +=
              state.avg_ppsave[ppsave_idx (ii, jj, i + 1)];

      cused[static_cast<std::size_t> (i)] = '$';
      ++nsum;
    }

  if (nsum == 0)
    return result;

  for (float& v : sym)
    v /= static_cast<float> (nsum);

  for (int i = 0; i < state.nsave; ++i)
    {
      char csync = state.avg_flipsave[i] < 0.0f ? '#' : '*';
      result.avg_lines.push_back (format_avg_line (
          cused[static_cast<std::size_t> (i)],
          state.avg_iutc[i],
          state.avg_syncsave[i],
          state.avg_dtsave[i],
          state.avg_nfsave[i],
          csync));
    }

  // Try Fano decode for each applicable channel width
  for (int k = state.ich1; k <= state.ich2; ++k)
    {
      float* sym_col = sym.data () + sym_idx (1, k);
      QByteArray avemsg_k = blank_message ();
      int const ncount = extract4_cpp (sym_col, avemsg_k);
      if (ncount >= 0)
        {
          result.ichbest  = k;
          result.nfanoave = nsum;
          result.avemsg   = avemsg_k;
          return result;
        }
      if (kJt4Nch[static_cast<std::size_t> (k - 1)] >= mode4)
        break;
    }

  // Try deep decode for average (ndepth bit 5 = 32)
  if ((ndepth & 32) == 32)
    {
      float flipx       = 1.0f;
      float qbest_local = 0.0f;
      QByteArray deepbest_local = blank_message ();
      int   kbest_local = state.ich1;

      for (int k = state.ich1; k <= state.ich2; ++k)
        {
          float* sym_col2   = sym.data () + sym_idx (2, k);
          QByteArray deepave_k = blank_message ();
          float qave_k = 0.0f;
          deep4_cpp (sym_col2, neme, flipx,
                     mycall, hiscall, hisgrid,
                     deepave_k, qave_k, dataDir, state.deep4_cache);
          if (qave_k > qbest_local)
            {
              qbest_local     = qave_k;
              deepbest_local  = deepave_k;
              kbest_local     = k;
              result.ndeepave = nsum;
            }
          if (kJt4Nch[static_cast<std::size_t> (k - 1)] >= mode4)
            break;
        }

      result.deepave = deepbest_local;
      result.qave    = qbest_local;
      result.ichbest = kbest_local;
    }

  return result;
}

// ── wsjt4 orchestration ───────────────────────────────────────────────────────

QStringList wsjt4_impl (float* dat, int npts,
                        int nutc, int nfqso, int ntol, int ndepth,
                        int minsync, int minw, int mode4, int neme,
                        bool nclearave,
                        QByteArray const& mycall, QByteArray const& hiscall,
                        QByteArray const& hisgrid,
                        QString const& tempDir, QString const& dataDir,
                        decodium::jt4::AverageState& state)
{
  QStringList lines;

  int naggressive = ((ndepth & 7) >= 2) ? 1 : 0;
  int const nq1   = (naggressive == 1) ? 1 : 3;

  if (nclearave)
    {
      state.nsave = 0;
      state.avg_iutc.fill (-1);
      state.avg_nfsave.fill (0);
      state.avg_syncsave.fill (0.0f);
      state.avg_dtsave.fill (0.0f);
      state.avg_flipsave.fill (0.0f);
      state.avg_ppsave.fill (0.0f);
      state.rsymbol.fill (0.0f);
      state.avg_last_utc  = -999;
      state.avg_last_freq = -999999;
    }

  // ── sync4: find best DT and DF ───────────────────────────────────────────
  int   minwidth = minw + 1;
  float dtx = 0.0f, dfx = 0.0f, snrx = 0.0f, snrsync = 0.0f;
  float flip = 1.0f, width = 0.0f;

  sync4_cpp (dat, npts, ntol, nfqso, mode4, minwidth,
             dtx, dfx, snrx, snrsync, flip, width);

  float const syncmin = 1.0f + static_cast<float> (minsync);
  float const dtxz    = dtx - 0.8f;
  int   const nfreqz  = static_cast<int> (std::lround (dfx));
  char  const csync   = (flip < 0.0f) ? '#' : '*';

  int nsnr = -26;

  if (snrsync < syncmin)
    {
      QString line = format_nosync_line (nutc, nsnr, dtxz, nfreqz);
      line = trim_and_format (line);
      if (!line.isEmpty ())
        lines << line;
      return lines;
    }

  nsnr = static_cast<int> (std::lround (snrsync - 22.9f));

  bool   prtavg  = false;
  float  qbest   = 0.0f;
  float  dtx0    = dtxz;
  int    nfreq0  = nfreqz;
  QByteArray deepmsg0 = blank_message ();
  int    ich0    = state.ich1;

  float  qabest  = 0.0f;
  float  dtx1    = dtxz;
  int    nfreq1  = nfreqz;
  QByteArray deepave1 = blank_message ();
  int    ndeepave = 0;

  std::vector<std::string> all_avg_lines;

  for (int idt = -2; idt <= 2; ++idt)
    {
      float const dtx_cur  = dtxz + 0.03f * static_cast<float> (idt);
      int   const nfreq_cur = nfreqz;

      QByteArray decoded  = blank_message ();
      QByteArray deepbest = blank_message ();
      float qbest_local   = 0.0f;
      int   ichbest       = state.ich1;
      int   nfano         = 0;

      decode4_cpp (dat, npts, dtx_cur, nfreq_cur,
                   flip, mode4, ndepth, neme, minw,
                   mycall, hiscall, hisgrid, dataDir, state.deep4_cache,
                   decoded, nfano, deepbest, qbest_local,
                   ichbest, state.rsymbol.data (),
                   state.ich1, state.ich2);

      if (nfano > 0)
        {
          nsnr = compute_jt4_snr (dat, npts, mode4, dtx_cur, decoded);
          bool const is_decoded = (decoded != blank_message ());
          QByteArray cflags =
              compute_cflags (is_decoded, false, 99.0f, false, 0, decoded);
          QString line = format_sync_line (nutc, nsnr, dtx_cur, nfreq_cur,
                                           csync, decoded, cflags);
          line = trim_and_format (line);
          if (!line.isEmpty ())
            lines << line;
          write_avg_lines (tempDir, all_avg_lines);
          return lines;
        }

      if (qbest_local > qbest)
        {
          qbest    = qbest_local;
          dtx0     = dtx_cur;
          nfreq0   = nfreq_cur;
          deepmsg0 = deepbest;
          ich0     = ichbest;
        }

      if (idt != 0)
        continue;

      if ((ndepth & 16) == 16 && !prtavg)
        {
          if (nutc != state.avg_last_utc
              || std::abs (nfreq_cur - state.avg_last_freq) > ntol)
            {
              state.avg_last_utc  = nutc;
              state.avg_last_freq = nfreq_cur;
              state.nsave = (state.nsave % kMaxAve) + 1;

              Avg4Result avg = avg4_compute (
                  state, nutc, snrsync, dtx_cur, flip, nfreq_cur,
                  mode4, ntol, ndepth, neme, mycall, hiscall, hisgrid, dataDir);

              all_avg_lines.insert (all_avg_lines.end (),
                                    avg.avg_lines.begin (), avg.avg_lines.end ());

              if (avg.nfanoave > 0 && avg.nfanoave >= 2)
                {
                  bool const is_decoded = (avg.avemsg != blank_message ());
                  QByteArray cf = compute_cflags (is_decoded, false, 99.0f,
                                                  true, avg.nfanoave, avg.avemsg);
                  QString line = format_sync_line (nutc, nsnr, dtx_cur, nfreq_cur,
                                                   csync, avg.avemsg, cf);
                  line = trim_and_format (line);
                  if (!line.isEmpty ())
                    lines << line;
                  prtavg = true;
                }
              else
                {
                  if (avg.qave > qabest)
                    {
                      qabest   = avg.qave;
                      dtx1     = avg.dtave;
                      nfreq1   = avg.nfreqave;
                      deepave1 = avg.deepave;
                      state.ich1 = avg.ichbest;
                      ndeepave = avg.ndeepave;
                    }
                }
            }
        }
    }

  // ── Post-loop: report best single deep decode ─────────────────────────────
  {
    float      dtx_post     = dtx0;
    QByteArray decoded_post = deepmsg0;
    float      qual_post    = qbest;
    (void) nfreq0;
    (void) ich0;

    if (qual_post > 0.0f)
      nsnr = compute_jt4_snr (dat, npts, mode4, dtx_post, decoded_post);

    if (static_cast<int> (qual_post) >= nq1)
      {
        bool const is_decoded = (decoded_post != blank_message ());
        QByteArray cf =
            compute_cflags (is_decoded, true, qual_post, false, 0, decoded_post);
        QString line = format_sync_line (nutc, nsnr, dtx_post, nfreqz, csync,
                                         decoded_post, cf);
        line = trim_and_format (line);
        if (!line.isEmpty ())
          lines << line;
      }
    else
      {
        QByteArray blank = blank_message ();
        QByteArray cf {"   ", 3};
        QString line = format_sync_line (nutc, nsnr, dtxz, nfreqz, csync, blank, cf);
        line = trim_and_format (line);
        if (!line.isEmpty ())
          lines << line;
      }
  }

  // ── Post-loop: report best average deep decode ────────────────────────────
  if (ndeepave >= 2)
    {
      QByteArray deepave_post = deepave1;
      if (static_cast<int> (qabest) >= nq1)
        {
          bool const is_decoded = (deepave_post != blank_message ());
          QByteArray cf = compute_cflags (is_decoded, true, qabest,
                                          true, ndeepave, deepave_post);
          QString line = format_sync_line (nutc, nsnr, dtx1, nfreq1, csync,
                                           deepave_post, cf);
          line = trim_and_format (line);
          if (!line.isEmpty ())
            lines << line;
        }
    }

  write_avg_lines (tempDir, all_avg_lines);
  return lines;
}

}  // namespace

// ── Public API ───────────────────────────────────────────────────────────────

namespace decodium
{
namespace jt4
{

QStringList decode_async_jt4 (legacyjt::DecodeRequest const& request, AverageState* state)
{
  if (!state)
    return {};

  int const submode = std::max (0, std::min (request.nsubmode, 6));
  int const mode4   = kJt4Nch[static_cast<std::size_t> (submode)];

  QByteArray mycall  = request.mycall.left (12);
  if (mycall.size () < 12)
    mycall.append (QByteArray (12 - mycall.size (), ' '));
  QByteArray hiscall = request.hiscall.left (12);
  if (hiscall.size () < 12)
    hiscall.append (QByteArray (12 - hiscall.size (), ' '));
  QByteArray hisgrid = request.hisgrid.left (6);
  if (hisgrid.size () < 6)
    hisgrid.append (QByteArray (6 - hisgrid.size (), ' '));

  // max wav11 output = 52*11025 = 573300; max lpf1 output = jz/2 = 286650
  constexpr int kMaxDd = 52 * 11025 + 1;
  if (static_cast<int> (state->cached_dd.size ()) < kMaxDd)
    state->cached_dd.assign (static_cast<std::size_t> (kMaxDd), 0.0f);

  // ── Step 1: wav11 — resample 12000 Hz int16 → 11025 Hz float ──────────────
  int jz = kMaxSamples12k;
  if (request.newdat != 0)
    {
      std::vector<short> d2 (static_cast<std::size_t> (kMaxSamples12k), 0);
      int const copyCount = std::min<int> (request.audio.size (), kMaxSamples12k);
      if (copyCount > 0)
        std::copy_n (request.audio.constBegin (), copyCount, d2.begin ());
      wav11_cpp (d2.data (), jz, state->cached_dd.data (), jz);
      // jz is now npts_out ≈ 52*11025 = 573300
    }
  else
    {
      jz = 52 * 11025;
    }

  // ── Step 2: lpf1 — lowpass + decimate by 2 (→ 5512.5 Hz) ─────────────────
  std::vector<float> dat (static_cast<std::size_t> (jz), 0.0f);
  int jz2 = 0;
  lpf1_cpp (state->cached_dd.data (), jz, dat.data (), jz2);

  // ── Step 3: wsjt4 orchestration ───────────────────────────────────────────
  bool const nclearave = (request.nclearave != 0);
  int  const neme      = 0;
  QString const tempDir = QString::fromLocal8Bit (request.tempDir);
  QString const dataDir = request.dataDir.isEmpty ()
                              ? QDir::currentPath ()
                              : QString::fromLocal8Bit (request.dataDir);

  return wsjt4_impl (dat.data (), jz2,
                     request.nutc, request.nfqso, request.ntol, request.ndepth,
                     request.minsync, request.minw, mode4, neme, nclearave,
                     mycall, hiscall, hisgrid, tempDir, dataDir, *state);
}

}  // namespace jt4
}  // namespace decodium
