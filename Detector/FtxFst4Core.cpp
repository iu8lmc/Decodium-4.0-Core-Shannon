// -*- Mode: C++ -*-
#include "wsjtx_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

namespace
{
  using Complex = std::complex<float>;

  constexpr int kFst4SyncSymbols {40};
  constexpr int kFst4DataSymbols {120};
  constexpr int kFst4TotalSymbols {kFst4SyncSymbols + kFst4DataSymbols};
  constexpr int kFst4MetricRows {2 * kFst4TotalSymbols};
  constexpr float kTwoPi {6.2831853071795864769f};

  constexpr std::array<int, 25> kFst4Crc24Poly {{
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1
  }};
  constexpr std::array<int, 8> kFst4SyncWord1 {{0, 1, 3, 2, 1, 0, 2, 3}};
  constexpr std::array<int, 8> kFst4SyncWord2 {{2, 3, 1, 0, 3, 2, 0, 1}};
  constexpr std::array<int, 4> kFst4GrayMap {{0, 1, 3, 2}};
  constexpr std::array<int, 16> kFst4HardSync1 {{0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0}};
  constexpr std::array<int, 16> kFst4HardSync2 {{1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1}};

  int fst4_crc24 (signed char const* bits, int len)
  {
    if (!bits || len < 25)
      {
        return 0;
      }

    std::array<int, 25> r {};
    for (int i = 0; i < 25; ++i)
      {
        r[static_cast<size_t> (i)] = bits[i] != 0 ? 1 : 0;
      }

    for (int i = 0; i <= len - 25; ++i)
      {
        r[24] = bits[i + 24] != 0 ? 1 : 0;
        int const lead = r[0];
        for (int j = 0; j < 25; ++j)
          {
            r[static_cast<size_t> (j)] = (r[static_cast<size_t> (j)] + lead * kFst4Crc24Poly[static_cast<size_t> (j)]) & 1;
          }
        int const wrapped = r[0];
        for (int j = 0; j < 24; ++j)
          {
            r[static_cast<size_t> (j)] = r[static_cast<size_t> (j + 1)];
          }
        r[24] = wrapped;
      }

    int crc = 0;
    for (int i = 0; i < 24; ++i)
      {
        crc = (crc << 1) | r[static_cast<size_t> (i)];
      }
    return crc;
  }

  float max_metric_for_bit (float const* values, int count, int bit_index, int bit_value)
  {
    float best = -std::numeric_limits<float>::infinity ();
    for (int i = 0; i < count; ++i)
      {
        if (((i >> bit_index) & 1) == bit_value)
          {
            best = std::max (best, values[static_cast<size_t> (i)]);
          }
      }
    return best;
  }

  void normalize_bitmetrics (float* column, int n)
  {
    float sum = 0.0f;
    float sum2 = 0.0f;
    for (int i = 0; i < n; ++i)
      {
        sum += column[i];
        sum2 += column[i] * column[i];
      }
    float const mean = sum / n;
    float const mean2 = sum2 / n;
    float const var = mean2 - mean * mean;
    float sigma = 0.0f;
    if (var > 0.0f)
      {
        sigma = std::sqrt (var);
      }
    else
      {
        sigma = std::sqrt (std::max (mean2, 0.0f));
      }
    if (!(sigma > 0.0f))
      {
        return;
      }
    for (int i = 0; i < n; ++i)
      {
        column[i] /= sigma;
      }
  }

  int max_tone_index (float const* s4, int symbol_index0, int sub_index0)
  {
    int best = 0;
    float best_value = -1.0f;
    for (int tone = 0; tone < 4; ++tone)
      {
        size_t index = static_cast<size_t> (tone + 4 * (symbol_index0 + kFst4TotalSymbols * sub_index0));
        float const value = s4[index];
        if (value > best_value)
          {
            best_value = value;
            best = tone;
          }
      }
    return best;
  }

  int fst4_sync_quality (float const* s4, int sub_index0)
  {
    int is1 = 0;
    int is2 = 0;
    int is3 = 0;
    int is4 = 0;
    int is5 = 0;
    for (int k = 0; k < 8; ++k)
      {
        if (max_tone_index (s4, k, sub_index0) == kFst4SyncWord1[static_cast<size_t> (k)]) ++is1;
        if (max_tone_index (s4, k + 38, sub_index0) == kFst4SyncWord2[static_cast<size_t> (k)]) ++is2;
        if (max_tone_index (s4, k + 76, sub_index0) == kFst4SyncWord1[static_cast<size_t> (k)]) ++is3;
        if (max_tone_index (s4, k + 114, sub_index0) == kFst4SyncWord2[static_cast<size_t> (k)]) ++is4;
        if (max_tone_index (s4, k + 152, sub_index0) == kFst4SyncWord1[static_cast<size_t> (k)]) ++is5;
      }
    return is1 + is2 + is3 + is4 + is5;
  }

  int fst4_hard_sync_quality (float const* bitmetrics)
  {
    std::array<int, kFst4MetricRows> hbits {};
    for (int i = 0; i < kFst4MetricRows; ++i)
      {
        hbits[static_cast<size_t> (i)] = bitmetrics[i] >= 0.0f ? 1 : 0;
      }

    auto count_match = [&hbits] (int start0, std::array<int, 16> const& pattern) {
      int count = 0;
      for (int i = 0; i < 16; ++i)
        {
          count += hbits[static_cast<size_t> (start0 + i)] == pattern[static_cast<size_t> (i)] ? 1 : 0;
        }
      return count;
    };

    return count_match (0, kFst4HardSync1)
      + count_match (76, kFst4HardSync2)
      + count_match (152, kFst4HardSync1)
      + count_match (228, kFst4HardSync2)
      + count_match (304, kFst4HardSync1);
  }

  std::array<std::vector<Complex>, 4> make_fst4_templates (int nss, int hmod)
  {
    std::array<std::vector<Complex>, 4> templates;
    float const dphi = kTwoPi * hmod / nss;
    for (int tone = 0; tone < 4; ++tone)
      {
        templates[static_cast<size_t> (tone)].resize (static_cast<size_t> (nss));
        float const dp = (tone - 1.5f) * dphi;
        float phi = 0.0f;
        for (int j = 0; j < nss; ++j)
          {
            templates[static_cast<size_t> (tone)][static_cast<size_t> (j)] = Complex {std::cos (phi), std::sin (phi)};
            phi = std::fmod (phi + dp, kTwoPi);
            if (phi < 0.0f)
              {
                phi += kTwoPi;
              }
          }
      }
    return templates;
  }

  Complex correlation (Complex const* samples, Complex const* tones, int first, int count)
  {
    Complex sum {0.0f, 0.0f};
    for (int i = 0; i < count; ++i)
      {
        sum += samples[first + i] * std::conj (tones[first + i]);
      }
    return sum;
  }

  void fst4_bitmetrics_impl (Complex const* cd, int nss, float* bitmetrics, float* s4_out,
                             int* nsync_qual, int* badsync)
  {
    if (!bitmetrics || !s4_out || !nsync_qual || !badsync || !cd || nss <= 0)
      {
        if (nsync_qual) *nsync_qual = 0;
        if (badsync) *badsync = 1;
        return;
      }

    auto const templates = make_fst4_templates (nss, 1);
    std::fill_n (bitmetrics, kFst4MetricRows * 4, 0.0f);
    std::fill_n (s4_out, 4 * kFst4TotalSymbols, 0.0f);

    for (int k = 0; k < kFst4TotalSymbols; ++k)
      {
        Complex const* symbol = cd + k * nss;
        for (int tone = 0; tone < 4; ++tone)
          {
            Complex const cs = correlation (symbol, templates[static_cast<size_t> (tone)].data (), 0, nss);
            s4_out[static_cast<size_t> (tone + 4 * k)] = std::norm (cs);
          }
      }

    if (fst4_sync_quality (s4_out, 0) < 16)
      {
        *nsync_qual = 0;
        *badsync = 1;
        return;
      }

    std::array<Complex, 32> c1 {};
    std::array<Complex, 64> c2 {};
    std::array<Complex, 512> c4 {};
    std::vector<float> s2 (65536);

    auto metric_slot = [&bitmetrics] (int row0, int block0) -> float& {
      return bitmetrics[static_cast<size_t> (row0 + kFst4MetricRows * block0)];
    };

    for (int k = 0; k < kFst4TotalSymbols; k += 8)
      {
        for (int m = 0; m < 8; ++m)
          {
            float tones[4] {};
            for (int n = 0; n < 4; ++n)
              {
                c1[static_cast<size_t> (n + 4 * m)] = correlation (cd + (k + m) * nss,
                                                                   templates[static_cast<size_t> (kFst4GrayMap[static_cast<size_t> (n)])].data (),
                                                                   0, nss);
                tones[n] = std::abs (c1[static_cast<size_t> (n + 4 * m)]);
              }
            int const ipt = 2 * k + 2 * m;
            for (int ib = 0; ib < 2; ++ib)
              {
                metric_slot (ipt + ib, 0) = max_metric_for_bit (tones, 4, 1 - ib, 1)
                                          - max_metric_for_bit (tones, 4, 1 - ib, 0);
              }
          }

        for (int m = 0; m < 4; ++m)
          {
            float tones[16] {};
            for (int i = 0; i < 4; ++i)
              {
                for (int j = 0; j < 4; ++j)
                  {
                    int const is = i * 4 + j;
                    c2[static_cast<size_t> (is + 16 * m)] = c1[static_cast<size_t> (i + 4 * (2 * m))]
                                                          - c1[static_cast<size_t> (j + 4 * (2 * m + 1))];
                    tones[is] = std::abs (c2[static_cast<size_t> (is + 16 * m)]);
                  }
              }
            int const ipt = 2 * k + 4 * m;
            for (int ib = 0; ib < 4; ++ib)
              {
                metric_slot (ipt + ib, 1) = max_metric_for_bit (tones, 16, 3 - ib, 1)
                                          - max_metric_for_bit (tones, 16, 3 - ib, 0);
              }
          }

        for (int m = 0; m < 2; ++m)
          {
            std::vector<float> tones (256);
            for (int i = 0; i < 16; ++i)
              {
                for (int j = 0; j < 16; ++j)
                  {
                    int const is = i * 16 + j;
                    c4[static_cast<size_t> (is + 256 * m)] = c2[static_cast<size_t> (i + 16 * (2 * m))]
                                                           + c2[static_cast<size_t> (j + 16 * (2 * m + 1))];
                    tones[static_cast<size_t> (is)] = std::abs (c4[static_cast<size_t> (is + 256 * m)]);
                  }
              }
            int const ipt = 2 * k + 8 * m;
            for (int ib = 0; ib < 8; ++ib)
              {
                metric_slot (ipt + ib, 2) = max_metric_for_bit (tones.data (), 256, 7 - ib, 1)
                                          - max_metric_for_bit (tones.data (), 256, 7 - ib, 0);
              }
          }

        for (int i = 0; i < 256; ++i)
          {
            for (int j = 0; j < 256; ++j)
              {
                int const is = i * 256 + j;
                s2[static_cast<size_t> (is)] = std::abs (c4[static_cast<size_t> (i)]
                                                       + c4[static_cast<size_t> (j + 256)]);
              }
          }
        int const ipt = 2 * k;
        for (int ib = 0; ib < 16; ++ib)
          {
            metric_slot (ipt + ib, 3) = max_metric_for_bit (s2.data (), 65536, 15 - ib, 1)
                                      - max_metric_for_bit (s2.data (), 65536, 15 - ib, 0);
          }
      }

    *nsync_qual = fst4_hard_sync_quality (bitmetrics);
    if (*nsync_qual < 46)
      {
        *badsync = 1;
        return;
      }

    for (int block = 0; block < 4; ++block)
      {
        normalize_bitmetrics (bitmetrics + kFst4MetricRows * block, kFst4MetricRows);
        for (int i = 0; i < kFst4MetricRows; ++i)
          {
            metric_slot (i, block) *= 2.83f;
          }
      }
    *badsync = 0;
  }

  void fst4_bitmetrics2_impl (Complex const* cd, int nss, int hmod, int nsizes, float* bitmetrics,
                              float* s4snr, int* badsync)
  {
    if (!bitmetrics || !s4snr || !badsync || !cd || nss <= 0)
      {
        if (badsync) *badsync = 1;
        return;
      }

    auto const templates = make_fst4_templates (nss, hmod);
    std::fill_n (bitmetrics, kFst4MetricRows * 4, 0.0f);
    std::fill_n (s4snr, 4 * kFst4TotalSymbols, 0.0f);
    std::vector<float> s4 (static_cast<size_t> (4 * kFst4TotalSymbols * 4), 0.0f);

    for (int k = 0; k < kFst4TotalSymbols; ++k)
      {
        Complex const* symbol = cd + k * nss;
        for (int tone = 0; tone < 4; ++tone)
          {
            Complex const* tmpl = templates[static_cast<size_t> (tone)].data ();
            for (int nsub = 0; nsub < 4; ++nsub)
              {
                int const segments = 1 << nsub;
                int const seg_len = nss / segments;
                float sum = 0.0f;
                for (int seg = 0; seg < segments; ++seg)
                  {
                    sum += std::norm (correlation (symbol, tmpl, seg * seg_len, seg_len));
                  }
                s4[static_cast<size_t> (tone + 4 * (k + kFst4TotalSymbols * nsub))] = sum;
              }
          }
      }

    if (fst4_sync_quality (s4.data (), 0) < 16)
      {
        *badsync = 1;
        return;
      }

    auto metric_slot = [&bitmetrics] (int row0, int block0) -> float& {
      return bitmetrics[static_cast<size_t> (row0 + kFst4MetricRows * block0)];
    };

    for (int nsub = 0; nsub < std::max (0, std::min (nsizes, 4)); ++nsub)
      {
        for (int ks = 0; ks < kFst4TotalSymbols; ++ks)
          {
            float tones[4] {};
            for (int i = 0; i < 4; ++i)
              {
                tones[i] = s4[static_cast<size_t> (kFst4GrayMap[static_cast<size_t> (i)] + 4 * (ks + kFst4TotalSymbols * nsub))];
              }
            int const ipt = 2 * ks;
            for (int ib = 0; ib < 2; ++ib)
              {
                metric_slot (ipt + ib, nsub) = max_metric_for_bit (tones, 4, 1 - ib, 1)
                                             - max_metric_for_bit (tones, 4, 1 - ib, 0);
              }
          }
      }

    for (int block = 0; block < 4; ++block)
      {
        normalize_bitmetrics (bitmetrics + kFst4MetricRows * block, kFst4MetricRows);
      }

    for (int tone = 0; tone < 4; ++tone)
      {
        for (int ks = 0; ks < kFst4TotalSymbols; ++ks)
          {
            s4snr[static_cast<size_t> (tone + 4 * ks)] = s4[static_cast<size_t> (tone + 4 * ks)];
          }
      }
    *badsync = 0;
  }
}

extern "C"
void get_crc24_ (signed char mc[], int* len, int* ncrc)
{
  if (!ncrc)
    {
      return;
    }
  if (!mc || !len)
    {
      *ncrc = 0;
      return;
    }
  *ncrc = fst4_crc24 (mc, *len);
}

extern "C"
void get_fst4_bitmetrics_ (Complex cd[], int* nss, float bitmetrics[], float s4[],
                           int* nsync_qual, int* badsync)
{
  fst4_bitmetrics_impl (cd, nss ? *nss : 0, bitmetrics, s4, nsync_qual, badsync);
}

extern "C"
void get_fst4_bitmetrics2_ (Complex cd[], int* nss, int* hmod, int* nsizes,
                            float bitmetrics[], float s4snr[], int* badsync)
{
  fst4_bitmetrics2_impl (cd, nss ? *nss : 0, hmod ? *hmod : 1, nsizes ? *nsizes : 0,
                         bitmetrics, s4snr, badsync);
}
