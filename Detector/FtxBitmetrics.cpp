#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <complex>
#include <limits>
#include <memory>
#include <vector>

#include <fftw3.h>

#include "Detector/FftCompat.hpp"

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
        plan = decodium::fft_compat::plan_dft_1d (32, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
      }
  }

  ~ComplexFft32 ()
  {
    if (plan)
      {
        decodium::fft_compat::destroy_plan (plan);
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
  // FFTW teardown can race macOS/OpenMP TLS finalization during application
  // shutdown. Keep this tiny per-thread workspace alive until process exit
  // instead of destroying its FFTW plan from a late thread_local destructor.
  static thread_local auto* instance = new ComplexFft32;
  return *instance;
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

// DECODIUM_FT8_STORICO_ENERGIA=1: somma di energie nello storico CQ (vedi
// save_cq_signal_history in FtxFt8Stage4.cpp). Letto una volta sola.
inline bool ft8_history_energy_mode ()
{
  static bool const v = [] {
    // Acceso di default dalla 1.0.625. DECODIUM_FT8_STORICO_ENERGIA=0 lo spegne.
    char const* raw = std::getenv ("DECODIUM_FT8_STORICO_ENERGIA");
    return !raw || raw[0] != '0';
  }();
  return v;
}

void normalizebmet_rms_cpp (float* data, int n)
{
  float sum2 = 0.0f;
  for (int i = 0; i < n; ++i)
    {
      sum2 += data[i] * data[i];
    }

  float const mean2 = sum2 / static_cast<float> (n);
  float const sigma = std::sqrt (std::max (mean2, 0.0f));
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

float ft8_sync_average (Complex const* cd0, int np2, int ibest,
                        std::array<int, 7> const& icos7)
{
  constexpr int NN = 79;
  constexpr int NSS = 32;

  float total = 0.0f;
  int count = 0;
  for (int symbol = 0; symbol < NN; ++symbol)
    {
      int sync_index = -1;
      if (symbol < 7)
        {
          sync_index = symbol;
        }
      else if (symbol >= 36 && symbol < 43)
        {
          sync_index = symbol - 36;
        }
      else if (symbol >= 72)
        {
          sync_index = symbol - 72;
        }
      if (sync_index < 0)
        {
          continue;
        }

      int const i1 = ibest + symbol * NSS;
      if (i1 < 0 || i1 + NSS - 1 > np2 - 1)
        {
          continue;
        }

      std::array<Complex, NSS> csymb {};
      for (int i = 0; i < NSS; ++i)
        {
          csymb[static_cast<size_t> (i)] = cd0[i1 + i];
        }

      std::array<Complex, 8> tones {};
      std::array<float, 8> mags {};
      fft_symbol_8tones_ft8 (csymb.data (), tones, mags);

      int const sync_tone = icos7[static_cast<size_t> (sync_index)];
      float const sync_level = mags[static_cast<size_t> (sync_tone)];
      float total_level = 0.0f;
      for (float value : mags)
        {
          total_level += value;
        }
      float const noise_level = (total_level - sync_level) / 7.0f;
      if (noise_level > 1.0e-16f)
        {
          total += sync_level / noise_level;
          ++count;
        }
    }

  return count > 0 ? total / static_cast<float> (count) : 99.0f;
}

void condition_ft8_weak_symbol_edges (std::array<Complex, 32>& csymb)
{
  csymb[0] *= 1.9f;
  csymb[31] *= 1.9f;

  float const first = abs1 (csymb[0]);
  float const last = abs1 (csymb[31]);
  if (last <= 1.0e-16f)
    {
      return;
    }

  float const scr = std::sqrt (first) / std::sqrt (last);
  if (scr > 1.0f)
    {
      csymb[31] *= scr;
    }
  else if (scr > 1.0e-16f)
    {
      csymb[0] /= scr;
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
                         float scale, bool weak_deep,
                         bool equalize_tone_power,
                         Complex const* history_cs,
                         Complex* current_cs_out,
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
  std::array<std::array<Complex, 8>, NN> cs_reverse {};
  std::array<float, 512> s2 {};
  std::array<float, 174> bmeta {};
  std::array<float, 174> bmetb {};
  std::array<float, 174> bmetc {};
  std::array<float, 174> bmetd {};
  std::array<float, 174> bmete {};

  bool const reverse_metric = (imetric == 3);
  int const metric_shape = reverse_metric ? 1 : (imetric == 4 ? 2 : imetric);
  float const sync_average = ft8_sync_average (cd0, np2, ibest, icos7);
  bool const condition_weak_edges = sync_average < 2.5f;

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
      if (condition_weak_edges)
        {
          condition_ft8_weak_symbol_edges (csymb);
        }

      std::array<Complex, 8> forward_tones {};
      std::array<float, 8> forward_mags {};
      fft_symbol_8tones_ft8 (csymb.data (), forward_tones, forward_mags);

      std::array<Complex, 32> csymb_reverse {};
      for (int i = 0; i < NSS; ++i)
        {
          csymb_reverse[static_cast<size_t> (i)] =
              std::conj (csymb[static_cast<size_t> (NSS - 1 - i)]);
        }
      std::array<Complex, 8> reverse_tones {};
      std::array<float, 8> reverse_mags {};
      fft_symbol_8tones_ft8 (csymb_reverse.data (), reverse_tones, reverse_mags);

      auto const& tones = reverse_metric ? reverse_tones : forward_tones;
      auto const& mags = reverse_metric ? reverse_mags : forward_mags;
      for (int tone = 0; tone < 8; ++tone)
        {
          cs[static_cast<size_t> (k)][static_cast<size_t> (tone)] = tones[static_cast<size_t> (tone)] / 1.0e3f;
          cs_reverse[static_cast<size_t> (k)][static_cast<size_t> (tone)] =
              reverse_tones[static_cast<size_t> (tone)] / 1.0e3f;
          s8_out[tone + 8 * k] = mags[static_cast<size_t> (tone)];
        }
    }

  if (equalize_tone_power)
    {
      std::array<float, 8> tone_power {};
      for (int tone = 0; tone < 8; ++tone)
        {
          for (int symbol = 0; symbol < 7; ++symbol)
            {
              tone_power[static_cast<size_t> (tone)] += s8_out[tone + 8 * symbol];
            }
          for (int symbol = 17; symbol < NN; ++symbol)
            {
              tone_power[static_cast<size_t> (tone)] += s8_out[tone + 8 * symbol];
            }
        }
      int quiet_tone = 0;
      for (int tone = 1; tone < 8; ++tone)
        {
          if (tone_power[static_cast<size_t> (tone)] < tone_power[static_cast<size_t> (quiet_tone)])
            {
              quiet_tone = tone;
            }
        }
      float const quiet_power = tone_power[static_cast<size_t> (quiet_tone)];
      if (quiet_power > 1.0e-12f)
        {
          for (int tone = 0; tone < 8; ++tone)
            {
              if (tone == quiet_tone)
                {
                  continue;
                }
              float const ratio = tone_power[static_cast<size_t> (tone)] / quiet_power;
              if (ratio <= 1.5f)
                {
                  continue;
                }
              float const complex_scale = 1.0f / std::sqrt (ratio);
              for (int symbol = 0; symbol < NN; ++symbol)
                {
                  s8_out[tone + 8 * symbol] /= ratio;
                  cs[static_cast<size_t> (symbol)][static_cast<size_t> (tone)] *= complex_scale;
                  cs_reverse[static_cast<size_t> (symbol)][static_cast<size_t> (tone)] *= complex_scale;
                }
            }
        }
    }

  if (current_cs_out)
    {
      for (int symbol = 0; symbol < NN; ++symbol)
        {
          for (int tone = 0; tone < 8; ++tone)
            {
              current_cs_out[symbol * 8 + tone] =
                  cs[static_cast<size_t> (symbol)][static_cast<size_t> (tone)];
            }
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

  float srr = 99.0f;
  if (weak_deep)
    {
      float synclev = 0.0f;
      float sumlev = 0.0f;
      for (int k = 0; k < 7; ++k)
        {
          int const symbol = k + 36;
          synclev += s8_out[icos7[static_cast<size_t> (k)] + 8 * symbol];
          for (int tone = 0; tone < 8; ++tone)
            {
              sumlev += s8_out[tone + 8 * symbol];
            }
        }
      float snoiselev = (sumlev - synclev) / 7.0f;
      if (snoiselev < 0.1f)
        {
          snoiselev = 1.0f;
        }
      srr = synclev / snoiselev;
    }
  bool const use_weak_transform = weak_deep && srr < 2.5f;

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
                  if (history_cs)
                    {
                      Complex current {};
                      Complex history {};
                      if (nsym == 1)
                        {
                          int const tone = graymap[static_cast<size_t> (i3)];
                          current = cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (tone)];
                          history = history_cs[(ks - 1) * 8 + tone];
                        }
                      else if (nsym == 2)
                        {
                          int const tone1 = graymap[static_cast<size_t> (i2)];
                          int const tone2 = graymap[static_cast<size_t> (i3)];
                          current =
                              cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (tone1)] +
                              cs[static_cast<size_t> (ks)][static_cast<size_t> (tone2)];
                          history = history_cs[(ks - 1) * 8 + tone1]
                                    + history_cs[ks * 8 + tone2];
                        }
                      else
                        {
                          int const tone1 = graymap[static_cast<size_t> (i1)];
                          int const tone2 = graymap[static_cast<size_t> (i2)];
                          int const tone3 = graymap[static_cast<size_t> (i3)];
                          current =
                              cs[static_cast<size_t> (ks - 1)][static_cast<size_t> (tone1)] +
                              cs[static_cast<size_t> (ks)][static_cast<size_t> (tone2)] +
                              cs[static_cast<size_t> (ks + 1)][static_cast<size_t> (tone3)];
                          history = history_cs[(ks - 1) * 8 + tone1]
                                    + history_cs[ks * 8 + tone2]
                                    + history_cs[(ks + 1) * 8 + tone3];
                        }
                      // Con DECODIUM_FT8_STORICO_ENERGIA=1 lo storico porta la
                      // radice della somma delle energie passate, e qui si sommano
                      // le ENERGIE (|S|^2), non i moduli: e' il combinatore quadratico
                      // classico, misurato in FT2 entro 0,05 dB dalla verosimiglianza
                      // esatta. Il risultato resta in unita' di modulo per le forme
                      // di metrica a valle.
                      value = ft8_history_energy_mode ()
                                  ? std::sqrt (abs2 (current) + abs2 (history))
                                  : abs1 (current) + abs1 (history);
                      if (use_weak_transform)
                        {
                          value = std::pow (0.5f * value, 3.0f);
                        }
                    }
                  else if (nsym == 1)
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
                  if (!history_cs && use_weak_transform)
                    {
                      if (metric_shape == 1)
                        {
                          if (srr > 2.3f)
                            {
                              value *= value;
                            }
                          else if (value < 5.77f)
                            {
                              float const value2 = value * value;
                              value = 1.0f + 8.0f * value2 - 0.12f * value2 * value2;
                            }
                          else
                            {
                              value = (value + 5.82f) * (value + 5.82f);
                            }
                        }
                      else
                        {
                          value = std::pow (0.5f * value, 3.0f);
                        }
                    }
                  else if (!history_cs && metric_shape == 2)
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

  std::array<float, 174> bmet_reverse {};
  bool const use_reverse_extra = (imetric == 4);
  if (use_reverse_extra)
    {
      std::array<float, 174> rbmeta {};
      std::array<float, 174> rbmetb {};
      std::array<float, 174> rbmetc {};
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
                          value = abs1 (cs_reverse[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]);
                        }
                      else if (nsym == 2)
                        {
                          value = abs1 (cs_reverse[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i2)])] +
                                        cs_reverse[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]);
                        }
                      else
                        {
                          value = abs1 (cs_reverse[static_cast<size_t> (ks - 1)][static_cast<size_t> (graymap[static_cast<size_t> (i1)])] +
                                        cs_reverse[static_cast<size_t> (ks)][static_cast<size_t> (graymap[static_cast<size_t> (i2)])] +
                                        cs_reverse[static_cast<size_t> (ks + 1)][static_cast<size_t> (graymap[static_cast<size_t> (i3)])]);
                        }
                      if (use_weak_transform)
                        {
                          value = std::pow (0.5f * value, 3.0f);
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
                      if (nsym == 1) rbmeta[static_cast<size_t> (out_index)] = bm;
                      else if (nsym == 2) rbmetb[static_cast<size_t> (out_index)] = bm;
                      else rbmetc[static_cast<size_t> (out_index)] = bm;
                    }
                }
            }
        }

      for (int i = 0; i < 174; ++i)
        {
          std::array<float, 3> const temp {{
              rbmeta[static_cast<size_t> (i)],
              rbmetb[static_cast<size_t> (i)],
              rbmetc[static_cast<size_t> (i)]
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
          bmet_reverse[static_cast<size_t> (i)] = temp[static_cast<size_t> (best)];
        }
    }

  normalizebmet_rms_cpp (bmeta.data (), 174);
  normalizebmet_rms_cpp (bmetb.data (), 174);
  normalizebmet_rms_cpp (bmetc.data (), 174);
  normalizebmet_rms_cpp (bmetd.data (), 174);
  normalizebmet_rms_cpp (bmete.data (), 174);
  if (use_reverse_extra)
    {
      normalizebmet_rms_cpp (bmet_reverse.data (), 174);
    }

  for (int i = 0; i < 174; ++i)
    {
      llra[i] = scale * bmeta[static_cast<size_t> (i)];
      llrb[i] = scale * bmetb[static_cast<size_t> (i)];
      llrc[i] = scale * bmetc[static_cast<size_t> (i)];
      llrd[i] = scale * bmetd[static_cast<size_t> (i)];
      llre[i] = scale * (use_reverse_extra
                         ? bmet_reverse[static_cast<size_t> (i)]
                         : bmete[static_cast<size_t> (i)]);
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

// Moduli dei 4 toni per ciascuno dei 103 simboli della finestra: mags[k*4+j].
// Serve alla conferma a livello di tono del messaggio atteso (Stage7, tipo 8):
// con la sequenza di toni nota, la statistica e' la somma non coerente del
// modulo sul tono atteso meno la media degli altri tre, sui simboli dati.
extern "C" void ftx_ft2_symbol_mags_c (Complex const* cd, float* mags)
{
  constexpr int NN = 103;
  constexpr int NSS = 32;
  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> m {};
      fft_symbol_4tones (cd + k * NSS, tones, m);
      for (int j = 0; j < 4; ++j)
        {
          mags[k * 4 + j] = m[static_cast<size_t> (j)];
        }
    }
}

// Accumulo fra slot (Stage7, DECODIUM_FT2_ACCUMULO). Spettri di simbolo e
// fattore di rumore di UNA finestra, da sommare a livello di IPOTESI con
// quelli di altri slot della stessa stazione. cs_out[k*4+j] = FFT del simbolo
// k sul tono j (103 x 4); beta_out = 0,5 / varianza di rumore, lo stesso beta
// che il demodulatore usa per scalare le energie.
// Bit-metrics COERENTI per FT8: la fase del canale stimata dai 21 simboli
// Costas (tre gruppi da 7 ai simboli k, k+36, k+72, toni {3,1,4,0,6,5,2}).
//
// Stessa idea della versione FT2 qui sotto, con le differenze del modo: 8
// ipotesi di tono invece di 4, max-log invece di log-sum-exp, tre blocchi di
// combinazione coerente (1, 2 e 3 simboli) invece di 1, 2 e 4, e la
// normalizzazione in RMS sui 174 bit invece che in sigma sulle 206 righe.
//
// Con la fase nota l'ipotesi si pesa sulla PROIEZIONE invece che sul modulo, e
// la proiezione puo' essere negativa: e' quella l'informazione in piu', perche'
// un tono in controfase e' meno probabile di uno a energia nulla.
//
// Tetto teorico su FT8: 1,27 dB (terne 3,96 contro 2,69 del limite coerente),
// piu' alto dell'1,1 di FT2. Come in FT2 va usata come attempt IN PIU', mai al
// posto delle altre: da sola peggiora a segnale forte.
//
// cs: [79][8] spettri per simbolo, gia' catturati da
// ftx_ft8_bitmetrics_capture_c. Uscite: cinque insiemi da 174, gia' scalati.
extern "C" void ftx_ft8_bitmetrics_coherent_c (Complex const* cs, float scale,
                                               float* llra, float* llrb, float* llrc,
                                               float* llrd, float* llre)
{
  static std::array<int, 8> const graymap {{0, 1, 3, 2, 5, 6, 4, 7}};
  static int const icos7[7] = {3, 1, 4, 0, 6, 5, 2};
  static int const gruppo[3] = {0, 36, 72};
  auto const& one = one9_table ();

  // Ancore di fase sui tre gruppi Costas.
  std::array<Complex, 3> ancora {};
  std::array<float, 3> centro {};
  for (int g = 0; g < 3; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 7; ++k)
        somma += cs[static_cast<size_t> ((gruppo[g] + k) * 8 + icos7[k])];
      float const mag = std::abs (somma);
      ancora[static_cast<size_t> (g)] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[static_cast<size_t> (g)] = static_cast<float> (gruppo[g]) + 3.0f;
    }
  auto rotazione = [&] (int simbolo) {
    float const x = static_cast<float> (simbolo);
    Complex rif;
    if (x <= centro[0]) rif = ancora[0];
    else if (x >= centro[2]) rif = ancora[2];
    else
      {
        int const g = (x > centro[1]) ? 1 : 0;
        float const t = (x - centro[static_cast<size_t> (g)])
                        / (centro[static_cast<size_t> (g + 1)] - centro[static_cast<size_t> (g)]);
        rif = ancora[static_cast<size_t> (g)] * (1.0f - t) + ancora[static_cast<size_t> (g + 1)] * t;
      }
    float const mag = std::abs (rif);
    return mag > 0.0f ? std::conj (rif / mag) : Complex {1.0f, 0.0f};
  };

  std::array<float, 174> bmeta {}, bmetb {}, bmetc {};
  for (int nsym = 1; nsym <= 3; ++nsym)
    {
      int const nt = 1 << (3 * nsym);
      for (int ihalf = 1; ihalf <= 2; ++ihalf)
        {
          for (int k = 1; k <= 29; k += nsym)
            {
              int const ks = (ihalf == 1) ? (k + 7) : (k + 43);
              Complex const rot = rotazione (ks - 1);
              std::array<float, 512> sp {};
              for (int i = 0; i < nt; ++i)
                {
                  int const i1 = (i >> 6) & 7, i2 = (i >> 3) & 7, i3 = i & 7;
                  Complex somma {};
                  if (nsym == 1)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  else if (nsym == 2)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i3)])];
                  else
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i1)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> ((ks + 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  sp[static_cast<size_t> (i)] = std::real (somma * rot);
                }

              int const i32 = 1 + (k - 1) * 3 + (ihalf - 1) * 87;
              int const ibmax = (nsym == 1 ? 2 : (nsym == 2 ? 5 : 8));
              for (int ib = 0; ib <= ibmax; ++ib)
                {
                  if (i32 + ib > 174) continue;
                  float max1 = -1.0e30f, max0 = -1.0e30f;
                  for (int i = 0; i < nt; ++i)
                    {
                      float const v = sp[static_cast<size_t> (i)];
                      if (one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)]) max1 = std::max (max1, v);
                      else max0 = std::max (max0, v);
                    }
                  int const out_index = i32 + ib - 1;
                  float const bm = max1 - max0;
                  if (nsym == 1) bmeta[static_cast<size_t> (out_index)] = bm;
                  else if (nsym == 2) bmetb[static_cast<size_t> (out_index)] = bm;
                  else bmetc[static_cast<size_t> (out_index)] = bm;
                }
            }
        }
    }

  normalizebmet_rms_cpp (bmeta.data (), 174);
  normalizebmet_rms_cpp (bmetb.data (), 174);
  normalizebmet_rms_cpp (bmetc.data (), 174);

  for (int i = 0; i < 174; ++i)
    {
      float const a = bmeta[static_cast<size_t> (i)];
      float const b = bmetb[static_cast<size_t> (i)];
      float const c = bmetc[static_cast<size_t> (i)];
      float primo = a, secondo = b;
      if (std::abs (b) > std::abs (primo)) { secondo = primo; primo = b; }
      else if (std::abs (b) > std::abs (secondo)) { secondo = b; }
      if (std::abs (c) > std::abs (primo)) { secondo = primo; primo = c; }
      else if (std::abs (c) > std::abs (secondo)) { secondo = c; }
      llra[i] = scale * a;
      llrb[i] = scale * b;
      llrc[i] = scale * c;
      llrd[i] = scale * primo;
      llre[i] = scale * secondo;
    }
}

// Bit-metrics FT8 con informazione a priori sugli ALTRI bit del gruppo
// (demodulazione iterativa, BICM-ID).
//
// PERCHE'. Il demodulatore scompone ogni gruppo di simboli in bit trattati come
// indipendenti; non lo sono, perche' condividono le stesse ipotesi di tono. Il
// calcolo dell'informazione mutua (lab/tools/fsk_gmi.py) misura quanto costa
// quella scomposizione: 0,93 dB su un simbolo, 1,45 su coppie, 1,63 su terne --
// e cresce proprio dove la produzione combina piu' simboli. Il max-log invece
// non c'entra (0,06-0,13 dB dalla metrica esatta): ottimizzare la formula
// sarebbe tempo buttato.
//
// COME. `la` porta l'estrinseca del decodificatore
// (fastldpc_extrinsic174_91_c): per ogni ipotesi si somma alla metrica il
// contributo a priori di TUTTI i bit del gruppo TRANNE quello che si sta
// calcolando. Escluderlo e' cio' che rende l'uscita estrinseca, cioe' notizia
// nuova per il decodificatore e non l'eco della sua stessa opinione.
//
// LA SCALA CONTA. La metrica e' un modulo |cs|, l'a priori un LLR: sommarli
// direttamente vorrebbe dire pesare l'a priori a caso. Il fattore di
// conversione e' l'RMS della metrica grezza diviso `scale`, perche' l'uscita
// normalizzata vale scale*(max1-max0)/rms. Si ricava qui da una prima passata
// di max-log senza a priori, cosi' non dipende da cosa passa il chiamante.
// Gli |cs| delle ipotesi si calcolano UNA volta sola e si riusano nelle due
// passate: la parte cara sono quelli, non il max-log.
//
// I CINQUE INSIEMI sono quelli di run_ft8_bitmetrics e non altri: a, b, c sono
// le metriche a 1, 2 e 3 simboli; d e' quella a un simbolo divisa per il
// proprio livello max(max1,max0); e e' quella col modulo maggiore fra le tre.
// (Attenzione: NON e' la coppia "primo e secondo" della versione coerente.
// Con `la` nullo questa funzione riproduce la produzione bit per bit, e il
// banco lo verifica su tutti e cinque -- e' l'unico modo di sapere che
// l'innesto e' inerte da spento.)
//
// Come la passata coerente va usata come tentativo IN PIU', mai al posto degli
// altri. Misurato sul banco (tests/ft8_bicm_genie.cpp, solo il ramo a un
// simbolo): +21,3% di decodifiche, ~0,3 dB, zero falsi su 2236 cornici di
// rumore e sulle 960 prove con segnale. Il banco pero' confronta con la sola
// llra, mentre qui la produzione ha gia' cinque passate: il guadagno
// incrementale vero va misurato sulla catena, non dedotto da quel numero --
// e' l'errore che aveva gonfiato la passata coerente da +23% a +80%.
// Rapporto: lab/misure/20260910_bicm_id_ft8.md
//
// cs: [79][8] spettri per simbolo, gia' catturati da
// ftx_ft8_bitmetrics_capture_c (stesso ingresso della versione coerente).
// la: [174] convenzione Decodium (positivo = bit 1); nullo = nessuna.
// Uscite: cinque insiemi da 174, gia' scalati.
extern "C" void ftx_ft8_bitmetrics_bicm_c (Complex const* cs, float scale, float const* la,
                                           float* llra, float* llrb, float* llrc,
                                           float* llrd, float* llre)
{
  static std::array<int, 8> const graymap {{0, 1, 3, 2, 5, 6, 4, 7}};
  auto const& one = one9_table ();

  struct Gruppo
  {
    int nsym {0};
    int i32 {0};
    int ibmax {0};
    std::size_t off {0};
  };

  // Passata comune: i moduli delle ipotesi, una volta sola per ogni gruppo.
  std::vector<float> ipotesi;
  ipotesi.reserve (12700);                 // 58*8 + 30*64 + 20*512
  std::vector<Gruppo> gruppi;
  gruppi.reserve (110);
  for (int nsym = 1; nsym <= 3; ++nsym)
    {
      int const nt = 1 << (3 * nsym);
      int const ibmax = (nsym == 1 ? 2 : (nsym == 2 ? 5 : 8));
      for (int ihalf = 1; ihalf <= 2; ++ihalf)
        {
          for (int k = 1; k <= 29; k += nsym)
            {
              int const ks = (ihalf == 1) ? (k + 7) : (k + 43);
              std::size_t const off = ipotesi.size ();
              for (int i = 0; i < nt; ++i)
                {
                  int const i1 = i / 64, i2 = (i & 63) / 8, i3 = i & 7;
                  Complex somma {};
                  if (nsym == 1)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  else if (nsym == 2)
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i3)])];
                  else
                    somma = cs[static_cast<size_t> ((ks - 1) * 8 + graymap[static_cast<size_t> (i1)])]
                            + cs[static_cast<size_t> (ks * 8 + graymap[static_cast<size_t> (i2)])]
                            + cs[static_cast<size_t> ((ks + 1) * 8 + graymap[static_cast<size_t> (i3)])];
                  ipotesi.push_back (abs1 (somma));
                }
              gruppi.push_back (Gruppo {nsym, 1 + (k - 1) * 3 + (ihalf - 1) * 87, ibmax, off});
            }
        }
    }

  // max-log di un gruppo. Con `peso` non nullo ogni ipotesi porta in piu' il
  // contributo a priori degli altri bit del gruppo, gia' in unita' di metrica.
  // `dst_d` riceve la forma divisa per il livello, che esiste solo a un simbolo.
  auto max_log = [&] (Gruppo const& g, float const* peso, float const* proprio,
                      std::array<float, 174>& dst, std::array<float, 174>* dst_d) {
    int const nt = 1 << (3 * g.nsym);
    for (int ib = 0; ib <= g.ibmax; ++ib)
      {
        if (g.i32 + ib > 174) continue;
        float max1 = -1.0e30f, max0 = -1.0e30f;
        for (int i = 0; i < nt; ++i)
          {
            float v = ipotesi[g.off + static_cast<std::size_t> (i)];
            bool const uno = one[static_cast<size_t> (i)][static_cast<size_t> (g.ibmax - ib)];
            if (peso)
              {
                // il proprio bit non entra mai nel proprio a priori
                v += peso[i] - (uno ? proprio[ib] : 0.0f);
              }
            if (uno) max1 = std::max (max1, v);
            else max0 = std::max (max0, v);
          }
        std::size_t const out_index = static_cast<size_t> (g.i32 + ib - 1);
        float const bm = max1 - max0;
        dst[out_index] = bm;
        if (dst_d)
          {
            float const den = std::max (max1, max0);
            (*dst_d)[out_index] = den > 0.0f ? (bm / den) : 0.0f;
          }
      }
  };

  // Passata 1: senza a priori. Serve a due cose -- le metriche di partenza e,
  // dal loro RMS, il fattore che porta un LLR nelle unita' della metrica.
  std::array<float, 174> bmeta {}, bmetb {}, bmetc {}, bmetd {}, bmete {};
  for (Gruppo const& g : gruppi)
    max_log (g, nullptr, nullptr,
             g.nsym == 1 ? bmeta : (g.nsym == 2 ? bmetb : bmetc),
             g.nsym == 1 ? &bmetd : nullptr);

  if (la)
    {
      auto rms_di = [] (std::array<float, 174> const& v) {
        double s = 0.0;
        for (int i = 0; i < 174; ++i) s += static_cast<double> (v[static_cast<size_t> (i)]) * v[static_cast<size_t> (i)];
        double const r = std::sqrt (s / 174.0);
        return r > 0.0 ? static_cast<float> (r) : 1.0f;
      };
      float const conv[3] = {rms_di (bmeta) / scale, rms_di (bmetb) / scale, rms_di (bmetc) / scale};

      // Passata 2: le stesse ipotesi, ora pesate con l'estrinseca.
      std::array<float, 512> peso {};
      std::array<float, 9> proprio {};
      for (Gruppo const& g : gruppi)
        {
          int const nt = 1 << (3 * g.nsym);
          float const cv = conv[static_cast<size_t> (g.nsym - 1)];
          for (int ib = 0; ib <= g.ibmax; ++ib)
            {
              int const c = g.i32 + ib - 1;
              proprio[static_cast<size_t> (ib)] = (c < 174) ? cv * la[c] : 0.0f;
            }
          for (int i = 0; i < nt; ++i)
            {
              float somma = 0.0f;
              for (int ib = 0; ib <= g.ibmax; ++ib)
                if (one[static_cast<size_t> (i)][static_cast<size_t> (g.ibmax - ib)])
                  somma += proprio[static_cast<size_t> (ib)];
              peso[static_cast<size_t> (i)] = somma;
            }
          max_log (g, peso.data (), proprio.data (),
                   g.nsym == 1 ? bmeta : (g.nsym == 2 ? bmetb : bmetc),
                   g.nsym == 1 ? &bmetd : nullptr);
        }
    }

  // e = quella col modulo maggiore fra le tre, come in run_ft8_bitmetrics.
  for (int i = 0; i < 174; ++i)
    {
      std::array<float, 3> const temp {{bmeta[static_cast<size_t> (i)],
                                        bmetb[static_cast<size_t> (i)],
                                        bmetc[static_cast<size_t> (i)]}};
      int best = 0;
      float best_abs = std::abs (temp[0]);
      for (int j = 1; j < 3; ++j)
        {
          float const current_abs = std::abs (temp[static_cast<size_t> (j)]);
          if (current_abs > best_abs) { best = j; best_abs = current_abs; }
        }
      bmete[static_cast<size_t> (i)] = temp[static_cast<size_t> (best)];
    }

  normalizebmet_rms_cpp (bmeta.data (), 174);
  normalizebmet_rms_cpp (bmetb.data (), 174);
  normalizebmet_rms_cpp (bmetc.data (), 174);
  normalizebmet_rms_cpp (bmetd.data (), 174);
  normalizebmet_rms_cpp (bmete.data (), 174);

  for (int i = 0; i < 174; ++i)
    {
      llra[i] = scale * bmeta[static_cast<size_t> (i)];
      llrb[i] = scale * bmetb[static_cast<size_t> (i)];
      llrc[i] = scale * bmetc[static_cast<size_t> (i)];
      llrd[i] = scale * bmetd[static_cast<size_t> (i)];
      llre[i] = scale * bmete[static_cast<size_t> (i)];
    }
}

// Bit-metrics COERENTI: la fase del canale stimata dai simboli di
// sincronismo, invece di essere ignorata.
//
// PERCHE'. Il demodulatore di FT2 e' non coerente: usa l'energia dell'ipotesi
// di tono, perche' la fase del canale non si conosce. Ma la fase non e'
// l'informazione da decodificare, e' un parametro di disturbo -- e i parametri
// di disturbo si stimano. FT2 trasmette 16 simboli di sincronismo con toni
// NOTI (i quattro gruppi Costas ai simboli 0-3, 33-36, 66-69, 99-102): su
// quelli la fase si misura direttamente.
//
// COME. Un'ancora per gruppo (somma coerente dei suoi quattro simboli al tono
// giusto, per alzare il rapporto segnale-rumore della stima), poi
// interpolazione fra un'ancora e l'altra. L'interpolazione si fa sui vettori
// complessi unitari e non sugli angoli, cosi' non serve gestire i salti di
// 2 pi greco. Con la fase nota l'informazione sta nella parte reale e la
// quadratura e' solo rumore, quindi l'ipotesi si pesa sulla sua proiezione.
//
// COSA ASPETTARSI. Da sola questa metrica PEGGIORA a rapporto segnale-rumore
// alto, dove il non coerente va gia' bene e l'errore di stima butta via
// segnale. Va usata come passata IN PIU' accanto alle cinque cieche, mai al
// loro posto: misurato +23% di decodifiche e +6% di fantasmi contro le cinque
// passate di produzione (lab/misure/20260909_genio_ft8_e_fase.md).
//
// Uscita: 206 righe normalizzate, stesso formato di un piano di
// ftx_ft2_bitmetrics_c, da mappare con gli indici di build_llr_sets.
extern "C" void ftx_ft2_bitmetrics_coherent_c (Complex const* cd, float* rows_out)
{
  constexpr int NN = 103;
  constexpr int NSS = 32;
  constexpr int rows = 2 * NN;
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  static int const icos[4][4] = {{0,1,3,2},{1,0,2,3},{2,3,1,0},{3,2,0,1}};
  static int const gruppo[4] = {0, 33, 66, 99};

  std::array<std::array<Complex, 4>, NN> cs {};
  std::array<std::array<float, 4>, NN> pwr {};
  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + k * NSS, tones, mags, &power);
      cs[static_cast<size_t> (k)] = tones;
      pwr[static_cast<size_t> (k)] = power;
    }
  float const noise_var = ft2_legacy_base_noise_var (pwr);
  float const beta = clamp_value (0.5f / noise_var, 0.01f, 50.0f);

  // Ancore di fase sui quattro gruppi Costas.
  std::array<Complex, 4> ancora {};
  std::array<float, 4> centro {};
  for (int g = 0; g < 4; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 4; ++k)
        {
          somma += cs[static_cast<size_t> (gruppo[g] + k)][static_cast<size_t> (icos[g][k])];
        }
      float const mag = std::abs (somma);
      ancora[static_cast<size_t> (g)] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[static_cast<size_t> (g)] = static_cast<float> (gruppo[g]) + 1.5f;
    }

  std::array<float, rows> m {};
  for (int r = 0; r < rows; ++r)
    {
      int const s = r / 2;
      int const mio = 1 - (r % 2);

      float const x = static_cast<float> (s);
      Complex rif;
      if (x <= centro[0]) rif = ancora[0];
      else if (x >= centro[3]) rif = ancora[3];
      else
        {
          int g = 0;
          while (g < 3 && x > centro[static_cast<size_t> (g + 1)]) ++g;
          float const t = (x - centro[static_cast<size_t> (g)])
                          / (centro[static_cast<size_t> (g + 1)] - centro[static_cast<size_t> (g)]);
          rif = ancora[static_cast<size_t> (g)] * (1.0f - t)
                + ancora[static_cast<size_t> (g + 1)] * t;
        }
      float const rmag = std::abs (rif);
      Complex const rot = rmag > 0.0f ? std::conj (rif / rmag) : Complex {1.0f, 0.0f};

      float w[4];
      for (int i = 0; i < 4; ++i)
        {
          float const proj = std::real (cs[static_cast<size_t> (s)][static_cast<size_t> (graymap[static_cast<size_t> (i)])] * rot);
          w[i] = beta * proj * std::fabs (proj);
        }
      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) max1 = std::max (max1, w[i]);
          else max0 = std::max (max0, w[i]);
        }
      float lse1 = 0.0f, lse0 = 0.0f;
      for (int i = 0; i < 4; ++i)
        {
          if ((i & (1 << mio)) != 0) lse1 += std::exp (w[i] - max1);
          else lse0 += std::exp (w[i] - max0);
        }
      m[static_cast<size_t> (r)] = (max1 + std::log (std::max (lse1, kTiny)))
                                   - (max0 + std::log (std::max (lse0, kTiny)));
    }

  normalizebmet_cpp (m.data (), rows);
  std::copy_n (m.data (), rows, rows_out);
}

extern "C" void ftx_ft2_symbol_spectra_c (Complex const* cd, Complex* cs_out, float* beta_out)
{
  constexpr int NN = 103;
  constexpr int NSS = 32;
  std::array<std::array<float, 4>, NN> pwr {};
  for (int k = 0; k < NN; ++k)
    {
      std::array<Complex, 4> tones {};
      std::array<float, 4> mags {};
      std::array<float, 4> power {};
      fft_symbol_4tones (cd + k * NSS, tones, mags, &power);
      for (int j = 0; j < 4; ++j)
        {
          cs_out[k * 4 + j] = tones[static_cast<size_t> (j)];
        }
      pwr[static_cast<size_t> (k)] = power;
    }
  float const noise_var = ft2_legacy_base_noise_var (pwr);
  *beta_out = clamp_value (0.5f / noise_var, 0.01f, 50.0f);
}

// Bit-metrics dalla SOMMA di piu' slot. Stesse ipotesi a 1, 2 e 4 simboli del
// demodulatore (run_ft2_bitmetrics_impl, ramo non equalizzato), ma l'energia
// di ogni ipotesi e' sommata sugli slot, ciascuno col proprio beta: la somma
// coerente resta DENTRO lo slot (fase continua fra simboli vicini), fra slot
// diversi si sommano le energie (fase indipendente). E' la combinazione
// quadratica classica; il Monte Carlo (lab/tools/slot_accumulo.py) la da'
// entro 0,05 dB dalla verosimiglianza esatta. Con nslot = 1 riproduce il
// ramo base. cs = [nslot][103*4], beta = [nslot].
extern "C" void ftx_ft2_bitmetrics_accum_c (Complex const* cs, float const* beta, int nslot,
                                            float* bitmetrics)
{
  static std::array<int, 4> const graymap {{0, 1, 3, 2}};
  auto const& one = one8_table ();
  constexpr int NN = 103;
  constexpr int rows = 2 * NN;
  std::fill (bitmetrics, bitmetrics + rows * 3, 0.0f);
  if (!cs || !beta || nslot <= 0)
    {
      return;
    }

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
              float acc = 0.0f;
              for (int s = 0; s < nslot; ++s)
                {
                  Complex const* c = cs + static_cast<size_t> (s) * NN * 4;
                  auto tone = [&] (int k, int idx) {
                    return c[(ks - 1 + k) * 4 + graymap[static_cast<size_t> (idx)]];
                  };
                  Complex ctmp {};
                  if (nsym == 1)
                    {
                      ctmp = tone (0, i4);
                    }
                  else if (nsym == 2)
                    {
                      ctmp = tone (0, i3) + tone (1, i4);
                    }
                  else
                    {
                      ctmp = tone (0, i1) + tone (1, i2) + tone (2, i3) + tone (3, i4);
                    }
                  acc += (beta[s] / static_cast<float> (nsym)) * abs2 (ctmp);
                }
              sp[static_cast<size_t> (i)] = acc;
            }

          int const ipt = 1 + (ks - 1) * 2;
          int const ibmax = (nsym == 1 ? 1 : (nsym == 2 ? 3 : 7));
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
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)];
                  if (set) maxp1 = std::max (maxp1, sp[static_cast<size_t> (i)]);
                  else maxp0 = std::max (maxp0, sp[static_cast<size_t> (i)]);
                }
              float lse1 = 0.0f;
              float lse0 = 0.0f;
              for (int i = 0; i < nt; ++i)
                {
                  bool const set = one[static_cast<size_t> (i)][static_cast<size_t> (ibmax - ib)];
                  if (set) lse1 += std::exp (sp[static_cast<size_t> (i)] - maxp1);
                  else lse0 += std::exp (sp[static_cast<size_t> (i)] - maxp0);
                }
              bm_at (bitmetrics, rows, ipt + ib - 1, nseq - 1) =
                (maxp1 + std::log (std::max (lse1, kTiny))) -
                (maxp0 + std::log (std::max (lse0, kTiny)));
            }
        }
    }

  normalizebmet_cpp (bitmetrics + rows * 0, rows);
  finalize_ft2_bitmetric_columns (bitmetrics, rows);
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
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, 3.2f, false, false,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_scaled_c (Complex const* cd0, int np2, int ibest, int imetric,
                                             float scale,
                                             float* s8_out, int* nsync_out,
                                             float* llra, float* llrb, float* llrc,
                                             float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, false, false,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_deep_c (Complex const* cd0, int np2, int ibest, int imetric,
                                            float scale,
                                            float* s8_out, int* nsync_out,
                                            float* llra, float* llrb, float* llrc,
                                            float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, true, false,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_equalized_c (Complex const* cd0, int np2, int ibest, int imetric,
                                                float scale,
                                                float* s8_out, int* nsync_out,
                                                float* llra, float* llrb, float* llrc,
                                                float* llrd, float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, false, true,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_deep_equalized_c (Complex const* cd0, int np2, int ibest,
                                                     int imetric, float scale,
                                                     float* s8_out, int* nsync_out,
                                                     float* llra, float* llrb,
                                                     float* llrc, float* llrd,
                                                     float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale, true, true,
                      nullptr, nullptr,
                      s8_out, nsync_out, llra, llrb, llrc, llrd, llre);
}

extern "C" void ftx_ft8_bitmetrics_capture_c (Complex const* cd0, int np2, int ibest,
                                               int imetric, float scale,
                                               int weak_deep, int equalize_tone_power,
                                               Complex const* history_cs,
                                               Complex* current_cs_out,
                                               float* s8_out, int* nsync_out,
                                               float* llra, float* llrb,
                                               float* llrc, float* llrd,
                                               float* llre)
{
  run_ft8_bitmetrics (cd0, np2, ibest, imetric, scale,
                      weak_deep != 0, equalize_tone_power != 0,
                      history_cs, current_cs_out,
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
