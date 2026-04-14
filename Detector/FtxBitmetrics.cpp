#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <memory>
#include <vector>

#include <fftw3.h>

namespace
{

using Complex = std::complex<float>;

constexpr float kTiny = 1.0e-30f;

template <typename T>
T clamp_value (T value, T lo, T hi)
{
  return std::max (lo, std::min (hi, value));
}

struct ComplexFft32
{
  ComplexFft32 ()
    : in (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * 32))),
      out (reinterpret_cast<fftwf_complex*> (fftwf_malloc (sizeof (fftwf_complex) * 32))),
      plan (nullptr)
  {
    if (in && out)
      {
        plan = fftwf_plan_dft_1d (32, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~ComplexFft32 ()
  {
    if (plan)
      {
        fftwf_destroy_plan (plan);
      }
    if (out)
      {
        fftwf_free (out);
      }
    if (in)
      {
        fftwf_free (in);
      }
  }

  bool valid () const
  {
    return in && out && plan;
  }

  fftwf_complex* in;
  fftwf_complex* out;
  fftwf_plan plan;
};

ComplexFft32& fft32 ()
{
  thread_local ComplexFft32 instance;
  return instance;
}

inline float abs2 (Complex const& value)
{
  return std::norm (value);
}

inline float abs1 (Complex const& value)
{
  return std::abs (value);
}

inline float& bm_at (float* bitmetrics, int rows, int row, int col)
{
  return bitmetrics[row + rows * col];
}

float ft2_legacy_equalized_beta (std::array<std::array<float, 4>, 103> const& pwr_eq,
                                 float* noise_var_eq_out = nullptr)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};

  int noise_sum_eq = 0;
  int nnoise_eq = 0;
  for (int k = 0; k < 4; ++k)
    {
      for (int itone = 0; itone < 4; ++itone)
        {
          if (itone != icos4a[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
          if (itone != icos4b[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k + 33)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
          if (itone != icos4c[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k + 66)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
          if (itone != icos4d[static_cast<size_t> (k)])
            {
              noise_sum_eq += static_cast<int> (pwr_eq[static_cast<size_t> (k + 99)][static_cast<size_t> (itone)]);
              ++nnoise_eq;
            }
        }
    }

  int noise_var_eq = 0;
  if (nnoise_eq > 0)
    {
      noise_var_eq = static_cast<int> (static_cast<float> (noise_sum_eq)
                                       / std::max (1.0f, static_cast<float> (nnoise_eq)));
    }
  if (static_cast<float> (noise_var_eq) < 1.0e-10f)
    {
      noise_var_eq = static_cast<int> (1.0e-10f);
    }
  if (noise_var_eq_out)
    {
      *noise_var_eq_out = static_cast<float> (noise_var_eq);
    }

  float beta_eq = std::numeric_limits<float>::infinity ();
  if (noise_var_eq != 0)
    {
      beta_eq = 0.5f / static_cast<float> (noise_var_eq);
    }
  return clamp_value (beta_eq, 0.01f, 50.0f);
}

float ft2_legacy_base_noise_var (std::array<std::array<float, 4>, 103> const& pwr)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};

  int noise_sum = 0;
  int nnoise = 0;
  for (int k = 0; k < 4; ++k)
    {
      for (int itone = 0; itone < 4; ++itone)
        {
          if (itone != icos4a[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
          if (itone != icos4b[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k + 33)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
          if (itone != icos4c[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k + 66)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
          if (itone != icos4d[static_cast<size_t> (k)])
            {
              noise_sum += static_cast<int> (pwr[static_cast<size_t> (k + 99)][static_cast<size_t> (itone)]);
              ++nnoise;
            }
        }
    }

  float noise_var = 0.0f;
  if (nnoise > 0)
    {
      noise_var = static_cast<float> (noise_sum) / std::max (1.0f, static_cast<float> (nnoise));
    }
  return std::max (noise_var, 1.0e-10f);
}

void normalizebmet_cpp (float* data, int n)
{
  float sum = 0.0f;
  float sum2 = 0.0f;
  for (int i = 0; i < n; ++i)
    {
      sum += data[i];
      sum2 += data[i] * data[i];
    }

  float const mean = sum / static_cast<float> (n);
  float const mean2 = sum2 / static_cast<float> (n);
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
  if (sigma <= 0.0f)
    {
      return;
    }

  for (int i = 0; i < n; ++i)
    {
      data[i] /= sigma;
    }
}

void finalize_ft2_bitmetric_columns (float* bitmetrics, int rows)
{
  bm_at (bitmetrics, rows, 204, 1) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 1) = bm_at (bitmetrics, rows, 205, 0);
  for (int i = 200; i < 204; ++i)
    {
      bm_at (bitmetrics, rows, i, 2) = bm_at (bitmetrics, rows, i, 1);
    }
  bm_at (bitmetrics, rows, 204, 2) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 2) = bm_at (bitmetrics, rows, 205, 0);

  normalizebmet_cpp (bitmetrics + rows * 1, rows);
  normalizebmet_cpp (bitmetrics + rows * 2, rows);
}

std::array<std::array<bool, 8>, 256> const& one8_table ()
{
  static std::array<std::array<bool, 8>, 256> table = [] {
    std::array<std::array<bool, 8>, 256> value {};
    for (int i = 0; i < 256; ++i)
      {
        for (int j = 0; j < 8; ++j)
          {
            value[static_cast<size_t> (i)][static_cast<size_t> (j)] = (i & (1 << j)) != 0;
          }
      }
    return value;
  }();
  return table;
}

std::array<std::array<bool, 9>, 512> const& one9_table ()
{
  static std::array<std::array<bool, 9>, 512> table = [] {
    std::array<std::array<bool, 9>, 512> value {};
    for (int i = 0; i < 512; ++i)
      {
        for (int j = 0; j < 9; ++j)
          {
            value[static_cast<size_t> (i)][static_cast<size_t> (j)] = (i & (1 << j)) != 0;
          }
      }
    return value;
  }();
  return table;
}

void fft_symbol_4tones (Complex const* input, std::array<Complex, 4>& tones,
                        std::array<float, 4>& mags, std::array<float, 4>* power = nullptr)
{
  auto& fft = fft32 ();
  if (!fft.valid ())
    {
      tones.fill (Complex {});
      mags.fill (0.0f);
      if (power)
        {
          power->fill (0.0f);
        }
      return;
    }

  for (int i = 0; i < 32; ++i)
    {
      fft.in[i][0] = input[i].real ();
      fft.in[i][1] = input[i].imag ();
    }
  fftwf_execute (fft.plan);

  for (int i = 0; i < 4; ++i)
    {
      Complex const value {fft.out[i][0], fft.out[i][1]};
      tones[static_cast<size_t> (i)] = value;
      mags[static_cast<size_t> (i)] = abs1 (value);
      if (power)
        {
          (*power)[static_cast<size_t> (i)] = abs2 (value);
        }
    }
}

void dft_symbol_4tones (Complex const* input, std::array<Complex, 4>& tones,
                        std::array<float, 4>& mags, std::array<float, 4>* power = nullptr)
{
  constexpr int NSS = 32;
  constexpr float kTwoPi = 6.28318530717958647692f;

  tones.fill (Complex {});
  mags.fill (0.0f);
  if (power)
    {
      power->fill (0.0f);
    }

  for (int n = 0; n < NSS; ++n)
    {
      Complex const sample = input[n];
      for (int bin = 0; bin < 4; ++bin)
        {
          float const angle = -kTwoPi * static_cast<float> (bin * n) / static_cast<float> (NSS);
          tones[static_cast<size_t> (bin)] += sample * Complex {std::cos (angle), std::sin (angle)};
        }
    }

  for (int bin = 0; bin < 4; ++bin)
    {
      Complex const value = tones[static_cast<size_t> (bin)];
      mags[static_cast<size_t> (bin)] = abs1 (value);
      if (power)
        {
          (*power)[static_cast<size_t> (bin)] = abs2 (value);
        }
    }
}

void fft_symbol_8tones_ft8 (Complex const* input, std::array<Complex, 8>& tones,
                            std::array<float, 8>& mags)
{
  auto& fft = fft32 ();
  if (!fft.valid ())
    {
      tones.fill (Complex {});
      mags.fill (0.0f);
      return;
    }

  for (int i = 0; i < 32; ++i)
    {
      fft.in[i][0] = input[i].real ();
      fft.in[i][1] = input[i].imag ();
    }
  fftwf_execute (fft.plan);

  for (int i = 0; i < 8; ++i)
    {
      Complex const value {fft.out[i][0], fft.out[i][1]};
      tones[static_cast<size_t> (i)] = value;
      mags[static_cast<size_t> (i)] = abs1 (value);
    }
}

bool check_ft4_sync (std::array<std::array<float, 4>, 103> const& s4, int minimum)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};
  int is1 = 0;
  int is2 = 0;
  int is3 = 0;
  int is4 = 0;

  for (int k = 0; k < 4; ++k)
    {
      auto argmax = [] (std::array<float, 4> const& row) {
        return static_cast<int> (std::max_element (row.begin (), row.end ()) - row.begin ());
      };
      if (icos4a[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k)])) ++is1;
      if (icos4b[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k + 33)])) ++is2;
      if (icos4c[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k + 66)])) ++is3;
      if (icos4d[static_cast<size_t> (k)] == argmax (s4[static_cast<size_t> (k + 99)])) ++is4;
    }

  return (is1 + is2 + is3 + is4) >= minimum;
}

void ft2_channel_est_cpp (Complex const* cd, Complex* cd_eq, float* ch_snr)
{
  static std::array<int, 4> const icos4a {{0, 1, 3, 2}};
  static std::array<int, 4> const icos4b {{1, 0, 2, 3}};
  static std::array<int, 4> const icos4c {{2, 3, 1, 0}};
  static std::array<int, 4> const icos4d {{3, 2, 0, 1}};
  constexpr int NN = 103;
  constexpr int NSS = 32;

  std::array<int, 16> sync_pos {};
  for (int j = 0; j < 4; ++j)
    {
      sync_pos[static_cast<size_t> (j)] = j + 1;
      sync_pos[static_cast<size_t> (j + 4)] = j + 34;
      sync_pos[static_cast<size_t> (j + 8)] = j + 67;
      sync_pos[static_cast<size_t> (j + 12)] = j + 100;
    }

  std::array<Complex, 16> h_sync {};
  std::array<Complex, NN> h_est {};
  std::array<float, NN> h_mag {};
  float sum_noise = 0.0f;
  float ncount = 0.0f;

  for (int j = 0; j < 16; ++j)
    {
      int const k = sync_pos[static_cast<size_t> (j)];
      int const idx = (k - 1) * NSS;
      std::array<Complex, 4> cs_rx {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + idx, cs_rx, mags, &power);

      int itone = 0;
      if (j < 4) itone = icos4a[static_cast<size_t> (j)];
      else if (j < 8) itone = icos4b[static_cast<size_t> (j - 4)];
      else if (j < 12) itone = icos4c[static_cast<size_t> (j - 8)];
      else itone = icos4d[static_cast<size_t> (j - 12)];

      h_sync[static_cast<size_t> (j)] = cs_rx[static_cast<size_t> (itone)];
      for (int m = 0; m < 4; ++m)
        {
          if (m != itone)
            {
              sum_noise += power[static_cast<size_t> (m)];
              ncount += 1.0f;
            }
        }
    }

  float noise_var = ncount > 0.0f ? sum_noise / ncount : 1.0e-10f;
  noise_var = std::max (noise_var, 1.0e-10f);

  for (int j = 0; j < 16; ++j)
    {
      h_est[static_cast<size_t> (sync_pos[static_cast<size_t> (j)] - 1)] = h_sync[static_cast<size_t> (j)];
    }

  h_est[0] = h_sync[0];
  h_est[1] = 0.5f * (h_sync[0] + h_sync[1]);
  h_est[2] = 0.5f * (h_sync[1] + h_sync[2]);
  h_est[3] = 0.5f * (h_sync[2] + h_sync[3]);

  for (int k = 5; k <= 33; ++k)
    {
      float w = static_cast<float> (k - 3) / static_cast<float> (35 - 3);
      w = clamp_value (w, 0.0f, 1.0f);
      h_est[static_cast<size_t> (k - 1)] = (1.0f - w) * 0.5f * (h_sync[2] + h_sync[3])
        + w * 0.5f * (h_sync[4] + h_sync[5]);
    }

  h_est[33] = 0.5f * (h_sync[4] + h_sync[5]);
  h_est[34] = 0.5f * (h_sync[5] + h_sync[6]);
  h_est[35] = 0.5f * (h_sync[6] + h_sync[7]);
  h_est[36] = h_sync[7];

  for (int k = 38; k <= 66; ++k)
    {
      float w = static_cast<float> (k - 36) / static_cast<float> (68 - 36);
      w = clamp_value (w, 0.0f, 1.0f);
      h_est[static_cast<size_t> (k - 1)] = (1.0f - w) * 0.5f * (h_sync[6] + h_sync[7])
        + w * 0.5f * (h_sync[8] + h_sync[9]);
    }

  h_est[66] = 0.5f * (h_sync[8] + h_sync[9]);
  h_est[67] = 0.5f * (h_sync[9] + h_sync[10]);
  h_est[68] = 0.5f * (h_sync[10] + h_sync[11]);
  h_est[69] = h_sync[11];

  for (int k = 71; k <= 99; ++k)
    {
      float w = static_cast<float> (k - 69) / static_cast<float> (101 - 69);
      w = clamp_value (w, 0.0f, 1.0f);
      h_est[static_cast<size_t> (k - 1)] = (1.0f - w) * 0.5f * (h_sync[10] + h_sync[11])
        + w * 0.5f * (h_sync[12] + h_sync[13]);
    }

  h_est[99] = 0.5f * (h_sync[12] + h_sync[13]);
  h_est[100] = 0.5f * (h_sync[13] + h_sync[14]);
  h_est[101] = 0.5f * (h_sync[14] + h_sync[15]);
  h_est[102] = h_sync[15];

  for (int k = 0; k < NN; ++k)
    {
      h_mag[static_cast<size_t> (k)] = abs2 (h_est[static_cast<size_t> (k)]);
    }

  for (int k = 0; k < NN; ++k)
    {
      int const idx = k * NSS;
      float const den = h_mag[static_cast<size_t> (k)] + noise_var;
      if (den > 1.0e-20f)
        {
          Complex const coeff = std::conj (h_est[static_cast<size_t> (k)]) / den;
          for (int i = 0; i < NSS; ++i)
            {
              cd_eq[idx + i] = cd[idx + i] * coeff;
            }
        }
      else
        {
          std::copy (cd + idx, cd + idx + NSS, cd_eq + idx);
        }

      ch_snr[k] = noise_var > 1.0e-20f ? h_mag[static_cast<size_t> (k)] / noise_var : 100.0f;
    }
}

void run_ft4_bitmetrics (Complex const* cd, float* bitmetrics, int* badsync)
{
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  auto const& one = one8_table ();
  constexpr int NN = 103;
  constexpr int NSS = 32;
  constexpr int rows = 2 * NN;

  std::fill (bitmetrics, bitmetrics + rows * 3, 0.0f);
  *badsync = 0;

  std::array<std::array<Complex, 4>, NN> cs {};
  std::array<std::array<float, 4>, NN> s4 {};

  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      fft_symbol_4tones (cd + k * NSS, tones, mags);
      cs[static_cast<size_t> (k)] = tones;
      s4[static_cast<size_t> (k)] = mags;
    }

  if (!check_ft4_sync (s4, 8))
    {
      *badsync = 1;
      return;
    }

  for (int nseq = 1; nseq <= 3; ++nseq)
    {
      int const nsym = (nseq == 1 ? 1 : (nseq == 2 ? 2 : 4));
      int const nt = 1 << (2 * nsym);
      std::array<float, 256> s2 {};

      for (int ks = 1; ks <= NN - nsym + 1; ks += nsym)
        {
          for (int i = 0; i < nt; ++i)
            {
              int const i1 = i / 64;
              int const i2 = (i & 63) / 16;
              int const i3 = (i & 15) / 4;
              int const i4 = (i & 3);
              if (nsym == 1)
                {
                  s2[static_cast<size_t> (i)] = abs1 (cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i4)])]);
                }
              else if (nsym == 2)
                {
                  s2[static_cast<size_t> (i)] =
                    abs1 (cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]
                        + cs[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i4)])]);
                }
              else
                {
                  s2[static_cast<size_t> (i)] =
                    abs1 (cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i1)])]
                        + cs[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i2)])]
                        + cs[static_cast<size_t> (ks + 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]
                        + cs[static_cast<size_t> (ks + 2)][static_cast<size_t> (graymap[static_cast<size_t> (i4)])]);
                }
            }

          int const ipt = 1 + (ks - 1) * 2;
          int const ibmax = (nsym == 1 ? 1 : (nsym == 2 ? 3 : 7));
          for (int ib = 0; ib <= ibmax; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }
              float max1 = -1.0e30f;
              float max0 = -1.0e30f;
              for (int i = 0; i < nt; ++i)
                {
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)];
                  if (set) max1 = std::max (max1, s2[static_cast<size_t> (i)]);
                  else max0 = std::max (max0, s2[static_cast<size_t> (i)]);
                }
              bm_at (bitmetrics, rows, ipt + ib - 1, nseq - 1) = max1 - max0;
            }
        }
    }

  bm_at (bitmetrics, rows, 204, 1) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 1) = bm_at (bitmetrics, rows, 205, 0);
  for (int i = 200; i < 204; ++i)
    {
      bm_at (bitmetrics, rows, i, 2) = bm_at (bitmetrics, rows, i, 1);
    }
  bm_at (bitmetrics, rows, 204, 2) = bm_at (bitmetrics, rows, 204, 0);
  bm_at (bitmetrics, rows, 205, 2) = bm_at (bitmetrics, rows, 205, 0);

  normalizebmet_cpp (bitmetrics + rows * 0, rows);
  normalizebmet_cpp (bitmetrics + rows * 1, rows);
  normalizebmet_cpp (bitmetrics + rows * 2, rows);
}

void run_ft2_bitmetrics_impl (Complex const* cd, float* bitmetrics, int* badsync,
                              bool allow_equalized_branch,
                              float* bitmetrics_base_out,
                              float* bmet_eq_raw_out,
                              float* bmet_eq_out,
                              Complex* cd_eq_out,
                              float* ch_snr_out,
                              int* use_cheq_out,
                              float* snr_min_out,
                              float* snr_max_out,
                              float* snr_mean_out,
                              float* fading_depth_out,
                              float* noise_var_out,
                              float* noise_var_eq_out)
{
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  auto const& one = one8_table ();
  constexpr int NN = 103;
  constexpr int NSS = 32;
  constexpr int rows = 2 * NN;

  std::fill (bitmetrics, bitmetrics + rows * 3, 0.0f);
  *badsync = 0;
  if (bitmetrics_base_out)
    {
      std::fill (bitmetrics_base_out, bitmetrics_base_out + rows * 3, 0.0f);
    }
  if (bmet_eq_raw_out)
    {
      std::fill (bmet_eq_raw_out, bmet_eq_raw_out + rows, 0.0f);
    }
  if (bmet_eq_out)
    {
      std::fill (bmet_eq_out, bmet_eq_out + rows, 0.0f);
    }
  if (cd_eq_out)
    {
      std::fill_n (cd_eq_out, NN * NSS, Complex {});
    }
  if (ch_snr_out)
    {
      std::fill (ch_snr_out, ch_snr_out + NN, 0.0f);
    }
  if (use_cheq_out) *use_cheq_out = 0;
  if (snr_min_out) *snr_min_out = 0.0f;
  if (snr_max_out) *snr_max_out = 0.0f;
  if (snr_mean_out) *snr_mean_out = 0.0f;
  if (fading_depth_out) *fading_depth_out = 0.0f;
  if (noise_var_out) *noise_var_out = 0.0f;
  if (noise_var_eq_out) *noise_var_eq_out = 0.0f;

  std::array<std::array<Complex, 4>, NN> cs {};
  std::array<std::array<float, 4>, NN> s4 {};
  std::array<std::array<float, 4>, NN> pwr {};

  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + k * NSS, tones, mags, &power);
      cs[static_cast<size_t> (k)] = tones;
      s4[static_cast<size_t> (k)] = mags;
      pwr[static_cast<size_t> (k)] = power;
    }

  float noise_var = ft2_legacy_base_noise_var (pwr);
  if (noise_var_out)
    {
      *noise_var_out = noise_var;
    }
  float beta = clamp_value (0.5f / noise_var, 0.01f, 50.0f);

  if (!check_ft4_sync (s4, 3))
    {
      *badsync = 1;
      return;
    }

  std::array<float, rows> bmet_eq {};
  for (int nseq = 1; nseq <= 3; ++nseq)
    {
      int const nsym = (nseq == 1 ? 1 : (nseq == 2 ? 2 : 4));
      int const nt = 1 << (2 * nsym);
      std::array<float, 256> sp {};

      for (int ks = 1; ks <= NN - nsym + 1; ks += nsym)
        {
          for (int i = 0; i < nt; ++i)
            {
              int const i1 = i / 64;
              int const i2 = (i & 63) / 16;
              int const i3 = (i & 15) / 4;
              int const i4 = (i & 3);
              Complex ctmp {};
              if (nsym == 1)
                {
                  ctmp = cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i4)])];
                }
              else if (nsym == 2)
                {
                  ctmp = cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]
                    + cs[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i4)])];
                }
              else
                {
                  ctmp = cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i1)])]
                    + cs[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i2)])]
                    + cs[static_cast<size_t> (ks + 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]
                    + cs[static_cast<size_t> (ks + 2)][static_cast<size_t> (graymap[static_cast<size_t> (i4)])];
                }
              sp[static_cast<size_t> (i)] = abs2 (ctmp);
            }

          int const ipt = 1 + (ks - 1) * 2;
          int const ibmax = (nsym == 1 ? 1 : (nsym == 2 ? 3 : 7));
          float const beta_eff = beta / static_cast<float> (nsym);
          for (int ib = 0; ib <= ibmax; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }

              float maxp1 = -1.0e30f;
              float maxp0 = -1.0e30f;
              for (int i = 0; i < nt; ++i)
                {
                  float const pval = beta_eff * sp[static_cast<size_t> (i)];
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)];
                  if (set) maxp1 = std::max (maxp1, pval);
                  else maxp0 = std::max (maxp0, pval);
                }

              float lse1 = 0.0f;
              float lse0 = 0.0f;
              for (int i = 0; i < nt; ++i)
                {
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)];
                  float const pval = beta_eff * sp[static_cast<size_t> (i)];
                  if (set) lse1 += std::exp (pval - maxp1);
                  else lse0 += std::exp (pval - maxp0);
                }

              bm_at (bitmetrics, rows, ipt + ib - 1, nseq - 1) =
                (maxp1 + std::log (std::max (lse1, kTiny))) -
                (maxp0 + std::log (std::max (lse0, kTiny)));
            }
        }
    }

  std::array<float, rows * 3> bitmetrics_base {};
  std::copy_n (bitmetrics, rows * 3, bitmetrics_base.data ());
  normalizebmet_cpp (bitmetrics_base.data () + rows * 0, rows);
  finalize_ft2_bitmetric_columns (bitmetrics_base.data (), rows);
  if (bitmetrics_base_out)
    {
      std::copy_n (bitmetrics_base.data (), rows * 3, bitmetrics_base_out);
    }

  std::vector<Complex> cd_eq (NN * NSS);
  std::array<float, NN> ch_snr {};
  ft2_channel_est_cpp (cd, cd_eq.data (), ch_snr.data ());
  if (cd_eq_out)
    {
      std::copy_n (cd_eq.data (), NN * NSS, cd_eq_out);
    }
  if (ch_snr_out)
    {
      std::copy_n (ch_snr.data (), NN, ch_snr_out);
    }

  float snr_min = ch_snr[0];
  float snr_max = ch_snr[0];
  float snr_mean = 0.0f;
  for (float value : ch_snr)
    {
      snr_min = std::min (snr_min, value);
      snr_max = std::max (snr_max, value);
      snr_mean += value;
    }
  snr_mean /= static_cast<float> (NN);
  float fading_depth = snr_min > 1.0e-10f ? 10.0f * std::log10 (snr_max / snr_min) : 30.0f;
  if (snr_min_out) *snr_min_out = snr_min;
  if (snr_max_out) *snr_max_out = snr_max;
  if (snr_mean_out) *snr_mean_out = snr_mean;
  if (fading_depth_out) *fading_depth_out = fading_depth;
  // The legacy FT2 demapper relies on implicit Fortran typing in the
  // equalized-noise path. Keep that behaviour explicit here so the C++ port
  // matches the historical decoder output, including weak-signal edge cases.
  bool const use_cheq = allow_equalized_branch && fading_depth > 2.0f;
  if (use_cheq_out)
    {
      *use_cheq_out = use_cheq ? 1 : 0;
    }

  if (use_cheq)
    {
      std::array<std::array<Complex, 4>, NN> cs_eq {};
      std::array<std::array<float, 4>, NN> pwr_eq {};
      for (int k = 0; k < NN; ++k)
        {
          std::array<Complex, 4> tones {};
          std::array<float, 4> mags {};
          std::array<float, 4> power {};
          dft_symbol_4tones (cd_eq.data () + k * NSS, tones, mags, &power);
          cs_eq[static_cast<size_t> (k)] = tones;
          pwr_eq[static_cast<size_t> (k)] = power;
        }

      float beta_eq = ft2_legacy_equalized_beta (pwr_eq, noise_var_eq_out);

      for (int ks = 1; ks <= NN; ++ks)
        {
          std::array<float, 4> sp_eq {};
          for (int i = 0; i < 4; ++i)
            {
              sp_eq[static_cast<size_t> (i)] = pwr_eq[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i)])];
            }
          int const ipt = 1 + (ks - 1) * 2;
          float snr_weight = 1.0f;
          if (snr_mean > 1.0e-10f)
            {
              snr_weight = std::sqrt (ch_snr[static_cast<size_t> (ks - 1)] / snr_mean);
              snr_weight = clamp_value (snr_weight, 0.1f, 3.0f);
            }
          for (int ib = 0; ib <= 1; ++ib)
            {
              if (ipt + ib > rows)
                {
                  continue;
                }
              float maxp1 = -1.0e30f;
              float maxp0 = -1.0e30f;
              for (int i = 0; i < 4; ++i)
                {
                  float const pval = beta_eq * sp_eq[static_cast<size_t> (i)];
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (1 - ib)];
                  if (set) maxp1 = std::max (maxp1, pval);
                  else maxp0 = std::max (maxp0, pval);
                }
              float lse1 = 0.0f;
              float lse0 = 0.0f;
              for (int i = 0; i < 4; ++i)
                {
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (1 - ib)];
                  float const pval = beta_eq * sp_eq[static_cast<size_t> (i)];
                  if (set) lse1 += std::exp (pval - maxp1);
                  else lse0 += std::exp (pval - maxp0);
                }
              bmet_eq[static_cast<size_t> (ipt + ib - 1)] =
                ((maxp1 + std::log (std::max (lse1, kTiny))) -
                 (maxp0 + std::log (std::max (lse0, kTiny)))) * snr_weight;
            }
        }

      if (bmet_eq_raw_out)
        {
          std::copy_n (bmet_eq.data (), rows, bmet_eq_raw_out);
        }
      normalizebmet_cpp (bmet_eq.data (), rows);
      if (bmet_eq_out)
        {
          std::copy_n (bmet_eq.data (), rows, bmet_eq_out);
        }
      float blend = clamp_value ((fading_depth - 2.0f) / 10.0f, 0.0f, 0.90f);
      normalizebmet_cpp (bitmetrics + rows * 0, rows);
      for (int i = 0; i < rows; ++i)
        {
          bm_at (bitmetrics, rows, i, 0) =
            (1.0f - blend) * bm_at (bitmetrics, rows, i, 0) + blend * bmet_eq[static_cast<size_t> (i)];
        }
      normalizebmet_cpp (bitmetrics + rows * 0, rows);
    }
  else
    {
      normalizebmet_cpp (bitmetrics + rows * 0, rows);
    }

  finalize_ft2_bitmetric_columns (bitmetrics, rows);
}

void run_ft2_bitmetrics (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft2_bitmetrics_impl (cd, bitmetrics, badsync,
                           true,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

void run_ft8_bitmetrics (Complex const* cd0, int np2, int ibest, int imetric,
                         float scale,
                         float* s8_out, int* nsync_out,
                         float* llra, float* llrb, float* llrc,
                         float* llrd, float* llre)
{
  constexpr int NN = 79;
  constexpr int NSS = 32;
  static std::array<int, 7> const icos7 {{3, 1, 4, 0, 6, 5, 2}};
  static std::array<int, 8> const graymap {{0, 1, 3, 2, 5, 6, 4, 7}};
  auto const& one = one9_table ();

  std::array<std::array<Complex, 8>, NN> cs {};
  std::array<float, 512> s2 {};
  std::array<float, 174> bmeta {};
  std::array<float, 174> bmetb {};
  std::array<float, 174> bmetc {};
  std::array<float, 174> bmetd {};
  std::array<float, 174> bmete {};

  for (int k = 0; k < NN; ++k)
    {
      int const i1 = ibest + k * NSS;
      std::array<Complex, 32> csymb {};
      if (i1 >= 0 && i1 + 31 <= np2 - 1)
        {
          for (int i = 0; i < NSS; ++i)
            {
              csymb[static_cast<size_t> (i)] = cd0[i1 + i];
            }
        }

      std::array<Complex, 8> tones {};
      std::array<float, 8> mags {};
      fft_symbol_8tones_ft8 (csymb.data (), tones, mags);
      for (int tone = 0; tone < 8; ++tone)
        {
          cs[static_cast<size_t> (k)][static_cast<size_t> (tone)] = tones[static_cast<size_t> (tone)] / 1.0e3f;
          s8_out[tone + 8 * k] = mags[static_cast<size_t> (tone)];
        }
    }

  int is1 = 0;
  int is2 = 0;
  int is3 = 0;
  for (int k = 0; k < 7; ++k)
    {
      auto argmax_symbol = [s8_out](int symbol) {
        int best = 0;
        float best_value = s8_out[8 * symbol];
        for (int tone = 1; tone < 8; ++tone)
          {
            float const value = s8_out[tone + 8 * symbol];
            if (value > best_value)
              {
                best = tone;
                best_value = value;
              }
          }
        return best;
      };

      if (icos7[static_cast<size_t> (k)] == argmax_symbol (k)) ++is1;
      if (icos7[static_cast<size_t> (k)] == argmax_symbol (k + 36)) ++is2;
      if (icos7[static_cast<size_t> (k)] == argmax_symbol (k + 72)) ++is3;
    }
  *nsync_out = is1 + is2 + is3;

  for (int nsym = 1; nsym <= 3; ++nsym)
    {
      int const nt = 1 << (3 * nsym);
      for (int ihalf = 1; ihalf <= 2; ++ihalf)
        {
          for (int k = 1; k <= 29; k += nsym)
            {
              int const ks = (ihalf == 1) ? (k + 7) : (k + 43);

              for (int i = 0; i < nt; ++i)
                {
                  int const i1 = i / 64;
                  int const i2 = (i & 63) / 8;
                  int const i3 = i & 7;
                  float value = 0.0f;
                  if (nsym == 1)
                    {
                      value = abs1 (cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]);
                    }
                  else if (nsym == 2)
                    {
                      value = abs1 (cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i2)])] +
                                    cs[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]);
                    }
                  else
                    {
                      value = abs1 (cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i1)])] +
                                    cs[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i2)])] +
                                    cs[static_cast<size_t> (ks + 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]);
                    }
                  if (imetric == 2)
                    {
                      value *= value;
                    }
                  s2[static_cast<size_t> (i)] = value;
                }

              int const i32 = 1 + (k - 1) * 3 + (ihalf - 1) * 87;
              int ibmax = 2;
              if (nsym == 2) ibmax = 5;
              if (nsym == 3) ibmax = 8;

              for (int ib = 0; ib <= ibmax; ++ib)
                {
                  if (i32 + ib > 174)
                    {
                      continue;
                    }

                  float max1 = -1.0e30f;
                  float max0 = -1.0e30f;
                  for (int i = 0; i < nt; ++i)
                    {
                      float const value = s2[static_cast<size_t> (i)];
                      if (one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)]) max1 = std::max (max1, value);
                      else max0 = std::max (max0, value);
                    }
                  float const bm = max1 - max0;
                  int const out_index = i32 + ib - 1;

                  if (nsym == 1)
                    {
                      bmeta[static_cast<size_t> (out_index)] = bm;
                      float const den = std::max (max1, max0);
                      bmetd[static_cast<size_t> (out_index)] = den > 0.0f ? (bm / den) : 0.0f;
                    }
                  else if (nsym == 2)
                    {
                      bmetb[static_cast<size_t> (out_index)] = bm;
                    }
                  else
                    {
                      bmetc[static_cast<size_t> (out_index)] = bm;
                    }
                }
            }
        }
    }

  for (int i = 0; i < 174; ++i)
    {
      std::array<float, 3> const temp {{
          bmeta[static_cast<size_t> (i)],
          bmetb[static_cast<size_t> (i)],
          bmetc[static_cast<size_t> (i)]
        }};
      int best = 0;
      float best_abs = std::abs (temp[0]);
      for (int j = 1; j < 3; ++j)
        {
          float const current_abs = std::abs (temp[static_cast<size_t> (j)]);
          if (current_abs > best_abs)
            {
              best = j;
              best_abs = current_abs;
            }
        }
      bmete[static_cast<size_t> (i)] = temp[static_cast<size_t> (best)];
    }

  normalizebmet_cpp (bmeta.data (), 174);
  normalizebmet_cpp (bmetb.data (), 174);
  normalizebmet_cpp (bmetc.data (), 174);
  normalizebmet_cpp (bmetd.data (), 174);
  normalizebmet_cpp (bmete.data (), 174);

  for (int i = 0; i < 174; ++i)
    {
      llra[i] = scale * bmeta[static_cast<size_t> (i)];
      llrb[i] = scale * bmetb[static_cast<size_t> (i)];
      llrc[i] = scale * bmetc[static_cast<size_t> (i)];
      llrd[i] = scale * bmetd[static_cast<size_t> (i)];
      llre[i] = scale * bmete[static_cast<size_t> (i)];
    }
}

}

extern "C" void ftx_ft4_bitmetrics_c (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft4_bitmetrics (cd, bitmetrics, badsync);
}

extern "C" void ftx_ft4_bitmetrics_ref_c (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft4_bitmetrics (cd, bitmetrics, badsync);
}

extern "C" void ftx_ft2_bitmetrics_c (Complex const* cd, float* bitmetrics, int* badsync)
{
  run_ft2_bitmetrics (cd, bitmetrics, badsync);
}

extern "C" void ftx_ft2_channel_est_c (Complex const* cd, Complex* cd_eq, float* ch_snr)
{
  if (!cd || !cd_eq || !ch_snr)
    {
      return;
    }

  ft2_channel_est_cpp (cd, cd_eq, ch_snr);
}

extern "C" void ftx_ft2_bitmetrics_diag_c (Complex const* cd,
                                           float* bitmetrics_final,
                                           float* bitmetrics_base,
                                           float* bmet_eq_raw,
                                           float* bmet_eq,
                                           Complex* cd_eq,
                                           float* ch_snr,
                                           int* badsync,
                                           int* use_cheq,
                                           float* snr_min,
                                           float* snr_max,
                                           float* snr_mean,
                                           float* fading_depth,
                                           float* noise_var,
                                           float* noise_var_eq)
{
  run_ft2_bitmetrics_impl (cd, bitmetrics_final, badsync,
                           true,
                           bitmetrics_base, bmet_eq_raw, bmet_eq, cd_eq, ch_snr, use_cheq,
                           snr_min, snr_max, snr_mean, fading_depth, noise_var, noise_var_eq);
}

extern "C" void ftx_ft8_bitmetrics_c (Complex const* cd0, int np2, int ibest, int imetric,
                                      float* s8_out, int* nsync_out,
                                      float* llra, float* llrb, float* llrc,
                                      float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, 3.2f,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_scaled_c (Complex const* cd0, int np2, int ibest, int imetric,
                                             float scale,
                                             float* s8_out, int* nsync_out,
                                             float* llra, float* llrb, float* llrc,
                                             float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_a8_score_c (Complex const* cd, int nzz, float tbest, int const* itone_best,
                                    float* plog, int* nhard, float* sigobig)
{
  if (!cd || !itone_best || !plog || !nhard || !sigobig)
    {
      return;
    }

  constexpr int NN = 79;
  constexpr int NSS = 32;

  float plog_value = 0.0f;
  int nhard_value = 0;
  int nsum = 0;
  float sum_sync = 0.0f;
  float sum_sig = 0.0f;
  float sum_big = 0.0f;
  int const offset = static_cast<int> (std::lround ((tbest + 0.5f) / 0.005f));

  for (int k = 0; k < NN; ++k)
    {
      int const i0 = NSS * k + offset;
      std::array<Complex, NSS> csymb {};
      for (int i = 0; i < NSS; ++i)
        {
          int const index = i0 + i;
          if (index >= 0 && index < nzz)
            {
              csymb[static_cast<size_t> (i)] = cd[index];
            }
        }

      std::array<Complex, 8> tones {};
      std::array<float, 8> mags {};
      fft_symbol_8tones_ft8 (csymb.data (), tones, mags);

      std::array<float, 8> power {};
      float sum = 0.0f;
      for (int tone = 0; tone < 8; ++tone)
        {
          power[static_cast<size_t> (tone)] = mags[static_cast<size_t> (tone)] * mags[static_cast<size_t> (tone)];
          sum += power[static_cast<size_t> (tone)];
        }

      int const target_tone = std::max (0, std::min (itone_best[k], 7));
      if (sum > 0.0f)
        {
          float const p = power[static_cast<size_t> (target_tone)] / sum;
          plog_value += std::log (std::max (p, kTiny));
          ++nsum;
        }

      int const ipk = static_cast<int> (std::max_element (power.begin (), power.end ()) - power.begin ());
      if (ipk != target_tone)
        {
          ++nhard_value;
        }

      if (k <= 6 || (k >= 36 && k <= 42) || k >= 72)
        {
          sum_sync += power[static_cast<size_t> (target_tone)];
        }
      else
        {
          sum_sig += power[static_cast<size_t> (target_tone)];
        }
      sum_big += power[static_cast<size_t> (ipk)];
    }

  if (nsum < NN)
    {
      plog_value += static_cast<float> (NN - nsum) * std::log (0.125f);
    }

  *plog = plog_value;
  *nhard = nhard_value;
  *sigobig = sum_big > 0.0f ? (sum_sync + sum_sig) / sum_big : 0.0f;
}
