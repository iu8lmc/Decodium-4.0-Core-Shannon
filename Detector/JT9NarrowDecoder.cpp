// JT9 narrow RX decoder — fully C++, no Fortran calls.
// sync9 and softsym are both pure C++ (fully reentrant, no save-state Fortran).
#include "Detector/JT9NarrowDecoder.hpp"
#include "Detector/JT9FastDecoder.hpp"
#include "Detector/LegacyJtDecodeWorker.hpp"
#include "commons.h"
#include "wsjtx_config.h"

#include <QByteArray>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <fftw3.h>

// ── Downsam9State destructor ──────────────────────────────────────────────────
namespace decodium { namespace jt9narrow {

Downsam9State::~Downsam9State () noexcept
{
  if (plan_r2c)
    {
      fftwf_destroy_plan (static_cast<fftwf_plan> (plan_r2c));
      plan_r2c = nullptr;
    }
}

}} // namespace decodium::jt9narrow

namespace
{

// ── jt9sync constants (from jt9sync.f90 include) ─────────────────────────────

// isync[i-1] = 1 for sync symbols, 0 for data; 85 entries.
constexpr std::array<int, 85> kIsync {{
  1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,
  0,0,1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,0,1,
  0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
  0,0,1,0,1
}};

// ii[i-1]: 1-based positions of sync symbols (16 of them), 1-indexed.
constexpr std::array<int, 16> kIi {{
  1,2,5,10,16,23,33,35,51,52,55,60,66,73,83,85
}};

// ── twkfreq_cpp ───────────────────────────────────────────────────────────────
// Port of twkfreq.f90: mix complex signal by a(1) + chirp a(2) + a(3) terms.
// a[0]=f0 (Hz), a[1]=fDot (Hz per half-duration), a[2]=extra phase term.
// c4 = c3 * exp(i*phi(t)), can be in-place (c4 == c3).
void twkfreq_cpp (std::complex<float> const* c3,
                  std::complex<float>*       c4,
                  int                        npts,
                  float                      fsample,
                  float const                a[3])
{
  constexpr float twopi = 6.283185307f;
  std::complex<float> w {1.0f, 0.0f};
  float const x0 = 0.5f * static_cast<float> (npts + 1);
  float const s  = 2.0f / static_cast<float> (npts);

  for (int i = 1; i <= npts; ++i)
    {
      float const x   = s * static_cast<float> (i) - s * x0;
      float const p2  = 1.5f * x * x - 0.5f;
      float const dphi = (a[0] + x * a[1] + p2 * a[2]) * (twopi / fsample);
      float const cs   = std::cos (dphi);
      float const sn   = std::sin (dphi);
      w = w * std::complex<float> {cs, sn};
      c4[i - 1] = w * c3[i - 1];
    }
}

// ── fchisq_cpp ───────────────────────────────────────────────────────────────
// Port of fchisq.f90: compute -sync_power after applying twkfreq.
// Returns -(sum1/16) / (sum0/69 + eps) * sum1/10000, i.e. -sync_4993.
float fchisq_cpp (std::complex<float> const* c3, int npts,
                  float fsample, float const a[3])
{
  constexpr int nspsd = 16;
  std::vector<std::complex<float>> c4 (static_cast<std::size_t> (npts));
  twkfreq_cpp (c3, c4.data (), npts, fsample, a);

  float sum1 = 0.0f;
  int k = -1;
  for (int i = 1; i <= 85; ++i)
    {
      std::complex<float> z {0.0f, 0.0f};
      for (int j = 1; j <= nspsd; ++j)
        {
          ++k;
          z += c4[static_cast<std::size_t> (k)];
        }
      if (kIsync[static_cast<std::size_t> (i - 1)])
        sum1 += std::norm (z);
    }
  return -(sum1 / 10000.0f);  // fchisq = -sync_4993 (minimized → maximized sync)
}

// ── afc9_cpp ──────────────────────────────────────────────────────────────────
// Port of afc9.f90: iterative 3-parameter AFC (frequency, drift, extra DT).
// Minimizes -sync_power using golden-section-style hill-climbing.
// On return: a[0..2] are the corrections, syncpk = best sync score,
// c3a is replaced by its shifted version (as in Fortran "c3a=c3" at end).
void afc9_cpp (std::complex<float>* c3a, int npts, float fsample,
               float a[3], float& syncpk)
{
  float delta[3] = {1.736f, 1.736f, 1.0f};
  a[0] = 0.0f; a[1] = 0.0f; a[2] = 0.0f;

  // c3 is the working copy of c3a (shifted by shft); c3a is never modified until the end.
  std::vector<std::complex<float>> c3 (c3a, c3a + npts);
  float a3_cur = 0.0f;  // shift currently applied to c3

  // shft(c3a, a3new, a3_cur, c3): rebuild c3 = shifted c3a.
  // Mirrors Fortran shft.f90: for n>0, non-circular left-shift + zero tail;
  // for n<0, circular right-shift (c3(:n-1)=0 is empty range → no zeroing).
  auto shft = [&] (float a3new) {
    int const n = static_cast<int> (std::lround (a3new));
    a3_cur = a3new;
    if (n == 0)
      {
        std::copy (c3a, c3a + npts, c3.begin ());
        return;
      }
    if (n > 0)
      {
        // non-circular left shift: c3[i] = c3a[i+n], zeros at tail
        int const valid = npts - n;
        if (valid > 0) std::copy (c3a + n, c3a + npts, c3.begin ());
        std::fill (c3.begin () + std::max (0, valid), c3.end (), std::complex<float> {});
      }
    else
      {
        // circular right shift: first |n| elements come from end of c3a
        int const abs_n = -n;
        std::copy (c3a + npts - abs_n, c3a + npts, c3.begin ());
        std::copy (c3a, c3a + npts - abs_n, c3.begin () + abs_n);
      }
  };

  float chisqr0 = 1.0e6f;

  for (int iter = 0; iter < 4; ++iter)
    {
      for (int j = 0; j < 3; ++j)
        {
          if (a[2] != a3_cur)
            shft (a[2]);
          float chisq1 = fchisq_cpp (c3.data (), npts, fsample, a);
          float fn = 0.0f;
          float dj = delta[j];

          // Find downhill direction
          a[j] += dj;
          if (a[2] != a3_cur) shft (a[2]);
          float chisq2 = fchisq_cpp (c3.data (), npts, fsample, a);
          if (chisq2 == chisq1)
            {
              a[j] += dj;
              if (a[2] != a3_cur) shft (a[2]);
              chisq2 = fchisq_cpp (c3.data (), npts, fsample, a);
            }
          if (chisq2 > chisq1)
            {
              dj = -dj;
              a[j] += dj;
              std::swap (chisq1, chisq2);
            }
          // Walk downhill
          for (;;)
            {
              fn += 1.0f;
              a[j] += dj;
              if (a[2] != a3_cur) shft (a[2]);
              float const chisq3 = fchisq_cpp (c3.data (), npts, fsample, a);
              if (chisq3 >= chisq2)
                {
                  // Parabolic interpolation to find minimum
                  float const denom = chisq3 - chisq2;
                  if (std::fabs (denom) > 1e-30f)
                    dj = dj * (1.0f / (1.0f + (chisq1 - chisq2) / denom) + 0.5f);
                  a[j] -= dj;
                  break;
                }
              chisq1 = chisq2;
              chisq2 = chisq3;
            }
          if (j < 2)
            delta[j] = delta[j] * fn / 3.0f;
        }

      if (a[2] != a3_cur) shft (a[2]);
      float const chisqr = fchisq_cpp (c3.data (), npts, fsample, a);
      if (chisqr / chisqr0 > 0.99f)
        break;
      chisqr0 = chisqr;
    }

  if (a[2] != a3_cur) shft (a[2]);
  syncpk = -fchisq_cpp (c3.data (), npts, fsample, a);

  // Fortran "c3a=c3": copy final shifted c3 back into c3a
  std::copy (c3.begin (), c3.end (), c3a);
}

// ── peakdt9_cpp ──────────────────────────────────────────────────────────────
// Port of peakdt9.f90: find timing peak, extract aligned c3.
void peakdt9_cpp (std::complex<float> const* c2, int nsps8, int nspsd,
                  std::complex<float>* c3, float& xdt)
{
  constexpr int NZ2 = 1512;
  constexpr int NZ3 = 1360;

  // Integrated symbol power
  int const p_size = 5 * nspsd + NZ2;
  std::vector<float> p (static_cast<std::size_t> (p_size + 1), 0.0f);
  int const i0 = 5 * nspsd;
  for (int i = 0; i < NZ2; ++i)
    {
      // running sum of last nspsd samples
      int const lo = std::max (i - (nspsd - 1), 0);
      std::complex<float> z {0.0f, 0.0f};
      for (int n = lo; n <= i; ++n)
        z += c2[static_cast<std::size_t> (n)];
      z *= 1.0e-3f;
      p[static_cast<std::size_t> (i0 + i)] = std::norm (z);
    }

  // getlags(nsps8): lookup table
  int lag0, lag1, lag2;
  if      (nsps8 == 864)   { lag1 = 39;  lag2 = 291; lag0 = 123; }
  else if (nsps8 == 1920)  { lag1 = 70;  lag2 = 184; lag0 = 108; }
  else if (nsps8 == 5120)  { lag1 = 84;  lag2 = 129; lag0 = 99;  }
  else if (nsps8 == 10368) { lag1 = 91;  lag2 = 112; lag0 = 98;  }
  else                     { lag1 = 93;  lag2 = 102; lag0 = 96;  }

  float const tsymbol = static_cast<float> (nsps8) / 1500.0f;
  float const dtlag   = tsymbol / static_cast<float> (nspsd);

  float smax = 0.0f;
  int lagpk = lag0;

  for (int lag = lag1; lag <= lag2; ++lag)
    {
      float sum0 = 0.0f, sum1 = 0.0f;
      int j = -nspsd;
      for (int ii = 1; ii <= 85; ++ii)
        {
          j += nspsd;
          int const idx = j + lag;
          if (idx >= 0 && idx < p_size)
            {
              float const pv = p[static_cast<std::size_t> (idx)];
              if (kIsync[static_cast<std::size_t> (ii - 1)])
                sum1 += pv;
              else
                sum0 += pv;
            }
        }
      float const ss = (sum1 / 16.0f) / (sum0 / 69.0f) - 1.0f;
      if (ss > smax)
        {
          smax  = ss;
          lagpk = lag;
        }
    }

  xdt = static_cast<float> (lagpk - lag0) * dtlag;

  // Copy c2 → c3 at best lag (NZ3 = 1360 samples)
  for (int i = 0; i < NZ3; ++i)
    {
      int const j = i + lagpk - i0 - nspsd + 1;
      if (j >= 0 && j < NZ2)
        c3[static_cast<std::size_t> (i)] = c2[static_cast<std::size_t> (j)];
      else
        c3[static_cast<std::size_t> (i)] = {0.0f, 0.0f};
    }
}

// ── chkss2_cpp ────────────────────────────────────────────────────────────────
// Port of chkss2.f90: compute schk = mean sync-tone power at tone 0.
float chkss2_cpp (float const ss2[9][85])
{
  float ave = 0.0f;
  for (int i = 0; i < 9; ++i)
    for (int j = 0; j < 85; ++j)
      ave += ss2[i][j];
  ave /= (9 * 85);

  float s1 = 0.0f;
  for (int ii = 0; ii < 16; ++ii)
    {
      int const j = kIi[static_cast<std::size_t> (ii)] - 1;  // 0-indexed
      if (j < 85)
        s1 += ss2[0][j] / ave - 1.0f;
    }
  return s1 / 16.0f;
}

// ── pctile_narrow ─────────────────────────────────────────────────────────────
// Mirrors pctile.f90: j = nint(0.01*p*n); return sorted_x[j-1].
float pctile_narrow (float const* x, int n, int p)
{
  std::vector<float> tmp (x, x + n);
  std::sort (tmp.begin (), tmp.end ());
  int j = static_cast<int> (std::lround (0.01f * static_cast<float> (p * n)));
  if (j < 1) j = 1;
  if (j > n) j = n;
  return tmp[static_cast<std::size_t> (j - 1)];
}

// ── symspec2_cpp ──────────────────────────────────────────────────────────────
// Port of symspec2.f90: compute 207 soft symbols from downsampled IQ data.
// c5[0..NZ3-1]: complex IQ at 16 samples/symbol; mutated during processing.
// Outputs i1SoftSymbolsScrambled[0..206] (int8, still interleaved).
void symspec2_cpp (std::complex<float>* c5, int nz3, int nsps8, int nspsd,
                   float fsample, float /*freq*/, float /*drift*/,
                   float& snrdb, float& schk,
                   std::array<std::int8_t, 207>& scrambled)
{
  constexpr float scale = 10.0f;

  // aa: frequency step between tones = -1500/nsps8 Hz per application of twkfreq
  float aa[3] = {-1500.0f / static_cast<float> (nsps8), 0.0f, 0.0f};

  // ss2[tone 0..8][symbol 1..85] and ss3[tone 1..8 = 0..7][data_symbol 1..69]
  // Using row-major C arrays to match Fortran ss2(0:8,85) / ss3(0:7,69)
  float ss2[9][85];
  float ss3[8][69];
  std::memset (ss2, 0, sizeof (ss2));
  std::memset (ss3, 0, sizeof (ss3));

  std::vector<std::complex<float>> c_work (c5, c5 + nz3);

  for (int tone = 0; tone <= 8; ++tone)
    {
      if (tone >= 1)
        {
          // Shift c_work by -1 tone step (in-place: aa applied cumulatively)
          twkfreq_cpp (c_work.data (), c_work.data (), nz3, fsample, aa);
        }

      int m = 0;
      int k = -1;
      for (int j = 1; j <= 85; ++j)
        {
          std::complex<float> z {0.0f, 0.0f};
          for (int n = 1; n <= nspsd; ++n)
            {
              ++k;
              z += c_work[static_cast<std::size_t> (k)];
            }
          ss2[tone][j - 1] = std::norm (z);
          if (tone >= 1 && kIsync[static_cast<std::size_t> (j - 1)] == 0)
            {
              ss3[tone - 1][m] = ss2[tone][j - 1];
              ++m;
            }
        }
    }

  // schk: sync check
  schk = chkss2_cpp (ss2);

  // Baseline / SNR
  float ss_total = 0.0f, sig = 0.0f;
  for (int j = 0; j < 69; ++j)
    {
      float smax = 0.0f;
      for (int i = 0; i < 8; ++i)
        {
          if (ss3[i][j] > smax) smax = ss3[i][j];
          ss_total += ss3[i][j];
        }
      sig      += smax;
      ss_total -= smax;  // subtract max from each column (removes signal)
    }
  float const ave = ss_total / (69.0f * 7.0f);

  // Use ave to normalise pctile (mirrors pctile(ss2, 9*85, 35, xmed) — result unused)
  // Actually symspec2 doesn't use xmed further — the call is just for side-effect in Fortran.
  // We skip it since the result isn't used.

  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 69; ++j)
      ss3[i][j] /= ave;

  float const t = std::max (1.0f, sig / 69.0f - 1.0f);
  snrdb = 10.0f * std::log10 (t) - 61.3f;

  // Bit-wise soft symbols (3 bits per data symbol = 207 total)
  int k2 = 0;
  for (int j = 0; j < 69; ++j)
    {
      for (int m_bit = 2; m_bit >= 0; --m_bit)
        {
          float r1, r0;
          if (m_bit == 2)
            {
              r1 = std::max ({ss3[4][j], ss3[5][j], ss3[6][j], ss3[7][j]});
              r0 = std::max ({ss3[0][j], ss3[1][j], ss3[2][j], ss3[3][j]});
            }
          else if (m_bit == 1)
            {
              r1 = std::max ({ss3[2][j], ss3[3][j], ss3[4][j], ss3[5][j]});
              r0 = std::max ({ss3[0][j], ss3[1][j], ss3[6][j], ss3[7][j]});
            }
          else
            {
              r1 = std::max ({ss3[1][j], ss3[2][j], ss3[4][j], ss3[7][j]});
              r0 = std::max ({ss3[0][j], ss3[3][j], ss3[5][j], ss3[6][j]});
            }
          int i4 = static_cast<int> (std::lround (scale * (r1 - r0)));
          if (i4 < -127) i4 = -127;
          if (i4 >  127) i4 =  127;
          scrambled[static_cast<std::size_t> (k2)] = static_cast<std::int8_t> (i4);
          ++k2;
        }
    }
}

// ── interleave9_deinterleave ──────────────────────────────────────────────────
// De-interleave 207 int8 symbols: ib[i] = ia[j0[i]] (ndir=-1 path of interleave9.f90).
void interleave9_deinterleave (std::array<std::int8_t, 207> const& ia,
                               std::array<std::int8_t, 207>&       ib)
{
  // Build j0 table (same as interleave9_order in LegacyJtEncoder.hpp)
  static std::array<int, 206> const j0 = [] {
    std::array<int, 206> order {};
    int k = 0;
    for (int i = 0; i < 256; ++i)
      {
        int m = i, n = 0;
        for (int b = 0; b < 8; ++b)
          { n = (n << 1) | (m & 1); m >>= 1; }
        if (n <= 205)
          order[static_cast<std::size_t> (k++)] = n;
      }
    return order;
  }();

  for (int i = 0; i < 206; ++i)
    ib[static_cast<std::size_t> (i)] = ia[static_cast<std::size_t> (j0[static_cast<std::size_t> (i)])];
}

// ── downsam9_cpp ──────────────────────────────────────────────────────────────
// Port of downsam9.f90: mix to baseband, decimate NFFT1 → NFFT2 via FFT window.
// Mirrors save state in Downsam9State; only redoes big FFT when newdat==true.
void downsam9_cpp (short const* id2, int npts8, int nsps8,
                   bool& newdat, int nspsd, float fpk,
                   std::complex<float>* c2,
                   decodium::jt9narrow::Downsam9State& st)
{
  constexpr int NFFT1 = decodium::jt9narrow::Downsam9State::kNfft1;  // 653184
  constexpr int NFFT2 = decodium::jt9narrow::Downsam9State::kNfft2;  // 1512
  constexpr int NC1   = decodium::jt9narrow::Downsam9State::kNc1;    // 326593

  float const df1 = 12000.0f / static_cast<float> (NFFT1);  // ≈ 0.018367 Hz/bin
  int   const npts = std::min (8 * npts8, NFFT1);

  // ── one-time FFTW plan creation ────────────────────────────────────────────
  if (!st.initialized)
    {
      st.x1.assign (NFFT1, 0.0f);
      st.c1.assign (NC1, {0.0f, 0.0f});
      st.s.assign  (5000, 0.0f);
      st.plan_r2c = fftwf_plan_dft_r2c_1d (
          NFFT1,
          st.x1.data (),
          reinterpret_cast<fftwf_complex*> (st.c1.data ()),
          FFTW_ESTIMATE);
      st.initialized = true;
    }

  // ── big FFT (only when new audio data) ────────────────────────────────────
  if (newdat)
    {
      std::fill (st.x1.begin (), st.x1.end (), 0.0f);
      for (int i = 0; i < npts; ++i)
        st.x1[static_cast<std::size_t> (i)] = static_cast<float> (id2[i]);

      fftwf_execute_dft_r2c (
          static_cast<fftwf_plan> (st.plan_r2c),
          st.x1.data (),
          reinterpret_cast<fftwf_complex*> (st.c1.data ()));

      // Build power spectrum s[0..4999] (1-indexed in Fortran, hence s[i-1])
      int const nadd = static_cast<int> (1.0f / df1);  // 54
      std::fill (st.s.begin (), st.s.end (), 0.0f);
      for (int i = 1; i <= 5000; ++i)
        {
          int j = static_cast<int> (static_cast<float> (i - 1) / df1);
          for (int n = 0; n < nadd; ++n)
            {
              ++j;
              if (j < NC1)
                st.s[static_cast<std::size_t> (i - 1)] += std::norm (st.c1[static_cast<std::size_t> (j)]);
            }
        }
      newdat = false;
    }

  // ── extract frequency window + pctile normalisation ───────────────────────
  int const nf  = static_cast<int> (std::lround (fpk));
  int const i0  = static_cast<int> (fpk / df1);
  int const ia  = std::max (1, nf - 100) - 1;   // 0-indexed
  int const ib  = std::min (5000, nf + 100) - 1; // 0-indexed
  float const avenoise = pctile_narrow (st.s.data () + ia, ib - ia + 1, 40);
  float const fac = (avenoise > 0.0f) ? std::sqrt (1.0f / avenoise) : 1.0f;

  int const nh2 = NFFT2 / 2;  // 756
  for (int i = 0; i < NFFT2; ++i)
    {
      int j = i0 + i;
      if (i > nh2) j -= NFFT2;
      if (j >= 0 && j < NC1)
        c2[i] = fac * st.c1[static_cast<std::size_t> (j)];
      else
        c2[i] = {0.0f, 0.0f};
    }

  // ── c2c IFFT matching the FFT-compat path; no normalisation ───────────────
  {
    fftwf_plan p = fftwf_plan_dft_1d (
        NFFT2,
        reinterpret_cast<fftwf_complex*> (c2),
        reinterpret_cast<fftwf_complex*> (c2),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    fftwf_execute (p);
    fftwf_destroy_plan (p);
  }

  (void) nsps8; (void) nspsd;
}

// ── softsym_cpp ───────────────────────────────────────────────────────────────
// C++ replacement for softsym_.
// Outputs: syncpk, snrdb, xdt, freq (corrected), drift, a3, schk,
//          i1SoftSymbols[0..206] (de-interleaved int8 soft bits).
void softsym_cpp (short const* id2, int npts8, int nsps8,
                  bool& newdat, float fpk,
                  float& syncpk, float& snrdb, float& xdt,
                  float& freq,   float& drift, float& a3, float& schk,
                  std::array<std::int8_t, 207>& i1SoftSymbols,
                  decodium::jt9narrow::Downsam9State& ds_state)
{
  constexpr int NZ2   = 1512;
  constexpr int NZ3   = 1360;
  constexpr int nspsd = 16;

  int const ndown     = 8 * nsps8 / nspsd;  // = 432 for nsps8=864
  float const fsample = 1500.0f / static_cast<float> (ndown);

  std::vector<std::complex<float>> c2 (NZ2);
  std::vector<std::complex<float>> c3 (NZ3);
  std::vector<std::complex<float>> c5 (NZ3);

  // Step 1: mix + decimate to nspsd samples/symbol
  downsam9_cpp (id2, npts8, nsps8, newdat, nspsd, fpk, c2.data (), ds_state);

  // Step 2: find timing peak → c3 (NZ3 aligned samples)
  peakdt9_cpp (c2.data (), nsps8, nspsd, c3.data (), xdt);

  // Step 3: AFC → corrected freq, drift, a3; c3a modified in-place
  float afc[3] = {0.0f, 0.0f, 0.0f};
  afc9_cpp (c3.data (), NZ3, fsample, afc, syncpk);
  freq  = fpk - afc[0];
  drift = -2.0f * afc[1];
  a3    = afc[2];
  afc[2] = 0.0f;

  // Step 4: frequency correction → c5
  twkfreq_cpp (c3.data (), c5.data (), NZ3, fsample, afc);

  // Step 5: compute scrambled soft symbols
  std::array<std::int8_t, 207> scrambled {};
  symspec2_cpp (c5.data (), NZ3, nsps8, nspsd, fsample, freq, drift,
                snrdb, schk, scrambled);

  // Step 6: de-interleave
  interleave9_deinterleave (scrambled, i1SoftSymbols);
}

constexpr int   kNsmax   = NSMAX;                        // 6827
constexpr int   kNsps    = 6912;                         // samples per symbol, JT9-1
constexpr float kDf3     = 1500.0f / 2048.0f;           // Hz per spectrum bin
constexpr int   kLag1    = -9;                           // -int(2.5/kTstep + 0.9999)
constexpr int   kLag2    = 18;                           // int(5.0/kTstep + 0.9999)
constexpr int   kNsps8   = kNsps / 8;                   // 864
constexpr int   kAudioSz = NTMAX * 12000;               // max audio buffer (60 s)
constexpr int   kSsSz    = 184 * NSMAX;                 // ss spectrum array size

// Return the p-th percentile (p in 0..100) of a[0..n-1] without modifying a.
// Mirrors pctile.f90: j = nint(0.01*p*n); xp = sorted_a[j-1].
float pctile (float const* a, int n, int p)
{
  std::vector<float> b (a, a + n);
  std::sort (b.begin (), b.end ());
  int j = static_cast<int> (std::lround (0.01f * static_cast<float> (p) * static_cast<float> (n)));
  if (j < 1) j = 1;
  if (j > n) j = n;
  return b[static_cast<std::size_t> (j - 1)];
}

// C++ port of sync9.f90.
// ss[184*NSMAX]: spectrum matrix stored column-major (freq bin outer, half-sym inner).
//   Element (half_sym h, freq_bin i) at offset (i-1)*184 + (h-1)  [1-indexed].
// Outputs ccfred[NSMAX] and red2[NSMAX] exactly as the Fortran subroutine does.
// No Fortran save-state: fully reentrant.
void sync9_cpp (float const* ss, int nzhsym,
                int lag1, int lag2, int ia, int ib,
                float* ccfred, float* red2, int* ipkbest)
{
  // 16 sync half-symbol positions (1-indexed), from jt9sync.f90
  static constexpr int kSyncPos[16] = {1,3,9,19,31,45,65,69,101,103,109,119,131,145,165,169};

  *ipkbest = 0;
  float sbest = 0.0f;
  std::fill (ccfred, ccfred + kNsmax, 0.0f);

  // --- Pass 1: sync correlation per frequency bin ---
  std::vector<float> ss1 (184);
  for (int i = ia; i <= ib; ++i)
    {
      // Extract column i (1-indexed) from the 184×NSMAX column-major matrix
      float const* col = ss + static_cast<std::size_t> (i - 1) * 184;
      std::copy (col, col + 184, ss1.begin ());

      // Normalise: divide by 40th percentile of the active rows, subtract 1
      float xmed = pctile (ss1.data (), nzhsym, 40);
      if (xmed <= 0.0f) xmed = 1.0f;
      for (int j = 0; j < 184; ++j)
        ss1[j] = ss1[j] / xmed - 1.0f;

      // Clip active rows at 3.0
      for (int j = 0; j < nzhsym; ++j)
        if (ss1[j] > 3.0f) ss1[j] = 3.0f;

      // Subtract 45th-percentile baseline
      float const sbase = pctile (ss1.data (), nzhsym, 45);
      for (int j = 0; j < 184; ++j)
        ss1[j] -= sbase;

      // Best sync correlation over all lags
      float smax = 0.0f;
      for (int lag = lag1; lag <= lag2; ++lag)
        {
          float sum1 = 0.0f;
          for (int s = 0; s < 16; ++s)
            {
              int const k = kSyncPos[s] + lag;   // 1-indexed
              if (k >= 1 && k <= nzhsym)
                sum1 += ss1[k - 1];
            }
          if (sum1 > smax) smax = sum1;
        }

      ccfred[i - 1] = smax;
      if (smax > sbest)
        {
          sbest = smax;
          *ipkbest = i;
        }
    }

  // Normalise ccfred by its median over [ia..ib]
  float xmed2 = pctile (ccfred + ia - 1, ib - ia + 1, 50);
  if (xmed2 <= 0.0f) xmed2 = 1.0f;
  for (int i = 0; i < kNsmax; ++i)
    ccfred[i] = 2.0f * ccfred[i] / xmed2;

  // --- Pass 2: spectral average + bandpass filter → red2 ---

  // savg[i] = time-average of spectrum column i  (only bins ia..ib)
  std::vector<float> savg (kNsmax, 0.0f);
  for (int i = ia; i <= ib; ++i)
    {
      float const* col = ss + static_cast<std::size_t> (i - 1) * 184;
      float s = 0.0f;
      for (int j = 0; j < nzhsym; ++j)
        s += col[j];
      savg[i - 1] = s / static_cast<float> (nzhsym);
    }

  // smo bandpass: +1/21 for j in [0,20], -1/10 for j in [-5,-1] and [21,25]
  // (indices relative to i, from jt9sync.f90 / sync9.f90)
  static constexpr float kSmoPos = 1.0f / 21.0f;
  static constexpr float kSmoNeg = -(1.0f / 21.0f) * (21.0f / 10.0f);  // = -1/10
  auto smo = [](int j) -> float {
    if (j >= 0 && j <= 20)  return kSmoPos;
    if ((j >= -5 && j <= -1) || (j >= 21 && j <= 25)) return kSmoNeg;
    return 0.0f;
  };

  std::vector<float> savg2 (kNsmax, 0.0f);
  for (int i = ia; i <= ib; ++i)
    {
      float sm = 0.0f;
      for (int j = -5; j <= 25; ++j)
        {
          int const idx = i + j;   // 1-indexed Fortran
          if (idx >= 1 && idx < kNsmax)
            sm += smo (j) * savg[idx - 1];
        }
      savg2[i - 1] = sm;
    }

  // Normalise savg2 by sqrt of 20th percentile of sq = savg2^2  over [ia..ib]
  std::vector<float> sq (ib - ia + 1);
  for (int i = ia; i <= ib; ++i)
    sq[static_cast<std::size_t> (i - ia)] = savg2[i - 1] * savg2[i - 1];
  float const sq0 = pctile (sq.data (), ib - ia + 1, 20);
  float rms = std::sqrt (sq0);
  if (rms <= 0.0f) rms = 1.0f;
  for (int i = ia; i <= ib; ++i)
    savg2[i - 1] /= (5.0f * rms);

  // red2[i] = savg2[i] - max(savg2[i-10], savg2[i+10]), clamped to ±99
  std::fill (red2, red2 + kNsmax, 0.0f);
  for (int i = ia + 11; i <= ib - 10; ++i)
    {
      float const ref = std::max (savg2[i - 10 - 1], savg2[i + 10 - 1]);
      float val = savg2[i - 1] - ref;
      if (val < -99.0f) val = -99.0f;
      if (val >  99.0f) val =  99.0f;
      red2[i - 1] = val;
    }
}

QString format_jt9_row (int nutc, int nsnr, float dt, float freq, QByteArray const& decoded)
{
  QByteArray const fixed = decoded.leftJustified (22, ' ', true).left (22);
  char prefix[64] {};
  std::snprintf (prefix, sizeof prefix, "%04d%4d%5.1f%5d @  ",
                 nutc, nsnr, static_cast<double> (dt), static_cast<int> (std::lround (freq)));
  QByteArray row {prefix};
  row.append (fixed);
  // Strip trailing whitespace (mirrors LegacyJtDecodeWorker row trimming)
  while (!row.isEmpty ())
    {
      char const c = row.back ();
      if (c == ' ' || c == '\0' || c == '\r' || c == '\n' || c == '\t')
        {
          row.chop (1);
          continue;
        }
      break;
    }
  return QString::fromLatin1 (row);
}

}  // namespace

namespace decodium
{
namespace jt9narrow
{

// Mirrors jt9_decoder%decode (jt9_decode.f90) for nmode=9, nsubmode=0.
// Fully C++ — no Fortran calls.
QStringList decode_async_jt9_narrow (legacyjt::DecodeRequest const& request, CorrState* state)
{
  if (!state)
    return {};

  // Lazily initialise correlation state arrays
  if (static_cast<int> (state->ccfred.size ()) != kNsmax)
    {
      state->ccfred.assign (kNsmax, 0.0f);
      state->red2.assign (kNsmax, 0.0f);
    }

  // Build ss spectrum buffer (184 × NSMAX, Fortran column-major)
  std::vector<float> ss (kSsSz, 0.0f);
  if (!request.ss.isEmpty ())
    std::copy_n (request.ss.constBegin (), std::min ((int)request.ss.size (), kSsSz), ss.begin ());

  // Build audio buffer
  std::vector<short> id2 (kAudioSz, 0);
  if (request.newdat != 0 && !request.audio.isEmpty ())
    {
      int const n = std::min ((int)request.audio.size (), kAudioSz);
      std::copy_n (request.audio.constBegin (), n, id2.begin ());
    }

  // Frequency range bounds
  int nfqso  = std::max (0, std::min (request.nfqso, 5000));
  int ntol   = std::max (0, request.ntol);
  int nfa    = std::max (0, std::min (request.nfa,  5000));
  int nfb    = std::max (nfa + 50, std::min (request.nfb, 5000));
  int nzhsym = std::max (0, request.nzhsym);
  int npts8  = std::max (0, request.npts8);
  bool const nagain = (request.nagain != 0);
  int  const ndepth = request.ndepth;
  int  const nutc   = request.nutc;

  // Frequency bin limits for the full search range
  int ia_global = std::max (1, static_cast<int> (std::lround (nfa / kDf3)));
  int ib_global = std::min (kNsmax, static_cast<int> (std::lround (nfb / kDf3)));

  // Step 1: sync9 — compute ccfred and red2 (only when new data arrived)
  if (request.newdat != 0)
    {
      int ipk = 0;
      sync9_cpp (ss.data (), nzhsym,
                 kLag1, kLag2, ia_global, ib_global,
                 state->ccfred.data (), state->red2.data (), &ipk);
    }

  // Step 2: two-pass candidate loop
  // nqd=1: focused pass around nfqso±ntol (looser thresholds)
  // nqd=0: broad pass over full nfa..nfb (stricter thresholds, exclude focused region)
  std::vector<bool> done (kNsmax, false);
  int ia1 = 1, ib1 = 1;

  // newdat_softsym: 1 for first call (triggers big FFT in downsam9_cpp), then 0.
  int newdat_softsym = (request.newdat != 0) ? 1 : 0;
  int nsps8 = kNsps8;
  float const df8 = 1500.0f / static_cast<float> (kNsps8);  // ~1.736 Hz / bin

  QStringList lines;

  for (int nqd = 1; nqd >= 0; --nqd)
    {
      // Threshold parameters vary with ndepth and pass index
      int   limit   = 5000;
      float ccflim  = 3.0f;
      float red2lim = 1.6f;
      float schklim = 2.2f;

      if ((ndepth & 7) == 2)
        {
          limit  = 10000;
          ccflim = 2.7f;
        }
      if ((ndepth & 7) == 3 || nqd == 1)
        {
          limit   = 30000;
          ccflim  = 2.5f;
          schklim = 2.0f;
        }
      if (nagain)
        {
          limit   = 100000;
          ccflim  = 2.4f;
          schklim = 1.8f;
        }

      // Build candidate mask for this pass
      std::vector<bool> ccfok (kNsmax, false);
      int ia, ib;

      if (nqd == 1)
        {
          // Focused: nfqso±ntol, looser thresholds (ccflim-2, red2lim-1)
          ia = std::max (1, static_cast<int> (std::lround ((nfqso - ntol) / kDf3)));
          ib = std::min (kNsmax, static_cast<int> (std::lround ((nfqso + ntol) / kDf3)));
          for (int i = ia; i <= ib; ++i)
            ccfok[static_cast<std::size_t> (i - 1)] =
                (state->ccfred[static_cast<std::size_t> (i - 1)] > (ccflim - 2.0f))
             && (state->red2[static_cast<std::size_t> (i - 1)] > (red2lim - 1.0f));
          ia1 = ia;
          ib1 = ib;
        }
      else
        {
          // Broad: full nfa..nfb, exclude focused region already tried in nqd=1
          ia = ia_global;
          ib = ib_global;
          for (int i = ia; i <= ib; ++i)
            ccfok[static_cast<std::size_t> (i - 1)] =
                (state->ccfred[static_cast<std::size_t> (i - 1)] > ccflim)
             && (state->red2[static_cast<std::size_t> (i - 1)] > red2lim);
          for (int i = ia1; i <= ib1; ++i)
            ccfok[static_cast<std::size_t> (i - 1)] = false;
        }

      float fgood = 0.0f;

      for (int i = ia; i <= ib; ++i)
        {
          if (done[static_cast<std::size_t> (i - 1)] || !ccfok[static_cast<std::size_t> (i - 1)])
            continue;

          float const f   = (i - 1) * kDf3;
          float const ccf = state->ccfred[static_cast<std::size_t> (i - 1)];

          // For the broad pass, require ccf >= ccflim and a minimum frequency gap
          // from the last decoded candidate (prevents decoding the same signal twice).
          if (nqd != 1 && !(ccf >= ccflim && std::fabs (f - fgood) > 10.0f * df8))
            continue;

          // Extract 207 soft symbols via C++ softsym_cpp
          float fpk    = f;
          float syncpk = 0.0f, snrdb = 0.0f, xdt = 0.0f, freq = 0.0f;
          float drift  = 0.0f, a3   = 0.0f, schk = 0.0f;
          std::array<std::int8_t, 207> soft {};
          bool newdat_bool = (newdat_softsym != 0);

          softsym_cpp (id2.data (), npts8, nsps8, newdat_bool,
                       fpk, syncpk, snrdb, xdt,
                       freq, drift, a3, schk,
                       soft, state->downsam9);

          newdat_softsym = newdat_bool ? 1 : 0;

          // Sync quality gates (mirror jt9_decode.f90 threshold checks)
          float const sync = (syncpk + 1.0f) / 4.0f;
          if (nqd == 1 && (sync < 0.5f || schk < 1.0f))
            continue;
          if (nqd != 1 && (sync < 1.0f || schk < schklim))
            continue;

          // Fano convolutional decode (C++ implementation, reentrant)
          int nlim = 0;
          QByteArray const msg = decodium::jt9fast::decode_soft_symbols (soft, limit, &nlim);

          QByteArray const blank22 (22, ' ');
          if (msg != blank22)
            {
              int const nsnr = static_cast<int> (std::lround (snrdb));
              lines << format_jt9_row (nutc, nsnr, xdt, freq, msg);

              // Mark nearby bins as done to prevent duplicate decodes
              int const iaa = std::max (1, i - 1);
              int const ibb = std::min (kNsmax, i + 22);
              for (int k = iaa; k <= ibb; ++k)
                {
                  done[static_cast<std::size_t> (k - 1)] = true;
                  ccfok[static_cast<std::size_t> (k - 1)] = false;
                }
              fgood = f;
            }
        }

      // With nagain, a single pass is sufficient
      if (nagain)
        break;
    }

  return lines;
}

}  // namespace jt9narrow
}  // namespace decodium
