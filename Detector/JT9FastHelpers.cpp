#include "Detector/JT9FastHelpers.hpp"

#include "Modulator/LegacyJtEncoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

extern "C"
{
  void four2a_ (std::complex<float> a[], int* nfft, int* ndim, int* isign, int* iform, int);
}

namespace
{

void four2a_forward_real_buffer (float* buffer, int nfft)
{
  int ndim = 1;
  int isign = -1;
  int iform = 0;
  four2a_ (reinterpret_cast<std::complex<float>*> (buffer), &nfft, &ndim, &isign, &iform, 0);
}

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

inline int s1_index (int i1, int j1, int nq)
{
  return (i1 - 1) + nq * (j1 - 1);
}

inline int s2_index (int i1, int j1)
{
  return (i1 - 1) + 240 * (j1 - 1);
}

inline int ss2_index (int i0, int j1)
{
  return i0 + 9 * (j1 - 1);
}

inline int ss3_index (int i0, int j1)
{
  return i0 + 8 * (j1 - 1);
}

}

namespace decodium
{
namespace jt9fast
{

void spec9f_compute (short const* id2, int npts, int nsps, std::vector<float>& s1, int* jz_out,
                     int* nq_out)
{
  int const nfft = 2 * nsps;
  int const nh = nfft / 2;
  int const nq = nfft / 4;
  int const jz = npts / (nsps / 4);

  if (jz_out) *jz_out = jz;
  if (nq_out) *nq_out = nq;

  s1.assign (static_cast<std::size_t> (nq * jz), 0.0f);
  std::vector<float> x (static_cast<std::size_t> (nfft + 2), 0.0f);
  auto* c = reinterpret_cast<std::complex<float>*> (x.data ());

  for (int j = 1; j <= jz; ++j)
    {
      int const ia = (j - 1) * nsps / 4;
      int const ib = ia + nsps - 1;
      if (ib > npts)
        {
          break;
        }
      for (int i = 0; i < nh; ++i)
        {
          x[static_cast<std::size_t> (i)] = id2[ia + i];
        }
      std::fill (x.begin () + nh, x.end (), 0.0f);
      four2a_forward_real_buffer (x.data (), nfft);
      int const k = ((j - 1) % 340) + 1;
      (void) k;
      for (int i = 1; i <= nq; ++i)
        {
          auto const value = c[i];
          s1[static_cast<std::size_t> (s1_index (i, j, nq))] =
              1.0e-10f * (value.real () * value.real () + value.imag () * value.imag ());
        }
    }
}

void foldspec9f_compute (float const* s1, int nq, int jz, int ja, int jb, std::array<float, 240 * 340>& s2)
{
  s2.fill (0.0f);
  std::array<int, 340> nsum {};

  for (int j = ja; j <= jb; ++j)
    {
      int const k = ((j - 1) % 340) + 1;
      nsum[static_cast<std::size_t> (k - 1)] += 1;
      for (int i = 1; i <= nq; ++i)
        {
          s2[static_cast<std::size_t> (s2_index (i, k))] +=
              s1[static_cast<std::size_t> (s1_index (i, j, nq))];
        }
    }

  for (int k = 1; k <= 340; ++k)
    {
      float fac = 1.0f;
      if (nsum[static_cast<std::size_t> (k - 1)] > 0)
        {
          fac = 1.0f / nsum[static_cast<std::size_t> (k - 1)];
        }
      for (int i = 1; i <= nq; ++i)
        {
          s2[static_cast<std::size_t> (s2_index (i, k))] *= fac;
        }
    }

  float const ave =
      std::accumulate (s2.begin (), s2.end (), 0.0f) / (340.0f * nq);
  if (ave > 0.0f)
    {
      for (float& value : s2)
        {
          value /= ave;
        }
    }
  (void) jz;
}

Sync9Result sync9f_compute (std::array<float, 240 * 340> const& s2, int nq, int nfa, int nfb)
{
  Sync9Result result;
  std::array<int, 16> ii4 {};
  for (int n = 0; n < 16; ++n)
    {
      ii4[static_cast<std::size_t> (n)] = 4 * kJt9SyncSymbols[static_cast<std::size_t> (n)] - 3;
    }

  int const nfft = 4 * nq;
  float const df = 12000.0f / nfft;
  int const ia = static_cast<int> (nfa / df);
  int const ib = static_cast<int> (nfb / df + 0.9999f);

  for (int i = ia; i <= ib; ++i)
    {
      for (int lag = 0; lag <= 339; ++lag)
        {
          float t = 0.0f;
          for (int n = 0; n < 16; ++n)
            {
              int j = ii4[static_cast<std::size_t> (n)] + lag;
              if (j > 340)
                {
                  j -= 340;
                }
              t += s2[static_cast<std::size_t> (s2_index (i, j))];
            }
          if (t > result.ccfbest)
            {
              result.lagpk = lag;
              result.ipk = i;
              result.ccfbest = t;
            }
        }
    }

  for (int i = 0; i <= 8; ++i)
    {
      int j4 = result.lagpk - 4;
      int i2 = 2 * i + result.ipk;
      if (i2 < 1)
        {
          i2 = 1;
        }
      int m = 0;
      for (int j = 1; j <= 85; ++j)
        {
          j4 += 4;
          if (j4 > 340)
            {
              j4 -= 340;
            }
          if (j4 < 1)
            {
              j4 += 340;
            }
          float const value = s2[static_cast<std::size_t> (s2_index (i2, j4))];
          result.ss2[static_cast<std::size_t> (ss2_index (i, j))] = value;
          if (i >= 1 && kJt9Isync[static_cast<std::size_t> (j - 1)] == 0)
            {
              ++m;
              result.ss3[static_cast<std::size_t> (ss3_index (i - 1, m))] = value;
            }
        }
    }

  return result;
}

void softsym9f_compute (std::array<float, 9 * 85> const& ss2, std::array<float, 8 * 69> const& ss3,
                        std::array<std::int8_t, 207>& soft_symbols)
{
  float ss = 0.0f;
  float sig = 0.0f;
  if (ss2[0] == -999.0f)
    {
      return;
    }

  std::array<float, 8 * 69> norm = ss3;
  for (int j = 1; j <= 69; ++j)
    {
      float smax = 0.0f;
      for (int i = 0; i <= 7; ++i)
        {
          float const value = norm[static_cast<std::size_t> (ss3_index (i, j))];
          smax = std::max (smax, value);
          ss += value;
        }
      sig += smax;
      ss -= smax;
    }
  float const ave = ss / (69.0f * 7.0f);
  for (float& value : norm)
    {
      value /= ave;
    }
  sig /= 69.0f;
  (void) sig;

  std::array<unsigned char, 206> scrambled {};
  int k = 0;
  float const scale = 10.0f;
  for (int j = 1; j <= 69; ++j)
    {
      for (int m = 2; m >= 0; --m)
        {
          float r1 = 0.0f;
          float r0 = 0.0f;
          if (m == 2)
            {
              r1 = std::max ({norm[ss3_index (4, j)], norm[ss3_index (5, j)],
                              norm[ss3_index (6, j)], norm[ss3_index (7, j)]});
              r0 = std::max ({norm[ss3_index (0, j)], norm[ss3_index (1, j)],
                              norm[ss3_index (2, j)], norm[ss3_index (3, j)]});
            }
          else if (m == 1)
            {
              r1 = std::max ({norm[ss3_index (2, j)], norm[ss3_index (3, j)],
                              norm[ss3_index (4, j)], norm[ss3_index (5, j)]});
              r0 = std::max ({norm[ss3_index (0, j)], norm[ss3_index (1, j)],
                              norm[ss3_index (6, j)], norm[ss3_index (7, j)]});
            }
          else
            {
              r1 = std::max ({norm[ss3_index (1, j)], norm[ss3_index (2, j)],
                              norm[ss3_index (4, j)], norm[ss3_index (7, j)]});
              r0 = std::max ({norm[ss3_index (0, j)], norm[ss3_index (3, j)],
                              norm[ss3_index (5, j)], norm[ss3_index (6, j)]});
            }

          int i4 = static_cast<int> (std::lround (scale * (r1 - r0)));
          i4 = std::max (-127, std::min (127, i4));
          if (k < static_cast<int> (scrambled.size ()))
            {
              scrambled[static_cast<std::size_t> (k)] =
                  static_cast<unsigned char> (static_cast<std::int8_t> (i4));
            }
          ++k;
        }
    }

  auto const order = decodium::legacy_jt::detail::interleave9_order ();
  for (int i = 0; i < 206; ++i)
    {
      soft_symbols[static_cast<std::size_t> (i)] =
          static_cast<std::int8_t> (scrambled[static_cast<std::size_t> (order[static_cast<std::size_t> (i)])]);
    }
  soft_symbols[206] = 0;
}

}
}
