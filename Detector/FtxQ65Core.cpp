// -*- Mode: C++ -*-
#include "Detector/FftCompat.hpp"
#include "Detector/FtxQ65Core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
extern "C"
{
#include "lib/qra/q65/q65.h"
#include "lib/qra/q65/qra15_65_64_irr_e23.h"
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

extern "C"
{
void ftx_q65_ap_c (int* nqsoprogress, int* ipass, int* ncontest, int* lapcqonly,
                   int apsym0[], int aph10[], int apmask[], int apsymbols[], int* iaptype);
void legacy_pack77_unpack_c (char const c77[77], int received, char msgsent[37], bool* success);
}

namespace
{
using decodium::fft_compat::forward_complex;

constexpr int kQ65MessageSymbols {13};
constexpr int kQ65DecodedChars {37};
constexpr int kQ65Alphabet {64};
constexpr int kQ65SyncSymbols {85};
constexpr int kQ65PayloadColumns {63};
constexpr int kQ65BirdieThreshold {15};
constexpr int kQ65SpecColumns {63};
constexpr float kQ65SampleRate {6000.0f};
constexpr float kQ65S3Limit {20.0f};
constexpr int kQ65ApBits {78};
constexpr int kQ65ApPacked {13};
constexpr int kQ65SpecMaxFft {20736};
constexpr int kQ65SpecSyncCount {22};
constexpr float kQ65TwoPi {6.28318530717958647692f};

q65_codec_ds& shared_q65_codec ()
{
  static q65_codec_ds codec {};
  static std::once_flag once;
  std::call_once (once, [] {
    if (q65_init (&codec, &qra15_65_64_irr_e23) < 0)
      {
        std::fprintf (stderr, "error in q65_init()\n");
        std::abort ();
      }
  });
  return codec;
}

std::mutex& shared_q65_codec_mutex ()
{
  static std::mutex mutex;
  return mutex;
}

inline float db_scale (float value)
{
  if (value > 1.259e-10f)
    {
      return 10.0f * std::log10 (value);
    }
  return -99.0f;
}

inline int col_major_index (int row, int column, int rows)
{
  return row + rows * column;
}

void fill_spaces (char* decoded)
{
  if (decoded)
    {
      std::fill_n (decoded, kQ65DecodedChars, ' ');
    }
}

void dat4_to_c77 (int const dat4[kQ65MessageSymbols], char c77[77])
{
  int offset = 0;
  for (int i = 0; i < 12; ++i)
    {
      int value = dat4[i];
      for (int bit = 5; bit >= 0; --bit)
        {
          c77[offset++] = ((value >> bit) & 1) != 0 ? '1' : '0';
        }
    }

  int value = dat4[12] / 2;
  for (int bit = 4; bit >= 0; --bit)
    {
      c77[offset++] = ((value >> bit) & 1) != 0 ? '1' : '0';
    }
}

void q65_bzap_impl (float* s3, int ll)
{
  std::vector<int> hist (static_cast<size_t> (ll), 0);
  for (int column = 0; column < kQ65PayloadColumns; ++column)
    {
      int best_row = 0;
      float best_value = s3[col_major_index (0, column, ll)];
      for (int row = 1; row < ll; ++row)
        {
          float const value = s3[col_major_index (row, column, ll)];
          if (value > best_value)
            {
              best_value = value;
              best_row = row;
            }
        }
      hist[static_cast<size_t> (best_row)] += 1;
    }

  for (int row = 0; row < ll; ++row)
    {
      if (hist[static_cast<size_t> (row)] > kQ65BirdieThreshold)
        {
          for (int column = 0; column < kQ65PayloadColumns; ++column)
            {
              s3[col_major_index (row, column, ll)] = 1.0f;
            }
        }
    }
}

int q65_mode_to_submode (int mode_q65)
{
  switch (mode_q65)
    {
    case 1: return 0;
    case 2: return 1;
    case 4: return 2;
    case 8: return 3;
    case 16: return 4;
    case 32: return 5;
    default: return 0;
    }
}

void pack_q65_ap_bits (int const unpacked[kQ65ApBits], int packed[kQ65ApPacked])
{
  for (int group = 0; group < kQ65ApPacked; ++group)
    {
      int value = 0;
      for (int bit = 0; bit < 6; ++bit)
        {
          value = (value << 1) | (unpacked[group * 6 + bit] != 0 ? 1 : 0);
        }
      packed[group] = value;
    }
}

float percentile_value (float const* values, int count, int pct)
{
  if (!values || count <= 0 || pct < 0 || pct > 100)
    {
      return 1.0f;
    }

  std::vector<float> sorted (values, values + count);
  std::sort (sorted.begin (), sorted.end ());
  int index = static_cast<int> (std::lround (count * 0.01f * pct));
  index = std::max (1, std::min (count, index));
  return sorted[static_cast<std::size_t> (index - 1)];
}

void twkfreq_impl (std::complex<float> const* c3, std::complex<float>* c4, int npts,
                   float fsample, std::array<float, 3> const& a)
{
  if (!c3 || !c4 || npts <= 0 || fsample <= 0.0f)
    {
      return;
    }

  std::complex<float> w {1.0f, 0.0f};
  float const x0 = 0.5f * (npts + 1);
  float const s = 2.0f / static_cast<float> (npts);
  for (int i = 1; i <= npts; ++i)
    {
      float const x = s * (i - x0);
      float const p2 = 1.5f * x * x - 0.5f;
      float const dphi = (a[0] + x * a[1] + p2 * a[2]) * (kQ65TwoPi / fsample);
      std::complex<float> const wstep {std::cos (dphi), std::sin (dphi)};
      w *= wstep;
      c4[static_cast<std::size_t> (i - 1)] = w * c3[static_cast<std::size_t> (i - 1)];
    }
}

void spec64_impl (std::complex<float> const* c0, int npts, int nsps, int mode_q65, int jpk,
                  float* s3, int ll, int nn)
{
  static std::array<int, kQ65SpecSyncCount> const sync_positions {{
      1, 9, 12, 13, 15, 22, 23, 26, 27, 33, 35, 38, 46, 50, 55, 60, 62, 66, 69, 74, 76, 85
  }};

  if (!c0 || !s3 || npts <= 0 || nsps <= 0 || ll <= 0 || nn <= 0)
    {
      return;
    }

  std::fill_n (s3, static_cast<std::size_t> (ll) * nn, 0.0f);
  std::vector<std::complex<float>> cs (static_cast<std::size_t> (kQ65SpecMaxFft));
  int sync_index = 0;
  int payload = 0;
  for (int k = 1; k <= 84; ++k)
    {
      if (sync_index < kQ65SpecSyncCount && k == sync_positions[static_cast<std::size_t> (sync_index)])
        {
          ++sync_index;
          continue;
        }
      ++payload;
      int ja = (k - 1) * nsps + jpk;
      int jb = ja + nsps - 1;
      ja = std::max (0, ja);
      jb = std::min (npts - 1, jb);
      int const nz = jb - ja;
      if (nz < 0)
        {
          continue;
        }

      std::fill (cs.begin (), cs.begin () + nsps, std::complex<float> {});
      std::copy (c0 + ja, c0 + jb + 1, cs.begin ());
      forward_complex (cs.data (), nsps);
      for (int ii = 1; ii <= ll; ++ii)
        {
          int i = ii - 65 + mode_q65;
          if (i < 0) i += nsps;
          s3[col_major_index (ii - 1, payload - 1, ll)] =
              std::norm (cs[static_cast<std::size_t> (i)]);
        }
    }

  std::vector<float> xbase0 (static_cast<std::size_t> (ll));
  std::vector<float> xbase (static_cast<std::size_t> (ll));
  std::vector<float> row_values (static_cast<std::size_t> (nn));
  for (int i = 0; i < ll; ++i)
    {
      for (int column = 0; column < nn; ++column)
        {
          row_values[static_cast<std::size_t> (column)] = s3[col_major_index (i, column, ll)];
        }
      xbase0[static_cast<std::size_t> (i)] = percentile_value (row_values.data (), nn, 45);
    }
  int const nh = 25;
  if (ll >= nh)
    {
      float sum_lo = 0.0f;
      for (int i = 0; i < nh - 1; ++i) sum_lo += xbase0[static_cast<std::size_t> (i)];
      float sum_hi = 0.0f;
      for (int i = ll - nh + 1; i < ll; ++i) sum_hi += xbase0[static_cast<std::size_t> (i)];
      for (int i = 0; i < nh - 1; ++i) xbase[static_cast<std::size_t> (i)] = sum_lo / (nh - 1.0f);
      for (int i = ll - nh + 1; i < ll; ++i) xbase[static_cast<std::size_t> (i)] = sum_hi / (nh - 1.0f);
      for (int i = nh - 1; i <= ll - nh - 1; ++i)
        {
          float sum = 0.0f;
          for (int j = i - nh + 1; j <= i + nh; ++j)
            {
              sum += xbase0[static_cast<std::size_t> (j)];
            }
          xbase[static_cast<std::size_t> (i)] = sum / (2 * nh + 1.0f);
        }
    }
  else
    {
      std::fill (xbase.begin (), xbase.end (),
                 percentile_value (xbase0.data (), ll, 50));
    }

  for (int column = 0; column < nn; ++column)
    {
      for (int row = 0; row < ll; ++row)
        {
          s3[col_major_index (row, column, ll)] /=
              xbase[static_cast<std::size_t> (row)] + 0.001f;
        }
    }
}

}

void decodium::q65::encode_payload_symbols (int const payload[13], int codeword[63])
{
  if (!payload || !codeword)
    {
      return;
    }

  std::lock_guard<std::mutex> lock {shared_q65_codec_mutex ()};
  q65_codec_ds& codec = shared_q65_codec ();
  q65_encode (&codec, codeword, payload);
}

extern "C" void q65_intrinsics_ff_ (float s3[], int* submode, float* b90ts,
                                    int* fading_model, float s3prob[])
{
  std::lock_guard<std::mutex> lock {shared_q65_codec_mutex ()};
  q65_codec_ds& codec = shared_q65_codec ();
  if (q65_intrinsics_fastfading (&codec, s3prob, s3, *submode, *b90ts, *fading_model) < 0)
    {
      std::fprintf (stderr, "error in q65_intrinsics()\n");
      std::abort ();
    }
}

extern "C" void q65_dec_ (float s3[], float s3prob[], int apmask[], int apsymbols[],
                          int* maxiters, float* esnodb, int xdec[], int* rc)
{
  std::lock_guard<std::mutex> lock {shared_q65_codec_mutex ()};
  q65_codec_ds& codec = shared_q65_codec ();
  int ydec[kQ65PayloadColumns] {};
  float esnodb_local = 0.0f;

  *rc = q65_decode (&codec, ydec, xdec, s3prob, apmask, apsymbols, *maxiters);
  *esnodb = 0.0f;
  if (*rc < 0)
    {
      return;
    }

  if (q65_esnodb_fastfading (&codec, &esnodb_local, ydec, s3) < 0)
    {
      std::fprintf (stderr, "error in q65_esnodb_fastfading()\n");
      std::abort ();
    }
  *esnodb = esnodb_local;
}

extern "C" void q65_dec_fullaplist_ (float s3[], float s3prob[], int codewords[],
                                     int* ncw, float* esnodb, int xdec[], float* plog, int* rc)
{
  std::lock_guard<std::mutex> lock {shared_q65_codec_mutex ()};
  q65_codec_ds& codec = shared_q65_codec ();
  int ydec[kQ65PayloadColumns] {};
  float esnodb_local = 0.0f;

  *rc = q65_decode_fullaplist (&codec, ydec, xdec, s3prob, codewords, *ncw);
  *plog = q65_llh;
  *esnodb = 0.0f;
  if (*rc < 0)
    {
      return;
    }

  if (q65_esnodb_fastfading (&codec, &esnodb_local, ydec, s3) < 0)
    {
      std::fprintf (stderr, "error in q65_esnodb_fastfading()\n");
      std::abort ();
    }
  *esnodb = esnodb_local;
}

namespace
{
}

extern "C" void ftx_q65_bzap_c (float s3[], int* ll)
{
  if (!s3 || !ll || *ll <= 0)
    {
      return;
    }
  q65_bzap_impl (s3, *ll);
}

extern "C" void ftx_q65_s1_to_s3_c (float const s1[], int* iz, int* jz, int* i0, int* j0,
                                    int* ipk, int* jpk, int* ll, int* mode_q65,
                                    float const sync[85], float s3[])
{
  if (!s1 || !iz || !jz || !i0 || !j0 || !ipk || !jpk || !ll || !mode_q65 || !sync || !s3
      || *iz <= 0 || *jz <= 0 || *ll <= 0)
    {
      return;
    }

  std::fill_n (s3, static_cast<size_t> (*ll) * kQ65PayloadColumns, 0.0f);

  int const i1 = *i0 + *ipk - 64 + *mode_q65;
  int const i2 = i1 + *ll - 1;
  if (i1 >= 1 && i2 <= *iz)
    {
      int j = *j0 + *jpk - 7;
      int payload = 0;
      for (int k = 1; k <= kQ65SyncSymbols; ++k)
        {
          j += 8;
          if (sync[k - 1] > 0.0f)
            {
              continue;
            }
          ++payload;
          if (payload > kQ65PayloadColumns)
            {
              break;
            }
          if (j >= 1 && j <= *jz)
            {
              for (int row = 0; row < *ll; ++row)
                {
                  s3[col_major_index (row, payload - 1, *ll)] =
                      s1[col_major_index (i1 - 1 + row, j - 1, *iz)];
                }
            }
        }
    }

  q65_bzap_impl (s3, *ll);
}

extern "C" void ftx_q65_sync_curve_c (float ccf1[], int* count, float* rms1)
{
  if (!ccf1 || !count || !rms1 || *count <= 0)
    {
      if (rms1)
        {
          *rms1 = 0.0f;
        }
      return;
    }

  int const ic = (*count - 1) / 8;
  int const nsum = 2 * (ic + 1);
  float sum = 0.0f;
  for (int i = 0; i <= ic; ++i)
    {
      sum += ccf1[i];
      sum += ccf1[*count - 1 - i];
    }

  float const base = nsum > 0 ? sum / static_cast<float> (nsum) : 0.0f;
  for (int i = 0; i < *count; ++i)
    {
      ccf1[i] -= base;
    }

  float sq = 0.0f;
  for (int i = 0; i <= ic; ++i)
    {
      sq += ccf1[i] * ccf1[i];
      sq += ccf1[*count - 1 - i] * ccf1[*count - 1 - i];
    }

  *rms1 = nsum > 0 ? std::sqrt (sq / static_cast<float> (nsum)) : 0.0f;
  if (*rms1 > 0.0f)
    {
      for (int i = 0; i < *count; ++i)
        {
          ccf1[i] /= *rms1;
        }
    }
}

extern "C" void ftx_q65_dec1_c (float s3[], int* ll, int* nsubmode, float* b90ts,
                                float* esnodb, int* irc, int dat4[13], char decoded[37],
                                int codewords[], int* ncw, float* plog)
{
  if (irc)
    {
      *irc = -2;
    }
  if (esnodb)
    {
      *esnodb = 0.0f;
    }
  if (plog)
    {
      *plog = 0.0f;
    }
  if (dat4)
    {
      std::fill_n (dat4, kQ65MessageSymbols, 0);
    }
  fill_spaces (decoded);

  if (!s3 || !ll || !nsubmode || !b90ts || !esnodb || !irc || !dat4 || !decoded
      || !codewords || !ncw || !plog || *ll <= 0)
    {
      return;
    }

  std::vector<float> s3prob (static_cast<size_t> (kQ65Alphabet) * kQ65PayloadColumns, 0.0f);
  int fading_model = 1;
  q65_intrinsics_ff_ (s3, nsubmode, b90ts, &fading_model, s3prob.data ());
  q65_dec_fullaplist_ (s3, s3prob.data (), codewords, ncw, esnodb, dat4, plog, irc);
  if (std::all_of (dat4, dat4 + kQ65MessageSymbols, [] (int value) { return value == 0; }))
    {
      *irc = -2;
    }
  if (*irc >= 0 && *plog > -242.0f)
    {
      char c77[77];
      bool success = false;
      dat4_to_c77 (dat4, c77);
      legacy_pack77_unpack_c (c77, 0, decoded, &success);
      if (!success)
        {
          fill_spaces (decoded);
          *irc = -1;
        }
    }
  else
    {
      *irc = -1;
    }
}

extern "C" void ftx_q65_dec2_c (float s3[], int* ll, int* nsubmode, float* b90ts,
                                float* esnodb, int* irc, int dat4[13], char decoded[37],
                                int apmask[13], int apsymbols[13], int* maxiters)
{
  if (irc)
    {
      *irc = -2;
    }
  if (esnodb)
    {
      *esnodb = 0.0f;
    }
  if (dat4)
    {
      std::fill_n (dat4, kQ65MessageSymbols, 0);
    }
  fill_spaces (decoded);

  if (!s3 || !ll || !nsubmode || !b90ts || !esnodb || !irc || !dat4 || !decoded
      || !apmask || !apsymbols || !maxiters || *ll <= 0)
    {
      return;
    }

  std::vector<float> s3prob (static_cast<size_t> (kQ65Alphabet) * kQ65PayloadColumns, 0.0f);
  int fading_model = 1;
  q65_intrinsics_ff_ (s3, nsubmode, b90ts, &fading_model, s3prob.data ());
  q65_dec_ (s3, s3prob.data (), apmask, apsymbols, maxiters, esnodb, dat4, irc);
  if (std::all_of (dat4, dat4 + kQ65MessageSymbols, [] (int value) { return value == 0; }))
    {
      *irc = -2;
    }
  if (*irc >= 0)
    {
      char c77[77];
      bool success = false;
      dat4_to_c77 (dat4, c77);
      legacy_pack77_unpack_c (c77, 0, decoded, &success);
      if (!success)
        {
          fill_spaces (decoded);
          *irc = -1;
        }
    }
}

extern "C" void ftx_q65_loops_c (std::complex<float> const c00[], int* npts2, int* nsps2,
                                 int* nsubmode, int* ndepth, int* jpk0, float* xdt0,
                                 float* f0, int* iaptype, int* mode_q65, int* ibwa,
                                 int* ibwb, float* drift, int apmask[13],
                                 int apsymbols[13], int* maxiters, float* xdt1,
                                 float* f1, float* snr2, int dat4[13], int* idec,
                                 int* idfbest, int* idtbest, int* ndistbest,
                                 int* ibwbest, int* nrc)
{
  if (idec)
    {
      *idec = -1;
    }
  if (xdt1 && xdt0)
    {
      *xdt1 = *xdt0;
    }
  if (f1 && f0)
    {
      *f1 = *f0;
    }
  if (snr2)
    {
      *snr2 = 0.0f;
    }
  if (dat4)
    {
      std::fill_n (dat4, kQ65MessageSymbols, 0);
    }
  if (idfbest)
    {
      *idfbest = 0;
    }
  if (idtbest)
    {
      *idtbest = 0;
    }
  if (ndistbest)
    {
      *ndistbest = 0;
    }
  if (ibwbest)
    {
      *ibwbest = 0;
    }
  if (nrc)
    {
      *nrc = -2;
    }

  if (!c00 || !npts2 || !nsps2 || !nsubmode || !ndepth || !jpk0 || !xdt0 || !f0 || !iaptype
      || !mode_q65 || !ibwa || !ibwb || !drift || !apmask || !apsymbols || !maxiters
      || !xdt1 || !f1 || !snr2 || !dat4 || !idec || !idfbest || !idtbest || !ndistbest
      || !ibwbest || !nrc || *npts2 <= 0 || *nsps2 <= 0)
    {
      return;
    }

  int const ll = 64 * (*mode_q65 + 2);
  float const baud = kQ65SampleRate / static_cast<float> (*nsps2);
  int idfmax = 1;
  int idtmax = 1;
  int maxdist = 4;
  int const ibw0 = (*ibwa + *ibwb) / 2;
  int const depth_class = *ndepth & 3;
  if (depth_class == 2)
    {
      idfmax = 3;
      idtmax = 3;
      maxdist = 5;
    }
  else if (depth_class == 3)
    {
      idfmax = 5;
      idtmax = 5;
      maxdist = 5;
    }

  std::vector<std::complex<float>> c0 (static_cast<size_t> (*npts2));
  std::vector<float> s3 (static_cast<size_t> (ll) * kQ65SpecColumns, 0.0f);
  std::array<char, kQ65DecodedChars> decoded {};

  for (int idf = 1; idf <= idfmax; ++idf)
    {
      int ndf = idf / 2;
      if ((idf % 2) == 0)
        {
          ndf = -ndf;
        }

      std::array<float, 3> a {};
      a[0] = -(*f0 + 0.5f * baud * static_cast<float> (ndf));
      a[1] = -0.5f * (*drift);
      int local_npts = *npts2;
      float fsample = kQ65SampleRate;
      twkfreq_impl (c00, c0.data (), local_npts, fsample, a);

      for (int idt = 1; idt <= idtmax; ++idt)
        {
          int ndt = idt / 2;
          if ((idt % 2) == 0)
            {
              ndt = -ndt;
            }

          int jpk = *jpk0 + (*nsps2) * ndt / 16;
          jpk = std::max (0, std::min (29000, jpk));

          int local_nsps = *nsps2;
          int local_mode = *mode_q65;
          int local_ll = ll;
          int local_nn = kQ65SpecColumns;
          spec64_impl (c0.data (), local_npts, local_nsps, local_mode, jpk,
                       s3.data (), local_ll, local_nn);

          float base = 1.0f;
          int total_bins = ll * kQ65SpecColumns;
          base = percentile_value (s3.data (), total_bins, 40);
          if (base <= 0.0f)
            {
              base = 1.0f;
            }

          for (float& value : s3)
            {
              value = std::min (value / base, kQ65S3Limit);
            }
          q65_bzap_impl (s3.data (), ll);

          for (int ibw = *ibwa; ibw <= *ibwb; ++ibw)
            {
              int const ndist = ndf * ndf + ndt * ndt + (ibw - ibw0) * (ibw - ibw0);
              if (ndist > maxdist)
                {
                  continue;
                }

              float const b90 = std::pow (1.72f, static_cast<float> (ibw));
              if (b90 > 345.0f)
                {
                  continue;
                }

              float b90ts = b90 / baud;
              float esnodb = 0.0f;
              int irc = -2;
              std::fill (decoded.begin (), decoded.end (), '\0');
              ftx_q65_dec2_c (s3.data (), &local_ll, nsubmode, &b90ts, &esnodb, &irc,
                              dat4, decoded.data (), apmask, apsymbols, maxiters);
              if (irc < 0)
                {
                  continue;
                }

              *idfbest = idf;
              *idtbest = idt;
              *ndistbest = ndist;
              *ibwbest = ibw;
              *nrc = irc;
              *idec = *iaptype;
              *snr2 = esnodb - db_scale (2500.0f / baud);
              *xdt1 = *xdt0 + static_cast<float> (*nsps2 * ndt) / (16.0f * kQ65SampleRate);
              *f1 = *f0 + 0.5f * baud * static_cast<float> (ndf);
              return;
            }
        }
    }
}

extern "C" void ftx_q65_dec_q3_c (float const s1[], int* iz, int* jz, int* ipk, int* jpk,
                                  int* ll, int* mode_q65, int* nsps, float const sync[85],
                                  int* ibwa, int* ibwb, int codewords[], int* ncw,
                                  float* snr2, int dat4[13], int* idec, char decoded[37],
                                  int* ibwbest, int* nrc, float* plog)
{
  if (idec)
    {
      *idec = -1;
    }
  if (snr2)
    {
      *snr2 = 0.0f;
    }
  if (dat4)
    {
      std::fill_n (dat4, kQ65MessageSymbols, 0);
    }
  fill_spaces (decoded);
  if (ibwbest)
    {
      *ibwbest = 0;
    }
  if (nrc)
    {
      *nrc = -2;
    }
  if (plog)
    {
      *plog = 0.0f;
    }

  if (!s1 || !iz || !jz || !ipk || !jpk || !ll || !mode_q65 || !nsps || !sync || !ibwa
      || !ibwb || !codewords || !ncw || !snr2 || !dat4 || !idec || !decoded || !ibwbest
      || !nrc || !plog || *ll <= 0)
    {
      return;
    }

  std::vector<float> s3 (static_cast<size_t> (*ll) * kQ65PayloadColumns, 0.0f);
  int i0 = 0;
  int j0 = 0;
  ftx_q65_s1_to_s3_c (s1, iz, jz, &i0, &j0, ipk, jpk, ll, mode_q65, sync, s3.data ());

  int nsubmode = q65_mode_to_submode (*mode_q65);
  float const baud = 12000.0f / static_cast<float> (*nsps);
  for (int ibw = *ibwa; ibw <= *ibwb; ++ibw)
    {
      float const b90 = std::pow (1.72f, static_cast<float> (ibw));
      float b90ts = b90 / baud;
      float esnodb = 0.0f;
      int irc = -2;
      ftx_q65_dec1_c (s3.data (), ll, &nsubmode, &b90ts, &esnodb, &irc, dat4,
                      decoded, codewords, ncw, plog);
      *nrc = irc;
      if (irc < 0)
        {
          continue;
        }

      *ibwbest = ibw;
      *snr2 = esnodb - db_scale (2500.0f / baud) + 3.0f;
      *idec = 3;
      return;
    }
}

extern "C" void ftx_q65_dec_q012_c (float s3[], int* ll, int* mode_q65, int* nsps,
                                    int* npasses, int apsym0[58], int aph10[10],
                                    int* ibwa, int* ibwb, int* maxiters, float* snr2,
                                    int dat4[13], int* idec, char decoded[37],
                                    int* ibwbest, int* nrc)
{
  if (idec)
    {
      *idec = -1;
    }
  if (snr2)
    {
      *snr2 = 0.0f;
    }
  if (dat4)
    {
      std::fill_n (dat4, kQ65MessageSymbols, 0);
    }
  fill_spaces (decoded);
  if (ibwbest)
    {
      *ibwbest = 0;
    }
  if (nrc)
    {
      *nrc = -2;
    }

  if (!s3 || !ll || !mode_q65 || !nsps || !npasses || !apsym0 || !aph10 || !ibwa || !ibwb
      || !maxiters || !snr2 || !dat4 || !idec || !decoded || !ibwbest || !nrc || *ll <= 0)
    {
      return;
    }

  int nsubmode = q65_mode_to_submode (*mode_q65);
  float const baud = 12000.0f / static_cast<float> (*nsps);

  for (int ipass = 0; ipass <= *npasses; ++ipass)
    {
      int apmask_packed[kQ65ApPacked] {};
      int apsymbols_packed[kQ65ApPacked] {};
      int iaptype = 0;

      if (ipass >= 1)
        {
          int apmask_bits[kQ65ApBits] {};
          int apsymbols_bits[kQ65ApBits] {};
          int nqsoprogress = 0;
          int ncontest = 0;
          int lapcqonly = 0;
          ftx_q65_ap_c (&nqsoprogress, &ipass, &ncontest, &lapcqonly,
                        apsym0, aph10, apmask_bits, apsymbols_bits, &iaptype);
          pack_q65_ap_bits (apmask_bits, apmask_packed);
          pack_q65_ap_bits (apsymbols_bits, apsymbols_packed);
        }

      for (int ibw = *ibwa; ibw <= *ibwb; ++ibw)
        {
          float const b90 = std::pow (1.72f, static_cast<float> (ibw));
          float b90ts = b90 / baud;
          float esnodb = 0.0f;
          int irc = -2;
          ftx_q65_dec2_c (s3, ll, &nsubmode, &b90ts, &esnodb, &irc, dat4,
                          decoded, apmask_packed, apsymbols_packed, maxiters);
          *nrc = irc;
          if (irc < 0)
            {
              continue;
            }

          *ibwbest = ibw;
          *snr2 = esnodb - db_scale (2500.0f / baud) + 3.0f;
          *idec = iaptype;
          return;
        }
    }
}
