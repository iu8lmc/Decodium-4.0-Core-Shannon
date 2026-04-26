#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numeric>

namespace
{

using Complex = std::complex<float>;

constexpr float kPi = 3.14159265358979323846f;

template <size_t N>
std::array<Complex, N> apply_tweak (std::array<Complex, N> const& base,
                                    Complex const* tweak,
                                    bool itwk)
{
  if (!itwk)
    {
      return base;
    }

  std::array<Complex, N> result {};
  for (size_t i = 0; i < N; ++i)
    {
      result[i] = tweak[i] * base[i];
    }
  return result;
}

std::array<Complex, 64> make_sync4 (std::array<int, 4> const& sequence)
{
  std::array<Complex, 64> result {};
  constexpr float kSync4PhaseSamples = 16.0f;
  int k = 0;
  float phi = 0.0f;
  for (int i = 0; i < 4; ++i)
    {
      // FT4 sync4d correlates every other sample of the 32 sps stream, so the
      // reference template advances over 16 effective phase steps per symbol.
      float const dphi = 2.0f * kPi * static_cast<float> (sequence[i]) / kSync4PhaseSamples;
      for (int j = 0; j < 16; ++j)
        {
          result[k++] = Complex (std::cos (phi), std::sin (phi));
          phi = std::fmod (phi + dphi, 2.0f * kPi);
        }
    }
  return result;
}

std::array<Complex, 64> const& sync4a ()
{
  static std::array<Complex, 64> value = make_sync4 ({0, 1, 3, 2});
  return value;
}

std::array<Complex, 64> const& sync4b ()
{
  static std::array<Complex, 64> value = make_sync4 ({1, 0, 2, 3});
  return value;
}

std::array<Complex, 64> const& sync4c ()
{
  static std::array<Complex, 64> value = make_sync4 ({2, 3, 1, 0});
  return value;
}

std::array<Complex, 64> const& sync4d ()
{
  static std::array<Complex, 64> value = make_sync4 ({3, 2, 0, 1});
  return value;
}

float sync4_power (Complex const* cd0, int np, int start, std::array<Complex, 64> const& sync,
                   bool allow_left_edge, bool allow_right_edge)
{
  constexpr float fac = 1.0f / 64.0f;
  Complex z {};

  if (start >= 0 && start + 127 <= np - 1)
    {
      for (int i = 0; i < 64; ++i)
        {
          z += cd0[start + i * 2] * std::conj (sync[static_cast<size_t> (i)]);
        }
    }
  else if (allow_left_edge && start < 0)
    {
      int const npts = (start + 127) / 2;
      if (npts > 16)
        {
          int const count = npts + 1;
          for (int i = 0; i < count; ++i)
            {
              z += cd0[i * 2] * std::conj (sync[static_cast<size_t> (64 - count + i)]);
            }
        }
    }
  else if (allow_right_edge && start + 127 > np - 1)
    {
      int const npts = (np - start) / 2;
      if (npts > 16)
        {
          for (int i = 0; i < npts; ++i)
            {
              z += cd0[start + i * 2] * std::conj (sync[static_cast<size_t> (i)]);
            }
        }
    }

  return std::abs (z) * fac;
}

float sync4_metric (Complex const* cd0, int np, int i0, Complex const* ctwk, bool itwk)
{
  std::array<Complex, 64> const s1 = apply_tweak (sync4a (), ctwk, itwk);
  std::array<Complex, 64> const s2 = apply_tweak (sync4b (), ctwk, itwk);
  std::array<Complex, 64> const s3 = apply_tweak (sync4c (), ctwk, itwk);
  std::array<Complex, 64> const s4 = apply_tweak (sync4d (), ctwk, itwk);

  int const i1 = i0;
  int const i2 = i0 + 33 * 32;
  int const i3 = i0 + 66 * 32;
  int const i4 = i0 + 99 * 32;

  float const p1 = sync4_power (cd0, np, i1, s1, true, false);
  float const p2 = sync4_power (cd0, np, i2, s2, false, false);
  float const p3 = sync4_power (cd0, np, i3, s3, false, false);
  float const p4 = sync4_power (cd0, np, i4, s4, false, true);
  float const pmin = std::min (std::min (p1, p2), std::min (p3, p4));

  return p1 + p2 + p3 + p4 - pmin;
}

std::array<std::array<Complex, 32>, 7> const& sync8_patterns ()
{
  static std::array<std::array<Complex, 32>, 7> value = [] {
    std::array<std::array<Complex, 32>, 7> patterns {};
    std::array<int, 7> const costas {{3, 1, 4, 0, 6, 5, 2}};
    for (int i = 0; i < 7; ++i)
      {
        float phi = 0.0f;
        float const dphi = 2.0f * kPi * static_cast<float> (costas[static_cast<size_t> (i)]) / 32.0f;
        for (int j = 0; j < 32; ++j)
          {
            patterns[static_cast<size_t> (i)][static_cast<size_t> (j)] = Complex (std::cos (phi), std::sin (phi));
            phi = std::fmod (phi + dphi, 2.0f * kPi);
          }
      }
    return patterns;
  }();
  return value;
}

float sync8_metric (Complex const* cd0, int np, int i0, Complex const* ctwk, bool itwk)
{
  float sync = 0.0f;
  auto const& patterns = sync8_patterns ();

  for (int i = 0; i < 7; ++i)
    {
      std::array<Complex, 32> const pattern = apply_tweak (patterns[static_cast<size_t> (i)], ctwk, itwk);
      int const i1 = i0 + i * 32;
      int const i2 = i1 + 36 * 32;
      int const i3 = i1 + 72 * 32;

      Complex z1 {};
      Complex z2 {};
      Complex z3 {};

      if (i1 >= 0 && i1 + 31 <= np - 1)
        {
          for (int j = 0; j < 32; ++j) z1 += cd0[i1 + j] * std::conj (pattern[static_cast<size_t> (j)]);
        }
      if (i2 >= 0 && i2 + 31 <= np - 1)
        {
          for (int j = 0; j < 32; ++j) z2 += cd0[i2 + j] * std::conj (pattern[static_cast<size_t> (j)]);
        }
      if (i3 >= 0 && i3 + 31 <= np - 1)
        {
          for (int j = 0; j < 32; ++j) z3 += cd0[i3 + j] * std::conj (pattern[static_cast<size_t> (j)]);
        }

      sync += std::norm (z1) + std::norm (z2) + std::norm (z3);
    }

  return sync;
}

inline Complex pattern_at (Complex const* data, int rows, int row0, int col1)
{
  return data[row0 + rows * (col1 - 1)];
}

inline float metric_value (Complex const& z, int ipass, bool lastsync)
{
  if (lastsync || ipass == 2 || ipass == 6 || ipass == 7)
    {
      return std::norm (z);
    }
  if (ipass == 1 || ipass == 5 || ipass == 9)
    {
      return std::abs (z);
    }
  return std::abs (z.real ()) + std::abs (z.imag ());
}

inline Complex cd0_at (Complex const* cd0, int logical_index)
{
  return cd0[logical_index + 800];
}

float sync8var_scan_metric (float const* s, int nh1, int i, int j, int ipass,
                            bool lagcc, bool lagccbail, bool& lcq_out)
{
  std::array<int, 7> const icos7 {{3, 1, 4, 0, 6, 5, 2}};
  constexpr int nssy = 4;
  constexpr int nssy36 = 144;
  constexpr int nssy72 = 288;
  constexpr int nfos = 2;
  constexpr int jstrt = 12;

  auto s_at = [s, nh1](int ii, int jj) -> float {
    return s[(ii - 1) + nh1 * (jj - 1)];
  };

  lcq_out = false;

  if (lagcc && !lagccbail)
    {
      constexpr int nfos6 = 12;
      std::array<float, 30> tall {};
      for (int n = 0; n <= 6; ++n)
        {
          int const k = j + jstrt + nssy * n;
          if (k > 0)
            {
              float const ta = s_at (i + nfos * icos7[static_cast<size_t> (n)], k);
              if (ta > 1.0e-9f)
                {
                  float denom = 0.0f;
                  for (int ii = i; ii <= i + nfos6; ii += nfos) denom += s_at (ii, k);
                  tall[static_cast<size_t> (n)] = ta * 6.0f / (denom - ta);
                }
            }

          int const k36 = k + nssy36;
          if (k36 > 0 && k36 <= 372)
            {
              float const tb = s_at (i + nfos * icos7[static_cast<size_t> (n)], k36);
              if (tb > 1.0e-9f)
                {
                  float denom = 0.0f;
                  for (int ii = i; ii <= i + nfos6; ii += nfos) denom += s_at (ii, k36);
                  tall[static_cast<size_t> (n + 16)] = tb * 6.0f / (denom - tb);
                }
            }

          int const k72 = k + nssy72;
          if (k72 <= 372)
            {
              float const tc = s_at (i + nfos * icos7[static_cast<size_t> (n)], k72);
              if (tc > 1.0e-9f)
                {
                  float denom = 0.0f;
                  for (int ii = i; ii <= i + nfos6; ii += nfos) denom += s_at (ii, k72);
                  tall[static_cast<size_t> (n + 23)] = tc * 6.0f / (denom - tc);
                }
            }
        }

      bool lcq = false;
      if (ipass > 1)
        {
          for (int n = 7; n <= 15; ++n)
            {
              int const k = j + jstrt + nssy * n;
              if (k > 0)
                {
                  float denom = 0.0f;
                  for (int ii = i; ii <= i + nfos6; ii += nfos) denom += s_at (ii, k);
                  if (n < 15)
                    {
                      tall[static_cast<size_t> (n)] = s_at (i, k) * 6.0f / (denom - s_at (i, k));
                    }
                  else
                    {
                      tall[static_cast<size_t> (n)] = s_at (i + 2, k) * 6.0f / (denom - s_at (i + 2, k));
                    }
                }
            }
          float const sya = std::accumulate (tall.begin (), tall.begin () + 7, 0.0f);
          float const sycq = std::accumulate (tall.begin () + 7, tall.begin () + 16, 0.0f);
          float const sybc = std::accumulate (tall.begin () + 16, tall.end (), 0.0f);
          float const sync_abc = std::max ((sya + sycq + sybc) / 30.0f, (sya + sybc) / 21.0f);
          float const sy1 = (sycq + sybc) / 23.0f;
          float const sy2 = sybc / 14.0f;
          float const sync_bc = std::max (sy1, sy2);
          if (sy1 > sy2) lcq = true;
          lcq_out = lcq;
          return std::max (sync_abc, sync_bc);
        }

      float const sya = std::accumulate (tall.begin (), tall.begin () + 7, 0.0f);
      float const sybc = std::accumulate (tall.begin () + 16, tall.end (), 0.0f);
      float const sync_abc = (sya + sybc) / 21.0f;
      float const sync_bc = sybc / 14.0f;
      return std::max (sync_abc, sync_bc);
    }

  constexpr int nfos6 = 16;
  float ta = 0.0f, tb = 0.0f, tc = 0.0f, tcq = 0.0f;
  float t0a = 0.0f, t0b = 0.0f, t0c = 0.0f, t0cq = 0.0f;
  for (int n = 0; n <= 6; ++n)
    {
      int const k = j + jstrt + nssy * n;
      if (k > 0)
        {
          ta += s_at (i + nfos * icos7[static_cast<size_t> (n)], k);
          for (int ii = i; ii <= i + nfos6; ++ii) t0a += s_at (ii, k);
          t0a -= s_at (i + nfos * icos7[static_cast<size_t> (n)] + 1, k);
        }
      int const k36 = k + nssy36;
      if (k36 > 0 && k36 <= 372)
        {
          tb += s_at (i + nfos * icos7[static_cast<size_t> (n)], k36);
          for (int ii = i; ii <= i + nfos6; ++ii) t0b += s_at (ii, k36);
          t0b -= s_at (i + nfos * icos7[static_cast<size_t> (n)] + 1, k36);
        }
      int const k72 = k + nssy72;
      if (k72 <= 372)
        {
          tc += s_at (i + nfos * icos7[static_cast<size_t> (n)], k72);
          for (int ii = i; ii <= i + nfos6; ++ii) t0c += s_at (ii, k72);
          t0c -= s_at (i + nfos * icos7[static_cast<size_t> (n)] + 1, k72);
        }
    }
  for (int n = 7; n <= 15; ++n)
    {
      int const k = j + jstrt + nssy * n;
      if (k >= 1)
        {
          for (int ii = i; ii <= i + nfos6; ++ii) t0cq += s_at (ii, k);
          if (n < 15)
            {
              tcq += s_at (i, k);
              t0cq -= s_at (i, k + 1);
            }
          else
            {
              tcq += s_at (i + 2, k);
              t0cq -= s_at (i, k + 3);
            }
        }
    }

  float t1 = ta + tb + tc;
  float t01 = t0a + t0b + t0c;
  float t2 = t1 + tcq;
  float t02 = t01 + t0cq;
  t01 = (t01 - t1 * 2.0f) / 42.0f;
  if (t01 < 1.0e-8f) t01 = 1.0f;
  t02 = (t02 - t2 * 2.0f) / 60.0f;
  if (t02 < 1.0e-8f) t02 = 1.0f;
  float const syncf = std::max (t1 / (7.0f * t01), (t1 / 7.0f + tcq / 9.0f) / t02);
  bool lcq = ((t1 / 7.0f + tcq / 9.0f) / t02) > (t1 / (7.0f * t01));

  t1 = tb + tc;
  t01 = t0b + t0c;
  t2 = t1 + tcq;
  t02 = t01 + t0cq;
  t01 = (t01 - t1 * 2.0f) / 28.0f;
  if (t01 < 1.0e-8f) t01 = 1.0f;
  t02 = (t02 - t2 * 2.0f) / 46.0f;
  if (t02 < 1.0e-8f) t02 = 1.0f;
  float const syncs = std::max (t1 / (7.0f * t01), (t1 / 7.0f + tcq / 9.0f) / t02);
  bool const lcq2 = ((t1 / 7.0f + tcq / 9.0f) / t02) > (t1 / (7.0f * t01));

  lcq_out = (syncf > syncs) ? lcq : lcq2;
  return std::max (syncf, syncs);
}

}

extern "C" void ftx_sync2d_c (Complex const* cd0, int np, int i0,
                               Complex const* ctwk, int itwk, float* sync)
{
  *sync = sync4_metric (cd0, np, i0, ctwk, itwk != 0);
}

extern "C" void ftx_sync4d_c (Complex const* cd0, int np, int i0,
                               Complex const* ctwk, int itwk, float* sync)
{
  *sync = sync4_metric (cd0, np, i0, ctwk, itwk != 0);
}

extern "C" void ftx_sync4d_ref_c (Complex const* cd0, int np, int i0,
                                   Complex const* ctwk, int itwk, float* sync)
{
  ftx_sync4d_c (cd0, np, i0, ctwk, itwk, sync);
}

extern "C" void ftx_sync8d_c (Complex const* cd0, int np, int i0,
                               Complex const* ctwk, int itwk, float* sync)
{
  *sync = sync8_metric (cd0, np, i0, ctwk, itwk != 0);
}

extern "C" void sync8d_ (Complex const* cd0, int* i0, Complex const* ctwk, int* itwk, float* sync)
{
  if (!i0 || !itwk || !sync)
    {
      return;
    }

  ftx_sync8d_c (cd0, 2812, *i0, ctwk, *itwk, sync);
}

extern "C" void ftx_sync8var_scan_c (float const* s, int nh1, int nhsym, int iaw, int ibw,
                                     int jzb, int jzt, int ipass, int lagcc, int lagccbail,
                                     float* sync2d, signed char* syncq)
{
  (void) nhsym;
  std::fill_n (sync2d, nh1 * (jzt - jzb + 1), 0.0f);
  std::fill_n (syncq, nh1 * (jzt - jzb + 1), 0);

  for (int j = jzb; j <= jzt; ++j)
    {
      for (int i = iaw; i <= ibw; ++i)
        {
          bool lcq = false;
          float const value = sync8var_scan_metric (s, nh1, i, j, ipass, lagcc != 0, lagccbail != 0, lcq);
          size_t const offset = static_cast<size_t> ((i - 1) + nh1 * (j - jzb));
          sync2d[offset] = value;
          syncq[offset] = lcq ? 1 : 0;
        }
    }
}

extern "C" void ftx_sync8dvar_c (Complex const* cd0, int i0, Complex const* ctwk, int itwk,
                                 int ipass, int lastsync, int iqso, int lcq,
                                 int lcallsstd, int lcqcand, Complex const* csync,
                                 Complex const* csynce, Complex const* csyncsd,
                                 Complex const* csyncsdcq, Complex const* csynccq,
                                 float* sync)
{
  *sync = 0.0f;
  std::array<Complex, 7> zt1 {};
  std::array<Complex, 7> zt2 {};
  std::array<Complex, 7> zt3 {};

  for (int i = 0; i <= 6; ++i)
    {
      int const i1 = i0 + i * 32;
      int const i2 = i1 + 1152;
      int const i3 = i1 + 2304;
      std::array<Complex, 32> pattern {};
      for (int k = 1; k <= 32; ++k)
        {
          pattern[static_cast<size_t> (k - 1)] = pattern_at (csync, 7, i, k);
          if (itwk == 1) pattern[static_cast<size_t> (k - 1)] *= ctwk[k - 1];
        }
      for (int k = 0; k < 32; ++k)
        {
          zt1[static_cast<size_t> (i)] += cd0_at (cd0, i1 + k) * std::conj (pattern[static_cast<size_t> (k)]);
          zt2[static_cast<size_t> (i)] += cd0_at (cd0, i2 + k) * std::conj (pattern[static_cast<size_t> (k)]);
          zt3[static_cast<size_t> (i)] += cd0_at (cd0, i3 + k) * std::conj (pattern[static_cast<size_t> (k)]);
        }
    }

  if (ipass == 1 || ipass == 5 || ipass == 9 || ipass == 2 || ipass == 6 || ipass == 7)
    {
      for (int i = 0; i <= 6; ++i)
        {
          *sync += metric_value (zt1[static_cast<size_t> (i)], ipass, lastsync != 0);
          *sync += metric_value (zt2[static_cast<size_t> (i)], ipass, lastsync != 0);
          *sync += metric_value (zt3[static_cast<size_t> (i)], ipass, lastsync != 0);
        }
    }
  else
    {
      for (int i = 0; i <= 6; ++i)
        {
          Complex const z1 = (i < 6) ? (zt1[static_cast<size_t> (i)] + zt1[static_cast<size_t> (i + 1)]) / 2.0f : zt1[static_cast<size_t> (i)];
          Complex const z2 = (i < 6) ? (zt2[static_cast<size_t> (i)] + zt2[static_cast<size_t> (i + 1)]) / 2.0f : zt2[static_cast<size_t> (i)];
          Complex const z3 = (i < 6) ? (zt3[static_cast<size_t> (i)] + zt3[static_cast<size_t> (i + 1)]) / 2.0f : zt3[static_cast<size_t> (i)];
          *sync += metric_value (z1, ipass, lastsync != 0);
          *sync += metric_value (z2, ipass, lastsync != 0);
          *sync += metric_value (z3, ipass, lastsync != 0);
        }
    }

  if (itwk == 1 && lcqcand != 0 && iqso == 1)
    {
      for (int i = 0; i <= 7; ++i)
        {
          Complex z4 {};
          for (int k = 1; k <= 32; ++k)
            {
              Complex pattern = pattern_at (csynccq, 8, i, k) * ctwk[k - 1];
              int const i4 = i0 + (i + 7) * 32 + (k - 1);
              z4 += cd0_at (cd0, i4) * std::conj (pattern);
            }
          Complex const z_eff = (ipass == 3 || ipass == 4 || ipass == 8) && i < 7 ? z4 / 2.0f : z4;
          *sync += metric_value (z_eff, ipass, false);
        }
    }

  if (lastsync == 0)
    {
      if (((iqso > 1 && iqso < 4) && lcallsstd != 0) || (iqso == 4 && lcq == 0))
        {
          Complex const* extra = (iqso == 4 && lcq == 0) ? csyncsd : csynce;
          for (int i = 0; i <= 18; ++i)
            {
              Complex z4 {};
              for (int k = 1; k <= 32; ++k)
                {
                  Complex pattern = pattern_at (extra, 19, i, k);
                  if (itwk == 1) pattern *= ctwk[k - 1];
                  int const i4 = i0 + (i + 7) * 32 + (k - 1);
                  z4 += cd0_at (cd0, i4) * std::conj (pattern);
                }
              Complex const z_eff = (ipass == 2 || ipass == 6 || ipass == 7) && i < 18 ? z4 / 2.0f : z4;
              *sync += metric_value (z_eff, ipass, false);
            }
        }

      if (iqso == 4 && lcq != 0)
        {
          for (int i = 0; i <= 57; ++i)
            {
              Complex z4 {};
              for (int k = 1; k <= 32; ++k)
                {
                  Complex pattern = pattern_at (csyncsdcq, 58, i, k);
                  if (itwk == 1) pattern *= ctwk[k - 1];
                  int const slot = (i < 29) ? (i + 7) : (i + 14);
                  int const i4 = i0 + slot * 32 + (k - 1);
                  z4 += cd0_at (cd0, i4) * std::conj (pattern);
                }
              *sync += metric_value (z4, ipass, false);
            }
        }
    }
}
